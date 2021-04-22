// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ExploreConstants.hpp"
#include "ExploreTx.hpp"

#include <boost/foreach.hpp>

using namespace json_spirit;
using namespace std;

const char* GetExploreTxType(int t)
{
    switch (t & ExploreTx::MASK_TXTYPE)
    {
    case (int)EXPLORE_TXFLAGS_NONE:      return "none";
    case (int)EXPLORE_TXFLAGS_COINBASE:  return "coinbase";
    case (int)EXPLORE_TXFLAGS_COINSTAKE: return "coinstake";
    default: return "none";
    }  // switch
    return NULL;
}

void ExploreTx::SetNull()
{
    nVersion = ExploreTx::CURRENT_VERSION;
    blockhash = 0;
    blocktime = 0;
    height = -1;
    vtx = -1;
    vfrom.clear();
    vto.clear();
    txflags = (int)EXPLORE_TXFLAGS_NONE;
}

ExploreTx::ExploreTx()
{
    SetNull();
}

ExploreTx::ExploreTx(const uint256& blockhashIn,
                     const unsigned int blocktimeIn,
                     const int heightIn,
                     const int vtxIn,
                     const VecDest& vfromIn,
                     const VecDest& vtoIn,
                     const int txflagsIn)
{
    nVersion = ExploreTx::CURRENT_VERSION;
    blockhash = blockhashIn;
    blocktime = blocktimeIn;
    height = heightIn;
    vtx = vtxIn;
    vfrom = vfromIn;
    vto = vtoIn;
    txflags = txflagsIn;
}

bool ExploreTx::IsNull() const
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

bool ExploreTx::operator < (const ExploreTx& other) const
{
    if (height != other.height)
    {
        return (height < other.height);
    }
    return (vtx < other.vtx);
}

bool ExploreTx::operator > (const ExploreTx& other) const
{
    if (height != other.height)
    {
        return (height > other.height);
    }
    return (vtx > other.vtx);
}

void ExploreTx::FlagsAsJSON(Object& objRet) const
{
    Array aryFlags;
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
    if (txflags & EXPLORE_TXFLAGS_PURCHASE4)
    {
        aryFlags.push_back("purchase4");
    }
    if (txflags & EXPLORE_TXFLAGS_SETOWNER)
    {
        aryFlags.push_back("setowner");
    }
    if (txflags & EXPLORE_TXFLAGS_SETMANAGER)
    {
        aryFlags.push_back("setmanager");
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
    if (txflags & EXPLORE_TXFLAGS_FEEWORK)
    {
        aryFlags.push_back("feework");
    }
    if (aryFlags.size() > 0)
    {
        objRet.push_back(Pair("txflags", aryFlags));
    }
}

void ExploreTx::AsJSON(Object& objRet,
                           const int nBestHeight) const
{
    objRet.push_back(Pair("blockhash", blockhash.GetHex()));
    objRet.push_back(Pair("blocktime",
                                       (boost::int64_t)blocktime));
    objRet.push_back(Pair("height", (boost::int64_t)height));
    if (nBestHeight >= 0)
    {
        int confs = 1 + nBestHeight - height;
        objRet.push_back(Pair("confirmations",
                                           (boost::int64_t)confs));
    }
    objRet.push_back(Pair("vtx", (boost::int64_t)vtx));
    Array aryFrom;
    BOOST_FOREACH(const ExploreDestination& dest, vfrom)
    {
        Object objDest;
        dest.AsJSON(objDest);
        aryFrom.push_back(objDest);
    }
    objRet.push_back(Pair("sources", aryFrom));
    Array aryTo;
    BOOST_FOREACH(const ExploreDestination& dest, vto)
    {
        Object objDest;
        dest.AsJSON(objDest);
        aryTo.push_back(objDest);
    }
    objRet.push_back(Pair("destinations", aryTo));
    if (txflags & MASK_TXTYPE)
    {
        string strTxType(GetExploreTxType(txflags));
        objRet.push_back(Pair("txtype", strTxType));
    }
    FlagsAsJSON(objRet);
}
