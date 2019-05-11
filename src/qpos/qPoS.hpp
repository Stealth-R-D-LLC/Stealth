// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _STEALTHQPOS_H_
#define _STEALTHQPOS_H_ 1

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

enum QPKeyType
{
    QPKEY_NONE       = 0,
    QPKEY_OWNER      = 1 << 0,
    QPKEY_DELEGATE   = 1 << 1,
    QPKEY_CONTROLLER = 1 << 2,
    QPKEY_ALL        = QPKEY_OWNER | QPKEY_DELEGATE | QPKEY_CONTROLLER
};

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

// general purpose, can be used for any qpos tx type
class QPoSTxDetails
{
public:
    int t;               // indicates how to use data fields
    unsigned int id;            // staker ID (0 if N/A)
    std::string alias;          // alias (empty if N/A)
    std::vector<CPubKey> keys;  // length 1 or 3, latter for TX_PURCHASE3 only
    int32_t pcm;                // nonzero if TX_PURCHASE3 or TX_SETDELEGATE
    int64_t value;              // nonzero if TX_CLAIM
    QPoSTxDetails()
    {
        t = TX_NONSTANDARD;
        id = 0;
        alias = "";
        keys.clear();
        pcm = 0;
        value = 0;
    }
    IMPLEMENT_SERIALIZE
    (
        READWRITE(t);
        READWRITE(id);
        READWRITE(alias);
        READWRITE(keys);
        READWRITE(pcm);
        READWRITE(value);
    )
};

void ExtractPurchase(const valtype &vch, qpos_purchase &prchsRet);
void ExtractPurchase(const valtype &vch, QPoSTxDetails &deetsRet);
void ExtractSetKey(const valtype &vch, qpos_setkey &setkeyRet);
void ExtractSetKey(const valtype &vch, QPoSTxDetails &deetsRet);
void ExtractSetState(const valtype &vch, qpos_setstate &setstateRet);
void ExtractSetState(const valtype &vch, QPoSTxDetails &deetsRet);
void ExtractClaim(const valtype &vch, qpos_claim &claimRet);
void ExtractClaim(const valtype &vch, QPoSTxDetails &deetsRet);

#endif  /* _STEALTHQPOS_H_ */
