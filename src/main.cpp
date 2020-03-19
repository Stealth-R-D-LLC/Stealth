// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2014-2018 Stealth R&D LLC
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "alert.h"
#include "checkpoints.h"
#include "txdb-leveldb.h"
#include "net.h"
#include "init.h"
#include "key.h"
#include "ui_interface.h"
#include "kernel.h"
#include "QPRegistry.hpp"
#include "stealthaddress.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

using namespace std;
using namespace boost;

//
// Global state
//

extern std::map<txnouttype, QPKeyType> mapQPoSKeyTypes;

CCriticalSection cs_setpwalletRegistered;
set<CWallet*> setpwalletRegistered;

CCriticalSection cs_main;

CTxMemPool mempool;
unsigned int nTransactionsUpdated = 0;

map<uint256, CBlockIndex*> mapBlockIndex;
set<pair<COutPoint, unsigned int> > setStakeSeen;
uint256 hashGenesisBlock = hashGenesisBlockOfficial;
static CBigNum bnProofOfWorkLimit(~uint256(0) >> 20);
static CBigNum bnProofOfStakeLimit(~uint256(0) >> 2);

static CBigNum bnProofOfWorkLimitTestNet(~uint256(0) >> 16);
static CBigNum bnProofOfStakeLimitTestNet(~uint256(0) >> 2);

unsigned int nStakeMinAge = 60 * 60 * 24 * 3;    //minimum age for coin age:  3 day
unsigned int nStakeMaxAge = 60 * 60 * 24 * 9;    //stake age of full weight:  9 day
unsigned int nStakeTargetSpacing = 60;           // 60 sec block spacing

int64_t nChainStartTime = 1403684997;
int nCoinbaseMaturity = 40;
CBlockIndex* pindexGenesisBlock = NULL;
int nBestHeight = -1;
CBigNum bnBestChainTrust = 0;
CBigNum bnBestInvalidTrust = 0;
uint256 hashBestChain = 0;
CBlockIndex* pindexBest = NULL;
int64_t nTimeBestReceived = 0;

static const int GETBLOCKS_LIMIT = 2000;


CMedianFilter<int> cPeerBlockCounts(5, 0); // Amount of blocks that other nodes claim to have

map<uint256, CBlock*> mapOrphanBlocks;
multimap<uint256, CBlock*> mapOrphanBlocksByPrev;
set<pair<COutPoint, unsigned int> > setStakeSeenOrphan;
map<uint256, uint256> mapProofOfStake;

map<uint256, CDataStream*> mapOrphanTransactions;
map<uint256, map<uint256, CDataStream*> > mapOrphanTransactionsByPrev;

// Constant stuff for coinbase transactions we create:
CScript COINBASE_FLAGS;

const string strMessageMagic = "StealthCoin Signed Message:\n";

double dHashesPerSec;
int64_t nHPSTimerStart;

// Settings
int64_t nTransactionFee = MIN_TX_FEE;
int64_t nReserveBalance = 0;


//////////////////////////////////////////////////////////////////////////////
//
// forks
//

int GetFork(int nHeight)
{
    // Make sure Heights are ascending!
    const int aForks[2][TOTAL_FORKS][2] =
                                     {
    /* MAIN NET */                     {      // Height, Fork Number
    /* Jul  4 02:47:04 MST 2014 */       {            0, XST_GENESIS },
    /* Jul 11 18:33:08 MST 2014 */       { CUTOFF_POW_M, XST_FORK002 },
    /* Oct  9 00:00:42 MST 2014 */       {       130669, XST_FORK004 },
    /* Aug 16 10:23:28 MST 2017 */       {      1732201, XST_FORK005 },
    /* Nov 14 08:09:53 MDT 2018 */       {      2378000, XST_FORK006 },
    /* Approx ????????????????  */       { START_PURCHASE_M, XST_FORKPURCHASE },
    /* Approx ????????????????  */       { CUTOFF_POS_M, XST_FORKASDF }
                                       },
    /* TEST NET */                     {      // Height, Fork Number
                                         {            0, XST_GENESIS },
    /*                          */       { CUTOFF_POW_T, XST_FORK002 },
                                         {          130, XST_FORK004 },
                                         {          140, XST_FORK005 },
                                         {          145, XST_FORK006 },
                                         { START_PURCHASE_T, XST_FORKPURCHASE },
                                         { CUTOFF_POS_T, XST_FORKASDF }
                                       }
                                     };

    // loop has strange logic, but if fork i height is greater than nHeight
    // then you are on fork i-1
    const int idx = fTestNet ? 1 : 0;
    int nFork = aForks[idx][0][1];
    for (int i = 1; i < TOTAL_FORKS; ++i)
    {
       if (aForks[idx][i][0] > nHeight)
       {
           break;
       }
       nFork = aForks[idx][i][1];
    }
    return nFork;
}


//////////////////////////////////////////////////////////////////////////////
//
// network
//

int GetMinPeerProtoVersion(int nHeight)
{
    // helps to prevent buffer overrun
    static const int nVersions = 5;

    // Make sure forks are ascending!
    const int aVersions[nVersions][2] = {
    //                                    Fork, Proto Version
                   {               XST_GENESIS,         62020 },
                   {               XST_FORK005,         62100 },
                   {               XST_FORK006,         62200 },
                   {          XST_FORKPURCHASE,         63000 },
                   {              XST_FORKASDF,         63000 }
                                          };

    int nFork = GetFork(nHeight);

    int nVersion = aVersions[0][1];

    for (int i = 1; i < nVersions; ++i)
    {
       if (aVersions[i][0] > nFork)
       {
           break;
       }
       nVersion = aVersions[i][1];
    }

    return nVersion;
}





//////////////////////////////////////////////////////////////////////////////
//
// block creation
//

const char* DescribeBlockCreationResult(BlockCreationResult r)
{
    switch (r)
    {
        case BLOCKCREATION_OK: return "OK";
        case BLOCKCREATION_QPOS_IN_REPLAY: return "QPoS In Replay";
        case BLOCKCREATION_NOT_CURRENTSTAKER: return "Not Current Staker";
        case BLOCKCREATION_QPOS_BLOCK_EXISTS: return "QPoS Block Exists";
        case BLOCKCREATION_INSTANTIATION_FAIL: return "Instantiation Fail";
        case BLOCKCREATION_REGISTRY_FAIL: return "Registry Fail";
        case BLOCKCREATION_PURCHASE_FAIL: return "Purchase Fail";
        case BLOCKCREATION_CLAIM_FAIL: return "Claim Fail";
        case BLOCKCREATION_PROOFTYPE_FAIL: return "Prooftype Fail";
    }
    return NULL;
}

//////////////////////////////////////////////////////////////////////////////
//
// dispatching functions
//

// These functions dispatch to one or all registered wallets

void RegisterWallet(CWallet* pwalletIn)
{
    {
        LOCK(cs_setpwalletRegistered);
        setpwalletRegistered.insert(pwalletIn);
    }
}

void UnregisterWallet(CWallet* pwalletIn)
{
    {
        LOCK(cs_setpwalletRegistered);
        setpwalletRegistered.erase(pwalletIn);
    }
}

// check whether the passed transaction is from us
bool static IsFromMe(CTransaction& tx)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        if (pwallet->IsFromMe(tx))
            return true;
    return false;
}

// get the wallet transaction with the given hash (if it exists)
bool static GetTransaction(const uint256& hashTx, CWalletTx& wtx)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        if (pwallet->GetTransaction(hashTx, wtx))
            return true;
    return false;
}

// erases transaction with the given hash from all wallets
void static EraseFromWallets(uint256 hash)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->EraseFromWallet(hash);
}

// make sure all wallets know about the given transaction, in the given block
void SyncWithWallets(const CTransaction& tx, const CBlock* pblock, bool fUpdate, bool fConnect)
{
    if (!fConnect)
    {
        // ppcoin: wallets need to refund inputs when disconnecting coinstake
        if (tx.IsCoinStake())
        {
            BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
                if (pwallet->IsFromMe(tx))
                    pwallet->DisableTransaction(tx);
        }
        return;
    }

    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->AddToWalletIfInvolvingMe(tx, pblock, fUpdate);
}

// notify wallets about a new best chain
void static SetBestChain(const CBlockLocator& loc)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->SetBestChain(loc);
}

// notify wallets about an updated transaction
void static UpdatedTransaction(const uint256& hashTx)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->UpdatedTransaction(hashTx);
}

// dump all wallets
void static PrintWallets(const CBlock& block)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->PrintWallet(block);
}

// notify wallets about an incoming inventory (for request counts)
void static Inventory(const uint256& hash)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->Inventory(hash);
}

// ask wallets to resend their transactions
void ResendWalletTransactions()
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->ResendWalletTransactions();
}



//////////////////////////////////////////////////////////////////////////////
//
// Registry Snapshots
//

void GetRegistrySnapshot(CTxDB &txdb, int nReplay, QPRegistry *pregistryTemp)
{
    printf("asdf 6.0.1.0 going to replay to %d\n", nReplay);
    // find a snapshot earlier than pindexReplay
    int nSnap = nReplay - (nReplay % BLOCKS_PER_SNAPSHOT);
    bool fReadSnapshot = false;
    while (nSnap >= GetPurchaseStart())
    {
        printf("asdf 6.0.2.0 nSnap is %d\n", nSnap);
        if (txdb.ReadRegistrySnapshot(nSnap, *pregistryTemp))
        {
            printf("asdf 6.0.3.0 breaking at nSnap is %d\n", nSnap);
            fReadSnapshot = true;
            break;
        }
        nSnap -= BLOCKS_PER_SNAPSHOT;
    }
    if (!fReadSnapshot)
    {
        printf("asdf 6.0.4.0 going fresh -- snap is %d\n", nSnap);
        // ensure it is fresh
        pregistryTemp->SetNull();
    }
}





//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

bool AddOrphanTx(const CDataStream& vMsg)
{
    CTransaction tx;
    CDataStream(vMsg) >> tx;
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return false;

    CDataStream* pvMsg = new CDataStream(vMsg);

    // Ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. If a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 10,000 orphans, each of which is at most 5,000 bytes big is
    // at most 500 megabytes of orphans:
    if (pvMsg->size() > 5000)
    {
        printf("ignoring large orphan tx (size: %" PRIszu ", hash: %s)\n", pvMsg->size(), hash.ToString().c_str());
        delete pvMsg;
        return false;
    }

    mapOrphanTransactions[hash] = pvMsg;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
        mapOrphanTransactionsByPrev[txin.prevout.hash].insert(make_pair(hash, pvMsg));

    printf("stored orphan tx %s (mapsz %" PRIszu ")\n", hash.ToString().c_str(),
        mapOrphanTransactions.size());
    return true;
}

void static EraseOrphanTx(uint256 hash)
{
    if (!mapOrphanTransactions.count(hash))
        return;
    const CDataStream* pvMsg = mapOrphanTransactions[hash];
    CTransaction tx;
    CDataStream(*pvMsg) >> tx;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        mapOrphanTransactionsByPrev[txin.prevout.hash].erase(hash);
        if (mapOrphanTransactionsByPrev[txin.prevout.hash].empty())
            mapOrphanTransactionsByPrev.erase(txin.prevout.hash);
    }
    delete pvMsg;
    mapOrphanTransactions.erase(hash);
}

unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans)
{
    unsigned int nEvicted = 0;
    while (mapOrphanTransactions.size() > nMaxOrphans)
    {
        // Evict a random orphan:
        uint256 randomhash = GetRandHash();
        map<uint256, CDataStream*>::iterator it = mapOrphanTransactions.lower_bound(randomhash);
        if (it == mapOrphanTransactions.end())
            it = mapOrphanTransactions.begin();
        EraseOrphanTx(it->first);
        ++nEvicted;
    }
    return nEvicted;
}



//////////////////////////////////////////////////////////////////////////////
//
// CTransaction and CTxIndex
//

// empty keyRet on return means the input may be good, but can't get key
// can only get signatory (compressed pubkey) from PUBKEY/HASH and CLAIM
bool CTransaction::GetSignatory(const MapPrevTx &mapInputs,
                                unsigned int idx, CPubKey &keyRet) const
{
    keyRet.Clear();
    txnouttype typetxo;
    vector<valtype> vSolutions;
    if (idx >= vin.size())
    {
        return false;
    }
    CTxIn input = vin[idx];
    CTxOut prevout = GetOutputFor(input, mapInputs);
    if (!Solver(prevout.scriptPubKey, typetxo, vSolutions))
    {
        // input is bad
        return false;
    }
    switch (typetxo)
    {
        case TX_PUBKEY:
            keyRet = CPubKey(vSolutions.front());
            if (!keyRet.IsCompressed())
            {
                // pubkey is fine, but only compressed pubkeys are allowed
                return false;
            }
            break;
        case TX_PUBKEYHASH:
        case TX_CLAIM:
            // extract pubkey from scriptSig (last 33 bytes)
            //     scriptSig: <sig> <pubKey>
            // sig validation of inputs done elsewhere since this is a spend
            if (input.scriptSig.size() >= 33)
            {
                CScript::const_iterator last = input.scriptSig.end();
                CScript::const_iterator first = last - 33;
                keyRet = CPubKey(static_cast<valtype>(CScript(first, last)));
            }
            else
            {
                return false;
            }
            break;
        default:
            return false;
    }
    return true;
}

// compares owner key of nStakerID to the input signatory
bool CTransaction::ValidateSignatory(const MapPrevTx &mapInputs,
                                     int idx, const CPubKey &keyOwner) const
{
    CPubKey keySignatory;
    if (!GetSignatory(mapInputs, 0, keySignatory))
    {
        // nonstandard
        return false;
    }
    if (keySignatory.IsEmpty())
    {
        // set key input should be PUBKEY, PUBKEYHASH, or CLAIM
        return false;
    }
    return (keySignatory == keyOwner);
}

// compares owner key of nStakerID to the input signatory
bool CTransaction::ValidateSignatory(const QPRegistry *pregistry,
                                     const MapPrevTx &mapInputs,
                                     int idx, unsigned int nStakerID) const
{
    CPubKey keyOwner;
    if (!pregistry->GetOwnerKey(nStakerID, keyOwner))
    {
        // ID doesn't correspond to a qualified staker
        return false;
    }
    return ValidateSignatory(mapInputs, idx, keyOwner);
}


// this does a full check: depends on the state of the registry
// total value in and out is checked elsewhere, where it is more sensible
// mapRet is keyed by the normalized (lowercase) alias
bool CTransaction::CheckPurchases(const QPRegistry *pregistry,
                                  int64_t nStakerPrice,
                                  map<string, qpos_purchase> &mapRet) const
{
    int nFork = GetFork(nBestHeight + 1);
    BOOST_FOREACH(const CTxOut &txout, vout)
    {
        txnouttype typetxo;
        vector<valtype> vSolutions;
        if (!Solver(txout.scriptPubKey, typetxo, vSolutions))
        {
            // nonstandard
            return error("CheckPurchase(): nonstandard");
        }
        if ((typetxo != TX_PURCHASE1) && (typetxo != TX_PURCHASE3))
        {
            // not a purchase
            continue;
        }
        if (nFork < XST_FORKPURCHASE)
        {
            // too soon to purchase
            return error("CheckPurchase(): too soon");
        }
        if (txout.nValue != 0)
        {
            // purchase output can't have a value
            return error("CheckPurchase(): has value");
        }
        if (vSolutions.empty())
        {
            // should never happen
            return error("CheckPurchase(): no vSolution");
        }
        valtype vch = vSolutions.front();
        qpos_purchase purchase;
        ExtractPurchase(vch, purchase);
        if (typetxo == TX_PURCHASE1)
        {
            if (purchase.keys.size() != 1)
            {
                // malformed PURCHASE1
                return error("CheckPurchase(): malformed 1");
            }
        }
        else if (purchase.keys.size() != 3)
        {
            // malformed PURCHASE3
            return error("CheckPurchase(): malformed 3");
        }
        // disallowing more than 2x price removes any chance of overflow
        if ((purchase.value < nStakerPrice) ||
            (purchase.value > (nStakerPrice * 2)))
        {
            // illegal amount paid for registration
            return error("CheckPurchase(): bad pay value");
        }
        if (purchase.pcm > 100000)
        {
            // can't delegate more than 100%
            return error("CheckPurchase(): delegate payout too much");
        }
        string sLC;
        if (!pregistry->AliasIsAvailable(purchase.alias, sLC))
        {
            // alias is not valid or is taken
            return error("CheckPurchase(): alias not valid or taken");
        }
        if (mapRet.count(sLC) > 0)
        {
            // tx registers same alias more than once
            return error("CheckPurchase(): multiple regs");
        }
        mapRet[sLC] = purchase;
    }
    // returns true for no registrations too
    // to know if any were registered, check size of mapRet
    return true;
}


// this does a full check: depends on the state of the registry
// lots of checks are done to allow 1, 2, or 3 key changes in one tx
// enforces that owner change is last, if there are multiple changes
// vout order determines sequence of key changes
// return pair is <staker ID, vector of setkeys>
bool CTransaction::CheckSetKeys(const QPRegistry *pregistry,
                                const MapPrevTx &mapInputs,
                                vector<qpos_setkey> &vRet) const
{
    // one block after the purchase period starts
    int nFork = GetFork(nBestHeight);
    vRet.clear();
    int fKeyTypes = 0;
    unsigned int nStakerID = 0;
    bool fCheckedSignatory = false;
    BOOST_FOREACH(const CTxOut &txout, vout)
    {
        txnouttype typetxo;
        vector<valtype> vSolutions;
        if (!Solver(txout.scriptPubKey, typetxo, vSolutions))
        {
            printf("CheckSetKeys(): fail: nonstandard\n");
            // nonstandard
            return false;
        }
        if ((typetxo != TX_SETOWNER) &&
            (typetxo != TX_SETDELEGATE) &&
            (typetxo != TX_SETCONTROLLER))
        {
            continue;
        }
        // one block after the purchase period starts
        if (nFork < XST_FORKPURCHASE)
        {
            printf("CheckSetKeys(): fail: too soon\n");
            // too soon to setkeys
            return false;
        }
        if (txout.nValue != 0)
        {
            // setkey output can't have a value
            printf("CheckSetKeys(): fail: output has a value\n");
            return false;
        }
        if (vin.size() != 1)
        {
            // key setting transactions have only 1 input
            // to avoid complex searches through inputs for keys
            printf("CheckSetKeys(): fail: multiple inputs\n");
            return false;
        }
        QPKeyType fThisKeyType = mapQPoSKeyTypes[typetxo];
        if (fKeyTypes & fThisKeyType)
        {
            // don't allow assigning the same key twice in one tx
            printf("CheckSetKeys(): fail: multiple assignment of same key\n");
            return false;
        }
        if ((fThisKeyType != QPKEY_OWNER) && (QPKEY_OWNER & fKeyTypes))
        {
            // can't change owner then expect any other key change to work
            // especially since registry won't be updated until the
            //    tx is connected
            printf("CheckSetKeys(): fail: already changed owner\n");
            return false;
        }
        qpos_setkey setkey;
        setkey.keytype = fThisKeyType;
        ExtractSetKey(vSolutions.front(), setkey);
        if ((fThisKeyType == QPKEY_CONTROLLER) && (setkey.pcm > 100000))
        {
            printf("CheckSetKeys(): fail: pcm too high\n");
            // can't assign more than 100%
            return false;
        }
        if (nStakerID == 0)
        {
            if (!pregistry->IsQualifiedStaker(setkey.id))
            {
                // disqualified stakers get purged before key
                // changes would have any effect
                printf("CheckSetKeys(): fail: staker is not qualified\n");
                return false;
            }
            nStakerID = setkey.id;
        }
        else if (setkey.id != nStakerID)
        {
            // avoid complexities by disallowing more than one ID
            printf("CheckSetKeys(): fail: can't change multiple stakers\n");
            return false;
        }
        // only need to do this once per tx because only one ID allowed
        if (!fCheckedSignatory)
        {
            if (!ValidateSignatory(pregistry, mapInputs, 0, setkey.id))
            {
                // signatory doesn't own staker
                printf("CheckSetKeys(): fail: signatory doesn't own staker\n");
                return false;
            }
            fCheckedSignatory = true;
        }
        fKeyTypes &= fThisKeyType;
        vRet.push_back(setkey);
    }
    return true;
}


// this does a full check: depends on the state of the registry
// only one state change per tx allowed because only one ID is allowed per tx
// true return value and setstateRet.id == 0 may be good tx, but not setstate
bool CTransaction::CheckSetState(const QPRegistry *pregistry,
                                 const MapPrevTx &mapInputs,
                                 qpos_setstate &setstateRet) const
{
    // one block after the purchase period starts
    int nFork = GetFork(nBestHeight);
    qpos_setstate setstate;
    setstate.id = 0;
    setstate.enable = false;
    txnouttype typetxo;
    vector<valtype> vSolutions;
    for (unsigned int i = 0; i < vout.size(); ++i)
    {
        if (!Solver(vout[i].scriptPubKey, typetxo, vSolutions))
        {
            // nonstandard
            printf("CheckSetState(): nonstandard\n");
            return false;
        }
        if ((typetxo != TX_ENABLE) && (typetxo != TX_DISABLE))
        {
            continue;
        }
        // one block after the purchase period starts
        if (nFork < XST_FORKPURCHASE)
        {
            // too soon to set state
            printf("CheckSetState(): too soon\n");
            return false;
        }
        if (vout[i].nValue != 0)
        {
            // setstate output can't have a value
            printf("CheckSetState(): output value not permitted\n");
            return false;
        }
        if (setstate.id != 0)
        {
            // only allow one per tx
            printf("CheckSetState(): multiple setstates\n");
            return false;
        }
        if (vin.size() != 1)
        {
            // avoid searching for keys by allowing only 1 input
            printf("CheckSetState(): multiple inputs\n");
            return false;
        }
        setstate.enable = (typetxo == TX_ENABLE);
        ExtractSetState(vSolutions.front(), setstate);
        if (!pregistry->IsQualifiedStaker(setstate.id))
        {
            // disallow setting state of disqualified stakers even if extant
            printf("CheckSetState(): staker is disqualified\n");
            return false;
        }
        if (!ValidateSignatory(pregistry, mapInputs, 0, setstate.id))
        {
            // signatory doesn't own the staker
            printf("CheckSetState(): signatory is not owner\n");
            return false;
        }
    }
    setstateRet.id = setstate.id;
    setstateRet.enable = setstate.enable;
    return true;
}


// this does a full check: depends on the state of the registry
// false return value means claim is malformed or illegal
bool CTransaction::CheckClaim(const QPRegistry *pregistry,
                              const MapPrevTx &mapInputs,
                              qpos_claim &claimRet) const
{
    // 0 claim value with return true means tx is okay, but not a claim
    // any false return value means tx or inputs are bad
    claimRet.value = 0;
    bool fFoundClaim = false;
    txnouttype typetxo;
    vector<valtype> vSolutions;
    // this loop is complicated so it doesn't return false
    // for valid non-claim transactions
    for (unsigned int i = 0; i < vout.size(); ++i)
    {
        if (!Solver(vout[i].scriptPubKey, typetxo, vSolutions))
        {
            return false;
        }
        if (typetxo == TX_CLAIM)
        {
            if ((vin.size() != 1) || (vout.size() != 1))
            {
                // claims can only have 1 input and 1 output to keep things simple
                return false;
            }
            fFoundClaim = true;
        }
    }
    if (!fFoundClaim)
    {
        // tx could very well be fine, but it isn't a claim
        return true;
    }
    // make them wait a day
    if (GetFork(nBestHeight - QP_BLOCKS_PER_DAY) < XST_FORKASDF)
    {
        // too soon to purchase
        return false;
    }

    ExtractClaim(vSolutions.front(), claimRet);

    if (claimRet.value < MIN_TXOUT_AMOUNT)
    {
        return false;
    }

    if (!pregistry->CanClaim(claimRet.key, claimRet.value))
    {
        return false;
    }

    if (!ValidateSignatory(mapInputs, 0, claimRet.key))
    {
        // signatory doesn't own the staker
        return false;
    }

    return true;
}


// use only for fully validated transactions, no real checks are performed
// returns true if any of the qPoS transactions need inputs
bool CTransaction::GetQPoSTxDetails(vector<QPoSTxDetails> &vDeets) const
{
    bool fNeedsInputs = false;
    vector<valtype> vSolutions;
    txnouttype whichType;
    BOOST_FOREACH(const CTxOut &txout, vout)
    {
        if (!Solver(txout.scriptPubKey, whichType, vSolutions))
        {
            continue;
        }
        QPoSTxDetails deets;
        deets.t = whichType;
        switch (static_cast<txnouttype>(deets.t))
        {
        case TX_PURCHASE1:
        case TX_PURCHASE3:
            ExtractPurchase(vSolutions.front(), deets);
            break;
        case TX_SETOWNER:
        case TX_SETDELEGATE:
        case TX_SETCONTROLLER:
            fNeedsInputs = true;
            ExtractSetKey(vSolutions.front(), deets);
            break;
        case TX_ENABLE:
        case TX_DISABLE:
            fNeedsInputs = true;
            ExtractSetState(vSolutions.front(), deets);
            break;
        case TX_CLAIM:
            fNeedsInputs = true;
            ExtractClaim(vSolutions.front(), deets);
            break;
        default:
            continue;
        }
        vDeets.push_back(deets);
    }
    return fNeedsInputs;
}


bool CTransaction::ReadFromDisk(CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet)
{
    SetNull();
    if (!txdb.ReadTxIndex(prevout.hash, txindexRet))
        return false;
    if (!ReadFromDisk(txindexRet.pos))
        return false;
    if (prevout.n >= vout.size())
    {
        SetNull();
        return false;
    }
    return true;
}

bool CTransaction::ReadFromDisk(CTxDB& txdb, COutPoint prevout)
{
    CTxIndex txindex;
    return ReadFromDisk(txdb, prevout, txindex);
}

bool CTransaction::ReadFromDisk(COutPoint prevout)
{
    CTxDB txdb("r");
    CTxIndex txindex;
    return ReadFromDisk(txdb, prevout, txindex);
}

