// Copyright (c) 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb-leveldb.h"
#include "base58.h"
#include "main.h"
#include "explore.hpp"

using namespace std;


//////////////////////////////////////////////////////////////////////////////
//
// global variables
//

bool fWithExploreAPI = false;
bool fDebugExplore = false;
bool fReindexExplore = false;

int64_t nMaxDust = chainParams.DEFAULT_MAXDUST;

// As an STL map, this is ordered decreasing (big balances first).
// These are ordered in-memory keys to sets of addresses,
// and are used to lookup addresses with these balances in the database.
// Values are the number of addresses for each balance. This allows to iterate
// over the balances to find, say, the top 100 addresses.
MapBalanceCounts mapAddressBalances;


//////////////////////////////////////////////////////////////////////////////
//
// functions
//

bool _Err(const char *pmsg,
          const string& strAddr,
          int nID,
          const uint256& txid,
          int n,
          const uint256& hashPrev=0,
          int nPrev=-1)
{
    string result(pmsg);
    result += strprintf("\n    %s (record ID=%d):\n  this: %s-%d",
                        strAddr.c_str(), nID,
                        txid.GetHex().c_str(), n);

    if ((hashPrev > 0) && (nPrev >= 0))
    {
        result += strprintf("\n  prev: %s-%d",
                            hashPrev.GetHex().c_str(), nPrev);
    }

    return error(result);
}


void ExploreGetDestinations(const CTransaction& tx, VecDest& vret)
{
    for (unsigned int n = 0; n < tx.vout.size(); ++n)
    {
        const CTxOut& txout = tx.vout[n];

        vector<string> vAddrs;
        int nReq;
        const char* type;

        txnouttype t;
        vector<CTxDestination> vTxDest;
        if (ExtractDestinations(txout.scriptPubKey, t, vTxDest, nReq))

        {
            BOOST_FOREACH(const CTxDestination& txdest, vTxDest)
            {
                vAddrs.push_back(CBitcoinAddress(txdest).ToString());
            }
            type = GetTxnOutputType(t);
        }
        else
        {
            nReq = 0;
            if ((t >= TX_PURCHASE1) && (t <= TX_SETMETA))
            {
                type = GetTxnOutputType(t);
            }
            else
            {
                type = GetTxnOutputType(TX_NONSTANDARD);
            }
        }

        ExploreDestination d(vAddrs, nReq, txout.nValue, type);
        vret.push_back(d);
    }
}


void UpdateMapAddressBalances(const MapBalanceCounts& mapAddressBalancesAdd,
                              const set<int64_t>& setAddressBalancesRemove,
                              MapBalanceCounts& mapAddressBalancesRet)
{
   /**********************************************************************
    * update the in-memory mapAddressBalances
    **********************************************************************/
    // sanity check: intersection set should be empty
    set<int64_t> setAddressBalancesAdd;
    BOOST_FOREACH(const MapBalanceCounts::value_type& p, mapAddressBalancesAdd)
    {
        setAddressBalancesAdd.insert(p.first);
    }
    set<int64_t> intersection;
    set_intersection(setAddressBalancesAdd.begin(),
                     setAddressBalancesAdd.end(),
                     setAddressBalancesRemove.begin(),
                     setAddressBalancesRemove.end(),
                     inserter(intersection, intersection.begin()));
    assert(intersection.empty());
    // remove
    BOOST_FOREACH(int64_t b, setAddressBalancesRemove)
    {
        mapAddressBalancesRet.erase(b);
    }
    // add
    BOOST_FOREACH(const MapBalanceCounts::value_type& p, mapAddressBalancesAdd)
    {
        mapAddressBalancesRet[p.first] = p.second;
    }
}

