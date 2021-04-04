// Copyright (c) 2020 Stealth R&D LLC
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "bitcoinrpc.h"
#include "txdb-leveldb.h"
#include "base58.h"

#include "AddrInOutInfo.hpp"
#include "AddrTxInfo.hpp"
#include "ExploreInOutList.hpp"
#include "ExploreInOutLookup.hpp"
#include "ExploreTx.hpp"
#include "HDTxInfo.hpp"

#include "hdkeys.h"


using namespace json_spirit;
using namespace std;


extern int64_t nMaxDust;
extern MapBalanceCounts mapAddressBalances;

static const unsigned int SEC_PER_DAY = 86400;


//
// Types
//
typedef pair<int, int> txkey_t;


class AddrInOutList : public ExploreInOutList
{
public:
    string address;
    AddrInOutList(const string& addressIn, const ExploreInOutList& inoutlistIn)
    : ExploreInOutList(inoutlistIn)
    {
        address = addressIn;
    }
};


//
// Params
//
unsigned int GetMaxHDChildren()
{
    return GetArg("-maxhdchildren", chainParams.MAX_HD_CHILDREN); 
}

unsigned int GetMaxHDInOuts()
{
    return GetArg("-maxhdinouts", chainParams.MAX_HD_INOUTS); 
}

unsigned int GetMaxHDTxs()
{
    return GetArg("-maxhdtxs", chainParams.MAX_HD_TXS); 
}


//
// Inputs/Outputs
//
int GetInOutID(const int nData)
{
    return nData & MASK_ADDR_TX;
}

void GetInputInfo(CTxDB& txdb,
                   const string& strAddress,
                   const int id,
                   vector<AddrTxInfo>& vRet)
{
    ExploreInput input;
    if (!txdb.ReadAddrTx(ADDR_TX_INPUT, strAddress, id, input))
    {
        throw runtime_error("TSNH: Problem reading input.");
    }
    bool fMakeNew = false;
    if (vRet.empty())
    {
        fMakeNew = true;
    }
    else if (*vRet.back().GetTxID() != input.txid)
    {
        fMakeNew = true;
    }
    if (fMakeNew)
    {
        ExploreTx extx;
        if (!txdb.ReadExploreTx(input.txid, extx))
        {
            throw runtime_error("TSNH: Problem reading transaction.");
        }
        InOutInfo inout(extx.height, extx.vtx, input);
        AddrTxInfo addrtx;
        addrtx.address = strAddress;
        addrtx.extx = extx;
        addrtx.inouts.insert(inout);
        vRet.push_back(addrtx);
    }
    else
    {
        AddrTxInfo& addrtx = vRet.back();
        InOutInfo inout(addrtx.extx.height, addrtx.extx.vtx, input);
        addrtx.inouts.insert(inout);
    }
}

void GetAddrInputs(CTxDB& txdb,
                   const string& strAddress,
                   const int nStart,
                   const int nMax,
                   const int nQtyInputs,
                   vector<AddrTxInfo>& vRet)
{
    int nStop = min(nStart + nMax - 1, nQtyInputs);
    for (int id = nStart; id <= nStop; ++id)
    {
        GetInputInfo(txdb, strAddress, id, vRet);
    }
}

void GetOutputInfo(CTxDB& txdb,
                   const string& strAddress,
                   const int id,
                   vector<AddrTxInfo>& vRet)
{
    ExploreOutput output;
    if (!txdb.ReadAddrTx(ADDR_TX_OUTPUT, strAddress, id, output))
    {
        throw runtime_error("TSNH: Problem reading output.");
    }
    if (vRet.empty() || (*vRet.back().GetTxID() != output.txid))
    {
        ExploreTx extx;
        if (!txdb.ReadExploreTx(output.txid, extx))
        {
            throw runtime_error("TSNH: Problem reading transaction.");
        }
        InOutInfo inout(extx.height, extx.vtx, output);
        AddrTxInfo addrtx;
        addrtx.address = strAddress;
        addrtx.extx = extx;
        addrtx.inouts.insert(inout);
        vRet.push_back(addrtx);
    }
    else
    {
        AddrTxInfo& addrtx = vRet.back();
        InOutInfo inout(addrtx.extx.height, addrtx.extx.vtx, output);
        addrtx.inouts.insert(inout);
    }
}

void GetAddrOutputs(CTxDB& txdb,
                    const string& strAddress,
                    const int nStart,
                    const int nMax,
                    const int nQtyOutputs,
                    vector<AddrTxInfo>& vRet)
{
    int nStop = min(nStart + nMax - 1, nQtyOutputs);
    for (int i = nStart; i <= nStop; ++i)
    {
        GetOutputInfo(txdb, strAddress, i, vRet);
    }
}

void GetInOut(CTxDB& txdb,
              const string& strAddress,
              const int id,
              vector<AddrTxInfo>& vAddrTxRet)
{
    ExploreInOutLookup inout;
    if (!txdb.ReadAddrTx(ADDR_TX_INOUT, strAddress, id, inout))
    {
        throw runtime_error("TSNH: Problem reading inout.");
    }
    if (inout.IsInput())
    {
        GetInputInfo(txdb, strAddress, inout.GetID(), vAddrTxRet);
    }
    else
    {
        GetOutputInfo(txdb, strAddress, inout.GetID(), vAddrTxRet);
    }
}

void GetInOuts(CTxDB& txdb,
               const string& strAddress,
               const int nStart,
               const int nMax,
               const int nQtyInOuts,
               vector<AddrTxInfo>& vAddrTxRet)
{
    int nStop = min(nStart + nMax - 1, nQtyInOuts);
    for (int i = nStart; i <= nStop; ++i)
    {
        GetInOut(txdb, strAddress, i, vAddrTxRet);
    }
}


//
// Addresses
//
void GetAddrInfo(const string& strAddress, Object& objRet)
{
    CTxDB txdb;

    if (!txdb.AddrValueIsViable(ADDR_BALANCE, strAddress))
    {
        throw runtime_error("Address does not exist.");
    }

    int64_t nBalance;
    if (!txdb.ReadAddrValue(ADDR_BALANCE, strAddress, nBalance))
    {
         throw runtime_error("TSNH: Can't read balance.");
    }

    int nQtyVIO;
    if (!txdb.ReadAddrQty(ADDR_QTY_VIO, strAddress, nQtyVIO))
    {
         throw runtime_error("TSNH: Can't read number of transactions.");
    }

    int nQtyOutputs;
    if (!txdb.ReadAddrQty(ADDR_QTY_OUTPUT, strAddress, nQtyOutputs))
    {
         throw runtime_error("TSNH: Can't read number of outputs.");
    }

    int64_t nValueIn;
    if (!txdb.ReadAddrValue(ADDR_VALUEIN, strAddress, nValueIn))
    {
         throw runtime_error("TSNH: Can't read total value in.");
    }

    int nQtyInputs;
    if (!txdb.ReadAddrQty(ADDR_QTY_INPUT, strAddress, nQtyInputs))
    {
         throw runtime_error("TSNH: Can't read number of inputs.");
    }

    int64_t nValueOut;
    if (!txdb.ReadAddrValue(ADDR_VALUEOUT, strAddress, nValueOut))
    {
         throw runtime_error("TSNH: Can't read total value out.");
    }

    int nRank = 0;
    if (nBalance > nMaxDust)
    {
        MapBalanceCounts::const_iterator it;
        for (it = mapAddressBalances.begin(); it != mapAddressBalances.end(); ++it)
        {
            if ((*it).first < nBalance)
            {
                break;
            }
            else if ((*it).first == nBalance)
            {
                // all in a tie have the same rank
                nRank += 1;
                break;
            }
            else
            {
                nRank += (*it).second;
            }
        }
    }

    int nQtyUnspent = nQtyOutputs - nQtyInputs;

    objRet.push_back(Pair("address", strAddress));
    objRet.push_back(Pair("balance", ValueFromAmount(nBalance)));
    objRet.push_back(Pair("rank", (boost::int64_t)nRank));
    objRet.push_back(Pair("transactions", (boost::int64_t)nQtyVIO));
    objRet.push_back(Pair("outputs", (boost::int64_t)nQtyOutputs));
    objRet.push_back(Pair("received", ValueFromAmount(nValueIn)));
    objRet.push_back(Pair("inputs", (boost::int64_t)nQtyInputs));
    objRet.push_back(Pair("sent", ValueFromAmount(nValueOut)));
    objRet.push_back(Pair("unspent", (boost::int64_t)nQtyUnspent));
    objRet.push_back(Pair("in-outs", (boost::int64_t)(nQtyOutputs + nQtyInputs)));
    objRet.push_back(Pair("blocks", (boost::int64_t)nBestHeight));
}