bool CTransaction::IsStandard() const
{
    if (nVersion > CTransaction::CURRENT_VERSION)
    {
        return false;
    }

    if (GetFork(nBestHeight + 1) >= XST_FORK005)
    {
        if (vout.size() < 1)
        {
             return false;
        }

        // Treat non-final transactions as non-standard to prevent a specific type
        // of double-spend attack, as well as DoS attacks. (if the transaction
        // can't be mined, the attacker isn't expending resources broadcasting it)
        // Basically we don't want to propagate transactions that can't be included in
        // the next block.
        //
        // However, IsFinalTx() is confusing... Without arguments, it uses
        // chainActive.Height() to evaluate nLockTime; when a block is
        // accepted, chainActive.Height()
        // is set to the value of nHeight in the block. However, when IsFinalTx()
        // is called within CBlock::AcceptBlock(), the height of the block *being*
        // evaluated is what is used. Thus if we want to know if a transaction can
        // be part of the *next* block, we need to call IsFinalTx() with one more
        // than chainActive.Height().
        //
        // Timestamps on the other hand don't get any special treatment, because we
        // can't know what timestamp the next block will have, and there aren't
        // timestamp applications where it matters.
        if (!IsFinal(nBestHeight + 1)) {
            return false;
        }

        // nTime, aka GetTxTime(), has a different purpose from nLockTime
        // but can be used in similar attacks
        // CTransaction gets timestamp from block upon NOTXTIME_VERSION
        // block timestamp checked elsewhere
        if (HasTimestamp() && (GetTxTime() > FutureDrift(GetAdjustedTime())))
        {
            return false;
        }

        // Extremely large transactions with lots of inputs can cost the network
        // almost as much to process as they cost the sender in fees, because
        // computing signature hashes is O(ninputs*txsize). Limiting transactions
        // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
        unsigned int sz = GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
        if (sz >= MAX_STANDARD_TX_SIZE) {
            return false;
        }
    }

    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        // Biggest 'standard' txin is a 3-signature 3-of-3 CHECKMULTISIG
        // pay-to-script-hash, which is 3 ~80-byte signatures, 3
        // ~65-byte public keys, plus a few script ops.
        if (txin.scriptSig.size() > 500)
            return false;
        if (!txin.scriptSig.IsPushOnly())
            return false;
    }

    unsigned int nDataOut = 0;
    unsigned int nTxnOut = 0;
    txnouttype whichType;
    BOOST_FOREACH(const CTxOut& txout, vout) {
        if (!::IsStandard(txout.scriptPubKey, whichType))
            return false;
        if (whichType == TX_NULL_DATA)
        {
            nDataOut++;
        } else
        {
        if (txout.nValue == 0)
            return false;
        nTxnOut++;
        }
    }
    if (nDataOut > nTxnOut) {
        return false;
    }
    return true;
}

//
// Check transaction inputs, and make sure any
// pay-to-script-hash transactions are evaluating IsStandard scripts
//
// Why bother? To avoid denial-of-service attacks; an attacker
// can submit a standard HASH... OP_EQUAL transaction,
// which will get accepted into blocks. The redemption
// script can be anything; an attacker could use a very
// expensive-to-check-upon-redemption script like:
//   DUP CHECKSIG DROP ... repeated 100 times... OP_1
//
bool CTransaction::AreInputsStandard(const MapPrevTx& mapInputs) const
{
    if (IsCoinBase())
        return true; // Coinbases don't use vin normally

    for (unsigned int i = 0; i < vin.size(); i++)
    {
        const CTxOut& prev = GetOutputFor(vin[i], mapInputs);

        vector<vector<unsigned char> > vSolutions;
        txnouttype whichType;
        // get the scriptPubKey corresponding to this input:
        const CScript& prevScript = prev.scriptPubKey;
        if (!Solver(prevScript, whichType, vSolutions))
            return false;
        int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
        if (nArgsExpected < 0)
            return false;

        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig the
        // IsStandard() call returns false
        vector<vector<unsigned char> > stack;
        if (!EvalScript(stack, vin[i].scriptSig, *this, i, SCRIPT_VERIFY_NONE, 0))
            return false;

        if (whichType == TX_SCRIPTHASH)
        {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            vector<vector<unsigned char> > vSolutions2;
            txnouttype whichType2;
            if (!Solver(subscript, whichType2, vSolutions2))
                return false;
            if (whichType2 == TX_SCRIPTHASH)
                return false;

            int tmpExpected;
            tmpExpected = ScriptSigArgsExpected(whichType2, vSolutions2);
            if (tmpExpected < 0)
                return false;
            nArgsExpected += tmpExpected;
        }

        if (stack.size() != (unsigned int)nArgsExpected)
            return false;
    }

    return true;
}

unsigned int
CTransaction::GetLegacySigOpCount() const
{
    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    BOOST_FOREACH(const CTxOut& txout, vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}


int CMerkleTx::SetMerkleBranch(const CBlock* pblock)
{
    if (fClient)
    {
        if (hashBlock == 0)
            return 0;
    }
    else
    {
        CBlock blockTmp;
        if (pblock == NULL)
        {
            // Load the block this tx is in
            CTxIndex txindex;
            if (!CTxDB("r").ReadTxIndex(GetHash(), txindex))
                return 0;
            if (!blockTmp.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos))
                return 0;
            pblock = &blockTmp;
        }

        // Update the tx's hashBlock
        hashBlock = pblock->GetHash();

        // Locate the transaction
        for (nIndex = 0; nIndex < (int)pblock->vtx.size(); nIndex++)
            if (pblock->vtx[nIndex] == *(CTransaction*)this)
                break;
        if (nIndex == (int)pblock->vtx.size())
        {
            vMerkleBranch.clear();
            nIndex = -1;
            printf("ERROR: SetMerkleBranch() : couldn't find tx in block\n");
            return 0;
        }

        // Fill in merkle branch
        vMerkleBranch = pblock->GetMerkleBranch(nIndex);
    }

    // Is the tx in a block that's in the main chain
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    return pindexBest->nHeight - pindex->nHeight + 1;
}


bool CTransaction::CheckTransaction() const
{
    // Basic checks that don't depend on any context
    if (vin.empty())
        return DoS(10, error("CTransaction::CheckTransaction() : vin empty %s",
                                  GetHash().ToString().c_str()));
    if (vout.empty())
    {
        return DoS(10, error("CTransaction::CheckTransaction() : vout empty %s",
                                  GetHash().ToString().c_str()));
    }
    // Size limits
    if (::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return DoS(100, error("CTransaction::CheckTransaction() : size limits failed %s",
                                  GetHash().ToString().c_str()));

    // Check for negative or overflow output values
    int64_t nValueOut = 0;
    for (unsigned int i = 0; i < vout.size(); i++)
    {
        const CTxOut& txout = vout[i];
        if (txout.IsEmpty() && !IsCoinBase() && !IsCoinStake())
        {
            return DoS(100, error("CTransaction::CheckTransaction() : txout empty for user transaction %s %u",
                                  GetHash().ToString().c_str(), i));
        }

        if (GetFork(nBestHeight+1) >= XST_FORK004)
        {
            if (txout.nValue < 0)
                return DoS(100,
                  error("CTransaction::CheckTransaction() : txout.nValue negative %s %u",
                                  GetHash().ToString().c_str(), i));
        }
        else
        {
            vector<valtype> vSolutions;
            txnouttype whichType;
            if (Solver(txout.scriptPubKey, whichType, vSolutions) &&
                (whichType >= TX_PURCHASE1) && (whichType <= TX_DISABLE))
            {
                if (txout.nValue < 0)
                {
                  return DoS(100,
                    error("CTransaction::CheckTransaction() : txout.nValue negative %s %u",
                                  GetHash().ToString().c_str(), i));
                }
            }
            else if ((!txout.IsEmpty()) && (txout.nValue < MIN_TXOUT_AMOUNT))
            {
                return DoS(100,
                  error("CTransaction::CheckTransaction() : txout.nValue below minimum %s %u",
                                  GetHash().ToString().c_str(), i));
            }
        }

        if (txout.nValue > MAX_MONEY)
            return DoS(100,
              error("CTransaction::CheckTransaction() : txout.nValue too high %s %u",
                                  GetHash().ToString().c_str(), i));
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return DoS(100,
              error("CTransaction::CheckTransaction() : txout total out of range %s %u",
                                  GetHash().ToString().c_str(), i));
    }

    // Check for duplicate inputs
    set<COutPoint> vInOutPoints;
    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return false;
        vInOutPoints.insert(txin.prevout);
    }

    if (IsCoinBase())
    {
        if (vin[0].scriptSig.size() < 2 || vin[0].scriptSig.size() > 100)
            return DoS(100,
               error("CTransaction::CheckTransaction() : coinbase script size is invalid %s",
                                  GetHash().ToString().c_str()));
    }
    else
    {
        BOOST_FOREACH(const CTxIn& txin, vin)
            if (txin.prevout.IsNull())
                return DoS(10, error("CTransaction::CheckTransaction() : prevout is null %s",
                                  GetHash().ToString().c_str()));
    }

    return true;
}


int64_t CTransaction::GetMinFee(unsigned int nBlockSize, bool fAllowFree,
                              enum GetMinFee_mode mode, unsigned int nBytes) const
{
    // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
    int64_t nBaseFee = (mode == GMF_RELAY) ? MIN_RELAY_TX_FEE : MIN_TX_FEE;

    unsigned int nNewBlockSize = nBlockSize + nBytes;
    int64_t nMinFee = (1 + (int64_t)nBytes / 1000) * nBaseFee;

    // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.01
    if (nMinFee < nBaseFee)
    {
        BOOST_FOREACH(const CTxOut& txout, vout)
            if (txout.nValue < CENT)
                nMinFee = nBaseFee;
    }

    // Raise the price as the block approaches full
    if (nBlockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN/2)
    {
        if (nNewBlockSize >= MAX_BLOCK_SIZE_GEN)
            return MAX_MONEY;
        nMinFee *= MAX_BLOCK_SIZE_GEN / (MAX_BLOCK_SIZE_GEN - nNewBlockSize);
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}


bool CTxMemPool::accept(CTxDB& txdb, CTransaction &tx,
                        bool fCheckInputs, bool* pfMissingInputs)
{
    if (pfMissingInputs)
        *pfMissingInputs = false;

    if (!tx.CheckTransaction())
        return error("CTxMemPool::accept() : CheckTransaction failed");

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return tx.DoS(100, error("CTxMemPool::accept() : coinbase as individual tx"));

    // ppcoin: coinstake is also only valid in a block, not as a loose transaction
    if (tx.IsCoinStake())
        return tx.DoS(100, error("CTxMemPool::accept() : coinstake as individual tx"));

    // Rather not work on nonstandard transactions (unless -testnet)
    if (!fTestNet && !tx.IsStandard())
        return error("CTxMemPool::accept() : nonstandard transaction type");

    // Do we already have it?
    uint256 hash = tx.GetHash();
    {
        LOCK(cs);
        if (mapTx.count(hash))
            return false;
    }
    if (fCheckInputs)
        if (txdb.ContainsTx(hash))
            return false;

    // Check for conflicts with in-memory transactions
    CTransaction* ptxOld = NULL;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        COutPoint outpoint = tx.vin[i].prevout;
        if (mapNextTx.count(outpoint))
        {
            // Disable replacement feature for now
            return false;

            // Allow replacing with a newer version of the same transaction
            if (i != 0)
                return false;
            ptxOld = mapNextTx[outpoint].ptx;
            if (ptxOld->IsFinal())
                return false;
            if (!tx.IsNewerThan(*ptxOld))
                return false;
            for (unsigned int i = 0; i < tx.vin.size(); i++)
            {
                COutPoint outpoint = tx.vin[i].prevout;
                if (!mapNextTx.count(outpoint) || mapNextTx[outpoint].ptx != ptxOld)
                    return false;
            }
            break;
        }
    }


    // Like claims, registrations need to be checked for validity
    // to prevent mempool flooding of bad registrations, although
    // these are going to take a reserve of a lot more XST.
    // The all-v-all check would be expensive, but registrations are
    // expensive so they shouldn't cause a non-trivial burden here.
    int64_t nValuePurchases = 0;
    map<string, qpos_purchase> mapNames;
    int64_t nStakerPrice = GetStakerPrice(pregistryMain, pindexBest);
    if (!tx.CheckPurchases(pregistryMain, nStakerPrice, mapNames))
    {
        return error("accept(): bad purchase\n");
        return false;
    }

    if (!mapNames.empty())
    {
        if (mapRegistrations.count(hash) != 0)
        {
            // duplicate hash already checked but check here anyway
            return false;
        }
        map<uint256, vector<string> >::const_iterator it;
        for (it = mapRegistrations.begin(); it != mapRegistrations.end(); ++it)
        {
            vector<string>::const_iterator jt;
            for (jt = it->second.begin(); jt != it->second.end(); ++jt)
            {
                if (mapNames.count(*jt) != 0)
                {
                    // trying to register an alias already in the mempool
                    return false;
                }
            }
        }

        vector<string> vNames;
        map<string, qpos_purchase>::const_iterator kt;
        for (kt = mapNames.begin(); kt != mapNames.end(); ++kt)
        {
            vNames.push_back(kt->first);
            nValuePurchases += kt->second.value;
        }
        mapRegistrations[hash] = vNames;
    }

    if (fCheckInputs)
    {
        MapPrevTx mapInputs;
        map<uint256, CTxIndex> mapUnused;
        bool fInvalid = false;
        if (!tx.FetchInputs(txdb, mapUnused, false, false, mapInputs, fInvalid))
        {
            if (fInvalid)
            {
                return error("accept() : FetchInputs found invalid tx %s",
                                                        hash.ToString().c_str());
            }
            if (pfMissingInputs)
            {
                *pfMissingInputs = true;
            }
            return false;
        }

        // The "input" for a claim is stored in the registry ledger which
        // cannot be deducted from the ledger until the block is accepted into
        // the main chain. Absent any checks, it is possible to spam the mempool
        // with duplicate claims for the same pubkey with different txids.
        // We protect against this "duplicate claim attack" by
        // enforcing that only one claim for a given pubkey
        // can exist in the mempool at once.
        qpos_claim claim;
        if (!tx.CheckClaim(pregistryMain, mapInputs, claim))
        {
            return false;
        }

        if (claim.value > 0)
        {
             map<uint256, CPubKey>::const_iterator it;
             for (it = mapClaims.begin(); it != mapClaims.end(); ++it)
             {
                 if (it->second == claim.key)
                 {
                     return false;
                 }
             }

             if (!pregistryMain->CanClaim(claim.key, claim.value))
             {
                 return false;
             }
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (!tx.AreInputsStandard(mapInputs) && !fTestNet)
            return error("CTxMemPool::accept() : nonstandard transaction input");

        // Note: if you modify this code to accept non-standard transactions, then
        // you should add code here to check that the transaction does a
        // reasonable number of ECDSA signature verifications.

        int64_t nFees = tx.GetValueIn(mapInputs, claim.value) -
                        (tx.GetValueOut() + nValuePurchases);
        unsigned int nSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

        // Don't accept it if it can't get into a block
        int64_t txMinFee = tx.GetMinFee(1000, false, GMF_RELAY, nSize);
        if (nFees < txMinFee)
            return error("CTxMemPool::accept() : not enough fees %s, %" PRId64 " < %" PRId64,
                         hash.ToString().c_str(),
                         nFees, txMinFee);

        // Continuously rate-limit free transactions
        // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
        // be annoying or make others' transactions take longer to confirm.
        if (nFees < MIN_RELAY_TX_FEE)
        {
            static CCriticalSection cs;
            static double dFreeCount;
            static int64_t nLastTime;
            int64_t nNow = GetTime();

            {
                LOCK(cs);
                // Use an exponentially decaying ~10-minute window:
                dFreeCount *= pow(1.0 - 1.0/600.0, (double)(nNow - nLastTime));
                nLastTime = nNow;
                // -limitfreerelay unit is thousand-bytes-per-minute
                // At default rate it would take over a month to fill 1GB
                if (dFreeCount > GetArg("-limitfreerelay", 15)*10*1000 && !IsFromMe(tx))
                    return error("CTxMemPool::accept() : free transaction rejected by rate limiter");
                if (fDebug)
                    printf("Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount+nSize);
                dFreeCount += nSize;
            }
        }

        unsigned int flags = STANDARD_SCRIPT_VERIFY_FLAGS;
        if (GetFork(nBestHeight+1) < XST_FORK005)
        {
            flags = flags & ~SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
        }

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!tx.ConnectInputs(txdb, mapInputs, mapUnused, CDiskTxPos(1,1,1),
            pindexBest, false, false, flags, nValuePurchases, claim.value))
        {
            return error("CTxMemPool::accept() : ConnectInputs failed %s", hash.ToString().c_str());
        }
    }

    // Store transaction in memory
    {
        LOCK(cs);
        if (ptxOld)
        {
            printf("CTxMemPool::accept() : replacing tx %s with new version\n", ptxOld->GetHash().ToString().c_str());
            remove(*ptxOld);
        }
        addUnchecked(hash, tx);
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
    if (ptxOld)
        EraseFromWallets(ptxOld->GetHash());

    printf("CTxMemPool::accept() : accepted %s (poolsz %" PRIszu ")\n",
           hash.ToString().c_str(),
           mapTx.size());
    return true;
}

bool CTransaction::AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs, bool* pfMissingInputs)
{
    return mempool.accept(txdb, *this, fCheckInputs, pfMissingInputs);
}

bool CTxMemPool::addUnchecked(const uint256& hash, CTransaction &tx)
{
    // Add to memory pool without checking anything.  Don't call this directly,
    // call CTxMemPool::accept to properly check the transaction first.
    {
        mapTx[hash] = tx;
        for (unsigned int i = 0; i < tx.vin.size(); i++)
            mapNextTx[tx.vin[i].prevout] = CInPoint(&mapTx[hash], i);
        nTransactionsUpdated++;
    }
    return true;
}


bool CTxMemPool::remove(const CTransaction &tx, bool fRecursive)
{
    // Remove transaction from memory pool
    {
        LOCK(cs);
        uint256 hash = tx.GetHash();
        if (mapTx.count(hash))
        {
            if (fRecursive)
            {
                for (unsigned int i = 0; i < tx.vout.size(); i++){
                    std::map<COutPoint, CInPoint>:: iterator it =
                                                       mapNextTx.find(COutPoint(hash, i));
                    if (it != mapNextTx.end())
                            remove(*it->second.ptx, true);
                }
            }
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
                mapNextTx.erase(txin.prevout);
            mapTx.erase(hash);
            nTransactionsUpdated++;
        }
        // cheap, non recursive, so do without checking mapTx, etc
        mapClaims.erase(hash);
        mapRegistrations.erase(hash);
    }
    return true;
}

bool CTxMemPool::removeConflicts(const CTransaction &tx)
{
    // Remove transactions which depend on inputs of tx, recursively
    LOCK(cs);
    BOOST_FOREACH(const CTxIn &txin, tx.vin) {
        std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            const CTransaction &txConflict = *it->second.ptx;
            if (txConflict != tx)
                remove(txConflict, true);
        }
    }
    return true;
}

void CTxMemPool::clear()
{
    LOCK(cs);
    mapTx.clear();
    mapNextTx.clear();
    ++nTransactionsUpdated;
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid)
{
    vtxid.clear();

    LOCK(cs);
    vtxid.reserve(mapTx.size());
    for (map<uint256, CTransaction>::iterator mi = mapTx.begin(); mi != mapTx.end(); ++mi)
        vtxid.push_back((*mi).first);
}


int CMerkleTx::GetDepthInMainChainINTERNAL(CBlockIndex* &pindexRet) const
{
    if (hashBlock == 0 || nIndex == -1)
        return 0;

    // Find the block it claims to be in
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    // Make sure the merkle branch connects to this block
    if (!fMerkleVerified)
    {
        if (CBlock::CheckMerkleBranch(GetHash(), vMerkleBranch, nIndex) != pindex->hashMerkleRoot)
            return 0;
        fMerkleVerified = true;
    }

    pindexRet = pindex;
    return pindexBest->nHeight - pindex->nHeight + 1;
}

int CMerkleTx::GetDepthInMainChain(CBlockIndex *&pindexRet) const
{
    int nResult = GetDepthInMainChainINTERNAL(pindexRet);
    if (nResult == 0 && !mempool.exists(GetHash()))
        return -1;
    return nResult;
}

int CMerkleTx::GetBlocksToMaturity() const
{
    if (!(IsCoinBase() || IsCoinStake()))
        return 0;
    return max(0, (nCoinbaseMaturity+20) - GetDepthInMainChain());
}


bool CMerkleTx::AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs)
{
    if (fClient)
    {
        if (!IsInMainChain() && !ClientConnectInputs())
            return false;
        return CTransaction::AcceptToMemoryPool(txdb, false);
    }
    else
    {
        return CTransaction::AcceptToMemoryPool(txdb, fCheckInputs);
    }
}

bool CMerkleTx::AcceptToMemoryPool()
{
    CTxDB txdb("r");
    return AcceptToMemoryPool(txdb);
}

bool CWalletTx::AcceptWalletTransaction(CTxDB& txdb, bool fCheckInputs)
{
    {
        LOCK(mempool.cs);
        // Add previous supporting transactions first
        BOOST_FOREACH(CMerkleTx& tx, vtxPrev)
        {
            if (!(tx.IsCoinBase() || tx.IsCoinStake()))
            {
                uint256 hash = tx.GetHash();
                if (!mempool.exists(hash) && !txdb.ContainsTx(hash))
                    tx.AcceptToMemoryPool(txdb, fCheckInputs);
            }
        }
        return AcceptToMemoryPool(txdb, fCheckInputs);
    }
    return false;
}

bool CWalletTx::AcceptWalletTransaction()
{
    CTxDB txdb("r");
    return AcceptWalletTransaction(txdb);
}


int CTxIndex::GetDepthInMainChain() const
{
    // Read block header
    CBlock block;
    if (!block.ReadFromDisk(pos.nFile, pos.nBlockPos, false))
        return 0;
    // Find the block in the index
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(block.GetHash());
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    return 1 + nBestHeight - pindex->nHeight;
}

// Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock
bool GetTransaction(const uint256 &hash, CTransaction &tx,
                    uint256 &hashBlock, unsigned int &nTimeBlock)
{
    {
        LOCK(cs_main);
        {
            LOCK(mempool.cs);
            if (mempool.lookup(hash, tx))
            {
                return true;
            }
        }
        CTxDB txdb("r");
        CTxIndex txindex;
        if (tx.ReadFromDisk(txdb, COutPoint(hash, 0), txindex))
        {
            CBlock block;
            if (block.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
            {
                hashBlock = block.GetHash();
                nTimeBlock = block.GetBlockTime();
            }
            return true;
        }
    }
    return false;
}


//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

static CBlockIndex* pblockindexFBBHLast;
CBlockIndex* FindBlockByHeight(int nHeight)
{
    CBlockIndex *pblockindex;
    if (nHeight < nBestHeight / 2)
        pblockindex = pindexGenesisBlock;
    else
        pblockindex = pindexBest;
    if (pblockindexFBBHLast && abs(nHeight - pblockindex->nHeight) > abs(nHeight - pblockindexFBBHLast->nHeight))
        pblockindex = pblockindexFBBHLast;
    while (pblockindex->nHeight > nHeight)
        pblockindex = pblockindex->pprev;
    while (pblockindex->nHeight < nHeight)
        pblockindex = pblockindex->pnext;
    pblockindexFBBHLast = pblockindex;
    return pblockindex;
}


bool CBlock::ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions)
{
    if (!fReadTransactions)
    {
        *this = pindex->GetBlockHeader();
        return true;
    }
    if (!ReadFromDisk(pindex->nFile, pindex->nBlockPos, fReadTransactions))
        return false;
    if (GetHash() != pindex->GetBlockHash())
        return error("CBlock::ReadFromDisk() : GetHash() doesn't match index");
    return true;
}


uint256 static GetOrphanRoot(const CBlock* pblock)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblock->hashPrevBlock))
        pblock = mapOrphanBlocks[pblock->hashPrevBlock];
    return pblock->GetHash();
}


// ppcoin: find block wanted by given orphan block
uint256 WantedByOrphan(const CBlock* pblockOrphan)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblockOrphan->hashPrevBlock))
        pblockOrphan = mapOrphanBlocks[pblockOrphan->hashPrevBlock];
    return pblockOrphan->hashPrevBlock;
}


int generateMTRandom(unsigned int s, int range)
{
    random::mt19937 gen(s);
    random::uniform_int_distribution<> dist(0, range);
    return dist(gen);
}



// miner's coin base reward based on nHeight
int64_t GetProofOfWorkReward(int nHeight, int64_t nFees)
{
    int64_t nSubsidy = 0 * COIN;

    int nFork = GetFork(nHeight);

    if (fTestNet)
    {
        if (nHeight == 0)
            nSubsidy = 16 * COIN;
        else if (nFork < XST_FORK002)
            nSubsidy = 90000 * COIN;
    }
    else
    {
        if (nHeight == 0)
            nSubsidy = 16 * COIN; // genesis block coinbase is unspendable
        else if (nHeight <= 10)
            nSubsidy = 23300 * COIN; // Blocks 1-10 are premine
        else if (nHeight <= 260)
            nSubsidy = 16 * COIN; // 4 hr Low Reward Period for Fairness
        else if (nHeight <= 1700)
            nSubsidy = 8000 * COIN;
        else if (nHeight <= 3140)
            nSubsidy = 4000 * COIN;
        else if (nHeight <= 4580)
            nSubsidy = 2000 * COIN;
        else if (nFork < XST_FORK002)
            nSubsidy = 1000 * COIN;  // was 1 coin
    }

    return nSubsidy + nFees;
}

// miner's coin stake reward based on nBits and coin age spent (coin-days)
// simple algorithm, not depend on the diff
int64_t GetProofOfStakeReward(int64_t nCoinAge, unsigned int nBits)
{
    int64_t nRewardCoinYear;
    nRewardCoinYear = fTestNet ? MAX_STEALTH_PROOF_OF_STAKE_TESTNET : MAX_STEALTH_PROOF_OF_STAKE;
    int64_t nSubsidy = nCoinAge * nRewardCoinYear / 365;

    if (fDebug && GetBoolArg("-printcreation"))
        printf("GetProofOfStakeReward(): create=%s nCoinAge=%" PRId64 " nBits=%d\n", FormatMoney(nSubsidy).c_str(), nCoinAge, nBits);

    return nSubsidy;
}

