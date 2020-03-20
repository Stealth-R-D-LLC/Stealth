// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ExploreConstants.hpp"
#include "ExploreTxInfo.hpp"

#include <boost/foreach.hpp>

using namespace std;


const char* GetExploreTxType(int t)
{
    switch ((ExploreTxType)t)
    {
    case EXPLORE_TXTYPE_NONE: return "none";
    case EXPLORE_TXTYPE_COINBASE: return "coinbase";
    case EXPLORE_TXTYPE_COINSTAKE: return "coinstake";
    default: return "none";
    }  // switch
    return NULL;
}


void ExploreTxInfo::SetNull()
{
    nVersion = ExploreTxInfo::CURRENT_VERSION;
    blockhash = 0;
    blocktime = 0;
    height = -1;
    vtx = -1;
    destinations.clear();
    vin_size = 0;
    txtype = (int)EXPLORE_TXTYPE_NONE;
}

ExploreTxInfo::ExploreTxInfo()
{
    SetNull();
}

ExploreTxInfo::ExploreTxInfo(const uint256& blockhashIn,
              const unsigned int blocktimeIn,
              const int heightIn,
              const int vtxIn,
              const VecDest& destinationsIn,
              const unsigned int vin_sizeIn,
              const int txtypeIn)
{
    nVersion = ExploreTxInfo::CURRENT_VERSION;
    blockhash = blockhashIn;
    blocktime = blocktimeIn;
    height = heightIn;
    vtx = vtxIn;
    destinations = destinationsIn;
    vin_size = vin_sizeIn;
    txtype = txtypeIn;
}

void ExploreTxInfo::AsJSON(json_spirit::Object objRet) const
{
    objRet.push_back(json_spirit::Pair("blockhash", blockhash.GetHex()));
    objRet.push_back(json_spirit::Pair("blocktime",
                                       (boost::int64_t)blocktime));
    json_spirit::Array aryDest;
    BOOST_FOREACH(const ExploreDestination& dest, destinations)
    {
        json_spirit::Object objDest;
        dest.AsJSON(objDest);
        aryDest.push_back(objDest);
    }
    objRet.push_back(json_spirit::Pair("vtx", (boost::int64_t)vtx));
    objRet.push_back(json_spirit::Pair("destinations", aryDest));
    objRet.push_back(json_spirit::Pair("vin_size",
                                       (boost::int64_t)vin_size));
    if (txtype != (int)EXPLORE_TXTYPE_NONE)
    {
        string strTxType(GetExploreTxType(txtype));
        objRet.push_back(json_spirit::Pair("txtype", strTxType));
    }
}

