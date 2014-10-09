/* Copyright (c) 2014, Razor Developers */
/* See LICENSE for licensing information */

/**
 * \file stealth.h
 * \brief Headers for stealth.cpp
 **/

#ifndef TOR_RAZOR_H
#define TOR_RAZOR_H

#ifdef __cplusplus
extern "C" {
#endif

    char const* stealth_tor_data_directory(
    );

    char const* stealth_service_directory(
    );

    int check_interrupted(
    );

    void set_initialized(
    );

    void wait_initialized(
    );

#ifdef __cplusplus
}
#endif

#endif

