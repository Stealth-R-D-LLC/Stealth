// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPCONSTANTS_H_
#define _QPCONSTANTS_H_ 1

#include <stdint.h>

#include <string>

static const uint64_t TRIL = 1000000000000;  // trillion, 1e12

static const unsigned int QP_RECENT_BLOCKS = 4096;
static const unsigned int QP_NOOB_BLOCKS = QP_RECENT_BLOCKS * 2;
// makes it hard to game the rotation
static const int QP_ROUNDS = 16;
// block target time
static const int QP_TARGET_TIME = 5;
// blocks per day
static const int QP_BLOCKS_PER_DAY = (60 * 60 * 24) / QP_TARGET_TIME;
// fraction of money supply (stealthtoshis) to dock per round for inactive keys
static const int64_t DOCK_INACTIVE_FRACTION = 310000000000;
// number of blocks between snapshots
static const unsigned int BLOCKS_PER_SNAPSHOT = 24;
// number of snapshots to keep
static const unsigned int SNAPSHOTS_TO_KEEP = 180;
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

enum QPKeyType
{
    QPKEY_NONE       = 0,
    QPKEY_OWNER      = 1 << 0,
    QPKEY_DELEGATE   = 1 << 1,
    QPKEY_CONTROLLER = 1 << 2,
    QPKEY_OD         = QPKEY_OWNER | QPKEY_DELEGATE,
    QPKEY_OC         = QPKEY_OWNER | QPKEY_CONTROLLER,
    QPKEY_ANY        = QPKEY_OWNER | QPKEY_DELEGATE | QPKEY_CONTROLLER
};

enum ProofTypes
{
    PROOFTYPE_POW  = 1 << 0,
    PROOFTYPE_POS  = 1 << 1,
    PROOFTYPE_QPOS = 1 << 2
};

inline ProofTypes operator|(ProofTypes a, ProofTypes b)
{
    return static_cast<ProofTypes>(static_cast<int>(a) | static_cast<int>(b));
}

inline ProofTypes operator&(ProofTypes a, ProofTypes b)
{
    return static_cast<ProofTypes>(static_cast<int>(a) & static_cast<int>(b));
}

uint32_t bit_length(uint32_t v);
uint32_t uisqrt(uint32_t n);

#endif  /* _QPCONSTANTS_H_ */
