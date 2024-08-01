// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "InOutInfo.hpp"
#include "ExploreConstants.hpp"


using namespace json_spirit;
using namespace std;


extern Value ValueFromAmount(int64_t amount);

void InOutInfo::SetNull()
{
    height = -1;
    vtx = 0;
    type = InOutInfo::no_type;
    inout.input.SetNull();
}

InOutInfo::InOutInfo()
{
    SetNull();
}

void InOutInfo::Set(const int fType)
{
    if (fType & FLAG_ADDR_TX)
    {
        type = InOutInfo::input_type;
    }
    else
    {
        type = InOutInfo::output_type;
    }
}

InOutInfo::InOutInfo(const int fType)
{
    height = -1;
    vtx = 0;
    Set(fType);
    inout.input.SetNull();
}

void InOutInfo::Set(const int heightIn,
                    const int vtxIn,
                    const ExploreInput& inputIn)
{
    height = heightIn;
    vtx = vtxIn;
    type = InOutInfo::input_type;
    inout.input = inputIn;
}

void InOutInfo::Set(const int heightIn,
                    const int vtxIn,
                    const ExploreOutput& outputIn)
{
    height = heightIn;
    vtx = vtxIn;
    type = InOutInfo::output_type;
    inout.output = outputIn;
}

InOutInfo::InOutInfo(const int heightIn,
                     const int vtxIn,
                     const ExploreInput& inputIn)
{
    Set(heightIn, vtxIn, inputIn);
}

InOutInfo::InOutInfo(const int heightIn,
                     const int vtxIn,
                     const ExploreOutput& outputIn)
{
    Set(heightIn, vtxIn, outputIn);
}

bool InOutInfo::operator < (const InOutInfo& other) const
{
    if (height != other.height)
    {
        return (height < other.height);
    }
    else if (vtx != other.vtx)
    {
        return (vtx < other.vtx);
    }
    else if (type != other.type)
    {
        return ((int)type < (int)other.type);
    }
    return (GetN() < other.GetN());
}

bool InOutInfo::operator > (const InOutInfo& other) const
{
    if (height != other.height)
    {
        return (height > other.height);
    }
    else if (vtx != other.vtx)
    {
        return (vtx > other.vtx);
    }
    else if (type != other.type)
    {
        return ((int)type > (int)other.type);
    }
    return (GetN() > other.GetN());
}

void InOutInfo::BecomeInput()
{
    type = InOutInfo::input_type;
}

void InOutInfo::BecomeOutput()
{
    type = InOutInfo::output_type;
}

bool InOutInfo::IsInput() const
{
    return (type == InOutInfo::input_type);
}

bool InOutInfo::IsOutput() const
{
    return (type == InOutInfo::output_type);
}

const uint256* InOutInfo::GetTxID() const
{
    if (IsInput())
    {
        return &(inout.input.txid);
    }
    else
    {
        return &(inout.output.txid);
    }
}

int InOutInfo::GetN() const
{
    if (IsInput())
    {
        return inout.input.vin;
    }
    else
    {
        return inout.output.vout;
    }
}

int64_t InOutInfo::GetAmount() const
{
    if (IsInput())
    {
        return inout.input.amount;
    }
    else
    {
        return inout.output.amount;
    }
}

int64_t InOutInfo::GetBalance() const
{
    if (IsInput())
    {
        return inout.input.balance;
    }
    else
    {
        return inout.output.balance;
    }
}

ExploreInput& InOutInfo::GetInput()
{
    return inout.input;
}

ExploreOutput& InOutInfo::GetOutput()
{
    return inout.output;
}


