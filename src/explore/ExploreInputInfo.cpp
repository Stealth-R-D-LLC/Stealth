// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ExploreConstants.hpp"
#include "ExploreInputInfo.hpp"


using namespace std;

void ExploreInputInfo::SetNull()
{
    nVersion = ExploreInputInfo::CURRENT_VERSION;
    txid = 0;
    vin = 0;
    prev_txid = 0;
    prev_vout = 0;
    amount = 0;
}

ExploreInputInfo::ExploreInputInfo()
{
    SetNull();
}

ExploreInputInfo::ExploreInputInfo(const uint256& txidIn,
                                   const unsigned int vinIn,
                                   const uint256& prev_txidIn,
                                   const int prev_voutIn,
                                   const int64_t amountIn)
{
    nVersion = ExploreInputInfo::CURRENT_VERSION;
    txid = txidIn;
    vin = vinIn;
    prev_txid = prev_txidIn;
    prev_vout = prev_voutIn;
    amount = amountIn;
}

void ExploreInputInfo::AsJSON(json_spirit::Object& objRet) const
{
    objRet.clear();
    objRet.push_back(json_spirit::Pair("txid", txid.GetHex()));
    objRet.push_back(json_spirit::Pair("vin", (boost::int64_t)vin));
    objRet.push_back(json_spirit::Pair("prev_txid", prev_txid.GetHex()));
    objRet.push_back(json_spirit::Pair("prev_vout",
                                       (boost::int64_t)prev_vout));
    objRet.push_back(json_spirit::Pair("amount",
                                       ValueFromAmount(amount)));
}