void GetAddrTx(CTxDB& txdb,
               const string& strAddress,
               const int i,
               AddrTxInfo& addrtxRet)
{
    ExploreInOutList vIO;
    if (!txdb.ReadAddrList(ADDR_LIST_VIO, strAddress, i, vIO))
    {
        throw runtime_error("TSNH: Can't read transaction in-outs");
    }
    // sanity check: ensure the vio has transactions
    if (vIO.vinouts.empty())
    {
        throw runtime_error("TSNH: transaction has no in-outs");
    }
    addrtxRet.inouts.clear();
    BOOST_FOREACH(const int& j, vIO.vinouts)
    {
        InOutInfo inoutinfo(j);
        if (inoutinfo.IsInput())
        {
            if (!txdb.ReadAddrTx(ADDR_TX_INPUT, strAddress, GetInOutID(j),
                                 inoutinfo.inout.input))
            {
                throw runtime_error("TSNH: Problem reading input.");
            }
        }
        else
        {
            if (!txdb.ReadAddrTx(ADDR_TX_OUTPUT, strAddress, GetInOutID(j),
                                 inoutinfo.inout.output))
            {
                throw runtime_error("TSNH: Problem reading input.");
            }
        }
        const uint256* ptxid = inoutinfo.GetTxID();
        if (addrtxRet.extx.IsNull())
        {
            if (!txdb.ReadExploreTx(*ptxid, addrtxRet.extx))
            {
                throw runtime_error("TSNH: Problem reading transaction.");
            }
        }
        else if (*(addrtxRet.GetTxID()) != *ptxid)
        {
           throw runtime_error("TSNH: In-outs not from the same tx.");
        }
        addrtxRet.address = strAddress;
        inoutinfo.height = addrtxRet.extx.height;
        inoutinfo.vtx = addrtxRet.extx.vtx;
        addrtxRet.inouts.insert(inoutinfo);
    }
}

void GetAddrTxs(CTxDB& txdb,
                const string& strAddress,
                const int nStart,
                const int nMax,
                const int nQtyTxs,
                vector<AddrTxInfo>& vAddrTxRet)
{
    int nStop = min(nStart + nMax - 1, nQtyTxs);
    for (int i = nStart; i <= nStop; ++i)
    {
        AddrTxInfo addrtx;
        GetAddrTx(txdb, strAddress, i, addrtx);
        vAddrTxRet.push_back(addrtx);
    }
}


//
// Pagination
//
typedef struct pagination_t
{
    int page;
    int per_page;
    bool forward;
    int start;
    int finish;
    int max;
    int last_page;
} pagination_t;

void GetPagination(const Array& params, const unsigned int nLeadingParams,
                   const int nTotal, pagination_t& pgRet)
{
    pgRet.page = params[nLeadingParams].get_int();
    if (pgRet.page < 1)
    {
         throw runtime_error("Number of pages must be greater than 1.");
    }

    pgRet.per_page = params[1 + nLeadingParams].get_int();
    if (pgRet.per_page < 1)
    {
         throw runtime_error("Number per page must be greater than 1.");
    }

    pgRet.forward = true;
    if (params.size() > (2 + nLeadingParams))
    {
        pgRet.forward = params[2 + nLeadingParams].get_bool();
    }

    int nFinish;
    if (pgRet.forward)
    {
        pgRet.start = 1 + ((pgRet.page - 1) * pgRet.per_page);
        if (pgRet.start > nTotal)
        {
             throw runtime_error("Start exceeds total number of in-outs.");
        }
        nFinish = min(pgRet.start + pgRet.per_page - 1, nTotal);
    }
    else
    {
        // calculate as if the sequence is reversed
        int nFirst_r = 1 + ((pgRet.page - 1) * pgRet.per_page);
        if (nFirst_r > nTotal)
        {
             throw runtime_error("Start exceeds total number of in-outs.");
        }
        int nLast_r = pgRet.page * pgRet.per_page;
        // now reverse those calculations
        int nFirst = 1 + (nTotal - nLast_r);
        pgRet.start = max(1, nFirst);
        nFinish = 1 + (nTotal - nFirst_r);
    }
    pgRet.max = min(nFinish - pgRet.start + 1, pgRet.per_page);
    pgRet.last_page = 1 + (nTotal - 1) / pgRet.per_page;
}


//
// Stats
//
class StatHelper
{
public:
    string label;
    int64_t (*Get)(CBlockIndex*);
    Value (*Reduce)(const vector<int64_t>&);

    StatHelper(const string& labelIn,
               int64_t (*GetIn)(CBlockIndex*),
               Value (*ReduceIn)(const vector<int64_t>&))
    {
        label = labelIn;
        Get = GetIn;
        Reduce = ReduceIn;
    }
    string GetLabel() const
    {
        return label;
    }
};


//
// HD Wallets
//
string GetHDChildAddress(const Bip32::HDKeychain& hdParent,
                         const uint32_t nChild)
{
    Bip32::HDKeychain hdChild(hdParent.getChild(nChild));
    uchar_vector vchPub = uchar_vector(hdChild.key());
    CPubKey pubKey(vchPub);
    CKeyID keyID = pubKey.GetID();
    CBitcoinAddress address;
    address.Set(keyID);
    return address.ToString();
}

void GetHDKeychains(const uchar_vector& vchExtKey,
                    vector<Bip32::HDKeychain>& vRet)
{
    vRet.clear();
    Bip32::HDKeychain hdAccount(vchExtKey);
    Bip32::HDKeychain hdExternal(hdAccount.getChild(0));
    vRet.push_back(hdExternal);
    Bip32::HDKeychain hdChange(hdAccount.getChild(1));
    vRet.push_back(hdChange);
}