void InOutInfo::AsUniqueJSON(Object& objRet) const
{
    string n_label;
    boost::int64_t n;
    string other_txid;
    boost::int64_t other_n;
    Value amount;
    if (IsInput())
    {
        n_label = "vin";
        n = inout.input.vin;
        other_txid = inout.input.prev_txid.GetHex();
        other_n = inout.input.prev_vout;
        amount = ValueFromAmount(inout.input.amount);
    }
    else
    {
        n_label = "vout";
        n = inout.output.vout;
        other_txid = inout.output.next_txid.GetHex();
        other_n = inout.output.next_vin;
        amount = ValueFromAmount(inout.output.amount);
    }
    objRet.push_back(Pair(n_label, n));
    objRet.push_back(Pair("amount", amount));
    if (IsInput())
    {
        objRet.push_back(Pair("prev_txid", other_txid));
        objRet.push_back(Pair("prev_vout", other_n));
    }
    else if (inout.output.IsSpent())
    {
        objRet.push_back(Pair("isspent", "true"));
        objRet.push_back(Pair("next_txid", other_txid));
        objRet.push_back(Pair("next_vin", other_n));
    }
    else
    {
        objRet.push_back(Pair("isspent", "false"));
    }
}


void InOutInfo::AsJSON(const int nBestHeight,
                       const string* pstrAddress,
                       const ExploreTx* ptx,
                       Object* pobjCommon,
                       Object& objInOut) const
{
    string txid;
    string n_label;
    boost::int64_t n;
    Value balance;
    string other_txid;
    boost::int64_t other_n;
    Value amount;
    if (IsInput())
    {
        txid = inout.input.txid.GetHex();
        n_label = "vin";
        n = inout.input.vin;
        balance = ValueFromAmount(inout.input.balance);
        other_txid = inout.input.prev_txid.GetHex();
        other_n = inout.input.prev_vout;
        amount = ValueFromAmount(inout.input.amount);
    }
    else
    {
        txid = inout.output.txid.GetHex();
        n_label = "vout";
        n = inout.output.vout;
        balance = ValueFromAmount(inout.output.balance);
        other_txid = inout.output.next_txid.GetHex();
        other_n = inout.output.next_vin;
        amount = ValueFromAmount(inout.output.amount);
    }

    if (pobjCommon)
    {
        pobjCommon->push_back(Pair("txid", txid));
    }

    if (pobjCommon && ptx)
    {
        pobjCommon->push_back(Pair("height", (boost::int64_t)(ptx->height)));
    }

    if (ptx)
    {
        objInOut.push_back(Pair("vtx", (boost::int64_t)(ptx->vtx)));
    }

    objInOut.push_back(Pair(n_label, n));

    if (pobjCommon && pstrAddress)
    {
        pobjCommon->push_back(Pair("address", *pstrAddress));
    }

    objInOut.push_back(Pair("amount", amount));

    if (pobjCommon)
    {
        pobjCommon->push_back(Pair("balance", balance));
    }

    if (pobjCommon && ptx)
    {
        pobjCommon->push_back(Pair("blockhash", ptx->blockhash.GetHex()));
        boost::int64_t nConfs = 1 + nBestHeight - ptx->height;
        pobjCommon->push_back(Pair("confirmations", nConfs));
        pobjCommon->push_back(Pair("blocktime",
                                   (boost::int64_t)(ptx->blocktime)));
    }

    if (IsInput())
    {
        objInOut.push_back(Pair("prev_txid", other_txid));
        objInOut.push_back(Pair("prev_vout", other_n));
    }
    else if (inout.output.IsSpent())
    {
        objInOut.push_back(Pair("isspent", "true"));
        objInOut.push_back(Pair("next_txid", other_txid));
        objInOut.push_back(Pair("next_in", other_n));
    }
    else
    {
        objInOut.push_back(Pair("isspent", "false"));
    }
    // TODO: add this at some point?
    // objInOut.push_back(Pair("locktime", (boost::int64_t)tx.nLockTime));
}


void InOutInfo::AsJSON(const int nBestHeight,
                       const string& strAddress,
                       const ExploreTx& tx,
                       Object& objRet) const
{
    AsJSON(nBestHeight, &strAddress, &tx, &objRet, objRet);
}


void InOutInfo::AsJSON(const int nBestHeight, Object& objRet) const
{
    objRet.push_back(Pair("height", (boost::int64_t)height));
    objRet.push_back(Pair("vtx", (boost::int64_t)vtx));
    boost::int64_t confs = 1 + nBestHeight - height;
    objRet.push_back(Pair("confirmations", confs));
    if (IsInput())
    {
        inout.input.AsJSON(objRet);
    }
    else
    {
        inout.output.AsJSON(objRet);
    }
}
