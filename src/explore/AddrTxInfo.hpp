// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLOREADDRTXINFO_H_
#define _EXPLOREADDRTXINFO_H_ 1

#include "InOutInfo.hpp"


class AddrTxInfo
{
public:
    std::string address;
    std::set<InOutInfo> inouts;
    ExploreTx extx;

    void SetNull();

    AddrTxInfo();

    bool operator < (const AddrTxInfo& other) const;
    bool operator > (const AddrTxInfo& other) const;

    const InOutInfo* GetLast() const;
    const uint256* GetTxID() const;
    int64_t GetFinalBalance() const;

    void AsJSON(const int nBestHeight,
                const unsigned int i,
                json_spirit::Object& objRet) const;

    void AsJSON(const int nBestHeight, json_spirit::Object& objRet) const;
};


#endif  /* _EXPLOREADDRTXINFO_H_ */
