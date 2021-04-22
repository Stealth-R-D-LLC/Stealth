// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPCONSTANTS_H_
#define _QPCONSTANTS_H_ 1

#include "chainparams.hpp"

#include <string>

// consecutive misses not total
unsigned int GetStakerMaxMisses(int nHeight);

static const uint64_t TRIL = 1000000000000;  // trillion, 1e12

static const int64_t RECIPROCAL_QPOS_INFLATION = 100;
// maximum number of seconds the best block time can fall behind
static const unsigned int QP_REGISTRY_MAX_FALL_BEHIND = 120;
static const unsigned int QP_REGISTRY_RECENT_BLOCKS = 32768;
static const unsigned int QP_STAKER_RECENT_BLOCKS = 4096;
static const unsigned int QP_NOOB_BLOCKS = QP_STAKER_RECENT_BLOCKS * 2;
// consecutive misses, not total
static const unsigned int QP_STAKER_MAX_MISSES_M = 512;
// testnet
static const unsigned int QP_STAKER_MAX_MISSES_1_T = 8;
static const unsigned int QP_STAKER_MAX_MISSES_2_T = 512;
// number of blocks to wait after being disabled
static const unsigned int QP_STAKER_REENABLE_WAIT_M = 8640;  // 12 hr
static const unsigned int QP_STAKER_REENABLE_WAIT_T = 6;     // 30 seconds
// makes it hard to game the rotation
static const int QP_ROUNDS = 1;
// block target time
static const int QP_TARGET_SPACING = 5;
// blocks per day
static const int QP_BLOCKS_PER_DAY = (60 * 60 * 24) / QP_TARGET_SPACING;
// fraction of money supply (stealthtoshis) to dock per round for inactive keys
static const int64_t DOCK_INACTIVE_FRACTION = 310000000000;
// number of blocks between snapshots
static const unsigned int BLOCKS_PER_SNAPSHOT = 24;
// number of recent snapshots to keep
static const unsigned int RECENT_SNAPSHOTS = 144;
// number of snapshots per permanent snapshot
static const unsigned int PERMANENT_SNAPSHOT_RATIO = 90;
// min and max staker alias lengths
static const unsigned int QP_MIN_ALIAS_LENGTH = 3;
static const unsigned int QP_MAX_ALIAS_LENGTH = 16;
// max staker metadata key length
static const unsigned int QP_MAX_META_KEY_LENGTH = 16;
// max staker metadata value length
static const unsigned int QP_MAX_META_VALUE_LENGTH = 40;
// minimum number of seconds between claims (prevents claim spam)
static const unsigned int QP_MIN_SECS_PER_CLAIM = 60 * 60 * 24;  // 1 day
// minimum pico power of a chain that will produce blocks
//    51% = 51e10/1e12
static const uint64_t QP_MIN_PICO_POWER = 510000000000u;
// return string when no alias or name exists for a staker
static const std::string STRING_NO_ALIAS = "*";

// because of asynchronous clock, there is no such thing as (hit | current)
enum QPSlotStatus
{
    QPSLOT_INVALID  = 0,
    QPSLOT_MISSED   = 1 << 0,
    QPSLOT_HIT      = 1 << 1,
    QPSLOT_CURRENT  = 1 << 2,
    QPSLOT_FUTURE   = 1 << 3
};

enum QPKeyType
{
    QPKEY_NONE       = 0,
    QPKEY_OWNER      = 1 << 0,
    QPKEY_MANAGER    = 1 << 1,
    QPKEY_DELEGATE   = 1 << 2,
    QPKEY_CONTROLLER = 1 << 3,
    QPKEY_OM         = QPKEY_OWNER | QPKEY_MANAGER,
    QPKEY_OD         = QPKEY_OWNER | QPKEY_DELEGATE,
    QPKEY_OC         = QPKEY_OWNER | QPKEY_CONTROLLER,
    QPKEY_OMD        = QPKEY_OM | QPKEY_DELEGATE,
    QPKEY_OMC        = QPKEY_OM | QPKEY_CONTROLLER,
    QPKEY_ODC        = QPKEY_OD | QPKEY_CONTROLLER,
    QPKEY_ANY        = QPKEY_OWNER | QPKEY_MANAGER |
                          QPKEY_DELEGATE | QPKEY_CONTROLLER
};

enum ProofTypes
{
    PROOFTYPE_POW  = 1 << 0,
    PROOFTYPE_POS  = 1 << 1,
    PROOFTYPE_QPOS = 1 << 2
};


uint32_t bit_length(uint32_t v);
uint32_t uisqrt(uint32_t n);
uint64_t bit_length(uint64_t v);
uint64_t uisqrt(uint64_t n);

#endif  /* _QPCONSTANTS_H_ */
