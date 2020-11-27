// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLORETXINFO_H_
#define _EXPLORETXINFO_H_ 1

#include "uint256.h"

#include "ExploreDestination.hpp"

#include "json/json_spirit_utils.h"


typedef std::vector<ExploreDestination> VecDest;

// shameless overloading to save storage
enum ExploreTxFlags
{
    // tx types
    EXPLORE_TXFLAGS_NONE           = 0,
    EXPLORE_TXFLAGS_COINBASE       = 1 << 0,
    EXPLORE_TXFLAGS_COINSTAKE      = 1 << 1,
    // Junaeth output types
    EXPLORE_TXFLAGS_PURCHASE1      = 1 << 6,
    EXPLORE_TXFLAGS_PURCHASE3      = 1 << 7,
    EXPLORE_TXFLAGS_SETOWNER       = 1 << 8,
    EXPLORE_TXFLAGS_SETDELEGATE    = 1 << 9,
    EXPLORE_TXFLAGS_SETCONTROLLER  = 1 << 10,
    EXPLORE_TXFLAGS_ENABLE         = 1 << 11,
    EXPLORE_TXFLAGS_DISABLE        = 1 << 12,
    EXPLORE_TXFLAGS_CLAIM          = 1 << 13,
    EXPLORE_TXFLAGS_SETMETA        = 1 << 14
};

const char* GetExploreTxType(int t);


class ExploreTxInfo
{
private:
    int nVersion;
public:
    static const int CURRENT_VERSION = 1;

    static const int MASK_TXTYPE = EXPLORE_TXFLAGS_COINBASE |
                                   EXPLORE_TXFLAGS_COINSTAKE;

    uint256 blockhash;
    unsigned int blocktime;
    int height;
    int vtx;
    VecDest vfrom;
    VecDest vto;
    int txflags;

    void SetNull();

    ExploreTxInfo();

    ExploreTxInfo(const uint256& blockhashIn,
                  const unsigned int blocktimeIn,
                  const int heightIn,
                  const int vtxIn,
                  const VecDest& vfromIn,
                  const VecDest& vtoIn,
                  const int txflagsIn);

    bool IsNull() const;

    void FlagsAsJSON(json_spirit::Object objRet) const;
    void AsJSON(json_spirit::Object objRet) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(blockhash);
        READWRITE(blocktime);
        READWRITE(height);
        READWRITE(vtx);
        READWRITE(vfrom);
        READWRITE(vto);
        READWRITE(txflags);
    )
};

#endif  /* _EXPLORETXINFO_H_ */
