// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLOREADDRINOUTINFO_H_
#define _EXPLOREADDRINOUTINFO_H_ 1

#include "InOutInfo.hpp"

class AddrInOutInfo
{
public:
    std::string address;
    InOutInfo inoutinfo;

    void SetNull();
    AddrInOutInfo();

    void Set(const int flag);
    AddrInOutInfo(const int flag);

    void Set(const std::string& addressIn,
             const InOutInfo& inoutinfoIn);
    AddrInOutInfo(const std::string& addressIn,
                  const InOutInfo& inoutinfoIn);

    bool operator < (const AddrInOutInfo& other) const;
    bool operator > (const AddrInOutInfo& other) const;

    void BecomeInput();
    void BecomeOutput();

    bool IsInput() const;
    bool IsOutput() const;

    const uint256* GetTxID() const;

    int64_t GetBalanceChange() const;

    ExploreInput& GetInput();
    ExploreOutput& GetOutput();

    void AsJSON(const int nBestHeight,
                const ExploreTx& tx,
                json_spirit::Object& objRet) const;

    void AsJSON(json_spirit::Object& objRet) const;
};


#endif  /* _EXPLOREADDRINOUTINFO_H_ */
