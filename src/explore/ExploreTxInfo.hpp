// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLORETXINFO_H_
#define _EXPLORETXINFO_H_ 1

#include "uint256.h"

#include "ExploreDestination.hpp"

#include "json/json_spirit_utils.h"


typedef std::vector<ExploreDestination> VecDest;

enum ExploreTxType
{
    EXPLORE_TXTYPE_NONE,
    EXPLORE_TXTYPE_COINBASE,
    EXPLORE_TXTYPE_COINSTAKE
};

const char* GetExploreTxType(int t);


class ExploreTxInfo
{
private:
    int nVersion;
public:
    static const int CURRENT_VERSION = 1;
    uint256 blockhash;
    unsigned int blocktime;
    int height;
    VecDest destinations;
    unsigned int vin_size;
    int txtype;

    void SetNull();

    ExploreTxInfo();

    ExploreTxInfo(const uint256& blockhashIn,
                  const unsigned int blocktimeIn,
                  const int heightIn,
                  const VecDest& destinationsIn,
                  const unsigned int vin_sizeIn,
                  const int txtypeIn);

    void AsJSON(json_spirit::Object objRet) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(blockhash);
        READWRITE(blocktime);
        READWRITE(height);
        READWRITE(destinations);
        READWRITE(vin_size);
        READWRITE(txtype);
    )
};

#endif  /* _EXPLORETXINFO_H_ */