int64_t GetQPoSReward(const CBlockIndex *pindexPrev)
{
    // qPoS 5s blocks per year (365.25 * 24 * 60 * 60/5)
    static const int64_t BPY = 6311520;
    // 1% inflation (1/100)
    static const int64_t divisor = BPY * 100;
    return pindexPrev->nMoneySupply / divisor;
}

int64_t GetStakerPrice(const QPRegistry *pregistry,
                       CBlockIndex *pindexPrev,
                       bool fPurchase)
{
    // This says
    //    1) stakers 1 to 22: 1st tier price (discount)
    //    2) stakers 23 to 86: 2nd tier price (no discount)
    //    3) stakers 87 to 214: 3rd tier price (premium)
    //    4) stakers 215 to 470: 4th tier price (unaffordable)
    static const uint32_t TIER1 = 22;
    static const uint32_t TIER2 = 64;
    static const uint32_t K_TIER = TIER2 - TIER1;
    static const int64_t K_SCALE = 4000;
    static const int64_t K_INCENTIVE = 200;
    // Excpeted fraction of the money supply increase waiting on a purchase.
    // Based on 5 second blocks, this is about 10 min meaning if you have
    // to wait 10 min for a purchase this estimate will still be enough.
    // Adds less than 0.2 XST to the price of a 50,000 XST staker.
    static const int64_t INVERSE_WAIT_INCREASE = 3153600;
    uint32_t N = static_cast<uint32_t>(pregistry->GetNumberQualified());
    int64_t blen = static_cast<int64_t>(bit_length(N + K_TIER));
    int64_t nSupply = pindexPrev->nMoneySupply;
    if (fPurchase)
    {
        nSupply += (nSupply / INVERSE_WAIT_INCREASE);
    }
    return ((nSupply / K_SCALE) * (blen - 1)) + (K_INCENTIVE * N);
}

static const int64_t nTargetTimespan = 60 * 30; // 30 blocks
static const int64_t nTargetSpacingWorkMax = 3 * nStakeTargetSpacing;

//
// maximum nBits value could possible be required nTime after
// minimum proof-of-work required was nBase
//
unsigned int ComputeMaxBits(CBigNum bnTargetLimit, unsigned int nBase, int64_t nTime)
{
    CBigNum bnResult;
    bnResult.SetCompact(nBase);
    bnResult *= 2;
    while (nTime > 0 && bnResult < bnTargetLimit)
    {
        // Maximum 200% adjustment per day/10...because block times 1/10 PPC
        bnResult *= 2;
        nTime -= 24 * 60 * 6;
    }
    if (bnResult > bnTargetLimit)
        bnResult = bnTargetLimit;
    return bnResult.GetCompact();
}

//
// minimum amount of work that could possibly be required nTime after
// minimum proof-of-work required was nBase
//
unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime)
{
    return ComputeMaxBits(bnProofOfWorkLimit, nBase, nTime);
}

//
// minimum amount of stake that could possibly be required nTime after
// minimum proof-of-stake required was nBase
//
unsigned int ComputeMinStake(unsigned int nBase, int64_t nTime, unsigned int nBlockTime)
{
    return ComputeMaxBits(bnProofOfStakeLimit, nBase, nTime);
}


// ppcoin: find last block index up to pindex
const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake)
{
    while (pindex && pindex->pprev && (pindex->IsProofOfStake() != fProofOfStake))
        pindex = pindex->pprev;
    return pindex;
}

unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, bool fProofOfStake)
{
    CBigNum bnTargetLimit = bnProofOfWorkLimit;

    if(fProofOfStake)
    {
        // Proof-of-Stake blocks has own target limit since nVersion=3 supermajority on mainNet and always on testNet
        bnTargetLimit = bnProofOfStakeLimit;
    }

    if (pindexLast == NULL)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(pindexPrev->pprev, fProofOfStake);
    if (pindexPrevPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();
    if(nActualSpacing < 0)
    {
        // printf(">> nActualSpacing = %" PRI64d " corrected to 1.\n", nActualSpacing);
        nActualSpacing = 1;
    }
    else if(nActualSpacing > nTargetTimespan)
    {
        // printf(">> nActualSpacing = %" PRI64d " corrected to nTargetTimespan (900).\n", nActualSpacing);
        nActualSpacing = nTargetTimespan;
    }

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev->nBits);

    int64_t nTargetSpacing = fProofOfStake? nStakeTargetSpacing : min(nTargetSpacingWorkMax, (int64_t) nStakeTargetSpacing * (1 + pindexLast->nHeight - pindexPrev->nHeight));
    int64_t nInterval = nTargetTimespan / nTargetSpacing;
    bnNew *= ((nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTargetSpacing);

    if (bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // Check range
    if (bnTarget <= 0 || bnTarget > bnProofOfWorkLimit)
        return error("CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");

    return true;
}

// Return maximum amount of blocks that other nodes claim to have
int GetNumBlocksOfPeers()
{
    return std::max(cPeerBlockCounts.median(), Checkpoints::GetTotalBlocksEstimate());
}

bool IsInitialBlockDownload()
{
    if (pindexBest == NULL || nBestHeight < Checkpoints::GetTotalBlocksEstimate())
        return true;
    static int64_t nLastUpdate;
    static CBlockIndex* pindexLastBest;
    if (pindexBest != pindexLastBest)
    {
        pindexLastBest = pindexBest;
        nLastUpdate = GetTime();
    }
    return (GetTime() - nLastUpdate < 10 &&
            pindexBest->GetBlockTime() < GetTime() - 24 * 60 * 60);
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (pindexNew->bnChainTrust > bnBestInvalidTrust)
    {
        bnBestInvalidTrust = pindexNew->bnChainTrust;
        CTxDB().WriteBestInvalidTrust(bnBestInvalidTrust);
        uiInterface.NotifyBlocksChanged();
    }

    printf("InvalidChainFound: invalid block=%s  height=%d  trust=%s  date=%s\n",
      pindexNew->GetBlockHash().ToString().c_str(), pindexNew->nHeight,
      pindexNew->bnChainTrust.ToString().c_str(), DateTimeStrFormat("%x %H:%M:%S",
      pindexNew->GetBlockTime()).c_str());
    printf("InvalidChainFound:  current best=%s  height=%d  trust=%s  date=%s\n",
      hashBestChain.ToString().c_str(), nBestHeight, bnBestChainTrust.ToString().c_str(),
      DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str());
}

void CBlock::UpdateTime(const CBlockIndex* pindexPrev)
{
    nTime = max(GetBlockTime(), GetAdjustedTime());
}


bool CTransaction::DisconnectInputs(CTxDB& txdb)
{
    // Relinquish previous transactions' spent pointers
    if (!IsCoinBase())
    {
        BOOST_FOREACH(const CTxIn& txin, vin)
        {
            COutPoint prevout = txin.prevout;

            // Get prev txindex from disk
            CTxIndex txindex;
            if (!txdb.ReadTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : ReadTxIndex failed");

            if (prevout.n >= txindex.vSpent.size())
                return error("DisconnectInputs() : prevout.n out of range");

            // Mark outpoint as not spent
            txindex.vSpent[prevout.n].SetNull();

            // Write back
            if (!txdb.UpdateTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : UpdateTxIndex failed");
        }
    }

    // Remove transaction from index
    // This can fail if a duplicate of this transaction was in a chain that got
    // reorganized away. This is only possible if this transaction was completely
    // spent, so erasing it would be a no-op anyway.
    txdb.EraseTxIndex(*this);

    return true;
}


bool CTransaction::FetchInputs(CTxDB& txdb, const map<uint256, CTxIndex>& mapTestPool,
                               bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid) const
{
    // FetchInputs can return false either because we just haven't seen some inputs
    // (in which case the transaction should be stored as an orphan)
    // or because the transaction is malformed (in which case the transaction should
    // be dropped).  If tx is definitely invalid, fInvalid will be set to true.
    fInvalid = false;

    if (IsCoinBase())
    {
        return true; // Coinbase transactions have no inputs to fetch.
    }

    for (unsigned int i = 0; i < vin.size(); i++)
    {
        COutPoint prevout = vin[i].prevout;
        if (inputsRet.count(prevout.hash))
        {
            continue; // Got it already
        }

        // Read txindex
        CTxIndex& txindex = inputsRet[prevout.hash].first;
        bool fFound = true;
        if ((fBlock || fMiner) && mapTestPool.count(prevout.hash))
        {
            // Get txindex from current proposed changes
            txindex = mapTestPool.find(prevout.hash)->second;
        }
        else
        {
            // Read txindex from txdb
            fFound = txdb.ReadTxIndex(prevout.hash, txindex);
        }
        if (!fFound && (fBlock || fMiner))
        {
            return fMiner ? false :
                            error("FetchInputs() : %s prev tx %s index entry not found",
                                  GetHash().ToString().c_str(),
                                  prevout.hash.ToString().c_str());
        }

        // Read txPrev
        CTransaction& txPrev = inputsRet[prevout.hash].second;
        if (!fFound || txindex.pos == CDiskTxPos(1,1,1))
        {
            // Get prev tx from single transactions in memory
            {
                LOCK(mempool.cs);
                if (!mempool.lookup(prevout.hash, txPrev))
                    return error("FetchInputs() : %s mempool Tx prev not found %s",
                                 GetHash().ToString().c_str(),
                                 prevout.hash.ToString().c_str());
                // txPrev = mempool.lookup(prevout.hash);
            }
            if (!fFound)
            {
                txindex.vSpent.resize(txPrev.vout.size());
            }
        }
        else
        {
            // Get prev tx from disk
            if (!txPrev.ReadFromDisk(txindex.pos))
                return error("FetchInputs() : %s ReadFromDisk prev tx %s failed", GetHash().ToString().c_str(),  prevout.hash.ToString().c_str());
        }
    }

    // Make sure all prevout.n indexes are valid:
    for (unsigned int i = 0; i < vin.size(); i++)
    {
        const COutPoint prevout = vin[i].prevout;
        assert(inputsRet.count(prevout.hash) != 0);
        const CTxIndex& txindex = inputsRet[prevout.hash].first;
        const CTransaction& txPrev = inputsRet[prevout.hash].second;
        if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
        {
            // Revisit this if/when transaction replacement is implemented and allows
            // adding inputs:
            fInvalid = true;
            return DoS(100,
                       error("FetchInputs() : tx %s prevout.n %d out of range, "
                             "prev vout size: %" PRIszu ", spent size: "
                             "%" PRIszu ", prev tx %s (%s)",
                             GetHash().ToString().c_str(), prevout.n,
                             txPrev.vout.size(), txindex.vSpent.size(),
                             prevout.hash.ToString().c_str(),
                             txPrev.GetHash().ToString().c_str()));
        }
    }

    return true;
}


const CTxOut& CTransaction::GetOutputFor(const CTxIn& input, const MapPrevTx& inputs) const
{
    MapPrevTx::const_iterator mi = inputs.find(input.prevout.hash);
    if (mi == inputs.end())
        throw std::runtime_error("CTransaction::GetOutputFor() : prevout.hash not found");

    const CTransaction& txPrev = (mi->second).second;
    if (input.prevout.n >= txPrev.vout.size())
        throw std::runtime_error("CTransaction::GetOutputFor() : prevout.n out of range");

    return txPrev.vout[input.prevout.n];
}


// asdf claim could be determined to be 0, so need to use -1 for unchecked
// asdf    this will save some cpu cycles
int64_t CTransaction::GetValueIn(const MapPrevTx& inputs, int64_t nClaim) const
{
    if (IsCoinBase())
    {
        return 0;
    }

    if (nClaim == 0)
    {
        qpos_claim claim;
        CheckClaim(pregistryMain, inputs, claim);
        nClaim = claim.value;
    }

    int64_t nResult = nClaim;
    for (unsigned int i = 0; i < vin.size(); i++)
    {
        nResult += GetOutputFor(vin[i], inputs).nValue;
    }
    return nResult;
}


unsigned int CTransaction::GetP2SHSigOpCount(const MapPrevTx& inputs) const
{
    if (IsCoinBase())
    {
        return 0;
    }

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < vin.size(); i++)
    {
        const CTxOut& prevout = GetOutputFor(vin[i], inputs);
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(vin[i].scriptSig);
    }
    return nSigOps;
}


bool CTransaction::ConnectInputs(CTxDB& txdb, MapPrevTx inputs,
                                 map<uint256, CTxIndex>& mapTestPool,
                                 const CDiskTxPos& posThisTx,
                                 const CBlockIndex* pindexBlock,
                                 bool fBlock, bool fMiner,
                                 unsigned int flags,
                                 int64_t nValuePurchases, int64_t nClaim)
{
    // Take over previous transactions' spent pointers
    // fBlock is true when this is called from AcceptBlock when a new best-block is added to the blockchain
    // fMiner is true when called from the internal bitcoin miner
    // ... both are false when called from CTransaction::AcceptToMemoryPool
    unsigned int nTxTime = HasTimestamp() ? GetTxTime() : pindexBlock->nTime;
    if (!IsCoinBase())
    {
        int64_t nValueIn = 0;
        int64_t nFees = 0;
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            COutPoint prevout = vin[i].prevout;
            assert(inputs.count(prevout.hash) > 0);
            CTxIndex& txindex = inputs[prevout.hash].first;
            CTransaction& txPrev = inputs[prevout.hash].second;

            if (prevout.n >= txPrev.vout.size() ||
                prevout.n >= txindex.vSpent.size())
            {
                return DoS(100, error("ConnectInputs() : %s prevout.n out of range %d %"
                                          PRIszu " %" PRIszu " prev tx %s\n%s",
                                      GetHash().ToString().c_str(), prevout.n,
                                      txPrev.vout.size(), txindex.vSpent.size(),
                                      prevout.hash.ToString().c_str(),
                                      txPrev.ToString().c_str()));
            }

            // If prev is coinbase or coinstake, check that it's matured
            if (txPrev.IsCoinBase() || txPrev.IsCoinStake())
            {
                for (const CBlockIndex* pindex = pindexBlock;
                     pindex && pindexBlock->nHeight - pindex->nHeight < nCoinbaseMaturity;
                     pindex = pindex->pprev)
                {
                    if (pindex->nBlockPos == txindex.pos.nBlockPos && pindex->nFile == txindex.pos.nFile)
                    {
                        return error("ConnectInputs() : tried to spend %s at depth %d",
                                     txPrev.IsCoinBase() ? "coinbase" : "coinstake",
                                     pindexBlock->nHeight - pindex->nHeight);
                    }
                }
            }

            // check is meaningless if one or the other has no timestamp
            if (txPrev.HasTimestamp() && HasTimestamp())
            {
                // ppcoin: check transaction timestamp
                if (txPrev.GetTxTime() > nTxTime)
                {
                        return DoS(100, error("ConnectInputs() : transaction timestamp earlier than input transaction"));
                }
            }

            if (txPrev.vout[prevout.n].IsEmpty()  && (GetFork(nBestHeight + 1) >= XST_FORK005))
            {
                return DoS(1, error("ConnectInputs() : special marker is not spendable"));
            }

            // Check for negative or overflow input values
            nValueIn += txPrev.vout[prevout.n].nValue;
            if (!MoneyRange(txPrev.vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return DoS(100, error("ConnectInputs() : txin values out of range"));

        }


        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            COutPoint prevout = vin[i].prevout;
            assert(inputs.count(prevout.hash) > 0);
            CTxIndex& txindex = inputs[prevout.hash].first;
            CTransaction& txPrev = inputs[prevout.hash].second;

            // Check for conflicts (double-spend)
            // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
            // for an attacker to attempt to split the network.
            if (!txindex.vSpent[prevout.n].IsNull())
            {
                return fMiner ? false :
                                error("ConnectInputs() : %s prev tx already used at %s",
                                      GetHash().ToString().c_str(),
                                      txindex.vSpent[prevout.n].ToString().c_str());
            }

            // Skip ECDSA signature verification when connecting blocks (fBlock=true)
            // before the last blockchain checkpoint. This is safe because block merkle hashes are
            // still computed and checked, and any change will be caught at the next checkpoint.
            if (!(fBlock && (nBestHeight < Checkpoints::GetTotalBlocksEstimate())))
            {
                // Verify signature
                if (!VerifySignature(txPrev, *this, i, flags, 0))
                {
                    return DoS(100,error("ConnectInputs() : %s VerifySignature failed",
                               GetHash().ToString().c_str()));
                }
            }

            // Mark outpoints as spent
            txindex.vSpent[prevout.n] = posThisTx;

            // Write back
            if (fBlock || fMiner)
            {
                mapTestPool[prevout.hash] = txindex;
            }
        }

        if (IsCoinStake())
        {
            // ppcoin: coin stake tx earns reward instead of paying fee
            uint64_t nCoinAge;
            if (!GetCoinAge(txdb, pindexBlock->nTime, nCoinAge))
            {
                return error("ConnectInputs() : %s unable to get coin age for coinstake",
                             GetHash().ToString().c_str());
            }
            int64_t nStakeReward = GetValueOut() - nValueIn;
            if (nStakeReward > (GetProofOfStakeReward(nCoinAge,
                                                     pindexBlock->nBits) -
                                GetMinFee() + MIN_TX_FEE))
            {
                return DoS(100, error("ConnectInputs() : %s stake reward exceeded",
                                      GetHash().ToString().c_str()));
            }
        }
        else
        {
            int64_t nTxCredit = nValueIn + nClaim;
            int64_t nTxDebit = GetValueOut() + nValuePurchases;
            if (nTxCredit < nTxDebit)
            {
                return DoS(100, error("ConnectInputs() : %s value in < value out",
                                      GetHash().ToString().c_str()));
            }

            // Tally transaction fees
            int64_t nTxFee = nTxCredit - nTxDebit;
            if (nTxFee < 0)
            {
                return DoS(100, error("ConnectInputs() : %s nTxFee < 0",
                                      GetHash().ToString().c_str()));
            }
            // ppcoin: enforce transaction fees for every block
            if (nTxFee < GetMinFee())
            {
                return fBlock? DoS(100, error("ConnectInputs() : %s not paying required fee=%s, paid=%s",
                                              GetHash().ToString().c_str(),
                                              FormatMoney(GetMinFee()).c_str(),
                                              FormatMoney(nTxFee).c_str())) : false;
            }

            nFees += nTxFee;
            if (!MoneyRange(nFees))
            {
                return DoS(100, error("ConnectInputs() : nFees out of range"));
            }
        }
    }

    return true;
}

bool CTransaction::ClientConnectInputs()
{
    if (IsCoinBase())
    {
        return false;
    }

    // Take over previous transactions' spent pointers
    {
        LOCK(mempool.cs);
        int64_t nValueIn = 0;
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            // Get prev tx from single transactions in memory
            COutPoint prevout = vin[i].prevout;
            if (!mempool.exists(prevout.hash))
                return false;
            CTransaction& txPrev = mempool.lookup(prevout.hash);

            if (prevout.n >= txPrev.vout.size())
                return false;

            unsigned int flags = STANDARD_SCRIPT_VERIFY_FLAGS;
            if (GetFork(nBestHeight + 1) < XST_FORK005)
            {
                 flags = flags & ~SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
            }

            // Verify signature
            if (!VerifySignature(txPrev, *this, i, flags, 0))
                return error("ClientConnectInputs() : VerifySignature failed");

            nValueIn += txPrev.vout[prevout.n].nValue;

            if (!MoneyRange(txPrev.vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return error("ClientConnectInputs() : txin values out of range");
        }
        if (GetValueOut() > nValueIn)
            return false;
    }

    return true;
}


bool CBlock::DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex)
{
    // Disconnect in reverse order
    for (int i = vtx.size()-1; i >= 0; i--)
        if (!vtx[i].DisconnectInputs(txdb))
            return false;

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = 0;
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("DisconnectBlock() : WriteBlockIndex failed");
    }

    // ppcoin: clean up wallet after disconnecting coinstake
    BOOST_FOREACH(CTransaction& tx, vtx)
        SyncWithWallets(tx, this, false, false);

    return true;
}

bool CBlock::ConnectBlock(CTxDB& txdb, CBlockIndex* pindex, bool fJustCheck)
{
    QPRegistry *pregistryTemp = new QPRegistry(pregistryMain);
    vector<QPoSTxDetails> vDeets;
    // Check it again in case a previous version let a bad block in
    // fCheckSig was always true here by default
    if (!CheckBlock(pregistryTemp, vDeets, pindex->pprev,
                    !fJustCheck, !fJustCheck, true, !fJustCheck))
    {
        return false;
    }
    delete pregistryTemp;
    pregistryTemp = NULL;
    unsigned int flags = SCRIPT_VERIFY_NOCACHE | STANDARD_SCRIPT_VERIFY_FLAGS;
    unsigned int nFork = GetFork(nBestHeight + 1);
    if (nFork < XST_FORK005)
    {
        flags = flags & ~SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    }

    // Do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    // If such overwrites are allowed, coinbases and transactions depending upon those
    // can be duplicated to remove the ability to spend the first instance -- even after
    // being sent to another address.
    // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
    // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
    // already refuses previously-known transaction ids entirely.
    // This rule was originally applied all blocks whose timestamp was after March 15, 2012, 0:00 UTC.
    // Now that the whole chain is irreversibly beyond that time it is applied to all blocks except the
    // two in the chain that violate it. This prevents exploiting the issue against nodes in their
    // initial block download.
    bool fEnforceBIP30 = true; // Always active in Stealth
    bool fStrictPayToScriptHash = true; // Always active in Stealth

    //// issue here: it doesn't know the version
    unsigned int nTxPos;
    if (fJustCheck)
    {
        // FetchInputs treats CDiskTxPos(1,1,1) as a special "refer to memorypool" indicator
        // Since we're just checking the block and not actually connecting it, it might not (and probably shouldn't) be on the disk to get the transaction from
        nTxPos = 1;
    }
    else
    {
        CBlock blockTemp;
        blockTemp.nVersion = pindex->nVersion;
        nTxPos = pindex->nBlockPos +
                 ::GetSerializeSize(blockTemp, SER_DISK, CLIENT_VERSION) -
                 (2 * GetSizeOfCompactSize(0)) +
                 GetSizeOfCompactSize(vtx.size());
    }

    map<uint256, CTxIndex> mapQueuedChanges;
    int64_t nFees = 0;
    int64_t nValueIn = 0;
    int64_t nValueOut = 0;
    int64_t nValuePurchases = 0;
    int64_t nValueClaims = 0;
    unsigned int nSigOps = 0;
    BOOST_FOREACH(CTransaction& tx, vtx)
    {
        uint256 hashTx = tx.GetHash();

        if (fEnforceBIP30) {
            CTxIndex txindexOld;
            if (txdb.ReadTxIndex(hashTx, txindexOld)) {
                BOOST_FOREACH(CDiskTxPos &pos, txindexOld.vSpent)
                    if (pos.IsNull())
                        return false;
            }
        }

        nSigOps += tx.GetLegacySigOpCount();
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return DoS(100, error("ConnectBlock() : too many sigops"));

        CDiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
        if (!fJustCheck)
            nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

        MapPrevTx mapInputs;
        if (tx.IsCoinBase())
        {
            nValueOut += tx.GetValueOut();
        }
        else
        {
            bool fInvalid;
            if (!tx.FetchInputs(txdb, mapQueuedChanges, true, false, mapInputs, fInvalid))
                return false;

            if (fStrictPayToScriptHash)
            {
                // Add in sigops done by pay-to-script-hash inputs;
                // this is to prevent a "rogue miner" from creating
                // an incredibly-expensive-to-validate block.
                nSigOps += tx.GetP2SHSigOpCount(mapInputs);
                if (nSigOps > MAX_BLOCK_SIGOPS)
                    return DoS(100, error("ConnectBlock() : too many sigops"));
            }

            qpos_claim claim;
            claim.value = 0;
            if (!tx.IsCoinStake())
            {
                if (!tx.CheckClaim(pregistryMain, mapInputs, claim))
                {
                    return DoS(100, error("ConnectBlock() : bad claim"));
                }

                if (claim.value != 0)
                {
                    nValueClaims += claim.value;
                }
            }

            int64_t nTxValuePurchases = 0;
            int64_t nTxValueIn = tx.GetValueIn(mapInputs, claim.value);
            int64_t nTxValueOut = tx.GetValueOut();
            nValueIn += nTxValueIn;
            nValueOut += nTxValueOut;
            if (!tx.IsCoinStake())
            {
                int64_t nStakerPrice = GetStakerPrice(pregistryMain,
                                                      pindex->pprev);
                map<string, qpos_purchase> mapPurchases;
                if (!tx.CheckPurchases(pregistryMain,
                                       nStakerPrice,
                                       mapPurchases))
                {
                    return DoS(100, error("ConnectBlock() : bad purchase"));
                }
                map<string, qpos_purchase>::const_iterator it;
                for (it = mapPurchases.begin(); it != mapPurchases.end(); ++it)
                {
                    nTxValuePurchases += it->second.value;
                }

                nValuePurchases += nTxValuePurchases;
                nFees += nTxValueIn - (nTxValueOut + nTxValuePurchases);
            }

            if (!tx.ConnectInputs(txdb, mapInputs, mapQueuedChanges,
                                  posThisTx, pindex, true, false,
                                  flags, nTxValuePurchases, claim.value))
            {
                return false;
            }
        }

        mapQueuedChanges[hashTx] = CTxIndex(posThisTx, tx.vout.size());
    }


    // Peercoin: track money supply and mint amount info
    // XST: The following calculation is inherited from Peercoin
    //      but it is incorrect for XST PoW blocks, of which there were
    //      only a couple thousand. Including fees in this calculation
    //      was incorrect for XST PoW because XST PoW miners were awarded
    //      fees, unlike Peercoin miners. This has a negligible
    //      impact on the mint and money supply calculations, but should
    //      be fixed with a money supply adjustment upon FORKASDF.
    // mint & supply: claims are included in nValueIn to make accounting easier
    //                elsewhere, so they must be subtracted from nValueIn here
    pindex->nMint = nValueOut + nValuePurchases + nFees - (nValueIn - nValueClaims);
    // supply: purchases are not reflected in nValueIn, so must be added to it
    pindex->nMoneySupply = (pindex->pprev? pindex->pprev->nMoneySupply : 0) +
                               nValueOut - (nValuePurchases + nValueIn - nValueClaims);

    pindex->vDeets = vDeets;

    if (!txdb.WriteBlockIndex(CDiskBlockIndex(pindex)))
    {
        return error("Connect() : WriteBlockIndex for pindex failed");
    }

    // ppcoin: fees are not collected by miners as in bitcoin
    // ppcoin: fees are destroyed to compensate the entire network
    if (fDebug && GetBoolArg("-printcreation"))
    {
        printf("ConnectBlock() : destroy=%s nFees=%" PRId64 "\n",
               FormatMoney(nFees).c_str(), nFees);
    }

    if (fJustCheck)
    {
        return true;
    }

    // Write queued txindex changes
    for (map<uint256, CTxIndex>::iterator mi = mapQueuedChanges.begin(); mi != mapQueuedChanges.end(); ++mi)
    {
        if (!txdb.UpdateTxIndex((*mi).first, (*mi).second))
            return error("ConnectBlock() : UpdateTxIndex failed");
    }

    uint256 prevHash = 0;
    if(pindex->pprev)
    {
         prevHash = pindex->pprev->GetBlockHash();
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = pindex->GetBlockHash();
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("ConnectBlock() : WriteBlockIndex failed");
    }

    // Watch for transactions paying to me
    BOOST_FOREACH(CTransaction& tx, vtx)
        SyncWithWallets(tx, this, true);

    return true;
}

bool static Reorganize(CTxDB& txdb,
                       CBlockIndex* pindexNew,
                       CBlockIndex* &pindexReplayRet)
{
    printf("REORGANIZE\n");

    // Find the fork
    CBlockIndex* pfork = pindexBest;
    CBlockIndex* plonger = pindexNew;
    while (pfork != plonger)
    {
        while (plonger->nHeight > pfork->nHeight)
            if (!(plonger = plonger->pprev))
                return error("Reorganize() : plonger->pprev is null");
        if (pfork == plonger)
            break;
        if (!(pfork = pfork->pprev))
            return error("Reorganize() : pfork->pprev is null");
    }

    // List of what to disconnect
    vector<CBlockIndex*> vDisconnect;
    for (CBlockIndex* pindex = pindexBest; pindex != pfork; pindex = pindex->pprev)
        vDisconnect.push_back(pindex);

    // List of what to connect
    vector<CBlockIndex*> vConnect;
    for (CBlockIndex* pindex = pindexNew; pindex != pfork; pindex = pindex->pprev)
        vConnect.push_back(pindex);
    reverse(vConnect.begin(), vConnect.end());

    printf("REORGANIZE: Disconnect %" PRIszu " blocks; %s..%s\n", vDisconnect.size(), pfork->GetBlockHash().ToString().c_str(), pindexBest->GetBlockHash().ToString().c_str());
    printf("REORGANIZE: Connect %" PRIszu " blocks; %s..%s\n", vConnect.size(), pfork->GetBlockHash().ToString().c_str(), pindexNew->GetBlockHash().ToString().c_str());

    // Disconnect shorter branch
    list<CTransaction> vResurrect;
    BOOST_FOREACH(CBlockIndex* pindex, vDisconnect)
    {
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            return error("Reorganize() : ReadFromDisk for disconnect failed");
        if (!block.DisconnectBlock(txdb, pindex))
            return error("Reorganize() : DisconnectBlock %s failed", pindex->GetBlockHash().ToString().c_str());

        // Queue memory transactions to resurrect
        BOOST_REVERSE_FOREACH(const CTransaction& tx, block.vtx)
        {
            if (!(tx.IsCoinBase() || tx.IsCoinStake()) &&
                pindex->nHeight > Checkpoints::GetTotalBlocksEstimate())
            {
                vResurrect.push_front(tx);
            }
        }

    }

    // registry must replay from at least the fork
    pindexReplayRet = pfork;

    // Connect longer branch
    vector<CTransaction> vDelete;
    for (unsigned int i = 0; i < vConnect.size(); i++)
    {
        CBlockIndex* pindex = vConnect[i];
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            return error("Reorganize() : ReadFromDisk for connect failed");
        if (!block.ConnectBlock(txdb, pindex))
        {
            // Invalid block
            return error("Reorganize() : ConnectBlock %s failed", pindex->GetBlockHash().ToString().c_str());
        }

        // Queue memory transactions to delete
        BOOST_FOREACH(const CTransaction& tx, block.vtx)
            vDelete.push_back(tx);
    }
    if (!txdb.WriteHashBestChain(pindexNew->GetBlockHash()))
        return error("Reorganize() : WriteHashBestChain failed");

    // Make sure it's successfully written to disk before changing memory structure
    if (!txdb.TxnCommit())
        return error("Reorganize() : TxnCommit failed");

    // Disconnect shorter branch
    BOOST_FOREACH(CBlockIndex* pindex, vDisconnect)
        if (pindex->pprev)
            pindex->pprev->pnext = NULL;

    // Connect longer branch
    BOOST_FOREACH(CBlockIndex* pindex, vConnect)
        if (pindex->pprev)
            pindex->pprev->pnext = pindex;

    // Resurrect memory transactions that were in the disconnected branch
    BOOST_FOREACH(CTransaction& tx, vResurrect)
        tx.AcceptToMemoryPool(txdb, false);

    // Delete redundant memory transactions that are in the connected branch
    BOOST_FOREACH(CTransaction& tx, vDelete)
        mempool.remove(tx);

    printf("REORGANIZE: done\n");

    return true;
}

// Called from inside SetBestChain: attaches a block to the new best chain being built
bool CBlock::SetBestChainInner(CTxDB& txdb, CBlockIndex *pindexNew)
{
    uint256 hash = GetHash();

    // Adding to current best branch
    if (!ConnectBlock(txdb, pindexNew) || !txdb.WriteHashBestChain(hash))
    {
        txdb.TxnAbort();
        InvalidChainFound(pindexNew);
        return false;
    }
    if (!txdb.TxnCommit())
        return error("SetBestChainInner() : TxnCommit failed");

    // Add to current best branch
    pindexNew->pprev->pnext = pindexNew;

    // Delete redundant memory transactions
    BOOST_FOREACH(CTransaction& tx, vtx)
        mempool.remove(tx);

    return true;
}

bool CBlock::SetBestChain(CTxDB& txdb, CBlockIndex* pindexNew)
{
    uint256 hash = GetHash();

    if (!txdb.TxnBegin())
        return error("SetBestChain() : TxnBegin failed");

    if ((pindexGenesisBlock == NULL) &&
        (hash == (fTestNet ? hashGenesisBlockTestNet : hashGenesisBlock)))
    {
        txdb.WriteHashBestChain(hash);
        if (!txdb.TxnCommit())
            return error("SetBestChain() : TxnCommit failed");
        pindexGenesisBlock = pindexNew;
    }
    else if (hashPrevBlock == hashBestChain)
    {
        if (!SetBestChainInner(txdb, pindexNew))
            return error("SetBestChain() : SetBestChainInner failed");
    }
    else
    {
       /**********************************************************************
        * REORGANIZE
        **********************************************************************/
        // the first block in the new chain that will cause it to become the new best chain
        CBlockIndex *pindexIntermediate = pindexNew;

        // list of blocks that need to be connected afterwards
        std::vector<CBlockIndex*> vpindexSecondary;

        // Reorganize is costly in terms of db load, as it works in a single db transaction.
        // Try to limit how much needs to be done inside
        while (pindexIntermediate->pprev &&
               (pindexIntermediate->pprev->bnChainTrust > pindexBest->bnChainTrust))
        {
            vpindexSecondary.push_back(pindexIntermediate);
            pindexIntermediate = pindexIntermediate->pprev;
        }

        if (!vpindexSecondary.empty())
        {
            printf("Postponing %" PRIszu " reconnects\n", vpindexSecondary.size());
        }

        // Switch to new best branch
        // pindexReplay is the oldest block needed for replay
        CBlockIndex *pindexReplay = pindexIntermediate;
        if (!Reorganize(txdb, pindexIntermediate, pindexReplay))
        {
            txdb.TxnAbort();
            InvalidChainFound(pindexNew);
            return error("SetBestChain() : Reorganize failed");
        }

        QPRegistry *pregistryTemp = new QPRegistry();
        GetRegistrySnapshot(txdb, pindexReplay->nHeight, pregistryTemp);

        // 1. load snapshot of pStakerRegistry preceding pindexReplay->nHeight
        // 2. make new pindex = pindexReplay
        // 3. roll this back to the snapshotblock+1
        // 4. replay registry from snapshot+1 to vpindexSecondary.back()-1 (earliest)

        if (vpindexSecondary.empty())
        {
            uint256 blockHash = pregistryTemp->GetBlockHash();
            printf("asdf secondary empty replay registry from %s\n", blockHash.ToString().c_str());
            CBlockIndex *pindexCurrent = mapBlockIndex[blockHash];
            while (pindexCurrent->pnext != NULL)
            {
                pindexCurrent = pindexCurrent->pnext;
                printf("asdf 6.0.6.0 not null pindexcurrent nHeight is %d\n", pindexCurrent->nHeight);
                pregistryTemp->UpdateOnNewBlock(pindexCurrent, true);
                if ((pindexCurrent->pnext != NULL) && (!pindexCurrent->pnext->IsInMainChain()))
                {
                   printf("asdf 6.0.6.1 pindexcurrent->next not in main chain\n");
                   break;
                }
            }
        }
        else
        {
            uint256 blockHash = pregistryTemp->GetBlockHash();
            printf("asdf secondary not empty replay registry from %s\n", blockHash.ToString().c_str());
            CBlockIndex *pindexCurrent = mapBlockIndex[blockHash];
            // note vpindexSecondary is descending block height
            while (pindexCurrent != vpindexSecondary.back()->pprev)
            {
                printf("asdf 6.0.5.0 pindexcurrent nHeight is %d\n", pindexCurrent->nHeight);
                if (pindexCurrent->pnext == NULL)
                {
                printf("asdf 6.0.6.0 is null pindexcurrent nHeight is %d\n", pindexCurrent->nHeight);
                    break;
                }
                pindexCurrent = pindexCurrent->pnext;
                pregistryTemp->UpdateOnNewBlock(pindexCurrent, true);
            }
            if (pindexCurrent != vpindexSecondary.back()->pprev)
            {
                return error("SetBestChain() : block index not found for replay");
            }
        }

        // Connect further blocks
        BOOST_REVERSE_FOREACH(CBlockIndex *pindex, vpindexSecondary)
        {
            CBlock block;
            if (!block.ReadFromDisk(pindex))
            {
                printf("SetBestChain() : ReadFromDisk failed\n");
                break;
            }
            if (!txdb.TxnBegin())
            {
                printf("SetBestChain() : TxnBegin 2 failed\n");
                break;
            }
            // errors now are not fatal, we still did a reorganisation to a new chain in a valid way
            if (!block.SetBestChainInner(txdb, pindex))
            {
                break;
            }
                printf("asdf 6.0.1.0 updating pindex nHeight is %d\n", pindex->nHeight);
            pregistryTemp->UpdateOnNewBlock(pindex, true);
        }

        // copy rather than assign to retain mutexes, etc.
                printf("asdf 6.0.1.0 copying registry\n");
        pregistryMain->Copy(pregistryTemp);
        delete pregistryTemp;
        pregistryTemp = NULL;
    }

    // Update best block in wallet (so we can detect restored wallets)
    bool fIsInitialDownload = IsInitialBlockDownload();
    if (!fIsInitialDownload)
    {
        const CBlockLocator locator(pindexNew);
        ::SetBestChain(locator);
    }

    // New best block
    hashBestChain = hash;
    pindexBest = pindexNew;
    pblockindexFBBHLast = NULL;
    nBestHeight = pindexBest->nHeight;
    bnBestChainTrust = pindexNew->bnChainTrust;
    nTimeBestReceived = GetTime();
    nTransactionsUpdated++;

    printf("SetBestChain: new best=%s\n"
              "    height=%d  staker=%s  trust=%s  time=%" PRIu64 " (%s)\n",
           hashBestChain.ToString().c_str(),
           nBestHeight,
           pregistryMain->GetAliasForID(nStakerID).c_str(),
           bnBestChainTrust.ToString().c_str(),
           pindexBest->GetBlockTime(),
           DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str());

    printf("Stake checkpoint: %x\n", pindexBest->nStakeModifierChecksum);

    // Check the version of the last 100 blocks to see if we need to upgrade:
    if (!fIsInitialDownload)
    {
        int nUpgraded = 0;
        const CBlockIndex* pindex = pindexBest;
        for (int i = 0; i < 100 && pindex != NULL; i++)
        {
            if (pindex->nVersion > CBlock::CURRENT_VERSION)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            printf("SetBestChain: %d of last 100 blocks above version %d\n", nUpgraded, CBlock::CURRENT_VERSION);
        if (nUpgraded > 100/2)
            // strMiscWarning is read by GetWarnings(), called by Qt and the JSON-RPC code to warn the user:
            strMiscWarning = _("Warning: This version is obsolete, upgrade required!");
    }

    std::string strCmd = GetArg("-blocknotify", "");

    if (!fIsInitialDownload && !strCmd.empty())
    {
        boost::replace_all(strCmd, "%s", hashBestChain.GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }

    return true;
}

// ppcoin: total coin age spent in transaction, in the unit of coin-days.
// Only those coins meeting minimum age requirement counts. As those
// transactions not in main chain are not currently indexed so we
// might not find out about their coin age. Older transactions are
// guaranteed to be in main chain by sync-checkpoint. This rule is
// introduced to help nodes establish a consistent view of the coin
// age (trust score) of competing branches.
bool CTransaction::GetCoinAge(CTxDB& txdb, unsigned int nBlockTime, uint64_t& nCoinAge) const
{
    CBigNum bnCentSecond = 0;  // coin age in the unit of cent-seconds
    nCoinAge = 0;

    CBigNum bnSeconds ; // age of coins (not coin age)

    if (IsCoinBase())
    {
        return true;
    }

    unsigned int nTxTime = HasTimestamp() ? GetTxTime() : nBlockTime;

    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        // First try finding the previous transaction in database
        CTransaction txPrev;
        CTxIndex txindex;
        if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex))
        {
            continue;  // previous transaction not in main chain
        }
        // Read block header
        CBlock block;
        if (!block.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
        {
            return false; // unable to read block of previous transaction
        }
        unsigned int nBlockTime = block.GetBlockTime();
        unsigned int nTxPrevTime = txPrev.HasTimestamp() ?
                                            txPrev.GetTxTime() : nBlockTime;
        if (nTxTime < nTxPrevTime)
        {
            return false;  // Transaction timestamp violation
        }

        if (nBlockTime + nStakeMinAge > nTxTime)
            continue; // only count coins meeting min age requirement

        int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
        bnSeconds = min(CBigNum(nTxTime - nTxPrevTime), MAX_COIN_SECONDS);
        bnCentSecond += CBigNum(nValueIn) * bnSeconds / CENT;

        if (fDebug && GetBoolArg("-printcoinage"))
            printf("coin age nValueIn=%" PRId64 " nTimeDiff=%d bnCentSecond=%s\n",
                    nValueIn, nTxTime - nTxPrevTime, bnCentSecond.ToString().c_str());
    }


    CBigNum bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    if (fDebug && GetBoolArg("-printcoinage"))
        printf("coin age bnCoinDay=%s\n", bnCoinDay.ToString().c_str());
    nCoinAge = bnCoinDay.getuint64();
    return true;
}

// ppcoin: total coin age spent in block, in the unit of coin-days.
bool CBlock::GetCoinAge(uint64_t& nCoinAge) const
{
    nCoinAge = 0;

    CTxDB txdb("r");
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        uint64_t nTxCoinAge;
        if (tx.GetCoinAge(txdb, nTime, nTxCoinAge))
        {
            nCoinAge += nTxCoinAge;
        }
        else
        {
            return false;
        }
    }

    if (nCoinAge == 0) // block coin age minimum 1 coin-day
        nCoinAge = 1;
    if (fDebug && GetBoolArg("-printcoinage"))
        printf("block coin age total nCoinDays=%" PRId64 "\n", nCoinAge);
    return true;
}

bool CBlock::AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos, const uint256& hashProof)
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AddToBlockIndex() : %s already exists", hash.ToString().c_str());

    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(nFile, nBlockPos, *this);
    if (!pindexNew)
        return error("AddToBlockIndex() : new CBlockIndex failed");
    pindexNew->phashBlock = &hash;
    map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }

    // ppcoin: compute chain trust score
    pindexNew->bnChainTrust = (pindexNew->pprev ?
                                 pindexNew->pprev->bnChainTrust : 0) +
                              pindexNew->GetBlockTrust(pregistryMain);

    // ppcoin: compute stake entropy bit for stake modifier
    if (!pindexNew->SetStakeEntropyBit(GetStakeEntropyBit(pindexNew->nHeight)))
        return error("AddToBlockIndex() : SetStakeEntropyBit() failed");

    // Record proof hash value
    if (pindexNew->IsProofOfStake())
    {
         pindexNew->hashProofOfStake = hashProof;
    }
    // ppcoin: compute stake modifier
    uint64_t nStakeModifier = 0;
    bool fGeneratedStakeModifier = false;
    if (!ComputeNextStakeModifier(pindexNew->pprev, nStakeModifier, fGeneratedStakeModifier))
        return error("AddToBlockIndex() : ComputeNextStakeModifier() failed");
    pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
    pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew);
    if (!CheckStakeModifierCheckpoints(pindexNew->nHeight, pindexNew->nStakeModifierChecksum))
    {
        return error("AddToBlockIndex() : Rejected by stake modifier "
                     "checkpoint height=%d, modifier=0x%016" PRIx64,
                     pindexNew->nHeight, nStakeModifier);
    }

    // Add to mapBlockIndex
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    if (pindexNew->IsProofOfStake())
        setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));
    pindexNew->phashBlock = &((*mi).first);

    // Write to disk block index
    CTxDB txdb;
    if (!txdb.TxnBegin())
        return false;
    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));
    if (!txdb.TxnCommit())
        return false;

    // New best
    if (pindexNew->bnChainTrust > bnBestChainTrust)
    {
        QPRegistry *pregistryTemp = new QPRegistry(pregistryMain);
        if (!pregistryTemp->UpdateOnNewBlock(pindexNew,
                                             !IsInitialBlockDownload()))
        {
            return error("AddToBlockIndex() : registry couldn't update new block");
        }

        if (!SetBestChain(txdb, pindexNew))
            return false;

        pregistryMain->Copy(pregistryTemp);
        delete pregistryTemp;
        pregistryTemp = NULL;
    }

    //txdb.Close();

    if (pindexNew == pindexBest && !IsQuantumProofOfStake())
    {
        // Notify UI to display prev block's coinbase if it was ours
        static uint256 hashPrevBestCoinBase;
        UpdatedTransaction(hashPrevBestCoinBase);
        if (vtx.size() > 0)
        {
            hashPrevBestCoinBase = vtx[0].GetHash();
        }
    }



    uiInterface.NotifyBlocksChanged();
    return true;
}


