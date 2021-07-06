// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "HDTxInfo.hpp"

#include <boost/foreach.hpp>


using namespace std;
using namespace json_spirit;


extern Value ValueFromAmount(int64_t amount);

void HDTxInfo::SetNull()
{
    addrinouts.clear();
    payees.clear();
    extx.SetNull();
}

HDTxInfo::HDTxInfo()
{
    SetNull();
}


bool HDTxInfo::operator < (const HDTxInfo& other) const
{
    return (extx < other.extx);
}

bool HDTxInfo::operator > (const HDTxInfo& other) const
{
    return (extx > other.extx);
}

void HDTxInfo::SetPayees()
{
    payees.clear();
    VecDest::const_iterator it;
    for (it = extx.vto.begin(); it != extx.vto.end(); ++it)
    {
        bool fIsOther = true;
        BOOST_FOREACH(const AddrInOutInfo& addrinout, addrinouts)
        {
            if (it->IsSameAs(addrinout.address))
            {
                fIsOther = false;
                break;
            }
        }
        if (fIsOther)
        {
            payees.push_back(*it);
        }
    }
}

const AddrInOutInfo* HDTxInfo::GetLast() const
{
    if (addrinouts.empty())
    {
        return NULL;
    }
    else
    {
        return &(*addrinouts.rbegin());
    }
}

const uint256* HDTxInfo::GetTxID() const
{
    const AddrInOutInfo* paddrinout = GetLast();
    if (paddrinout == NULL)
    {
        return NULL;
    }
    return paddrinout->GetTxID();
}

int64_t HDTxInfo::GetAccountBalanceChange() const
{
    int64_t nChange = 0;
    BOOST_FOREACH(const AddrInOutInfo& addrinout, addrinouts)
    {
        nChange += addrinout.GetBalanceChange();
    }
    return nChange;
}

void HDTxInfo::AsJSON(const int nBestHeight,
                      const unsigned int i,
                      Object& objRet) const
{
    if (i >= addrinouts.size())
    {
        printf("TSNH: No such in-out %u.\n", i);
        return;
    }
    set<AddrInOutInfo>::const_iterator it = addrinouts.begin();
    advance(it, i);
    it->AsJSON(nBestHeight, extx, objRet);
}


void HDTxInfo::AsJSON(const int nBestHeight,
                      Object& objRet) const
{
    objRet.clear();

    Array aryInputs;
    Array aryOutputs;
    BOOST_FOREACH(const AddrInOutInfo& addrinout, addrinouts)
    {
        Object objInOut;
        addrinout.AsJSON(objInOut);
        if (addrinout.IsInput())
        {
            aryInputs.push_back(objInOut);
        }
        else
        {
            aryOutputs.push_back(objInOut);
        }
    }

    Array aryPayees;
    BOOST_FOREACH(const ExploreDestination& dest, payees)
    {
        Object objPayee;
        dest.AsJSON(objPayee);
        aryPayees.push_back(objPayee);
    }

    Object objExTx;
    extx.AsJSON(objExTx, nBestHeight);

    if (!addrinouts.empty())
    {
        objRet.push_back(Pair("txid", GetTxID()->GetHex()));
    }
    objRet.push_back(Pair("account_balance_change",
                          ValueFromAmount(GetAccountBalanceChange())));
    objRet.push_back(Pair("inputs", aryInputs));
    objRet.push_back(Pair("outputs", aryOutputs));
    objRet.push_back(Pair("payees", aryPayees));
    objRet.push_back(Pair("txinfo", objExTx));
}