bool ExploreConnectInput(CTxDB& txdb,
                         const CTransaction& tx,
                         const unsigned int n,
                         const MapPrevTx& mapInputs,
                         const uint256& txid,
                         MapBalanceCounts& mapAddressBalancesAddRet,
                         set<int64_t>& setAddressBalancesRemoveRet)
{
    const CTxIn& txIn = tx.vin[n];
    const CTxOut txOut = GetOutputFor(txIn, mapInputs);
    const CScript &script = txOut.scriptPubKey;
    txnouttype typetxo;
    vector<valtype> vSolutions;
    if (!Solver(script, typetxo, vSolutions))
    {
        // this should never happen: input has insoluble script
        return error("ExploreConnectInput() : TSNH input %u has insoluble script: %s\n", n,
                     txid.ToString().c_str());
    }
    switch (typetxo)
    {
    // standard destinations
    case TX_PUBKEY:
    case TX_PUBKEYHASH:
    case TX_CLAIM:
    case TX_SCRIPTHASH:
    {
        CTxDestination dest;
        if (!ExtractDestination(script, dest))
        {
            // This should never happen: scriptPubKey is bad?
            return error("ExploreConnectInput() : TSNH scriptPubKey is bad");
        }

        string strAddr = CBitcoinAddress(dest).ToString();

       /***************************************************************
        * 1. mark the previous output as spent (set the spent flag)
        ***************************************************************/
        // lookup the OutputID for the previous output
        int nOutputID = -1;
        if (!txdb.ReadAddrLookup(ADDR_LOOKUP_OUTPUT,
                                 strAddr, txIn.prevout.hash, txIn.prevout.n,
                                 nOutputID))
        {
            return _Err("ExploreConnectInput(): can't read prev output ID",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }

        if (nOutputID < 1)
        {
            // This should never happen : invalid output ID
            return _Err("ExploreConnectInput(): TSNH invalid output ID",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        // for sanity: check that the explore db and chain are synced
        ExploreOutputInfo storedOut;
        if (!txdb.ReadAddrTx(ADDR_TX_OUTPUT, strAddr, nOutputID, storedOut))
        {
            return _Err("ExploreConnectInput(): can't read prev output",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }

        if (storedOut.txid != txIn.prevout.hash)
        {
            // This should never happen, stored tx doesn't match
            return _Err("ExploreConnectInput() : TSNH stored prev tx doesn't match",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (storedOut.vout != txIn.prevout.n)
        {
            // This should never happen, stored n doesn't match output n
            return _Err("ExploreConnectInput(): TSNH stored prev vout doesn't match",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (storedOut.IsSpent())
        {
            // This should never happen, stored output is spent
            return _Err("ExploreConnectInput(): TSNH stored prev output is spent",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }

        // write the previous output back, marked as spent
        storedOut.Spend(txid, n);
        if (!txdb.WriteAddrTx(ADDR_TX_OUTPUT,
                              strAddr, nOutputID, storedOut))
        {
            return _Err("ExploreConnectInput(): can't write prev output",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }

       /***************************************************************
        * 2. add the input
        ***************************************************************/
        int nQtyInputs;
        if (!txdb.ReadAddrQty(ADDR_QTY_INPUT, strAddr, nQtyInputs))
        {
            return _Err("ExploreConnectInput() : can't read qty inputs",
                        strAddr, nQtyInputs, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (nQtyInputs < 0)
        {
            // This should never happen: negative number of inputs
            return _Err("ExploreConnectInput() : TSNH negative inputs",
                        strAddr, nQtyInputs, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        nQtyInputs += 1;
        if (txdb.AddrTxExists(ADDR_TX_INPUT, strAddr, nQtyInputs))
        {
            // This should never happen, input tx already exists
            return _Err("ExploreConnectInput() : TSNH input tx exists",
                        strAddr, nQtyInputs, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        // write the new index record
        if (!txdb.WriteAddrQty(ADDR_QTY_INPUT, strAddr, nQtyInputs))
        {
            return _Err("ExploreConnectInput() : can't write qty of inputs",
                        strAddr, nQtyInputs, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        // write the input record
        ExploreInputInfo newIn(txid, n, txIn.prevout.hash, txIn.prevout.n);
        if (!txdb.WriteAddrTx(ADDR_TX_INPUT, strAddr, nQtyInputs, newIn))
        {
            return _Err("ExploreConnectInput() : can't write input",
                        strAddr, nQtyInputs, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }

       /***************************************************************
        * 3. add the in-out
        ***************************************************************/
        int nQtyInOuts;
        if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return _Err("ExploreConnectInput() : can't read qty in-outs",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (nQtyInOuts < 0)
        {
            // This should never happen: negative number of in-outs
            return _Err("ExploreConnectInput() : TSNH negative in-outs",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        nQtyInOuts += 1;
        if (txdb.AddrTxExists(ADDR_TX_INOUT, strAddr, nQtyInOuts))
        {
            // This should never happen, input in-out tx already exists
            return _Err("ExploreConnectInput() : TSNH input in-out tx exists",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        // write the new index record
        if (!txdb.WriteAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return _Err("ExploreConnectInput() : can't write qty of in-outs",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        // write the in-out record
        ExploreInOutLookup newInOut(nQtyInputs, true);
        if (!txdb.WriteAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts, newInOut))
        {
            return _Err("ExploreConnectInput() : can't write input in-out",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (fDebugExplore && !fReindexExplore)
        {
            printf("EXPLORE connecting INPUT in-out (%d)\n"
                   "   %s - %d\n   %d:%s\n",
                   nQtyInOuts,
                   strAddr.c_str(), nQtyInputs,
                   n, txid.GetHex().c_str());
        }

       /***************************************************************
        * 4. update the balance
        ***************************************************************/
        int64_t nValue = txOut.nValue;
        int64_t nBalanceOld;
        if (!txdb.ReadAddrBalance(ADDR_BALANCE, strAddr, nBalanceOld))
        {
            return _Err("ExploreConnectInput() : can't read addr balance",
                        strAddr, -1, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (nValue > nBalanceOld)
        {
            // This should never happen: output value exceeds balance
            return _Err("ExploreConnectInput() : TSNH prev output value exceeds balance",
                        strAddr, -1, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        int64_t nBalanceNew = nBalanceOld - nValue;
        // no desire to remove evidence of this address here, even if no balance
        if (!txdb.WriteAddrBalance(ADDR_BALANCE, strAddr, nBalanceNew))
        {
            return _Err("ExploreConnectInput() : can't write addr balance",
                        strAddr, -1, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }

       /***************************************************************
        * 5. update the balance sets (for rich list, etc)
        ***************************************************************/
        set<string> setAddr;
        // no tracking of dust balances here
        if (nBalanceOld > nMaxDust)
        {
            if (!txdb.ReadAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
            {
                // This should never happen: no old balance
                return error("ExploreConnectInput() : TSNH no balance set for %s",
                             strAddr.c_str());
            }
            // remove from its old set
            setAddr.erase(strAddr);
            if (setAddr.empty())
            {
                if (!txdb.RemoveAddrSet(ADDR_SET_BAL, nBalanceOld))
                {
                    return error("ExploreConnectInput() : can't remove addr set");
                }
                mapAddressBalancesAddRet.erase(nBalanceOld);
                setAddressBalancesRemoveRet.insert(nBalanceOld);
            }
            else
            {
                if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
                {
                    return error("ExploreConnectInput() : can't write addr set");
                }
                mapAddressBalancesAddRet[nBalanceOld] = setAddr.size();
                setAddressBalancesRemoveRet.erase(nBalanceOld);
            }
            // add to new set if non-dust
            //    (dust balances are not tracked this way)
            if (nBalanceNew > nMaxDust)
            {
                if (!txdb.ReadAddrSet(ADDR_SET_BAL, nBalanceNew, setAddr))
                {
                    return error("ExploreConnectInput() : can't read addr set %s",
                                 FormatMoney(nBalanceNew).c_str());
                }
                if (setAddr.insert(strAddr).second == false)
                {
                    // This should never happen: address unexpectedly in set
                    return error("ExploreConnectInput() : TSNH address unexpectedly in set: %s",
                                 strAddr.c_str());
                }
                if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceNew, setAddr))
                {
                    return error("ExploreConnectInput() : can't write addr set");
                }
                mapAddressBalancesAddRet[nBalanceNew] = setAddr.size();
                setAddressBalancesRemoveRet.erase(nBalanceNew);
            }
        }
    }
        break;
    // nonstandard
    case TX_NONSTANDARD:
    // musltisig
    case TX_MULTISIG:
    // qPoS (standard)
    case TX_PURCHASE1:
    case TX_PURCHASE3:
    case TX_SETOWNER:
    case TX_SETDELEGATE:
    case TX_SETCONTROLLER:
    case TX_ENABLE:
    case TX_DISABLE:
    case TX_SETMETA:
    // null data
    case TX_NULL_DATA:
    default:
        break;
    }  // switch
    return true;
}

bool ExploreConnectOutput(CTxDB& txdb,
                         const CTransaction& tx,
                         const unsigned int n,
                         const uint256& txid,
                         MapBalanceCounts& mapAddressBalancesAddRet,
                         set<int64_t>& setAddressBalancesRemoveRet)
{
    if (tx.IsCoinStake() && (n == 0))
    {
        // skip coin stake marker output
        return true;
    }
    const CTxOut& txOut = tx.vout[n];
    const CScript &script = txOut.scriptPubKey;
    txnouttype typetxo;
    vector<valtype> vSolutions;
    if (!Solver(script, typetxo, vSolutions))
    {
        // output has insoluble script -- skip
        // printf("ExploreConnectOutput() : output %u has insoluble script: %s\n", n,
        //        tx.GetHash().ToString().c_str());
        return true;
    }
    switch (typetxo)
    {
    // standard destinations
    case TX_PUBKEY:
    case TX_PUBKEYHASH:
    case TX_CLAIM:
    case TX_SCRIPTHASH:
    {
        CTxDestination dest;
        if (!ExtractDestination(script, dest))
        {
            // This should never happen: scriptPubKey is bad?
            return error("ExploreConnectOutput() : TSNH scriptPubKey is bad");
        }

        string strAddr = CBitcoinAddress(dest).ToString();

       /***************************************************************
        * 1. add the output
        ***************************************************************/
        int nQtyOutputs;
        if (!txdb.ReadAddrQty(ADDR_QTY_OUTPUT, strAddr, nQtyOutputs))
        {
            return _Err("ExploreConnectOutput() : can't read qty outputs",
                        strAddr, nQtyOutputs, txid, n);
        }
        if (nQtyOutputs < 0)
        {
            // This should never happen: negative number of outputs
            return _Err("ExploreConnectOutput() : TSNH negative outputs",
                        strAddr, nQtyOutputs, txid, n);
        }
        nQtyOutputs += 1;
        if (txdb.AddrTxExists(ADDR_TX_OUTPUT, strAddr, nQtyOutputs))
        {
            // This should never happen, output tx already exists
            return _Err("ExploreConnectOutput() : TSNH output tx exists",
                        strAddr, nQtyOutputs, txid, n);
        }
        // write the new index record
        if (!txdb.WriteAddrQty(ADDR_QTY_OUTPUT, strAddr, nQtyOutputs))
        {
            return _Err("ExploreConnectOutput() : can't write qty of outputs",
                        strAddr, nQtyOutputs, txid, n);
        }
        // write the output record
        ExploreOutputInfo newOut(txid, n, txOut.nValue);
        if (!txdb.WriteAddrTx(ADDR_TX_OUTPUT, strAddr, nQtyOutputs, newOut))
        {
            return _Err("ExploreConnectOutput() : can't write output",
                        strAddr, nQtyOutputs, txid, n);
        }

       /***************************************************************
        * 2. add the output lookup
        ***************************************************************/
        // ensure the output lookup does not exist
        if (txdb.AddrLookupExists(ADDR_LOOKUP_OUTPUT, strAddr, txid, n))
        {
            // This should never happen, output lookup already exists
            return _Err("ExploreConnectOutput() : TSNH lookup exists",
                        strAddr, nQtyOutputs, txid, n);
        }
        // fetch the OutputID
        if (!txdb.WriteAddrLookup(ADDR_LOOKUP_OUTPUT, strAddr, txid, n,
                                                     nQtyOutputs))
        {
            return _Err("ExploreConnectOutput() : can't write lookup",
                        strAddr, nQtyOutputs, txid, n);
        }

       /***************************************************************
        * 3. add the in-out
        ***************************************************************/
        int nQtyInOuts;
        if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return _Err("ExploreConnectOutput() : can't read qty in-outs",
                        strAddr, nQtyInOuts, txid, n);
        }
        if (nQtyInOuts < 0)
        {
            // This should never happen: negative number of in-outs
            return _Err("ExploreConnectOutput() : TSNH negative in-outs",
                        strAddr, nQtyInOuts, txid, n);
        }
        nQtyInOuts += 1;
        if (txdb.AddrTxExists(ADDR_TX_INOUT, strAddr, nQtyInOuts))
        {
            // This should never happen, output in-out tx already exists
            return _Err("ExploreConnectOutput() : TSNH output in-out tx exists",
                        strAddr, nQtyInOuts, txid, n);
        }
        // write the new index record
        if (!txdb.WriteAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return _Err("ExploreConnectOutput() : can't write qty of in-outs",
                        strAddr, nQtyInOuts, txid, n);
        }
        // write the in-out record
        ExploreInOutLookup newInOut(nQtyOutputs, false);
        if (!txdb.WriteAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts, newInOut))
        {
            return _Err("ExploreConnectOutput() : can't write output in-out",
                        strAddr, nQtyInOuts, txid, n);
        }
        if (fDebugExplore && !fReindexExplore)
        {
            printf("EXPLORE connecting OUTPUT in-out (%d)\n"
                   "   %s - %d\n   %d:%s\n",
                   nQtyInOuts,
                   strAddr.c_str(), nQtyOutputs,
                   n, txid.GetHex().c_str());
        }

       /***************************************************************
        * 4. update the balance
        ***************************************************************/
        int64_t nValue = txOut.nValue;
        int64_t nBalanceOld;
        if (!txdb.ReadAddrBalance(ADDR_BALANCE, strAddr, nBalanceOld))
        {
            return _Err("ExploreConnectOutput() : can't read addr balance",
                        strAddr, -1, txid, n);
        }
        int64_t nBalanceNew = nBalanceOld + nValue;
        // no desire to remove evidence of this address here, even if no balance
        if (!txdb.WriteAddrBalance(ADDR_BALANCE, strAddr, nBalanceNew))
        {
            return _Err("ExploreConnectOutput() : can't write addr balance",
                        strAddr, -1, txid, n);
        }

       /***************************************************************
        * 6. update the balance address sets (for rich list, etc)
        ***************************************************************/
        set<string> setAddr;
        // no tracking of dust balances here
        if (nBalanceOld > nMaxDust)
        {
            if (!txdb.ReadAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
            {
                // This should never happen: no old balance
                return error("ExploreConnectOutput() : TSNH no balance set for %s",
                             strAddr.c_str());
            }
            // remove from its old set
            setAddr.erase(strAddr);
            if (setAddr.empty())
            {
                if (!txdb.RemoveAddrSet(ADDR_SET_BAL, nBalanceOld))
                {
                    return error("ExploreConnectOutput() : can't remove addr set");
                }
                mapAddressBalancesAddRet.erase(nBalanceOld);
                setAddressBalancesRemoveRet.insert(nBalanceOld);
            }
            else
            {
                if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
                {
                    return error("ExploreConnectOutput() : can't write addr set");
                }
                mapAddressBalancesAddRet[nBalanceOld] = setAddr.size();
                setAddressBalancesRemoveRet.erase(nBalanceOld);
            }
        }
        // add to new set if non-dust
        //    (dust balances are not tracked this way)
        if (nBalanceNew > nMaxDust)
        {
            if (!txdb.ReadAddrSet(ADDR_SET_BAL, nBalanceNew, setAddr))
            {
                return error("ExploreConnectOutput() : can't read addr set %s",
                             FormatMoney(nBalanceNew).c_str());
            }
            if (setAddr.insert(strAddr).second == false)
            {
                // This should never happen: address unexpectedly in set
                return error("ExploreConnectOutput() : TSNH address unexpectedly in set: %s",
                             strAddr.c_str());
            }
            if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceNew, setAddr))
            {
                return error("ExploreConnectOutput() : can't write addr set");
            }
            mapAddressBalancesAddRet[nBalanceNew] = setAddr.size();
            setAddressBalancesRemoveRet.erase(nBalanceNew);
        }
    }
        break;
    // nonstandard
    case TX_NONSTANDARD:
    // musltisig
    case TX_MULTISIG:
    // qPoS (standard)
    case TX_PURCHASE1:
    case TX_PURCHASE3:
    case TX_SETOWNER:
    case TX_SETDELEGATE:
    case TX_SETCONTROLLER:
    case TX_ENABLE:
    case TX_DISABLE:
    case TX_SETMETA:
    // null data
    case TX_NULL_DATA:
    default:
        break;
    }  // switch
    return true;
}


bool ExploreConnectTx(CTxDB& txdb,
                      const CTransaction& tx,
                      const uint256& hashBlock,
                      const unsigned int nBlockTime,
                      const int nHeight,
                      const int nVtx)
{
    MapBalanceCounts mapAddressBalancesAdd;
    set<int64_t> setAddressBalancesRemove;

    map<uint256, CTxIndex> mapUnused;
    bool fInvalid;
    MapPrevTx mapInputs;
    if (!tx.FetchInputs(txdb, mapUnused, true, false, mapInputs, fInvalid))
    {
        // This should never happen: couldn't fetch inputs
        return error("ExploreConnectTx() : TSNH couldn't fetch inputs");
    }

    uint256 txid = tx.GetHash();

    ExploreTxType txtype = EXPLORE_TXTYPE_NONE;

    if (tx.IsCoinBase())
    {
        txtype = EXPLORE_TXTYPE_COINBASE;
    }
    else
    {
        if (tx.IsCoinStake())
        {
            txtype = EXPLORE_TXTYPE_COINSTAKE;
        }
        for (unsigned int n = 0; n < tx.vin.size(); ++n)
        {
            ExploreConnectInput(txdb, tx, n, mapInputs, txid,
                                mapAddressBalancesAdd,
                                setAddressBalancesRemove);
        }
    }

    for (unsigned int n = 0; n < tx.vout.size(); ++n)
    {
        ExploreConnectOutput(txdb, tx, n, txid,
                             mapAddressBalancesAdd,
                             setAddressBalancesRemove);
    }

    VecDest vDest;
    ExploreGetDestinations(tx, vDest);

    ExploreTxInfo txInfo(hashBlock, nBlockTime, nHeight, nVtx,
                         vDest, tx.vin.size(), (int)txtype);

    txdb.WriteTxInfo(txid, txInfo);

    UpdateMapAddressBalances(mapAddressBalancesAdd,
                             setAddressBalancesRemove,
                             mapAddressBalances);

    return true;
}

bool ExploreConnectBlock(CTxDB& txdb, const CBlock *const block)
{
    const uint256 h = block->GetHash();

    CBlockIndex* pindex;
    map<uint256, CBlockIndex*>::const_iterator mi = mapBlockIndex.find(h);
    if (mi != mapBlockIndex.end() && (*mi).second)
    {
        pindex = (*mi).second;
    }
    else
    {
        return error("ExploreConnectBlock() : TSNH block not in index");
    }

    if (h == hashGenesisBlock)
    {
        if (fDebugExplore)
        {
            printf("ExploreConnectBlock(): writing explore sentinel\n");
        }
        txdb.WriteExploreSentinel();
    }

    int nVtx = 0;
    BOOST_FOREACH(const CTransaction& tx, block->vtx)
    {
        if (!ExploreConnectTx(txdb, tx, h,
                              pindex->nTime, pindex->nHeight, nVtx))
        {
            return false;
        }
        nVtx += 1;
    }
    return true;
}

bool ExploreDisconnectOutput(CTxDB& txdb,
                             const CTransaction& tx,
                             const unsigned int n,
                             const uint256& txid,
                             MapBalanceCounts& mapAddressBalancesAddRet,
                             set<int64_t>& setAddressBalancesRemoveRet)
{
    const CTxOut& txOut = tx.vout[n];
    const CScript &script = txOut.scriptPubKey;
    txnouttype typetxo;
    vector<valtype> vSolutions;
    if (!Solver(script, typetxo, vSolutions))
    {
        // output has insoluble script -- skip
        // printf("DisonnectBlock() : output %u has insoluble script\n   %s\n",
        //        n, txid.ToString().c_str());
        return true;
    }
    switch (typetxo)
    {
    // standard destinations
    case TX_PUBKEY:
    case TX_PUBKEYHASH:
    case TX_CLAIM:
    case TX_SCRIPTHASH:
    {
        CTxDestination dest;
        if (!ExtractDestination(script, dest))
        {
            // This should never happen: scriptPubKey is bad?
            return error("ExploreDisconnectOutput() : TSNH scriptPubKey is bad");
        }

        string strAddr = CBitcoinAddress(dest).ToString();

       /***************************************************************
        * 1. remove the output lookup
        ***************************************************************/
        // fetch the OutputID
        int nOutputID = -1;
        if (!txdb.ReadAddrLookup(ADDR_LOOKUP_OUTPUT, strAddr, txid, n,
                                                     nOutputID))
        {
            return _Err("ExploreDisconnectOutput() : can't read output ID",
                        strAddr, nOutputID, txid, n);
        }
        if (nOutputID < 1)
        {
            // This should never happen : invalid output ID
            return _Err("ExploreDisconnectOutput() : invalid output ID",
                        strAddr, nOutputID, txid, n);
        }
        // finalize removal of the lookup
        if (!txdb.RemoveAddrLookup(ADDR_LOOKUP_OUTPUT, strAddr, txid, n))
        {
            return _Err("ExploreDisconnectOutput() : can't remove lookup",
                        strAddr, nOutputID, txid, n);
        }

       /***************************************************************
        * 2. remove the output
        ***************************************************************/
        int nQtyOutStored;
        if (!txdb.ReadAddrQty(ADDR_QTY_OUTPUT, strAddr, nQtyOutStored))
        {
            return _Err("ExploreDisconnectOutput() : can't read qty outputs",
                        strAddr, nOutputID, txid, n);
        }
        if (nQtyOutStored != nOutputID)
        {
            // This should never happen : output ID mismatch
            return _Err("ExploreDisconnectOutput() : TSNH output ID mismatch",
                        strAddr, nOutputID, txid, n);
        }
        // for sanity: check that the explore db and chain are synced
        ExploreOutputInfo storedOut;
        if (!txdb.ReadAddrTx(ADDR_TX_OUTPUT, strAddr, nOutputID, storedOut))
        {
            return _Err("ExploreDisconnectOutput() : can't read output",
                        strAddr, nOutputID, txid, n);
        }
        if (storedOut.txid != txid)
        {
            // This should never happen, stored tx doesn't match
            return _Err("ExploreDisconnectOutput() : TSNH stored tx doesn't match",
                        strAddr, nOutputID, txid, n);
        }
        if (storedOut.vout != n)
        {
            // This should never happen, stored n doesn't match output n
            return _Err("ExploreDisconnectOutput() : TSNH stored vout doesn't match",
                        strAddr, nOutputID, txid, n);
        }
        if (storedOut.IsSpent())
        {
            // This should never happne, stored output is spent
            return _Err("ExploreDisconnectOutput() : TSNH stored output is spent",
                        strAddr, nOutputID, txid, n);
        }
        // finalize removal
        if (!txdb.RemoveAddrTx(ADDR_TX_OUTPUT, strAddr, nOutputID))
        {
            return _Err("ExploreDisconnectOutput() : can't remove output",
                        strAddr, nOutputID, txid, n);
        }
        int nQtyOutputs = nQtyOutStored - 1;
        // there is no desire to remove evidence of this address here,
        // so we don't test for 0 outputs, etc
        if (!txdb.WriteAddrQty(ADDR_QTY_OUTPUT, strAddr, nQtyOutputs))
        {
            return _Err("ExploreDisconnectOutput() : can't write qty outputs",
                        strAddr, nQtyOutputs, txid, n);
        }

       /***************************************************************
        * 3. remove the in-out
        ***************************************************************/
        // read the qty in-outs
        int nQtyInOuts;
        if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return _Err("ExploreConnectOutput() : can't read qty in-outs",
                        strAddr, nQtyInOuts, txid, n);
        }
        if (nQtyInOuts < 1)
        {
            // This should never happen: negative number of in-outs
            return _Err("ExploreConnectOutput() : TSNH no in-outs",
                        strAddr, nQtyInOuts, txid, n);
        }
        // read the in-out lookup
        ExploreInOutLookup storedInOut;
        if (!txdb.ReadAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts, storedInOut))
        {
            return _Err("ExploreDisconnectOutput() : can't read in-out",
                        strAddr, nQtyInOuts, txid, n);
        }
        if (storedInOut.GetID() != nQtyOutStored)
        {
            // This should never happen, stored tx doesn't match
            return _Err("ExploreDisconnectOutput() : TSNH stored in-out doesn't match",
                        strAddr, nQtyInOuts, txid, n);
        }
        if (storedInOut.IsInput())
        {
            // This should never happen, stored is an input
            return _Err("ExploreDisconnectOutput() : TSNH stored is an input",
                        strAddr, nQtyInOuts, txid, n);
        }
        // finalize removal
        if (!txdb.RemoveAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts))
        {
            return _Err("ExploreDisconnectOutput() : can't remove in-out",
                        strAddr, nQtyInOuts, txid, n);
        }
        nQtyInOuts -= 1;
        // there is no desire to remove evidence of this address here,
        // so we don't test for 0 outputs, etc
        if (!txdb.WriteAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return _Err("ExploreDisconnectOutput() : can't write qty in-outs",
                        strAddr, nQtyInOuts, txid, n);
        }
        if (fDebugExplore && !fReindexExplore)
        {
            printf("EXPLORE disconnecting OUTPUT in-out (%d)\n"
                   "   %s - %d\n   %d:%s\n",
                   nQtyInOuts,
                   strAddr.c_str(), nQtyOutStored,
                   n, txid.GetHex().c_str());
        }

       /***************************************************************
        * 4. update the balance
        ***************************************************************/
        int64_t nValue = txOut.nValue;
        int64_t nBalanceOld;
        if (!txdb.ReadAddrBalance(ADDR_BALANCE, strAddr, nBalanceOld))
        {
            return _Err("ExploreDisconnectOutput() : can't read addr balance",
                        strAddr, -1, txid, n);
        }
        if (nValue > nBalanceOld)
        {
            // This should never happen: output value exceeds balance
            return _Err("ExploreDisconnectOutput() : TSNH output value exceeds balance",
                        strAddr, -1, txid, n);
        }
        int64_t nBalanceNew = nBalanceOld - nValue;
        // no desire to remove evidence of this address here, even if no balance
        if (!txdb.WriteAddrBalance(ADDR_BALANCE, strAddr, nBalanceNew))
        {
            return _Err("ExploreDisconnectOutput() : can't write addr balance",
                        strAddr, -1, txid, n);
        }

       /***************************************************************
        * 5. update the balance sets (for rich list, etc)
        ***************************************************************/
        set<string> setAddr;
        // no tracking of dust balances here
        if (nBalanceOld > nMaxDust)
        {
            if (!txdb.ReadAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
            {
                // This should never happen: no old balance
                return error("ExploreDisconnectOutput() : TSNH no balance set for %s",
                             strAddr.c_str());
            }
            // remove from its old set
            setAddr.erase(strAddr);
            if (setAddr.empty())
            {
                if (!txdb.RemoveAddrSet(ADDR_SET_BAL, nBalanceOld))
                {
                    return error("ExploreDisconnectOutput() : can't remove addr set");
                }
                mapAddressBalancesAddRet.erase(nBalanceOld);
                setAddressBalancesRemoveRet.insert(nBalanceOld);
            }
            else
            {
                if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
                {
                    return error("ExploreDisconnectOutput() : can't write addr set");
                }
                mapAddressBalancesAddRet[nBalanceOld] = setAddr.size();
                setAddressBalancesRemoveRet.erase(nBalanceOld);
            }
            // add to new set if non-dust
            //    (dust balances are not tracked this way)
            if (nBalanceNew > nMaxDust)
            {
                if (!txdb.ReadAddrSet(ADDR_SET_BAL, nBalanceNew, setAddr))
                {
                    return error("ExploreDisconnectOutput() : can't read addr set %s",
                                 FormatMoney(nBalanceNew).c_str());
                }
                if (setAddr.insert(strAddr).second == false)
                {
                    // This should never happen: address unexpectedly in set
                    return error("ExploreDisconnectOutput() : TSNH address unexpectedly in set: %s",
                                 strAddr.c_str());
                }
                if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceNew, setAddr))
                {
                    return error("ExploreDisconnectOutput() : can't write addr set");
                }
                mapAddressBalancesAddRet[nBalanceNew] = setAddr.size();
                setAddressBalancesRemoveRet.erase(nBalanceNew);
            }
        }
    }
        break;
    // nonstandard
    case TX_NONSTANDARD:
    // musltisig
    case TX_MULTISIG:
    // qPoS (standard)
    case TX_PURCHASE1:
    case TX_PURCHASE3:
    case TX_SETOWNER:
    case TX_SETDELEGATE:
    case TX_SETCONTROLLER:
    case TX_ENABLE:
    case TX_DISABLE:
    case TX_SETMETA:
    // null data
    case TX_NULL_DATA:
    default:
        break;
    }  // switch
    return true;
}

bool ExploreDisconnectInput(CTxDB& txdb,
                            const CTransaction& tx,
                            const unsigned int n,
                            const MapPrevTx& mapInputs,
                            const uint256& txid,
                            MapBalanceCounts& mapAddressBalancesAddRet,
                            set<int64_t>& setAddressBalancesRemoveRet)
{
    const CTxIn& txIn = tx.vin[n];
    const CTxOut txOut = GetOutputFor(txIn, mapInputs);
    const CScript &script = txOut.scriptPubKey;
    txnouttype typetxo;
    vector<valtype> vSolutions;
    if (!Solver(script, typetxo, vSolutions))
    {
        // this should never happen: input has insoluble script
        return error("ExploreDisconnectInput() : TSNH input %u has insoluble script: %s\n", n,
                     txid.ToString().c_str());
    }
    switch (typetxo)
    {
    // standard destinations
    case TX_PUBKEY:
    case TX_PUBKEYHASH:
    case TX_CLAIM:
    case TX_SCRIPTHASH:
    {
        CTxDestination dest;
        if (!ExtractDestination(script, dest))
        {
            // This should never happen: scriptPubKey is bad?
            return error("ExploreDisconnectInput() : TSNH scriptPubKey is bad");
        }

        string strAddr = CBitcoinAddress(dest).ToString();
       /***************************************************************
        * 1. remove the input
        ***************************************************************/
        int nQtyInStored;
        if (!txdb.ReadAddrQty(ADDR_QTY_INPUT, strAddr, nQtyInStored))
        {
            return _Err("ExploreDisconnectInput() : can't read qty inputs",
                        strAddr, nQtyInStored, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (nQtyInStored < 1)
        {
            // This should never happen: no input found
            return _Err("ExploreDisconnectInput() : TSNH no input found",
                        strAddr, nQtyInStored, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        // for sanity: check that db and chain are synced
        ExploreInputInfo storedIn;
        if (!txdb.ReadAddrTx(ADDR_TX_INPUT, strAddr, nQtyInStored, storedIn))
        {
            return _Err("ExploreDisconnectInput() : can't read input",
                        strAddr, nQtyInStored, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (storedIn.txid != txid)
        {
            // This should never happen, stored tx doesn't match input tx
            return _Err("ExploreDisconnectInput() : TSNH stored input tx doesn't match",
                        strAddr, nQtyInStored, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (storedIn.vin != n)
        {
            // This should never happen, stored doesn't match input n
            return _Err("ExploreDisconnectInput() : TSNH stored input n doesn't match",
                        strAddr, nQtyInStored, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        // finalize removal
        if (!txdb.RemoveAddrTx(ADDR_TX_INPUT, strAddr, nQtyInStored))
        {
            return _Err("ExploreDisconnectInput() : can't remove input",
                        strAddr, nQtyInStored, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        int nQtyInputs = nQtyInStored - 1;
        // no need to remove evidence of this address here even if balance is 0
        if (!txdb.WriteAddrQty(ADDR_QTY_INPUT, strAddr, nQtyInputs))
        {
            return _Err("ExploreDisconnectInput() : can't write qty of inputs",
                        strAddr, nQtyInputs, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }


       /***************************************************************
        * 2. remove the in-out
        ***************************************************************/
        int nQtyInOuts;
        if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return _Err("ExploreDisconnectInput() : can't read qty in-outs",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (nQtyInOuts < 1)
        {
            // This should never happen : no in-outs
            return _Err("ExploreDisconnectInput() : TSNH no in-outs found",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        // for sanity: check that the explore db and chain are synced
        ExploreInOutLookup storedInOut;
        if (!txdb.ReadAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts, storedInOut))
        {
            return _Err("ExploreDisconnectInput() : can't read in-out",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (storedInOut.GetID() != nQtyInStored)
        {
            // This should never happen, stored tx doesn't match
            return _Err("ExploreDisconnectInput() : TSNH stored in-out doesn't match",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (storedInOut.IsOutput())
        {
            // This should never happen, stored is an output
            return _Err("ExploreDisconnectInput() : TSNH stored is an output",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        // finalize removal
        if (!txdb.RemoveAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts))
        {
            return _Err("ExploreDisconnectInput() : can't remove in-out",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        nQtyInOuts -= 1;
        // there is no desire to remove evidence of this address here,
        // so we don't test for 0 outputs, etc
        if (!txdb.WriteAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return _Err("ExploreDisconnectInput() : can't write qty in-outs",
                        strAddr, nQtyInOuts, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (fDebugExplore && !fReindexExplore)
        {
            printf("EXPLORE disconnecting INPUT in-out (%d)\n"
                   "   %s - %d\n   %d:%s\n",
                   nQtyInOuts,
                   strAddr.c_str(), nQtyInStored,
                   n, txid.GetHex().c_str());
        }

       /***************************************************************
        * 3. mark the prevout as unspent
        ***************************************************************/
        // lookup the OutputID
        int nOutputID = -1;
        if (!txdb.ReadAddrLookup(ADDR_LOOKUP_OUTPUT,
                                 strAddr, txIn.prevout.hash, txIn.prevout.n,
                                 nOutputID))
        {
            return _Err("ExploreDisconnectInput() : can't read output ID",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (nOutputID < 1)
        {
            // This should never happen : invalid output ID
            return _Err("ExploreDisconnectInput() : invalid output ID",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        // for sanity: check that the explore db and chain are synced
        ExploreOutputInfo storedOut;
        if (!txdb.ReadAddrTx(ADDR_TX_OUTPUT, strAddr, nOutputID, storedOut))
        {
            return _Err("ExploreDisconnectInput() : can't read output",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (storedOut.txid != txIn.prevout.hash)
        {
            // This should never happen, stored txid doesn't match
            return _Err("ExploreDisconnectInput() : TSNH stored txid doesn't match",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (storedOut.vout != txIn.prevout.n)
        {
            // This should never happen, stored vout doesn't match output n
            return _Err("ExploreDisconnectInput() : TSNH stored vout doesn't match",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (storedOut.next_txid != txid)
        {
            // This should never happen, stored next_txid doesn't match txid
            return _Err("ExploreDisconnectInput() : TSNH stored next_txid doesn't match",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (storedOut.next_vin != n)
        {
            // This should never happen, stored next_vin doesn't match output n
            return _Err("ExploreDisconnectInput() : TSNH stored next_vin doesn't match",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        if (storedOut.IsUnspent())
        {
            // This should never happen (and is redundant), stored prev output is unspent
            return _Err("ExploreDisconnectInput() : TSNH stored prev output is unspent",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        // write the prev output as unspent
        storedOut.Unspend();
        if (!txdb.WriteAddrTx(ADDR_TX_OUTPUT, strAddr, nOutputID, storedOut))
        {
            return _Err("ExploreDisconnectInput() : can't write output as spent",
                        strAddr, nOutputID, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }

       /***************************************************************
        * 4. update the balance
        ***************************************************************/
        int64_t nValue = txOut.nValue;
        int64_t nBalanceOld;
        if (!txdb.ReadAddrBalance(ADDR_BALANCE, strAddr, nBalanceOld))
        {
            return _Err("ExploreDisconnectTx() : can't read addr balance",
                        strAddr, -1, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }
        int64_t nBalanceNew = nBalanceOld + nValue;
        // no desire to remove evidence of this address here, even if no balance
        if (!txdb.WriteAddrBalance(ADDR_BALANCE, strAddr, nBalanceNew))
        {
            return _Err("ExploreDisconnectInput() : can't write addr balance",
                        strAddr, -1, txid, n,
                        txIn.prevout.hash, txIn.prevout.n);
        }

       /***************************************************************
        * 5. update the balance address sets (for rich list, etc)
        ***************************************************************/
        set<string> setAddr;
        // no tracking of dust balances here
        if (nBalanceOld > nMaxDust)
        {
            if (!txdb.ReadAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
            {
                // This should never happen: no old balance
                return error("ExploreDisconnectInput() : TSNH no balance set for %s",
                             strAddr.c_str());
            }
            // remove from its old set
            setAddr.erase(strAddr);
            if (setAddr.empty())
            {
                if (!txdb.RemoveAddrSet(ADDR_SET_BAL, nBalanceOld))
                {
                    return error("ExploreDisconnectInput() : can't remove addr set");
                }
                mapAddressBalancesAddRet.erase(nBalanceOld);
                setAddressBalancesRemoveRet.insert(nBalanceOld);
            }
            else
            {
                if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
                {
                    return error("ExploreDisconnectInput() : can't write addr set");
                }
                mapAddressBalancesAddRet[nBalanceOld] = setAddr.size();
                setAddressBalancesRemoveRet.erase(nBalanceOld);
            }
        }
        // add to new set if non-dust
        //    (dust balances are not tracked this way)
        if (nBalanceNew > nMaxDust)
        {
            if (!txdb.ReadAddrSet(ADDR_SET_BAL, nBalanceNew, setAddr))
            {
                return error("ExploreDisconnectInput() : can't read addr set %s",
                             FormatMoney(nBalanceNew).c_str());
            }
            if (setAddr.insert(strAddr).second == false)
            {
                // This should never happen: address unexpectedly in set
                return error("ExploreDisconnectInput() : TSNH address unexpectedly in set: %s",
                             strAddr.c_str());
            }
            if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceNew, setAddr))
            {
                return error("ExploreDisconnectInput() : can't write addr set");
            }
            mapAddressBalancesAddRet[nBalanceNew] = setAddr.size();
            setAddressBalancesRemoveRet.erase(nBalanceNew);
        }
    }
        break;
    // nonstandard
    case TX_NONSTANDARD:
    // musltisig
    case TX_MULTISIG:
    // qPoS (standard)
    case TX_PURCHASE1:
    case TX_PURCHASE3:
    case TX_SETOWNER:
    case TX_SETDELEGATE:
    case TX_SETCONTROLLER:
    case TX_ENABLE:
    case TX_DISABLE:
    case TX_SETMETA:
    // null data
    case TX_NULL_DATA:
    default:
        break;
    }  // switch
    return true;
}

bool ExploreDisconnectTx(CTxDB& txdb, const CTransaction &tx)
{
    MapBalanceCounts mapAddressBalancesAdd;
    set<int64_t> setAddressBalancesRemove;

    map<uint256, CTxIndex> mapUnused;
    bool fInvalid;
    MapPrevTx mapInputs;
    if (!tx.FetchInputs(txdb, mapUnused, true, false, mapInputs, fInvalid))
    {
        // This should never happen: couldn't fetch inputs
        return error("ExploreDisconnectTx() : TSNH couldn't fetch inputs");
    }

    uint256 txid = tx.GetHash();

    // outputs (iterate backwards)
    for (int n = tx.vout.size() - 1; n >= 0; --n)
    {

        ExploreDisconnectOutput(txdb, tx, (unsigned int)n, txid,
                                mapAddressBalancesAdd,
                                setAddressBalancesRemove);
    }

    if (!tx.IsCoinBase())
    {
        // inputs (iterate backwards)
        for (int n = tx.vin.size() - 1; n >= 0; --n)
        {
            ExploreDisconnectInput(txdb, tx, (unsigned int)n, mapInputs, txid,
                                   mapAddressBalancesAdd,
                                   setAddressBalancesRemove);
        }
    }

    txdb.RemoveTxInfo(txid);

    UpdateMapAddressBalances(mapAddressBalancesAdd,
                             setAddressBalancesRemove,
                             mapAddressBalances);

    return true;
}
                             

bool ExploreDisconnectBlock(CTxDB& txdb, const CBlock *const block)
{
    // iterate backwards through everything on the disconnect
    BOOST_REVERSE_FOREACH(const CTransaction& tx, block->vtx)
    {
        if (!ExploreDisconnectTx(txdb, tx))
        {
            return false;
        }
    }
    return true;
}

