// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <map>

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <leveldb/env.h>
#include <leveldb/cache.h>
#include <leveldb/filter_policy.h>
#include <memenv/memenv.h>

#include "kernel.h"
#include "checkpoints.h"
#include "txdb.h"
#include "util.h"
#include "main.h"

#include "explore.hpp"


using namespace std;
using namespace boost;

extern QPRegistry *pregistryMain;

leveldb::DB *txdb; // global pointer for LevelDB object instance

static leveldb::Options GetOptions() {
    leveldb::Options options;
    int nCacheSizeMB = GetArg("-dbcache",
                              chainParams.DEFAULT_DBCACHE);
    options.block_cache = leveldb::NewLRUCache(nCacheSizeMB * 1048576);
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    return options;
}

void init_blockindex(leveldb::Options& options, bool fRemoveOld = false) {
    // First time init.
    filesystem::path directory = GetDataDir() / "txleveldb";

    if (fRemoveOld) {
        filesystem::remove_all(directory); // remove directory
        unsigned int nFile = 1;

        while (true)
        {
            filesystem::path strBlockFile = GetDataDir() / strprintf("blk%04u.dat", nFile);

            // Break if no such file
            if( !filesystem::exists( strBlockFile ) )
                break;

            filesystem::remove(strBlockFile);

            nFile++;
        }
    }

    filesystem::create_directory(directory);
    printf("Opening LevelDB in %s\n", directory.string().c_str());
    leveldb::Status status = leveldb::DB::Open(options, directory.string(), &txdb);
    if (!status.ok()) {
        throw runtime_error(strprintf("init_blockindex(): error opening database environment %s", status.ToString().c_str()));
    }
}

// CDB subclasses are created and destroyed VERY OFTEN. That's why
// we shouldn't treat this as a free operations.
CTxDB::CTxDB(const char* pszMode)
{
    assert(pszMode);
    activeBatch = NULL;
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));

    if (txdb) {
        pdb = txdb;
        return;
    }

    bool fCreate = strchr(pszMode, 'c');

    options = GetOptions();
    options.create_if_missing = fCreate;
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);

    init_blockindex(options); // Init directory
    pdb = txdb;

    if (Exists(string("version")))
    {
        ReadVersion(nVersion);
        printf("Transaction index version is %d\n", nVersion);

        if (nVersion < DATABASE_VERSION)
        {
            printf("Required index version is %d, removing old database\n", DATABASE_VERSION);

            // Leveldb instance destruction
            delete txdb;
            txdb = pdb = NULL;
            delete activeBatch;
            activeBatch = NULL;

            init_blockindex(options, true); // Remove directory and create new database
            pdb = txdb;

            bool fTmp = fReadOnly;
            fReadOnly = false;
            WriteVersion(DATABASE_VERSION); // Save transaction index version
            fReadOnly = fTmp;
        }
    }
    else if (fCreate)
    {
        bool fTmp = fReadOnly;
        fReadOnly = false;
        WriteVersion(DATABASE_VERSION);
        fReadOnly = fTmp;
    }

    printf("Opened LevelDB successfully\n");
}

void CTxDB::Close()
{
    delete txdb;
    txdb = pdb = NULL;
    delete options.filter_policy;
    options.filter_policy = NULL;
    delete options.block_cache;
    options.block_cache = NULL;
    delete activeBatch;
    activeBatch = NULL;
}

bool CTxDB::TxnBegin()
{
    assert(!activeBatch);
    activeBatch = new leveldb::WriteBatch();
    return true;
}

bool CTxDB::TxnCommit()
{
    assert(activeBatch);
    leveldb::Status status = pdb->Write(leveldb::WriteOptions(), activeBatch);
    delete activeBatch;
    activeBatch = NULL;
    if (!status.ok()) {
        printf("LevelDB batch commit failure: %s\n", status.ToString().c_str());
        return false;
    }
    return true;
}

class CBatchScanner : public leveldb::WriteBatch::Handler {
public:
    std::string needle;
    bool *deleted;
    std::string *foundValue;
    bool foundEntry;

    CBatchScanner() : foundEntry(false) {}

    virtual void Put(const leveldb::Slice& key, const leveldb::Slice& value) {
        if (key.ToString() == needle) {
            foundEntry = true;
            *deleted = false;
            *foundValue = value.ToString();
        }
    }