bool GetAddrInOuts(CTxDB& txdb,
                   const string& strAddress,
                   vector<HDTxInfo>& vHDTxRet)
{
    if (!txdb.AddrValueIsViable(ADDR_BALANCE, strAddress))
    {
        return false;
    }

    int nQtyInOuts;
    if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddress, nQtyInOuts))
    {
         throw runtime_error("TSNH: Can't read number of in-outs.");
    }

    // TODO: build vHDTxRet directly, without vAddrTx
    vector<AddrTxInfo> vAddrTx;
    for (int i = 1; i <= nQtyInOuts; ++i)
    {
        GetInOut(txdb, strAddress, i, vAddrTx);
    }

    BOOST_FOREACH(const AddrTxInfo& addrtx, vAddrTx)
    {
        HDTxInfo hdtx;
        hdtx.extx = addrtx.extx;
        BOOST_FOREACH(const InOutInfo& inout, addrtx.inouts)
        {
           hdtx.addrinouts.insert(AddrInOutInfo(strAddress, inout));
        }
        vHDTxRet.push_back(hdtx);
    }

    return true;
}

void GetHDTxs(const uchar_vector& vchExtKey, vector<HDTxInfo>& vHDTxRet)
{
    const unsigned int nMaxHDChildren = GetMaxHDChildren();
    CTxDB txdb;
    vector<Bip32::HDKeychain> vKeychains;
    GetHDKeychains(vchExtKey, vKeychains);
    vector<HDTxInfo> vHDTxTemp;
    vector<Bip32::HDKeychain>::const_iterator kcit;
    BOOST_FOREACH(const Bip32::HDKeychain& hdParent, vKeychains)
    {
        for (uint32_t nChild = 0; nChild < nMaxHDChildren; ++nChild)
        {
            string strAddress = GetHDChildAddress(hdParent, nChild);
            if (!GetAddrInOuts(txdb, strAddress, vHDTxTemp))
            {
                break;
            }
        }
    }
    // results in hdtxes from same txs being contiguous
    sort(vHDTxTemp.begin(), vHDTxTemp.end());
    BOOST_FOREACH(const HDTxInfo& hdtxtemp, vHDTxTemp)
    {
        if (hdtxtemp.GetTxID() == NULL)
        {
           throw runtime_error("TSNH: Txid of temp is null.");
        }
        if (vHDTxRet.empty())
        {
            vHDTxRet.push_back(hdtxtemp);
            continue;
        }
        HDTxInfo& hdtx = vHDTxRet.back();
        if (*hdtxtemp.GetTxID() == *hdtx.GetTxID())
        {
            BOOST_FOREACH(const AddrInOutInfo& addrinout, hdtxtemp.addrinouts)
            {
                hdtx.addrinouts.insert(addrinout);
            }
        }
        else
        {
            vHDTxRet.push_back(hdtxtemp);
        }
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// Addresses

Value getaddressbalance(const Array &params, bool fHelp)
{
    if (fHelp || (params.size()  != 1))
    {
        throw runtime_error(
                "getaddressbalance <address>\n"
                "Returns the balance of <address>.");
    }

    string strAddress = params[0].get_str();

    CTxDB txdb;

    if (!txdb.AddrValueIsViable(ADDR_BALANCE, strAddress))
    {
        throw runtime_error("Address does not exist.");
    }

    int64_t nBalance;
    if (!txdb.ReadAddrValue(ADDR_BALANCE, strAddress, nBalance))
    {
         throw runtime_error("TSNH: Can't read balance.");
    }

    return FormatMoney(nBalance).c_str();
}

Value getaddressinfo(const Array &params, bool fHelp)
{
    if (fHelp || (params.size() != 1))
    {
        throw runtime_error(
                "getaddressinfo <address>\n"
                "Returns info about <address>.");
    }

    string strAddress = params[0].get_str();

    Object obj;
    GetAddrInfo(strAddress, obj);
    return obj;
}


Value getaddressinputs(const Array &params, bool fHelp)
{
    if (fHelp || (params.size()  < 1) || (params.size() > 3))
    {
        throw runtime_error(
                "getaddressinputs <address> [start] [max]\n"
                "Returns [max] inputs of <address> beginning with [start]\n"
                "  For example, if [start]=101 and [max]=100 means to\n"
                "  return the second 100 inputs (if possible).\n"
                "    [start] is the nth input (default: 1)\n"
                "    [max] is the max inputs to return (default: 100)");
    }

    string strAddress = params[0].get_str();

    CTxDB txdb;

    int nQtyInputs;
    if (!txdb.ReadAddrQty(ADDR_QTY_INPUT, strAddress, nQtyInputs))
    {
         throw runtime_error("TSNH: Can't read number of inputs.");
    }

    Array result;

    if (nQtyInputs == 0)
    {
        return result;
    }

    int nStart = 1;
    if (params.size() > 1)
    {
        nStart = params[1].get_int();
        if (nStart < 1)
        {
            throw runtime_error("Start must be greater than 0.");
        }
        if (nStart > nQtyInputs)
        {
            throw runtime_error(
                    strprintf("Start must be less than %d.", nQtyInputs));
        }
    }

    int nMax = 100;
    if (params.size() > 2)
    {
        nMax = params[2].get_int();
        if (nMax < 1)
        {
            throw runtime_error("Max must be greater than 0.");
        }
    }

    int nBestHeightStart = nBestHeight;
    vector<AddrTxInfo> vAddrTx;
    GetAddrInputs(txdb, strAddress, nStart, nMax, nQtyInputs, vAddrTx);
    BOOST_FOREACH(const AddrTxInfo& addrtx, vAddrTx)
    {
        unsigned int i = 0;
        BOOST_FOREACH(const InOutInfo& inout, addrtx.inouts)
        {
            if (inout.IsOutput())
            {
                throw runtime_error("TSNH: In-out is an output.");
            }
            Object objOutput;
            addrtx.AsJSON(nBestHeightStart, i, objOutput);
            result.push_back(objOutput);
            i += 1;
        }
    }

    return result;
}

Value getaddressoutputs(const Array &params, bool fHelp)
{
    if (fHelp || (params.size()  < 1) || (params.size() > 3))
    {
        throw runtime_error(
                "getaddressoutputs <address> [start] [max]\n"
                "Returns [max] outputs of <address> beginning with [start]\n"
                "  For example, if [start]=101 and [max]=100 means to\n"
                "  return the second 100 outputs (if possible).\n"
                "    [start] is the nth output (default: 1)\n"
                "    [max] is the max outputs to return (default: 100)");
    }

    string strAddress = params[0].get_str();

    CTxDB txdb;

    int nQtyOutputs;
    if (!txdb.ReadAddrQty(ADDR_QTY_OUTPUT, strAddress, nQtyOutputs))
    {
         throw runtime_error("TSNH: Can't read number of outputs.");
    }

    Array result;

    if (nQtyOutputs == 0)
    {
        return result;
    }

    int nStart = 1;
    if (params.size() > 1)
    {
        nStart = params[1].get_int();
        if (nStart < 1)
        {
            throw runtime_error("Start must be greater than 0.");
        }
        if (nStart > nQtyOutputs)
        {
            throw runtime_error(
                    strprintf("Start must be less than %d.", nQtyOutputs));
        }
    }

    int nMax = 100;
    if (params.size() > 2)
    {
        nMax = params[2].get_int();
        if (nMax < 1)
        {
            throw runtime_error("Max must be greater than 0.");
        }
    }

    int nBestHeightStart = nBestHeight;
    vector<AddrTxInfo> vAddrTx;
    GetAddrOutputs(txdb, strAddress, nStart, nMax, nQtyOutputs, vAddrTx);
    BOOST_FOREACH(const AddrTxInfo& addrtx, vAddrTx)
    {
        unsigned int i = 0;
        BOOST_FOREACH(const InOutInfo& inout, addrtx.inouts)
        {
            if (inout.IsInput())
            {
                throw runtime_error("TSNH: In-out is an input.");
            }
            Object objOutput;
            addrtx.AsJSON(nBestHeightStart, i, objOutput);
            result.push_back(objOutput);
            i += 1;
        }
    }

    return result;
}

Value getaddresstxspg(const Array &params, bool fHelp)
{
    if (fHelp || (params.size() < 3) || (params.size() > 4))
    {
        throw runtime_error(
                "getaddresstxspg <address> <page> <perpage> [ordering]\n"
                "Returns up to <perpage> transactions of <address>\n"
                "  beginning with 1 + (<perpage> * (<page> - 1>))\n"
                "  For example, <page>=2 and <perpage>=20 means to\n"
                "  return transactions 21 - 40 (if possible).\n"
                "    <page> is the page number\n"
                "    <perpage> is the number of transactions per page\n"
                "    [ordering] by blockchain position (default=true -> forward)");
    }

    // leading params = 1 (1st param is <address>, 2nd is <page>)
    static const unsigned int LEADING_PARAMS = 1;

    string strAddress = params[0].get_str();

    CTxDB txdb;

    int nQtyTxs;
    if (!txdb.ReadAddrQty(ADDR_QTY_VIO, strAddress, nQtyTxs))
    {
         throw runtime_error("TSNH: Can't read number of transactions.");
    }

    if (nQtyTxs == 0)
    {
         throw runtime_error("Address has no transactions.");
    }

    pagination_t pg;
    GetPagination(params, LEADING_PARAMS, nQtyTxs, pg);

    vector<AddrTxInfo> vAddrTx;
    GetAddrTxs(txdb, strAddress, pg.start, pg.max, nQtyTxs, vAddrTx);

    if (!pg.forward)
    {
        reverse(vAddrTx.begin(), vAddrTx.end());
    }

    int nBestHeightStart = nBestHeight;
    Array data;
    BOOST_FOREACH(const AddrTxInfo& addrtx, vAddrTx)
    {
        Object obj;
        addrtx.AsJSON(nBestHeightStart, obj);
        data.push_back(obj);
    }

    Object result;
    result.push_back(Pair("total", nQtyTxs));
    result.push_back(Pair("page", pg.page));
    result.push_back(Pair("per_page", pg.per_page));
    result.push_back(Pair("last_page", pg.last_page));
    result.push_back(Pair("data", data));

    return result;
}

Value getaddressinouts(const Array &params, bool fHelp)
{
    if (fHelp || (params.size() < 1) || (params.size() > 3))
    {
        throw runtime_error(
                "getaddressinouts <address> [start] [max]\n"
                "Returns [max] inputs + outputs of <address> beginning with [start]\n"
                "  For example, if [start]=101 and [max]=100 means to\n"
                "  return the second 100 in-outs (if possible).\n"
                "    [start] is the nth in-out (default: 1)\n"
                "    [max] is the max in-outs to return (default: 100)");
    }

    string strAddress = params[0].get_str();

    CTxDB txdb;

    int nQtyInOuts;
    if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddress, nQtyInOuts))
    {
         throw runtime_error("TSNH: Can't read number of in-outs.");
    }

    Array result;

    if (nQtyInOuts == 0)
    {
        return result;
    }

    int nStart = 1;
    if (params.size() > 1)
    {
        nStart = params[1].get_int();
        if (nStart < 1)
        {
            throw runtime_error("Start must be greater than 0.");
        }
        if (nStart > nQtyInOuts)
        {
            throw runtime_error(
                    strprintf("Start must be less than %d.", nQtyInOuts));
        }
    }

    int nMax = 100;
    if (params.size() > 2)
    {
        nMax = params[2].get_int();
        if (nMax < 1)
        {
            throw runtime_error("Max must be greater than 0.");
        }
    }

    int nBestHeightStart = nBestHeight;
    vector<AddrTxInfo> vAddrTx;
    GetInOuts(txdb, strAddress, nStart, nMax, nQtyInOuts, vAddrTx);

    BOOST_FOREACH(const AddrTxInfo& addrtx, vAddrTx)
    {
        for (unsigned int i = 0; i < addrtx.inouts.size(); ++i)
        {
            Object objInput;
            addrtx.AsJSON(nBestHeightStart, i, objInput);
            result.push_back(objInput);
        }
    }

    return result;
}

