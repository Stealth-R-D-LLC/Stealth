/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2021, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file selftest.c
 * \brief Relay self-testing
 *
 * Relays need to make sure that their own ports are reachable, and estimate
 * their own bandwidth, before publishing.
 */

#include "core/or.h"

#include "app/config.h"

#include "core/connection.h"
#include "core/mainloop.h"
#include "core/netstatus.h"

#include "core/circuitbuild.h"
#include "core/circuitlist.h"
#include "core/circuituse.h"
#include "core/crypt_path_st.h"
#include "core/extendinfo.h"
#include "core/extend_info_st.h"
#include "core/origin_circuit_st.h"
#include "core/relay.h"

#include "feature/control_events.h"

#include "feature/authmode.h"

#include "feature/dirclient.h"
#include "feature/directory.h"

#include "feature/authority_cert_st.h"
#include "feature/routerinfo.h"
#include "feature/routerinfo_st.h"
#include "feature/routerlist.h"
#include "feature/routerset.h"
#include "feature/torcert.h"

#include "feature/relay_periodic.h"
#include "feature/router.h"
#include "feature/selftest.h"

static bool have_orport_for_family(int family);
static void inform_testing_reachability(const tor_addr_t *addr,
                                        uint16_t port);

/** Whether we can reach our IPv4 ORPort from the outside. */
static bool can_reach_or_port_ipv4 = false;
/** Whether we can reach our IPv6 ORPort from the outside. */
static bool can_reach_or_port_ipv6 = false;

/** Has informed_testing_reachable logged a message about testing our IPv4
 * ORPort? */
static bool have_informed_testing_or_port_ipv4 = false;
/** Has informed_testing_reachable logged a message about testing our IPv6
 * ORPort? */
static bool have_informed_testing_or_port_ipv6 = false;

/** Forget what we have learned about our reachability status. */
void
router_reset_reachability(void)
{
  can_reach_or_port_ipv4 = can_reach_or_port_ipv6 = false;
  have_informed_testing_or_port_ipv4 =
    have_informed_testing_or_port_ipv6 = false;
}

/** Return 1 if we won't do reachability checks, because:
 *   - AssumeReachable is set, or
 *   - the network is disabled.
 * Otherwise, return 0.
 */
static int
router_reachability_checks_disabled(const or_options_t *options)
{
  return options->AssumeReachable ||
         net_is_disabled();
}

/** Return 0 if we need to do an ORPort reachability check, because:
 *   - no reachability check has been done yet, or
 *   - we've initiated reachability checks, but none have succeeded.
 *  Return 1 if we don't need to do an ORPort reachability check, because:
 *   - we've seen a successful reachability check, or
 *   - AssumeReachable is set, or
 *   - the network is disabled.

 * If `family'`is AF_INET or AF_INET6, return true only when we should skip
 * the given family's orport check (Because it's been checked, or because we
 * aren't checking it.)  If `family` is 0, return true if we can skip _all_
 * orport checks.
 */
int
router_orport_seems_reachable(const or_options_t *options,
                              int family)
{
  tor_assert_nonfatal(family == AF_INET || family == AF_INET6 || family == 0);
  int reach_checks_disabled = router_reachability_checks_disabled(options);
  if (reach_checks_disabled) {
    return true;
  }

  // Note that we do a == 1 here, not just a boolean check.  This value
  // is also an autobool, so CFG_AUTO does not mean that we should
  // assume IPv6 ports are reachable.
  const bool ipv6_assume_reachable = (options->AssumeReachableIPv6 == 1);

  // Which reachability flags should we look at?
  const bool checking_ipv4 = (family == AF_INET || family == 0);
  const bool checking_ipv6 = (family == AF_INET6 || family == 0);

  if (checking_ipv4) {
    if (have_orport_for_family(AF_INET) && !can_reach_or_port_ipv4) {
      return false;
    }
  }
  if (checking_ipv6 && !ipv6_assume_reachable) {
    if (have_orport_for_family(AF_INET6) && !can_reach_or_port_ipv6) {
      return false;
    }
  }

  return true;
}

/** Relay DirPorts are no longer used (though authorities are). In either case,
 * reachability self test is done anymore, since network re-entry towards an
 * authority DirPort is not allowed. Thus, consider it always reachable. */
int
router_dirport_seems_reachable(const or_options_t *options)
{
  (void) options;
  return 1;
}

/** See if we currently believe our ORPort to be unreachable. If so, return 1
 * else return 0. */
static int
router_should_check_reachability(void)
{
  const routerinfo_t *me = router_get_my_routerinfo();
  const or_options_t *options = get_options();

  if (!me)
    return 0;

  /* Doesn't check our IPv6 address, see #34065. */
  if (routerset_contains_router(options->ExcludeNodes, me, -1) &&
      options->StrictNodes) {
    /* If we've excluded ourself, and StrictNodes is set, we can't test
     * ourself. */
#define SELF_EXCLUDED_WARN_INTERVAL 3600
    static ratelim_t warning_limit=RATELIM_INIT(SELF_EXCLUDED_WARN_INTERVAL);
    log_fn_ratelim(&warning_limit, LOG_WARN, LD_CIRC,
                   "Can't perform self-tests for this relay: we have "
                   "listed ourself in ExcludeNodes, and StrictNodes is set. "
                   "We cannot learn whether we are usable, and will not "
                   "be able to advertise ourself.");
    return 0;
  }
  return 1;
}

