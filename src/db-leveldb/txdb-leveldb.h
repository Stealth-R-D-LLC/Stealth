// Copyright (c) 2009-2012 The Bitcoin Developers.
// Authored by Google, Inc.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LEVELDB_H
#define BITCOIN_LEVELDB_H

#include "main.h"

#include "ExploreConstants.hpp"

#include <map>
#include <string>
#include <vector>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

class ExploreTx;

///////////////////////////////////////////////////////////////////////////////
// LevelDB Keys
///////////////////////////////////////////////////////////////////////////////
// generalized explore keys
typedef pair<exploreKey_t, string> ss_key_t;
// amount key (e.g. balances)
typedef pair<ss_key_t, int64_t> amount_key_t;
// in-out lookup key
typedef pair<uint256, int> txidn_key_t;
typedef pair<ss_key_t, txidn_key_t> lookup_key_t;
///////////////////////////////////////////////////////////////////////////////


template<typename K>
std::string DBKeyToString(K& key)
{
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(1000);
    ssKey << key;
    return ssKey.str();
}

// Class that provides access to a LevelDB. Note that this class is frequently
// instantiated on the stack and then destroyed again, so instantiation has to
// be very cheap. Unfortunately that means, a CTxDB instance is actually just a
// wrapper around some global state.
//
// A LevelDB is a key/value store that is optimized for fast usage on hard
// disks. It prefers long read/writes to seeks and is based on a series of
// sorted key/value mapping files that are stacked on top of each other, with
// newer files overriding older files. A background thread compacts them
// together when too many files stack up.
//
// Learn more: http://code.google.com/p/leveldb/
class CTxDB
{
public:
    CTxDB(const char* pszMode="r+");
    ~CTxDB() {
        // Note that this is not the same as Close() because it deletes only
        // data scoped to this TxDB object.
        delete activeBatch;
    }

    bool ActiveBatchIsNull()
    {
        return !bool(activeBatch);
    }

    // Destroys the underlying shared global state accessed by this TxDB.
    void Close();

private:
    leveldb::DB *pdb;  // Points to the global instance.

    // A batch stores up writes and deletes for atomic application. When this
    // field is non-NULL, writes/deletes go there instead of directly to disk.
    leveldb::WriteBatch *activeBatch;
    leveldb::Options options;
    bool fReadOnly;
    int nVersion;

protected:
    // Returns true and sets (value,false) if activeBatch contains the given key
    // or leaves value alone and sets deleted = true if activeBatch contains a
    // delete for it.
    bool ScanBatch(const CDataStream &key, std::string *value, bool *deleted) const;

    template<typename K, typename T>
    bool Read(const K& key, T& value, bool& fOk)
    {
        fOk = true;
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        std::string strValue;

        bool readFromDb = true;
        if (activeBatch) {
            // First we must search for it in the currently pending set of
            // changes to the db. If not found in the batch, go on to read disk.
            bool deleted = false;
            readFromDb = ScanBatch(ssKey, &strValue, &deleted) == false;
            if (deleted) {
                return false;
            }
        }
        if (readFromDb) {
            leveldb::Status status = pdb->Get(leveldb::ReadOptions(),
                                              ssKey.str(), &strValue);
            if (!status.ok())
            {
                if (status.IsNotFound())
                {
                    return false;
                }
                // Some unexpected error.
                printf("LevelDB read failure: %s\n", status.ToString().c_str());
                fOk = false;
                return false;
            }
        }
        // Unserialize value
        try {
            CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(),
                                SER_DISK, CLIENT_VERSION);
            ssValue >> value;
        }
        catch (std::exception &e) {
            fOk = false;
            return false;
        }
        return true;
    }

    template<typename K, typename T>
    bool Read(const K& key, T& value)
    {
        bool fReadOk;
        return Read(key, value, fReadOk);
    }

    template<typename K, typename T>
    bool Write(const K& key, const T& value)
    {
        if (fReadOnly)
            assert(!"Write called on database in read-only mode");

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        ssValue << value;

        if (activeBatch) {
            activeBatch->Put(ssKey.str(), ssValue.str());
            return true;
        }
        leveldb::Status status = pdb->Put(leveldb::WriteOptions(), ssKey.str(), ssValue.str());
        if (!status.ok()) {
            printf("LevelDB write failure: %s\n", status.ToString().c_str());
            return false;
        }
        return true;
    }

    template<typename K>
    bool Erase(const K& key)
    {
        if (!pdb)
        {
            return false;
        }
        if (fReadOnly)
            assert(!"Erase called on database in read-only mode");

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        if (activeBatch)
        {
            activeBatch->Delete(ssKey.str());
            return true;
        }
        leveldb::Status status = pdb->Delete(leveldb::WriteOptions(), ssKey.str());
        if (!status.ok())
        {
            if (status.IsNotFound())
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        return true;
    }


    template<typename K>
    bool Exists(const K& key)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        std::string unused;

        if (activeBatch) {
            bool deleted;
            if (ScanBatch(ssKey, &unused, &deleted) && !deleted) {
                return true;
            }
        }

        leveldb::Status status = pdb->Get(leveldb::ReadOptions(), ssKey.str(), &unused);
        return status.IsNotFound() == false;
    }

    // IsViable() has slightly different semantics from Exist().
    // The latter will return true even when the record is marked
    // for deletion in the active batch but hasn't yet been deleted
    // on disk. The former will return true only if the record
    // is both found somewhere and not marked for deletion anywhere.
    template<typename K>
    bool IsViable(const K& key)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        std::string unused;

        if (activeBatch)
        {
            bool deleted;
            if (ScanBatch(ssKey, &unused, &deleted))
            {
                if (deleted)
                {
                    return false;
                }
                else
                {
                    return true;
                }
            }
        }

        leveldb::Status status = pdb->Get(leveldb::ReadOptions(),ssKey.str(), &unused);
        return status.IsNotFound() == false;
    }

    template<typename K, typename T>
    bool ReadRecord(const K& key, T& value)
    {
        bool fReadOk;
        Read(key, value, fReadOk);
        return fReadOk;
    }

    template<typename K>
    bool RemoveRecord(const K& key)
    {
        if (IsViable(key))
        {
            return Erase(key);
        }
        return false;
    }

