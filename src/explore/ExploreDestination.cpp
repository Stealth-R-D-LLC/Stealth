// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ExploreDestination.hpp"

#include "bitcoinrpc.h"

using namespace std;

void ExploreDestination::SetNull()
{
    nVersion = ExploreDestination::CURRENT_VERSION;
    addresses.clear();
    required = -1;
    amount = -1;
    type.clear();
}

ExploreDestination::ExploreDestination()
{
    SetNull();
}

ExploreDestination::ExploreDestination(const vector<string>& addressesIn,
                                       const int requiredIn,
                                       const int64_t& amountIn,
                                       const string& typeIn)
{
    nVersion = ExploreDestination::CURRENT_VERSION;
    addresses = addressesIn;
    required = requiredIn;
    amount = amountIn;
    type = typeIn;
}

void ExploreDestination::AsJSON(json_spirit::Object& objRet) const
{
    objRet.clear();
    if (!addresses.empty())
    {
        json_spirit::Array aryAddr;
        BOOST_FOREACH(const string& a, addresses)
        {
            aryAddr.push_back(a);
        }
        objRet.push_back(json_spirit::Pair("addresses", aryAddr));
        objRet.push_back(json_spirit::Pair("reqSigs",
                                           (boost::int64_t)required));
    }
    objRet.push_back(json_spirit::Pair("amount",
                                       ValueFromAmount(amount)));
    objRet.push_back(json_spirit::Pair("type", type));
}
