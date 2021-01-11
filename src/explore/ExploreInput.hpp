// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLOREINPUTINFO_H_
#define _EXPLOREINPUTINFO_H_ 1

#include "uint256.h"
#include "serialize.h"

#include "json/json_spirit_utils.h"


class ExploreInput
{
public:
    uint256 txid = 0;
    int vin = 0;
    uint256 prev_txid = 0;
    int prev_vout = 0;
    int64_t amount = 0;
    int64_t balance = 0;

    void SetNull();

    void Set(const uint256& txidIn,
             const int vinIn,
             const uint256& prev_txidIn,
             const int prev_voutIn,
             const int64_t amountIn,
             const int64_t balanceIn);

void AsJSON(json_spirit::Object& objRet) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(txid);
        READWRITE(vin);
        READWRITE(prev_txid);
        READWRITE(prev_vout);
        READWRITE(amount);
        READWRITE(balance);
    )
};

#endif  /* _EXPLOREINPUTINFO_H_ */
