/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2021, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * @file shutdown.c
 * @brief Code to free global resources used by Tor.
 *
 * In the future, this should all be handled by the subsystem manager. */

#include "core/or.h"

#include "app/config.h"
#include "app/statefile.h"
#include "app/toronion_main.h"
#include "app/shutdown.h"
#include "app/subsysmgr.h"
#include "core/connection.h"
#include "core/mainloop_pubsub.h"
#include "core/channeltls.h"
#include "core/circuitlist.h"
#include "core/circuitmux_ewma.h"
#include "core/circuitpadding.h"
#include "core/conflux_pool.h"
#include "core/connection_edge.h"
#include "core/dos.h"
#include "core/scheduler.h"
#include "feature/addressmap.h"
#include "feature/bridges.h"
#include "feature/entrynodes.h"
#include "feature/transports.h"
#include "feature/control.h"
#include "feature/control_auth.h"
#include "feature/authmode.h"
#include "feature/shared_random.h"
#include "feature/consdiffmgr.h"
#include "feature/dirserv.h"
#include "feature/routerparse.h"
#include "feature/hibernate.h"
#include "feature/hs_common.h"
#include "feature/microdesc.h"
#include "feature/networkstatus.h"
#include "feature/nodelist.h"
#include "feature/routerlist.h"
#include "feature/ext_orport.h"
#include "feature/relay_config.h"
#include "feature/bwhist.h"
#include "feature/geoip_stats.h"
#include "feature/rephist.h"
#include "lib/compat_libevent.h"
#include "lib/geoip.h"

void evdns_shutdown(int);

/** Do whatever cleanup is necessary before shutting Tor down. */
void
tor_cleanup(void)
{
  const or_options_t *options = get_options();
  if (options->command == CMD_RUN_TOR) {
    time_t now = time(NULL);
    /* Remove our pid file. We don't care if there was an error when we
     * unlink, nothing we could do about it anyways. */
    tor_remove_file(options->PidFile);
    /* Remove control port file */
    tor_remove_file(options->ControlPortWriteToFile);
    /* Remove cookie authentication file */
    {
      char *cookie_fname = get_controller_cookie_file_name();
      tor_remove_file(cookie_fname);
      tor_free(cookie_fname);
    }
    /* Remove Extended ORPort cookie authentication file */
    {
      char *cookie_fname = get_ext_or_auth_cookie_file_name();
      if (cookie_fname)
        tor_remove_file(cookie_fname);
      tor_free(cookie_fname);
    }
    if (accounting_is_enabled(options))
      accounting_record_bandwidth_usage(now, get_or_state());
    or_state_mark_dirty(get_or_state(), 0); /* force an immediate save. */
    or_state_save(now);
    if (authdir_mode(options)) {
      sr_save_and_cleanup();
    }
    if (authdir_mode_tests_reachability(options))
      rep_hist_record_mtbf_data(now, 0);
  }

  timers_shutdown();

  tor_free_all(0); /* We could move tor_free_all back into the ifdef below
                      later, if it makes shutdown unacceptably slow.  But for
                      now, leave it here: it's helped us catch bugs in the
                      past. */
}

/** Free all memory that we might have allocated somewhere.
 * If <b>postfork</b>, we are a worker process and we want to free
 * only the parts of memory that we won't touch. If !<b>postfork</b>,
 * Tor is shutting down and we should free everything.
 *
 * Helps us find the real leaks with sanitizers and the like. Also valgrind
 * should then report 0 reachable in its leak report (in an ideal world --
 * in practice libevent, SSL, libc etc never quite free everything). */
void
tor_free_all(int postfork)
{
  if (!postfork) {
    evdns_shutdown(1);
  }
  geoip_free_all();
  geoip_stats_free_all();
  routerlist_free_all();
  networkstatus_free_all();
  addressmap_free_all();
  dirserv_free_all();
  rep_hist_free_all();
  bwhist_free_all();
  circuit_free_all();
  conflux_pool_free_all();
  circpad_machines_free();
  entry_guards_free_all();
  pt_free_all();
  channel_tls_free_all();
  channel_free_all();
  connection_free_all();
  connection_edge_free_all();
  scheduler_free_all();
  nodelist_free_all();
  microdesc_free_all();
  routerparse_free_all();
  control_free_all();
  bridges_free_all();
  consdiffmgr_free_all();
  hs_free_all();
  dos_free_all();
  circuitmux_ewma_free_all();
  accounting_free_all();
  circpad_free_all();

  if (!postfork) {
    config_free_all();
    relay_config_free_all();
    or_state_free_all();
  }
  if (!postfork) {
#ifndef _WIN32
    tor_getpwnam(NULL);
#endif
  }
  /* stuff in main.c */

  tor_mainloop_disconnect_pubsub();

  if (!postfork) {
    release_lockfile();
  }

  subsystems_shutdown();

  /* Stuff in util.c and address.c*/
  if (!postfork) {
    esc_router_info(NULL);
  }
}