bool CBlock::CheckBlock(QPRegistry *pregistryTemp,
                        vector<QPoSTxDetails> &vDeetsRet,
                        CBlockIndex* pindexPrev,
                        bool fCheckPOW,
                        bool fCheckMerkleRoot,
                        bool fCheckSig,
                        bool fCheckQPoS) const
{
    int nThisHeight = pindexPrev->nHeight + 1;
    int nFork = GetFork(nThisHeight);
    int nBlockFork = GetFork(nHeight);

    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits (note qPoS allows empty blocks)
    if (((nBlockFork < XST_FORKASDF) && vtx.empty()) ||
        (vtx.size() > MAX_BLOCK_SIZE) ||
        ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
    {
        return DoS(100, error("CheckBlock() : size limits failed at height %d", nThisHeight));
    }

    // Check proof of work matches claimed amount
    if (fCheckPOW && IsProofOfWork() && !CheckProofOfWork(GetHash(), nBits))
    {
        return DoS(50, error("CheckBlock() : proof of work failed"));
    }

    // Check block timestamp
    if ((nBlockFork >= XST_FORKASDF) && fCheckQPoS)
    {
        if (!pregistryTemp->IsInReplayMode())
        {
            pregistryTemp->UpdateOnNewTime(nTime, pindexBest, false);
            if (!pregistryTemp->TimestampIsValid(nStakerID, nTime))
            {
                printf("CheckBlock(): now=%" PRId64 ", "
                         "hash=%s, height=%d, staker=%u, "
                         "timestamp=%d, round=%u, window=(%u, %u)\n",
                       GetAdjustedTime(),
                       GetHash().ToString().c_str(),
                       nHeight, nStakerID, nTime,
                       pregistryTemp->GetRound(),
                       pregistryTemp->GetCurrentSlotStart(),
                       pregistryTemp->GetCurrentSlotEnd());
                return error("CheckBlock() : timestamp is invalid for staker");
            }
        }
    }
    else if (nFork >= XST_FORK005)
    {
        if (GetBlockTime() > FutureDrift(GetAdjustedTime()))
        {
            return error("CheckBlock() : block timestamp too far in the future");
        }
    }
    else
    {
        if (GetBlockTime() > GetAdjustedTime() + nMaxClockDrift)
        {
            return error("CheckBlock() : block timestamp too far in the future");
        }
    }

    if (nBlockFork >= XST_FORKASDF)
    {
        if (!vtx.empty())
        {
            for (unsigned int i = 0; i < vtx.size(); i++)
            {
                if (vtx[i].IsCoinBase() || vtx[i].IsCoinStake())
                {
                    return DoS(100, error("CheckBlock() : no base/stake allowed"));
                }
            }
        }
        if (!IsQuantumProofOfStake())
        {
            return DoS(100, error("CheckBlock() : block must be qPoS"));
        }
    }
    else
    {
        // First transaction must be coinbase, the rest must not be
        if (!vtx[0].IsCoinBase())
        {
            return DoS(100, error("CheckBlock() : first tx is not coinbase"));
        }
        for (unsigned int i = 1; i < vtx.size(); i++)
        {
            if (vtx[i].IsCoinBase())
            {
                return DoS(100, error("CheckBlock() : more than one coinbase"));
            }
        }
    }

    // Check coinbase timestamp
    if ((nFork < XST_FORK005) && (nBlockFork < XST_FORKASDF))
    {
        // prior to XST_FORK006 CTransactions have timestamps
        if (GetBlockTime() > (int64_t)vtx[0].GetTxTime() + nMaxClockDrift)
        {
            return DoS(50, error("CheckBlock() : coinbase timestamp is too early"));
        }
    }
    // exclude pow because
    //    (1) testnet usually has too few miners to stay in future drift
    //    (2) main net mining is done, so there is no need to check drift,
    //        and also the future drift was much more at the time
    // this probably should be fixed "the right way" one day
    else
    {
        if (!vtx.empty())
        {
            if (vtx[0].HasTimestamp() && !IsProofOfWork())
            {
                if (GetBlockTime() > FutureDrift((int64_t)vtx[0].GetTxTime()))
                {
                    return DoS(50, error("CheckBlock() : coinbase timestamp: %" PRId64 " + 15 sec, "
                                     "is too early for block: %" PRId64,
                                        (int64_t)vtx[0].GetTxTime(), (int64_t)GetBlockTime()));
                }
            }
        }
    }

    if (IsProofOfStake())
    {
        if (vtx[0].vout.size() != 1 || !vtx[0].vout[0].IsEmpty())
        {
            return DoS(100,
              error("CheckBlock() : coinbase output not empty for proof-of-stake block"));
        }
        // Second transaction must be coinstake, the rest must not be
        if (!vtx[1].IsCoinStake())
        {
            return DoS(100, error("CheckBlock() : second tx is not coinstake"));
        }
        for (unsigned int i = 2; i < vtx.size(); i++)
        {
            if (vtx[i].IsCoinStake())
            {
                return DoS(100, error("CheckBlock() : more than one coinstake"));
            }
        }

        // Check coinstake timestamp
        // no check upon XST_FORK006 because tx timestamps eliminated,
        //      effectivly making tx same as block
        if (vtx[1].HasTimestamp() &&
            (!CheckCoinStakeTimestamp(GetBlockTime(), (int64_t)vtx[1].GetTxTime())))
        {
                return DoS(50,
                   error("CheckBlock() : coinstake timestamp violation nTimeBlock=%"
                         PRId64 " nTimeTx=%u", GetBlockTime(), vtx[1].GetTxTime()));
        }

        if ((GetFork(nBestHeight + 1) >= XST_FORK002) && (IsProofOfWork()))
        {
             return DoS(100, error("CheckBlock() : Proof of work (%f XST) at t=%d on or after block %d.\n",
                                   ((double) vtx[0].GetValueOut() / (double) COIN),
                                    (int) nTime,
                                    (int) GetPoWCutoff()));
        }
        if (fCheckSig & !CheckBlockSignature(pregistryTemp))
        {
            return DoS(100, error("CheckBlock() : bad proof-of-stake block signature"));
        }
    }
    if (IsQuantumProofOfStake())
    {
        if (fCheckSig & !CheckBlockSignature(pregistryTemp))
        {
            return DoS(100, error("CheckBlock() : bad qPoS block signature"));
        }
    }

    // Check transactions
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        if (!tx.CheckTransaction())
        {
            return DoS(tx.nDoS, error("CheckBlock() : CheckTransaction failed"));
        }

        // ppcoin: check transaction timestamp
        // ignore as of XST_FORK006 as tx timestamps are thereupon eliminated
        if (tx.HasTimestamp() && (GetBlockTime() < (int64_t)tx.GetTxTime()))
        {
            return DoS(50, error("CheckBlock() : block timestamp earlier than transaction timestamp"));
        }
    }

    // Check for duplicate txids. This is caught by ConnectInputs(),
    // but catching it earlier avoids a potential DoS attack:
    set<uint256> uniqueTx;
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        uniqueTx.insert(tx.GetHash());
    }
    if (uniqueTx.size() != vtx.size())
        return DoS(100, error("CheckBlock() : duplicate transaction"));

    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        nSigOps += tx.GetLegacySigOpCount();
    }
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return DoS(100, error("CheckBlock() : out-of-bounds SigOpCount"));

    // Check merkle root
    if (fCheckMerkleRoot && hashMerkleRoot != BuildMerkleTree())
        return DoS(100, error("CheckBlock() : hashMerkleRoot mismatch"));

   if (fCheckQPoS)
   {
       /***********************************************************************
        * qPoS checks
        ***********************************************************************/
        // This loop gathers all valid qPoS transactions, keeping their order
        // according to order in CBlock.vtx and order in CTransaction.vout
        // Each tx.vout is looped over and the candidate qPoS transactions
        // gathered, then they are checked within each transaction.
        // Other checks herein include
        //   (1) ensuring the same alias is not attempted
        //       to be registered for two different stakers
        //   (2) setkey operations are not attempted on the same staker in
        //       two different transactions
        //   (3) claims are gathered by public key and the total claim can
        //       not exceed the registry balance for the key
        // Note that any number and any sequence of valid setstate operations
        // are permissible on the block level, even though only one is
        // permitted on the transaction level. On the transaction level,
        // the other important checks are that a setstate tx has only a
        // single input and that the signatory of that input owns the staker.
        CTxDB txdb("r");
        map<string, qpos_purchase> mapPurchases;
        map<unsigned int, vector<qpos_setkey> > mapSetKeys;
        map<CPubKey, vector<qpos_claim> > mapClaims;
        // holds ordered validated deets that will be applied to registry state
        vDeetsRet.clear();
        BOOST_FOREACH(const CTransaction &tx, vtx)
        {
            std::vector<QPoSTxDetails> vDeets;
            MapPrevTx mapInputs;
            // round up candidate qPoS transactions without any validation
            bool fNeedsInputs = tx.GetQPoSTxDetails(vDeets);
            if (fNeedsInputs)
            {
                // pre-fill mapInputs with prevouts in this (same) block
                for (unsigned int i = 0; i < tx.vin.size(); ++i)
                {
                    uint256 hash = tx.vin[i].prevout.hash;
                    BOOST_FOREACH(const CTransaction &tx2, vtx)
                    {
                        if (tx2 == tx)
                        {
                            // prevouts must be previous in the vtx (?)
                            break;
                        }
                        if (tx2.GetHash() == hash)
                        {
                            mapInputs[hash].second = tx;
                            // only need a dummy CTxIndex to check sig
                            mapInputs[hash].first.vSpent.resize(tx.vout.size());
                            continue;
                        }
                    }
                }
                map<uint256, CTxIndex> mapUnused;
                bool fInvalid = false;
                if (!tx.FetchInputs(txdb, mapUnused,
                                    false, false, mapInputs, fInvalid))
                {
                    if (fInvalid)
                    {
                        printf("CheckBlock(): invalid qPoS inputs\n");
                    }
                    else
                    {
                        // no DoS here because inputs validated elsewhere
                        printf("CheckBlock(): fail fetching qPoS inputs\n");
                        return false;
                    }
                }
            }

            bool fPurchasesChecked = false;
            bool fSetKeysChecked = false;
            bool fSetStateChecked = false;
            bool fClaimChecked = false;
            // validate the candidate qPoS transactions
            BOOST_FOREACH(const QPoSTxDetails &deet, vDeets)
            {
                switch (static_cast<txnouttype>(deet.t))
                {
                case TX_PURCHASE1:
                case TX_PURCHASE3:
                  {
                    if (fPurchasesChecked)
                    {
                        break;
                    }
                    int64_t nStakerPrice = GetStakerPrice(pregistryTemp,
                                                          pindexPrev);
                    map<string, qpos_purchase> mapTxPrchs;
                    if (!tx.CheckPurchases(pregistryTemp,
                                           nStakerPrice,
                                           mapTxPrchs))
                    {
                        return DoS(100, error("CheckBlock(): purchase fail"));
                    }
                    map<string, qpos_purchase>::const_iterator it;
                    // two loops because we have to check all first
                    // tx must fail if any fail
                    for (it = mapTxPrchs.begin(); it != mapTxPrchs.end(); ++it)
                    {
                        if (mapPurchases.count(it->first))
                        {
                            return DoS(100,
                              error("CheckBlock(): multiple purchases for name"));
                        }
                    }
                    for (it = mapTxPrchs.begin(); it != mapTxPrchs.end(); ++it)
                    {
                        mapPurchases[it->first] = it->second;
                    }
                    fPurchasesChecked = true;
                    break;
                  }
                case TX_SETOWNER:
                case TX_SETDELEGATE:
                case TX_SETCONTROLLER:
                  {
                    if (fSetKeysChecked)
                    {
                        break;
                    }
                    vector<qpos_setkey> vSetKeys;
                    if (!tx.CheckSetKeys(pregistryTemp, mapInputs, vSetKeys))
                    {
                        return DoS(100, error("CheckBlock(): setkey fail"));
                    }
                    if (vSetKeys.empty())
                    {
                        // should never happen
                        return DoS(100, error("CheckBlock(): no keys set"));
                    }
                    if (mapSetKeys.count(vSetKeys[0].id))
                    {
                        return DoS(100, error("CheckBlock(): multiple setkeys"));
                    }
                    mapSetKeys[vSetKeys[0].id] = vSetKeys;
                    fSetKeysChecked = true;
                    break;
                  }
                case TX_ENABLE:
                case TX_DISABLE:
                  {
                    if (fSetStateChecked)
                    {
                        // this should never happen
                        return DoS(100, error("CheckBlock(): multiple setstates"));
                    }
                    qpos_setstate setstate;
                    if (!tx.CheckSetState(pregistryTemp, mapInputs, setstate))
                    {
                        return DoS(100, error("CheckBlock(): bad setstate"));
                    }
                    fSetStateChecked = true;
                    break;
                  }
                case TX_CLAIM:
                  {
                    if (fClaimChecked)
                    {
                        // this should never happen
                        return DoS(100, error("CheckBlock(): multiple claims"));
                    }
                    qpos_claim claim;
                    if (!tx.CheckClaim(pregistryTemp, mapInputs, claim))
                    {
                        return DoS(100, error("CheckBlock(): bad claim"));
                    }
                    map<CPubKey, vector<qpos_claim> >::const_iterator it;
                    it = mapClaims.find(claim.key);
                    if (it != mapClaims.end())
                    {
                        int64_t nTotal = claim.value;
                        BOOST_FOREACH(const qpos_claim &c, it->second)
                        {
                            nTotal += c.value;
                        }
                        if (!pregistryTemp->CanClaim(claim.key, nTotal, nTime))
                        {
                            return DoS(100,
                               error("CheckBlock(): invalid total claimed"));
                        }
                    }
                    mapClaims[claim.key].push_back(claim);
                    fClaimChecked = true;
                    break;
                  }
                default:
                    // this should never happen
                    return DoS(100, error("CheckBlock(): unknown qPoS operation"));
                }
                vDeetsRet.push_back(deet);
            }
        }
       /***   end qPos checks   **********************************************/
    }

    return true;
}


