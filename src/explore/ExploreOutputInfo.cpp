// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ExploreOutputInfo.hpp"

#include "serialize.h"
#include "bitcoinrpc.h"


using namespace std;

void ExploreOutputInfo::SetNull()
{
    nVersion = ExploreOutputInfo::CURRENT_VERSION;
    txid = 0;
    vout = 0;
    amount = -1;
    balance = 0;
    next_txid = 0;
    next_vin = 0;
}

ExploreOutputInfo::ExploreOutputInfo()
{
    SetNull();
}

ExploreOutputInfo::ExploreOutputInfo(const uint256& txidIn,
                                     const unsigned int voutIn,
                                     const int64_t amountIn,
                                     const int64_t balanceIn)
{
    nVersion = ExploreOutputInfo::CURRENT_VERSION;
    txid = txidIn;
    vout = voutIn;
    amount = amountIn;
    balance = balanceIn;
    next_txid = 0;
    next_vin = 0;
}

ExploreOutputInfo::ExploreOutputInfo(const uint256& txidIn,
                                     const unsigned int voutIn,
                                     const int64_t amountIn,
                                     const int64_t balanceIn,
                                     const uint256& next_txidIn,
                                     const unsigned int next_vinIn)
{
    nVersion = ExploreOutputInfo::CURRENT_VERSION;
    txid = txidIn;
    vout = voutIn;
    amount = amountIn;
    balance = balanceIn;
    next_txid = next_txidIn;
    next_vin = next_vinIn;
}

bool ExploreOutputInfo::IsUnspent() const
{
    return (next_txid == 0);
}

bool ExploreOutputInfo::IsSpent() const
{
    return (next_txid != 0);
}

bool ExploreOutputInfo::Spend(const uint256& next_txidIn,
                              const unsigned int next_vinIn)
{
    if (IsSpent())
    {
        return false;
    }
    next_txid = next_txidIn;
    next_vin = next_vinIn;
    return true;
}

bool ExploreOutputInfo::Unspend()
{
    if (!IsSpent())
    {
        return false;
    }
    next_txid = 0;
    next_vin = 0;
    return true;
}

void ExploreOutputInfo::AsJSON(json_spirit::Object& objRet) const
{
    objRet.clear();
    objRet.push_back(json_spirit::Pair("txid", txid.GetHex()));
    objRet.push_back(json_spirit::Pair("vout", (boost::int64_t)vout));
    objRet.push_back(json_spirit::Pair("amount",
                                       ValueFromAmount(amount)));
    objRet.push_back(json_spirit::Pair("amount",
                                       ValueFromAmount(balance)));
    objRet.push_back(json_spirit::Pair("next_txid", txid.GetHex()));
    objRet.push_back(json_spirit::Pair("next_vin",
                                       (boost::int64_t)next_vin));
}
