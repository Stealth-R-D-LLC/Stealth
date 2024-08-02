/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2021, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file dirlist.c
 * \brief Code to maintain our lists of directory authorities and
 *    fallback directories.
 *
 * For the directory authorities, we have a list containing the public
 * identity key, and contact points, for each authority.  The
 * authorities receive descriptors from relays, and publish consensuses,
 * descriptors, and microdescriptors.  This list is pre-configured.
 *
 * Fallback directories are well-known, stable, but untrusted directory
 * caches that clients which have not yet bootstrapped can use to get
 * their first networkstatus consensus, in order to find out where the
 * Tor network really is.  This list is pre-configured in
 * fallback_dirs.inc.  Every authority also serves as a fallback.
 *
 * Both fallback directories and directory authorities are are
 * represented by a dir_server_t.
 */

#include "core/or.h"

#include "app/config.h"
#include "app/resolve_addr.h"
#include "core/policies.h"
#include "feature/control_events.h"
#include "feature/authmode.h"
#include "feature/directory.h"
#include "feature/dirlist.h"
#include "feature/networkstatus.h"
#include "feature/nodelist.h"
#include "feature/routerlist.h"
#include "feature/routerset.h"
#include "feature/router.h"
#include "lib/resolve.h"

#include "feature/dir_server_st.h"
#include "feature/node_st.h"

/** Information about an (HTTP) dirport for a directory authority. */
struct auth_dirport_t {
  /** What is the intended usage for this dirport? One of AUTH_USAGE_* */
  auth_dirport_usage_t usage;
  /** What is the correct address/port ? */
  tor_addr_port_t dirport;
};

/** Global list of a dir_server_t object for each directory
 * authority. */
static smartlist_t *trusted_dir_servers = NULL;
/** Global list of dir_server_t objects for all directory authorities
 * and all fallback directory servers. */
static smartlist_t *fallback_dir_servers = NULL;

/** Helper: From a given trusted directory entry, add the v4 or/and v6 address
 * to the nodelist address set. */
static void
add_trusted_dir_to_nodelist_addr_set(const dir_server_t *dir)
{
  tor_assert(dir);
  tor_assert(dir->is_authority);

  /* Add IPv4 and then IPv6 if applicable. For authorities, we add the ORPort
   * and DirPort so re-entry into the network back to them is not possible. */
  nodelist_add_addr_to_address_set(&dir->ipv4_addr, dir->ipv4_orport,
                                   dir->ipv4_dirport);
  if (!tor_addr_is_null(&dir->ipv6_addr)) {
    /* IPv6 DirPort is not a thing yet for authorities. */
    nodelist_add_addr_to_address_set(&dir->ipv6_addr, dir->ipv6_orport, 0);
  }
  if (dir->auth_dirports) {
    SMARTLIST_FOREACH_BEGIN(dir->auth_dirports, const auth_dirport_t *, p) {
      nodelist_add_addr_to_address_set(&p->dirport.addr, 0, p->dirport.port);
    } SMARTLIST_FOREACH_END(p);
  }
}

/** Go over the trusted directory server list and add their address(es) to the
 * nodelist address set. This is called every time a new consensus is set. */
MOCK_IMPL(void,
dirlist_add_trusted_dir_addresses, (void))
{
  if (!trusted_dir_servers) {
    return;
  }

  SMARTLIST_FOREACH_BEGIN(trusted_dir_servers, const dir_server_t *, ent) {
    if (ent->is_authority) {
      add_trusted_dir_to_nodelist_addr_set(ent);
    }
  } SMARTLIST_FOREACH_END(ent);
}

/** Return the number of directory authorities whose type matches some bit set
 * in <b>type</b>  */
int
get_n_authorities(dirinfo_type_t type)
{
  int n = 0;
  if (!trusted_dir_servers)
    return 0;
  SMARTLIST_FOREACH(trusted_dir_servers, dir_server_t *, ds,
                    if (ds->type & type)
                      ++n);
  return n;
}

/** Return a smartlist containing a list of dir_server_t * for all
 * known trusted dirservers.  Callers must not modify the list or its
 * contents.
 */
smartlist_t *
router_get_trusted_dir_servers_mutable(void)
{
  if (!trusted_dir_servers)
    trusted_dir_servers = smartlist_new();

  return trusted_dir_servers;
}

