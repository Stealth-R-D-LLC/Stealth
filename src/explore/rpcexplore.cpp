// Copyright (c) 2020 Stealth R&D LLC
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "bitcoinrpc.h"
#include "txdb-leveldb.h"
#include "base58.h"

#include "explore.hpp"

#include "hdkeys.h"


using namespace json_spirit;
using namespace std;


// TODO: Use a proper data structure for all this
// To save needless creation and destruction of data structures,
// this comparator is highly specific to the ordering of the json_spirit
// return values in GetInputInfo() and GetOutputInfo() below.
// Take extra care when modifying this comparator or these two funcitons.
// Orders by
//   1. block height
//   2. vtx index
//   3. inputs before outputs
//   4. vin index (inputs), vout (outputs)
struct inout_comparator
{
    bool operator() (const Object& objL, const Object& objR) const
    {
        if (objL[0].value_.get_int() < objR[0].value_.get_int())
        {
            return true;
        }
        if (objL[0].value_.get_int() > objR[0].value_.get_int())
        {
            return false;
        }
        if (objL[1].value_.get_int() < objR[1].value_.get_int())
        {
            return true;
        }
        if (objL[1].value_.get_int() > objR[1].value_.get_int())
        {
            return false;
        }
        if (objL[2].name_ < objR[2].name_)
        {
            return true;
        }
        if (objL[2].name_ > objR[2].name_)
        {
            return false;
        }
        if (objL[2].value_.get_int() < objR[2].value_.get_int())
        {
            return true;
        }
        if (objL[2].value_.get_int() > objR[2].value_.get_int())
        {
            return false;
        }
        return true;
    }
};


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

    if (!txdb.AddrBalanceExists(ADDR_BALANCE, strAddress))
    {
        throw runtime_error("Address does not exist.");
    }

    int64_t nBalance;
    if (!txdb.ReadAddrBalance(ADDR_BALANCE, strAddress, nBalance))
    {
         throw runtime_error("TSNH: Can't read balance.");
    }

    return FormatMoney(nBalance).c_str();
}