bool CBlock::AcceptBlock()
{
    int nFork = GetFork(nBestHeight + 1);

    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AcceptBlock() : block already in mapBlockIndex");

    // Get prev block index
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
    {
        return DoS(10, error("AcceptBlock() : prev block not found"));
    }
    CBlockIndex* pindexPrev = (*mi).second;
    int nThisHeight = pindexPrev->nHeight + 1;

    if (IsProofOfStake() && (nFork >= XST_FORKASDF))
    {
        return DoS(100, error("AcceptBlock() : No more PoS allowed (height = %d)", nThisHeight));
    }

    if (IsProofOfWork() && (nFork >= XST_FORK002))
    {
        return DoS(100, error("AcceptBlock() : No more PoW allowed (height = %d)", nThisHeight));
    }

    if ((nFork < XST_FORKASDF) && (nFork >= XST_FORK005))
    {
        if (vtx[0].HasTimestamp())
        {
            // Check coinbase timestamp
            if (GetBlockTime() > FutureDrift((int64_t)vtx[0].GetTxTime()))
            {
                return DoS(50, error("AcceptBlock() : coinbase timestamp is too early"));
            }
        }
        if (vtx[1].HasTimestamp())
        {
            // Check coinstake timestamp
            if (IsProofOfStake() && !CheckCoinStakeTimestamp(GetBlockTime(), (int64_t)vtx[1].GetTxTime()))
            {
                return DoS(50, error("AcceptBlock() : coinstake timestamp violation nTimeBlock=%" PRId64
                                     " nTimeTx=%u", GetBlockTime(), vtx[1].GetTxTime()));
            }
        }
    }

    // Check proof-of-work or proof-of-stake
    if ((nFork < XST_FORKASDF) &&
        (nBits != GetNextTargetRequired(pindexPrev, IsProofOfStake())))
    {
        return DoS(100, error("AcceptBlock() : incorrect %s", IsProofOfWork() ? "proof-of-work" : "proof-of-stake"));
    }

    // asdf should just move time slot check here (see checkblock)

    // Check timestamp against prev
    if (nFork >= XST_FORKASDF)
    {
        if (GetBlockTime() < pindexPrev->nTime)
        {
            return error("AcceptBlock() : block's timestamp is earlier than prev block");
        }
        if (GetBlockTime() == pindexPrev->nTime)
        {
            return error("AcceptBlock() : block's timestamp is same as prev block");
        }
    }
    else if (nFork >= XST_FORK005)
    {
        if (GetBlockTime() <= pindexPrev->GetPastTimeLimit() || FutureDrift(GetBlockTime()) < pindexPrev->GetBlockTime())
            return error("AcceptBlock() : block's timestamp is too early");
    }
    else
    {
        if (GetBlockTime() <= pindexPrev->GetMedianTimePast() || GetBlockTime() + nMaxClockDrift < pindexPrev->GetBlockTime())
            return error("AcceptBlock() : block's timestamp is too early");
    }

    // Check that all transactions are finalized
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        if (!tx.IsFinal(nThisHeight, GetBlockTime()))
        {
            return DoS(10, error("AcceptBlock() : contains a non-final transaction"));
        }
    }

    // Check that the block chain matches the known block chain up to a checkpoint
    if ((nFork < XST_FORKASDF) && (!Checkpoints::CheckHardened(nThisHeight, hash)))
    {
        return DoS(100, error("AcceptBlock() : rejected by hardened checkpoint lock-in at %d", nThisHeight));
    }

    uint256 hashProof;
    // Verify hash target and signature of coinstake tx
    if (IsProofOfStake())
    {
        uint256 targetProofOfStake;
        if (!CheckProofOfStake(vtx[1], nBits, hashProof, nTime))
        {
            printf("WARNING: AcceptBlock(): check proof-of-stake failed for block %s\n", hash.ToString().c_str());
            return false; // do not error here as we expect this during initial block download
        }
    }
    // PoW is checked in CheckBlock() & qPoS checked by registry
    else
    {
        hashProof = GetHash();
    }


    // ppcoin: check that the block satisfies synchronized checkpoint
    // xst: sync checkpoints are getting phased out, so ignored by default
    if (!Checkpoints::CheckSync(hash, pindexPrev))
    {
        if(!GetBoolArg("-nosynccheckpoints", true))
        {
            return error("AcceptBlock() : rejected by synchronized checkpoint");
        }
        else
        {
            strMiscWarning = _("WARNING: syncronized checkpoint violation detected, but skipped!");
        }
    }

    // Ensure that block height is serialized somwhere
    if (nFork >= XST_FORKASDF)
    {
        if (nHeight != nThisHeight)
        {
            return DoS(100, error("AcceptBlock() : block height mismatch"));
        }
    }
    else
    {
        // Pre-qPoS: Enforce rule that the coinbase starts with serialized block height
        CScript expect = CScript() << nThisHeight;
        if (!std::equal(expect.begin(), expect.end(), vtx[0].vin[0].scriptSig.begin()))
        {
            return DoS(100, error("AcceptBlock() : block height mismatch in coinbase"));
        }
    }

    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION)))
        return error("AcceptBlock() : out of disk space");
    unsigned int nFile = -1;
    unsigned int nBlockPos = 0;
    if (!WriteToDisk(nFile, nBlockPos))
        return error("AcceptBlock() : WriteToDisk failed");
    if (!AddToBlockIndex(nFile, nBlockPos, hashProof))
        return error("AcceptBlock() : AddToBlockIndex failed");

    // Relay inventory, but don't relay old inventory during initial block download
    int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate();
    if (hashBestChain == hash)
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
            if (nBestHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                pnode->PushInventory(CInv(MSG_BLOCK, hash));
    }

    // ppcoin: check pending sync-checkpoint
    if (nFork < XST_FORKASDF)
    {
        Checkpoints::AcceptPendingSyncCheckpoint();
    }

    return true;
}


CBigNum CBlockIndex::GetBlockTrust(const QPRegistry *pregistry) const
{
    if (IsQuantumProofOfStake())
    {
        unsigned int nWeight;
        if (!pregistry->GetStakerWeight(nStakerID, nWeight))
        {
            // set weight to 1 if staker is unknown
            printf("GetBlockTrust(): no such staker %u\n", nStakerID);
            nWeight = 1;
        }
        return CBigNum(nWeight);
    }
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);
    if (bnTarget <= 0)
    {
        return 0;
    }
    if (IsProofOfStake())
    {
        // Return trust score as usual
        return (CBigNum(1)<<256) / (bnTarget+1);
    }
    else
    {
        // Calculate work amount for block
        CBigNum bnPoWTrust = (bnProofOfWorkLimit / (bnTarget+1));
        return bnPoWTrust > 1 ? bnPoWTrust : 1;
    }
}

bool CBlockIndex::IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned int nRequired, unsigned int nToCheck)
{
    unsigned int nFound = 0;
    for (unsigned int i = 0; i < nToCheck && nFound < nRequired && pstart != NULL; i++)
    {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    return (nFound >= nRequired);
}


bool ProcessBlock(CNode* pfrom, CBlock* pblock,
                  bool fIsBootstrap, bool fJustCheck)
{
    bool fAllowDuplicateStake = (fIsBootstrap && GetBoolArg("-permitdirtybootstrap", false));
    // Check for duplicate
    uint256 hash = pblock->GetHash();
    if (mapBlockIndex.count(hash))
        return error("ProcessBlock() : already have block %d %s", mapBlockIndex[hash]->nHeight, hash.ToString().c_str());
    if (mapOrphanBlocks.count(hash))
        return error("ProcessBlock() : already have block (orphan) %s", hash.ToString().c_str());

    // ppcoin: check proof-of-stake
    // Limited duplicity on stake: prevents block flood attack
    // Duplicate stake allowed only when there is orphan child block
    // xst: or on bootstrap, which happens in very rare cases
    if (pblock->IsProofOfStake() &&
        setStakeSeen.count(pblock->GetProofOfStake()) &&
        !mapOrphanBlocksByPrev.count(hash) &&
        !Checkpoints::WantedByPendingSyncCheckpoint(hash) && !fAllowDuplicateStake)
    {
        return error("ProcessBlock() : duplicate proof-of-stake (%s, %d) for block %s",
                     pblock->GetProofOfStake().first.ToString().c_str(),
                     pblock->GetProofOfStake().second, hash.ToString().c_str());
    }

    CBlockLocator locator;
    int nHeight = locator.GetBlockIndex()->nHeight;

    // Preliminary checks

    // Why make a temp registry?
    // The registry clock is basically a Lamport clock, where advance
    // of the local timestamp beyond the network event requires a local
    // event (block production). To validate new blocks, the registry
    // clock needs to advance to the network event (block) timestamp,
    // as blocks are the only sources of time. However, care must be
    // taken not to advance the timestamp just to check a block because
    // that would make it easy for an attacker to advance its peers'
    // clocks with a block that is not fully validated. Therefore, a temp
    // registry must be used to be advanced by the block then check the
    // block. If the block is valid using this registry, then the local
    // clock can be advanced when the block is added to the growing chain.
    QPRegistry *pregistryTemp = new QPRegistry(pregistryMain);
    bool fCheckOK;
    if (fJustCheck)
    {
        printf("asdf just checking block\n");
        vector<QPoSTxDetails> vDeets;
        fCheckOK = pblock->CheckBlock(pregistryTemp, vDeets, pindexBest,
                                      true, true, false, false);
    }
    else
    {
        printf("asdf fully checking block\n");
        vector<QPoSTxDetails> vDeets;
        fCheckOK = pblock->CheckBlock(pregistryTemp, vDeets, pindexBest);
    }
    delete pregistryTemp;
    pregistryTemp = NULL;
    if (!fCheckOK)
    {
        return error("ProcessBlock() : CheckBlock FAILED");
    }

    // no more proof of work
    if (pblock->IsProofOfWork() && (GetFork(nHeight) >= XST_FORK002))
    {
        if (pfrom)
        {
              pfrom->Misbehaving(100);
        }
        printf("Proof-of-work on or after block %d.\n", GetPoWCutoff());
        return error("Proof-of-work on or after block %d.\n", GetPoWCutoff());
    }

    // no more proof of stake
    if (pblock->IsProofOfStake() && (GetFork(nHeight) >= XST_FORKASDF))
    {
        if (pfrom)
        {
              pfrom->Misbehaving(100);
        }
        printf("Proof-of-stake on or after block %d.\n", GetPoSCutoff());
        return error("Proof-of-stake on or after block %d.\n", GetPoSCutoff());
    }

    bool fUseSyncCheckpoints = !GetBoolArg("-nosynccheckpoints", true);

   /****************************************************************************
    * checkpoint specific code
    *
    * checkpoints are getting phased out
    * this code uses checkpoints for some spam prevention
    ***************************************************************************/
    if (fUseSyncCheckpoints)
    {
        CBlockIndex* pcheckpoint = Checkpoints::GetLastSyncCheckpoint();
        if(pcheckpoint && fDebug)
        {
            const CBlockIndex* pindexLastPos = GetLastBlockIndex(pcheckpoint, true);
            if(pindexLastPos)
            {
                printf("ProcessBlock(): Last POS Block Height: %d \n", pindexLastPos->nHeight);
            }
            else
            {
                printf("ProcessBlock(): Previous POS block not found.\n");
            }
        }

        if (pcheckpoint && pblock->hashPrevBlock != hashBestChain && !Checkpoints::WantedByPendingSyncCheckpoint(hash))
        {
            // Extra checks to prevent "fill up memory by spamming with bogus blocks"
            int64_t deltaTime = pblock->GetBlockTime() - pcheckpoint->nTime;
            CBigNum bnNewBlock;
            bnNewBlock.SetCompact(pblock->nBits);
            CBigNum bnRequired;
            const CBlockIndex* LastBlock = GetLastBlockIndex(pcheckpoint, true);
            int nThisHeight = LastBlock->nHeight + 1;
            unsigned int nLastBits = LastBlock->nBits;

            if (pblock->IsProofOfStake()) {
                bnRequired.SetCompact(ComputeMinStake(nLastBits, deltaTime, pblock->nTime));
                if ((GetFork(nThisHeight) < XST_FORK002) && (bnNewBlock > bnProofOfStakeLimit)) {
                    bnNewBlock = bnNewBlock >> 2;  // adjust target for big hashes
                }
            }
            else if (pblock->IsProofOfWork())
            {
                bnRequired.SetCompact(ComputeMinWork(nLastBits, deltaTime));
            }

            if (bnNewBlock > bnRequired)
            {
                if (pfrom)
                    pfrom->Misbehaving(100);
                return error("ProcessBlock() : block with too little %s", pblock->IsProofOfStake()? "proof-of-stake" : "proof-of-work");
            }
        }

        // ppcoin: ask for pending sync-checkpoint if any
        if (!IsInitialBlockDownload())
            Checkpoints::AskForPendingSyncCheckpoint(pfrom);
    }
   /**************************************************************************
    * end of checkpoint specific code
    **************************************************************************/

    // If don't already have its previous block, shunt it off to holding area until we get it
    if (!mapBlockIndex.count(pblock->hashPrevBlock))
    {
        printf("ProcessBlock: ORPHAN BLOCK, prev=%s\n", pblock->hashPrevBlock.ToString().c_str());
        CBlock* pblock2 = new CBlock(*pblock);
        // ppcoin: check proof-of-stake
        if (pblock2->IsProofOfStake())
        {
            // Limited duplicity on stake: prevents block flood attack
            // Duplicate stake allowed only when there is orphan child block
            if (setStakeSeenOrphan.count(pblock2->GetProofOfStake()) &&
                !mapOrphanBlocksByPrev.count(hash) &&
                !(fUseSyncCheckpoints && Checkpoints::WantedByPendingSyncCheckpoint(hash)))
            {
                return error("ProcessBlock() : duplicate proof-of-stake (%s, %d) for orphan block %s", pblock2->GetProofOfStake().first.ToString().c_str(), pblock2->GetProofOfStake().second, hash.ToString().c_str());
            }
            else
            {
                setStakeSeenOrphan.insert(pblock2->GetProofOfStake());
            }
        }
        mapOrphanBlocks.insert(make_pair(hash, pblock2));
        mapOrphanBlocksByPrev.insert(make_pair(pblock2->hashPrevBlock, pblock2));

        // Ask this guy to fill in what we're missing
        if (pfrom)
        {
            printf("asdf getting push blocks asdf\n");
            pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(pblock2));
            // ppcoin: getblocks may not obtain the ancestor block rejected
            // earlier by duplicate-stake check so we ask for it again directly
            if (IsInitialBlockDownload())
            {
                pfrom->nOrphans += 1;
            }
            else
            {
                pfrom->nOrphans = 0;
                pfrom->AskFor(CInv(MSG_BLOCK, WantedByOrphan(pblock2)));
            }
        }
        else printf("asdf no pfrom\n"); // asdf
        return true;
    }

    // Store to disk
    if (!pblock->AcceptBlock())
    {
        return error("ProcessBlock() : AcceptBlock FAILED");
    }

    // Recursively process any orphan blocks that depended on this one
    vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (unsigned int i = 0; i < vWorkQueue.size(); i++)
    {
        uint256 hashPrev = vWorkQueue[i];
        for (multimap<uint256, CBlock*>::iterator mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
             mi != mapOrphanBlocksByPrev.upper_bound(hashPrev);
             ++mi)
        {
            CBlock* pblockOrphan = (*mi).second;
            if (pblockOrphan->AcceptBlock())
                vWorkQueue.push_back(pblockOrphan->GetHash());
            mapOrphanBlocks.erase(pblockOrphan->GetHash());
            setStakeSeenOrphan.erase(pblockOrphan->GetProofOfStake());
            delete pblockOrphan;
        }
        mapOrphanBlocksByPrev.erase(hashPrev);
    }

    printf("ProcessBlock: ACCEPTED\n");

    // ppcoin: if responsible for sync-checkpoint send it
    if (pfrom && !CSyncCheckpoint::strMasterPrivKey.empty())
        Checkpoints::SendSyncCheckpoint(Checkpoints::AutoSelectSyncCheckpoint());

    return true;
}

// ppcoin: sign block
bool CBlock::SignBlock(const CKeyStore& keystore, const QPRegistry *pregistry)
{
    vector<valtype> vSolutions;
    txnouttype whichType;

    if (IsQuantumProofOfStake())
    {
        if (pregistry == NULL)
        {
            return error("SignBlock(): pregistry is NULL");
        }
        CPubKey pubkey;
        if (!pregistry->GetDelegateKey(nStakerID, pubkey))
        {
            return false;
        }
        CKey key;
        if (!keystore.GetKey(pubkey.GetID(), key))
        {
            return false;
        }
        if (key.GetPubKey() != pubkey)
        {
            return false;
        }
        return key.Sign(GetHash(), vchBlockSig);
    }
    else if (IsProofOfStake())
    {
        const CTxOut& txout = vtx[1].vout[1];

        if (!Solver(txout.scriptPubKey, whichType, vSolutions))
            return false;

        if (whichType == TX_PUBKEY)
        {
            // Sign
            valtype& vchPubKey = vSolutions[0];
            CKey key;

            if (!keystore.GetKey(Hash160(vchPubKey), key))
                return false;
            if (key.GetPubKey() != vchPubKey)
                return false;

            return key.Sign(GetHash(), vchBlockSig);
        }
    }
#ifdef WITH_MINER
    else  /* PoW */
    {
        for(unsigned int i = 0; i < vtx[0].vout.size(); i++)
        {
            const CTxOut& txout = vtx[0].vout[i];

            if (!Solver(txout.scriptPubKey, whichType, vSolutions))
                continue;

            if (whichType == TX_PUBKEY)
            {
                // Sign
                valtype& vchPubKey = vSolutions[0];
                CKey key;

                if (!keystore.GetKey(Hash160(vchPubKey), key))
                    continue;
                if (key.GetPubKey() != vchPubKey)
                    continue;
                if(!key.Sign(GetHash(), vchBlockSig))
                    continue;

                return true;
            }
        }
    }
#endif  /* WITH_MINER */

    printf("Sign failed\n");
    return false;
}

// ppcoin: check block signature
bool CBlock::CheckBlockSignature(const QPRegistry *pregistry) const
{
    if (GetHash() == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet))
        return vchBlockSig.empty();

    vector<valtype> vSolutions;
    txnouttype whichType;

    if(IsQuantumProofOfStake())
    {
        CPubKey vchPubKey;
        if (!pregistry->GetDelegateKey(nStakerID, vchPubKey))
        {
            return false;
        }
        CKey key;
        if (!key.SetPubKey(vchPubKey))
            return false;
        if (vchBlockSig.empty())
            return false;
        return key.Verify(GetHash(), vchBlockSig);
    }
    else if(IsProofOfStake())
    {
        const CTxOut& txout = vtx[1].vout[1];

        if (!Solver(txout.scriptPubKey, whichType, vSolutions))
        {
            return false;
        }
        if (whichType == TX_PUBKEY)
        {
            valtype& vchPubKey = vSolutions[0];
            CKey key;
            if (!key.SetPubKey(vchPubKey))
                return false;
            if (vchBlockSig.empty())
                return false;
            return key.Verify(GetHash(), vchBlockSig);
        }
    }
    else  /* PoW */
    {
        for(unsigned int i = 0; i < vtx[0].vout.size(); i++)
        {
            const CTxOut& txout = vtx[0].vout[i];

            if (!Solver(txout.scriptPubKey, whichType, vSolutions))
            {
                return false;
            }

            if (whichType == TX_PUBKEY)
            {
                // Verify
                valtype& vchPubKey = vSolutions[0];
                CKey key;
                if (!key.SetPubKey(vchPubKey))
                    continue;
                if (vchBlockSig.empty())
                    continue;
                if(!key.Verify(GetHash(), vchBlockSig))
                    continue;

                return true;
            }
        }
    }
    return false;
}


bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = filesystem::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
    {
        fShutdown = true;
        string strMessage = _("Warning: Disk space is low!");
        strMiscWarning = strMessage;
        printf("*** %s\n", strMessage.c_str());
        uiInterface.ThreadSafeMessageBox(strMessage, "Stealth", CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION | CClientUIInterface::MODAL);
        StartShutdown();
        return false;
    }
    return true;
}


static filesystem::path BlockFilePath(unsigned int nFile)
{
    string strBlockFn = strprintf("blk%04u.dat", nFile);
    return GetDataDir() / strBlockFn;
}


FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode)
{
    if ((nFile < 1) || (nFile == (unsigned int) -1))
        return NULL;
    FILE* file = fopen(BlockFilePath(nFile).string().c_str(), pszMode);
    if (!file)
        return NULL;
    if (nBlockPos != 0 && !strchr(pszMode, 'a') && !strchr(pszMode, 'w'))
    {
        if (fseek(file, nBlockPos, SEEK_SET) != 0)
        {
            fclose(file);
            return NULL;
        }
    }
    return file;
}


static unsigned int nCurrentBlockFile = 1;

FILE* AppendBlockFile(unsigned int& nFileRet)
{
    nFileRet = 0;
    LOOP
    {
        FILE* file = OpenBlockFile(nCurrentBlockFile, 0, "ab");
        if (!file)
            return NULL;
        if (fseek(file, 0, SEEK_END) != 0)
            return NULL;
        // FAT32 file size max 4GB, fseek and ftell max 2GB, so we must stay under 2GB
        if (ftell(file) < (long)(0x7F000000 - MAX_SIZE))
        {
            nFileRet = nCurrentBlockFile;
            return file;
        }
        fclose(file);
        nCurrentBlockFile++;
    }
}


bool LoadBlockIndex(bool fAllowNew)
{
    if (fTestNet)
    {
        pchMessageStart[0] = 0xcf;
        pchMessageStart[1] = 0xed;
        pchMessageStart[2] = 0xff;
        pchMessageStart[3] = 0xfd;

        bnProofOfStakeLimit = bnProofOfStakeLimitTestNet; // 0x00000fff PoS base target is fixed in testnet
        bnProofOfWorkLimit = bnProofOfWorkLimitTestNet; // 0x0000ffff PoW base target is fixed in testnet
        nStakeMinAge = 1 * 60; // test net min age is 1 min
        nStakeMaxAge = 40 * 60; // test net min age is 40 min
        nModifierInterval = 30; // test modifier interval is 30 seconds
        nCoinbaseMaturity = 10; // test maturity is 10 blocks
        nStakeTargetSpacing = 20; // test block spacing is 20 seconds
    }

    //
    // Load block index
    //
    CTxDB txdb("cr+");
    if (!txdb.LoadBlockIndex())
        return false;
    //txdb.Close();

    //
    // Init with genesis block
    //
    if (mapBlockIndex.empty())
    {
        if (!fAllowNew)
            return false;

        // Genesis block
        const char* pszTimestamp = "20140615 Stealth proves that pzTimestamp is overkill.";
        CTransaction txNew;
        txNew.SetTxTime(nChainStartTime);
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CBigNum(9999) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].SetEmpty();

        CBlock block;
        block.vtx.push_back(txNew);
        block.hashPrevBlock = 0;
        block.hashMerkleRoot = block.BuildMerkleTree();
        block.nVersion = 1;
        block.nTime    = 1403668979;
        block.nBits    = bnProofOfWorkLimit.GetCompact();
        block.nNonce   = 4204204204LL; // perfect nonce

        if (false && (block.GetHash() != hashGenesisBlock))
        {
            // This will figure out a valid hash and Nonce if you're
            // creating a different genesis block:
            uint256 hashTarget = CBigNum().SetCompact(block.nBits).getuint256();
            while (block.GetHash() > hashTarget)
            {
                 ++block.nNonce;
                 if (block.nNonce == 0)
                 {
                     printf("NONCE WRAPPED, incrementing time");
                     ++block.nTime;
                 }
            }
        }

        //// debug print
        block.print();
        printf("block.GetHash() == %s\n", block.GetHash().ToString().c_str());
        printf("block.hashMerkleRoot == %s\n", block.hashMerkleRoot.ToString().c_str());
        printf("block.nTime = %u \n", block.nTime);
        printf("block.nNonce = %u \n", block.nNonce);

        uint256 hashMerkleRootMainNet("0xe3de7c386d5b82f62ff24c6d2351539c22b17c6ffab0e267b3cdd72fda82bd83");
        uint256 hashMerkleRootTestNet("0xe3de7c386d5b82f62ff24c6d2351539c22b17c6ffab0e267b3cdd72fda82bd83");

        assert(block.hashMerkleRoot == (fTestNet ? hashMerkleRootTestNet : hashMerkleRootMainNet));
        assert(block.GetHash() == (fTestNet ? hashGenesisBlockTestNet : hashGenesisBlock));

        // Start new block file
        unsigned int nFile;
        unsigned int nBlockPos;
        if (!block.WriteToDisk(nFile, nBlockPos))
            return error("LoadBlockIndex() : writing genesis block to disk failed");
        if (!block.AddToBlockIndex(nFile, nBlockPos, hashGenesisBlock))
            return error("LoadBlockIndex() : genesis block not accepted");

        // ppcoin: initialize synchronized checkpoint
        if (!Checkpoints::WriteSyncCheckpoint((!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet)))
            return error("LoadBlockIndex() : failed to init sync checkpoint");
    }

    // ppcoin: if checkpoint master key changed must reset sync-checkpoint
    {
        CTxDB txdb;
        string strPubKey = "";
        if (!txdb.ReadCheckpointPubKey(strPubKey) || strPubKey != CSyncCheckpoint::strMasterPubKey)
        {
            // write checkpoint master key to db
            txdb.TxnBegin();
            if (!txdb.WriteCheckpointPubKey(CSyncCheckpoint::strMasterPubKey))
                return error("LoadBlockIndex() : failed to write new checkpoint master key to db");
            if (!txdb.TxnCommit())
                return error("LoadBlockIndex() : failed to commit new checkpoint master key to db");
            if ((!fTestNet) && !Checkpoints::ResetSyncCheckpoint())
                return error("LoadBlockIndex() : failed to reset sync-checkpoint");
        }
    }

    return true;
}

void PrintBlockTree()
{
    // pre-compute tree structure
    map<CBlockIndex*, vector<CBlockIndex*> > mapNext;
    for (map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
    {
        CBlockIndex* pindex = (*mi).second;
        mapNext[pindex->pprev].push_back(pindex);
    }

    vector<pair<int, CBlockIndex*> > vStack;
    vStack.push_back(make_pair(0, pindexGenesisBlock));

    int nPrevCol = 0;
    while (!vStack.empty())
    {
        int nCol = vStack.back().first;
        CBlockIndex* pindex = vStack.back().second;
        vStack.pop_back();

        // print split or gap
        if (nCol > nPrevCol)
        {
            for (int i = 0; i < nCol-1; i++)
                printf("| ");
            printf("|\\\n");
        }
        else if (nCol < nPrevCol)
        {
            for (int i = 0; i < nCol; i++)
                printf("| ");
            printf("|\n");
       }
        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++)
            printf("| ");

        // print item
        CBlock block;
        block.ReadFromDisk(pindex);
        printf("%d (%u,%u) %s  %08x  %s  mint %7s  tx %" PRIszu "",
            pindex->nHeight,
            pindex->nFile,
            pindex->nBlockPos,
            block.GetHash().ToString().c_str(),
            block.nBits,
            DateTimeStrFormat("%x %H:%M:%S", block.GetBlockTime()).c_str(),
            FormatMoney(pindex->nMint).c_str(),
            block.vtx.size());

        PrintWallets(block);

        // put the main time-chain first
        vector<CBlockIndex*>& vNext = mapNext[pindex];
        for (unsigned int i = 0; i < vNext.size(); i++)
        {
            if (vNext[i]->pnext)
            {
                swap(vNext[0], vNext[i]);
                break;
            }
        }

        // iterate children
        for (unsigned int i = 0; i < vNext.size(); i++)
            vStack.push_back(make_pair(nCol+i, vNext[i]));
    }
}

bool LoadExternalBlockFile(FILE* fileIn)
{
    int64_t nStart = GetTimeMillis();

    int nLoaded = 0;
    {
        LOCK(cs_main);
        try {
            CAutoFile blkdat(fileIn, SER_DISK, CLIENT_VERSION);
            unsigned int nPos = 0;
            while (nPos != (unsigned int)-1 && blkdat.good() && !fRequestShutdown)
            {
                unsigned char pchData[65536];
                do {
                    fseek(blkdat, nPos, SEEK_SET);
                    int nRead = fread(pchData, 1, sizeof(pchData), blkdat);
                    if (nRead <= 8)
                    {
                        nPos = (unsigned int)-1;
                        break;
                    }
                    void* nFind = memchr(pchData, pchMessageStart[0], nRead+1-sizeof(pchMessageStart));
                    if (nFind)
                    {
                        if (memcmp(nFind, pchMessageStart, sizeof(pchMessageStart))==0)
                        {
                            nPos += ((unsigned char*)nFind - pchData) + sizeof(pchMessageStart);
                            break;
                        }
                        nPos += ((unsigned char*)nFind - pchData) + 1;
                    }
                    else
                        nPos += sizeof(pchData) - sizeof(pchMessageStart) + 1;
                } while(!fRequestShutdown);
                if (nPos == (unsigned int)-1)
                    break;
                fseek(blkdat, nPos, SEEK_SET);
                unsigned int nSize;
                blkdat >> nSize;
                if (nSize > 0 && nSize <= MAX_BLOCK_SIZE)
                {
                    CBlock block;
                    blkdat >> block;
                    if (ProcessBlock(NULL, &block, true))
                    {
                        nLoaded++;
                        nPos += 4 + nSize;
                    }
                }
            }
        }
        catch (std::exception &e) {
            printf("%s() : Deserialize or I/O error caught during load\n",
                   __PRETTY_FUNCTION__);
        }
    }
    printf("Loaded %i blocks from external file in %" PRId64 "ms\n", nLoaded, GetTimeMillis() - nStart);
    if (GetBoolArg("-quitonbootstrap", false))
    {
       exit(EXIT_SUCCESS);
    }
    return nLoaded > 0;
}




//////////////////////////////////////////////////////////////////////////////
//
// CAlert
//

extern map<uint256, CAlert> mapAlerts;
extern CCriticalSection cs_mapAlerts;

string GetWarnings(string strFor)
{
    int nPriority = 0;
    string strStatusBar;
    string strRPC;

    if (GetBoolArg("-testsafemode"))
        strRPC = "test";

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "")
    {
        nPriority = 1000;
        strStatusBar = strMiscWarning;
    }

    // ppcoin: if detected invalid checkpoint enter safe mode
    if (Checkpoints::hashInvalidCheckpoint != 0)
    {
        nPriority = 3000;
        strStatusBar = strRPC = "WARNING: Invalid checkpoint found! Displayed transactions may not be correct! You may need to upgrade, or notify developers.";
    }

    // Alerts
    {
        LOCK(cs_mapAlerts);
        BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
        {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && (alert.nPriority > nPriority) && (alert.nID >= alert.nCancel))
            {
                nPriority = alert.nPriority;
                strStatusBar = alert.strStatusBar;
                if (nPriority > 1000)
                    strRPC = strStatusBar;  // ppcoin: safe mode for high alert
            }
        }
    }

    if (strFor == "statusbar")
        return strStatusBar;
    else if (strFor == "rpc")
        return strRPC;
    assert(!"GetWarnings() : invalid parameter");
    return "error";
}




//////////////////////////////////////////////////////////////////////////////
//
// Messages
//


