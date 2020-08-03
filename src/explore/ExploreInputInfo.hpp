// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLOREINPUTINFO_H_
#define _EXPLOREINPUTINFO_H_ 1

#include "uint256.h"
#include "serialize.h"
#include "bitcoinrpc.h"

#include "json/json_spirit_utils.h"



class ExploreInputInfo
{
private:
    int nVersion;
public:
    static const int CURRENT_VERSION = 1;
    uint256 txid;
    unsigned int vin;
    uint256 prev_txid;
    int prev_vout;
    int64_t amount;

    void SetNull();

    ExploreInputInfo();

    ExploreInputInfo(const uint256& txidIn,
                     const unsigned int vinIn,
                     const uint256& prev_txidIn,
                     const int prev_voutIn,
                     const int64_t amountIn);

    void AsJSON(json_spirit::Object& objRet) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(txid);
        READWRITE(vin);
        READWRITE(prev_txid);
        READWRITE(prev_vout);
        READWRITE(amount);
    )
};

#endif  /* _EXPLOREINPUTINFO_H_ */