smartlist_t *
router_get_fallback_dir_servers_mutable(void)
{
  if (!fallback_dir_servers)
    fallback_dir_servers = smartlist_new();

  return fallback_dir_servers;
}

const smartlist_t *
router_get_trusted_dir_servers(void)
{
  return router_get_trusted_dir_servers_mutable();
}

const smartlist_t *
router_get_fallback_dir_servers(void)
{
  return router_get_fallback_dir_servers_mutable();
}

/** Reset all internal variables used to count failed downloads of network
 * status objects. */
void
router_reset_status_download_failures(void)
{
  mark_all_dirservers_up(fallback_dir_servers);
}

/** Return the dir_server_t for the directory authority whose identity
 * key hashes to <b>digest</b>, or NULL if no such authority is known.
 */
dir_server_t *
router_get_trusteddirserver_by_digest(const char *digest)
{
  if (!trusted_dir_servers)
    return NULL;

  SMARTLIST_FOREACH(trusted_dir_servers, dir_server_t *, ds,
     {
       if (tor_memeq(ds->digest, digest, DIGEST_LEN))
         return ds;
     });

  return NULL;
}

/** Return the dir_server_t for the fallback dirserver whose identity
 * key hashes to <b>digest</b>, or NULL if no such fallback is in the list of
 * fallback_dir_servers. (fallback_dir_servers is affected by the FallbackDir
 * and UseDefaultFallbackDirs torrc options.)
 * The list of fallback directories includes the list of authorities.
 */
dir_server_t *
router_get_fallback_dirserver_by_digest(const char *digest)
{
  if (!fallback_dir_servers)
    return NULL;

  if (!digest)
    return NULL;

  SMARTLIST_FOREACH(fallback_dir_servers, dir_server_t *, ds,
     {
       if (tor_memeq(ds->digest, digest, DIGEST_LEN))
         return ds;
     });

  return NULL;
}

/** Return 1 if any fallback dirserver's identity key hashes to <b>digest</b>,
 * or 0 if no such fallback is in the list of fallback_dir_servers.
 * (fallback_dir_servers is affected by the FallbackDir and
 * UseDefaultFallbackDirs torrc options.)
 * The list of fallback directories includes the list of authorities.
 */
int
router_digest_is_fallback_dir(const char *digest)
{
  return (router_get_fallback_dirserver_by_digest(digest) != NULL);
}

/** Return the dir_server_t for the directory authority whose
 * v3 identity key hashes to <b>digest</b>, or NULL if no such authority
 * is known.
 */
MOCK_IMPL(dir_server_t *,
trusteddirserver_get_by_v3_auth_digest, (const char *digest))
{
  if (!trusted_dir_servers)
    return NULL;

  SMARTLIST_FOREACH(trusted_dir_servers, dir_server_t *, ds,
     {
       if (tor_memeq(ds->v3_identity_digest, digest, DIGEST_LEN) &&
           (ds->type & V3_DIRINFO))
         return ds;
     });

  return NULL;
}

/** Mark as running every dir_server_t in <b>server_list</b>. */
void
mark_all_dirservers_up(smartlist_t *server_list)
{
  if (server_list) {
    SMARTLIST_FOREACH_BEGIN(server_list, dir_server_t *, dir) {
      routerstatus_t *rs;
      node_t *node;
      dir->is_running = 1;
      node = node_get_mutable_by_id(dir->digest);
      if (node)
        node->is_running = 1;
      rs = router_get_mutable_consensus_status_by_id(dir->digest);
      if (rs) {
        rs->last_dir_503_at = 0;
        control_event_networkstatus_changed_single(rs);
      }
    } SMARTLIST_FOREACH_END(dir);
  }
  router_dir_info_changed();
}

/** Return true iff <b>digest</b> is the digest of the identity key of a
 * trusted directory matching at least one bit of <b>type</b>.  If <b>type</b>
 * is zero (NO_DIRINFO), or ALL_DIRINFO, any authority is okay. */
MOCK_IMPL(int, router_digest_is_trusted_dir_type,
        (const char *digest, dirinfo_type_t type))
{
  if (!trusted_dir_servers)
    return 0;
  if (authdir_mode(get_options()) && router_digest_is_me(digest))
    return 1;
  SMARTLIST_FOREACH(trusted_dir_servers, dir_server_t *, ent,
    if (tor_memeq(digest, ent->digest, DIGEST_LEN)) {
      return (!type) || ((type & ent->type) != 0);
    });
  return 0;
}

