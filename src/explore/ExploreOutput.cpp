// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ExploreOutput.hpp"


using namespace json_spirit;
using namespace std;

extern Value ValueFromAmount(int64_t amount);

void ExploreOutput::SetNull()
{
    txid = 0;
    vout = 0;
    next_txid = 0;
    next_vin = 0;
    amount = -1;
    balance = 0;
}

void ExploreOutput::Set(const uint256& txidIn,
                        const int voutIn,
                        const int64_t amountIn,
                        const int64_t balanceIn)
{
    txid = txidIn;
    vout = voutIn;
    next_txid = 0;
    next_vin = 0;
    amount = amountIn;
    balance = balanceIn;
}

void ExploreOutput::Set(const uint256& txidIn,
                        const int voutIn,
                        const uint256& next_txidIn,
                        const int next_vinIn,
                        const int64_t amountIn,
                        const int64_t balanceIn)
{
    txid = txidIn;
    vout = voutIn;
    next_txid = next_txidIn;
    next_vin = next_vinIn;
    amount = amountIn;
    balance = balanceIn;
}

bool ExploreOutput::IsUnspent() const
{
    return (next_txid == 0);
}

bool ExploreOutput::IsSpent() const
{
    return (next_txid != 0);
}

bool ExploreOutput::Spend(const uint256& next_txidIn,
                          const int next_vinIn)
{
    if (IsSpent())
    {
        return false;
    }
    next_txid = next_txidIn;
    next_vin = next_vinIn;
    return true;
}

bool ExploreOutput::Unspend()
{
    if (!IsSpent())
    {
        return false;
    }
    next_txid = 0;
    next_vin = 0;
    return true;
}

void ExploreOutput::AsJSON(Object& objRet) const
{
    objRet.clear();
    objRet.push_back(Pair("txid", txid.GetHex()));
    objRet.push_back(Pair("vout", (boost::int64_t)vout));
    objRet.push_back(Pair("amount",
                                       ValueFromAmount(amount)));
    objRet.push_back(Pair("balance",
                                       ValueFromAmount(balance)));
    if (IsSpent())
    {
        objRet.push_back(Pair("isspent", true));
        objRet.push_back(Pair("next_txid", txid.GetHex()));
        objRet.push_back(Pair("next_vin",
                                           (boost::int64_t)next_vin));
    }
    else
    {
        objRet.push_back(Pair("isspent", false));
    }
}