    virtual void Delete(const leveldb::Slice& key) {
        if (key.ToString() == needle) {
            foundEntry = true;
            *deleted = true;
        }
    }
};

// When performing a read, if we have an active batch we need to check it first
// before reading from the database, as the rest of the code assumes that once
// a database transaction begins reads are consistent with it. It would be good
// to change that assumption in future and avoid the performance hit, though in
// practice it does not appear to be large.
bool CTxDB::ScanBatch(const CDataStream &key, string *value, bool *deleted) const {
    assert(activeBatch);
    *deleted = false;
    CBatchScanner scanner;
    scanner.needle = key.str();
    scanner.deleted = deleted;
    scanner.foundValue = value;
    leveldb::Status status = activeBatch->Iterate(&scanner);
    if (!status.ok()) {
        throw runtime_error(status.ToString());
    }
    return scanner.foundEntry;

}

bool CTxDB::EraseStartsWith(const string& strSearch)
{
    int count = 0;
    int xcount = 0;
    leveldb::Iterator *iter = pdb->NewIterator(leveldb::ReadOptions());
    iter->SeekToFirst();
    while (iter->Valid())
    {
        xcount += 1;
        if (xcount % 100000 == 0)
        {
            printf("examined %d records\n", xcount);
        }
        if (fRequestShutdown)
        {
            break;
        }
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.write(iter->key().data(), iter->key().size());
        string strPrefix;
        ssKey >> strPrefix;
        if (strPrefix != strSearch)
        {
            if (count > 0)
            {
                break;
            }
            else
            {
                iter->Next();
                continue;
            }
        }
        if ((count == 1) || ((count > 0) && ((count % 100000) == 0)))
        {
            printf("EraseStartsWith(): erased %d records\n", count);
        }
        count += 1;
        if (activeBatch)
        {
            activeBatch->Delete(ssKey.str());
        }
        else
        {
            leveldb::Status status = pdb->Delete(leveldb::WriteOptions(), ssKey.str());
            if (!status.ok())
            {
                return error("TSNH: Can't erase record type \"%s\".",
                             strSearch.c_str());
            }
        }
        iter->Next();
    }
    return true;
}


/*  AddrQty
 *  Parameters - t:type, addr:address, qty:quantity
 */
bool CTxDB::ReadAddrQty(const exploreKey_t& t, const string& addr, int& qtyRet)
{
    qtyRet = 0;
    ss_key_t key = make_pair(t, addr);
    return ReadRecord(key, qtyRet);
}
bool CTxDB::WriteAddrQty(const exploreKey_t& t, const string& addr, const int& qty)
{
    return Write(make_pair(t, addr), qty);
}

/*  AddrTx
 *  Parameters - t:type, addr:address, qty:quantity,
                 value:input_info|output_info|inout
 */
bool CTxDB::RemoveAddrTx(const exploreKey_t& t, const string& addr, const int& qty)
{
    pair<ss_key_t, int> key = make_pair(make_pair(t, addr), qty);
    return RemoveRecord(key);
}
bool CTxDB::AddrTxExists(const exploreKey_t& t, const string& addr, const int& qty)
{
    pair<ss_key_t, int> key = make_pair(make_pair(t, addr), qty);
    return Exists(key);
}


/*  AddrLookup
 *  Parameters - t:type, addr:address,
 *               txid:TxID, n:vout|vin, qty:quantity
 */
bool CTxDB::ReadAddrLookup(const exploreKey_t& t, const string& addr,
                           const uint256& txid, const int& n,
                           int& qtyRet)
{
   qtyRet = -1;
   lookup_key_t key = make_pair(make_pair(t, addr), make_pair(txid, n));
   return ReadRecord(key, qtyRet);
}
bool CTxDB::WriteAddrLookup(const exploreKey_t& t, const string& addr,
                            const uint256& txid, const int& n,
                            const int& qty)
{
   lookup_key_t key = make_pair(make_pair(t, addr), make_pair(txid, n));
   return Write(key, qty);
}
bool CTxDB::RemoveAddrLookup(const exploreKey_t& t, const string& addr,
                             const uint256& txid, const int& n)
{
   lookup_key_t key = make_pair(make_pair(t, addr), make_pair(txid, n));
   return RemoveRecord(key);
}
bool CTxDB::AddrLookupExists(const exploreKey_t& t, const string& addr,
                             const uint256& txid, const int& n)
{
   lookup_key_t key = make_pair(make_pair(t, addr), make_pair(txid, n));
   return Exists(key);
}