/** Return true iff the given address matches a trusted directory that matches
 * at least one bit of type.
 *
 * If type is NO_DIRINFO or ALL_DIRINFO, any authority is matched.
 *
 * Only ORPorts' addresses are considered.
 */
bool
router_addr_is_trusted_dir_type(const tor_addr_t *addr, dirinfo_type_t type)
{
  int family = tor_addr_family(addr);

  if (!trusted_dir_servers) {
    return false;
  }

  SMARTLIST_FOREACH_BEGIN(trusted_dir_servers, dir_server_t *, ent) {
    /* Ignore entries that don't match the given type. */
    if (type != NO_DIRINFO && (type & ent->type) == 0) {
      continue;
    }
    /* Match IPv4 or IPv6 address. */
    if ((family == AF_INET && tor_addr_eq(addr, &ent->ipv4_addr)) ||
        (family == AF_INET6 && tor_addr_eq(addr, &ent->ipv6_addr))) {
      return true;
    }
  } SMARTLIST_FOREACH_END(ent);

  return false;
}

/** Return an appropriate usage value describing which authdir port to use
 * for a given directory connection purpose.
 */
auth_dirport_usage_t
auth_dirport_usage_for_purpose(int purpose)
{
  switch (purpose) {
    case DIR_PURPOSE_FETCH_SERVERDESC:
    case DIR_PURPOSE_FETCH_EXTRAINFO:
    case DIR_PURPOSE_FETCH_CONSENSUS:
    case DIR_PURPOSE_FETCH_CERTIFICATE:
    case DIR_PURPOSE_FETCH_MICRODESC:
      return AUTH_USAGE_DOWNLOAD;

    case DIR_PURPOSE_UPLOAD_DIR:
      return AUTH_USAGE_UPLOAD;

    case DIR_PURPOSE_UPLOAD_VOTE:
    case DIR_PURPOSE_UPLOAD_SIGNATURES:
    case DIR_PURPOSE_FETCH_DETACHED_SIGNATURES:
    case DIR_PURPOSE_FETCH_STATUS_VOTE:
      return AUTH_USAGE_VOTING;

    case DIR_PURPOSE_SERVER:
    case DIR_PURPOSE_UPLOAD_HSDESC:
    case DIR_PURPOSE_FETCH_HSDESC:
    case DIR_PURPOSE_HAS_FETCHED_HSDESC:
    default:
      tor_assert_nonfatal_unreached();
      return AUTH_USAGE_LEGACY;
  }
}

/** Create a directory server at <b>address</b>:<b>port</b>, with OR identity
 * key <b>digest</b> which has DIGEST_LEN bytes.  If <b>address</b> is NULL,
 * add ourself.  If <b>is_authority</b>, this is a directory authority.  Return
 * the new directory server entry on success or NULL on failure. */