Value getaddressinoutspg(const Array &params, bool fHelp)
{
    if (fHelp || (params.size() < 3) || (params.size() > 4))
    {
        throw runtime_error(
                "getaddressinoutspg <address> <page> <perpage> [ordering]\n"
                "Returns up to <perpage> inputs + outputs of <address>\n"
                "  beginning with 1 + (<perpage> * (<page> - 1>))\n"
                "  For example, <page>=2 and <perpage>=20 means to\n"
                "  return in-outs 21 - 40 (if possible).\n"
                "    <page> is the page number\n"
                "    <perpage> is the number of input/outputs per page\n"
                "    [ordering] by blockchain position (default=true -> forward)");
    }

    // leading params = 1 (1st param is <address>, 2nd is <page>)
    static const unsigned int LEADING_PARAMS = 1;

    string strAddress = params[0].get_str();

    CTxDB txdb;

    int nQtyInOuts;
    if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddress, nQtyInOuts))
    {
         throw runtime_error("TSNH: Can't read number of in-outs.");
    }

    if (nQtyInOuts == 0)
    {
         throw runtime_error("Address has no in-outs.");
    }

    pagination_t pg;
    GetPagination(params, LEADING_PARAMS, nQtyInOuts, pg);

    vector<AddrTxInfo> vAddrTx;
    GetInOuts(txdb, strAddress, pg.start, pg.max, nQtyInOuts, vAddrTx);

    int nBestHeightStart = nBestHeight;
    Array data;
    BOOST_FOREACH(const AddrTxInfo& addrtx, vAddrTx)
    {
        for (unsigned int i = 0; i < addrtx.inouts.size(); ++i)
        {
            Object objOutput;
            addrtx.AsJSON(nBestHeightStart, i, objOutput);
            data.push_back(objOutput);
        }
    }

    if (!pg.forward)
    {
        reverse(data.begin(), data.end());
    }

    Object result;
    result.push_back(Pair("total", nQtyInOuts));
    result.push_back(Pair("page", pg.page));
    result.push_back(Pair("per_page", pg.per_page));
    result.push_back(Pair("last_page", pg.last_page));
    result.push_back(Pair("data", data));

    return result;
}


//////////////////////////////////////////////////////////////////////////////
//
// HD Wallets