/*  AddrBalance
 *  Parameters - t:type, addr:address, b:balance
 */
bool CTxDB::ReadAddrBalance(const exploreKey_t& t, const string& addr,
                            int64_t& bRet)
{
    bRet = 0;
    ss_key_t key = make_pair(t, addr);
    return ReadRecord(key, bRet);
}
bool CTxDB::WriteAddrBalance(const exploreKey_t& t, const string& addr, const int64_t& b)
{
    ss_key_t key = make_pair(t, addr);
    return Write(key, b);
}
bool CTxDB::AddrBalanceExists(const exploreKey_t& t, const std::string& addr)
{
    ss_key_t key = make_pair(t, addr);
    return Exists(key);
}

/*  AddrSet
 *  Parameters - t:type, addr:address, b:balance, s:addresses
 */
bool CTxDB::ReadAddrSet(const exploreKey_t& t, const int64_t b, set<string>& sRet)
{
    sRet.clear();
    pair<exploreKey_t, int64_t> key = make_pair(t, b);
    return ReadRecord(key, sRet);
}
bool CTxDB::WriteAddrSet(const exploreKey_t& t, const int64_t b, const set<string>& s)
{
    pair<exploreKey_t, int64_t> key = make_pair(t, b);
    return Write(key, s);
}
bool CTxDB::RemoveAddrSet(const exploreKey_t& t, const int64_t b)
{
    pair<exploreKey_t, int64_t> key = make_pair(t, b);
    return RemoveRecord(key);
}

/*  TxInfo
 *  Parameters - txid:TxID, txinfo:TxInfo
 */
bool CTxDB::ReadTxInfo(const uint256& txid, ExploreTxInfo& txinfoRet)
{
   txinfoRet.SetNull();
   pair<exploreKey_t, uint256> key = make_pair(TX_INFO, txid);
   return ReadRecord(key, txinfoRet);
}
bool CTxDB::WriteTxInfo(const uint256& txid, const ExploreTxInfo& txinfo)
{
   pair<exploreKey_t, uint256> key = make_pair(TX_INFO, txid);
   return Write(key, txinfo);
}
bool CTxDB::RemoveTxInfo(const uint256& txid)
{
    pair<exploreKey_t, uint256> key = make_pair(TX_INFO, txid);
    return RemoveRecord(key);
}


