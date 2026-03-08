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

#include "kernel.h"
#include "checkpoints.h"
#include "txdb-leveldb.h"
#include "util.h"
#include "main.h"

#include "explore.hpp"


using namespace std;
using namespace boost;

extern QPRegistry *pregistryMain;

// global pointer for LevelDB object instance
leveldb::DB *txdb;

//////////////////////////////////////////////////////////////////////////////
//
// Block Index
//

constexpr size_t MAX_CACHE_SIZE = 30000;

typedef pair<int64_t, CDiskBlockIndex> CacheValue_t;
typedef pair<int64_t, uint256> LookupValue_t;
typedef map<uint256, CacheValue_t> BlockIndexCache_t;
typedef set<LookupValue_t> CacheLookup_t;

static CCriticalSection cs_mapCache;
static BlockIndexCache_t mapCache;
static CacheLookup_t setLookup;

// Helper: Remove an entry from the cache by hash.
// Returns true if an entry was removed.
// ** Caller must hold cs_mapCache. **
static bool RemoveFromCacheLocked(const uint256& hash)
{
    BlockIndexCache_t::iterator it_cache = mapCache.find(hash);
    if (it_cache == mapCache.end())
    {
        return false;
    }

    if (!setLookup.erase(make_pair(it_cache->second.first, hash)))
    {
        throw runtime_error(
            "RemoveFromCacheLocked() : TSNH: no matching lookup");
    }
    mapCache.erase(it_cache);
    return true;
}

// Helper: If cache is full, evict the oldest entry.
// ** Caller must hold cs_mapCache. **
static void EvictOldestIfFullLocked()
{
    if (mapCache.size() < MAX_CACHE_SIZE)
    {
        return;
    }

    CacheLookup_t::iterator it_oldest = setLookup.begin();
    BlockIndexCache_t::iterator it_cache = mapCache.find(it_oldest->second);
    if (it_cache == mapCache.end())
    {
        throw runtime_error(
            "EvictOldestIfFullLocked() : TSNH: no matching cache element");
    }
    mapCache.erase(it_cache);
    setLookup.erase(it_oldest);
}

// For fork tracking during replay
static void SetForkPoint(CBlockMemIndex*& pmemIndexFork,
                         int& nForkHeight,
                         CBlockMemIndex* pmemIndex,
                         int nHeight)
{
    pmemIndexFork = pmemIndex;
    nForkHeight = nHeight;
}

// Helper: Insert an entry into the cache.
// ** Caller must hold cs_mapCache. **
static void InsertIntoCacheLocked(const uint256& hash,
                                  const CDiskBlockIndex& blockIndex)
{
    int64_t nMicroTime = GetTimeMicros();
    mapCache[hash] = make_pair(nMicroTime, blockIndex);
    setLookup.insert(make_pair(nMicroTime, hash));

    // In case this assertion fails: the likely culprit is that somehow,
    // due to multithreading, the same hash was inserted more than once
    // into the cache without an intervening remove. In this case,
    // setLookup would have multiple entries for the same hash, while
    // mapCache would only have one.
    assert(setLookup.size() == mapCache.size());
}

bool ReadDiskBlockIndex(const char* caller,
                        const CBlockMemIndex* pmemIndex,
                        CDiskBlockIndex& blockIndex,
                        CTxDB* ptxdb,
                        bool fStrict)
{
    assert(pmemIndex != nullptr);

    uint256 hash = pmemIndex->GetBlockHash();

    {
        TRY_LOCK(cs_mapCache, lockCache);
        if (lockCache)
        {
            BlockIndexCache_t::iterator it_cache = mapCache.find(hash);
            if (it_cache != mapCache.end())
            {
                blockIndex = it_cache->second.second;
                // Refresh: remove and re-insert as newest
                RemoveFromCacheLocked(hash);
                InsertIntoCacheLocked(hash, blockIndex);
                blockIndex.UpdatePointers(pmemIndex);
                return true;
            }
        }
    }

    AUTO_PTR<CTxDB> localTxDB;
    if (ptxdb == nullptr)
    {
        localTxDB = MAKE_AUTO<CTxDB>("r");
        ptxdb = localTxDB.get();
    }

    if (!ptxdb->ReadBlockIndex(hash, blockIndex))
    {
        string sErrMsg = strprintf("%s() : can't read block index:\n    %s",
                                   caller,
                                   hash.ToString().c_str());
        if (fStrict)
        {
            throw runtime_error(sErrMsg);
        }
        else
        {
            return error(sErrMsg);
        }
    }

    // ensure future serializations include bnChainTrust
    // and nStakeModifierChecksum
    blockIndex.SetExtendedSerialization();

    // copy pointers
    blockIndex.UpdatePointers(pmemIndex);

    {
        LOCK(cs_mapCache);

        // Re-check: another thread may have inserted while we were reading
        if (RemoveFromCacheLocked(hash))
        {
            // Entry was added by another thread; just refresh it
        }
        else
        {
            // New entry; make room if needed
            EvictOldestIfFullLocked();
        }

        InsertIntoCacheLocked(hash, blockIndex);
    }

    return true;
}

bool WriteDiskBlockIndex(const char* caller,
                         const CBlockMemIndex* pmemIndex,
                         CDiskBlockIndex& blockIndex,
                         CTxDB* ptxdb,
                         bool fStrict)
{
    assert(pmemIndex != nullptr);

    uint256 hash = pmemIndex->GetBlockHash();

    // copy pointers
    blockIndex.UpdatePointers(pmemIndex);

    AUTO_PTR<CTxDB> localTxDB;
    if (ptxdb == nullptr)
    {
        localTxDB = MAKE_AUTO<CTxDB>();
        ptxdb = localTxDB.get();
    }

    if (!ptxdb->WriteBlockIndex(blockIndex))
    {
        string sErrMsg = strprintf("%s() : can't write block index:\n    %s",
                                   caller,
                                   hash.ToString().c_str());
        if (fStrict)
        {
            throw runtime_error(sErrMsg);
        }
        else
        {
            return error(sErrMsg);
        }
    }

    {
        LOCK(cs_mapCache);

        if (!RemoveFromCacheLocked(hash))
        {
            // Entry didn't exist; make room if needed
            EvictOldestIfFullLocked();
        }

        InsertIntoCacheLocked(hash, blockIndex);
    }

    return true;
}


int GetMemIndexHeight(const char* caller,
                      const CBlockMemIndex* pmemIndex,
                      CTxDB* ptxdb)
{
    return GetMemIndexAttr(caller,
                           pmemIndex,
                           ptxdb,
                           &CBlockIndex::nHeight);
}