Value getchildkey(const Array &params, bool fHelp)
{
    if (fHelp || (params.size() < 2) || (params.size() > 3))
    {
        throw runtime_error(
                strprintf(
                  "getchildkey <extended key> <child> [network byte]\n"
                  "Returns key and address information about the child.\n"
                  "The [network byte] defaults to %d",
                  (int)CBitcoinAddress::PUBKEY_ADDRESS));
    }

    string strExtKey = params[0].get_str();

    uchar_vector vchExtKey;
    if (!DecodeBase58Check(strExtKey, vchExtKey))
    {
        throw runtime_error("Invalid extended key.");
    }

    int nChild = params[1].get_int();

    if ((nChild < 0))
    {
        throw runtime_error("Child number should be positive.");
    }

    int nNetByte = (int)CBitcoinAddress::PUBKEY_ADDRESS;
    if (params.size() > 2)
    {
        nNetByte = params[2].get_int();
        if (nNetByte < 0)
        {
            throw runtime_error("Network byte must be at least 0.");
        }
        if (nNetByte > 255)
        {
            throw runtime_error("Network byte must no greater than 255.");
        }
    }

    Bip32::HDKeychain hdkeychain(vchExtKey);
    hdkeychain = hdkeychain.getChild((uint32_t)nChild);

    string strChildExt(EncodeBase58Check(hdkeychain.extkey()));
    uchar_vector vchChildPub = uchar_vector(hdkeychain.key());
    string strChildPub = vchChildPub.getHex();

    CPubKey pubKey(vchChildPub);
    CKeyID keyID = pubKey.GetID();
    CBitcoinAddress address;
    address.Set(keyID, nNetByte);

    Object obj;
    obj.push_back(Pair("extended", strChildExt));
    obj.push_back(Pair("pubkey", strChildPub));
    obj.push_back(Pair("address", address.ToString()));

    return obj;
}


Value gethdaccountpg(const Array &params, bool fHelp)
{
    if (fHelp || (params.size() < 3) || (params.size() > 4))
    {
        throw runtime_error(
                "gethdaccountpg <extended key> <page> <perpage> [ordering]\n"
                "Returns up to <perpage> transactions of <extended key>\n"
                "  beginning with 1 + (<perpage> * (<page> - 1>))\n"
                "  For example, <page>=2 and <perpage>=20 means to\n"
                "  return transactions 21 - 40 (if possible).\n"
                "    <page> is the page number\n"
                "    <perpage> is the number of transactions per page\n"
                "    [ordering] by blockchain position (default=true -> forward)");
    }

    // leading params = 1 (1st param is <address>, 2nd is <page>)
    static const unsigned int LEADING_PARAMS = 1;

    const unsigned int nMaxHDChildren = GetMaxHDChildren();
    const unsigned int nMaxHDInOuts = GetMaxHDInOuts();
    const unsigned int nMaxHDTxs = GetMaxHDTxs();

    string strExtKey = params[0].get_str();

    uchar_vector vchExtKey;
    if (!DecodeBase58Check(strExtKey, vchExtKey))
    {
        throw runtime_error("Invalid extended key.");
    }

    vector<Bip32::HDKeychain> vKeychains;
    GetHDKeychains(vchExtKey, vKeychains);

    CTxDB txdb;

    unsigned int nTotalInOuts = 0;
    // natural blockchain ordering
    map<txkey_t, vector<AddrInOutList> > mapHDTx;
    BOOST_FOREACH(const Bip32::HDKeychain& hdParent, vKeychains)
    {
        for (uint32_t n = 0; n < nMaxHDChildren; ++n)
        {
            Bip32::HDKeychain hdChild(hdParent.getChild(n));
            uchar_vector vchPub = uchar_vector(hdChild.key());

            CPubKey pubKey(vchPub);
            CKeyID keyID = pubKey.GetID();
            CBitcoinAddress address;
            address.Set(keyID);
            string strAddress = address.ToString();

            if (!txdb.AddrValueIsViable(ADDR_BALANCE, strAddress))
            {
                break;
            }

            int nQtyTxs;
            if (!txdb.ReadAddrQty(ADDR_QTY_VIO, strAddress, nQtyTxs))
            {
                 throw runtime_error("TSNH: Can't read number of vios.");
            }

            if (nQtyTxs == 0)
            {
                throw runtime_error("TSNH: Can't read number of transactions.");
            }

            for (int i = 1; i <= nQtyTxs; ++i)
            {
                ExploreInOutList vIO;
                if (!txdb.ReadAddrList(ADDR_LIST_VIO, strAddress, i, vIO))
                {
                    throw runtime_error("TSNH: Can't read transaction in-outs");
                }

                // sanity check: ensure the vio has transactions
                if (vIO.vinouts.empty())
                {
                    // this should never happen because
                    //nQtyTxs says this tx exists
                    throw runtime_error("TSNH: transaction has no in-outs");
                }

                nTotalInOuts += vIO.vinouts.size();

                if (nTotalInOuts > nMaxHDInOuts)
                {
                    throw runtime_error("Too many HD in-outs.");
                }

                AddrInOutList addrinouts(strAddress, vIO);
                txkey_t txkey = make_pair(vIO.height, vIO.vtx);
                mapHDTx[txkey].push_back(AddrInOutList(strAddress, vIO));
                if (mapHDTx.size() > nMaxHDTxs)
                {
                    throw runtime_error("Too many HD transactions.");
                }
            }
        }
    }

    int nTotalTxs = mapHDTx.size();

    pagination_t pg;
    GetPagination(params, LEADING_PARAMS, nTotalTxs, pg);
    int nStop = min(pg.start + pg.max - 1, nTotalTxs);
    int nElements = 1 + nStop - pg.start;
    // sanity checks, TODO can remove later
    if (pg.start < 1)
    {
        throw runtime_error("TSNH: Start of hd tx pagination less than 1.");
    }
    if (pg.start > nTotalTxs)
    {
        throw runtime_error("TSNH: Start of hd tx pagination is too large.");
    }
    if (pg.start > nStop)
    {
        throw runtime_error("TSNH: Stop of hd tx pagination is too small.");
    }
    if (nStop > nTotalTxs)
    {
        throw runtime_error("TSNH: Stop of hd tx pagination is too large.");
    }
    if ((pg.start + nElements - 1) > nTotalTxs)
    {
        throw runtime_error("TSNH: Number of elements is too large.");
    }

    vector<HDTxInfo> vHDTx;

    map<txkey_t, vector<AddrInOutList> >::const_iterator it = mapHDTx.begin();
    advance(it, pg.start - 1);
    map<txkey_t, vector<AddrInOutList> >::const_iterator itStop = it;
    advance(itStop, nElements);
    // outer loop by tx
    for (; it != itStop; ++it)
    {
        vector<AddrInOutList> vinoutlist = it->second;
        HDTxInfo hdtx;
        hdtx.extx.SetNull();
        // middle loop by address
        //    each address has its own in-out list for the tx
        vector<AddrInOutList>::const_iterator jt = vinoutlist.begin();
        for (jt = vinoutlist.begin(); jt != vinoutlist.end(); ++jt)
        {
            if (jt->vinouts.empty())
            {
               throw runtime_error("TSNH: In-out list is empty.");
            }
            // inner loop by in-out (for single address for single tx)
            BOOST_FOREACH(const int& n, jt->vinouts)
            {
                AddrInOutInfo addrinout(n);
                addrinout.address = jt->address;
                if (addrinout.IsInput())
                {
                    if (!txdb.ReadAddrTx(ADDR_TX_INPUT,
                                         addrinout.address, GetInOutID(n),
                                         addrinout.inoutinfo.inout.input))
                    {
                        throw runtime_error("TSNH: Problem reading input.");
                    }
                }
                else
                {
                    if (!txdb.ReadAddrTx(ADDR_TX_OUTPUT,
                                         addrinout.address, GetInOutID(n),
                                         addrinout.inoutinfo.inout.output))
                    {
                        throw runtime_error("TSNH: Problem reading output.");
                    }
                }
                const uint256* ptxid = addrinout.GetTxID();
                if (!ptxid)
                {
                    throw runtime_error("TSNH: In-out ptxid is null.");
                }
                if (hdtx.extx.IsNull())
                {
                    if (!txdb.ReadExploreTx(*ptxid, hdtx.extx))
                    {
                        throw runtime_error("TSNH: Problem reading transaction.");
                    }
                }
                hdtx.addrinouts.insert(addrinout);
                if (!hdtx.GetTxID())
                {
                    throw runtime_error("TSNH: HD txid is null.");
                }
                if (*hdtx.GetTxID() != *ptxid)
                {
                    throw runtime_error("TSNH: Inouts are from different txs.");
                }
            }  // inner loop (in-out)
        }  // middle loop (address)
        vHDTx.push_back(hdtx);
    }  // outer loop (tx)

    int nBestHeightStart = nBestHeight;
    Array data;
    BOOST_FOREACH(const HDTxInfo& hdtx, vHDTx)
    {
        Object obj;
        hdtx.AsJSON(nBestHeightStart, obj);
        data.push_back(obj);
    }

    if (!pg.forward)
    {
        reverse(data.begin(), data.end());
    }

    Object result;
    result.push_back(Pair("total", (boost::int64_t)nTotalTxs));
    result.push_back(Pair("page", pg.page));
    result.push_back(Pair("per_page", pg.per_page));
    result.push_back(Pair("last_page", pg.last_page));
    result.push_back(Pair("data", data));

    return result;
}