bool CTxDB::ReadTxIndex(uint256 hash, CTxIndex& txindex)
{
    txindex.SetNull();
    return Read(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::UpdateTxIndex(uint256 hash, const CTxIndex& txindex)
{
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight)
{
    // Add to tx index
    uint256 hash = tx.GetHash();
    CTxIndex txindex(pos, tx.vout.size());
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::EraseTxIndex(const CTransaction& tx)
{
    uint256 hash = tx.GetHash();
    return Erase(make_pair(string("tx"), hash));
}

bool CTxDB::ContainsTx(uint256 hash)
{
    return Exists(make_pair(string("tx"), hash));
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex)
{
    tx.SetNull();
    if (!ReadTxIndex(hash, txindex))
        return false;
    return (tx.ReadFromDisk(txindex.pos));
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex)
{
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair(string("blockindex"), blockindex.GetBlockHash()), blockindex);
}

bool CTxDB::ReadHashBestChain(uint256& hashBestChain)
{
    return Read(string("hashBestChain"), hashBestChain);
}

bool CTxDB::WriteHashBestChain(uint256 hashBestChain)
{
    return Write(string("hashBestChain"), hashBestChain);
}

bool CTxDB::ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust)
{
    return Read(string("bnBestInvalidTrust"), bnBestInvalidTrust);
}

bool CTxDB::WriteBestInvalidTrust(CBigNum bnBestInvalidTrust)
{
    return Write(string("bnBestInvalidTrust"), bnBestInvalidTrust);
}

bool CTxDB::ReadSyncCheckpoint(uint256& hashCheckpoint)
{
    return Read(string("hashSyncCheckpoint"), hashCheckpoint);
}

bool CTxDB::WriteSyncCheckpoint(uint256 hashCheckpoint)
{
    return Write(string("hashSyncCheckpoint"), hashCheckpoint);
}

bool CTxDB::ReadCheckpointPubKey(string& strPubKey)
{
    return Read(string("strCheckpointPubKey"), strPubKey);
}

bool CTxDB::WriteCheckpointPubKey(const string& strPubKey)
{
    return Write(string("strCheckpointPubKey"), strPubKey);
}

static CBlockIndex *InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return NULL;

    // Return existing
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

bool CTxDB::LoadBlockIndex()
{
    if (mapBlockIndex.size() > 0) {
        // Already loaded once in this session. It can happen during migration
        // from BDB.
        return true;
    }

    if (ReadBestRegistrySnapshot(*pregistryMain))
    {
        printf("LoadBlockIndex(): loaded registry at height %d\n",
                                           pregistryMain->GetBlockHeight());
    }
    else
    {
        pregistryMain->SetNull();
        printf("LoadBlockIndex(): registry snapshot not found, "
                                   "will replay registry from block 0\n");
    }

    printf("Loading block index...\n");

    // The block index is an in-memory structure that maps hashes to on-disk
    // locations where the contents of the block can be found. Here, we scan it
    // out of the DB and into mapBlockIndex.
    leveldb::Iterator *iterator = pdb->NewIterator(leveldb::ReadOptions());
    // Seek to start key.
    CDataStream ssStartKey(SER_DISK, CLIENT_VERSION);
    ssStartKey << make_pair(string("blockindex"), uint256(0));
    iterator->Seek(ssStartKey.str());
    int nCount = 0;
    // Now read each entry.
    while (iterator->Valid())
    {
        if ((nCount > 0) && ((nCount % 100000) == 0))
        {
            printf("Loaded %d block indices\n", nCount);
        }
        ++nCount;
        // Unpack keys and values.
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.write(iterator->key().data(), iterator->key().size());
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.write(iterator->value().data(), iterator->value().size());
        string strType;
        ssKey >> strType;
        // Did we reach the end of the data to read?
        if (fRequestShutdown || strType != "blockindex")
        {
            break;
        }
        CDiskBlockIndex diskindex;
        ssValue >> diskindex;

        uint256 blockHash = diskindex.GetBlockHash();

        // Construct block index object
        CBlockIndex* pindexNew      = InsertBlockIndex(blockHash);
        pindexNew->pprev            = InsertBlockIndex(diskindex.hashPrev);
        pindexNew->pnext            = InsertBlockIndex(diskindex.hashNext);
        pindexNew->nFile            = diskindex.nFile;
        pindexNew->nBlockPos        = diskindex.nBlockPos;
        pindexNew->nMint            = diskindex.nMint;
        pindexNew->nMoneySupply     = diskindex.nMoneySupply;
        pindexNew->nFlags           = diskindex.nFlags;
        pindexNew->nStakeModifier   = diskindex.nStakeModifier;
        pindexNew->prevoutStake     = diskindex.prevoutStake;
        pindexNew->nStakeTime       = diskindex.nStakeTime;
        pindexNew->hashProofOfStake = diskindex.hashProofOfStake;
        pindexNew->nTxVolume        = diskindex.nTxVolume;
        pindexNew->nXSTVolume       = diskindex.nXSTVolume;
        pindexNew->nVersion         = diskindex.nVersion;
        pindexNew->hashMerkleRoot   = diskindex.hashMerkleRoot;
        pindexNew->nTime            = diskindex.nTime;
        pindexNew->nBits            = diskindex.nBits;
        pindexNew->nNonce           = diskindex.nNonce;
        pindexNew->nHeight          = diskindex.nHeight;
        pindexNew->nStakerID        = diskindex.nStakerID;
        pindexNew->vDeets           = diskindex.vDeets;

        // Watch for genesis block
        if ((pindexGenesisBlock == NULL) &&
            (blockHash == (fTestNet ? chainParams.hashGenesisBlockTestNet :
                                      hashGenesisBlock)))
        {
            pindexGenesisBlock = pindexNew;
        }

        if (!pindexNew->CheckIndex()) {
            delete iterator;
            return error("LoadBlockIndex() : CheckIndex failed at %d", pindexNew->nHeight);
        }

        // NovaCoin: build setStakeSeen
        if (pindexNew->IsProofOfStake())
            setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));

        iterator->Next();
    }
    delete iterator;

    if (fRequestShutdown)
        return true;

    if (fWithExploreAPI && !GetBoolArg("-reindexexplore", false))
    {
        printf("==\n== Loading Explore API Data\n==\n");
        printf("Loading balance address sets...\n");

        // The mapAddressBalances is an in-memory structure that maps balances
        // to the number of addresses (accounts) with that balance.
        // It is useful for iterating over the rich list by account value.
        leveldb::Iterator *iter = pdb->NewIterator(leveldb::ReadOptions());
        // Seek to start key.
        CDataStream ssStartKey(SER_DISK, CLIENT_VERSION);
        // As a sentinel for this search, nMaxDust
        ssStartKey << make_pair(ADDR_SET_BAL, nMaxDust);
        if (!Exists(ssStartKey))
        {
           set<string> setAddrSentinel;
           if (!Write(ssStartKey, setAddrSentinel))
           {
               return error("LoadBlockIndex() : could not write sentinel address set");
           }
           iter = pdb->NewIterator(leveldb::ReadOptions());
        }
        iter->Seek(ssStartKey.str());
        int nCountSets = 0;
        // Now read each entry.
        while (iter->Valid())
        {
            if ((nCountSets > 0) && ((nCountSets % 1000) == 0))
            {
                printf("Loaded %d balance address sets\n", nCountSets);
            }
            ++nCountSets;
            // Unpack keys and values.
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            ssKey.write(iter->key().data(), iter->key().size());
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            ssValue.write(iter->value().data(), iter->value().size());
            exploreKey_t expkType;
            ssKey >> expkType;
            // Did we reach the end of the data to read?
            if (fRequestShutdown || expkType != ADDR_SET_BAL)
            {
                break;
            }
            int64_t nBalance;
            ssKey >> nBalance;
            set<string> setAddr;
            ssValue >> setAddr;
            if (fDebugExplore)
            {
                printf("==== loaded set of %lu with balance of %" PRId64 "\n",
                       (unsigned long)setAddr.size(), nBalance);
            }
            mapAddressBalances[nBalance] = setAddr.size();
            iter->Next();
        }

        delete iter;

        if (fRequestShutdown)
            return true;
    }

    // Calculate nChainTrust
    vector<pair<int, CBlockIndex*> > vSortedByHeight;
    vSortedByHeight.reserve(mapBlockIndex.size());
    BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        vSortedByHeight.push_back(make_pair(pindex->nHeight, pindex));
    }
    sort(vSortedByHeight.begin(), vSortedByHeight.end());

    int nMainChainHeight = 0;
    BOOST_REVERSE_FOREACH(const PAIRTYPE(int, CBlockIndex*)& item, vSortedByHeight)
    {
        CBlockIndex* pindex = item.second;
        if (pindex->IsInMainChain())
        {
            nMainChainHeight = pindex->nHeight;
            break;
        }
    }
    if (nMainChainHeight < pregistryMain->GetBlockHeight())
    {
        printf("LoadBlockIndex(): snapshot too new, replaying from 0\n");
        pregistryMain->SetNull();
    }
    
    CBlockIndex* pindexBestReplay = NULL;
    BOOST_FOREACH(const PAIRTYPE(int, CBlockIndex*)& item, vSortedByHeight)
    {
        CBlockIndex* pidx = item.second;
        pidx->bnChainTrust = (pidx->pprev ?  pidx->pprev->bnChainTrust : 0) +
                             pidx->GetBlockTrust(pregistryMain);

        int nFork = GetFork(pidx->nHeight);
        
        if (nFork < XST_FORKQPOS)
        {
            // NovaCoin: calculate stake modifier checksum
            pidx->nStakeModifierChecksum = GetStakeModifierChecksum(pidx);
            if (!CheckStakeModifierCheckpoints(pidx->nHeight, pidx->nStakeModifierChecksum))
            {
                return error("CTxDB::LoadBlockIndex() : "
                             "Failed stake modifier checkpoint height=%d, "
                             "modifier=0x%016" PRIx64,
                             pidx->nHeight, pidx->nStakeModifier);
            }
        }
        if ((nFork >= XST_FORKPURCHASE) &&
            (pidx->nHeight > pregistryMain->GetBlockHeight()) &&
            (pidx->IsInMainChain()))
        {
            if (!pregistryMain->UpdateOnNewBlock(pidx, false))
            {
                printf("CTxDB::LoadBlockIndex() : "
                           "Failed registry update from snapshot height=%d\n",
                           pidx->nHeight);
                // something was wrong with the snapshot, start from 0
                pregistryMain->SetNull();
                BOOST_FOREACH(const PAIRTYPE(int, CBlockIndex*)& item2,
                              vSortedByHeight)
                {
                    CBlockIndex* pidx2 = item2.second;
                    int nFork2 = GetFork(pidx2->nHeight);
                    if ((nFork2 >= XST_FORKPURCHASE) &&
                        (pidx2->nHeight > pregistryMain->GetBlockHeight()) &&
                        (pidx2->IsInMainChain()))
                    {
                        if (!pregistryMain->UpdateOnNewBlock(pidx2, false))
                        {
                            return error("CTxDB::LoadBlockIndex() : "
                                            "Failed registry update on replay height=%d",
                                            pidx2->nHeight);
                        }
                    }
                    if (pidx2 == pidx)
                    {
                        break;
                    }
                }
            }
            pindexBestReplay = pidx;
        }
    }

    // Load hashBestChain pointer to end of best chain
    if (!ReadHashBestChain(hashBestChain))
    {
        if (pindexGenesisBlock == NULL)
            return true;
        return error("CTxDB::LoadBlockIndex() : hashBestChain not loaded");
    }

    if (!mapBlockIndex.count(hashBestChain))
        return error("CTxDB::LoadBlockIndex() : hashBestChain not found in the block index");

    pindexBest = mapBlockIndex[hashBestChain];

    // pindexBest is not in main chain according to test
    //    but it is subject to replay
    if ((GetFork(pindexBest->nHeight) >= XST_FORKPURCHASE) &&
        (pindexBest->nHeight > pregistryMain->GetBlockHeight()) &&
        (pindexBestReplay != NULL) &&
        (pindexBest == pindexBestReplay->pnext))
    {
        printf("Updating on best block: %d\n", pindexBest->nHeight);
        if (!pregistryMain->UpdateOnNewBlock(pindexBest, false, true))
        {
            return error("CTxDB::LoadBlockIndex() : "
                             "Failed registry update best block height=%d",
                          pindexBest->nHeight);
        }
    }

    nBestHeight = pindexBest->nHeight;
    bnBestChainTrust = pindexBest->bnChainTrust;

    printf("LoadBlockIndex():  height=%d  trust=%s  date=%s\n   hashBestChain=%s",
      nBestHeight, CBigNum(bnBestChainTrust).ToString().c_str(),
      DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str(),
      hashBestChain.ToString().c_str());

    // NovaCoin: load hashSyncCheckpoint
    if (!ReadSyncCheckpoint(Checkpoints::hashSyncCheckpoint))
        return error("CTxDB::LoadBlockIndex() : hashSyncCheckpoint not loaded");
    printf("LoadBlockIndex(): synchronized checkpoint %s\n", Checkpoints::hashSyncCheckpoint.ToString().c_str());

    // Load bnBestInvalidTrust, OK if it doesn't exist
    CBigNum bnBestInvalidTrust;
    ReadBestInvalidTrust(bnBestInvalidTrust);

    // Verify blocks in the best chain
    int nCheckLevel = GetArg("-checklevel",
                             chainParams.DEFAULT_CHECKLEVEL);
    int nCheckDepth = GetArg("-checkblocks",
                             chainParams.DEFAULT_CHECKBLOCKS);
    if (nCheckDepth == 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    // no need to check genesis
    if (nCheckDepth > (nBestHeight - 1))
    {
        nCheckDepth = nBestHeight - 1;
    }
    printf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);

    // backtrack from best to check start
    int nStartHeight = nBestHeight;
    CBlockIndex *pindexStart = pindexBest;
    for (int i = 1; i < nCheckDepth; ++i)
    {
        // pprev is genesis
        if (pindexStart->pprev->pprev == NULL)
        {
            break;
        }
        pindexStart = pindexStart->pprev;
        nStartHeight -= 1;
    }

    // replay from snapshot height to start
    QPRegistry registryCheck(pregistryMain);
    QPRegistry *pregistryCheck = &registryCheck;
    CTxDB txdb = *this;
    GetRegistrySnapshot(txdb, nStartHeight - 1, pregistryCheck);
    // TODO: refactor (see main.cpp)
    uint256 blockHash = pregistryCheck->GetBlockHash();
    CBlockIndex *pindexCurrent = mapBlockIndex[blockHash];
    while ((pindexCurrent->pnext != NULL) && (pindexCurrent != pindexStart->pprev))
    {
        pindexCurrent = pindexCurrent->pnext;
        pregistryCheck->UpdateOnNewBlock(pindexCurrent, true);
        if ((pindexCurrent->pnext != NULL) && (!pindexCurrent->pnext->IsInMainChain()))
        {
           // this should never happen because we just backtracked from best
           printf("LoadBlockIndex(): TSNH pindexcurrent->next (%s) not in main chain\n",
                  pindexCurrent->pnext->GetBlockHash().ToString().c_str());
           break;
        }
    }

    CBlockIndex* pindexFork = NULL;
    map<pair<unsigned int, unsigned int>, CBlockIndex*> mapBlockPos;
    for (CBlockIndex* pindex = pindexStart; pindex && pindex->pnext; pindex = pindex->pnext)
    {
        if (fRequestShutdown || (pindex->nHeight > nBestHeight))
        {
            break;
        }
        CBlock block;
        if (!block.ReadFromDisk(pindex))
        {
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        }
        // check level 1: verify block validity
        // check level 7: verify block signature too
        vector<QPTxDetails> vDeets;
        if ((nCheckLevel > 0) &&
            !block.CheckBlock(pregistryCheck, vDeets, pindex->pprev))
        {
            printf("LoadBlockIndex() : *** found bad block at %d, hash=%s\n",
                   pindex->nHeight, pindex->GetBlockHash().ToString().c_str());
            pindexFork = pindex->pprev;
            break;
        }
        // check level 2: verify transaction index validity
        if (nCheckLevel>1)
        {
            pair<unsigned int, unsigned int> pos = make_pair(pindex->nFile, pindex->nBlockPos);
            mapBlockPos[pos] = pindex;
            BOOST_FOREACH(const CTransaction &tx, block.vtx)
            {
                uint256 hashTx = tx.GetHash();
                CTxIndex txindex;
                if (ReadTxIndex(hashTx, txindex))
                {
                    // check level 3: checker transaction hashes
                    if (nCheckLevel>2 || pindex->nFile != txindex.pos.nFile || pindex->nBlockPos != txindex.pos.nBlockPos)
                    {
                        // either an error or a duplicate transaction
                        CTransaction txFound;
                        if (!txFound.ReadFromDisk(txindex.pos))
                        {
                            printf("LoadBlockIndex() : *** cannot read mislocated transaction %s\n", hashTx.ToString().c_str());
                            pindexFork = pindex->pprev;
                            break;
                        }
                        else
                            if (txFound.GetHash() != hashTx) // not a duplicate tx
                            {
                                printf("LoadBlockIndex(): *** invalid tx position for %s\n", hashTx.ToString().c_str());
                                pindexFork = pindex->pprev;
                                break;
                            }
                    }
                    // check level 4: check whether spent txouts were spent within the main chain
                    unsigned int nOutput = 0;
                    if (nCheckLevel>3)
                    {
                        BOOST_FOREACH(const CDiskTxPos &txpos, txindex.vSpent)
                        {
                            if (!txpos.IsNull())
                            {
                                pair<unsigned int, unsigned int> posFind = make_pair(txpos.nFile, txpos.nBlockPos);
                                if (!mapBlockPos.count(posFind))
                                {
                                    printf("LoadBlockIndex(): *** found bad spend at %d, hashBlock=%s, hashTx=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString().c_str(), hashTx.ToString().c_str());
                                    pindexFork = pindex->pprev;
                                    break;
                                }
                                // check level 6: check whether spent txouts were spent by a valid transaction that consume them
                                if (nCheckLevel>5)
                                {
                                    CTransaction txSpend;
                                    if (!txSpend.ReadFromDisk(txpos))
                                    {
                                        printf("LoadBlockIndex(): *** cannot read spending transaction of %s:%i from disk\n", hashTx.ToString().c_str(), nOutput);
                                        pindexFork = pindex->pprev;
                                        break;
                                    }
                                    else if (!txSpend.CheckTransaction())
                                    {
                                        printf("LoadBlockIndex(): *** spending transaction of %s:%i is invalid\n", hashTx.ToString().c_str(), nOutput);
                                        pindexFork = pindex->pprev;
                                        break;
                                    }
                                    else
                                    {
                                        bool fFound = false;
                                        BOOST_FOREACH(const CTxIn &txin, txSpend.vin)
                                            if (txin.prevout.hash == hashTx && txin.prevout.n == nOutput)
                                                fFound = true;
                                        if (!fFound)
                                        {
                                            printf("LoadBlockIndex(): *** spending transaction of %s:%i does not spend it\n", hashTx.ToString().c_str(), nOutput);
                                            pindexFork = pindex->pprev;
                                            break;
                                        }
                                    }
                                }
                            }
                            nOutput++;
                        }
                    }
                }
                // check level 5: check whether all prevouts are marked spent
                if (nCheckLevel>4)
                {
                     BOOST_FOREACH(const CTxIn &txin, tx.vin)
                     {
                          CTxIndex txindex;
                          if (ReadTxIndex(txin.prevout.hash, txindex))
                              if (txindex.vSpent.size()-1 < txin.prevout.n || txindex.vSpent[txin.prevout.n].IsNull())
                              {
                                  printf("LoadBlockIndex(): *** found unspent prevout %s:%i in %s\n", txin.prevout.hash.ToString().c_str(), txin.prevout.n, hashTx.ToString().c_str());
                                  pindexFork = pindex->pprev;
                                  break;
                              }
                     }
                }
                // FIXME: is there a check level for qpos?
            }
        }
        pregistryCheck->UpdateOnNewBlock(pindex, false);
    }

    if (pindexFork && !fRequestShutdown)
    {
        // Reorg back to the fork
        printf("LoadBlockIndex() : *** moving best chain pointer back to block %d\n", pindexFork->nHeight);
        CBlock block;
        if (!block.ReadFromDisk(pindexFork))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        CTxDB txdb;
        block.SetBestChain(txdb, pindexFork);
    }

    return true;
}

