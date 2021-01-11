// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "AddrInOutInfo.hpp"

using namespace std;
using namespace json_spirit;


void AddrInOutInfo::SetNull()
{
    address.clear();
    inoutinfo.SetNull();
}

AddrInOutInfo::AddrInOutInfo()
{
    SetNull();
}

void AddrInOutInfo::Set(const int flag)
{
    address.clear();
    inoutinfo.Set(flag);
}

AddrInOutInfo::AddrInOutInfo(const int flag)
{
    Set(flag);
}

void AddrInOutInfo::Set(const std::string& addressIn,
                        const InOutInfo& inoutinfoIn)
{
    address = addressIn;
    inoutinfo = inoutinfoIn;
}

AddrInOutInfo::AddrInOutInfo(const std::string& addressIn,
                             const InOutInfo& inoutinfoIn)
{
    Set(addressIn, inoutinfoIn);
}

bool AddrInOutInfo::operator < (const AddrInOutInfo& other) const
{
    return inoutinfo < other.inoutinfo;
}

bool AddrInOutInfo::operator > (const AddrInOutInfo& other) const
{
    return inoutinfo > other.inoutinfo;
}

void AddrInOutInfo::BecomeInput()
{
    inoutinfo.BecomeInput();
}

void AddrInOutInfo::BecomeOutput()
{
    inoutinfo.BecomeOutput();
}

bool AddrInOutInfo::IsInput() const
{
    return inoutinfo.IsInput();
}

bool AddrInOutInfo::IsOutput() const
{
    return inoutinfo.IsOutput();
}

const uint256* AddrInOutInfo::GetTxID() const
{
    return inoutinfo.GetTxID();
}

int64_t AddrInOutInfo::GetBalanceChange() const
{
    if (inoutinfo.IsInput())
    {
        return -inoutinfo.GetAmount();
    }
    else
    {
        return inoutinfo.GetAmount();
    }
}

ExploreInput& AddrInOutInfo::GetInput()
{
    return inoutinfo.GetInput();
}

ExploreOutput& AddrInOutInfo::GetOutput()
{
    return inoutinfo.GetOutput();
}

void AddrInOutInfo::AsJSON(const int nBestHeight,
                           const ExploreTx& tx,
                           Object& objRet) const
{
    inoutinfo.AsJSON(nBestHeight, address, tx, objRet);
}

void AddrInOutInfo::AsJSON(Object& objRet) const
{
    objRet.push_back(Pair("address", address));
    inoutinfo.AsUniqueJSON(objRet);
}