void GetAddrInfo(const string& strAddress, Object& objRet)
{
    CTxDB txdb;

    if (!txdb.AddrBalanceExists(ADDR_BALANCE, strAddress))
    {
        throw runtime_error("Address does not exist.");
    }

    int64_t nBalance;
    if (!txdb.ReadAddrBalance(ADDR_BALANCE, strAddress, nBalance))
    {
         throw runtime_error("TSNH: Can't read balance.");
    }

    int nQtyInputs;
    if (!txdb.ReadAddrQty(ADDR_QTY_INPUT, strAddress, nQtyInputs))
    {
         throw runtime_error("TSNH: Can't read number of inputs.");
    }

    int nQtyOutputs;
    if (!txdb.ReadAddrQty(ADDR_QTY_OUTPUT, strAddress, nQtyOutputs))
    {
         throw runtime_error("TSNH: Can't read number of outputs.");
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
    objRet.push_back(Pair("inputs", (boost::int64_t)nQtyInputs));
    objRet.push_back(Pair("outputs", (boost::int64_t)nQtyOutputs));
    objRet.push_back(Pair("unspent", (boost::int64_t)nQtyUnspent));
    objRet.push_back(Pair("height", (boost::int64_t)nBestHeight));
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

void GetInputInfo(CTxDB& txdb,
                  const string& strAddress,
                  const int i,
                  Object& objRet)
{
    ExploreInputInfo input;
    if (!txdb.ReadAddrTx(ADDR_TX_INPUT, strAddress, i, input))
    {
        throw runtime_error("TSNH: Problem reading input.");
    }

    ExploreTxInfo tx;
    if (!txdb.ReadTxInfo(input.txid, tx))
    {
        throw runtime_error("TSNH: Problem reading transaction.");
    }
    // do not change the ordering of these key-value pairs
    // because they are used for sorting with inout_comparator above
    objRet.push_back(Pair("height", (boost::int64_t)tx.height));
    objRet.push_back(Pair("vtx", (boost::int64_t)tx.vtx));
    objRet.push_back(Pair("vin", (boost::int64_t)input.vin));
    objRet.push_back(Pair("txid", input.txid.GetHex()));
    objRet.push_back(Pair("blockhash", tx.blockhash.GetHex()));
    boost::int64_t nConfs = 1 + nBestHeight - tx.height;
    objRet.push_back(Pair("confirmations", nConfs));
    objRet.push_back(Pair("blocktime", (boost::int64_t)tx.blocktime));
    objRet.push_back(Pair("prev_txid", input.prev_txid.GetHex()));
    objRet.push_back(Pair("prev_vout", (boost::int64_t)input.prev_vout));
    // asdf objRet.push_back(Pair("locktime", (boost::int64_t)tx.nLockTime));
}


void GetAddrInputs(CTxDB& txdb,
                   const string& strAddress,
                   const int nStart,
                   const int nMax,
                   const int nQtyInputs,
                   Array& aryRet)
{
    int nStop = min(nStart + nMax - 1, nQtyInputs);
    for (int i = nStart; i <= nStop; ++i)
    {
        Object entry;
        GetInputInfo(txdb, strAddress, i, entry);
        aryRet.push_back(entry);
   }
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

    GetAddrInputs(txdb, strAddress, nStart, nMax, nQtyInputs, result);
    return result;
}


void GetOutputInfo(CTxDB& txdb,
                   const string& strAddress,
                   const int i,
                   Object& objRet)
{
    ExploreOutputInfo output;
    if (!txdb.ReadAddrTx(ADDR_TX_OUTPUT, strAddress, i, output))
    {
        throw runtime_error("TSNH: Problem reading output.");
    }
    ExploreTxInfo tx;
    if (!txdb.ReadTxInfo(output.txid, tx))
    {
        throw runtime_error("TSNH: Problem reading transaction.");
    }
    // do not change the ordering of these key-value pairs
    // because they are used for sorting with inout_comparator above
    objRet.push_back(Pair("height", (boost::int64_t)tx.height));
    objRet.push_back(Pair("vtx", (boost::int64_t)tx.vtx));
    objRet.push_back(Pair("vout", (boost::int64_t)output.vout));
    objRet.push_back(Pair("txid", output.txid.GetHex()));
    objRet.push_back(Pair("amount", ValueFromAmount(output.amount)));
    objRet.push_back(Pair("blockhash", tx.blockhash.GetHex()));
    boost::int64_t nConfs = 1 + nBestHeight - tx.height;
    objRet.push_back(Pair("confirmations", nConfs));
    objRet.push_back(Pair("blocktime", (boost::int64_t)tx.blocktime));
    if (output.IsSpent())
    {
        objRet.push_back(Pair("isspent", "true"));
        objRet.push_back(Pair("next_txid", output.next_txid.GetHex()));
        objRet.push_back(Pair("next_vin", (boost::int64_t)output.next_vin));
    }
    else
    {
        objRet.push_back(Pair("isspent", "false"));
    }
}


void GetAddrOutputs(CTxDB& txdb,
                    const string& strAddress,
                    const int nStart,
                    const int nMax,
                    const int nQtyOutputs,
                    Array& aryRet)
{
    int nStop = min(nStart + nMax - 1, nQtyOutputs);
    for (int i = nStart; i <= nStop; ++i)
    {
        Object entry;
        GetOutputInfo(txdb, strAddress, i, entry);
        aryRet.push_back(entry);
   }
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

    GetAddrOutputs(txdb, strAddress, nStart, nMax, nQtyOutputs, result);
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

    Bip32::HDKeychain hdAccount(vchExtKey);
    Bip32::HDKeychain hdExternal(hdAccount.getChild(0));
    Bip32::HDKeychain hdChange(hdAccount.getChild(1));

    CTxDB txdb;

    set<Object, inout_comparator> setObj;

    // gather external
    for (uint32_t nChild = 0; nChild < chainParams.MAX_HD_CHILDREN; ++nChild)
    {
        Bip32::HDKeychain hdChild(hdExternal.getChild(nChild));
        uchar_vector vchPub = uchar_vector(hdChild.key());

        CPubKey pubKey(vchPub);
        CKeyID keyID = pubKey.GetID();
        CBitcoinAddress address;
        address.Set(keyID);
        string strAddress = address.ToString();

        if (!txdb.AddrBalanceExists(ADDR_BALANCE, strAddress))
        {
            break;
        }

        int nQtyInOuts;
        if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddress, nQtyInOuts))
        {
             throw runtime_error("TSNH: Can't read number of in-outs.");
        }

        for (unsigned int i = 1; i <= (unsigned int)nQtyInOuts; ++i)
        {
            ExploreInOutLookup inout;
            if (!txdb.ReadAddrTx(ADDR_TX_INOUT, strAddress, i, inout))
            {
                throw runtime_error("TSNH: Problem reading inout.");
            }

            Object entry;
            if (inout.IsInput())
            {
                GetInputInfo(txdb, strAddress, inout.nID, entry);
            }
            else
            {
                GetOutputInfo(txdb, strAddress, inout.nID, entry);
            }

            setObj.insert(entry);
        }
    }

    Array result;
    BOOST_FOREACH(const Object& item, setObj)
    {
        result.push_back(item);
    }

    return result;
}


Value getrichlistsize(const Array &params, bool fHelp)
{
    if (fHelp || (params.size()  > 1))
    {
        throw runtime_error(
                strprintf(
                    "getrichlistsize [minumum]\n"
                    "Returns the number of addresses with balances\n"
                    "  greater than [minimum] (default: %s).",
                    FormatMoney(nMaxDust).c_str()));
    }

    int64_t nMinBalance = nMaxDust;
    if (params.size() > 0)
    {
        nMinBalance = AmountFromValue(params[0]);
    }

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

    CTxDB txdb;
    int nStop = nStart + nMax - 1;
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
                obj.push_back(Pair(addr, ValueFromAmount(nBalance)));
                nCount += 1;
            }
            // return all that tied for last spot
            if (nCount >= nStop)
            {
                break;
            }
        }
        else
        {
            nCount += nSize;
        }
    }

    return obj;
}

