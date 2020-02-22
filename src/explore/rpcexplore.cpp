// Copyright (c) 2020 Stealth R&D LLC
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "bitcoinrpc.h"
#include "txdb-leveldb.h"

#include "explore.hpp"


using namespace json_spirit;
using namespace std;

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

Value getaddressinfo(const Array &params, bool fHelp)
{
    if (fHelp || (params.size()  != 1))
    {
        throw runtime_error(
                "getaddressinfo <address>\n"
                "Returns info about <address>.");
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

    int nQtyUnspent;
    if (!txdb.ReadAddrQty(ADDR_QTY_UNSPENT, strAddress, nQtyUnspent))
    {
         throw runtime_error("TSNH: Can't read number of unspent.");
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

    Object obj;
    obj.push_back(Pair("address", strAddress));
    obj.push_back(Pair("balance", ValueFromAmount(nBalance)));
    obj.push_back(Pair("rank", static_cast<boost::int64_t>(nRank)));
    obj.push_back(Pair("inputs", static_cast<boost::int64_t>(nQtyInputs)));
    obj.push_back(Pair("outputs", static_cast<boost::int64_t>(nQtyOutputs)));
    obj.push_back(Pair("unspent", static_cast<boost::int64_t>(nQtyUnspent)));
    obj.push_back(Pair("height", static_cast<boost::int64_t>(nBestHeight)));

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

    int nStop = min(nStart + nMax - 1, nQtyInputs);

    for (int i = nStart; i <= nStop; ++i)
    {
        Object entry;
        uint256 txid;
        int n;
        if (!txdb.ReadAddrTx(ADDR_TX_INPUT, strAddress, i, txid, n))
        {
            throw runtime_error("TSNH: Problem reading input.");
        }
        CTransaction tx;
        uint256 hashBlock = 0;
        unsigned int nTimeBlock;
        if (GetTransaction(txid, tx, hashBlock, nTimeBlock))
        {
            entry.push_back(Pair("txid", txid.GetHex()));
            entry.push_back(Pair("vin", static_cast<boost::int64_t>(n)));
            if (hashBlock == 0)
            {
                entry.push_back(Pair("confirmations", 0));
            }
            else
            {
                map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
                if (mi != mapBlockIndex.end() && (*mi).second)
                {
                    CBlockIndex* pindex = (*mi).second;
                    if (pindex->IsInMainChain())
                    {
                        entry.push_back(Pair("confirmations", 1 + nBestHeight - pindex->nHeight));
                        entry.push_back(Pair("time", (boost::int64_t)pindex->nTime));
                    }
                    else
                    {
                        entry.push_back(Pair("confirmations", 0));
                    }
                }
                entry.push_back(Pair("blockhash", hashBlock.GetHex()));
            }
            entry.push_back(Pair("locktime", (boost::int64_t)tx.nLockTime));
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                               "TSNH No information available about transaction");
        }
        if (n >= static_cast<int>(tx.vin.size()))
        {
            throw runtime_error("TSNH vin does not exist");
        }
        const CTxIn& txin = tx.vin[n];
        entry.push_back(Pair("prev_txid", txin.prevout.hash.GetHex()));
        entry.push_back(Pair("prev_vout", (boost::int64_t)txin.prevout.n));

        result.push_back(entry);
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

    int nStop = min(nStart + nMax - 1, nQtyOutputs);

    for (int i = nStart; i <= nStop; ++i)
    {
        Object entry;
        uint256 txid;
        int n;
        bool fIsSpent;
        if (!txdb.ReadAddrTx(ADDR_TX_OUTPUT, strAddress, i, txid, n, fIsSpent))
        {
            throw runtime_error("TSNH: Problem reading output.");
        }
        CTransaction tx;
        uint256 hashBlock = 0;
        unsigned int nTimeBlock;
        if (GetTransaction(txid, tx, hashBlock, nTimeBlock))
        {
            entry.push_back(Pair("txid", txid.GetHex()));
            entry.push_back(Pair("vout", static_cast<boost::int64_t>(n)));
            if (n >= static_cast<int>(tx.vout.size()))
            {
                throw runtime_error("TSNH vin does not exist");
            }
            const CTxOut& txout = tx.vout[n];
            entry.push_back(Pair("amount", ValueFromAmount(txout.nValue)));
            if (hashBlock == 0)
            {
                entry.push_back(Pair("confirmations", 0));
            }
            else
            {
                map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
                if (mi != mapBlockIndex.end() && (*mi).second)
                {
                    CBlockIndex* pindex = (*mi).second;
                    if (pindex->IsInMainChain())
                    {
                        entry.push_back(Pair("confirmations", 1 + nBestHeight - pindex->nHeight));
                        entry.push_back(Pair("time", (boost::int64_t)pindex->nTime));
                    }
                    else
                    {
                        entry.push_back(Pair("confirmations", 0));
                    }
                }
                entry.push_back(Pair("blockhash", hashBlock.GetHex()));
            }
            if (fIsSpent)
            {
                entry.push_back(Pair("isspent", "true"));
            }
            else
            {
                entry.push_back(Pair("isspent", "false"));
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                               "TSNH No information available about transaction");
        }

        result.push_back(entry);
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

