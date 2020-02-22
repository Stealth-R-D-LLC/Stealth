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
// leveldb record strings for the explore API
//

// Addr Qty
const string ADDR_QTY_INPUT = "AddrQtyInput";
const string ADDR_QTY_OUTPUT = "AddrQtyOutput";
const string ADDR_QTY_INOUT = "AddrQtyInOut";
const string ADDR_QTY_UNSPENT = "AddrQtyUnspent";
// Addr Tx
const string ADDR_TX_INPUT = "AddrTxInput";
const string ADDR_TX_OUTPUT = "AddrTxOutput";
const string ADDR_TX_INOUT = "AddrTxInOut";
// Addr Lookup 
const string ADDR_LOOKUP_OUTPUT = "AddrLookupOutput";
// Addr Balance
const string ADDR_BALANCE = "AddrBalance";
// Addr Set
const string ADDR_SET_BAL = "AddrSetBal";


//////////////////////////////////////////////////////////////////////////////
//
// global variables
//

bool fWithExploreAPI = false;
bool fDebugExplore = false;
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

void UpdateMapAddressBalances(const MapBalanceCounts& mapAddressBalancesAdd,
                              const set<int64_t>& setAddressBalancesRemove,
                              MapBalanceCounts& mapAddressBalancesRet)
{
   /**********************************************************************
    * update the in-memory mapAddressBalances
    **********************************************************************/
    // sanity check: intersection set should be empty
    set<int64_t> setAddressBalancesAdd;
    BOOST_FOREACH(const MapBalanceCounts::value_type &p, mapAddressBalancesAdd)
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
    BOOST_FOREACH(const MapBalanceCounts::value_type &p, mapAddressBalancesAdd)
    {
        mapAddressBalancesRet[p.first] = p.second;
    }
}