Value gethdaccount(const Array &params, bool fHelp)
{
    if (fHelp || (params.size()  != 1))
    {
        throw runtime_error(
                "gethdaccount <extended key>\n"
                "Returns all transactions for the hdaccount.");
    }

    string strExtKey = params[0].get_str();

    uchar_vector vchExtKey;
    if (!DecodeBase58Check(strExtKey, vchExtKey))
    {
        throw runtime_error("Invalid extended key.");
    }

    vector<HDTxInfo> vHDTx;
    GetHDTxs(vchExtKey, vHDTx);

    int nBestHeightStart = nBestHeight;

    Array result;
    BOOST_FOREACH(const HDTxInfo& hdtx, vHDTx)
    {
        Object item;
        hdtx.AsJSON(nBestHeightStart, item);
        result.push_back(item);
    }

    return result;
}


//////////////////////////////////////////////////////////////////////////////
//
// Richlist

boost::int64_t GetRichListSize(int64_t nMinBalance)
{
    unsigned int nCount = 0;
    MapBalanceCounts::const_iterator it;
    for (it = mapAddressBalances.begin(); it != mapAddressBalances.end(); ++it)
    {
        if ((*it).first < nMinBalance)
        {
            break;
        }
        nCount += (*it).second;
    }
    return static_cast<boost::int64_t>(nCount);
}

Value getrichlistsize(const Array &params, bool fHelp)
{
    if (fHelp || (params.size()  > 1))
    {
        throw runtime_error(
                strprintf(
                    "getrichlistsize [minbalance]\n"
                    "Returns the number of addresses with balances\n"
                    "  greater than [minbalance] (default: %s).",
                    FormatMoney(nMaxDust).c_str()));
    }

    int64_t nMinBalance = nMaxDust;
    if (params.size() > 0)
    {
        nMinBalance = AmountFromValue(params[0]);
    }

    return GetRichListSize(nMinBalance);
}



void GetRichList(int nStart, int nMax, Object& objRet)
{
    CTxDB txdb;
    int nLimit = nStart + nMax - 1;
    int nCount = 0;

    MapBalanceCounts::const_iterator it;
    for (it = mapAddressBalances.begin(); it != mapAddressBalances.end(); ++it)
    {
        int nSize = static_cast<int>((*it).second);
        if ((nSize + nCount) >= nStart)
        {
            int64_t nBalance = (*it).first;
            set<string> setBalances;
            if (!txdb.ReadAddrSet(ADDR_SET_BAL, nBalance, setBalances))
            {
                throw runtime_error(
                        strprintf("TSNH: unable to read balance set %s",
                                  FormatMoney(nBalance).c_str()));
            }
            // sanity check
            if (nSize != static_cast<int>(setBalances.size()))
            {
                throw runtime_error(
                        strprintf("TSNH: balance set %s size mismatch",
                                  FormatMoney(nBalance).c_str()));
            }
            BOOST_FOREACH(const string& addr, setBalances)
            {
                objRet.push_back(Pair(addr, ValueFromAmount(nBalance)));
                nCount += 1;
            }
            // return all that tied for last spot
            if (nCount >= nLimit)
            {
                break;
            }
        }
        else
        {
            nCount += nSize;
        }
    }
}

Value getrichlist(const Array &params, bool fHelp)
{
    if (fHelp || (params.size()  > 2))
    {
        throw runtime_error(
            "getrichlist [start] [max]\n"
            "Returns [max] addresses from rich list beginning with [start]\n"
            "  For example, if [start]=101 and [max]=100 means to\n"
            "  return the second 100 richest (if possible).\n"
            "    [start] is the nth richest address (default: 1)\n"
            "    [max] is the max addresses to return (default: 100)");
    }

    Object obj;
    // nothing to count
    if (mapAddressBalances.empty())
    {
        return obj;
    }

    int nStart = 1;
    if (params.size() > 0)
    {
        nStart = params[0].get_int();
        if (nStart < 1)
        {
            throw runtime_error("Start must be greater than 0.");
        }
    }

    int nMax = 100;
    if (params.size() > 1)
    {
        nMax = params[1].get_int();
        if (nMax < 1)
        {
            throw runtime_error("Max must be greater than 0.");
        }
    }

    GetRichList(nStart, nMax, obj);

    return obj;
}


Value getrichlistpg(const Array &params, bool fHelp)
{
    if (fHelp || (params.size() < 2) || (params.size() > 3))
    {
        throw runtime_error(
                "getrichlistpg <page> <perpage> [ordering]\n"
                "Returns up to <perpage> addresses of the rich list\n"
                "  beginning with 1 + (<perpage> * (<page> - 1>))\n"
                "  For example, <page>=2 and <perpage>=20 means to\n"
                "  return addresses ranking 21 - 40 (if possible).\n"
                "    <page> is the page number\n"
                "    <perpage> is the number of address per page\n"
                "    [ordering] by balance (default=true -> descending)");
    }

    // leading params = 0 (first param is <page>)
    static const unsigned int LEADING_PARAMS = 0;

    if (mapAddressBalances.empty())
    {
         throw runtime_error("No rich list.");
    }

    int64_t nRichListSize = GetRichListSize(nMaxDust);

    pagination_t pg;
    GetPagination(params, LEADING_PARAMS, nRichListSize, pg);

    Object data;
    GetRichList(pg.start, pg.max, data);

    if (!pg.forward)
    {
        reverse(data.begin(), data.end());
    }

    Object result;
    result.push_back(Pair("total", nRichListSize));
    result.push_back(Pair("page", pg.page));
    result.push_back(Pair("per_page", pg.per_page));
    result.push_back(Pair("last_page", pg.last_page));
    result.push_back(Pair("data", data));

    return result;
}




