/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2021, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * @file relay_sys.c
 * @brief Subsystem definitions for the relay module.
 **/

#include "orconfig.h"
#include "core/or.h"

#include "feature/dns.h"
#include "feature/ext_orport.h"
#include "feature/relay_metrics.h"
#include "feature/onion_queue.h"
#include "feature/relay_periodic.h"
#include "feature/relay_sys.h"
#include "feature/routerkeys.h"
#include "feature/router.h"

#include "lib/subsys.h"

static int
subsys_relay_initialize(void)
{
  relay_metrics_init();
  relay_register_periodic_events();
  return 0;
}

static void
subsys_relay_shutdown(void)
{
  dns_free_all();
  ext_orport_free_all();
  clear_pending_onions();
  routerkeys_free_all();
  router_free_all();
  relay_metrics_free();
}

const struct subsys_fns_t sys_relay = {
  .name = "relay",
  SUBSYS_DECLARE_LOCATION(),
  .supported = true,
  .level = RELAY_SUBSYS_LEVEL,
  .initialize = subsys_relay_initialize,
  .shutdown = subsys_relay_shutdown,

  .get_metrics = relay_metrics_get_stores,
};
