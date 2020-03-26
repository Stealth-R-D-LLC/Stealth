// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _STEALTHQPOS_H_
#define _STEALTHQPOS_H_ 1

#include "QPConstants.hpp"
#include "QPTxDetails.hpp"

#include "script.h"

#define GETUINT16(first, last) \
   static_cast<uint16_t>(vchnum(first, last).GetValue())
#define GETUINT(first, last) \
   static_cast<unsigned int>(vchnum(first, last).GetValue())
#define GETINT(first, last) \
   static_cast<int>(vchnum(first, last).GetValue())
#define GETUINT32(first, last) \
   static_cast<uint32_t>(vchnum(first, last).GetValue())
#define GETINT32(first, last) \
   static_cast<int32_t>(vchnum(first, last).GetValue())
#define GETUINT64(first, last) \
   vchnum(first, last).GetValue()
#define GETINT64(first, last) \
   static_cast<int64_t>(vchnum(first, last).GetValue())

struct qpos_purchase
{
    std::string alias;          // case as registered, not lower
    std::vector<CPubKey> keys;  // length 1 or 3, latter for TX_PURCHASE3 only
    int32_t pcm;                // millipercent to delegate
    int64_t value;              // amount to pay
};

struct qpos_setkey
{
    QPKeyType keytype;
    unsigned int id;
    CPubKey key;
    uint32_t pcm;
};

struct qpos_setstate
{
    unsigned int id;
    bool enable;       // true: enable, false: disable
};

struct qpos_claim
{
    CPubKey key;
    int64_t value;
};

struct qpos_setmeta
{
    unsigned int id;
    std::string key;
    std::string value;
};

void ExtractPurchase(const valtype &vch, qpos_purchase &prchsRet);
void ExtractPurchase(const valtype &vch, QPTxDetails &deetsRet);
void ExtractSetKey(const valtype &vch, qpos_setkey &setkeyRet);
void ExtractSetKey(const valtype &vch, QPTxDetails &deetsRet);
void ExtractSetState(const valtype &vch, qpos_setstate &setstateRet);
void ExtractSetState(const valtype &vch, QPTxDetails &deetsRet);
void ExtractClaim(const valtype &vch, qpos_claim &claimRet);
void ExtractClaim(const valtype &vch, QPTxDetails &deetsRet);
void ExtractSetMeta(const valtype &vch, qpos_setmeta &setmetaRet);
void ExtractSetMeta(const valtype &vch, QPTxDetails &deetsRet);

#endif  /* _STEALTHQPOS_H_ */
