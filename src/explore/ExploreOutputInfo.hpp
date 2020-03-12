// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLOREOUTPUTINFO_H_
#define _EXPLOREOUTPUTINFO_H_ 1

#include "uint256.h"
#include "serialize.h"

#include "json/json_spirit_utils.h"

class ExploreOutputInfo
{
private:
    int nVersion;
public:
    static const int CURRENT_VERSION = 1;

    uint256 txid;
    unsigned int vout;
    int64_t amount;
    uint256 next_txid;
    unsigned int next_vin;

    void SetNull();

    ExploreOutputInfo();

    ExploreOutputInfo(const uint256& txidIn,
                      const unsigned int voutIn,
                      const int64_t amountIn);

    ExploreOutputInfo(const uint256& txidIn,
                      const unsigned int voutIn,
                      const int64_t amountIn,
                      const uint256& next_txidIn,
                      const unsigned int next_vinIn);

    bool IsUnspent() const;

    bool IsSpent() const;

    bool Spend(const uint256& next_txidIn, const unsigned int next_vinIn);

    bool Unspend();

    void AsJSON(json_spirit::Object& objRet) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(txid);
        READWRITE(vout);
        READWRITE(amount);
        READWRITE(next_txid);
        READWRITE(next_vin);
    )
};

#endif  /* _EXPLOREOUTPUTINFO_H_ */