/**
 * Return true if we have configured an ORPort for the given family that
 * we would like to advertise.
 *
 * Like other self-testing functions, this function looks at our most
 * recently built descriptor.
 **/
static bool
have_orport_for_family(int family)
{
  const routerinfo_t *me = router_get_my_routerinfo();

  if (!me)
    return false;

  tor_addr_port_t ap;
  if (router_get_orport(me, &ap, family) < 0) {
    return false;
  }
  return true;
}

/** Allocate and return a new extend_info_t that can be used to build
 * a circuit to or through the router <b>r</b>, using an address from
 * <b>family</b> (if available).
 *
 * Clients don't have routerinfos, so this function should only be called on a
 * server.
 *
 * If the requested address is not available, returns NULL. */
static extend_info_t *
extend_info_from_router(const routerinfo_t *r, int family)
{
  crypto_pk_t *rsa_pubkey;
  extend_info_t *info;
  tor_addr_port_t ap;

  if (BUG(!r)) {
    return NULL;
  }

  /* Relays always assume that the first hop is reachable. They ignore
   * ReachableAddresses. */
  tor_assert_nonfatal(router_or_conn_should_skip_reachable_address_check(
                                                           get_options(), 0));

  const ed25519_public_key_t *ed_id_key;
  if (r->cache_info.signing_key_cert)
    ed_id_key = &r->cache_info.signing_key_cert->signing_key;
  else
    ed_id_key = NULL;

  if (router_get_orport(r, &ap, family) < 0) {
    /* We don't have an ORPort for the requested family. */
    return NULL;
  }
  rsa_pubkey = router_get_rsa_onion_pkey(r->onion_pkey, r->onion_pkey_len);
  info = extend_info_new(r->nickname, r->cache_info.identity_digest,
                         ed_id_key,
                         rsa_pubkey, r->onion_curve25519_pkey,
                         &ap.addr, ap.port,
                         /* TODO-324: Should self-test circuits use
                          * congestion control? */
                         NULL, false);
  crypto_pk_free(rsa_pubkey);
  return info;
}

/** Launch a self-testing circuit to one of our ORPorts, using an address from
 * <b>family</b> (if available). The circuit can be used to test reachability
 * or bandwidth. <b>me</b> is our own routerinfo.
 *
 * Logs an info-level status message. If <b>orport_reachable</b> is false,
 * call it a reachability circuit. Otherwise, call it a bandwidth circuit.
 *
 * See router_do_reachability_checks() for details. */
static void
router_do_orport_reachability_checks(const routerinfo_t *me,
                                     int family,
                                     int orport_reachable)
{
  extend_info_t *ei = extend_info_from_router(me, family);
  int ipv6_flags = (family == AF_INET6 ? CIRCLAUNCH_IS_IPV6_SELFTEST : 0);

  /* If we're trying to test IPv6, but we don't have an IPv6 ORPort, ei will
   * be NULL. */
  if (ei) {
    const char *family_name = fmt_af_family(family);
    const tor_addr_port_t *ap = extend_info_get_orport(ei, family);
    if (BUG(!ap)) {
      /* Not much we can do here to recover apart from screaming loudly. */
      extend_info_free(ei);
      return;
    }
    log_info(LD_CIRC, "Testing %s of my %s ORPort: %s.",
             !orport_reachable ? "reachability" : "bandwidth",
             family_name, fmt_addrport_ap(ap));

    if (!orport_reachable) {
      /* Only log if we are actually doing a reachability test to learn if our
       * ORPort is reachable. Else, this prints a log notice if we are simply
       * opening a bandwidth testing circuit even though we are reachable. */
      inform_testing_reachability(&ap->addr, ap->port);
    }

    circuit_launch_by_extend_info(CIRCUIT_PURPOSE_TESTING, ei,
                                  CIRCLAUNCH_NEED_CAPACITY|
                                  CIRCLAUNCH_IS_INTERNAL|
                                  ipv6_flags);
    extend_info_free(ei);
  }
}

/** Some time has passed, or we just got new directory information. See if we
 * currently believe our ORPort to be unreachable. If so, launch a new test
 * for it.
 *
 * For ORPort, we simply try making a circuit that ends at ourselves.  Success
 * is noticed in onionskin_answer().
 */
void
router_do_reachability_checks(void)
{
  const routerinfo_t *me = router_get_my_routerinfo();
  const or_options_t *options = get_options();
  int orport_reachable_v4 =
    router_orport_seems_reachable(options, AF_INET);
  int orport_reachable_v6 =
    router_orport_seems_reachable(options, AF_INET6);

  if (router_should_check_reachability()) {
    bool need_testing = !circuit_enough_testing_circs();
    /* At the moment, tor relays believe that they are reachable when they
     * receive any create cell on an inbound connection, if the address
     * family is correct.
     */
    if (!orport_reachable_v4 || need_testing) {
      router_do_orport_reachability_checks(me, AF_INET, orport_reachable_v4);
    }
    if (!orport_reachable_v6 || need_testing) {
      router_do_orport_reachability_checks(me, AF_INET6, orport_reachable_v6);
    }
  }
}

