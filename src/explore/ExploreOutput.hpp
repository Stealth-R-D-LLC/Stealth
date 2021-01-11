// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLOREOUTPUTINFO_H_
#define _EXPLOREOUTPUTINFO_H_ 1

#include "uint256.h"
#include "serialize.h"

#include "json/json_spirit_utils.h"


class ExploreOutput
{
public:
    uint256 txid = 0;
    int vout = 0;
    uint256 next_txid = 0;
    int next_vin = 0;
    int64_t amount = -1;
    int64_t balance = 0;

    void SetNull();

    void Set(const uint256& txidIn,
             const int voutIn,
             const int64_t amountIn,
             const int64_t balanceIn);

    void Set(const uint256& txidIn,
             const int voutIn,
             const uint256& next_txidIn,
             const int next_vinIn,
             const int64_t amountIn,
             const int64_t balanceIn);

    bool IsUnspent() const;

    bool IsSpent() const;

    bool Spend(const uint256& next_txidIn, const int next_vinIn);

    bool Unspend();

    void AsJSON(json_spirit::Object& objRet) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(txid);
        READWRITE(vout);
        READWRITE(next_txid);
        READWRITE(next_vin);
        READWRITE(amount);
        READWRITE(balance);
    )
};

#endif  /* _EXPLOREOUTPUTINFO_H_ */