bool CTxDB::WriteRegistrySnapshot(int nHeight, const QPRegistry& registry)
{
    if (!TxnBegin())
    {
        return error("WriteRegistrySnapshot(): could not begin batch");
    }
    if (!Write(string("bestRegistryHeight"), nHeight))
    {
        return error("WriteRegistrySnapshot(): could not write best height");
    } 
    if (!Write(make_pair(string("registrySnapshot"), nHeight), registry))
    {
        return error("WriteRegistrySnapshot(): could not write registry");
    }
    if (!TxnCommit())
    {
        return error("WriteRegistrySnapshot(): could not commit batch");
    }
    return true;
}

bool CTxDB::ReadRegistrySnapshot(int nHeight, QPRegistry &registry)
{
    if (!Read(make_pair(string("registrySnapshot"), nHeight), registry))
    {
        return error("ReadRegistrySnapshot(): could not read registry at %d", nHeight);
    }
    if (registry.GetBlockHeight() != nHeight)
    {
        return error("ReadRegistrySnapshot(): registry height mismatch:"
                     "want=%d, registry=%d\n", nHeight, registry.GetBlockHeight());
    }
    return true;
}

bool CTxDB::ReadBestRegistrySnapshot(QPRegistry &registry)
{
    int nHeight;
    if (!Read(string("bestRegistryHeight"), nHeight))
    {
        // no error message because it could simply mean it doesn't exist
        return false;
    }
    return ReadRegistrySnapshot(nHeight, registry);
}
 
bool CTxDB::EraseRegistrySnapshot(int nHeight)
{
    return Erase(make_pair(string("registrySnapshot"), nHeight));
}