bool static AlreadyHave(CTxDB& txdb, const CInv& inv)
{
    switch (inv.type)
    {
    case MSG_TX:
        {
        bool txInMap = false;
        {
           LOCK(mempool.cs);
           txInMap = (mempool.exists(inv.hash));
        }
        return txInMap ||
               mapOrphanTransactions.count(inv.hash) ||
               txdb.ContainsTx(inv.hash);
        }

    case MSG_BLOCK:
        return mapBlockIndex.count(inv.hash) ||
               mapOrphanBlocks.count(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}

bool Rollback()
{
    uint256 hashRollback = pregistryMain->GetHashLastBlockPrevQueue();

    printf("ROLLBACK from %s to %s\n",
           pindexBest->GetBlockHash().ToString().c_str(),
           hashRollback.ToString().c_str());

    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashRollback);
    if (mi == mapBlockIndex.end())
    {
        return error("Rollback(): no block index to rollback to %s\n",
                     hashRollback.ToString().c_str());
    }

    CBlockIndex *pindexRollback = mi->second;

    // List of what to disconnect
    vector<CBlockIndex*> vDisconnect;
    for (CBlockIndex* pindex = pindexBest;
         pindex != pindexRollback;
         pindex = pindex->pprev)
    {
        vDisconnect.push_back(pindex);
        if (pindex->pprev == NULL)
        {
            break;
        }
    }

    if (vDisconnect.empty())
    {
        // this is not an error, just unusual
        printf("Rollback(): nothing to roll back to %s\n",
               hashRollback.ToString().c_str());
        return true;
    }

    printf("ROLLBACK: Disconnect %" PRIszu " blocks; %s back to %s\n",
           vDisconnect.size(),
           pindexBest->GetBlockHash().ToString().c_str(),
           hashRollback.ToString().c_str());

    CTxDB txdb;

    if (txdb.ActiveBatchIsNull()) printf("asdf no active batch 1\n");

    if (!txdb.TxnBegin())
        return error("Rollback() : TxnBegin failed");

    if (txdb.ActiveBatchIsNull()) printf("asdf no active batch 2\n");

    // Disconnect
    list<CTransaction> vResurrect;
    BOOST_FOREACH(CBlockIndex* pindex, vDisconnect)
    {
        CBlock block;
        if (!block.ReadFromDisk(pindex))
        {
            return error("Rollback() : ReadFromDisk for disconnect %s failed",
                         pindex->GetBlockHash().ToString().c_str());
        }
        if (!block.DisconnectBlock(txdb, pindex))
        {
            return error("Rollback() : DisconnectBlock %s failed",
                         pindex->GetBlockHash().ToString().c_str());
        }

        if (txdb.ActiveBatchIsNull()) printf("asdf no active batch 3\n");

        // Queue memory transactions to resurrect
        BOOST_REVERSE_FOREACH(const CTransaction& tx, block.vtx)
        {
            if (!(tx.IsCoinBase() || tx.IsCoinStake()) &&
                pindex->nHeight > Checkpoints::GetTotalBlocksEstimate())
            {
                vResurrect.push_front(tx);
            }
        }
    }

    if (!txdb.WriteHashBestChain(hashRollback))
    {
        return error("Rollback() : WriteHashBestChain failed");
    }

    if (txdb.ActiveBatchIsNull()) printf("asdf no active batch 4\n");

    // Make sure it's successfully written to disk before changing memory structure
    if (!txdb.TxnCommit())
    {
        return error("Rollback() : TxnCommit failed");
    }

    // Disconnect
    BOOST_FOREACH(CBlockIndex* pindex, vDisconnect)
    {
        if (pindex->pprev)
        {
            pindex->pprev->pnext = NULL;
        }
    }

    // Resurrect memory transactions that were in the disconnected branch
    BOOST_FOREACH(CTransaction& tx, vResurrect)
    {
        tx.AcceptToMemoryPool(txdb, false);
    }

    // asdf asdf REFACTOR
    // Get snapshot for temp registry and replay it to pindexRollback
    QPRegistry *pregistryTemp = new QPRegistry();
    GetRegistrySnapshot(txdb, pindexRollback->nHeight, pregistryTemp);
    uint256 blockHash = pregistryTemp->GetBlockHash();
    printf("asdf secondary empty replay registry from %s\n", blockHash.ToString().c_str());
    CBlockIndex *pindexCurrent = mapBlockIndex[blockHash];
    if (pindexCurrent->pnext == NULL)
    {
       // asdf asdf on refactor this should just return
       // this should happen exceedingly rarely if at all
       printf("Rollback(): null pnext, can't replay registry from %s\n",
              blockHash.ToString().c_str());
    }
    else
    {
        while (pindexCurrent != pindexRollback)
        {
            pindexCurrent = pindexCurrent->pnext;
            printf("asdf 8.0.6.0 not null pindexcurrent nHeight is %d\n", pindexCurrent->nHeight);
            pregistryTemp->UpdateOnNewBlock(pindexCurrent, true);
            if (pindexCurrent->pnext == NULL)
            {
                break;
            }
            if (!(pindexCurrent->pnext->IsInMainChain() ||
                  pindexCurrent->pnext == pindexRollback))
            {
               printf("asdf 8.0.6.1 pindexcurrent->next (%s) not rollback or in main chain\n",
                      pindexCurrent->pnext->GetBlockHash().ToString().c_str());
               break;
            }
        }
    }

    if (pindexCurrent != pindexRollback)
    {
        // this should never happen, but
        // don't fail in this case, just take best chain possible
        printf("Rollback(): ERROR: could not roll back to %s\n",
                hashRollback.ToString().c_str());
    }

    // copy rather than assign to retain mutexes, etc.
    pregistryMain->Copy(pregistryTemp);
    delete pregistryTemp;
    pregistryTemp = NULL;

    // Update best block in wallet (so we can detect restored wallets)
    if (!IsInitialBlockDownload())
    {
        const CBlockLocator locator(pindexCurrent);
        ::SetBestChain(locator);
    }

    // New best block
    pindexBest = pindexCurrent;
    hashBestChain = pindexBest->GetBlockHash();
    pblockindexFBBHLast = NULL;
    nBestHeight = pindexBest->nHeight;
    bnBestChainTrust = pindexBest->bnChainTrust;
    nTimeBestReceived = pindexBest->nTime;
    nTransactionsUpdated++;

    printf("Rollback: new best=%s\n"
              "    height=%d  staker=%s  trust=%s  time=%" PRIu64 " (%s)\n",
           hashBestChain.ToString().c_str(),
           nBestHeight,
           pregistryMain->GetAliasForID(pindexBest->nStakerID).c_str(),
           bnBestChainTrust.ToString().c_str(),
           pindexBest->GetBlockTime(),
           DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str());

    printf("ROLLBACK: done\n");

    return true;
}

// The message start string is designed to be unlikely to occur in normal data.
// The characters are rarely used upper ASCII, not valid as UTF-8, and produce
// a large 4-byte int at any alignment.
unsigned char pchMessageStart[4] = { 0x70, 0x35, 0x22, 0x05 };

bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv)
{
    if (fDebug) {
         printf("ProcessMessage(): pfrom-addr %s\n",
                pfrom->addr.ToString().c_str());
    }
    static map<CService, CPubKey> mapReuseKey;
    RandAddSeedPerfmon();
    if (fDebug)
        printf("received: %s (%" PRIszu " bytes)\n", strCommand.c_str(), vRecv.size());
    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
    {
        printf("dropmessagestest DROPPING RECV MESSAGE\n");
        return true;
    }

    // The following code has the unintended side-effect of adding
    // seed nodes twice. I'm leaving these comments to remind me
    // to deal with seed nodes' being discovered twice.
    // static const char *(*strOnionSeed)[1] = fTestNet ? strTestNetOnionSeed : strMainNetOnionSeed;

    // seed nodes have license to send version as much as they want
    // bool isOnionSeed;
    // isOnionSeed = false;

    // for (unsigned int seed_idx = 0; strOnionSeed[seed_idx][0] != NULL; seed_idx++) {
    //   if (pfrom->addr.ToString().c_str() == strOnionSeed[seed_idx][0]) {
    //         isOnionSeed = true;
    //         break;
    //   }
    // }

    // if ((strCommand == "version") && (isOnionSeed == false))
    if (strCommand == "version")
    {
        // Each connection can only send one version message
        if (pfrom->nVersion != 0)
        {
            pfrom->Misbehaving(1);
            return false;
        }

        int64_t nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64_t nNonce = 1;
        uint64_t verification_token = 0;
        vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime >> addrMe;
        if (pfrom->nVersion < GetMinPeerProtoVersion(nBestHeight))
        {
            // disconnect from peers older than this proto version
            printf("partner %s using obsolete version %i; disconnecting\n",
                                 pfrom->addr.ToString().c_str(), pfrom->nVersion);
            pfrom->fDisconnect = true;
            return false;
        }

        if (!vRecv.empty())
            vRecv >> addrFrom >> verification_token >> nNonce;
        if (!vRecv.empty()) {
            vRecv >> pfrom->strSubVer;
            pfrom->cleanSubVer = SanitizeString(pfrom->strSubVer);
        }
        if (!vRecv.empty())
            vRecv >> pfrom->nStartingHeight;

        if (pfrom->fInbound && addrMe.IsRoutable())
        {
            pfrom->addrLocal = addrMe;
            SeenLocal(addrMe);
        }

        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1)
        {
            printf("connected to self at %s, disconnecting\n", pfrom->addr.ToString().c_str());
            pfrom->fDisconnect = true;
            return true;
        }

        // ppcoin: record my external IP reported by peer
        if (addrFrom.IsRoutable() && addrMe.IsRoutable())
            addrSeenByPeer = addrMe;

        // Be shy and don't send version until we hear
        if (fDebug) {
              printf("ProcessMessage(): %s\n", pfrom->addr.ToString().c_str());
        }
        if (pfrom->fInbound)
            pfrom->PushVersion();

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

        AddTimeData(pfrom->addr, nTime);

        // Change version
        pfrom->PushMessage("verack");
        pfrom->vSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        if (!pfrom->fInbound)
        {
            // Advertise our address
            if (!IsInitialBlockDownload())
            {
                CAddress addr = GetLocalAddress(&pfrom->addr);
                if (addr.IsRoutable())
                    pfrom->PushAddress(addr);
            }

            // Get recent addresses
            if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION || addrman.size() < 1000)
            {
                pfrom->PushMessage("getaddr");
                pfrom->fGetAddr = true;
            }
            addrman.Good(pfrom->addr);
        } else {
            addrFrom.SetPort(GetDefaultPort());
            pfrom->addr = addrFrom;
            if (CNode::IsBanned(addrFrom))
            {
                printf("connection from %s dropped (banned)\n", addrFrom.ToString().c_str());
                pfrom->fDisconnect = true;
                return true;
            }
            if (addrman.CheckVerificationToken(addrFrom, verification_token)) {
                printf("connection from %s verified\n", addrFrom.ToString().c_str());
                pfrom->fVerified = true;
                addrman.Good(pfrom->addr);
            } else {
                printf("couldn't verify %s\n", addrFrom.ToString().c_str());
                addrman.SetReconnectToken(addrFrom, verification_token);
            }

        }

        // Ask the first connected node for block updates
        static int nAskedForBlocks = 0;
        if (!pfrom->fClient && !pfrom->fOneShot &&
            (pfrom->nStartingHeight > (nBestHeight - 144)) &&
            (pfrom->nVersion < NOBLKS_VERSION_START ||
             pfrom->nVersion >= NOBLKS_VERSION_END) &&
             (nAskedForBlocks < 1 || vNodes.size() <= 1))
        {
            nAskedForBlocks++;
            pfrom->PushGetBlocks(pindexBest, uint256(0));
        }

        // Relay alerts
        {
            LOCK(cs_mapAlerts);
            BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
                item.second.RelayTo(pfrom);
        }

        // ppcoin: relay sync-checkpoint
        {
            LOCK(Checkpoints::cs_hashSyncCheckpoint);
            if (!Checkpoints::checkpointMessage.IsNull())
                Checkpoints::checkpointMessage.RelayTo(pfrom);
        }

        pfrom->fSuccessfullyConnected = true;

        printf("receive version message: %s: version %d, blocks=%d, us=%s, them=%s, peer=%s, verification=%" PRId64 "\n", pfrom->cleanSubVer.c_str(), pfrom->nVersion, pfrom->nStartingHeight, addrMe.ToString().c_str(), addrFrom.ToString().c_str(), pfrom->addr.ToString().c_str(), verification_token);

        cPeerBlockCounts.input(pfrom->nStartingHeight);

        // ppcoin: ask for pending sync-checkpoint if any
        if (!IsInitialBlockDownload())
            Checkpoints::AskForPendingSyncCheckpoint(pfrom);
    }

    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else
        pfrom->Misbehaving(1);
        return false;
    }


    else if (strCommand == "verack")
    {
        pfrom->vRecv.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
    }


    else if (strCommand == "addr")
    {
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        // Don't want addr from older versions unless seeding
        if (pfrom->nVersion < CADDR_TIME_VERSION && addrman.size() > 1000)
            return true;
        if (vAddr.size() > 1000)
        {
            pfrom->Misbehaving(20);
            return error("message addr size() = %" PRIszu "", vAddr.size());
        }

        // Store the new addresses
        vector<CAddress> vAddrOk;
        int64_t nNow = GetAdjustedTime();
        int64_t nSince = nNow - 10 * 60;
        BOOST_FOREACH(CAddress& addr, vAddr)
        {
            if (fShutdown)
                return true;
            if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
                addr.nTime = nNow - 5 * 24 * 60 * 60;
            pfrom->AddAddressKnown(addr);
            bool fReachable = IsReachable(addr);
            if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable())
            {
                // Relay to a limited number of other nodes
                {
                    LOCK(cs_vNodes);
                    // Use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the setAddrKnowns of the chosen nodes prevent repeats
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint64_t hashAddr = addr.GetHash();
                    uint256 hashRand = hashSalt ^ (hashAddr<<32) ^ ((GetTime()+hashAddr)/(24*60*60));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    multimap<uint256, CNode*> mapMix;
                    BOOST_FOREACH(CNode* pnode, vNodes)
                    {
                        if (pnode->nVersion < CADDR_TIME_VERSION)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = hashRand ^ nPointer;
                        hashKey = Hash(BEGIN(hashKey), END(hashKey));
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    int nRelayNodes = fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(addr);
                }
            }
            // Do not store addresses outside our network
            if (fReachable)
                vAddrOk.push_back(addr);
        }
        addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
        if (pfrom->fOneShot)
            pfrom->fDisconnect = true;
    }


    else if (strCommand == "inv")
    {
        if (fDebugNet)
        {
            printf("ProcessMessage(): inv\n");
        }
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            pfrom->Misbehaving(20);
            return error("message inv size() = %" PRIszu "", vInv.size());
        }

        // find last block in inv vector
        unsigned int nLastBlock = (unsigned int)(-1);
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
        {
            if (vInv[vInv.size() - 1 - nInv].type == MSG_BLOCK)
            {
                nLastBlock = vInv.size() - 1 - nInv;
                break;
            }
        }
        CTxDB txdb("r");
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
        {
            const CInv &inv = vInv[nInv];

            if (fShutdown)
            {
                return true;
            }
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(txdb, inv);
            if (fDebugNet)
            {
                printf("  got inventory: %s  %s\n",
                       inv.ToString().c_str(),
                       fAlreadyHave ? "have" : "new");
            }

            if (!fAlreadyHave)
            {
                if (fDebugNet)
                {
                    printf("  not already have %s\n", inv.ToString().c_str());
                }
                pfrom->AskFor(inv);
            }
            else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash))
            {
                if (fDebugNet)
                {
                    printf("  map orphans count %s inv.hash\n",
                                            inv.ToString().c_str());
                }
                pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(mapOrphanBlocks[inv.hash]));
            }
            else if (nInv == nLastBlock)
            {
                if (fDebugNet)
                {
                    printf("  inv is last block %s\n", inv.ToString().c_str());
                }
                // In case we are on a very long side-chain, it is possible that we already have
                // the last block in an inv bundle sent in response to getblocks. Try to detect
                // this situation and push another getblocks to continue.
                pfrom->PushGetBlocks(mapBlockIndex[inv.hash], uint256(0));
                if (fDebugNet)
                {
                    printf("  force request: %s\n", inv.ToString().c_str());
                }
            }

            // Track requests for our stuff
            Inventory(inv.hash);
        }
    }


    else if (strCommand == "getdata")
    {
        if (fDebugNet)
        {
            printf("received getdata from %s\n",
                   pfrom->addr.ToString().c_str());
        }
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            pfrom->Misbehaving(20);
            return error("message getdata size() = %" PRIszu "", vInv.size());
        }

        if (fDebugNet || (vInv.size() != 1))
        {
            printf("   (%" PRIszu " invsz)\n", vInv.size());
        }

        BOOST_FOREACH(const CInv& inv, vInv)
        {
            if (fShutdown)
                return true;
            if (fDebugNet || (vInv.size() == 1))
            {
                printf("   for: %s\n", inv.ToString().c_str());
            }

            if (inv.type == MSG_BLOCK)
            {
                // Send block from disk
                map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end())
                {
                    CBlock block;
                    block.ReadFromDisk((*mi).second);
                    if (fDebugNet)
                    {
                        printf("   pushing block message to %s\n     %s\n",
                               pfrom->addr.ToString().c_str(),
                               block.GetHash().ToString().c_str());
                    }
                    pfrom->PushMessage("block", block);

                    // Trigger them to send a getblocks request for the next batch of inventory
                    if (inv.hash == pfrom->hashContinue)
                    {
                        // ppcoin: send latest proof-of-work block to allow the
                        // download node to accept as orphan (proof-of-stake
                        // block might be rejected by stake connection check)
                        vector<CInv> vInv;
                        vInv.push_back(CInv(MSG_BLOCK, GetLastBlockIndex(pindexBest, false)->GetBlockHash()));
                        pfrom->PushMessage("inv", vInv);
                        pfrom->hashContinue = 0;
                    }
                }
            }
            else if (inv.IsKnownType())
            {
                // Send stream from relay memory
                bool pushed = false;
                {
                    LOCK(cs_mapRelay);
                    map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end()) {
                        pfrom->PushMessage(inv.GetCommand(), (*mi).second);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_TX) {
                        LOCK(mempool.cs);
                        if (mempool.exists(inv.hash)) {
                           CTransaction tx = mempool.lookup(inv.hash);
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000);
                        ss << tx;
                        pfrom->PushMessage("tx", ss);
                    }
                }
            }

            // Track requests for our stuff
            Inventory(inv.hash);
        }
    }


    else if (strCommand == "getblocks")
    {
        if (pfrom->nVersion < GetMinPeerProtoVersion(nBestHeight))
        {
            printf("partner %s using obsolete version %i; disconnecting\n", pfrom->addr.ToString().c_str(), pfrom->nVersion);
            pfrom->fDisconnect = true;
            return false;
       }

        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        // Find the last block the caller has in the main chain
        CBlockIndex* pindex = locator.GetBlockIndex();

        // Send the rest of the chain
        if (pindex)
            pindex = pindex->pnext;
        int nLimit = GETBLOCKS_LIMIT;

        printf("getblocks %s: %d to %s limit %d\n",   // asdf
               pfrom->addr.ToString().c_str(),        // asdf
               (pindex ? pindex->nHeight : -1),       // asdf
               hashStop.ToString().c_str(), nLimit);  // asdf

        for (; pindex; pindex = pindex->pnext)
        {
            if (pindex->GetBlockHash() == hashStop)
            {
                printf("  getblocks stopping at %d %s\n",
                       pindex->nHeight,
                       pindex->GetBlockHash().ToString().c_str());   // asdf

                // ppcoin: tell downloading node about the latest block if it's
                // without risk of being rejected due to stake connection check
                if ((GetFork(pindexBest->nHeight) >= XST_FORKASDF) &&
                    (hashStop != hashBestChain) &&
                    (pindex->GetBlockTime() + nStakeMinAge > pindexBest->GetBlockTime()))
                {
                    pfrom->PushInventory(CInv(MSG_BLOCK, hashBestChain));
                }
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
            if (--nLimit <= 0)
            {
                printf("  getblocks stopping at limit %d %s\n",
                       pindex->nHeight,
                       pindex->GetBlockHash().ToString().c_str());  // asdf

                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    }
    else if (strCommand == "checkpoint")
    {
        CSyncCheckpoint checkpoint;
        vRecv >> checkpoint;

        if (checkpoint.ProcessSyncCheckpoint(pfrom))
        {
            // Relay
            pfrom->hashCheckpointKnown = checkpoint.hashCheckpoint;
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
                checkpoint.RelayTo(pnode);
        }
    }

    else if (strCommand == "getheaders")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        CBlockIndex* pindex = NULL;
        if (locator.IsNull())
        {
            // If locator is null, return the hashStop block
            map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashStop);
            if (mi == mapBlockIndex.end())
                return true;
            pindex = (*mi).second;
        }
        else
        {
            // Find the last block the caller has in the main chain
            pindex = locator.GetBlockIndex();
            if (pindex)
                pindex = pindex->pnext;
        }

        vector<CBlock> vHeaders;
        int nLimit = 2000;
        printf("getheaders %d to %s\n", (pindex ? pindex->nHeight : -1), hashStop.ToString().c_str());
        for (; pindex; pindex = pindex->pnext)
        {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
        }
        pfrom->PushMessage("headers", vHeaders);
    }


    else if (strCommand == "tx")
    {
        vector<uint256> vWorkQueue;
        vector<uint256> vEraseQueue;
        CDataStream vMsg(vRecv);
        CTxDB txdb("r");
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        bool fMissingInputs = false;

        if (tx.AcceptToMemoryPool(txdb, true, &fMissingInputs))
        {
            SyncWithWallets(tx, NULL, true);
            RelayTransaction(tx, inv.hash);
            mapAlreadyAskedFor.erase(inv);
            vWorkQueue.push_back(inv.hash);
            vEraseQueue.push_back(inv.hash);

            // Recursively process any orphan transactions that depended on this one
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hashPrev = vWorkQueue[i];
                for (map<uint256, CDataStream*>::iterator mi = mapOrphanTransactionsByPrev[hashPrev].begin();
                     mi != mapOrphanTransactionsByPrev[hashPrev].end();
                     ++mi)
                {
                    const CDataStream& vMsg = *((*mi).second);
                    CTransaction tx;
                    CDataStream(vMsg) >> tx;
                    CInv inv(MSG_TX, tx.GetHash());
                    bool fMissingInputs2 = false;

                    if (tx.AcceptToMemoryPool(txdb, true, &fMissingInputs2))
                    {
                        printf("   accepted orphan tx %s\n", inv.hash.ToString().c_str());
                        SyncWithWallets(tx, NULL, true);
                        RelayMessage(inv, vMsg);
                        mapAlreadyAskedFor.erase(inv);
                        vWorkQueue.push_back(inv.hash);
                        vEraseQueue.push_back(inv.hash);
                    }
                    else if (!fMissingInputs2)
                    {
                        // invalid orphan
                        vEraseQueue.push_back(inv.hash);
                        printf("   removed invalid orphan tx %s\n", inv.hash.ToString().c_str());
                    }
                }
            }

            BOOST_FOREACH(uint256 hash, vEraseQueue)
                EraseOrphanTx(hash);
        }
        else if (fMissingInputs)
        {
            AddOrphanTx(vMsg);

            // DoS prevention: do not allow mapOrphanTransactions to grow unbounded
            unsigned int nEvicted = LimitOrphanTxSize(MAX_ORPHAN_TRANSACTIONS);
            if (nEvicted > 0)
                printf("mapOrphan overflow, removed %u tx\n", nEvicted);
        }
        if (tx.nDoS) pfrom->Misbehaving(tx.nDoS);
    }


    else if (strCommand == "block")
    {
        CBlock block;
        vRecv >> block;

        printf("Received block %s from %s asdf\n",
                           block.GetHash().ToString().c_str(),
                           pfrom->addrName.c_str());
        // block.print();

        CInv inv(MSG_BLOCK, block.GetHash());
        pfrom->AddInventoryKnown(inv);

        bool fProcessOK;

        if ((GetFork(block.nHeight) < XST_FORKASDF) ||
            (block.hashPrevBlock == hashBestChain))
        {
            printf("asdf processing block fully 101\n");
            fProcessOK = ProcessBlock(pfrom, &block);
        }
        else
        {
            // Check nonsequential blocks as much as possible
            // to mitigate certain types of spam attacks.
            // A qPoS block can only fully validate if the registry is synced
            // with the block's immediate predecessor.
            // This full validation happens uppon connecting the block.
            printf("asdf processing block just check 102\n");
            fProcessOK = ProcessBlock(pfrom, &block, false, true);
        }

        // asdf asdf should this be recursive somehow?
        if (fProcessOK)
        {
            mapAlreadyAskedFor.erase(inv);
        }
        else if (pregistryMain->ShouldRollback())
        {
            if (Rollback())
            {
                // going to reprocess block so reset its nDoS
                block.nDoS = 0;
                if ((GetFork(block.nHeight) < XST_FORKASDF) ||
                    (block.nHeight == (nBestHeight + 1)))
                {
                    printf("asdf processing block fully 103\n");
                    fProcessOK = ProcessBlock(pfrom, &block);
                }
                else
                {
                    printf("asdf processing block just check 104\n");
                    fProcessOK = ProcessBlock(pfrom, &block, false, true);
                }
                if (fProcessOK)
                {
                    mapAlreadyAskedFor.erase(inv);
                }
            }
            else
            {
                // should never happen
                throw std::runtime_error(
                        "ProcessMessage(): ERROR: couldn't roll back");
            }
        }

        if (block.nDoS)
        {
            pfrom->Misbehaving(block.nDoS);
        }

        if (pfrom->nOrphans > 2 * GETBLOCKS_LIMIT)
        {
            printf("Node has exceeded max init download orphans.\n");
            pfrom->Misbehaving(100);
        }
    }


    else if (strCommand == "getaddr")
    {
        pfrom->vAddrToSend.clear();
        vector<CAddress> vAddr = addrman.GetAddr();
        BOOST_FOREACH(const CAddress &addr, vAddr)
            pfrom->PushAddress(addr);
    }


    else if (strCommand == "mempool")
    {
        std::vector<uint256> vtxid;
        mempool.queryHashes(vtxid);
        vector<CInv> vInv;
        for (unsigned int i = 0; i < vtxid.size(); i++) {
            CInv inv(MSG_TX, vtxid[i]);
            vInv.push_back(inv);
            if (i == (MAX_INV_SZ - 1))
                    break;
        }
        if (vInv.size() > 0)
            pfrom->PushMessage("inv", vInv);
    }


    else if (strCommand == "checkorder")
    {
        uint256 hashReply;
        vRecv >> hashReply;

        if (!GetBoolArg("-allowreceivebyip"))
        {
            pfrom->PushMessage("reply", hashReply, (int)2, string(""));
            return true;
        }

        CWalletTx order;
        vRecv >> order;

        /// we have a chance to check the order here

        // Keep giving the same key to the same ip until they use it
        if (!mapReuseKey.count(pfrom->addr))
            pwalletMain->GetKeyFromPool(mapReuseKey[pfrom->addr], true);

        // Send back approval of order and pubkey to use
        CScript scriptPubKey;
        scriptPubKey << mapReuseKey[pfrom->addr] << OP_CHECKSIG;
        pfrom->PushMessage("reply", hashReply, (int)0, scriptPubKey);
    }


    else if (strCommand == "reply")
    {
        uint256 hashReply;
        vRecv >> hashReply;

        CRequestTracker tracker;
        {
            LOCK(pfrom->cs_mapRequests);
            map<uint256, CRequestTracker>::iterator mi = pfrom->mapRequests.find(hashReply);
            if (mi != pfrom->mapRequests.end())
            {
                tracker = (*mi).second;
                pfrom->mapRequests.erase(mi);
            }
        }
        if (!tracker.IsNull())
            tracker.fn(tracker.param1, vRecv);
    }


    else if (strCommand == "ping")
    {
        if (pfrom->nVersion > BIP0031_VERSION)
        {
            uint64_t nonce = 0;
            vRecv >> nonce;
            // Echo the message back with the nonce. This allows for two useful features:
            //
            // 1) A remote node can quickly check if the connection is operational
            // 2) Remote nodes can measure the latency of the network thread. If this node
            //    is overloaded it won't respond to pings quickly and the remote node can
            //    avoid sending us more work, like chain download requests.
            //
            // The nonce stops the remote getting confused between different pings: without
            // it, if the remote node sends a ping once per second and this node takes 5
            // seconds to respond to each, the 5th ping the remote sends would appear to
            // return very quickly.
            pfrom->PushMessage("pong", nonce);
        }
    }


    else if (strCommand == "alert")
    {
        CAlert alert;
        vRecv >> alert;

        uint256 alertHash = alert.GetHash();
        if (pfrom->setKnown.count(alertHash) == 0)
        {
            if (alert.ProcessAlert())
            {
                // Relay
                pfrom->setKnown.insert(alertHash);
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH(CNode* pnode, vNodes)
                        alert.RelayTo(pnode);
                }
            }
            else {
                // Small DoS penalty so peers that send us lots of
                // duplicate/expired/invalid-signature/whatever alerts
                // eventually get banned.
                // This isn't a Misbehaving(100) (immediate ban) because the
                // peer might be an older or different implementation with
                // a different signature key, etc.
                pfrom->Misbehaving(10);
            }
        }
    }


    else
    {
        // Ignore unknown commands for extensibility
    }


    // Update the last seen time for this node's address
    if (pfrom->fNetworkNode)
        if (strCommand == "version" || strCommand == "addr" || strCommand == "inv" || strCommand == "getdata" || strCommand == "ping")
            AddressCurrentlyConnected(pfrom->addr);


    return true;
}

bool ProcessMessages(CNode* pfrom)
{
    // if (fDebug) {
    //       printf("ProcessMessages: %s\n",
    //              pfrom->addr.ToString().c_str());
    // }
    CDataStream& vRecv = pfrom->vRecv;
    if (vRecv.empty()) {
        // if (fDebug)
        // {
        //    printf("ProcessMessages: %s [empty]\n",
        //              pfrom->addr.ToString().c_str());
        // }
        return true;
    }

    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //

    LOOP
    {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->vSend.size() >= SendBufferSize())
            break;

        // Scan for message start
        CDataStream::iterator pstart = search(vRecv.begin(), vRecv.end(), BEGIN(pchMessageStart), END(pchMessageStart));
        int nHeaderSize = vRecv.GetSerializeSize(CMessageHeader());
        if (vRecv.end() - pstart < nHeaderSize)
        {
            if ((int)vRecv.size() > nHeaderSize)
            {
                printf("\n\nPROCESSMESSAGE MESSAGESTART NOT FOUND\n\n");
                vRecv.erase(vRecv.begin(), vRecv.end() - nHeaderSize);
            }
            break;
        }
        if (pstart - vRecv.begin() > 0)
            printf("\n\nPROCESSMESSAGE SKIPPED %" PRIpdd " BYTES\n\n", pstart - vRecv.begin());
        vRecv.erase(vRecv.begin(), pstart);

        // Read header
        vector<char> vHeaderSave(vRecv.begin(), vRecv.begin() + nHeaderSize);
        CMessageHeader hdr;
        vRecv >> hdr;
        if (!hdr.IsValid())
        {
            printf("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", hdr.GetCommand().c_str());
            continue;
        }
        string strCommand = hdr.GetCommand();
        if (fDebug) {
              printf("ProcessMessages: %s [%s]\n",
                         pfrom->addr.ToString().c_str(), strCommand.c_str());
        }

        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;
        if (nMessageSize > MAX_SIZE)
        {
            printf("ProcessMessages(%s, %u bytes) : nMessageSize > MAX_SIZE\n", strCommand.c_str(), nMessageSize);
            continue;
        }
        if (nMessageSize > vRecv.size())
        {
            // Rewind and wait for rest of message
            vRecv.insert(vRecv.begin(), vHeaderSave.begin(), vHeaderSave.end());
            break;
        }

        // Checksum
        uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        if (nChecksum != hdr.nChecksum)
        {
            printf("ProcessMessages(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n",
               strCommand.c_str(), nMessageSize, nChecksum, hdr.nChecksum);
            continue;
        }

        // Copy message to its own buffer
        CDataStream vMsg(vRecv.begin(), vRecv.begin() + nMessageSize, vRecv.nType, vRecv.nVersion);

        try
        {
            vRecv.ignore(nMessageSize);
        }
        catch (std::ios_base::failure& e)
        {
            // can only be end of data, should cause failure in processing below
            printf("ProcessMessages() : Exception '%s' caught, caused by unexpectedly reaching end of buffer\n", e.what());
        }

        // Process message
        bool fRet = false;
        try
        {
            {
                // order is important here to prevent deadlocks:
                //   first lock the registry, then cs_main
                boost::lock_guard<QPRegistry> guardRegistry(*pregistryMain);
                LOCK(cs_main);
                fRet = ProcessMessage(pfrom, strCommand, vMsg);
            }
            if (fShutdown)
                return true;
        }
        catch (std::ios_base::failure& e)
        {
            if (strstr(e.what(), "end of data"))
            {
                // Allow exceptions from under-length message on vRecv
                printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length\n", strCommand.c_str(), nMessageSize, e.what());
            }
            else if (strstr(e.what(), "size too large"))
            {
                // Allow exceptions from over-long size
                printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught\n", strCommand.c_str(), nMessageSize, e.what());
            }
            else
            {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        }
        catch (std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(NULL, "ProcessMessages()");
        }

        if (!fRet)
            printf("ProcessMessage(%s, %u bytes) FAILED\n", strCommand.c_str(), nMessageSize);
    }

    vRecv.Compact();
    return true;
}