unsigned int GetMemIndexTime(const char* caller,
                             const CBlockMemIndex* pmemIndex,
                             CTxDB* ptxdb)
{
    return GetMemIndexAttr(caller, pmemIndex, ptxdb, &CBlockIndex::nTime);
}

int64_t GetMemIndexBlockTime(const char* caller,
                             const CBlockMemIndex* pmemIndex,
                             CTxDB* ptxdb)
{
    return (int64_t)GetMemIndexAttr(caller,
                                    pmemIndex,
                                    ptxdb,
                                    &CBlockIndex::nTime);
}

int GetMemIndexVersion(const char* caller,
                       const CBlockMemIndex* pmemIndex,
                       CTxDB* ptxdb)
{
    return GetMemIndexAttr(caller,
                           pmemIndex,
                           ptxdb,
                           &CBlockIndex::nVersion);
}

CBigNum GetMemIndexChainTrust(const char* caller,
                              const CBlockMemIndex* pmemIndex,
                              CTxDB* ptxdb)
{
    return GetMemIndexAttr(caller,
                           pmemIndex,
                           ptxdb,
                           &CBlockIndex::bnChainTrust);
}

int GetMemIndexFlags(const char* caller,
                     const CBlockMemIndex* pmemIndex,
                     CTxDB* ptxdb)
{
    return GetMemIndexAttr(caller, pmemIndex, ptxdb, &CBlockIndex::nFlags);
}

int64_t GetMemIndexMoneySupply(const char* caller,
                               const CBlockMemIndex* pmemIndex,
                               CTxDB* ptxdb)
{
    return GetMemIndexAttr(caller,
                           pmemIndex,
                           ptxdb,
                           &CBlockIndex::nMoneySupply);
}

bool IsMemIndexProofOfStake(const char* caller,
                            const CBlockMemIndex* pmemIndex,
                            CTxDB* ptxdb)
{
    return GetMemIndexFlags(caller, pmemIndex, ptxdb) &
           CBlockIndex::BLOCK_PROOF_OF_STAKE;
}

unsigned int GetMemIndexStakeModifierChecksum(const char* caller,
                                              const CBlockMemIndex* pmemIndex,
                                              CTxDB* ptxdb)
{
    return GetMemIndexAttr(caller,
                           pmemIndex,
                           ptxdb,
                           &CBlockIndex::nStakeModifierChecksum);
}

uint256 GetMemIndexHashMerkleRoot(const char* caller,
                                  const CBlockMemIndex* pmemIndex,
                                  CTxDB* ptxdb)
{
    return GetMemIndexAttr(caller,
                           pmemIndex,
                           ptxdb,
                           &CBlockIndex::hashMerkleRoot);
}

static leveldb::Options GetOptions()
{
    leveldb::Options options;
    int nCacheSizeMB = GetArg("-dbcache", chainParams.DEFAULT_DBCACHE);
    options.block_cache = leveldb::NewLRUCache(nCacheSizeMB * 1048576);
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    return options;
}