static dir_server_t *
dir_server_new(int is_authority,
               const char *nickname,
               const tor_addr_t *ipv4_addr,
               const char *hostname,
               uint16_t ipv4_dirport, uint16_t ipv4_orport,
               const tor_addr_port_t *addrport_ipv6,
               const char *digest, const char *v3_auth_digest,
               dirinfo_type_t type,
               double weight)
{
  dir_server_t *ent;
  char *hostname_ = NULL;

  tor_assert(digest);

  if (weight < 0)
    return NULL;

  if (!ipv4_addr) {
    return NULL;
  }

  if (!hostname)
    hostname_ = tor_addr_to_str_dup(ipv4_addr);
  else
    hostname_ = tor_strdup(hostname);

  ent = tor_malloc_zero(sizeof(dir_server_t));
  ent->nickname = nickname ? tor_strdup(nickname) : NULL;
  ent->address = hostname_;
  tor_addr_copy(&ent->ipv4_addr, ipv4_addr);
  ent->ipv4_dirport = ipv4_dirport;
  ent->ipv4_orport = ipv4_orport;
  ent->is_running = 1;
  ent->is_authority = is_authority;
  ent->type = type;
  ent->weight = weight;
  if (addrport_ipv6 && tor_addr_port_is_valid_ap(addrport_ipv6, 0)) {
    if (tor_addr_family(&addrport_ipv6->addr) != AF_INET6) {
      log_warn(LD_BUG, "Hey, I got a non-ipv6 addr as addrport_ipv6.");
      tor_addr_make_unspec(&ent->ipv6_addr);
    } else {
      tor_addr_copy(&ent->ipv6_addr, &addrport_ipv6->addr);
      ent->ipv6_orport = addrport_ipv6->port;
    }
  } else {
    tor_addr_make_unspec(&ent->ipv6_addr);
  }

  memcpy(ent->digest, digest, DIGEST_LEN);
  if (v3_auth_digest && (type & V3_DIRINFO))
    memcpy(ent->v3_identity_digest, v3_auth_digest, DIGEST_LEN);

  if (nickname)
    tor_asprintf(&ent->description, "directory server \"%s\" at %s:%" PRIu16,
                 nickname, hostname_, ipv4_dirport);
  else
    tor_asprintf(&ent->description, "directory server at %s:%" PRIu16,
                 hostname_, ipv4_dirport);

  tor_addr_copy(&ent->fake_status.ipv4_addr, &ent->ipv4_addr);
  tor_addr_copy(&ent->fake_status.ipv6_addr, &ent->ipv6_addr);
  memcpy(ent->fake_status.identity_digest, digest, DIGEST_LEN);
  if (nickname)
    strlcpy(ent->fake_status.nickname, nickname,
            sizeof(ent->fake_status.nickname));
  else
    ent->fake_status.nickname[0] = '\0';
  ent->fake_status.ipv4_dirport = ent->ipv4_dirport;
  ent->fake_status.ipv4_orport = ent->ipv4_orport;
  ent->fake_status.ipv6_orport = ent->ipv6_orport;
  ent->fake_status.is_authority = !! is_authority;

  return ent;
}

/** Create an authoritative directory server at <b>address</b>:<b>port</b>,
 * with identity key <b>digest</b>.  If <b>ipv4_addr_str</b> is NULL, add
 * ourself.  Return the new trusted directory server entry on success or NULL
 * if we couldn't add it. */
dir_server_t *
trusted_dir_server_new(const char *nickname, const char *address,
                       uint16_t ipv4_dirport, uint16_t ipv4_orport,
                       const tor_addr_port_t *ipv6_addrport,
                       const char *digest, const char *v3_auth_digest,
                       dirinfo_type_t type, double weight)
{
  tor_addr_t ipv4_addr;
  char *hostname=NULL;
  dir_server_t *result;

  if (!address) { /* The address is us; we should guess. */
    if (!find_my_address(get_options(), AF_INET, LOG_WARN, &ipv4_addr,
                         NULL, &hostname)) {
      log_warn(LD_CONFIG,
               "Couldn't find a suitable address when adding ourself as a "
               "trusted directory server.");
      return NULL;
    }
    if (!hostname)
      hostname = tor_addr_to_str_dup(&ipv4_addr);

    if (!hostname)
      return NULL;
  } else {
    if (tor_addr_lookup(address, AF_INET, &ipv4_addr)) {
      log_warn(LD_CONFIG,
               "Unable to lookup address for directory server at '%s'",
               address);
      return NULL;
    }
    hostname = tor_strdup(address);
  }

  result = dir_server_new(1, nickname, &ipv4_addr, hostname,
                          ipv4_dirport, ipv4_orport,
                          ipv6_addrport,
                          digest,
                          v3_auth_digest, type, weight);

  if (ipv4_dirport) {
    tor_addr_port_t p;
    memset(&p, 0, sizeof(p));
    tor_addr_copy(&p.addr, &ipv4_addr);
    p.port = ipv4_dirport;
    trusted_dir_server_add_dirport(result, AUTH_USAGE_LEGACY, &p);
  }
  tor_free(hostname);
  return result;
}

/**
 * Add @a dirport as an HTTP DirPort contact point for the directory authority
 * @a ds, for use when contacting that authority for the given @a usage.
 *
 * Multiple ports of the same usage are allowed; if present, then only
 * the first one of each address family is currently used.
 */
void
trusted_dir_server_add_dirport(dir_server_t *ds,
                               auth_dirport_usage_t usage,
                               const tor_addr_port_t *dirport)
{
  tor_assert(ds);
  tor_assert(dirport);

  if (BUG(! ds->is_authority)) {
    return;
  }

  if (ds->auth_dirports == NULL) {
    ds->auth_dirports = smartlist_new();
  }

  auth_dirport_t *port = tor_malloc_zero(sizeof(auth_dirport_t));
  port->usage = usage;
  tor_addr_port_copy(&port->dirport, dirport);
  smartlist_add(ds->auth_dirports, port);
}

