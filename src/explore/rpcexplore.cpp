// Copyright (c) 2020 Stealth R&D LLC
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "wallet.h"
#include "bitcoinrpc.h"
#include "txdb-leveldb.h"

#include "txdb-leveldb.h"
#include "base58.h"
#include "main.h"
#include "explore.hpp"

extern QPRegistry *pregistryMain;
extern CWallet* pwalletMain;

using namespace json_spirit;
using namespace std;

Value getrichlistsize(const Array &params, bool fHelp)
{
    if (fHelp || (params.size()  > 1))
    {
        throw runtime_error(
                strprintf(
                    "getrichlistsize [minumum]\n"
                    "Returns the number of addresses with balances "
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
            "getrichlist [start] [total]\n"
            "Returns the [total] addresses from rich list beginning with [start]\n"
            "  For example, if [start]=101 and [total]=100 means to\n"
            "  return the second 100 richest.\n"
            "    [start] is the nth richest address (default: 1)\n"
            "    [total] is the total addresses to return (default: 100)");
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

    int nTotal = 100;
    if (params.size() > 1)
    {
        nTotal = params[1].get_int();
        if (nTotal < 1)
        {
            throw runtime_error("Total must be greater than 0.");
        }
    }

    CTxDB txdb;
    int nStop = nStart + nTotal - 1;
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
                obj.push_back(Pair(addr, nBalance));
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

