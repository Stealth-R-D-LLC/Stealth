/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2021, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * @file proxymode.c
 * @brief Determine whether we are trying to be a proxy.
 **/

#include "core/or.h"

#include "app/config.h"
#include "core/connection.h"
#include "core/port_cfg_st.h"
#include "feature/proxymode.h"

/** Return true iff we are trying to proxy client connections. */
int
proxy_mode(const or_options_t *options)
{
  (void)options;
  SMARTLIST_FOREACH_BEGIN(get_configured_ports(), const port_cfg_t *, p) {
    if (p->type == CONN_TYPE_AP_LISTENER ||
        p->type == CONN_TYPE_AP_TRANS_LISTENER ||
        p->type == CONN_TYPE_AP_DNS_LISTENER ||
        p->type == CONN_TYPE_AP_NATD_LISTENER)
      return 1;
  } SMARTLIST_FOREACH_END(p);
  return 0;
}