//////////////////////////////////////////////////////////////////////////////
//
// Blockchain Stats

int64_t IntSum(const vector<int64_t>& vNumbers)
{
    int64_t sum = 0;
    BOOST_FOREACH(const int64_t& value, vNumbers)
    {
        sum += value;
    }
    return sum;
}

Value SumAsAmount(const vector<int64_t>& vNumbers)
{
    return static_cast<boost::int64_t>(IntSum(vNumbers));
}

Value SumAsIntValue(const vector<int64_t>& vNumbers)
{
    return ValueFromAmount(IntSum(vNumbers));
}

double RealMean(const vector<int64_t>& vNumbers)
{
    if (vNumbers.empty())
    {
        return numeric_limits<double>::max();
    }
    int64_t sum = IntSum(vNumbers);
    return static_cast<double>(sum) / static_cast<double>(vNumbers.size());
}

Value MeanAsRealValue(const vector<int64_t>& vNumbers)
{
    return RealMean(vNumbers);
}

int64_t IntMean(const vector<int64_t>& vNumbers)
{
    if (vNumbers.empty())
    {
        return numeric_limits<int64_t>::max();
    }
    int64_t sum = IntSum(vNumbers);
    return sum / static_cast<int64_t>(vNumbers.size());
}


Value MeanAsIntValue(const vector<int64_t>& vNumbers)
{
    return IntMean(vNumbers);
}

double RealRMSD(const vector<int64_t>& vNumbers)
{
    if (vNumbers.empty())
    {
        return numeric_limits<double>::max();
    }
    double mean = RealMean(vNumbers);
    double sumsq = 0.0;
    BOOST_FOREACH(const int64_t& value, vNumbers)
    {
        double d = static_cast<double>(value) - mean;
        sumsq += d * d;
    }
    double variance = sumsq / static_cast<double>(vNumbers.size());
    return sqrt(variance);
}

Value RMSDAsRealValue(const vector<int64_t>& vNumbers)
{
    return RealRMSD(vNumbers);
}

Value GetWindowedValue(const Array& params,
                       const StatHelper& helper)
{
    int nPeriod = params[0].get_int();
    if (nPeriod < 1)
    {
        throw runtime_error(
            "Period should be greater than 0.\n");
    }
    if ((unsigned int) nPeriod > 36525 * SEC_PER_DAY)
    {
        throw runtime_error(
            "Period should be less than 100 years.\n");
    }

    int nWindow = params[1].get_int();
    if (nWindow < 1)
    {
        throw runtime_error(
            "Window size should be greater than 0.\n");
    }
    if (nWindow > nPeriod)
    {
        throw runtime_error(
            "Window size should be less than or equal to period.\n");
    }

    int nGranularity = params[2].get_int();
    if (nGranularity < 1)
    {
        throw runtime_error(
            "Window spacing should be greater than 0.\n");
    }
    if (nGranularity > nWindow)
    {
        throw runtime_error(
            "Window spacing should be less than or equal to window.\n");
    }

    if (pindexBest == NULL)
    {
        throw runtime_error("No blocks.\n");
    }

    unsigned int nTime = pindexBest->nTime;

    if (nTime < pindexGenesisBlock->pnext->nTime)
    {
        throw runtime_error("TSNH: Invalid block time.\n");
    }

    unsigned int nPeriodEnd = nTime;
    unsigned int nPeriodStart = 1 + nPeriodEnd - nPeriod;

    vector<unsigned int> vBlockTimes;
    vector<int64_t> vNumbers;
    CBlockIndex *pindex = pindexBest;
    while (pindex->pprev)
    {
        vBlockTimes.push_back(nTime);
        int64_t number = helper.Get(pindex);
        vNumbers.push_back(number);
        pindex = pindex->pprev;
        nTime = pindex->nTime;
        if (nTime < nPeriodStart)
        {
            break;
        }
    }

    std::reverse(vBlockTimes.begin(), vBlockTimes.end());
    std::reverse(vNumbers.begin(), vNumbers.end());

    unsigned int nSizePeriod = vBlockTimes.size();

    Array aryWindowStartTimes;
    Array aryTotalBlocks;
    Array aryTotals;

    unsigned int nWindowStart = nPeriodStart;
    unsigned int nWindowEnd = nWindowStart + nWindow - 1;

    unsigned int idx = 0;
    unsigned int idxNext = 0;
    bool fNextUnknown = true;

    while (nWindowEnd < nPeriodEnd)
    {
        if (fNextUnknown)
        {
            idxNext = idx;
        }
        else
        {
            fNextUnknown = true;
        }
        unsigned int nNextWindowStart = nWindowStart + nGranularity;
        unsigned int nWindowBlocks = 0;
        vector<int64_t> vWindowValues;
        for (idx = idxNext; idx < nSizePeriod; ++idx)
        {
            unsigned int nBlockTime = vBlockTimes[idx];
            // assumes blocks are chronologically ordered
            if (nBlockTime > nWindowEnd)
            {
                aryWindowStartTimes.push_back((boost::int64_t)nWindowStart);
                aryTotals.push_back(helper.Reduce(vWindowValues));
                aryTotalBlocks.push_back((boost::int64_t)nWindowBlocks);
                nWindowStart = nNextWindowStart;
                nWindowEnd += nGranularity;
                break;
            }
            nWindowBlocks += 1;
            vWindowValues.push_back(vNumbers[idx]);
            if (fNextUnknown && (nBlockTime >= nNextWindowStart))
            {
                idxNext = idx;
                fNextUnknown = false;
            }
        }
    }

    Object obj;
    obj.push_back(Pair("window_start", aryWindowStartTimes));
    obj.push_back(Pair("number_blocks", aryTotalBlocks));
    obj.push_back(Pair(helper.GetLabel(), aryTotals));

    return obj;
}


static const string strWindowHelp =
            "  last window ends at time of most recent block\n"
            "  - <period> : duration over which to calculate (sec)\n"
            "  - <windowsize> : duration of each window (sec)\n"
            "  - <windowspacing> : duration between start of consecutive windows (sec)\n"
            "Returns an object with attributes:\n"
            "  - window_start: starting time of each window\n"
            "  - number_blocks: number of blocks in each window\n";


int64_t GetTxVolume(CBlockIndex *pindex)
{
    return pindex->nTxVolume;
}

Value gettxvolume(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
    {
        throw runtime_error(
            "gettxvolume <period> <windowsize> <windowspacing>\n" +
            strWindowHelp +
            "  - tx_volume: number of transactions in each window");
    }

    static const string strValueName = "tx_volume";

    StatHelper helper("tx_volume", &GetTxVolume, &SumAsIntValue);

    return GetWindowedValue(params, helper);
}


int64_t GetXSTVolume(CBlockIndex *pindex)
{
    return pindex->nXSTVolume;
}

Value getxstvolume(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
    {
        throw runtime_error(
            "getxstvolume <period> <windowsize> <windowspacing>\n" +
            strWindowHelp +
            "  - xst_volume: amount of xst transferred in each window");
    }

    StatHelper helper("xst_volume", &GetXSTVolume, &SumAsIntValue);

    return GetWindowedValue(params, helper);
}