bool SendMessages(CNode* pto, bool fSendTrickle)
{
    TRY_LOCK(cs_main, lockMain);
    if (lockMain) {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
        {
            return true;
        }

        // Keep-alive ping. We send a nonce of zero because we don't use it anywhere
        // right now.
        if (pto->nLastSend && GetTime() - pto->nLastSend > 30 * 60 && pto->vSend.empty()) {
            uint64_t nonce = 0;
            if (pto->nVersion > BIP0031_VERSION)
                pto->PushMessage("ping", nonce);
            else
                pto->PushMessage("ping");
        }

        // Resend wallet transactions that haven't gotten in a block yet
        ResendWalletTransactions();

        // Address refresh broadcast
        static int64_t nLastRebroadcast;
        if (!IsInitialBlockDownload() && (GetTime() - nLastRebroadcast > 24 * 60 * 60))
        {
            {
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                {
                    // Periodically clear setAddrKnown to allow refresh broadcasts
                    if (nLastRebroadcast)
                        pnode->setAddrKnown.clear();

                    // Rebroadcast our address
                    if (true)
                    {
                        CAddress addr = GetLocalAddress(&pnode->addr);
                        if (addr.IsRoutable())
                            pnode->PushAddress(addr);
                    }
                }
            }
            nLastRebroadcast = GetTime();
        }

        //
        // Message: addr
        //
        if (fSendTrickle)
        {
            vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            BOOST_FOREACH(const CAddress& addr, pto->vAddrToSend)
            {
                // returns true if wasn't already contained in the set
                if (pto->setAddrKnown.insert(addr).second)
                {
                    vAddr.push_back(addr);
                    // receiver rejects addr messages larger than 1000
                    if (vAddr.size() >= 1000)
                    {
                        pto->PushMessage("addr", vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("addr", vAddr);
        }


        //
        // Message: inventory
        //
        vector<CInv> vInv;
        vector<CInv> vInvWait;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            BOOST_FOREACH(const CInv& inv, pto->vInventoryToSend)
            {
                if (pto->setInventoryKnown.count(inv))
                    continue;

                // trickle out tx inv to protect privacy
                if (inv.type == MSG_TX && !fSendTrickle)
                {
                    // 1/4 of tx invs blast to all immediately
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint256 hashRand = inv.hash ^ hashSalt;
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    bool fTrickleWait = ((hashRand & 3) != 0);

                    // always trickle our own transactions
                    if (!fTrickleWait)
                    {
                        CWalletTx wtx;
                        if (GetTransaction(inv.hash, wtx))
                            if (wtx.fFromMe)
                                fTrickleWait = true;
                    }

                    if (fTrickleWait)
                    {
                        vInvWait.push_back(inv);
                        continue;
                    }
                }

                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second)
                {
                    vInv.push_back(inv);
                    if (vInv.size() >= 1000)
                    {
                        pto->PushMessage("inv", vInv);
                        vInv.clear();
                    }
                }
            }
            pto->vInventoryToSend = vInvWait;
        }
        if (!vInv.empty())
            pto->PushMessage("inv", vInv);


        //
        // Message: getdata
        //
        vector<CInv> vGetData;
        int64_t nNow = GetTime() * 1000000;
        CTxDB txdb("r");
        while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
        {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (AlreadyHave(txdb, inv))
            {
                printf("already have %s\n",
                       inv.ToString().c_str());
            }
            else
            {
                if (fDebugNet)
                {
                    printf("sending getdata to %s:\n   %s\n",
                           pto->addr.ToString().c_str(),
                           inv.ToString().c_str());
                }
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000)
                {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
                mapAlreadyAskedFor[inv] = nNow;
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);
    }
    return true;
}





//////////////////////////////////////////////////////////////////////////////
//
// StealthMinter
//

#ifdef WITH_MINER

int static FormatHashBlocks(void* pbuffer, unsigned int len)
{
    unsigned char* pdata = (unsigned char*)pbuffer;
    unsigned int blocks = 1 + ((len + 8) / 64);
    unsigned char* pend = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len] = 0x80;
    unsigned int bits = len * 8;
    pend[-1] = (bits >> 0) & 0xff;
    pend[-2] = (bits >> 8) & 0xff;
    pend[-3] = (bits >> 16) & 0xff;
    pend[-4] = (bits >> 24) & 0xff;
    return blocks;
}

static const unsigned int pSHA256InitState[8] =
                              {0x6a09e667, 0xbb67ae85,
                               0x3c6ef372, 0xa54ff53a,
                               0x510e527f, 0x9b05688c,
                               0x1f83d9ab, 0x5be0cd19};

void SHA256Transform(void* pstate, void* pinput, const void* pinit)
{
    SHA256_CTX ctx;
    unsigned char data[64];

    SHA256_Init(&ctx);

    for (int i = 0; i < 16; i++)
        ((uint32_t*)data)[i] = ByteReverse(((uint32_t*)pinput)[i]);

    for (int i = 0; i < 8; i++)
        ctx.h[i] = ((uint32_t*)pinit)[i];

    SHA256_Update(&ctx, data, sizeof(data));
    for (int i = 0; i < 8; i++)
        ((uint32_t*)pstate)[i] = ctx.h[i];
}

#endif  /* WITH_MINER */


// Some explaining would be appreciated
class COrphan
{
public:
    CTransaction* ptx;
    set<uint256> setDependsOn;
    double dPriority;
    double dFeePerKb;

    COrphan(CTransaction* ptxIn)
    {
        ptx = ptxIn;
        dPriority = dFeePerKb = 0;
    }

    void print() const
    {
        printf("COrphan(hash=%s, dPriority=%.1f, dFeePerKb=%.1f)\n",
               ptx->GetHash().ToString().c_str(), dPriority, dFeePerKb);
        BOOST_FOREACH(uint256 hash, setDependsOn)
            printf("   setDependsOn %s\n", hash.ToString().c_str());
    }
};


uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;
int64_t nLastCoinStakeSearchInterval = 0;

// We want to sort transactions by priority and fee, so:
typedef boost::tuple<double, double, CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;
public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) { }
    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee)
        {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        }
        else
        {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

// asdf: this is in a thread, need to pass the pointer to the registry

// WARNING: pblockRet is passed in uninitialized
BlockCreationResult CreateNewBlock(CWallet* pwallet,
                                   ProofTypes fTypeOfProof,
                                   AUTO_POINTER<CBlock> &pblockRet)
{
    bool fProofOfStake;
    bool fQuantumPoS;

    switch (fTypeOfProof)
    {
        case PROOFTYPE_POW:
            fProofOfStake = false;
            fQuantumPoS = false;
            break;
        case PROOFTYPE_POS:
            fProofOfStake = true;
            fQuantumPoS = false;
            break;
        case PROOFTYPE_QPOS:
            fQuantumPoS = true;
            fProofOfStake = false;
            break;
        default:
            // asdf raise something?
            printf ("CreateNewBlock(): No such type of proof\n");
            return BLOCKCREATION_PROOFTYPE_FAIL;
    }

    CReserveKey reservekey(pwallet);

    CBlockIndex* pindexPrev = pindexBest;

    int nHeight = pindexPrev->nHeight+1;  // height of new block

    if (fQuantumPoS)
    {
        if (pregistryMain->IsInReplayMode())
        {
            return BLOCKCREATION_QPOS_IN_REPLAY;
        }

        unsigned int nID;
        bool fShould = pregistryMain->GetIDForCurrentTime(pindexBest, nID);
        if (nID == 0)
        {
            // should never happen when not in replay
            printf("CreateNewBlock(): Registry failed with 0 ID\n");
            return BLOCKCREATION_REGISTRY_FAIL;
        }
        if (!fShould)
        {
            return BLOCKCREATION_QPOS_BLOCK_EXISTS;
        }

        CPubKey pubkey;
        if (!pregistryMain->GetDelegateKey(nID, pubkey))
        {
            return BLOCKCREATION_REGISTRY_FAIL;
        }
        if (!pwallet->HaveKey(pubkey.GetID()))
        {
            return BLOCKCREATION_NOT_CURRENTSTAKER;
        }

        pblockRet->nStakerID = nID;
        pblockRet->nHeight = nHeight;
        pblockRet->nTime = static_cast<unsigned int>(GetAdjustedTime());
    }
    else
    {
        // Create coinbase tx
        CTransaction txNew;
        txNew.vin.resize(1);
        txNew.vin[0].prevout.SetNull();
        txNew.vout.resize(1);
        txNew.vout[0].scriptPubKey << reservekey.GetReservedKey() << OP_CHECKSIG;
        // Add our coinbase tx as first transaction
        pblockRet->vtx.push_back(txNew);
    }

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", MAX_BLOCK_SIZE_GEN/2);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE-1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", 27000);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", 0);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Fee-per-kilobyte amount considered the same as "free"
    // Be careful setting this: if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    int64_t nMinTxFee = MIN_TX_FEE;
    if (mapArgs.count("-mintxfee"))
    {
        ParseMoney(mapArgs["-mintxfee"], nMinTxFee);
    }


    int nFork = GetFork(nHeight);

    // ppcoin: if coinstake available add coinstake tx
    // only initialized at startup

    unsigned int nCoinStakeTime = (unsigned int) GetAdjustedTime();

    if (fProofOfStake)  // attempt to find a coinstake
    {
        static unsigned int nLastCoinStakeSearchTime = (unsigned int) GetAdjustedTime();
        pblockRet->nBits = GetNextTargetRequired(pindexPrev, true);
        CTransaction txCoinStake;

        // Upon XST_FORK006, transactions don't have meaningful timestamps.
        // However to use the logic of this function for now, nCoinStakeTime will
        // be used to hold the block timestamp. Upon XST_FORK006, the block
        // timestamp will be a permanent record of its transaction timestamps.
        if (txCoinStake.HasTimestamp())
        {
            txCoinStake.SetTxTime(nCoinStakeTime);
        }

        unsigned int nSearchTime = nCoinStakeTime; // search to current time
        if (nSearchTime > nLastCoinStakeSearchTime)
        {
            if (pwallet->CreateCoinStake(*pwallet, pblockRet->nBits,
                           nSearchTime - nLastCoinStakeSearchTime, txCoinStake, nCoinStakeTime))
            {
                unsigned int nTimeMax;
                if (nFork < XST_FORK005)
                {
                    nTimeMax = max(pindexPrev->GetPastTimeLimit()+1, pindexPrev->GetBlockTime() - nMaxClockDrift);
                }
                else
                {
                    if (nFork >= XST_FORK006)
                    {
                        pblockRet->nTime = nCoinStakeTime;
                    }
                    nTimeMax = max(pblockRet->GetBlockTime(), pindexPrev->GetBlockTime());
                }

                if (nCoinStakeTime >= nTimeMax)
                {   // make sure coinstake would meet timestamp protocol
                    // as it would be the same as the block timestamp
                    pblockRet->vtx[0].vout[0].SetEmpty();
                    // this test simply marks that assinging tx ntime upon creation
                    //        will be eliminated in the future
                    if (pblockRet->vtx[0].HasTimestamp())
                    {
                        pblockRet->vtx[0].SetTxTime(nCoinStakeTime);
                    }
                    pblockRet->vtx.push_back(txCoinStake);
                }
            }
            nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
            nLastCoinStakeSearchTime = nSearchTime;
        }
    }


    if (!fQuantumPoS)
    {
        pblockRet->nBits = GetNextTargetRequired(pindexPrev,
                                                 pblockRet->IsProofOfStake());
    }


    // Collect memory pool transactions into the block
    int64_t nFees = 0;
    {
        LOCK2(cs_main, mempool.cs);
        CBlockIndex* pindexPrev = pindexBest;
        CTxDB txdb("r");

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (map<uint256, CTransaction>::iterator mi = mempool.mapTx.begin(); mi != mempool.mapTx.end(); ++mi)
        {
            CTransaction& tx = (*mi).second;
            if (tx.IsCoinBase() || tx.IsCoinStake() || !tx.IsFinal())
            {
                continue;
            }

            COrphan* porphan = NULL;
            double dPriority = 0;
            int64_t nTotalIn = 0;
            bool fMissingInputs = false;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                // Read prev transaction
                CTransaction txPrev;
                CTxIndex txindex;
                if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex))
                {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash))
                    {
                        printf("ERROR: mempool transaction missing input\n");
                        if (fDebug) assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan)
                    {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx[txin.prevout.hash].vout[txin.prevout.n].nValue;
                    continue;
                }
                int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

                int nConf = txindex.GetDepthInMainChain();
                dPriority += (double)nValueIn * nConf;
            }
            if (fMissingInputs) continue;

            // Priority is sum(valuein * age) / txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority /= nTxSize;

            // This is a more accurate fee-per-kilobyte than is used by the client code, because the
            // client code rounds up the size to the nearest 1K. That's good, because it gives an
            // incentive to create smaller transactions.
            double dFeePerKb =  double(nTotalIn-tx.GetValueOut()) / (double(nTxSize)/1000.0);

            if (porphan)
            {
                porphan->dPriority = dPriority;
                porphan->dFeePerKb = dFeePerKb;
            }
            else
                vecPriority.push_back(TxPriority(dPriority, dFeePerKb, &(*mi).second));
        }

        // Collect transactions into block
        map<uint256, CTxIndex> mapTestPool;
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        while (!vecPriority.empty())
        {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            double dFeePerKb = vecPriority.front().get<1>();
            CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = tx.GetLegacySigOpCount();
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Timestamp limit
            if (tx.HasTimestamp())
            {
                if ( (tx.GetTxTime() > GetAdjustedTime()) ||
                     ( pblockRet->IsProofOfStake() &&
                       (pblockRet->vtx[1].HasTimestamp()) &&
                       (tx.GetTxTime() > pblockRet->vtx[1].GetTxTime()) ) )
                {
                    continue;
                }
            }

            // ppcoin: simplify transaction fee - allow free = false
            int64_t nMinFee = tx.GetMinFee(nBlockSize, false, GMF_BLOCK);

            // Skip free transactions if we're past the minimum block size:
            if (fSortedByFee && (dFeePerKb < nMinTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritize by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || (dPriority < COIN * 144 / 250)))
            {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            // Connecting shouldn't fail due to dependency on other memory pool transactions
            // because we're already processing them in order of dependency
            map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
            MapPrevTx mapInputs;
            bool fInvalid;
            if (!tx.FetchInputs(txdb, mapTestPoolTmp, false, true, mapInputs, fInvalid))
            {
                continue;
            }

            map<string, qpos_purchase> mapPurchases;
            int64_t nStakerPrice = GetStakerPrice(pregistryMain, pindexBest);
            if (!tx.CheckPurchases(pregistryMain, nStakerPrice, mapPurchases))
            {
                return BLOCKCREATION_PURCHASE_FAIL;
            }
            map<string, qpos_purchase>::const_iterator kt;
            int64_t nValuePurchases = 0;
            for (kt = mapPurchases.begin(); kt != mapPurchases.end(); ++kt)
            {
                nValuePurchases += kt->second.value;
            }

            qpos_claim claim;
            if (!tx.CheckClaim(pregistryMain, mapInputs, claim))
            {
                return BLOCKCREATION_CLAIM_FAIL;
            }

            int64_t nTxFees = tx.GetValueIn(mapInputs) - tx.GetValueOut();
            if (nTxFees < nMinFee)
            {
                continue;
            }

            nTxSigOps += tx.GetP2SHSigOpCount(mapInputs);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            {
                continue;
            }

            if (!tx.ConnectInputs(txdb, mapInputs, mapTestPoolTmp,
                                  CDiskTxPos(1,1,1), pindexPrev, false, true,
                                  STANDARD_SCRIPT_VERIFY_FLAGS,
                                  nValuePurchases, claim.value))
            {
                continue;
            }
            mapTestPoolTmp[tx.GetHash()] = CTxIndex(CDiskTxPos(1,1,1), tx.vout.size());
            swap(mapTestPool, mapTestPoolTmp);

            // Added
            pblockRet->vtx.push_back(tx);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fDebug && GetBoolArg("-printpriority"))
            {
                printf("priority %.1f feeperkb %.1f txid %s\n",
                       dPriority, dFeePerKb, tx.GetHash().ToString().c_str());
            }

            // Add transactions that depend on this one to the priority queue
            uint256 hash = tx.GetHash();
            if (mapDependers.count(hash))
            {
                BOOST_FOREACH(COrphan* porphan, mapDependers[hash])
                {
                    if (!porphan->setDependsOn.empty())
                    {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty())
                        {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->dFeePerKb, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;

        if (fDebug && GetBoolArg("-printpriority"))
        {
            printf("CreateNewBlock(): total size %" PRIu64 "\n", nBlockSize);
        }

        if (pblockRet->IsProofOfWork())
        {
            pblockRet->vtx[0].vout[0].nValue = GetProofOfWorkReward(nHeight,
                                                                    nFees);
        }

        // Fill in header
        pblockRet->hashPrevBlock = pindexPrev->GetBlockHash();
        if (pblockRet->IsProofOfStake())
        {
            pblockRet->nTime = nCoinStakeTime;
        }
        if (nFork < XST_FORK006)
        {
             pblockRet->nTime = max(pindexPrev->GetPastTimeLimit() + 1,
                                    pblockRet->GetMaxTransactionTime());
        }
        if (nFork < XST_FORKASDF)
        {
            pblockRet->nTime = max(pblockRet->GetBlockTime(),
                                   pindexPrev->GetPastTimeLimit() + 1);
        }
        else if (nFork < XST_FORK005)
        {
            pblockRet->nTime = max(pblockRet->GetBlockTime(),
                                   pindexPrev->GetBlockTime() - nMaxClockDrift);
        }
        if (pblockRet->IsProofOfWork())
        {
            pblockRet->UpdateTime(pindexPrev);
        }
        pblockRet->nNonce = 0;
    }

    return BLOCKCREATION_OK;
}

void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }

    ++nExtraNonce;

    // qPoS blocks don't have a coinbase transaction for vtx[0]
    if (!pblock->IsQuantumProofOfStake())
    {
        // Height first in coinbase required for block.version=2
        unsigned int nHeight = pindexPrev->nHeight+1;
        pblock->vtx[0].vin[0].scriptSig = (CScript() << nHeight << CBigNum(nExtraNonce)) + COINBASE_FLAGS;
        assert(pblock->vtx[0].vin[0].scriptSig.size() <= 100);
    }

    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}


#ifdef WITH_MINER
void FormatHashBuffers(CBlock* pblock, char* pmidstate, char* pdata, char* phash1)
{
    //
    // Pre-build hash buffers
    //
    struct
    {
        struct unnamed2
        {
            int nVersion;
            uint256 hashPrevBlock;
            uint256 hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
        }
        block;
        unsigned char pchPadding0[64];
        uint256 hash1;
        unsigned char pchPadding1[64];
    }
    tmp;
    memset(&tmp, 0, sizeof(tmp));

    tmp.block.nVersion       = pblock->nVersion;
    tmp.block.hashPrevBlock  = pblock->hashPrevBlock;
    tmp.block.hashMerkleRoot = pblock->hashMerkleRoot;
    tmp.block.nTime          = pblock->nTime;
    tmp.block.nBits          = pblock->nBits;
    tmp.block.nNonce         = pblock->nNonce;

    FormatHashBlocks(&tmp.block, sizeof(tmp.block));
    FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

    // Byte swap all the input buffer
    for (unsigned int i = 0; i < sizeof(tmp)/4; i++)
        ((unsigned int*)&tmp)[i] = ByteReverse(((unsigned int*)&tmp)[i]);

    // Precalc the first half of the first hash, which stays constant
    SHA256Transform(pmidstate, &tmp.block, pSHA256InitState);

    memcpy(pdata, &tmp.block, 128);
    memcpy(phash1, &tmp.hash1, 64);
}
#endif  /* WITH_MINER */


bool CheckWork(CBlock* pblock, CWallet& wallet,
               CReserveKey& reservekey, const CBlockIndex* pindexPrev)
{
    //// debug print
    printf("CheckWork:\n");
    uint256 hash = pblock->GetHash();
    if (pblock->IsQuantumProofOfStake())
    {
        printf("  new qPoS block found\n  hash: %s\n", hash.GetHex().c_str());
        int64_t nReward = GetQPoSReward(pindexPrev);
        printf("  generated %s\n", FormatMoney(nReward).c_str());
    }
    else
    {
        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

        if (hash > hashTarget && pblock->IsProofOfWork())
            return error("CheckWork() : proof-of-work not meeting target");

        printf("  new block found\n  hash: %s\ntarget: %s\n", hash.GetHex().c_str(), hashTarget.GetHex().c_str());
        pblock->print();
        printf("  generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue).c_str());
    }

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != hashBestChain)
        {
            return error("CheckWork() : generated block is stale");
        }

        // Remove key from key pool
        // FIXME: not really necessary for qPoS
        reservekey.KeepKey();

        // Track how many getdata requests this block gets
        {
            LOCK(wallet.cs_wallet);
            wallet.mapRequestCount[pblock->GetHash()] = 0;
        }


        // Process this block the same as if we had received it from another node
// asdfjkl;
        // Stakers produce qPoS on contingency, meaning theyblock can only fully validate if the registry is synced
        // with the block's immediate predecessor.
        // This full validation happens uppon connecting the block.
//         printf("asdf processing block just check\n");  asdf
//         if (!ProcessBlock(NULL, pblock, false, true))  asdf
        // Process this block the same as if we had received it from another node
        bool fProcessOK = ProcessBlock(NULL, pblock);
        // rollbackstale should only be true for testnet in certain situations
        //   to simulate a network of staker nodes that mostly don't go down
        if (pregistryMain->ShouldRollback() &&
            GetBoolArg("-rollbackstale", true))
        {
            printf("CheckWork(): insufficient power, chain should rollback\n");
            fProcessOK = false;
            if (!Rollback())
            {
                // should never happen
                throw std::runtime_error(
                        "ProcessMessage(): ERROR: couldn't roll back");
            }
        }
        if (!fProcessOK)
        {
            return error("CheckWork() : ProcessBlock, block not accepted");
        }
    }
    return true;
}

#ifdef WITH_MINER
void static ThreadStealthMiner(void* parg);
#endif  /* WITH_MINER */

static bool fGenerateXST = false;

#ifdef WITH_MINER
static bool fLimitProcessors = false;
static int nLimitProcessors = -1;
#endif  /* WITH_MINER */

void StealthMinter(CWallet *pwallet, ProofTypes fTypeOfProof)
{
    bool fProofOfWork = false;
    bool fProofOfStake = false;
    bool fQuantumPoS = false;

    uint64_t nSleepInterval = 0;
    switch (fTypeOfProof)
    {
        case PROOFTYPE_POW:
            fProofOfWork = true;
            RenameThread("stealth-minter-pow");
            printf("CPUMinter started for proof-of-work\n");
            break;
        case PROOFTYPE_POS:
            nSleepInterval = 60000;
            fProofOfStake = true;
            RenameThread("stealth-minter-pos");
            printf("CPUMinter started for proof-of-stake\n");
            break;
        case PROOFTYPE_QPOS :
            nSleepInterval = 100;
            fQuantumPoS = true;
            RenameThread("stealth-minter-qpos");
            printf("CPUMinter started for qPoS\n");
            break;
        default:
            printf("StealthMinter(): bad proof type\n");
            return;
    }

    SetThreadPriority(THREAD_PRIORITY_LOWEST);

    // Each thread has its own key and counter
    CReserveKey reservekey(pwallet);
    unsigned int nExtraNonce = 0;


    while (fGenerateXST || fProofOfStake || fQuantumPoS)
    {
        if (fShutdown)
        {
            return;
        }

        // rollbacks mean qPoS can keep producing even with 0 connections
        while ((vNodes.empty() && !fQuantumPoS) ||
               IsInitialBlockDownload() ||
               pwallet->IsLocked())
        {
            nLastCoinStakeSearchInterval = 0;
            MilliSleep(1000);
            if (fShutdown)
            {
                return;
            }
            if (!fGenerateXST && !fProofOfStake && !fQuantumPoS)
            {
                return;
            }
        }

        while (pindexBest == NULL)
        {
            MilliSleep(1000);
        }

        //
        // Create new block
        //
#ifdef WITH_MINER
        unsigned int nTransactionsUpdatedLast = nTransactionsUpdated;
#endif  /* WITH_MINER */
        CBlockIndex* pindexPrev = pindexBest;

        int nHeight = pindexPrev->nHeight + 1;

        // QPoS doesn't start until XST_FORKASDF, so loop until ready
        if (fQuantumPoS && (GetFork(nHeight) < XST_FORKASDF))
        {
            if (GetFork(nHeight + 2) < XST_FORKASDF)
            {
                // save some CPUs until almost time
                MilliSleep(1000);
            }
            else
            {
                MilliSleep(100);
            }
            continue;
        }

        // PoS ends with XST_FORKASDF, so kill the PoS minter thread
        if (fProofOfStake && (GetFork(nHeight) >= XST_FORKASDF))
        {
            return;
        }

        // PoW ends with XST_FORK002, so kill any PoW minter thread
        if (fProofOfWork && (GetFork(nHeight) >= XST_FORK002))
        {
            return;
        }

        AUTO_POINTER<CBlock> pblock(new CBlock());

        if (!pblock.get())
        {
             return;
        }

        {  // lock registry
            boost::lock_guard<QPRegistry> guardRegistry(*pregistryMain);

            BlockCreationResult nResult = CreateNewBlock(pwallet,
                                                         fTypeOfProof,
                                                         pblock);

            if (nResult == BLOCKCREATION_QPOS_IN_REPLAY)
            {
                MilliSleep(nSleepInterval);
                continue;
            }
            if ((nResult == BLOCKCREATION_NOT_CURRENTSTAKER) ||
                (nResult == BLOCKCREATION_QPOS_BLOCK_EXISTS))
            {
                if (fDebugQPoS)
                {
                    printf("block creation abandoned with \"%s\"\n",
                           DescribeBlockCreationResult(nResult));
                }
                MilliSleep(nSleepInterval);
                continue;
            }

            if (nResult == BLOCKCREATION_INSTANTIATION_FAIL ||
                nResult == BLOCKCREATION_REGISTRY_FAIL)
            {
                if (fDebugQPoS)
                {
                    printf("block creation catastrophic fail with \"%s\"\n",
                           DescribeBlockCreationResult(nResult));
                }
                return;
            }

            IncrementExtraNonce(pblock.get(), pindexPrev, nExtraNonce);

            if (fQuantumPoS)
            {
                // Stealth: process qPoS block
                if (fQuantumPoS)
                {
                    if (!pblock->SignBlock(*pwalletMain, pregistryMain))
                    {
                        continue;
                    }

                    printf("StealthMinter : qPoS block found %s\n",
                           pblock->GetHash().ToString().c_str());
                    pblock->print();
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    CheckWork(pblock.get(), *pwalletMain, reservekey, pindexPrev);
                    SetThreadPriority(THREAD_PRIORITY_LOWEST);
                }
                MilliSleep(nSleepInterval);
                continue;
            }
            if (fProofOfStake)
            {
                // ppcoin: if proof-of-stake block found then process block
                if (pblock->IsProofOfStake())
                {
                    if (!pblock->SignBlock(*pwalletMain))
                    {
                        continue;
                    }
                    printf("StealthMinter : PoS block found %s\n",
                                              pblock->GetHash().ToString().c_str());
                    pblock->print();
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    CheckWork(pblock.get(), *pwalletMain, reservekey, pindexPrev);
                    SetThreadPriority(THREAD_PRIORITY_LOWEST);
                }
            }
        }  // end lock registry

        // space blocks better, sleep 1 minute after PoS mint, etc
        MilliSleep(nSleepInterval);
        continue;

/****************************************************************************
 * The following preprocessor conditional block will trigger antivirus for
 * hits similar to "bitcoin miner". It is not compiled by default for XST
 * because PoW ended long ago.
 ****************************************************************************/
#ifdef WITH_MINER
        printf("Running StealthMinter with %" PRIszu " "
                   "transactions in block (%u bytes)\n",
               pblock->vtx.size(),
               ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

        //
        // Search
        //
        int64_t nStart = GetTime();
        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();
        uint256 hash;

        LOOP
        {
            hash = pblock->GetHash();
            if (hash <= hashTarget)
            {
                if (!pblock->SignBlock(*pwalletMain))
                    break;

                SetThreadPriority(THREAD_PRIORITY_NORMAL);

                printf("proof-of-work found \n hash: %s \ntarget: %s\n",
                       hash.GetHex().c_str(), hashTarget.GetHex().c_str());
                pblock->print();

                CheckWork(pblock.get(), *pwalletMain, reservekey, pindexPrev);
                SetThreadPriority(THREAD_PRIORITY_LOWEST);
                break;
            }
            ++pblock->nNonce;

            // Meter hashes/sec
            static int64_t nHashCounter;
            if (nHPSTimerStart == 0)
            {
                nHPSTimerStart = GetTimeMillis();
                nHashCounter = 0;
            }
            else
                nHashCounter += 1;

            if (GetTimeMillis() - nHPSTimerStart > 4000)
            {
                static CCriticalSection cs;
                {
                    LOCK(cs);
                    if (GetTimeMillis() - nHPSTimerStart > 4000)
                    {
                        dHashesPerSec = 1000.0 * nHashCounter /
                                           (GetTimeMillis() - nHPSTimerStart);
                        nHPSTimerStart = GetTimeMillis();
                        nHashCounter = 0;

                            printf("hashmeter %3d CPUs %6.0f khash/s\n",
                                   vnThreadsRunning[THREAD_MINER],
                                   dHashesPerSec/1000.0);
                    }
                }
            }

            // Check for stop or if block needs to be rebuilt
            if (fShutdown)
                return;
            if (!fGenerateXST)
                return;
            if (fLimitProcessors && vnThreadsRunning[THREAD_MINER] > nLimitProcessors)
                return;
            if (vNodes.empty())
                break;
            if (++pblock->nNonce >= 0xffff0000)
                break;
            if (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                break;
            if (pindexPrev != pindexBest)
                break;

            int nFork = GetFork(nHeight);

            // Update nTime every few seconds
            if (nFork < XST_FORK006)
            {
                pblock->nTime = max(pindexPrev->GetPastTimeLimit()+1,
                                    pblock->GetMaxTransactionTime());
            }
            if (nFork < XST_FORK005)
            {
                pblock->nTime = max(pblock->GetBlockTime(),
                                    pindexPrev->GetBlockTime() - nMaxClockDrift);
            }
            else
            {
                pblock->nTime = max(pblock->GetBlockTime(),
                                    pindexPrev->GetBlockTime()+1);
            }
            pblock->UpdateTime(pindexPrev);
            // coinbase timestamp is irrelevant upon XST_FORK006
            if (nFork < XST_FORK005)
            {
                if (pblock->GetBlockTime() >= (int64_t)pblock->vtx[0].GetTxTime() + nMaxClockDrift)
                    break;  // need to update coinbase timestamp
            }
            else if (nFork < XST_FORK006)
            {
                if (pblock->GetBlockTime() >= FutureDrift((int64_t)pblock->vtx[0].GetTxTime()))
                    break;  // need to update coinbase timestamp
            }
        }
#endif  /* WITH_MINER */
    }
}

#ifdef WITH_MINER
void static ThreadStealthMiner(void* parg)
{
    CWallet* pwallet = (CWallet*)parg;
    try
    {
        vnThreadsRunning[THREAD_MINER]++;
        StealthMinter(pwallet, PROOFTYPE_POW);
        vnThreadsRunning[THREAD_MINER]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_MINER]--;
        PrintException(&e, "ThreadStealthMiner()");
    } catch (...) {
        vnThreadsRunning[THREAD_MINER]--;
        PrintException(NULL, "ThreadStealthMiner()");
    }
    nHPSTimerStart = 0;
    if (vnThreadsRunning[THREAD_MINER] == 0)
        dHashesPerSec = 0;
    printf("ThreadStealthMiner exiting, %d threads remaining\n",
           vnThreadsRunning[THREAD_MINER]);
}

void GenerateXST(bool fGenerate, CWallet* pwallet)
{
    fGenerateXST = fGenerate;
    nLimitProcessors = GetArg("-genproclimit", -1);
    if (nLimitProcessors == 0)
        fGenerateXST = false;
    fLimitProcessors = (nLimitProcessors != -1);

    if (fGenerate)
    {
        int nProcessors = boost::thread::hardware_concurrency();
        printf("%d processors\n", nProcessors);
        if (nProcessors < 1)
            nProcessors = 1;
        if (fLimitProcessors && nProcessors > nLimitProcessors)
            nProcessors = nLimitProcessors;
        int nAddThreads = nProcessors - vnThreadsRunning[THREAD_MINER];
        printf("Starting %d StealthMinter threads\n", nAddThreads);
        for (int i = 0; i < nAddThreads; i++)
        {
            if (!NewThread(ThreadStealthMiner, pwallet))
                printf("Error: NewThread(ThreadStealthMiner) failed\n");
            MilliSleep(10);
        }
    }
}
#endif  /* WITH_MINER */
