// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "AddrTxInfo.hpp"

#include <boost/foreach.hpp>

using namespace std;
using namespace json_spirit;

extern Value ValueFromAmount(int64_t amount);

void AddrTxInfo::SetNull()
{
    address = "";
    inouts.clear();
    extx.SetNull();
}

AddrTxInfo::AddrTxInfo()
{
    SetNull();
}

bool AddrTxInfo::operator < (const AddrTxInfo& other) const
{
    return (extx < other.extx);
}

bool AddrTxInfo::operator > (const AddrTxInfo& other) const
{
    return (extx > other.extx);
}

const InOutInfo* AddrTxInfo::GetLast() const
{
    if (inouts.empty())
    {
        return NULL;
    }
    else
    {
        return &(*inouts.rbegin());
    }
}

const uint256* AddrTxInfo::GetTxID() const
{
    const InOutInfo* pinout = GetLast();
    if (pinout == NULL)
    {
        return NULL;
    }
    return pinout->GetTxID();
}

int64_t AddrTxInfo::GetFinalBalance() const
{
    const InOutInfo* pinout = GetLast();
    if (pinout == NULL)
    {
        return 0;
    }
    return pinout->GetBalance();
}


void AddrTxInfo::AsJSON(const int nBestHeight,
                        const unsigned int i,
                        Object& objRet) const
{
    if (i >= inouts.size())
    {
        printf("TSNH: No such in-out %u.\n", i);
        return;
    }
    set<InOutInfo>::const_iterator it = inouts.begin();
    advance(it, i);
    it->AsJSON(nBestHeight, address, extx, objRet);
}

void AddrTxInfo::AsJSON(const int nBestHeight,
                        Object& objRet) const
{
    objRet.clear();

    Array aryInputs;
    Array aryOutputs;
    BOOST_FOREACH(const InOutInfo& inout, inouts)
    {
        Object objInOut;
        inout.AsUniqueJSON(objInOut);
        if (inout.IsInput())
        {
            aryInputs.push_back(objInOut);
        }
        else
        {
            aryOutputs.push_back(objInOut);
        }
    }

    Object objTxInfo;
    extx.AsJSON(objTxInfo, nBestHeight);

    if (!inouts.empty())
    {
        objRet.push_back(Pair("txid", GetTxID()->GetHex()));
    }
    objRet.push_back(Pair("balance", ValueFromAmount(GetFinalBalance())));
    objRet.push_back(Pair("address_inputs", aryInputs));
    objRet.push_back(Pair("address_outputs", aryOutputs));
    objRet.push_back(Pair("txinfo", objTxInfo));
}