bool ExploreConnectInput(CTxDB& txdb,
                         const CTransaction& tx,
                         const unsigned int& n,
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
            printf("%s:\n  %s-%d (%d)\n", strAddr.c_str(),
                                     txIn.prevout.hash.ToString().c_str(),
                                     txIn.prevout.n,
                                     nOutputID);

            return error("ExploreConnectInput() : could not read prev output ID");
        }
        if (nOutputID < 1)
        {
            // This should never happen : invalid output ID
            return error("ExploreConnectInput() : invalid output ID");
        }
        // for sanity: check that the explore db and chain are synced
        uint256 txidStored = 0;
        int nStored = -1;
        bool fStoredIsSpent;
        if (!txdb.ReadAddrTx(ADDR_TX_OUTPUT, strAddr, nOutputID,
                             txidStored, nStored, fStoredIsSpent))
        {
            return error("ExploreConnectInput() : could not read prev output");
        }
        if (txidStored != txIn.prevout.hash)
        {
            // This should never happen, stored tx doesn't match
            return error("ExploreConnectInput() : TSNH stored prev tx doesn't match");
        }
        if (static_cast<unsigned int>(nStored) != txIn.prevout.n)
        {
            // This should never happen, stored n doesn't match output n
            return error("ExploreConnectInput() : TSNH stored prev vout doesn't match");
        }
        if (fStoredIsSpent)
        {
            // This should never happen, stored output is spent
            return error("ExploreConnectInput() : TSNH stored prev output is spent");
        }
        fStoredIsSpent = true;
        // write the previous output back, marked as spent
        if (!txdb.WriteAddrTx(ADDR_TX_OUTPUT, strAddr, nOutputID,
                              txIn.prevout.hash, txIn.prevout.n, fStoredIsSpent))
        {
            return error("ExploreConnectInput() : could not write prev output");
        }
        // decrement the unspent count
        int nQtyUnspent;
        if (!txdb.ReadAddrQty(ADDR_QTY_UNSPENT, strAddr, nQtyUnspent))
        {
            return error("ExploreConnectInput() : could not read qty unspent");
        }
        if (nQtyUnspent < 1)
        {
            return error("ExploreConnectInput() : TSNH no qty unspent found");
        }
        nQtyUnspent -= 1;
        if (!txdb.WriteAddrQty(ADDR_QTY_UNSPENT, strAddr, nQtyUnspent))
        {
            return error("ExploreConnectInput() : could not write qty unspent");
        }


       /***************************************************************
        * 2. add the input
        ***************************************************************/
        int nQtyInputs;
        if (!txdb.ReadAddrQty(ADDR_QTY_INPUT, strAddr, nQtyInputs))
        {
            return error("ExploreConnectInput() : could not read qty inputs");
        }
        if (nQtyInputs < 0)
        {
            // This should never happen: negative number of inputs
            return error("ExploreConnectInput() : TSNH negative number of inputs");
        }
        nQtyInputs += 1;
        if (txdb.AddrTxExists(ADDR_TX_INPUT, strAddr, nQtyInputs))
        {
            // This should never happen, input tx already exists
            return error("ExploreConnectInput() : TSNH input tx already exists");
        }
        // write the new index record
        if (!txdb.WriteAddrQty(ADDR_QTY_INPUT, strAddr, nQtyInputs))
        {
            return error("ExploreConnectInput() : could not write qty of inputs");
        }
        // write the input record
        if (!txdb.WriteAddrTx(ADDR_TX_INPUT, strAddr, nQtyInputs, txid, n))
        {
            return error("ExploreConnectInput() : could not write input");
        }

       /***************************************************************
        * 3. add the in-out
        ***************************************************************/
        int nQtyInOuts;
        if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return error("ExploreConnectInput() : could not read qty in-outs");
        }
        if (nQtyInOuts < 0)
        {
            // This should never happen: negative number of in-outs
            return error("ExploreConnectInput() : TSNH negative number of in-outs");
        }
        nQtyInOuts += 1;
        if (txdb.AddrTxExists(ADDR_TX_INOUT, strAddr, nQtyInOuts))
        {
            // This should never happen, input in-out tx already exists
            return error("ExploreConnectInput() : TSNH input in-out tx already exists");
        }
        // write the new index record
        if (!txdb.WriteAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return error("ExploreConnectInput() : could not write qty of in-outs");
        }
        // write the in-out record
        if (!txdb.WriteAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts, txid, n, true))
        {
            return error("ExploreConnectInput() : could not write input in-out");
        }

       /***************************************************************
        * 4. update the balance
        ***************************************************************/
        int64_t nValue = txOut.nValue;
        // someone could mindlessly set MIN_TXOUT_AMOUNT to 0 in the future
        if ((nValue < chainParams.MIN_TXOUT_AMOUNT) || (nValue <= 0))
        {
            // This should never happen, output value is less than minimum
            return error("ExploreConnectInput() : TSNH prev output value less than minimum");
        }
        int64_t nBalanceOld;
        if (!txdb.ReadAddrBalance(ADDR_BALANCE, strAddr, nBalanceOld))
        {
            return error("ExploreConnectInput() : could not read addr balance");
        }
        if (nValue > nBalanceOld)
        {
            // This should never happen: output value exceeds balance
            return error("ExploreConnectInput() : TSNH prev output value exceeds balance");
        }
        int64_t nBalanceNew = nBalanceOld - nValue;
        // no desire to remove evidence of this address here, even if no balance
        if (!txdb.WriteAddrBalance(ADDR_BALANCE, strAddr, nBalanceNew))
        {
            return error("ExploreConnectInput() : could not write addr balance");
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
                    return error("ExploreConnectInput() : could not remove addr set");
                }
                mapAddressBalancesAddRet.erase(nBalanceOld);
                setAddressBalancesRemoveRet.insert(nBalanceOld);
            }
            else
            {
                if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
                {
                    return error("ExploreConnectInput() : could not write addr set");
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
                    return error("ExploreConnectInput() : could not read addr set %s",
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
                    return error("ExploreConnectInput() : could not write addr set");
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
                         const unsigned int& n,
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
        printf("ExploreConnectOutput() : output %u has insoluble script: %s\n", n,
               tx.GetHash().ToString().c_str());
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
            return error("ExploreConnectOutput() : could not read qty outputs");
        }
        if (nQtyOutputs < 0)
        {
            // This should never happen: negative number of outputs
            return error("ExploreConnectOutput() : TSNH negative number of outputs");
        }
        nQtyOutputs += 1;
        if (txdb.AddrTxExists(ADDR_TX_OUTPUT, strAddr, nQtyOutputs))
        {
            // This should never happen, output tx already exists
            return error("ExploreConnectOutput() : TSNH output tx already exists");
        }
        // write the new index record
        if (!txdb.WriteAddrQty(ADDR_QTY_OUTPUT, strAddr, nQtyOutputs))
        {
            return error("ExploreConnectOutput() : could not write qty of outputs");
        }
        // write the output record
        if (!txdb.WriteAddrTx(ADDR_TX_OUTPUT, strAddr, nQtyOutputs, txid, n))
        {
            return error("ExploreConnectOutput() : could not write output");
        }

       /***************************************************************
        * 2. increment the quantity unspent
        ***************************************************************/
        // unspent are not stored separately from other outputs because the
        //   user wallet is expected to store all inputs and outputs anyway
        int nQtyUnspent;
        if (!txdb.ReadAddrQty(ADDR_QTY_UNSPENT, strAddr, nQtyUnspent))
        {
            return error("ExploreConnectOutput() : could not read qty unspent");
        }
        nQtyUnspent += 1;
        if (!txdb.WriteAddrQty(ADDR_QTY_UNSPENT, strAddr, nQtyUnspent))
        {
            return error("ExploreConnectOutput() : could not write qty unspent");
        }

       /***************************************************************
        * 3. add the output lookup
        ***************************************************************/
        // ensure the output lookup does not exist
        if (txdb.AddrLookupExists(ADDR_LOOKUP_OUTPUT, strAddr, txid, n))
        {
            // This should never happen, output lookup already exists
            return error("ExploreConnectOutput() : TSNH output lookup already exists");
        }
        // fetch the OutputID
        if (!txdb.WriteAddrLookup(ADDR_LOOKUP_OUTPUT, strAddr, txid, n,
                                                     nQtyOutputs))
        {
            return error("ExploreConnectOutput() : could not write output lookup");
        }

       /***************************************************************
        * 4. add the in-out
        ***************************************************************/
        int nQtyInOuts;
        if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return error("ExploreConnectOutput() : could not read qty in-outs");
        }
        if (nQtyInOuts < 0)
        {
            // This should never happen: negative number of in-outs
            return error("ExploreConnectOutput() : TSNH negative number of in-outs");
        }
        nQtyInOuts += 1;
        if (txdb.AddrTxExists(ADDR_TX_INOUT, strAddr, nQtyInOuts))
        {
            // This should never happen, output in-out tx already exists
            return error("ExploreConnectOutput() : TSNH output in-out tx already exists");
        }
        // write the new index record
        if (!txdb.WriteAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return error("ExploreConnectOutput() : could not write qty of in-outs");
        }
        // write the output in-out record
        if (!txdb.WriteAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts, txid, n, false))
        {
            return error("ExploreConnectOutput() : could not write output in-out");
        }

       /***************************************************************
        * 5. update the balance
        ***************************************************************/
        int64_t nValue = txOut.nValue;
        // someone could mindlessly set MIN_TXOUT_AMOUNT to 0 in the future
        if ((nValue < chainParams.MIN_TXOUT_AMOUNT) || (nValue <= 0))
        {
            // This should never happen, output value is less than minimum
            return error("ExploreConnectOutput() : TSNH output value less than minimum");
        }
        int64_t nBalanceOld;
        if (!txdb.ReadAddrBalance(ADDR_BALANCE, strAddr, nBalanceOld))
        {
            return error("ExploreConnectOutput() : could not read addr balance");
        }
        int64_t nBalanceNew = nBalanceOld + nValue;
        // no desire to remove evidence of this address here, even if no balance
        if (!txdb.WriteAddrBalance(ADDR_BALANCE, strAddr, nBalanceNew))
        {
            return error("ExploreConnectOutput() : could not write addr balance");
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
                    return error("ExploreConnectOutput() : could not remove addr set");
                }
                mapAddressBalancesAddRet.erase(nBalanceOld);
                setAddressBalancesRemoveRet.insert(nBalanceOld);
            }
            else
            {
                if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
                {
                    return error("ExploreConnectOutput() : could not write addr set");
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
                printf("asdf setAddr size is %u\n", setAddr.size());
                return error("ExploreConnectOutput() : could not read addr set %s",
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
                return error("ExploreConnectOutput() : could not write addr set");
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


bool ExploreConnectTx(CTxDB& txdb, const CTransaction &tx)
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

    if (!tx.IsCoinBase())
    {
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

    UpdateMapAddressBalances(mapAddressBalancesAdd,
                             setAddressBalancesRemove,
                             mapAddressBalances);

    return true;
}

bool ExploreConnectBlock(CTxDB& txdb, const CBlock *const block)
{
    // iterate backwards through everything on the disconnect
    BOOST_REVERSE_FOREACH(const CTransaction& tx, block->vtx)
    {
        if (!ExploreConnectTx(txdb, tx))
        {
            return false;
        }
    }
    return true;
}

bool ExploreDisconnectOutput(CTxDB& txdb,
                             const CTransaction& tx,
                             const int& n,
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
        printf("DisonnectBlock() : output %u has insoluble script\n   %s\n",
               n, txid.ToString().c_str());
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
        * 1. remove the in-out
        ***************************************************************/
        int nQtyInOuts;
        if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return error("ExploreDisconnectOutput() : could not read qty in-outs");
        }
        if (nQtyInOuts < 1)
        {
            // This should never happen : no in-outs
            return error("ExploreDisconnectOutput() : TSNH no in-outs found");
        }
        // for sanity: check that the explore db and chain are synced
        uint256 txidStored = 0;
        int nStored = -1;
        bool fStoredIsInput;
        if (!txdb.ReadAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts,
                             txidStored, nStored, fStoredIsInput))
        {
            return error("ExploreDisconnectOutput() : could not read in-out");
        }
        if (txidStored != txid)
        {
            // This should never happen, stored tx doesn't match
            return error("ExploreDisconnectOutput() : TSNH stored in-out doesn't match");
        }
        if (nStored != n)
        {
            // This should never happen, stored n doesn't match output n
            return error("ExploreDisconnectOutput() : TSNH stored n for in-out doesn't match");
        }
        if (fStoredIsInput)
        {
            // This should never happen, stored is an input
            return error("ExploreDisconnectOutput() : TSNH stored is an input");
        }
        // finalize removal
        if (!txdb.RemoveAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts))
        {
            return error("ExploreDisconnectOutput() : could not remove in-out");
        }
        nQtyInOuts -= 1;
        // there is no desire to remove evidence of this address here,
        // so we don't test for 0 outputs, etc
        if (!txdb.WriteAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return error("ExploreDisconnectOutput() : could not write qty in-outs");
        }

       /***************************************************************
        * 2. remove the output lookup
        ***************************************************************/
        // fetch the OutputID
        int nOutputID = -1;
        if (!txdb.ReadAddrLookup(ADDR_LOOKUP_OUTPUT, strAddr, txid, n,
                                                     nOutputID))
        {
            return error("ExploreDisconnectOutput() : could not read output ID");
        }
        if (nOutputID < 1)
        {
            // This should never happen : invalid output ID
            return error("ExploreDisconnectOutput() : invalid output ID");
        }
        // finalize removal of the lookup
        if (!txdb.RemoveAddrLookup(ADDR_LOOKUP_OUTPUT, strAddr, txid, n))
        {
            return error("ExploreDisconnectOutput() : could not remove the output lookup");
        }

       /***************************************************************
        * 3. remove the output
        ***************************************************************/
        int nQtyOutputs;
        if (!txdb.ReadAddrQty(ADDR_QTY_OUTPUT, strAddr, nQtyOutputs))
        {
            return error("ExploreDisconnectOutput() : could not read qty outputs");
        }
        if (nQtyOutputs != nOutputID)
        {
            // This should never happen : output ID mismatch
            return error("ExploreDisconnectOutput() : TSNH output ID mismatch");
        }
        // for sanity: check that the explore db and chain are synced
        txidStored = 0;
        nStored = -1;
        bool fStoredIsSpent;
        if (!txdb.ReadAddrTx(ADDR_TX_OUTPUT, strAddr, nOutputID,
                             txidStored, nStored, fStoredIsSpent))
        {
            return error("ExploreDisconnectOutput() : could not read output");
        }
        if (txidStored != txid)
        {
            // This should never happen, stored tx doesn't match
            return error("ExploreDisconnectOutput() : TSNH stored tx doesn't match");
        }
        if (nStored != n)
        {
            // This should never happen, stored n doesn't match output n
            return error("ExploreDisconnectOutput() : TSNH stored vout doesn't match");
        }
        if (fStoredIsSpent)
        {
            // This should never happne, stored output is spent
            return error("ExploreDisconnectOutput() : TSNH stored output is spent");
        }
        // finalize removal
        if (!txdb.RemoveAddrTx(ADDR_TX_OUTPUT, strAddr, nOutputID))
        {
            return error("ExploreDisconnectOutput() : could not remove output");
        }
        nQtyOutputs -= 1;
        // there is no desire to remove evidence of this address here,
        // so we don't test for 0 outputs, etc
        if (!txdb.WriteAddrQty(ADDR_QTY_OUTPUT, strAddr, nQtyOutputs))
        {
            return error("ExploreDisconnectOutput() : could not write qty outputs");
        }

       /***************************************************************
        * 4. decrement the quantity unspent
        ***************************************************************/
        int nQtyUnspent;
        if (!txdb.ReadAddrQty(ADDR_QTY_UNSPENT, strAddr, nQtyUnspent))
        {
            return error("ExploreDisconnectOutput() : could not read qty unspent");
        }
        if (nQtyUnspent < 1)
        {
            return error("ExploreDisconnectOutput() : TSNH no qty unspent found");
        }
        nQtyUnspent -= 1;
        if (!txdb.WriteAddrQty(ADDR_QTY_UNSPENT, strAddr, nQtyUnspent))
        {
            return error("ExploreDisconnectOutput() : could not write qty unspent");
        }

       /***************************************************************
        * 5. update the balance
        ***************************************************************/
        int64_t nValue = txOut.nValue;
        // someone could mindlessly set MIN_TXOUT_AMOUNT to 0 in the future
        if ((nValue < chainParams.MIN_TXOUT_AMOUNT) || (nValue <= 0))
        {
            // This should never happen, output value is less than minimum
            return error("ExploreDisconnectOutput() : TSNH output value less than minimum");
        }
        int64_t nBalanceOld;
        if (!txdb.ReadAddrBalance(ADDR_BALANCE, strAddr, nBalanceOld))
        {
            return error("ExploreDisconnectOutput() : could not read addr balance");
        }
        if (nValue > nBalanceOld)
        {
            // This should never happen: output value exceeds balance
            return error("ExploreDisconnectOutput() : TSNH output value exceeds balance");
        }
        int64_t nBalanceNew = nBalanceOld - nValue;
        // no desire to remove evidence of this address here, even if no balance
        if (!txdb.WriteAddrBalance(ADDR_BALANCE, strAddr, nBalanceNew))
        {
            return error("ExploreDisconnectOutput() : could not write addr balance");
        }

       /***************************************************************
        * 6. update the balance sets (for rich list, etc)
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
                    return error("ExploreDisconnectOutput() : could not remove addr set");
                }
                mapAddressBalancesAddRet.erase(nBalanceOld);
                setAddressBalancesRemoveRet.insert(nBalanceOld);
            }
            else
            {
                if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
                {
                    return error("ExploreDisconnectOutput() : could not write addr set");
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
                    return error("ExploreDisconnectOutput() : could not read addr set %s",
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
                    return error("ExploreDisconnectOutput() : could not write addr set");
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
                            const int& n,
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
        * 1. remove the in-out
        ***************************************************************/
        int nQtyInOuts;
        if (!txdb.ReadAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return error("ExploreDisconnectInput() : could not read qty in-outs");
        }
        if (nQtyInOuts < 1)
        {
            // This should never happen : no in-outs
            return error("ExploreDisconnectInput() : TSNH no in-outs found");
        }
        // for sanity: check that the explore db and chain are synced
        uint256 txidStored = 0;
        int nStored = -1;
        bool fStoredIsInput;
        if (!txdb.ReadAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts,
                             txidStored, nStored, fStoredIsInput))
        {
            return error("ExploreDisconnectInput() : could not read in-out");
        }
        if (txidStored != txid)
        {
            // This should never happen, stored tx doesn't match
            return error("ExploreDisconnectInput() : TSNH stored in-out doesn't match");
        }
        if (!fStoredIsInput)
        {
            // This should never happen, stored is an output
            return error("ExploreDisconnectInput() : TSNH stored is an output");
        }
        if (nStored != n)
        {
            // This should never happen, stored n doesn't match input n
            return error("ExploreDisconnectInput() : TSNH stored n for in-out doesn't match");
        }
        // finalize removal
        if (!txdb.RemoveAddrTx(ADDR_TX_INOUT, strAddr, nQtyInOuts))
        {
            return error("ExploreDisconnectInput() : could not remove in-out");
        }
        nQtyInOuts -= 1;
        // there is no desire to remove evidence of this address here,
        // so we don't test for 0 outputs, etc
        if (!txdb.WriteAddrQty(ADDR_QTY_INOUT, strAddr, nQtyInOuts))
        {
            return error("ExploreDisconnectInput() : could not write qty in-outs");
        }

       /***************************************************************
        * 2. remove the input
        ***************************************************************/
        int nQtyInputs;
        if (!txdb.ReadAddrQty(ADDR_QTY_INPUT, strAddr, nQtyInputs))
        {
            return error("ExploreDisconnectInput() : could not read qty inputs");
        }
        if (nQtyInputs < 1)
        {
            // This should never happen: no input found
            return error("ExploreDisconnectInput() : TSNH no input found");
        }
        // for sanity: check that db and chain are synced
        txidStored = 0;
        nStored = -1;
        if (!txdb.ReadAddrTx(ADDR_TX_INPUT, strAddr, nQtyInputs,
                             txidStored, nStored))
        {
            return error("ExploreDisconnectInput() : could not read input");
        }
        if (txidStored != txid)
        {
            // This should never happen, stored tx doesn't match input tx
            return error("ExploreDisconnectInput() : TSNH stored input tx doesn't match");
        }
        if (nStored != n)
        {
            // This should never happen, stored doesn't match input n
            return error("ExploreDisconnectInput() : TSNH stored input n doesn't match");
        }
        // finalize removal
        if (!txdb.RemoveAddrTx(ADDR_TX_INPUT, strAddr, nQtyInputs))
        {
            return error("ExploreDisconnectInput() : could not remove input");
        }
        nQtyInputs -= 1;
        // no need to remove evidence of this address here even if balance is 0
        if (!txdb.WriteAddrQty(ADDR_QTY_INPUT, strAddr, nQtyInputs))
        {
            return error("ExploreDisconnectInput() : could not write qty of inputs");
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
            return error("ExploreDisconnectInput() : could not read output ID");
        }
        if (nOutputID < 1)
        {
            // This should never happen : invalid output ID
            return error("ExploreDisconnectInput() : invalid output ID");
        }
        // for sanity: check that the explore db and chain are synced
        txidStored = 0;
        nStored = -1;
        bool fStoredIsSpent;
        if (!txdb.ReadAddrTx(ADDR_TX_OUTPUT, strAddr, nOutputID,
                             txidStored, nStored, fStoredIsSpent))
        {
            return error("ExploreDisconnectInput() : could not read output");
        }
        if (txidStored != txIn.prevout.hash)
        {
            // This should never happen, stored tx doesn't match
            return error("ExploreDisconnectInput() : TSNH stored tx doesn't match");
        }
        if (static_cast<unsigned int>(nStored) != txIn.prevout.n)
        {
            // This should never happen, stored n doesn't match output n
            return error("ExploreDisconnectInput() : TSNH stored vout doesn't match");
        }
        if (!fStoredIsSpent)
        {
            // This should never happen, stored prev output is unspent
            return error("ExploreDisconnectInput() : TSNH stored prev output is unspent");
        }
        // write the prev output as unspent
        fStoredIsSpent = false;
        if (!txdb.WriteAddrTx(ADDR_TX_OUTPUT, strAddr, nOutputID,
                              txIn.prevout.hash, txIn.prevout.n, fStoredIsSpent))
        {
            return error("ExploreDisconnectInput() : could not write output as spent");
        }
        // unspent are not stored separately from other outputs because the
        //   user wallet is expected to store all inputs and outputs anyway
        // increment the unspent count
        int nQtyUnspent;
        if (!txdb.ReadAddrQty(ADDR_QTY_UNSPENT, strAddr, nQtyUnspent))
        {
            return error("ExploreDisconnectInput() : could not read qty unspent");
        }
        nQtyUnspent += 1;
        if (!txdb.WriteAddrQty(ADDR_QTY_UNSPENT, strAddr, nQtyUnspent))
        {
            return error("ExploreDisconnectInput() : could not write qty unspent");
        }

       /***************************************************************
        * 4. update the balance
        ***************************************************************/
        int64_t nValue = txOut.nValue;
        // someone could mindlessly set MIN_TXOUT_AMOUNT to 0 in the future
        if ((nValue < chainParams.MIN_TXOUT_AMOUNT) || (nValue <= 0))
        {
            // This should never happen, output value is less than minimum
            return error("ExploreDisconnectInput() : TSNH output value less than minimum");
        }
        int64_t nBalanceOld;
        if (!txdb.ReadAddrBalance(ADDR_BALANCE, strAddr, nBalanceOld))
        {
            return error("ExploreDisconnectTx() : could not read addr balance");
        }
        int64_t nBalanceNew = nBalanceOld + nValue;
        // no desire to remove evidence of this address here, even if no balance
        if (!txdb.WriteAddrBalance(ADDR_BALANCE, strAddr, nBalanceNew))
        {
            return error("ExploreDisconnectInput() : could not write addr balance");
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
                    return error("ExploreDisconnectInput() : could not remove addr set");
                }
                mapAddressBalancesAddRet.erase(nBalanceOld);
                setAddressBalancesRemoveRet.insert(nBalanceOld);
            }
            else
            {
                if (!txdb.WriteAddrSet(ADDR_SET_BAL, nBalanceOld, setAddr))
                {
                    return error("ExploreDisconnectInput() : could not write addr set");
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
                return error("ExploreDisconnectInput() : could not read addr set %s",
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
                return error("ExploreDisconnectInput() : could not write addr set");
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

        ExploreDisconnectOutput(txdb, tx, n, txid,
                                mapAddressBalancesAdd,
                                setAddressBalancesRemove);
    }

    if (!tx.IsCoinBase())
    {
        // inputs (iterate backwards)
        for (int n = tx.vin.size() - 1; n >= 0; --n)
        {
            ExploreDisconnectInput(txdb, tx, n, mapInputs, txid,
                                   mapAddressBalancesAdd,
                                   setAddressBalancesRemove);
        }
    }

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