/**
 * Helper for trusted_dir_server_get_dirport: only return the exact requested
 * usage type.
 */
const tor_addr_port_t *
trusted_dir_server_get_dirport_exact(const dir_server_t *ds,
                                     auth_dirport_usage_t usage,
                                     int addr_family)
{
  tor_assert(ds);
  tor_assert_nonfatal(addr_family == AF_INET || addr_family == AF_INET6);
  if (ds->auth_dirports == NULL)
    return NULL;

  SMARTLIST_FOREACH_BEGIN(ds->auth_dirports, const auth_dirport_t *, port) {
    if (port->usage == usage &&
        tor_addr_family(&port->dirport.addr) == addr_family) {
      return &port->dirport;
    }
  } SMARTLIST_FOREACH_END(port);

  return NULL;
}

/**
 * Return the DirPort of the authority @a ds for with the usage type
 * @a usage and address family @a addr_family.  If none is found, try
 * again with an AUTH_USAGE_LEGACY dirport, if there is one.  Return NULL
 * if no port can be found.
 */
const tor_addr_port_t *
trusted_dir_server_get_dirport(const dir_server_t *ds,
                               auth_dirport_usage_t usage,
                               int addr_family)
{
  const tor_addr_port_t *port;

  while (1) {
    port = trusted_dir_server_get_dirport_exact(ds, usage, addr_family);
    if (port)
      return port;

    // If we tried LEGACY, there is no fallback from this point.
    if (usage == AUTH_USAGE_LEGACY)
      return NULL;

    // Try again with LEGACY.
    usage = AUTH_USAGE_LEGACY;
  }
}

/** Return a new dir_server_t for a fallback directory server at
 * <b>addr</b>:<b>or_port</b>/<b>dir_port</b>, with identity key digest
 * <b>id_digest</b> */
dir_server_t *
fallback_dir_server_new(const tor_addr_t *ipv4_addr,
                        uint16_t ipv4_dirport, uint16_t ipv4_orport,
                        const tor_addr_port_t *addrport_ipv6,
                        const char *id_digest, double weight)
{
  return dir_server_new(0, NULL, ipv4_addr, NULL, ipv4_dirport, ipv4_orport,
                        addrport_ipv6, id_digest, NULL, ALL_DIRINFO, weight);
}

/** Add a directory server to the global list(s). */
void
dir_server_add(dir_server_t *ent)
{
  if (!trusted_dir_servers)
    trusted_dir_servers = smartlist_new();
  if (!fallback_dir_servers)
    fallback_dir_servers = smartlist_new();

  if (ent->is_authority)
    smartlist_add(trusted_dir_servers, ent);

  smartlist_add(fallback_dir_servers, ent);
  router_dir_info_changed();
}

#define dir_server_free(val) \
  FREE_AND_NULL(dir_server_t, dir_server_free_, (val))

/** Free storage held in <b>ds</b>. */
static void
dir_server_free_(dir_server_t *ds)
{
  if (!ds)
    return;

  if (ds->auth_dirports) {
    SMARTLIST_FOREACH(ds->auth_dirports, auth_dirport_t *, p, tor_free(p));
    smartlist_free(ds->auth_dirports);
  }
  tor_free(ds->nickname);
  tor_free(ds->description);
  tor_free(ds->address);
  tor_free(ds);
}

/** Remove all members from the list of dir servers. */
void
clear_dir_servers(void)
{
  if (fallback_dir_servers) {
    SMARTLIST_FOREACH(fallback_dir_servers, dir_server_t *, ent,
                      dir_server_free(ent));
    smartlist_clear(fallback_dir_servers);
  } else {
    fallback_dir_servers = smartlist_new();
  }
  if (trusted_dir_servers) {
    smartlist_clear(trusted_dir_servers);
  } else {
    trusted_dir_servers = smartlist_new();
  }
  router_dir_info_changed();
}

void
dirlist_free_all(void)
{
  clear_dir_servers();
  smartlist_free(trusted_dir_servers);
  smartlist_free(fallback_dir_servers);
  trusted_dir_servers = fallback_dir_servers = NULL;
}