int64_t GetBlockInterval(CBlockIndex *pindex)
{
    int64_t interval;
    if (pindex->pnext)
    {
        interval = (int64_t)(pindex->pnext->nTime - pindex->nTime);
    }
    else
    {
        // doubling is an application of the Copernican Principle
        interval = 2 * (GetAdjustedTime() - (int64_t)pindex->nTime);
    }
    return interval;
}

Value getblockinterval(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
    {
        throw runtime_error(
            "getblockinterval <period> <windowsize> <windowspacing>\n" +
            strWindowHelp +
            "  - block_interval: total block interval for the window in seconds");
    }

    StatHelper helper("block_interval", &GetBlockInterval, &SumAsIntValue);

    return GetWindowedValue(params, helper);
}

Value getblockintervalmean(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
    {
        throw runtime_error(
            "getblockintervalmean <period> <windowsize> <windowspacing>\n" +
            strWindowHelp +
            "  - block_interval_mean: rmsd of the block intervals for the window in seconds");
    }

    StatHelper helper("block_interval_mean", &GetBlockInterval, &MeanAsRealValue);

    return GetWindowedValue(params, helper);
}

Value getblockintervalrmsd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
    {
        throw runtime_error(
            "getblockintervalrmsd <period> <windowsize> <windowspacing>\n" +
            strWindowHelp +
            "  - block_interval_rmsd: rmsd of the block intervals for the window in seconds");
    }

    StatHelper helper("block_interval_rmsd", &GetBlockInterval, &RMSDAsRealValue);

    return GetWindowedValue(params, helper);
}

int64_t GetPicoPower(CBlockIndex *pindex)
{
    return static_cast<int64_t>(pindex->nPicoPower);
}

Value getpicopowermean(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "getpicopowermean <period> <windowsize> <windowspacing>\n" +
            strWindowHelp +
            "  - pico_power_mean: mean expressed in units of 1e-12 power");

    StatHelper helper("pico_power_mean", &GetPicoPower, &MeanAsIntValue);

    return GetWindowedValue(params, helper);
}

Value gethourlymissed(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
    {
        throw runtime_error(
            "gethourlymissed <hours>\n"
            "  - hours: number of hours to look back");
    }

    static const int nStartQPoS = GetQPoSStart();

    // these will likely be user parameters in a future generalized RPC
    // first version is implicit 1 hr / window
    static const int64_t nSecondsPerWindow = 3600;

    static const int64_t nBlocksPerWindow = nSecondsPerWindow /
                                                  QP_TARGET_SPACING;

    if (pindexBest == NULL)
    {
        throw runtime_error("No blocks.\n");
    }

    int nWindows = params[0].get_int();

    if (nWindows < 1)
    {
        throw runtime_error("Number of windows must be greater than 0.");
    }

    CBlockIndex* pindex = pindexBest;

    // constrain window boundaries to multiples of QP_TARGET_SPACING
    int64_t nTimeNow = QP_TARGET_SPACING * (pindex->nTime / QP_TARGET_SPACING);
    int64_t nTimeBeginning = nTimeNow - (nWindows * nSecondsPerWindow);
    int64_t nTimeLater = nTimeNow;
    int64_t nTimeEarlier = nTimeLater - nSecondsPerWindow;
    int nHeightLater = pindex->nHeight;
    int nOvershoot = 0;
    bool fComplete = false;

    vector<int> vMissed;
    while (nHeightLater > nStartQPoS)
    {
        if (!pindex->pprev)
        {
            // this should never happen: no earlier block
            throw runtime_error("TSNH: No earlier block.");
        }
        pindex = pindex->pprev;
        if (pindex->nTime <= nTimeEarlier)
        {
            //
            //      + + + o o + + +
            //      <-> <-> <-> <->
            //      1 2 3 4 5 6 7 8
            //        ~
            //
            // Imagine 4 windows, 2 slots per window, we iterate backwards.
            // We start at window 7-8.
            // A complication arises with window 5-6.
            // The window 3-4 ends with a miss (o), so we have to
            // overshoot (backwards), to the previous block (+, slot 3).
            // We calculate the number of slots from 6 to 3, which
            // is 3 (4 - 1 = 3). Windows are 2 slots in width, however,
            // so the overshoot is 3 - 2 = 1.
            // We subtract this overshoot of 1 from the number missed,
            // calculated from the times of blocks at slot 6 and slot 3
            // (amounting to slots 4, 5, and 6).
            // Slot 4 is essentially the overshoot (~).
            // Among slots 4, 5 , and 6, there are 2 misses.
            // However, the overshoot is 1, so subtracting the overshoot
            // from misses yields 2 - 1 = 1 (misses - overshoot = 1).
            // This overshoot carries to the next (earlier) window 3-4,
            // and is added to the number of misses we would calculate,
            // starting at block 3 and looking backwards, stopping
            // at block 2, which is the first hit earlier than window 3-4.
            // In this case, misses = 0 (calculated over just block 3)
            // and undershoot = 1, where undershoot is just the overshoot
            // carried from window 5-6 (block 4 was the overshoot).
            // So, for window 3-4, the total missed is 1:
            //           0 + 1 = 1 (misses + undershoot = 1).

            int nBlocks = nHeightLater - pindex->nHeight;

            // See above: nOvershoot is carried and applied as undershoot
            int nMissed = nOvershoot + nBlocksPerWindow - nBlocks;
            int nTimeOvershoot = (nTimeEarlier - pindex->nTime);
            // Take modulo nBlocksPerWindow because nTimeOvershoot could span
            // more than 1 window. In these cases, there will be full
            // windows that have no blocks.
            nOvershoot = (nTimeOvershoot / QP_TARGET_SPACING) %
                                                        nBlocksPerWindow;
            nMissed -= nOvershoot;
            vMissed.push_back(nMissed);
            nTimeLater = nTimeEarlier;
            nTimeEarlier = nTimeLater - nSecondsPerWindow;
            // Fills in max missed for every full window without a block.
            for (int i = 0; i < (nTimeOvershoot / nBlocksPerWindow); ++i)
            {
                // Exclude windows with slots beyond the desired interval.
                if (nTimeEarlier < nTimeBeginning)
                {
                    fComplete = true;
                    break;
                }
                vMissed.push_back(nBlocksPerWindow);
                nTimeLater = nTimeEarlier;
                nTimeEarlier = nTimeLater - nSecondsPerWindow;
            }
            if (fComplete)
            {
                break;
            }
            else if (pindex->nTime <= nTimeBeginning)
            {
                fComplete = true;
                break;
            }
            nHeightLater = pindex->nHeight;
        }
    }

    if (!fComplete)
    {
        throw runtime_error("Not enough blocks for hours.");
    }

    // sanity check
    if ((int)vMissed.size() != nWindows)
    {
        // this should never happen: number of windows mismatch
        throw runtime_error(
                strprintf("TSNH: window mismatch, %d expected, %lu observed",
                          nWindows, vMissed.size()));
    }

    Array result;
    // reverse results to make windows chronological
    for (int i = vMissed.size() - 1; i >= 0; --i)
    {
        result.push_back(vMissed[i]);
    }

    return result;
}
