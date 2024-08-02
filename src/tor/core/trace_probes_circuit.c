/* Copyright (c) 2020-2021, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file trace_probes_circuit.c
 * \brief Tracepoint provider source file for the circuit subsystem. Probes
 *        are generated within this C file for LTTng-UST
 **/

#include "orconfig.h"

/*
 * Following section is specific to LTTng-UST.
 */
#ifdef USE_TRACING_INSTRUMENTATION_LTTNG

/* Header files that the probes need. */
#include "core/circuitlist.h"
#include "core/crypt_path_st.h"
#include "core/extend_info_st.h"
#include "core/or.h"
#include "core/or_circuit_st.h"
#include "core/origin_circuit_st.h"

#define TRACEPOINT_DEFINE
#define TRACEPOINT_CREATE_PROBES

#include "core/trace_probes_circuit.h"

#endif /* defined(USE_TRACING_INSTRUMENTATION_LTTNG) */
