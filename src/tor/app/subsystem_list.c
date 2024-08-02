/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2021, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * @file subsystem_list.c
 * @brief List of Tor's subsystems.
 **/

#include "orconfig.h"
#include "app/subsysmgr.h"
#include "lib/compat_compiler.h"
#include "lib/torint.h"

#include "core/mainloop_sys.h"
#include "core/conflux_sys.h"
#include "core/dos_sys.h"
#include "core/or_sys.h"
#include "feature/btrack_sys.h"
#include "lib/compress_sys.h"
#include "lib/crypto_sys.h"
#include "lib/torerr_sys.h"
#include "lib/log_sys.h"
#include "lib/network_sys.h"
#include "lib/process_sys.h"
#include "lib/winprocess_sys.h"
#include "lib/thread_sys.h"
#include "lib/time_sys.h"
#include "lib/tortls_sys.h"
#include "lib/trace_sys.h"
#include "lib/wallclock_sys.h"
#include "lib/evloop_sys.h"

#include "feature/dirauth_sys.h"
#include "feature/hs_sys.h"
#include "feature/metrics_sys.h"
#include "feature/relay_sys.h"

#include <stddef.h>

/**
 * Global list of the subsystems in Tor, in the order of their initialization.
 * Want to know the exact level numbers?
 * We'll implement a level dump command in #31614.
 **/
const subsys_fns_t *tor_subsystems[] = {
  &sys_winprocess,
  &sys_torerr,

  &sys_wallclock,
  &sys_logging,
  &sys_threads,

  &sys_tracing,

  &sys_time,

  &sys_crypto,
  &sys_compress,
  &sys_network,
  &sys_tortls,

  &sys_evloop,
  &sys_process,

  &sys_mainloop,
  &sys_conflux,
  &sys_or,
  &sys_dos,

  &sys_relay,
  &sys_hs,

  &sys_btrack,

  &sys_dirauth,
  &sys_metrics,
};

const unsigned n_tor_subsystems = ARRAY_LENGTH(tor_subsystems);
