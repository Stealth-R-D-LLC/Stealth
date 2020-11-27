// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ExploreConstants.hpp"
#include "ExploreTxInfo.hpp"

#include <boost/foreach.hpp>

using namespace std;

const char* GetExploreTxType(int t)
{
    switch (t & ExploreTxInfo::MASK_TXTYPE)
    {
    case (int)EXPLORE_TXFLAGS_NONE:      return "none";
    case (int)EXPLORE_TXFLAGS_COINBASE:  return "coinbase";
    case (int)EXPLORE_TXFLAGS_COINSTAKE: return "coinstake";
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
    vfrom.clear();
    vto.clear();
    txflags = (int)EXPLORE_TXFLAGS_NONE;
}

ExploreTxInfo::ExploreTxInfo()
{
    SetNull();
}

ExploreTxInfo::ExploreTxInfo(const uint256& blockhashIn,
              const unsigned int blocktimeIn,
              const int heightIn,
              const int vtxIn,
              const VecDest& vfromIn,
              const VecDest& vtoIn,
              const int txflagsIn)
{
    nVersion = ExploreTxInfo::CURRENT_VERSION;
    blockhash = blockhashIn;
    blocktime = blocktimeIn;
    height = heightIn;
    vtx = vtxIn;
    vfrom = vfromIn;
    vto = vtoIn;
    txflags = txflagsIn;
}

bool ExploreTxInfo::IsNull() const
{
    if ( (blockhash == 0) &&
         (blocktime == 0) &&
         (height == -1) &&
         (vtx == -1) &&
         (vfrom.empty()) &&
         (vto.empty()) &&
         (txflags == (int)EXPLORE_TXFLAGS_NONE) )
    {
        return true;
    }
    return false;
}

void ExploreTxInfo::FlagsAsJSON(json_spirit::Object objRet) const
{
    json_spirit::Array aryFlags;
    if (txflags & EXPLORE_TXFLAGS_COINBASE)
    {
        aryFlags.push_back("coinbase");
    }
    if (txflags & EXPLORE_TXFLAGS_COINSTAKE)
    {
        aryFlags.push_back("coinstake");
    }
    if (txflags & EXPLORE_TXFLAGS_PURCHASE1)
    {
        aryFlags.push_back("purchase1");
    }
    if (txflags & EXPLORE_TXFLAGS_PURCHASE3)
    {
        aryFlags.push_back("purchase3");
    }
    if (txflags & EXPLORE_TXFLAGS_SETOWNER)
    {
        aryFlags.push_back("setowner");
    }
    if (txflags & EXPLORE_TXFLAGS_SETDELEGATE)
    {
        aryFlags.push_back("setdelegate");
    }
    if (txflags & EXPLORE_TXFLAGS_SETCONTROLLER)
    {
        aryFlags.push_back("setcontroller");
    }
    if (txflags & EXPLORE_TXFLAGS_ENABLE)
    {
        aryFlags.push_back("enable");
    }
    if (txflags & EXPLORE_TXFLAGS_DISABLE)
    {
        aryFlags.push_back("disable");
    }
    if (txflags & EXPLORE_TXFLAGS_CLAIM)
    {
        aryFlags.push_back("claim");
    }
    if (txflags & EXPLORE_TXFLAGS_SETMETA)
    {
        aryFlags.push_back("setmeta");
    }
    if (aryFlags.size() > 0)
    {
        objRet.push_back(json_spirit::Pair("txflags", aryFlags));
    }
}

void ExploreTxInfo::AsJSON(json_spirit::Object objRet) const
{
    objRet.push_back(json_spirit::Pair("blockhash", blockhash.GetHex()));
    objRet.push_back(json_spirit::Pair("blocktime",
                                       (boost::int64_t)blocktime));

    objRet.push_back(json_spirit::Pair("vtx", (boost::int64_t)vtx));
    json_spirit::Array aryFrom;
    BOOST_FOREACH(const ExploreDestination& dest, vfrom)
    {
        json_spirit::Object objDest;
        dest.AsJSON(objDest);
        aryFrom.push_back(objDest);
    }
    objRet.push_back(json_spirit::Pair("sources", aryFrom));
    json_spirit::Array aryTo;
    BOOST_FOREACH(const ExploreDestination& dest, vto)
    {
        json_spirit::Object objDest;
        dest.AsJSON(objDest);
        aryTo.push_back(objDest);
    }
    objRet.push_back(json_spirit::Pair("destinations", aryTo));
    if (txflags & MASK_TXTYPE)
    {
        string strTxType(GetExploreTxType(txflags));
        objRet.push_back(json_spirit::Pair("txtype", strTxType));
    }
    FlagsAsJSON(objRet);
}

