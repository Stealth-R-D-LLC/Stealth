// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ExploreDestination.hpp"

#include <boost/foreach.hpp>

using namespace json_spirit;
using namespace std;

extern Value ValueFromAmount(int64_t amount);

void ExploreDestination::SetNull()
{
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
    addresses = addressesIn;
    required = requiredIn;
    amount = amountIn;
    type = typeIn;
}


bool ExploreDestination::HasAddress(const string& sAddr) const
{
    // linear search should be fast enough because of expected
    //    small size of addresses
    return find(addresses.begin(), addresses.end(), sAddr) != addresses.end();
}

bool ExploreDestination::IsSameAs(const string& sAddr) const
{
    return (addresses.size() == 1) && (addresses[0] == sAddr);
}
    
void ExploreDestination::AsJSON(Object& objRet) const
{
    objRet.clear();
    if (!addresses.empty())
    {
        Array aryAddr;
        BOOST_FOREACH(const string& a, addresses)
        {
            aryAddr.push_back(a);
        }
        objRet.push_back(Pair("addresses", aryAddr));
        objRet.push_back(Pair("reqSigs", (int64_t)required));
    }
    objRet.push_back(Pair("amount", ValueFromAmount(amount)));
    objRet.push_back(Pair("type", type));
}
