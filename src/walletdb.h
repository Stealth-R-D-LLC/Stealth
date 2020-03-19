// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_WALLETDB_H
#define BITCOIN_WALLETDB_H

#include "db.h"
#include "base58.h"
#include "stealthaddress.h"

class CKeyPool;
class CAccount;
class CAccountingEntry;

/** Error statuses for the wallet database */
enum DBErrors
{
    DB_LOAD_OK,
    DB_CORRUPT,
    DB_NONCRITICAL_ERROR,
    DB_TOO_NEW,
    DB_LOAD_FAIL,
    DB_NEED_REWRITE
};

class CKeyMetadata
{
public:
    static const int CURRENT_VERSION=1;
    int nVersion;
    int64_t nCreateTime; // 0 means unknown

    CKeyMetadata()
    {
        SetNull();
    }
    CKeyMetadata(int64_t nCreateTime_)
    {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = nCreateTime_;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nCreateTime);
    )

    void SetNull()
    {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = 0;
    }
};

class CStealthKeyMetadata
{
// -- used to get secret for keys created by stealth transaction with wallet locked
public:
    CStealthKeyMetadata() {};

    CStealthKeyMetadata(CPubKey pkEphem_, CPubKey pkScan_)
    {
        pkEphem = pkEphem_;
        pkScan = pkScan_;
    };

    CPubKey pkEphem;
    CPubKey pkScan;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(pkEphem);
        READWRITE(pkScan);
    )

};

/** Access to the wallet database (wallet.dat) */
class CWalletDB : public CDB
{
public:
    CWalletDB(std::string strFilename, const char* pszMode="r+") : CDB(strFilename.c_str(), pszMode)
    {
    }
private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);
public:
    Dbc* GetAtCursor()
        {
            return GetCursor();
        }

        Dbc* GetTxnCursor()
        {
            if (!pdb)
                return NULL;

            DbTxn* ptxnid = activeTxn; // call TxnBegin first

            Dbc* pcursor = NULL;
            int ret = pdb->cursor(ptxnid, &pcursor, 0);
            if (ret != 0)
                return NULL;
            return pcursor;
        }

        DbTxn* GetAtActiveTxn()
        {
            return activeTxn;
        }
    bool WriteName(const std::string& strAddress, const std::string& strName);

    bool EraseName(const std::string& strAddress);

    bool WriteTx(uint256 hash, const CWalletTx& wtx)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("tx"), hash), wtx);
    }

    bool EraseTx(uint256 hash)
    {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("tx"), hash));
    }
    bool WriteStealthKeyMeta(const CKeyID& keyId, const CStealthKeyMetadata& sxKeyMeta)
        {
            nWalletDBUpdated++;
            return Write(std::make_pair(std::string("sxKeyMeta"), keyId), sxKeyMeta, true);
        }

        bool EraseStealthKeyMeta(const CKeyID& keyId)
        {
            nWalletDBUpdated++;
            return Erase(std::make_pair(std::string("sxKeyMeta"), keyId));
        }

        bool WriteStealthAddress(const CStealthAddress& sxAddr)
        {
            nWalletDBUpdated++;

            return Write(std::make_pair(std::string("sxAddr"), sxAddr.scan_pubkey), sxAddr, true);
        }

        bool ReadStealthAddress(CStealthAddress& sxAddr)
        {
            // -- set scan_pubkey before reading
            return Read(std::make_pair(std::string("sxAddr"), sxAddr.scan_pubkey), sxAddr);
        }
    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata &keyMeta)
    {
        nWalletDBUpdated++;
        if(!Write(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta))
            return false;
        return Write(std::make_pair(std::string("key"), vchPubKey.Raw()), vchPrivKey, false);
    }

    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata &keyMeta)
        {
            nWalletDBUpdated++;
            bool fEraseUnencryptedKey = true;

            if(!Write(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta))
                return false;

            if (!Write(std::make_pair(std::string("ckey"), vchPubKey.Raw()), vchCryptedSecret, true))
                return false;
            if (fEraseUnencryptedKey)
            {
                Erase(std::make_pair(std::string("key"), vchPubKey.Raw()));
                Erase(std::make_pair(std::string("wkey"), vchPubKey.Raw()));
            }
            return true;
        }

    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
    }

    bool WriteCScript(const uint160& hash, const CScript& redeemScript)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("cscript"), hash), redeemScript, false);
    }

    bool WriteBestBlock(const CBlockLocator& locator)
    {
        nWalletDBUpdated++;
        return Write(std::string("bestblock"), locator);
    }

    bool ReadBestBlock(CBlockLocator& locator)
    {
        return Read(std::string("bestblock"), locator);
    }

    bool WriteOrderPosNext(int64_t nOrderPosNext)
    {
        nWalletDBUpdated++;
        return Write(std::string("orderposnext"), nOrderPosNext);
    }

    bool WriteDefaultKey(const CPubKey& vchPubKey)
    {
        nWalletDBUpdated++;
        return Write(std::string("defaultkey"), vchPubKey.Raw());
    }

    bool ReadPool(int64_t nPool, CKeyPool& keypool)
    {
        return Read(std::make_pair(std::string("pool"), nPool), keypool);
    }

    bool WritePool(int64_t nPool, const CKeyPool& keypool)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("pool"), nPool), keypool);
    }

    bool ErasePool(int64_t nPool)
    {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("pool"), nPool));
    }

    // Settings are no longer stored in wallet.dat; these are
    // used only for backwards compatibility:
    template<typename T>
    bool ReadSetting(const std::string& strKey, T& value)
    {
        return Read(std::make_pair(std::string("setting"), strKey), value);
    }
    template<typename T>
    bool WriteSetting(const std::string& strKey, const T& value)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("setting"), strKey), value);
    }
    bool EraseSetting(const std::string& strKey)
    {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("setting"), strKey));
    }

    bool WriteMinVersion(int nVersion)
    {
        return Write(std::string("minversion"), nVersion);
    }

    bool ReadAccount(const std::string& strAccount, CAccount& account);
    bool WriteAccount(const std::string& strAccount, const CAccount& account);
private:
    bool WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry);
public:
    bool WriteAccountingEntry(const CAccountingEntry& acentry);
    int64_t GetAccountCreditDebit(const std::string& strAccount);
    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& acentries);

    DBErrors ReorderTransactions(CWallet*);
    DBErrors LoadWallet(CWallet* pwallet);
    static bool Recover(CDBEnv& dbenv, std::string filename, bool fOnlyKeys);
    static bool Recover(CDBEnv& dbenv, std::string filename);
};

#endif // BITCOIN_WALLETDB_H