void init_blockindex(leveldb::Options& options, bool fRemoveOld = false)
{
    // First time init.
    boost::filesystem::path directory = GetDataDir() / "txleveldb";

    if (fRemoveOld)
    {
        // remove directory
        boost::filesystem::remove_all(directory);
        unsigned int nFile = 1;

        while (true)
        {
            boost::filesystem::path strBlockFile = GetDataDir() /
                                                   strprintf("blk%04u.dat",
                                                             nFile);

            // Break if no such file
            if (!boost::filesystem::exists(strBlockFile))
            {
                break;
            }

            boost::filesystem::remove(strBlockFile);

            nFile++;
        }
    }

    boost::filesystem::create_directory(directory);
    printf("Opening LevelDB in %s\n", directory.string().c_str());
    leveldb::Status status = leveldb::DB::Open(options,
                                               directory.string(),
                                               &txdb);
    if (!status.ok())
    {
        throw runtime_error(strprintf(
            "init_blockindex(): error opening database environment %s",
            status.ToString().c_str()));
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

    // Init directory
    init_blockindex(options);
    pdb = txdb;

    if (Exists(string("version")))
    {
        ReadVersion(nVersion);
        printf("Transaction index version is %d.\n", nVersion);

        if (nVersion < DATABASE_VERSION)
        {
            // The following auto-wipe code was removed (commented-out) because
            // (1) It would have never been called under any circumstances
            //     before Stealth v3.3.0.0, when it was removed.
            // (2) Having a version mismatch for the upgrade to v3.3.0.0
            //     is not fatal. v3.3.0.0 represents an upgrade from
            //     unversioned (implicitly LEGACY_DATABASE_VERSION) to
            //     EXTENDED_BLOCKINDEX_DATABASE_VERSION (see comments in
            //     version.h and to the enum BLOCK_EXTENDED_SER for
            //     CBlockIndex in main.h).
            // (3) Policy for Stealth is that database version mismatches
            //     should not be fatal, so auto-wipe should never be executed,
            //     even upon partial database upgrades that were aborted for
            //     some reason. In these cases the database should stay at the
            //     old version.
            //
            // Auto-wipe code is kept commented and left as a reference in
            // case the code is forked to a new project without the
            // requirement that database version mismatches
            // should not be fatal.

            // printf("Required index version is %d, removing old database\n",
            //        DATABASE_VERSION);

            // // Leveldb instance destruction
            // delete txdb;
            // txdb = pdb = NULL;
            // delete activeBatch;
            // activeBatch = NULL;

            // // Remove directory and create new database
            // init_blockindex(options, true);
            // pdb = txdb;
            // bool fTmp = fReadOnly;
            // fReadOnly = false;

            // // Save transaction index version
            // WriteVersion(DATABASE_VERSION);
            // fReadOnly = fTmp;

            printf("Database is old version, new is %d\n", DATABASE_VERSION);
        }
    }
    else
    {
        printf("Transaction index has no version.\n");
        if (fCreate)
        {
            bool fTmp = fReadOnly;
            fReadOnly = false;
            printf("Writing transaction index version %d.\n",
                   DATABASE_VERSION);
            WriteVersion(DATABASE_VERSION);
            fReadOnly = fTmp;
        }
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

bool CTxDB::EraseStartsWith(const string& strSentinel,
                            const string& strSearch,
                            bool fActiveBatchOK)
{
    if ((!fActiveBatchOK) && activeBatch)
    {
        return error("EraseStartsWith(): active batch not allowed");
    }
    int count = 0;
    int xcount = 0;
    leveldb::Iterator *iter = pdb->NewIterator(leveldb::ReadOptions());
    iter->Seek(strSentinel);
    // don't erase the sentinel
    iter->Next();
    if (!TxnBegin())
    {
        return error("EraseStartsWith() : first TxnBegin failed");
    }
    while (iter->Valid())
    {
        xcount += 1;
        if (fDebugExplore && (xcount % 100000 == 0))
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
        if ((xcount > 0) && ((count % 10000) == 0))
        {
            if (!TxnCommit())
            {
                return error("EraseStartsWith() : TxnCommit failed");
            }
            if (!TxnBegin())
            {
                return error("EraseStartsWith() : TxnBegin failed");
            }
        }
        if ((count == 1) || ((count > 0) && ((count % 100000) == 0)))
        {
            string strKeyType;
            ssKey >> strKeyType;
            printf("EraseStartsWith(): erased %d records\n", count);
            printf("   prefix: %s, type:%s\n", strPrefix.c_str(),
                                               strKeyType.c_str());
        }
        count += 1;
        string strKeyDel(ssKey.full_str());
        if (activeBatch)
        {
            activeBatch->Delete(strKeyDel);
        }
        else
        {
            leveldb::Status status = pdb->Delete(leveldb::WriteOptions(), strKeyDel);
            if (!status.ok())
            {
                return error("TSNH: Can't erase record type \"%s\".",
                             strSearch.c_str());
            }
        }
        iter->Next();
    }
    if (!TxnCommit())
    {
        return error("EraseStartsWith() : final TxnCommit failed");
    }
    return true;
}

bool CTxDB::WriteExploreSentinel(int value)
{
    return Write(EXPLORE_SENTINEL, value);
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
bool CTxDB::AddrTxIsViable(const exploreKey_t& t, const string& addr, const int& qty)
{
    pair<ss_key_t, int> key = make_pair(make_pair(t, addr), qty);
    return IsViable(key);
}

/*  AddrList
 *  Parameters - t:type, addr:address, qty:quantity, value:inout_list
 */
bool CTxDB::RemoveAddrList(const exploreKey_t& t, const string& addr, const int& qty)
{
    pair<ss_key_t, int> key = make_pair(make_pair(t, addr), qty);
    return RemoveRecord(key);
}
bool CTxDB::AddrListIsViable(const exploreKey_t& t, const string& addr, const int& qty)
{
    pair<ss_key_t, int> key = make_pair(make_pair(t, addr), qty);
    return IsViable(key);
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
bool CTxDB::AddrLookupIsViable(const exploreKey_t& t, const string& addr,
                               const uint256& txid, const int& n)
{
   lookup_key_t key = make_pair(make_pair(t, addr), make_pair(txid, n));
   return IsViable(key);
}


/*  AddrValue
 *  Parameters - t:type, addr:address, v:value
 */
bool CTxDB::ReadAddrValue(const exploreKey_t& t, const string& addr,
                          int64_t& vRet)
{
    vRet = 0;
    ss_key_t key = make_pair(t, addr);
    return ReadRecord(key, vRet);
}
bool CTxDB::WriteAddrValue(const exploreKey_t& t, const string& addr,
                           const int64_t& v)
{
    ss_key_t key = make_pair(t, addr);
    return Write(key, v);
}
bool CTxDB::AddrValueIsViable(const exploreKey_t& t, const std::string& addr)
{
    ss_key_t key = make_pair(t, addr);
    return IsViable(key);
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

/*  ExploreTx
 *  Parameters - txid:TxID, extx:ExploreTx
 */
bool CTxDB::ReadExploreTx(const uint256& txid, ExploreTx& extxRet)
{
   extxRet.SetNull();
   pair<exploreKey_t, uint256> key = make_pair(EXPLORE_TX, txid);
   return ReadRecord(key, extxRet);
}
bool CTxDB::WriteExploreTx(const uint256& txid, const ExploreTx& extx)
{
   pair<exploreKey_t, uint256> key = make_pair(EXPLORE_TX, txid);
   return Write(key, extx);
}
bool CTxDB::RemoveExploreTx(const uint256& txid)
{
    pair<exploreKey_t, uint256> key = make_pair(EXPLORE_TX, txid);
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

bool CTxDB::AddTxIndex(const CTransaction& tx,
                       const CDiskTxPos& pos,
                       int nHeight)
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
    {
        return false;
    }
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
    return Write(make_pair(string("blockindex"), blockindex.GetBlockHash()),
                 blockindex);
}

bool CTxDB::ReadBlockIndex(const uint256& hash, CDiskBlockIndex& blockindex)
{
    return Read(make_pair(string("blockindex"), hash), blockindex);
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

static CBlockMemIndex* InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
    {
        return NULL;
    }

    // Return existing
    CMapBlockIndex::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
    {
        return (*mi).second;
    }

    // Create new
    CBlockMemIndex* pmemIndexNew = new CBlockMemIndex();
    if (!pmemIndexNew)
    {
        throw runtime_error("InsertBlockIndex() : new CBlockMemIndex failed");
    }

    mi = mapBlockIndex.insert(make_pair(hash, pmemIndexNew)).first;

    pmemIndexNew->phashBlock = &((*mi).first);

    return pmemIndexNew;
}

bool CTxDB::LoadBlockIndex()
{
    if (mapBlockIndex.size() > 0) {
        // Already loaded once in this session. It can happen during migration
        // from BDB.
        return true;
    }

    printf("Loading block index...\n");

    int nDatabaseVersion = LEGACY_DATABASE_VERSION;

    if (Exists(string("version")))
    {
        ReadVersion(nDatabaseVersion);
    }

    // The block index is an in-memory structure that maps hashes to on-disk
    // locations where the contents of the block can be found. Here, we scan it
    // out of the DB and into mapBlockIndex.
    leveldb::Iterator *iterator = pdb->NewIterator(leveldb::ReadOptions());

    // Seek to start key.
    CDataStream ssStartKey(SER_DISK, CLIENT_VERSION);
    ssStartKey << make_pair(string("blockindex"), uint256(0));
    iterator->Seek(ssStartKey.str());

    // Count blockindex to reserve memory for the vSortedByHeight
    // which is used to construct mapBlockLookup
    printf("Taking inventory of block indices...\n");
    int nCountInventoried = 0;
    while (iterator->Valid())
    {
        if ((nCountInventoried > 0) && ((nCountInventoried % 1000000) == 0))
        {
            printf("Counted %d block indices\n", nCountInventoried);
        }
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.write(iterator->key().data(), iterator->key().size());
        string strType;
        ssKey >> strType;
        // Did we reach the end of the data to read?
        if (fRequestShutdown || strType != "blockindex")
        {
            break;
        }
        ++nCountInventoried;
        iterator->Next();
    }
    printf("Total indices inventoried: %d\n", nCountInventoried);

    if (fRequestShutdown)
    {
        delete iterator;
        return true;
    }

    // For mapBlockLookup, chain trust, and replaying the registry
    vector<pair<int, CBlockMemIndex*>> vSortedByHeight;
    vSortedByHeight.reserve(static_cast<size_t>(nCountInventoried));

    // Now go back to the beginning and read each entry.
    iterator->SeekToFirst();
    iterator->Seek(ssStartKey.str());
    int nCountLoaded = 0;
    while (iterator->Valid())
    {
        if ((nCountLoaded > 0) && ((nCountLoaded % 100000) == 0))
        {
            printf("Loaded %d block indices\n", nCountLoaded);
        }
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

        CDiskBlockIndex diskIndex;
        ssValue >> diskIndex;
        ++nCountLoaded;

        uint256 blockHash = diskIndex.GetBlockHash();

        // Construct block index object
        CBlockMemIndex* pmemIndexNew    = InsertBlockIndex(blockHash);
        pmemIndexNew->pprev             = InsertBlockIndex(diskIndex.hashPrev);
        pmemIndexNew->pnext             = InsertBlockIndex(diskIndex.hashNext);
        pmemIndexNew->nFile             = diskIndex.nFile;
        pmemIndexNew->nBlockPos         = diskIndex.nBlockPos;

        // Watch for genesis block
        if ((pindexGenesisBlock == NULL) &&
            (blockHash == (fTestNet ? chainParams.hashGenesisBlockTestNet
                                    : hashGenesisBlock)))
        {
            pmemIndexGenesisBlock = pmemIndexNew;
            diskIndex.phashBlock = pmemIndexNew->phashBlock;
            diskIndex.pprev = pmemIndexNew->pprev;
            diskIndex.pnext = pmemIndexNew->pnext;
            pindexGenesisBlock = new CBlockIndex(diskIndex);
        }

        // NovaCoin: build setStakeSeen
        if (diskIndex.IsProofOfStake())
        {
            setStakeSeen.insert(
                make_pair(diskIndex.prevoutStake, diskIndex.nStakeTime));
        }

        // Add to vSortedByHeight
        vSortedByHeight.push_back(make_pair(diskIndex.nHeight, pmemIndexNew));

        iterator->Next();
    }

    printf("Total indices loaded: %d\n", nCountLoaded);
    delete iterator;

    if (fRequestShutdown)
    {
        return true;
    }

    assert (nCountInventoried == nCountLoaded);

    // Re-indexing Explore
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
        ssStartKey << make_pair(ADDR_SET_BAL, 0);
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
            if (fRequestShutdown)
            {
                break;
            }
            // Did we reach the end of the address sets?
            string strDBLabel;
            ssKey >> strDBLabel;
            if (strDBLabel != EXPLORE_KEY)
            {
                break;
            }
            string strTypeLabel;
            ssKey >> strTypeLabel;
            if (strTypeLabel != ADDR_SET_BAL_LABEL)
            {
                break;
            }
            int64_t nBalance;
            ssKey >> nBalance;
            set<string> setAddr;
            ssValue >> setAddr;
            unsigned int sizeSetAddr = setAddr.size();
            if (fDebugExplore)
            {
                printf("==== loaded set of %lu with balance of %" PRId64 "\n",
                       (unsigned long)sizeSetAddr, nBalance);
            }
            mapAddressBalances[nBalance] = sizeSetAddr;
            iter->Next();
        }

        delete iter;

        if (fRequestShutdown)
        {
            return true;
        }
    }

    // Populate hashBestChain, pindexBest, pmemIndexBest, nBestHeight
    if (!ReadHashBestChain(hashBestChain))
    {
        if (pindexGenesisBlock == NULL)
        {
            // Empty database is valid for any version.
            WriteVersion(DATABASE_VERSION);
            return true;
        }
        return error("CTxDB::LoadBlockIndex() : hashBestChain not loaded");
    }

    if (!mapBlockIndex.count(hashBestChain))
    {
        return error("CTxDB::LoadBlockIndex() : "
                        "hashBestChain not found in the block index");
    }

    pmemIndexBest = mapBlockIndex[hashBestChain];
    CDiskBlockIndex diskIndexBest;
    ReadDiskBlockIndex("CTxDB::LoadBlockIndex",
                       pmemIndexBest,
                       diskIndexBest,
                       this);

    delete pindexBest;
    pindexBest = new CBlockIndex(diskIndexBest);

    nBestHeight = pindexBest->nHeight;
    printf("Best index is %d: %s\n",
           nBestHeight,
           hashBestChain.ToString().c_str());

    // Update pregistryMain to most recent snapshot
    pregistryMain->SetNull();

    if (vSortedByHeight.empty())
    {
        // This should never happen because at the very least
        //    mapBlockIndex has pindexBest.
        return error("LoadBlockIndex(): TSNH No blocks\n");
    }

    // The logic behind the nSnapshotHeight = -1 is that all blocks
    // AFTER the snapshot need to be replayed to calculate bnChainTrust
    // and nStakeModifierChecksum. Therefore, if the genesis block needs
    // to be replayed, then it is is as if the (virtual) snapshot is at -1.
    // Replay from the genesis block should only be necessary when
    // updating records for a new database version.
    int nSnapshotHeight = -1;
    if (nDatabaseVersion < EXTENDED_BLOCKINDEX_DATABASE_VERSION)
    {
        printf("LoadBlockIndex(): need to replay full chain to add chain "
               "trusts to block indices\n");
    }
    else if (!Exists(string("bestRegistryHeight")))
    {
        printf("LoadBlockIndex(): can't find best snapshot height,\n"
               "   will replay full block chain\n");
    }
    else if (!Read(string("bestRegistryHeight"), nSnapshotHeight))
    {
        printf("LoadBlockIndex(): can't read best snapshot height,\n"
               "   will replay full block chain");
        nSnapshotHeight = -1;
    }
    else if (nSnapshotHeight >= pindexBest->nHeight)
    {
        // Although unexpected, this situation is not impossible. It could
        // result from a reorganization. The backtrack is not expected to be
        // deep, but we'll go all the way back to START_PURCHASE if needed
        // (where the registry is null). This worst case falls through
        // and is essentially equivalent to replaying from genesis.
        printf("LoadBlockIndex(): UNEXPECTED best snapshot is higher than\n"
               "    best chain, backtracking...\n");
        static const int START_PURCHASE = fTestNet
                                              ? chainParams.START_PURCHASE_T
                                              : chainParams.START_PURCHASE_M;
        nSnapshotHeight = (nSnapshotHeight / BLOCKS_PER_SNAPSHOT - 1) *
                          BLOCKS_PER_SNAPSHOT;
        bool fFoundSnapshot = false;
        while (nSnapshotHeight > START_PURCHASE)
        {
            if ((nSnapshotHeight < pindexBest->nHeight) &&
                Exists(make_pair(string("registrySnapshot"), nSnapshotHeight)))
            {
                if (ReadRegistrySnapshot(nSnapshotHeight, *pregistryMain))
                {
                    fFoundSnapshot = true;
                }
                break;
            }
            nSnapshotHeight -= BLOCKS_PER_SNAPSHOT;
        }
        if (!fFoundSnapshot)
        {
            nSnapshotHeight = -1;
        }
    }
    else if (!ReadRegistrySnapshot(nSnapshotHeight, *pregistryMain))
    {
        printf("LoadBlockIndex(): can't read best registry snapshot,\n"
               "   will replay full block chain");
        nSnapshotHeight = -1;
    }

    int nHeightStop = pindexBest->nHeight - 1;

    int nToReplay = nHeightStop - nSnapshotHeight;

    // Replay registry from nBestRegistry height, and calculate chain trust.
    printf("Replaying qPoS registry from %d to %d...\n",
           nSnapshotHeight,
           nHeightStop);

    // For chain trust and replaying the registry
    sort(vSortedByHeight.begin(), vSortedByHeight.end());


    // Used to determine which registry snapshots to create.
    int nMaxHeightSorted = vSortedByHeight.back().first;

    static const int nRecentBlocks = RECENT_SNAPSHOTS * BLOCKS_PER_SNAPSHOT;

    CDiskBlockIndex diskIndex;
    CDiskBlockIndex diskIndexPrev;
    CBigNum bnChainTrust;
    unsigned int nStakeModifierChecksum;

    CBlockMemIndex* pmemIndexBestReplay = NULL;

    int nCountReplayed = 0;
    int nCountSkipped = 0;
    BOOST_FOREACH(const PAIRTYPE(int, CBlockMemIndex*)& item, vSortedByHeight)
    {
        if (item.first <= nSnapshotHeight)
        {
            nCountSkipped += 1;
            if ((nCountSkipped > 0) && (nCountSkipped % 1000000 == 0))
            {
                printf("Skipped %d blocks\n", nCountSkipped);
            }
            continue;
        }

        if (nCountSkipped)
        {
            printf("Skipped %d blocks\n", nCountSkipped);
            nCountSkipped = 0;
        }

        bool fWriteDiskIndex = false;

        int nHeight = item.first;

        CBlockMemIndex* pmemIndex = item.second;

        ReadDiskBlockIndex("LoadBlockIndex", pmemIndex, diskIndex, this);
        if (diskIndex.bnChainTrust < 0)
        {
            if (fDebug && (diskIndex.nHeight % 1000000 == 0))
            {
                printf("LoadBlockIndex(): block index record on disk "
                       "has legacy serialization at \n   %s\n      nFlags=%s\n",
                       diskIndex.GetBlockHash().ToString().c_str(),
                       UnsignedToBinary(diskIndex.nFlags, 4).c_str());
            }
            diskIndex.bnChainTrust = 0;
            fWriteDiskIndex = true;
        }

        if (pmemIndex->pprev)
        {
            ReadDiskBlockIndex("LoadBlockIndex",
                               pmemIndex->pprev,
                               diskIndexPrev,
                               this);
            if (diskIndexPrev.bnChainTrust < 0)
            {
                diskIndexPrev.bnChainTrust = 0;
                WriteDiskBlockIndex("LoadBlockIndex",
                                    pmemIndex->pprev,
                                    diskIndexPrev,
                                    this);
            }
            bnChainTrust = diskIndexPrev.bnChainTrust +
                           diskIndex.GetBlockTrust(pregistryMain);
        }
        else
        {
            bnChainTrust = diskIndex.GetBlockTrust(pregistryMain);
        }

        if (diskIndex.bnChainTrust != bnChainTrust)
        {
            diskIndex.bnChainTrust = bnChainTrust;
            fWriteDiskIndex = true;
        }

        int nFork = GetFork(nHeight);

        if (nFork < XST_FORKQPOS)
        {
            if (fDebug)
            {
                printf("stored stake checksum at %d: 0x%016" PRIx32 "\n",
                       diskIndex.nHeight,
                       diskIndex.nStakeModifierChecksum);
            }

            // NovaCoin: calculate stake modifier checksum
            nStakeModifierChecksum = GetStakeModifierChecksum(&diskIndex);

            if (diskIndex.nStakeModifierChecksum != nStakeModifierChecksum)
            {
                diskIndex.nStakeModifierChecksum = nStakeModifierChecksum;
                fWriteDiskIndex = true;
            }

            if (fDebug)
            {
                printf("calculated stake checksum at %d: 0x%016" PRIx32 "\n",
                       diskIndex.nHeight,
                       diskIndex.nStakeModifierChecksum);
            }

            if (!CheckStakeModifierCheckpoints(
                    nHeight,
                    diskIndex.nStakeModifierChecksum))
            {
                return error("CTxDB::LoadBlockIndex() : "
                             "Failed stake modifier checkpoint height=%d, "
                             "modifier=0x%016" PRIx64,
                             nHeight, diskIndex.nStakeModifier);
            }
        }

        if (fWriteDiskIndex)
        {
            WriteDiskBlockIndex("LoadBlockIndex", pmemIndex, diskIndex, this);
        }

        if (pmemIndex->IsInMainChain())
        {
            if (nCountReplayed % 100000 == 0)
            {
                printf("Replayed %d of %d blocks at height %d\n",
                       nCountReplayed,
                       nToReplay,
                       nHeight);
            }
            int nRegistryHeight = pregistryMain->GetBlockHeight();
            // The height test will ensure that no attempt will be made to replay
            // any blocks after a failed main chain block.
            if ((nFork >= XST_FORKPURCHASE) &&
                ((nRegistryHeight == 0) ||
                 (nHeight == pregistryMain->GetBlockHeight() + 1)))
            {
                // Failing to update on a new block will be interpreted as a protocol
                // change, forking the chain at the precursor to the failed block.
                int nSnapType = ((nMaxHeightSorted - nHeight) > nRecentBlocks) ?
                                                          QPRegistry::SPARSE_SNAPS :
                                                          QPRegistry::ALL_SNAPS;
                if (!pregistryMain->UpdateOnNewBlock(&diskIndex, nSnapType))
                {
                    // force update again to print some debugging info before exit
                    pregistryMain->UpdateOnNewBlock(&diskIndex, QPRegistry::NO_SNAPS, true);
                    printf("CTxDB::LoadBlockIndex() : "
                              "Failed registry update from snapshot height=%d\n",
                           nHeight);
                }
            }
            pmemIndexBestReplay = pmemIndex;
            nCountReplayed += 1;
            if (pregistryMain->GetBlockHeight() >= nHeightStop)
            {
                break;
            }
        }
    }

    printf("Replayed %d blocks at height %d\n", nCountReplayed, nHeightStop);

    diskIndex.SetNull();
    diskIndexPrev.SetNull();

    if (!pmemIndexBestReplay)
    {
        // Return error and force a start from scratch because there are
        //    no mainchain blocks in the block index (i.e. block index
        //    is a complete disaster).
        return error("LoadBlockIndex(): no mainchain blocks\n");
    }

    CDiskBlockIndex diskIndexBestReplay;
    ReadDiskBlockIndex("LoadBlockIndex",
                       pmemIndexBestReplay,
                       diskIndexBestReplay,
                       this);

    printf("Replayed main chain to %d: %s\n",
           diskIndexBestReplay.nHeight,
           diskIndexBestReplay.GetBlockHash().ToString().c_str());

    int nReplayHeight = pregistryMain->GetBlockHeight();

    CBlockMemIndex* pmemIndexFork = NULL;
    // initialized to -1 to catch logic errors during debugging
    int nForkHeight = -1;

    // Best height has been replayed, pindexBest is best at prior shutdown
    // (replay should be up to block before best, not best block)
    if (pindexBest->nHeight == nReplayHeight)
    {
        // Replay loop will cycle at least once, for the genesis block
        if (pindexBest != pindexGenesisBlock)
        {
            return error("CTxDB::LoadBlockIndex(): TSNH not genesis:\n"
                            "  %s", pindexBest->GetBlockHash().ToString().c_str());
        }
    }
    else if (pindexBest->nHeight > (nReplayHeight + 1))
    {
        // disconnect invalidated blocks
        printf("WARNING: Protocol change?\n"
                   "         Registry could not replay further than %d\n  %s\n",
               nReplayHeight,
               diskIndexBestReplay.GetBlockHash().ToString().c_str());
        // estimate trust as minimum possible
        if (pindexBest->bnChainTrust != diskIndexBestReplay.bnChainTrust)
        {
            pindexBest->bnChainTrust = diskIndexBestReplay.bnChainTrust;
            CDiskBlockIndex diskIndexBest(pindexBest);
            WriteDiskBlockIndex("LoadBlockIndex",
                                pmemIndexBest,
                                diskIndexBest,
                                this);
            SetForkPoint(pmemIndexFork,
                         nForkHeight,
                         pmemIndexBestReplay,
                         diskIndexBestReplay.nHeight);
        }
    }
    else if (pindexBest->nHeight < nReplayHeight)
    {
        // this should never happen
        return error("CTxDB::LoadBlockIndex(): TSNH "
                        "best index lower than best replay");
    }
    // pindexBest is not in main chain according to test
    //    but it is subject to replay
    else if ((GetFork(pindexBest->nHeight) >= XST_FORKPURCHASE) &&
             (pindexBest->nHeight > pregistryMain->GetBlockHeight()) &&
             (pmemIndexBestReplay != NULL) &&
             (pmemIndexBest == pmemIndexBestReplay->pnext))
    {
        printf("Updating on best block: %d\n", pindexBest->nHeight);
        if (!pregistryMain->UpdateOnNewBlock(pindexBest,
                                             QPRegistry::ALL_SNAPS,
                                             true))
        {
            return error("CTxDB::LoadBlockIndex() : "
                             "Failed registry update best block height=%d",
                          pindexBest->nHeight);
        }

        // unexpected condition where the best's prev is not the 2nd best's next
        if (pmemIndexBest->pprev != pmemIndexBestReplay)
        {
            if (pmemIndexBest->pprev == nullptr)
            {
                printf("LoadBlockIndex() : TSNH pmemIndexBest->pprev is null\n");
            }
            else
            {
                printf("LoadBlockIndex() : TSNH pmemIndexBest->pprev is incconsistent\n");
            }
            pmemIndexBest->pprev = pmemIndexBestReplay;
            pindexBest->UpdatePointers(pmemIndexBest);
            // update the cache
            WriteDiskBlockIndex("LoadBlockIndex",
                                pmemIndexBest,
                                diskIndexBest,
                                this);
        }

        if (pindexBest->pprev)
        {
            CDiskBlockIndex diskIndexBestPrev;
            ReadDiskBlockIndex("LoadBlockIndex",
                               pindexBest->pprev,
                               diskIndexBestPrev,
                               this);

            if (diskIndexBestPrev.bnChainTrust < 0)
            {
                diskIndexBestPrev.bnChainTrust = 0;
                WriteDiskBlockIndex("LoadBlockIndex",
                   pindexBest->pprev,
                   diskIndexBestPrev,
                   this);
            }

            CBigNum bnChainTrust = diskIndexBestPrev.bnChainTrust +
                pindexBest->GetBlockTrust(pregistryMain);

            if (pindexBest->bnChainTrust != bnChainTrust)
            {
                pindexBest->bnChainTrust = bnChainTrust;
                CDiskBlockIndex diskIndexBest(pindexBest);
                WriteDiskBlockIndex("LoadBlockIndex",
                                    pmemIndexBest,
                                    diskIndexBest,
                                    this);
            }
        }
        else
        {
            printf("LoadBlockIndex() : TSNH pindexBest->pprev is null\n");
            CBigNum bnChainTrust = pindexBest->GetBlockTrust(pregistryMain);
            if (pindexBest->bnChainTrust != bnChainTrust)
            {
                pindexBest->bnChainTrust = bnChainTrust;
                CDiskBlockIndex diskIndexBest(pindexBest);
                WriteDiskBlockIndex("LoadBlockIndex",
                                    pmemIndexBest,
                                    diskIndexBest,
                                    this);
            }
        }

        nReplayHeight = pindexBest->nHeight;
    }
    else
    {
        // estimate trust as minimum possible
        if (pindexBest->bnChainTrust != diskIndexBestReplay.bnChainTrust)
        {

            pindexBest->bnChainTrust = diskIndexBestReplay.bnChainTrust;
            CDiskBlockIndex diskIndexBest(pindexBest);
            WriteDiskBlockIndex("LoadBlockIndex",
                                pmemIndexBest,
                                diskIndexBest,
                                this);
        }
        SetForkPoint(pmemIndexFork,
                     nForkHeight,
                     pmemIndexBestReplay,
                     diskIndexBestReplay.nHeight);
    }

    bnBestChainTrust = pindexBest->bnChainTrust;

    printf(
        "LoadBlockIndex():  height=%d  trust=%s  date=%s\n   "
        "hashBestChain=%s\n",
        nBestHeight,
        CBigNum(bnBestChainTrust).ToString().c_str(),
        DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str(),
        hashBestChain.ToString().c_str());

    // NovaCoin: load hashSyncCheckpoint
    if (!ReadSyncCheckpoint(Checkpoints::hashSyncCheckpoint))
    {
        return error(
            "CTxDB::LoadBlockIndex() : hashSyncCheckpoint not loaded");
    }
    printf("LoadBlockIndex(): synchronized checkpoint %s\n",
           Checkpoints::hashSyncCheckpoint.ToString().c_str());

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
    CBlockMemIndex *pmemIndexStart = pmemIndexBest;
    for (int i = 1; i < nCheckDepth; ++i)
    {
        // pprev is genesis
        if (pmemIndexStart->pprev->pprev == NULL)
        {
            break;
        }
        pmemIndexStart = pmemIndexStart->pprev;
        nStartHeight -= 1;
    }

    // replay check registry from snapshot height to start
    QPRegistry registryCheck(pregistryMain);
    QPRegistry *pregistryCheck = &registryCheck;
    CTxDB txdb = *this;
    GetRegistrySnapshot(txdb, nStartHeight - 1, pregistryCheck);
    // TODO: refactor (see main.cpp)
    uint256 blockHash = pregistryCheck->GetBlockHash();
    CBlockMemIndex *pmemIndexCurrent = mapBlockIndex[blockHash];
    while ((pmemIndexCurrent->pnext != NULL) &&
           (pmemIndexCurrent != pmemIndexStart->pprev))
    {
        pmemIndexCurrent = pmemIndexCurrent->pnext;
        CDiskBlockIndex diskIndexCurrent;
        ReadDiskBlockIndex("LoadBlockIndex",
                           pmemIndexCurrent,
                           diskIndexCurrent,
                           this);

        // ensure all main chain indices have been upgraded on disk with
        // bnChainTrust
        assert (diskIndexCurrent.bnChainTrust >= 0);

        pregistryCheck->UpdateOnNewBlock(&diskIndexCurrent,
                                         QPRegistry::ALL_SNAPS);

        if ((pmemIndexCurrent->pnext != NULL) &&
            (!pmemIndexCurrent->pnext->IsInMainChain()))
        {
            // this should never happen because we just backtracked from best
            printf(
                "LoadBlockIndex(): TSNH pmemIndexCurrent->pnext (%s) not in main "
                "chain\n",
                pmemIndexCurrent->pnext->GetBlockHash().ToString().c_str());
            break;
        }
    }

    diskIndex.SetNull();
    diskIndexPrev.SetNull();
    map<pair<unsigned int, unsigned int>, CBlockMemIndex*> mapBlockPos;
    for (CBlockMemIndex* pmemIndex = pmemIndexStart;
         pmemIndex && pmemIndex->pnext;
         pmemIndex = pmemIndex->pnext)
    {
        ReadDiskBlockIndex("LoadBlockIndex",
                           pmemIndex,
                           diskIndex,
                           this);

        if (fRequestShutdown || (diskIndex.nHeight > nBestHeight))
        {
            break;
        }

        ReadDiskBlockIndex("LoadBlockIndex",
                           pmemIndex->pprev,
                           diskIndexPrev,
                           this);

        CBlock block;
        if (!block.ReadFromDisk(&diskIndex))
        {
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        }

        // check level 1: verify block validity
        // check level 7: verify block signature too
        vector<QPTxDetails> vDeets;
        if ((nCheckLevel > 0) &&
            !block.CheckBlock(pregistryCheck, vDeets, &diskIndexPrev))
        {
            printf("LoadBlockIndex() : *** found bad block at %d, hash=%s\n",
                   diskIndex.nHeight, diskIndex.GetBlockHash().ToString().c_str());

            if (!pmemIndexFork || (diskIndexPrev.nHeight < nForkHeight))
            {
                SetForkPoint(pmemIndexFork,
                             nForkHeight,
                             pmemIndex->pprev,
                             diskIndexPrev.nHeight);
            }
            break;
        }

        // check level 2: verify transaction index validity
        if (nCheckLevel > 1)
        {
            pair<unsigned int, unsigned int> pos = make_pair(
                pmemIndex->nFile,
                pmemIndex->nBlockPos);
            mapBlockPos[pos] = pmemIndex;
            BOOST_FOREACH (const CTransaction& tx, block.vtx)
            {
                uint256 hashTx = tx.GetHash();
                CTxIndex txindex;
                if (ReadTxIndex(hashTx, txindex))
                {
                    // check level 3: checker transaction hashes
                    if (nCheckLevel > 2 ||
                        pmemIndex->nFile != txindex.pos.nFile ||
                        pmemIndex->nBlockPos != txindex.pos.nBlockPos)
                    {
                        // either an error or a duplicate transaction
                        CTransaction txFound;
                        if (!txFound.ReadFromDisk(txindex.pos))
                        {
                            printf("LoadBlockIndex() : *** cannot read "
                                   "mislocated transaction %s\n",
                                   hashTx.ToString().c_str());
                            SetForkPoint(pmemIndexFork,
                                         nForkHeight,
                                         pmemIndex->pprev,
                                         diskIndexPrev.nHeight);
                            break;
                        }
                        else if (txFound.GetHash() != hashTx)
                        {
                            // not a duplicate tx
                            printf("LoadBlockIndex(): *** invalid tx position for %s\n",
                                   hashTx.ToString().c_str());
                            SetForkPoint(pmemIndexFork,
                                         nForkHeight,
                                         pmemIndex->pprev,
                                         diskIndexPrev.nHeight);
                            break;
                        }
                    }

                    // check level 4: check whether spent txouts were spent
                    // within the main chain
                    unsigned int nOutput = 0;
                    if (nCheckLevel > 3)
                    {
                        BOOST_FOREACH (const CDiskTxPos& txpos, txindex.vSpent)
                        {
                            if (!txpos.IsNull())
                            {
                                pair<unsigned int, unsigned int>
                                    posFind = make_pair(txpos.nFile,
                                                        txpos.nBlockPos);
                                if (!mapBlockPos.count(posFind))
                                {
                                    printf("LoadBlockIndex(): *** found bad "
                                           "spend at %d, hashBlock=%s, "
                                           "hashTx=%s\n",
                                           diskIndex.nHeight,
                                           pmemIndex->GetBlockHash().ToString().c_str(),
                                           hashTx.ToString().c_str());
                                    SetForkPoint(pmemIndexFork,
                                                 nForkHeight,
                                                 pmemIndex->pprev,
                                                 diskIndexPrev.nHeight);
                                    break;
                                }
                                // check level 6: check whether spent txouts
                                // were spent by a valid transaction that
                                // consume them
                                if (nCheckLevel > 5)
                                {
                                    CTransaction txSpend;
                                    if (!txSpend.ReadFromDisk(txpos))
                                    {
                                        printf("LoadBlockIndex(): *** cannot "
                                               "read spending transaction of "
                                               "%s:%i from disk\n",
                                               hashTx.ToString().c_str(),
                                               nOutput);
                                        SetForkPoint(pmemIndexFork,
                                                     nForkHeight,
                                                     pmemIndex->pprev,
                                                     diskIndexPrev.nHeight);
                                        break;
                                    }
                                    else if (!txSpend.CheckTransaction())
                                    {
                                        printf("LoadBlockIndex(): *** "
                                               "spending transaction of %s:%i "
                                               "is invalid\n",
                                               hashTx.ToString().c_str(),
                                               nOutput);
                                        SetForkPoint(pmemIndexFork,
                                                     nForkHeight,
                                                     pmemIndex->pprev,
                                                     diskIndexPrev.nHeight);
                                        break;
                                    }
                                    else
                                    {
                                        bool fFound = false;
                                        BOOST_FOREACH (const CTxIn& txin,
                                                       txSpend.vin)
                                        {
                                            if (txin.prevout.hash == hashTx &&
                                                txin.prevout.n == nOutput)
                                            {
                                                fFound = true;
                                            }
                                        }
                                        if (!fFound)
                                        {
                                            printf("LoadBlockIndex(): *** "
                                                   "spending transaction of "
                                                   "%s:%i does not spend it\n",
                                                   hashTx.ToString().c_str(),
                                                   nOutput);
                                            SetForkPoint(pmemIndexFork,
                                                         nForkHeight,
                                                         pmemIndex->pprev,
                                                         diskIndexPrev.nHeight);
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
                if (nCheckLevel > 4)
                {
                    BOOST_FOREACH (const CTxIn& txin, tx.vin)
                    {
                        CTxIndex txindex;
                        if (ReadTxIndex(txin.prevout.hash, txindex))
                        {
                            if (txindex.vSpent.size() - 1 < txin.prevout.n ||
                                txindex.vSpent[txin.prevout.n].IsNull())
                            {
                                printf("LoadBlockIndex(): *** found unspent "
                                       "prevout %s:%i in %s\n",
                                       txin.prevout.hash.ToString().c_str(),
                                       txin.prevout.n,
                                       hashTx.ToString().c_str());
                                SetForkPoint(pmemIndexFork,
                                             nForkHeight,
                                             pmemIndex->pprev,
                                             diskIndexPrev.nHeight);
                                break;
                            }
                        }
                    }
                }
                // FIXME: is there a check level for qpos?
            }
        }
        pregistryCheck->UpdateOnNewBlock(&diskIndex, QPRegistry::NO_SNAPS);
    }

    printf("LoadBlockIndex(): building block index lookup\n");
    CBlockMemIndex* pmemIndexLookup = pmemIndexBest;
    int progress = 1;
    for (int i = pindexBest->nHeight; i >= 0; --i)
    {
        if (!pmemIndexLookup)
        {
            return error("LoadBlockIndex() : unexpected null index");
        }
        mapBlockLookup[i] = pmemIndexLookup;
        if (progress % 100000 == 0)
        {
            printf("LoadBlockIndex(): created %d lookups\n", progress);
        }
        progress += 1;
        pmemIndexLookup = pmemIndexLookup->pprev;
    }

    if (pmemIndexFork && (pmemIndexFork != pmemIndexBest) && !fRequestShutdown)
    {
        // Reorg back to the fork
        // Consistency check: verify nForkHeight matches pmemIndexFork
        int nForkHeightCheck = GetMemIndexHeight("LoadBlockIndex",
                                                  pmemIndexFork,
                                                  this);
        if (nForkHeight != nForkHeightCheck)
        {
            return error("LoadBlockIndex() : TSNH nForkHeight mismatch: "
                         "tracked=%d, actual=%d",
                         nForkHeight, nForkHeightCheck);
        }
        printf("LoadBlockIndex() : moving best pointer back to block %d\n",
               nForkHeight);
        CBlock block;
        if (!block.ReadFromDisk(pmemIndexFork))
        {
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        }
        CTxDB txdb;
        block.SetBestChain(txdb, pmemIndexFork);
    }

    // Require a full clean load with verification before the database
    // gets certified with the latest version.
    WriteVersion(DATABASE_VERSION);

    return true;
}

bool CTxDB::RegistrySnapshotIsViable(int nHeight)
{
    return IsViable(make_pair(string("registrySnapshot"), nHeight));
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
    {
        if (!Read(make_pair(string("registrySnapshot"), nHeight), registry))
        {
            return error("ReadRegistrySnapshot(): could not read registry at %d", nHeight);
        }
    }
    if (registry.GetBlockHeight() != nHeight)
    {
        return error("ReadRegistrySnapshot(): registry height mismatch:"
                     "want=%d, registry=%d\n", nHeight, registry.GetBlockHeight());
    }
    return true;
}

bool CTxDB::EraseRegistrySnapshot(int nHeight)
{
    return Erase(make_pair(string("registrySnapshot"), nHeight));
}
