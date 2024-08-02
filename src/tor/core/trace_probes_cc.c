/* Copyright (c) 2021, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file trace_probes_cc.c
 * \brief Tracepoint provider source file for the cc subsystem. Probes
 *        are generated within this C file for LTTng-UST
 **/

#include "orconfig.h"

/*
 * Following section is specific to LTTng-UST.
 */
#ifdef USE_TRACING_INSTRUMENTATION_LTTNG

/* Header files that the probes need. */
#include "core/or.h"
#include "core/channel.h"
#include "core/circuit_st.h"
#include "core/circuitlist.h"
#include "core/congestion_control_common.h"
#include "core/congestion_control_st.h"
#include "core/connection_st.h"
#include "core/edge_connection_st.h"
#include "core/or_circuit_st.h"
#include "core/origin_circuit_st.h"

#define TRACEPOINT_DEFINE
#define TRACEPOINT_CREATE_PROBES

#include "core/trace_probes_cc.h"

#endif /* defined(USE_TRACING_INSTRUMENTATION_LTTNG) */