public:
    bool TxnBegin();
    bool TxnCommit();
    bool TxnAbort()
    {
        delete activeBatch;
        activeBatch = NULL;
        return true;
    }

    bool ReadVersion(int& nVersion)
    {
        nVersion = 0;
        return Read(std::string("version"), nVersion);
    }

    bool WriteVersion(int nVersion)
    {
        return Write(std::string("version"), nVersion);
    }

    bool EraseStartsWith(const string& strSentinel,
                         const string& strSearch,
                         bool fActiveBatchOK);

    bool WriteExploreSentinel(int value=0);
    bool ReadAddrQty(const exploreKey_t& t, const std::string& addr, int& qtyRet);
    bool WriteAddrQty(const exploreKey_t& t, const std::string& addr, const int& qty);

     // Parameters - t:type, addr:address, qty:quantity,
     //              value:input_info|output_info|inout_lookup|inout_list
    template<typename T>
    bool ReadAddrTx(const exploreKey_t& t, const string& addr, const int& qty,
                    T& value)
    {
        value.SetNull();
        pair<ss_key_t, int> key = make_pair(make_pair(t, addr), qty);
        return ReadRecord(key, value);
    }
    template<typename T>
    bool WriteAddrTx(const exploreKey_t& t, const string& addr, const int& qty,
                     const T& value)
    {
        pair<ss_key_t, int> key = make_pair(make_pair(t, addr), qty);
        return Write(key, value);
    }

    bool RemoveAddrTx(const exploreKey_t& t, const std::string& addr, const int& qty);
    bool AddrTxIsViable(const exploreKey_t& t, const std::string& addr, const int& qty);

    template<typename T>
    bool ReadAddrList(const exploreKey_t& t, const string& addr, const int& qty,
                      T& value)
    {
        value.SetNull();
        pair<ss_key_t, int> key = make_pair(make_pair(t, addr), qty);
        return ReadRecord(key, value);
    }
    template<typename T>
    bool WriteAddrList(const exploreKey_t& t, const string& addr, const int& qty,
                       const T& value)
    {
        pair<ss_key_t, int> key = make_pair(make_pair(t, addr), qty);
        return Write(key, value);
    }

    bool RemoveAddrList(const exploreKey_t& t, const std::string& addr, const int& qty);
    bool AddrListIsViable(const exploreKey_t& t, const std::string& addr, const int& qty);

    bool ReadAddrLookup(const exploreKey_t& t, const std::string& addr,
                        const uint256& txid, const int& n,
                        int& qtyRet);
    bool WriteAddrLookup(const exploreKey_t& t, const std::string& addr,
                         const uint256& txid, const int& n,
                         const int& qty);
    bool RemoveAddrLookup(const exploreKey_t& t, const std::string& addr,
                          const uint256& txid, const int& n);
    bool AddrLookupIsViable(const exploreKey_t& t, const std::string& addr,
                            const uint256& txid, const int& n);
    bool ReadAddrValue(const exploreKey_t& t, const std::string& addr,
                       int64_t& vRet);
    bool WriteAddrValue(const exploreKey_t& t, const std::string& addr,
                        const int64_t& v);
    bool AddrValueIsViable(const exploreKey_t& t, const std::string& addr);
    bool ReadAddrSet(const exploreKey_t& t, const int64_t b,
                     std::set<std::string>& sRet);
    bool WriteAddrSet(const exploreKey_t& t, const int64_t b, const std::set<std::string>& s);
    bool RemoveAddrSet(const exploreKey_t& t, const int64_t b);

    bool ReadExploreTx(const uint256& txid, ExploreTx& extxRet);
    bool WriteExploreTx(const uint256& txid, const ExploreTx& extx);
    bool RemoveExploreTx(const uint256& txid);

    bool ReadTxIndex(uint256 hash, CTxIndex& txindex);
    bool UpdateTxIndex(uint256 hash, const CTxIndex& txindex);
    bool AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight);
    bool EraseTxIndex(const CTransaction& tx);
    bool ContainsTx(uint256 hash);
    bool ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(uint256 hash, CTransaction& tx);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool ReadHashBestChain(uint256& hashBestChain);
    bool WriteHashBestChain(uint256 hashBestChain);
    bool ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust);
    bool WriteBestInvalidTrust(CBigNum bnBestInvalidTrust);
    bool ReadSyncCheckpoint(uint256& hashCheckpoint);
    bool WriteSyncCheckpoint(uint256 hashCheckpoint);
    bool ReadCheckpointPubKey(std::string& strPubKey);
    bool WriteCheckpointPubKey(const std::string& strPubKey);
    bool LoadBlockIndex();
    bool WriteRegistrySnapshot(int nHeight, const QPRegistry& registry);
    bool ReadRegistrySnapshot(int nHeight, QPRegistry &registry);
    bool RegistrySnapshotIsViable(int nHeight);
    bool ReadBestRegistrySnapshot(QPRegistry &registry);
    bool EraseRegistrySnapshot(int nHeight);
};


#endif // BITCOIN_DB_H