/** Log a message informing the user that we are testing a port for
 * reachability, if we have not already logged such a message.
 *
 * Calls to router_reset_reachability() will reset our view of whether we have
 * logged this message for a given port. */
static void
inform_testing_reachability(const tor_addr_t *addr, uint16_t port)
{
  if (!router_get_my_routerinfo())
    return;

  bool *have_informed_ptr;
  if (tor_addr_family(addr) == AF_INET) {
    have_informed_ptr = &have_informed_testing_or_port_ipv4;
  } else {
    have_informed_ptr = &have_informed_testing_or_port_ipv6;
  }

  if (*have_informed_ptr) {
    /* We already told the user that we're testing this port; no need to
     * do it again. */
    return;
  }

  char addr_buf[TOR_ADDRPORT_BUF_LEN];
  strlcpy(addr_buf, fmt_addrport(addr, port), sizeof(addr_buf));

  const char *afname = fmt_af_family(tor_addr_family(addr));

  control_event_server_status(LOG_NOTICE,
                              "CHECKING_REACHABILITY ORADDRESS=%s",
                              addr_buf);

  log_notice(LD_OR, "Now checking whether %s ORPort %s is reachable... "
             "(this may take up to %d minutes -- look for log "
             "messages indicating success)",
             afname, addr_buf,
             TIMEOUT_UNTIL_UNREACHABILITY_COMPLAINT/60);

  *have_informed_ptr = true;
}

/**
 * Return true if this module knows of no reason why we shouldn't publish
 * a server descriptor.
 **/
static bool
ready_to_publish(const or_options_t *options)
{
  return options->PublishServerDescriptor_ != NO_DIRINFO &&
         router_all_orports_seem_reachable(options);
}

/** Annotate that we found our ORPort reachable with a given address
 * family. */
void
router_orport_found_reachable(int family)
{
  const routerinfo_t *me = router_get_my_routerinfo();
  const or_options_t *options = get_options();
  const char *reachable_reason = "ORPort found reachable";
  bool *can_reach_ptr;
  if (family == AF_INET) {
    can_reach_ptr = &can_reach_or_port_ipv4;
  } else if (family == AF_INET6) {
    can_reach_ptr = &can_reach_or_port_ipv6;
  } else {
    tor_assert_nonfatal_unreached();
    return;
  }
  if (!*can_reach_ptr && me) {
    tor_addr_port_t ap;
    if (router_get_orport(me, &ap, family) < 0) {
      return;
    }
    char *address = tor_strdup(fmt_addrport_ap(&ap));

    *can_reach_ptr = true;

    log_notice(LD_OR,"Self-testing indicates your ORPort %s is reachable from "
               "the outside. Excellent.%s",
               address,
               ready_to_publish(options) ?
               " Publishing server descriptor." : "");

    /* Make sure our descriptor is marked to publish the IPv6 if it is now
     * reachable. This can change at runtime. */
    if (family == AF_INET6) {
      mark_my_descriptor_if_omit_ipv6_changes(reachable_reason, false);
    } else {
      mark_my_descriptor_dirty(reachable_reason);
    }
    /* This is a significant enough change to upload immediately,
     * at least in a test network */
    if (options->TestingTorNetwork == 1) {
      reschedule_descriptor_update_check();
    }
    control_event_server_status(LOG_NOTICE,
                                "REACHABILITY_SUCCEEDED ORADDRESS=%s",
                                address);
    tor_free(address);
  }
}

/** We have enough testing circuits open. Send a bunch of "drop"
 * cells down each of them, to exercise our bandwidth.
 *
 * May use IPv4 and IPv6 testing circuits (if available). */
void
router_perform_bandwidth_test(int num_circs, time_t now)
{
  int num_cells = (int)(get_options()->BandwidthRate * 10 /
                        CELL_MAX_NETWORK_SIZE);
  int max_cells = num_cells < CIRCWINDOW_START ?
                    num_cells : CIRCWINDOW_START;
  int cells_per_circuit = max_cells / num_circs;
  origin_circuit_t *circ = NULL;

  log_notice(LD_OR,"Performing bandwidth self-test...done.");
  while ((circ = circuit_get_next_by_purpose(circ,
                                             CIRCUIT_PURPOSE_TESTING))) {
    /* dump cells_per_circuit drop cells onto this circ */
    int i = cells_per_circuit;
    if (circ->base_.state != CIRCUIT_STATE_OPEN)
      continue;
    circ->base_.timestamp_dirty = now;
    while (i-- > 0) {
      if (relay_send_command_from_edge(0, TO_CIRCUIT(circ),
                                       RELAY_COMMAND_DROP,
                                       NULL, 0, circ->cpath->prev)<0) {
        return; /* stop if error */
      }
    }
  }
}
