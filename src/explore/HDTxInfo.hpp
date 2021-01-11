// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLOREHDTXINFO_H_
#define _EXPLOREHDTXINFO_H_ 1

#include "AddrInOutInfo.hpp"

class HDTxInfo
{
public:
    std::set<AddrInOutInfo> addrinouts;
    ExploreTx extx;

    void SetNull();

    HDTxInfo();

    bool operator < (const HDTxInfo& other) const;
    bool operator > (const HDTxInfo& other) const;

    const AddrInOutInfo* GetLast() const;
    const uint256* GetTxID() const;

    void GetBalanceChanges(std::map<std::string*, int64_t>& mapChanges) const;
    int64_t GetAccountBalanceChange() const;

    void AsJSON(const int nBestHeight,
                const unsigned int i,
                json_spirit::Object& objRet) const;

    void AsJSON(const int nBestHeight, json_spirit::Object& objRet) const;
};

#endif  /* _EXPLOREHDTXINFO_H_ */
