// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "txdb-leveldb.h"
#include "wallet.h"
#include "walletdb.h"
#include "crypter.h"
#include "ui_interface.h"
#include "base58.h"
#include "kernel.h"
#include "coincontrol.h"
#include "XORShift1024Star.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>

using namespace std;
extern int nStakeMaxAge;
unsigned int nStakeSplitAge = 18 * 60 * 60;
int64_t nStakeCombineThreshold = 500 * COIN;

extern QPRegistry* pregistryMain;

//////////////////////////////////////////////////////////////////////////////
//
// mapWallet
//

struct CompareValueOnly
{
    bool operator()(const pair<int64_t, pair<const CWalletTx*, unsigned int> >& t1,
                    const pair<int64_t, pair<const CWalletTx*, unsigned int> >& t2) const
    {
        return t1.first < t2.first;
    }
};

CPubKey CWallet::GenerateNewKey()
{
    bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    RandAddSeedPerfmon();
    CKey key;
    key.MakeNewKey(fCompressed);

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed)
        SetMinVersion(FEATURE_COMPRPUBKEY);

    CPubKey pubkey = key.GetPubKey();
    //Create new metadata
    int64_t nCreationTime = GetTime();
    mapKeyMetadata[pubkey.GetID()] = CKeyMetadata(nCreationTime);
    if (!nTimeFirstKey || nCreationTime < nTimeFirstKey)
        nTimeFirstKey = nCreationTime;

    if (!AddKey(key))
        throw std::runtime_error("CWallet::GenerateNewKey() : AddKey failed");
    return key.GetPubKey();
}

bool CWallet::AddKey(const CKey& key)
{
    CPubKey pubkey = key.GetPubKey();
    if (!CCryptoKeyStore::AddKey(key))
        return false;
    if (!fFileBacked)
        return true;
    if (!IsCrypted())
        return CWalletDB(strWalletFile).WriteKey(pubkey, key.GetPrivKey(),mapKeyMetadata[pubkey.GetID()]);
    return true;
}

bool CWallet::AddCryptedKey(const CPubKey &vchPubKey, const vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedKey(vchPubKey, vchCryptedSecret, mapKeyMetadata[vchPubKey.GetID()]);
        else
            return CWalletDB(strWalletFile).WriteCryptedKey(vchPubKey, vchCryptedSecret, mapKeyMetadata[vchPubKey.GetID()]);
    }
    return false;
}

bool CWallet::LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &meta)
{
    if(meta.nCreateTime &&(!nTimeFirstKey || meta.nCreateTime < nTimeFirstKey))
        nTimeFirstKey = meta.nCreateTime;
    mapKeyMetadata[pubkey.GetID()] = meta;
    return true;
}
bool CWallet::LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret);
}

bool CWallet::HaveCScript(const CScriptID& hash) const
{
    return CCryptoKeyStore::HaveCScript(hash);
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
    {
        return false;
    }
    if (!fFileBacked)
    {
        return true;
    }
    return CWalletDB(strWalletFile).WriteCScript(Hash160(redeemScript), redeemScript);
}

// ppcoin: optional setting to unlock wallet for block minting only;
//         serves to disable the trivial sendmoney when OS account compromised
bool fWalletUnlockMintOnly = false;

bool CWallet::LoadCScript(const CScript &redeemScript)
{
    if(redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE){
        std::string strAddr = CBitcoinAddress(redeemScript.GetID()).ToString();
        printf("%s: Warning: This wallet contains a redeemScript of size %" PRIszu " which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n",
               __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr.c_str());
        return true;
    }
    return CCryptoKeyStore::AddCScript(redeemScript);
}

bool CWallet::AddWatchOnly(const CTxDestination &dest)
{
    if (!CCryptoKeyStore::AddWatchOnly(dest))
    {
        return false;
    }
    // No birthday information for watch-only keys.
    nTimeFirstKey = 1;
    if (!fFileBacked)
    {
        return true;
    }
    return CWalletDB(strWalletFile).WriteWatchOnly(dest);
}

bool CWallet::LoadWatchOnly(const CTxDestination &dest)
{
    printf("Loaded %s!\n", CBitcoinAddress(dest).ToString().c_str());
    return CCryptoKeyStore::AddWatchOnly(dest);
}

bool CWallet::Lock()
{
    if (IsLocked())
        return true;
    if(fDebug)
        printf("Locking Wallet.\n");
    {
        LOCK(cs_wallet);
        CWalletDB wdb(strWalletFile);
        CStealthAddress sxAddrTemp;
        std::set<CStealthAddress>::iterator it;
        for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
        {
            if (it->scan_secret.size() < 32)
                continue;
            CStealthAddress &sxAddr = const_cast<CStealthAddress&>(*it);
            if (fDebug)
                printf("Recrypting stealth key %s\n", sxAddr.Encoded().c_str());
            sxAddrTemp.scan_pubkey = sxAddr.scan_pubkey;
            if (!wdb.ReadStealthAddress(sxAddrTemp))
            {
                printf("Error: Failed to read stealth key from db %s\n", sxAddr.Encoded().c_str());
                continue;
            }
            sxAddr.spend_secret = sxAddrTemp.spend_secret;
        };
    }
    return LockKeyStore();
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase, bool lockedOK)
{
    if ((!lockedOK) && (!IsLocked())) {
        return false;
    }

    CCrypter crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH(const MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (!CCryptoKeyStore::Unlock(vMasterKey))
                return false;
            break;
        }
        UnlockStealthAddresses(vMasterKey);
        return true;
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial vMasterKey;
        BOOST_FOREACH(MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey)
                    && UnlockStealthAddresses(vMasterKey))
            {
                int64_t nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime)));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                printf("Wallet passphrase changed to an nDeriveIterations of %i\n", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(strWalletFile).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();
                return true;
            }
        }
    }

    return false;
}

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteBestBlock(loc);
}

// This class implements an addrIncoming entry that causes pre-0.4
// clients to crash on startup if reading a private-key-encrypted wallet.
class CCorruptAddress
{
public:
    IMPLEMENT_SERIALIZE
    (
        if (nType & SER_DISK)
        {
            READWRITE(nSerVersion);
        }
    )
};

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
            nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    if (fFileBacked)
    {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(strWalletFile);
        if (nWalletVersion >= 40000)
        {
            // Versions prior to 0.4.0 did not support the "minversion" record.
            // Use a CCorruptAddress to make them crash instead.
            CCorruptAddress corruptAddress;
            pwalletdb->WriteSetting("addrIncoming", corruptAddress);
        }
        if (nWalletVersion > 40000)
            pwalletdb->WriteMinVersion(nWalletVersion);
        if (!pwalletdbIn)
            delete pwalletdb;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial vMasterKey;
    RandAddSeedPerfmon();

    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    RAND_bytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey(nDerivationMethodIndex);

    RandAddSeedPerfmon();
    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    RAND_bytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64_t nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    printf("Encrypting Wallet with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        if (fFileBacked)
        {
            pwalletdbEncryption = new CWalletDB(strWalletFile);
            if (!pwalletdbEncryption->TxnBegin())
                return false;
            pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);
        }

        if (!EncryptKeys(vMasterKey))
        {
            if (fFileBacked)
                pwalletdbEncryption->TxnAbort();
            exit(1); //We now probably have half of our keys encrypted in memory, and half not...die and let the user reload their unencrypted wallet.
        }
        std::set<CStealthAddress>::iterator it;
        for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
        {
            if (it->scan_secret.size() < 32)
                continue; // TimeVortex Address is not owned
            // -- CStealthAddress is only sorted on spend_pubkey
            CStealthAddress &sxAddr = const_cast<CStealthAddress&>(*it);

            if (fDebug)
                printf("Encrypting stealth key %s\n", sxAddr.Encoded().c_str());

            std::vector<unsigned char> vchCryptedSecret;

            CSecret vchSecret;
            vchSecret.resize(32);
            memcpy(&vchSecret[0], &sxAddr.spend_secret[0], 32);

            uint256 iv = Hash(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end());
            if (!EncryptSecret(vMasterKey, vchSecret, iv, vchCryptedSecret))
            {
                printf("Error: Failed encrypting stealth key %s\n", sxAddr.Encoded().c_str());
                continue;
            };

            sxAddr.spend_secret = vchCryptedSecret;
            pwalletdbEncryption->WriteStealthAddress(sxAddr);
        };
        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (fFileBacked)
        {
            if (!pwalletdbEncryption->TxnCommit())
                exit(1); //We now have keys encrypted in memory, but no on disk...die to avoid confusion and let the user reload their unencrypted wallet.

            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
        }

        Lock();
        Unlock(strWalletPassphrase);
        NewKeyPool();
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        CDB::Rewrite(strWalletFile);

    }
    NotifyStatusChanged(this);

    return true;
}

int64_t CWallet::IncOrderPosNext(CWalletDB *pwalletdb)
{

    int64_t nRet = nOrderPosNext++;
    if (pwalletdb) {
        pwalletdb->WriteOrderPosNext(nOrderPosNext);
    } else {
        CWalletDB(strWalletFile).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

CWallet::TxItems CWallet::OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount)
{
    CWalletDB walletdb(strWalletFile);

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-order multimap.
    TxItems txOrdered;

    // Note: maintaining indices in the database of (account,time) --> txid and (account, time) --> acentry
    // would make this much faster for applications that do this a lot.
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        CWalletTx* wtx = &((*it).second);
        txOrdered.insert(make_pair(wtx->nOrderPos, TxPair(wtx, (CAccountingEntry*)0)));
    }
    acentries.clear();
    walletdb.ListAccountCreditDebit(strAccount, acentries);
    BOOST_FOREACH(CAccountingEntry& entry, acentries)
    {
        txOrdered.insert(make_pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));
    }

    return txOrdered;
}

void CWallet::WalletUpdateSpent(const CTransaction &tx, bool fBlock)
{
    // Anytime a signature is successfully verified, it's proof the outpoint is spent.
    // Update the wallet spent flag if it doesn't know due to wallet.dat being
    // restored from backup or the user making copies of wallet.dat.
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(const CTxIn& txin, tx.vin)
        {
            map<uint256, CWalletTx>::iterator mi = mapWallet.find(txin.prevout.hash);
            if (mi != mapWallet.end())
            {
                CWalletTx& wtx = (*mi).second;
                if (txin.prevout.n >= wtx.vout.size())
                    printf("WalletUpdateSpent: bad wtx %s\n",
                           wtx.GetHash().ToString().c_str());
                else if (!wtx.IsSpent(txin.prevout.n) &&
                         IsMine(wtx.vout[txin.prevout.n]))
                {
                    printf("WalletUpdateSpent found spent coin %s XST %s\n",
                           FormatMoney(wtx.GetCredit()).c_str(),
                           wtx.GetHash().ToString().c_str());
                    wtx.MarkSpent(txin.prevout.n);
                    wtx.WriteToDisk();
                    NotifyTransactionChanged(this, txin.prevout.hash, CT_UPDATED);
                }
            }
        }
        if (fBlock)
                {
                    uint256 hash = tx.GetHash();
                    map<uint256, CWalletTx>::iterator mi = mapWallet.find(hash);
                    CWalletTx& wtx = (*mi).second;

                    BOOST_FOREACH(const CTxOut& txout, tx.vout)
                    {
                        if (IsMine(txout))
                        {
                            wtx.MarkUnspent(&txout - &tx.vout[0]);
                            wtx.WriteToDisk();
                            NotifyTransactionChanged(this, hash, CT_UPDATED);
                        }
                    }
                }
    }
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn)
{
    uint256 hash = wtxIn.GetHash();
    {
        LOCK(cs_wallet);
        // Inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        wtx.BindWallet(this);
        bool fInsertedNew = ret.second;
        if (fInsertedNew)
        {
            wtx.nTimeReceived = GetAdjustedTime();
            wtx.nOrderPos = IncOrderPosNext();

            wtx.nTimeSmart = wtx.nTimeReceived;
            if (wtxIn.hashBlock != 0)
            {
                if (mapBlockIndex.count(wtxIn.hashBlock))
                {
                    unsigned int latestNow = wtx.nTimeReceived;
                    unsigned int latestEntry = 0;
                    {
                        // Tolerate times up to the last timestamp in the wallet not more than 5 minutes into the future
                        int64_t latestTolerated = latestNow + 300;
                        std::list<CAccountingEntry> acentries;
                        TxItems txOrdered = OrderedTxItems(acentries);
                        for (TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
                        {
                            CWalletTx *const pwtx = (*it).second.first;
                            if (pwtx == &wtx)
                                continue;
                            CAccountingEntry *const pacentry = (*it).second.second;
                            int64_t nSmartTime;
                            if (pwtx)
                            {
                                nSmartTime = pwtx->nTimeSmart;
                                if (!nSmartTime)
                                    nSmartTime = pwtx->nTimeReceived;
                            }
                            else
                                nSmartTime = pacentry->nTime;
                            if (nSmartTime <= latestTolerated)
                            {
                                latestEntry = nSmartTime;
                                if (nSmartTime > latestNow)
                                    latestNow = nSmartTime;
                                break;
                            }
                        }
                    }

                    unsigned int& blocktime = mapBlockIndex[wtxIn.hashBlock]->nTime;
                    wtx.nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
                }
                else
                    printf("AddToWallet() : found %s in block %s not in index\n",
                           wtxIn.GetHash().ToString().substr(0,10).c_str(),
                           wtxIn.hashBlock.ToString().c_str());
            }
        }

        bool fUpdated = false;
        if (!fInsertedNew)
        {
            // Merge
            if (wtxIn.hashBlock != 0 && wtxIn.hashBlock != wtx.hashBlock)
            {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }
            if (wtxIn.nIndex != -1 && (wtxIn.vMerkleBranch != wtx.vMerkleBranch || wtxIn.nIndex != wtx.nIndex))
            {
                wtx.vMerkleBranch = wtxIn.vMerkleBranch;
                wtx.nIndex = wtxIn.nIndex;
                fUpdated = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
            {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
            fUpdated |= wtx.UpdateSpent(wtxIn.vfSpent);
        }

        //// debug print
        printf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString().c_str(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

        // Write to disk
        if (fInsertedNew || fUpdated)
            if (!wtx.WriteToDisk())
                return false;

#ifndef QT_GUI
        // If default receiving address gets used, replace it with a new one
        if (vchDefaultKey.IsValid()) {
        CScript scriptDefaultKey;
        scriptDefaultKey.SetDestination(vchDefaultKey.GetID());
        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            if (txout.scriptPubKey == scriptDefaultKey)
            {
                CPubKey newDefaultKey;
                if (GetKeyFromPool(newDefaultKey, false))
                {
                    SetDefaultKey(newDefaultKey);
                    SetAddressBookName(vchDefaultKey.GetID(), "");
                }
            }
        }}
#endif
        // since AddToWallet is called directly for self-originating transactions, check for consumption of own coins
        WalletUpdateSpent(wtx, (wtxIn.hashBlock != 0));

        // Notify UI of new or updated transaction
        NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

		// notify an external script when a wallet transaction comes in or is updated
        std::string strCmd = GetArg("-walletnotify", "");

        if ( !strCmd.empty())
        {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }
    }
    return true;
}

// Add a transaction to the wallet, or update it.
// pblock is optional, but should be provided if the transaction is known to be in a block.
// If fUpdate is true, existing transactions will be updated.
bool CWallet::AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate, bool fFindBlock)
{
    uint256 hash = tx.GetHash();
    {
        LOCK(cs_wallet);
        bool fExisted = mapWallet.count(hash);
        if (fExisted && !fUpdate) return false;
        mapValue_t mapNarr;
        FindStealthTransactions(tx, mapNarr);
        if (fExisted || IsMine(tx) || IsFromMe(tx))
        {
            CWalletTx wtx(this,tx);
            if (!mapNarr.empty())
            {
                wtx.mapValue.insert(mapNarr.begin(), mapNarr.end());
            }
            // Get merkle branch if transaction was found in a block
            if (pblock)
            {
                wtx.SetMerkleBranch(pblock);
            }
            return AddToWallet(wtx);
        }
        else
        {
            WalletUpdateSpent(tx);
        }
    }
    return false;
}

bool CWallet::EraseFromWallet(uint256 hash)
{
    if (!fFileBacked)
    {
        return false;
    }
    {
        LOCK(cs_wallet);
        if (mapWallet.erase(hash))
        {
            CWalletDB(strWalletFile).EraseTx(hash);
        }
    }
    return true;
}


isminetype CWallet::IsMine(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
            {
                return IsMine(prev.vout[txin.prevout.n]);
            }
        }
    }
    return MINE_NO;
}

int64_t CWallet::GetDebit(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]))
                    return prev.vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    CTxDestination address;

    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a TX_PUBKEYHASH that is mine but isn't in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    if (ExtractDestination(txout.scriptPubKey, address) && ::IsMine(*this, address))
    {
        LOCK(cs_wallet);
        if (!mapAddressBook.count(address))
            return true;
    }
    return false;
}

int64_t CWalletTx::GetWTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

// FIXME: optimization - set timestamp of CWalletTx when updated with blockindex
unsigned int CWalletTx::GetTxTime() const
{
    if (HasTimestamp())
    {
        return CTransaction::GetTxTime();
    }

    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
    {
           if (fDebug)
           {
               printf("CWalletTx::GetTxTime(): block not found: %s\n",
                      hashBlock.ToString().c_str());
           }
           // not in a block yet, give an earliest possible time
           return pindexBest->nTime + 1;
    }
    CBlockIndex* pindex = (*mi).second;
    return pindex->nTime;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase() || IsCoinStake())
        {
            // Generated block
            if (hashBlock != 0)
            {
                map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && hashBlock != 0)
                {
                    map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                    if (mi != pwallet->mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

void CWalletTx::GetAmounts(int64_t& nGeneratedImmature, int64_t& nGeneratedMature, list<pair<CTxDestination, int64_t> >& listReceived,
                           list<pair<CTxDestination, int64_t> >& listSent, int64_t& nFee, string& strSentAccount) const
{
    nGeneratedImmature = nGeneratedMature = nFee = 0;
    listReceived.clear();
    listSent.clear();
    strSentAccount = strFromAccount;

    // Compute fee:
    int64_t nDebit = GetDebit();
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        int64_t nValueOut = GetValueOut();
        nFee = nDebit - nValueOut;
    }

    // Sent/received.
    BOOST_FOREACH(const CTxOut& txout, vout)
    {
        if (txout.scriptPubKey.empty())
            continue;

        bool fIsMine;
        if (nDebit > 0)
        {
            // Don't report 'change' txouts
            if (pwallet->IsChange(txout) && !IsCoinStake())
                continue;
            fIsMine = pwallet->IsMine(txout);
        }
        else if (!(fIsMine = pwallet->IsMine(txout)))
            continue;

        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address))
        {
            printf("CWalletTx::GetAmounts: INFO: no destination found, txid %s \n",
                   this->GetHash().ToString().c_str());
            address = CNoDestination();
        }

        if (nDebit > 0)
            listSent.push_back(make_pair(address, txout.nValue));

        if (fIsMine)
            listReceived.push_back(make_pair(address, txout.nValue));
    }
}

void CWalletTx::GetAccountAmounts(const string& strAccount, int64_t& nGeneratedImmature, int64_t& nGeneratedMature, int64_t& nReceived,
                                  int64_t& nSent, int64_t& nFee) const
{
    nGeneratedImmature = nGeneratedMature = nReceived = nSent = nFee = 0;

    int64_t allGeneratedImmature, allGeneratedMature, allFee;
    string strSentAccount;
    list<pair<CTxDestination, int64_t> > listReceived;
    list<pair<CTxDestination, int64_t> > listSent;
    GetAmounts(allGeneratedImmature, allGeneratedMature, listReceived, listSent, allFee, strSentAccount);

    if (strAccount == strSentAccount)
    {
        BOOST_FOREACH(const PAIRTYPE(CTxDestination,int64_t)& s, listSent)
            nSent += s.second;
        nFee = allFee;
		nGeneratedImmature = allGeneratedImmature;
		nGeneratedMature = allGeneratedMature;
    }
    {
        LOCK(pwallet->cs_wallet);
        BOOST_FOREACH(const PAIRTYPE(CTxDestination,int64_t)& r, listReceived)
        {
            if (pwallet->mapAddressBook.count(r.first))
            {
                map<CTxDestination, string>::const_iterator mi = pwallet->mapAddressBook.find(r.first);
                if (mi != pwallet->mapAddressBook.end() && (*mi).second == strAccount)
                    nReceived += r.second;
            }
            else if (strAccount.empty())
            {
                nReceived += r.second;
            }
        }
    }
}

void CWalletTx::AddSupportingTransactions(CTxDB& txdb)
{
    vtxPrev.clear();

    const int COPY_DEPTH = 3;
    if (SetMerkleBranch() < COPY_DEPTH)
    {
        vector<uint256> vWorkQueue;
        BOOST_FOREACH(const CTxIn& txin, vin)
        {
            vWorkQueue.push_back(txin.prevout.hash);
        }

        // This critsect is OK because txdb is already open
        {
            LOCK(pwallet->cs_wallet);
            map<uint256, const CMerkleTx*> mapWalletPrev;
            set<uint256> setAlreadyDone;
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hash = vWorkQueue[i];
                if (setAlreadyDone.count(hash))
                    continue;
                setAlreadyDone.insert(hash);

                CMerkleTx tx;
                map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(hash);
                if (mi != pwallet->mapWallet.end())
                {
                    tx = (*mi).second;
                    BOOST_FOREACH(const CMerkleTx& txWalletPrev, (*mi).second.vtxPrev)
                        mapWalletPrev[txWalletPrev.GetHash()] = &txWalletPrev;
                }
                else if (mapWalletPrev.count(hash))
                {
                    tx = *mapWalletPrev[hash];
                }
                else if (!fClient && txdb.ReadDiskTx(hash, tx))
                {
                    ;
                }
                else
                {
                    printf("ERROR: AddSupportingTransactions() : unsupported transaction\n");
                    continue;
                }

                int nDepth = tx.SetMerkleBranch();
                vtxPrev.push_back(tx);

                if (nDepth < COPY_DEPTH)
                {
                    BOOST_FOREACH(const CTxIn& txin, tx.vin)
                        vWorkQueue.push_back(txin.prevout.hash);
                }
            }
        }
    }

    reverse(vtxPrev.begin(), vtxPrev.end());
}

bool CWalletTx::WriteToDisk()
{
    return CWalletDB(pwallet->strWalletFile).WriteTx(GetHash(), *this);
}

// Scan the block chain (starting in pindexStart) for transactions
// from or to us. If fUpdate is true, found transactions that already
// exist in the wallet will be updated.
int CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
    int ret = 0;

    CBlockIndex* pindex = pindexStart;
    {
        LOCK(cs_wallet);
        while (pindex)
        {
            if ((!fUpdate) &&
                 nTimeFirstKey &&
                 (pindex->nTime < (nTimeFirstKey - 7200)))
            {
                pindex = pindex->pnext;
                continue;
            }
            CBlock block;
            block.ReadFromDisk(pindex, true);
            BOOST_FOREACH(CTransaction& tx, block.vtx)
            {
                if (AddToWalletIfInvolvingMe(tx, &block, fUpdate))
                {
                    ret++;
                }
            }
            pindex = pindex->pnext;
        }
    }
    return ret;
}

int CWallet::ScanForWalletTransaction(const uint256& hashTx)
{
    CTransaction tx;
    tx.ReadFromDisk(COutPoint(hashTx, 0));
    if (AddToWalletIfInvolvingMe(tx, NULL, true, true))
        return 1;
    return 0;
}

void CWallet::ReacceptWalletTransactions()
{
    CTxDB txdb("r");
    bool fRepeat = true;
    while (fRepeat)
    {
        LOCK(cs_wallet);
        fRepeat = false;
        vector<CDiskTxPos> vMissingTx;
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            CWalletTx& wtx = item.second;
            if ((wtx.IsCoinBase() && wtx.IsSpent(0)) || (wtx.IsCoinStake() && wtx.IsSpent(1)))
                continue;

            CTxIndex txindex;
            bool fUpdated = false;
            if (txdb.ReadTxIndex(wtx.GetHash(), txindex))
            {
                // Update fSpent if a tx got spent somewhere else by a copy of wallet.dat
                if (txindex.vSpent.size() != wtx.vout.size())
                {
                    printf("ERROR: ReacceptWalletTransactions() : txindex.vSpent.size() %" PRIszu " != wtx.vout.size() %" PRIszu "\n", txindex.vSpent.size(), wtx.vout.size());
                    continue;
                }
                for (unsigned int i = 0; i < txindex.vSpent.size(); i++)
                {
                    if (wtx.IsSpent(i))
                        continue;
                    if (!txindex.vSpent[i].IsNull() && IsMine(wtx.vout[i]))
                    {
                        wtx.MarkSpent(i);
                        fUpdated = true;
                        vMissingTx.push_back(txindex.vSpent[i]);
                    }
                }
                if (fUpdated)
                {
                    printf("ReacceptWalletTransactions found spent coin %s XST\n  %s\n",
                           FormatMoney(wtx.GetCredit()).c_str(),
                           wtx.GetHash().ToString().c_str());
                    wtx.MarkDirty();
                    wtx.WriteToDisk();
                }
            }
            else
            {
                // Re-accept any txes of ours that aren't already in a block
                if (!(wtx.IsCoinBase() || wtx.IsCoinStake()))
                    wtx.AcceptWalletTransaction(txdb);
            }
        }
        if (!vMissingTx.empty())
        {
            // TODO: optimize this to scan just part of the block chain?
            if (ScanForWalletTransactions(pindexGenesisBlock))
                fRepeat = true;  // Found missing transactions: re-do re-accept.
        }
    }
}

void CWalletTx::RelayWalletTransaction(CTxDB& txdb)
{
    BOOST_FOREACH(const CMerkleTx& tx, vtxPrev)
    {
        if (!(tx.IsCoinBase() || tx.IsCoinStake()))
        {
            uint256 hash = tx.GetHash();
            if (!txdb.ContainsTx(hash))
                RelayMessage(CInv(MSG_TX, hash), (CTransaction)tx);
        }
    }
    if (!(IsCoinBase() || IsCoinStake()))
    {
        uint256 hash = GetHash();
        if (!txdb.ContainsTx(hash))
        {
            printf("Relaying wtx %s\n", hash.ToString().substr(0,10).c_str());
            RelayMessage(CInv(MSG_TX, hash), (CTransaction)*this);
        }
    }
}

void CWalletTx::RelayWalletTransaction()
{
   CTxDB txdb("r");
   RelayWalletTransaction(txdb);
}

void CWallet::ResendWalletTransactions(bool fForce)
{
    if(!fForce)
    {
    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    static int64_t nNextTime;
    if (GetTime() < nNextTime)
        return;
    bool fFirst = (nNextTime == 0);
    nNextTime = GetTime() + GetRand(30 * 60);
    if (fFirst)
        return;

    // Only do it if there's been a new block since last time
    static int64_t nLastTime;
    if (nTimeBestReceived < nLastTime)
        return;
    nLastTime = GetTime();
    }
    uint32_t N = static_cast<uint32_t>(
                    pregistryMain->GetNumberQualified());
    int nFork = GetFork(pindexBest->nHeight + 1);
    int64_t nStakerPrice = GetStakerPrice(N, pindexBest->nMoneySupply, nFork);
    // Rebroadcast any of our txes that aren't in a block yet
    printf("ResendWalletTransactions()\n");
    CTxDB txdb("r");
    {
        LOCK(cs_wallet);
        // Sort them in chronological order
        multimap<unsigned int, CWalletTx*> mapSorted;
        set<uint256> setToRemove;
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            CWalletTx& wtx = item.second;
            uint256 hash = wtx.GetHash();
            // remove any loose transactions that have been invalidated
            // by changes in the blockchain state
            if (!txdb.ContainsTx(hash))
            {
                CTransaction& tx = (CTransaction&)wtx;
                Feework feework;
                if (!tx.CheckFeework(feework, false, bfrFeeworkMiner, pindexBest))
                {
                     setToRemove.insert(hash);
                     continue;
                }
                map<string, qpos_purchase> mapPurchases;
                if (!tx.CheckPurchases(pregistryMain, nStakerPrice, mapPurchases))
                {
                    setToRemove.insert(hash);
                    continue;
                }
            }
            // Don't rebroadcast until it's had plenty of time that
            // it should have gotten in already by now.
            if (fForce || nTimeBestReceived - (int64_t)wtx.nTimeReceived > 5 * 60)
            {
                mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
            }
        }
        BOOST_FOREACH(const uint256& txid, setToRemove)
        {
            EraseFromWallet(txid);
            printf("CWallet::ResendWalletTransactions(): Removing old or invalid tx\n  %s\n",
                   txid.GetHex().c_str());
        }
        BOOST_FOREACH(PAIRTYPE(const unsigned int, CWalletTx*)& item, mapSorted)
        {
            CWalletTx& wtx = *item.second;
            if (wtx.CheckTransaction())
            {
                wtx.RelayWalletTransaction(txdb);
            }
            else
            {
                printf("ResendWalletTransactions() : "
                          "CheckTransaction failed for transaction %s\n",
                       wtx.GetHash().ToString().c_str());
            }
        }
    }
}






//////////////////////////////////////////////////////////////////////////////
//
// Actions
//


int64_t CWallet::GetBalance() const
{
    int64_t nTotal = 0;
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (pcoin->IsFinal() && pcoin->IsConfirmed())
                nTotal += pcoin->GetAvailableCredit();
        }
    }

    return nTotal;
}

int64_t CWallet::GetUnconfirmedBalance() const
{
    int64_t nTotal = 0;
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (!pcoin->IsFinal() || !pcoin->IsConfirmed())
                nTotal += pcoin->GetAvailableCredit();
        }
    }
    return nTotal;
}

// it's not obvious what the original version intended as it always summed to 0
// XST has hijacked it report inaccuracies in GetBalance() when stake is immature
// In reality, it seems like it should be the "stake" (the Debit going into the mint),
// which at present is doubly subtracted in GetBalance
// The present GetStake should really be called "GetStakeReward"
int64_t CWallet::GetImmatureBalance() const
{
    int64_t nTotal = 0;
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx& pcoin = (*it).second;
            if ((pcoin.GetBlocksToMaturity() > 0) && (pcoin.IsInMainChain())) {
                int64_t nDebit = GetDebit(pcoin);
                if ((nDebit > 0) && ((pcoin.GetValueOut() - nDebit) > 0)) {
                     nTotal += nDebit;
                }
            }
        }
    }
    return nTotal;
}

// populate vCoins with vector of spendable COutputs
void CWallet::AvailableCoins(vector<COutput>& vCoins, bool fOnlyConfirmed, const CCoinControl *coinControl) const
{
    vCoins.clear();

    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;

            if (!pcoin->IsFinal())
                continue;

            if (fOnlyConfirmed && !pcoin->IsConfirmed())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            if(pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if(nDepth < 0)
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            {
                isminetype mine = IsMine(pcoin->vout[i]);
                if (!(pcoin->IsSpent(i)) && (mine != MINE_NO) && pcoin->vout[i].nValue > 0 &&
                 (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
                 {
                    vCoins.push_back(COutput(pcoin, i, nDepth, mine & MINE_SPENDABLE));
                 }
            }
        }
    }
}

static void ApproximateBestSubset(vector<pair<int64_t, pair<const CWalletTx*,unsigned int> > >vValue, int64_t nTotalLower, int64_t nTargetValue,
                                  vector<char>& vfBest, int64_t& nBest, int iterations = 1000)
{
    vector<char> vfIncluded;

    vfBest.assign(vValue.size(), true);
    nBest = nTotalLower;

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(vValue.size(), false);
        int64_t nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (unsigned int i = 0; i < vValue.size(); i++)
            {
                if (nPass == 0 ? rand() % 2 : !vfIncluded[i])
                {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}

// ppcoin: total coins staked (non-spendable until maturity)
int64_t CWallet::GetStake() const
{
    int64_t nTotal = 0;
    LOCK(cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin);
    }
    return nTotal;
}

// redundant with GetStake
int64_t CWallet::GetNewMint() const
{
    int64_t nTotal = 0;
    LOCK(cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin);
    }
    return nTotal;
}

bool CWallet::SelectCoinsMinConf(int64_t nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, vector<COutput> vCoins, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    pair<int64_t, pair<const CWalletTx*,unsigned int> > coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<int64_t>::max();
    coinLowestLarger.second.first = NULL;
    vector<pair<int64_t, pair<const CWalletTx*,unsigned int> > > vValue;
    int64_t nTotalLower = 0;

    CRandGen rg;
    std::shuffle(vCoins.begin(), vCoins.end(), rg);

    BOOST_FOREACH(COutput output, vCoins)
    {
        if (!output.fSpendable)
        {
            continue;
        }

        const CWalletTx *pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe() ? nConfMine : nConfTheirs))
        {
            continue;
        }

        int i = output.i;

        // ppcoin: timestamp must not exceed spend time
        // if a tx doesn't have timestamp, then there is no way to set
        // it in the future, so ignore this test
        if (pcoin->HasTimestamp() && pcoin->GetTxTime() > nSpendTime)
        {
            continue;
        }

        int64_t n = pcoin->vout[i].nValue;

        pair<int64_t,pair<const CWalletTx*,unsigned int> > coin = make_pair(n,make_pair(pcoin, i));

        if (n == nTargetValue)
        {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            return true;
        }
        else if (n < nTargetValue + CENT)
        {
            vValue.push_back(coin);
            nTotalLower += n;
        }
        else if (n < coinLowestLarger.first)
        {
            coinLowestLarger = coin;
        }
    }

    if (nTotalLower == nTargetValue)
    {
        for (unsigned int i = 0; i < vValue.size(); ++i)
        {
            setCoinsRet.insert(vValue[i].second);
            nValueRet += vValue[i].first;
        }
        return true;
    }

    if (nTotalLower < nTargetValue)
    {
        if (coinLowestLarger.second.first == NULL)
        {
            return false;
        }
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
        return true;
    }

    // Solve subset sum by stochastic approximation
    sort(vValue.rbegin(), vValue.rend(), CompareValueOnly());
    vector<char> vfBest;
    int64_t nBest;

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest, 1000);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + CENT)
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + CENT, vfBest, nBest, 1000);

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinLowestLarger.second.first &&
        ((nBest != nTargetValue && nBest < nTargetValue + CENT) || coinLowestLarger.first <= nBest))
    {
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
    }
    else {
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
            {
                setCoinsRet.insert(vValue[i].second);
                nValueRet += vValue[i].first;
            }

        if (fDebug && GetBoolArg("-printpriority"))
        {
            //// debug print
            printf("SelectCoins() best subset: ");
            for (unsigned int i = 0; i < vValue.size(); i++)
                if (vfBest[i])
                    printf("%s ", FormatMoney(vValue[i].first).c_str());
            printf("total %s\n", FormatMoney(nBest).c_str());
        }
    }

    return true;
}

bool CWallet::SelectCoins(int64_t nTargetValue, unsigned int nSpendTime, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet, const CCoinControl* coinControl) const
{
    vector<COutput> vCoins;
    AvailableCoins(vCoins, true, coinControl);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected())
    {
        BOOST_FOREACH(const COutput& out, vCoins)
        {
            if(!out.fSpendable)
            {
                continue;
            }
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.insert(make_pair(out.tx, out.i));
        }
        return (nValueRet >= nTargetValue);
    }

    return (SelectCoinsMinConf(nTargetValue, nSpendTime, 1, 6, vCoins, setCoinsRet, nValueRet) ||
            SelectCoinsMinConf(nTargetValue, nSpendTime, 1, 1, vCoins, setCoinsRet, nValueRet) ||
            SelectCoinsMinConf(nTargetValue, nSpendTime, 0, 1, vCoins, setCoinsRet, nValueRet));
}
void CWallet::AvailableCoinsForStaking(vector<COutput>& vCoins, unsigned int nSpendTime) const
{
    vCoins.clear();

    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;

            if (pcoin->GetBlocksToMaturity() > 0)
                continue;

            // Filtering by tx timestamp instead of block timestamp
            // may give false positives but never false negatives
            if ((pcoin->GetTxTime() + nStakeMinAge) > nSpendTime)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < 1)
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            {
                isminetype mine = IsMine(pcoin->vout[i]);
                if (!(pcoin->IsSpent(i)) && (mine != MINE_NO) && pcoin->vout[i].nValue >= 0)
                {
                    vCoins.push_back(COutput(pcoin, i, nDepth, mine & MINE_SPENDABLE));
                }
            }
        }
    }
}

bool CWallet::SelectCoinsForStaking(int64_t nTargetValue, unsigned int nSpendTime, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const
{
    vector<COutput> vCoins;
    AvailableCoinsForStaking(vCoins, nSpendTime);

    setCoinsRet.clear();
    nValueRet = 0;

    BOOST_FOREACH(COutput output, vCoins)
    {
        if (!output.fSpendable)
        {
            continue;
        }

        const CWalletTx *pcoin = output.tx;
        int i = output.i;

        // Stop if we've chosen enough inputs
        if (nValueRet >= nTargetValue)
            break;

        int64_t n = pcoin->vout[i].nValue;

        pair<int64_t,pair<const CWalletTx*,unsigned int> > coin = make_pair(n,make_pair(pcoin, i));

        if (n >= nTargetValue)
        {
            // If input value is greater or equal to target then simply insert
            //    it into the current subset and exit
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            break;
        }
        else if (n < nTargetValue + CENT)
        {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
        }
    }

    return true;
}

string CWallet::MineFeework(unsigned int nBlockSize,
                            CTransaction& txNew,
                            Feework& feework,
                            int& nRounds) const
{
    nRounds = 0;
    if (!feework.pblockhash)
    {
        string strError = _("Error: no block hash provided");
        printf("MineFeework(): %s", strError.c_str());
        return strError;
    }
    feework.limit = chainParams.TX_FEEWORK_LIMIT;
    // check the new tx for absence of existing feework
    txNew.CheckFeework(feework, false, bfrFeeworkMiner, pindexBest);
    if (!feework.HasNone())
    {
        string strError = _("Error: tx already has feework or is malformed");
        printf("MineFeework(): %s", strError.c_str());
        return strError;
    }

    // blank the sigs just to be safe
    for (unsigned int i = 0; i < txNew.vin.size(); i++)
    {
        txNew.vin[i].scriptSig = CScript();
    }

    CDataStream ssData(SER_DISK, CLIENT_VERSION);
    ssData << *(feework.pblockhash) << txNew;

    if (feework.bytes > (1000 * chainParams.FEEWORK_MAX_MULTIPLIER))
    {
        string strError = _("Error: tx size exceeds limit for feeless");
        printf("MineFeework(): %s", strError.c_str());
        return strError;
    }
    if ((feework.bytes + nBlockSize) > chainParams.FEELESS_MAX_BLOCK_SIZE)
    {
        nBlockSize = chainParams.FEELESS_MAX_BLOCK_SIZE - feework.bytes;
    }

    feework.mcost = txNew.GetFeeworkHardness(nBlockSize, GMF_BLOCK,
                                             feework.bytes);

    if (feework.mcost > chainParams.FEEWORK_MAX_MCOST)
    {
        string strError = _("Error: memory cost exceeds limit");
        printf("MineFeework(): %s", strError.c_str());
        return strError;
    }

    // by default, rng is seeded with random device at initialization
    static XORShift1024Star rng;

    feework.work = rng.Next();
    feework.GetFeeworkHash(ssData, bfrFeeworkMiner);
    nRounds = 1;
    while (feework.hash > feework.limit)
    {
        feework.work = rng.Next();
        feework.GetFeeworkHash(ssData, bfrFeeworkMiner);
        nRounds += 1;
    }

    CScript scriptFeework = feework.GetScript();
    CTxOut txoutFeework(0, scriptFeework);
    txNew.vout.push_back(txoutFeework);

    return "";
}

bool CWallet::CreateTransaction(const vector<pair<CScript, int64_t> >& vecSend,
                                CWalletTx& wtxNew,
                                CReserveKey& reservekey,
                                int64_t& nFeeRet,
                                int32_t& nChangePos,
                                const CCoinControl* coinControl,
                                Feework* pfeework)
{
    int64_t nValue = 0;
    BOOST_FOREACH (const PAIRTYPE(CScript, int64_t)& s, vecSend)
    {
        if (nValue < 0)
            return false;
        nValue += s.second;
    }
    if (vecSend.empty() || nValue < 0)
        return false;

    // user beware, client will attempt a feeless tx even if not yet activated
    bool fFeeless = (bool)pfeework;

    wtxNew.BindWallet(this);

    {
        LOCK2(cs_main, cs_wallet);
        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        {
            nFeeRet = nTransactionFee;
            if (fFeeless)
            {
                nFeeRet = 0;
            }
            while (true)
            {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                wtxNew.fFromMe = true;

                int64_t nTotalValue = nValue + nFeeRet;
                // vouts to the payees
                BOOST_FOREACH (const PAIRTYPE(CScript, int64_t)& s, vecSend)
                {
                    wtxNew.vout.push_back(CTxOut(s.second, s.first));
                }

                // Choose coins to use
                set<pair<const CWalletTx*,unsigned int> > setCoins;
                int64_t nValueIn = 0;
                int nSpendTime = wtxNew.HasTimestamp() ? wtxNew.GetTxTime() : ::GetAdjustedTime();
                if (!SelectCoins(nTotalValue, nSpendTime, setCoins, nValueIn, coinControl))
                {
                    printf("CreateTransaction(): could not select coins\n");
                    return false;
                }

                int64_t nChange = nValueIn - nValue - nFeeRet;
                // if sub-cent change is required, the fee must be raised to at least MIN_TX_FEE
                // or until nChange becomes zero
                // NOTE: this depends on the exact behaviour of GetMinFee
                if ((nFeeRet < chainParams.MIN_TX_FEE) &&
                    (!fFeeless) &&
                    (nChange > 0) &&
                    (nChange < CENT))
                {
                    int64_t nMoveToFee = min(nChange,
                                             (chainParams.MIN_TX_FEE - nFeeRet));
                    nChange -= nMoveToFee;
                    nFeeRet += nMoveToFee;
                }

                // ppcoin: sub-cent change is moved to fee
                if (nChange > 0 )
                {
                    CScript scriptChange;

                    if (coinControl && !boost::get<CNoDestination>(&coinControl->destChange))
                                            scriptChange.SetDestination(coinControl->destChange);
                    else
                    {
                        CPubKey vchPubKey = reservekey.GetReservedKey();
                        scriptChange.SetDestination(vchPubKey.GetID());
                    }
                    vector<CTxOut>::iterator position = wtxNew.vout.begin()+GetRandInt(wtxNew.vout.size() + 1);

                    if (position > wtxNew.vout.begin() && position < wtxNew.vout.end())
                    {
                         while (position > wtxNew.vout.begin())
                         {
                             if (position->nValue != 0)
                                break;
                             position--;
                          };
                     };
                    wtxNew.vout.insert(position, CTxOut(nChange, scriptChange));
                    nChangePos = std::distance(wtxNew.vout.begin(), position);
                }
                else
                {
                    reservekey.ReturnKey();
                }
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                wtxNew.vin.push_back(CTxIn(coin.first->GetHash(),coin.second));

                if (fFeeless)
                {
                    static const int DEPTH_WANTED = 4;
                    CBlockIndex* pindex = mapBlockIndex[hashBestChain];
                    int nHeight = pindex->nHeight;
                    // If possible, go several blocks deep in the edge case
                    // where the lead blocks get reorganized before this
                    // transaction is ossified in the chain.
                    int nHeightWanted = nHeight - (DEPTH_WANTED - 1);
                    int nDeepest = nHeight - chainParams.FEELESS_MAX_DEPTH;
                    unsigned int nBlocks = 0;
                    unsigned int nSizeTotal = 0;
                    CBlockIndex* pindexFeework = pindexGenesisBlock;
                    while (pindex->nHeight >= nDeepest)
                    {
                        if (nHeightWanted <= pindex->nHeight)
                        {
                            pindexFeework = pindex;
                        }
                        nBlocks += 1;
                        nSizeTotal += pindex->nBlockSize;
                        if (!pindex->pprev)
                        {
                            break;
                        }
                        pindex = pindex->pprev;
                    }
                    int nBlockSize = nSizeTotal / nBlocks;
                    pfeework->height = pindexFeework->nHeight;
                    pfeework->pblockhash = pindexFeework->phashBlock;
                    // sign to see how big the tx would be
                    int nIn = 0;
                    BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                    {
                        if (SignSignature(*this, *coin.first, wtxNew, nIn++) != 0)
                        {
                            printf("CreateTransaction(): could not sign tx\n");
                            return false;
                        }
                    }
                    pfeework->bytes = ::GetSerializeSize(*(CTransaction*)&wtxNew,
                                                         SER_NETWORK,
                                                         PROTOCOL_VERSION);
                    // feework output is 27 bytes (16 of data + op + other)
                    pfeework->bytes += 27;
                    // add 1 byte for the feework and every input signature
                    pfeework->bytes += 1 + wtxNew.vin.size();
                    int nRounds;
                    string strErr = MineFeework(nBlockSize, wtxNew,
                                                *pfeework, nRounds);
                    if (!strErr.empty())
                    {
                        printf("CreateTransaction(): %s\n", strErr.c_str());
                        return false;
                    }
                    // blank the sigs
                    for (unsigned int i = 0; i < wtxNew.vin.size(); i++)
                    {
                        wtxNew.vin[i].scriptSig = CScript();
                    }
                }

                int nIn = 0;
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                {
                    if (SignSignature(*this, *coin.first, wtxNew, nIn++) != 0)
                    {
                        printf("CreateTransaction(): could not sign tx\n");
                        return false;
                    }
                }
                unsigned int nBytes = ::GetSerializeSize(*(CTransaction*)&wtxNew,
                                                         SER_NETWORK,
                                                         PROTOCOL_VERSION);
                if (nBytes >= chainParams.MAX_BLOCK_SIZE_GEN / 5)
                {
                    printf("CreateTransaction(): too many bytes\n");
                    return false;
                }

                if (fFeeless)
                {
                    if (pfeework->bytes < nBytes)
                    {
                        printf("CreateTransaction(): "
                                  "TSNH: feework bytes (%d) too few for %d\n",
                               (int)pfeework->bytes, (int)nBytes);
                        return false;
                    }
                }
                else
                {
                    int64_t nPayFee = nTransactionFee * (1 + (int64_t)nBytes / 1000);
                    int64_t nMinFee = wtxNew.GetMinFee(1, GMF_SEND, nBytes);

                    if (nFeeRet < max(nPayFee, nMinFee))
                    {
                     nFeeRet = max(nPayFee, nMinFee);
                     continue;
                    }
                }
                // Fill vtxPrev by copying from previous transactions vtxPrev
                wtxNew.AddSupportingTransactions(txdb);
                wtxNew.fTimeReceivedIsTxTime = true;

                break;
            }
        }
    }
    return true;
}

bool CWallet::CreateTransaction(CScript scriptPubKey,
                                int64_t nValue,
                                CWalletTx& wtxNew,
                                CReserveKey& reservekey,
                                int64_t& nFeeRet,
                                const CCoinControl* coinControl,
                                Feework* pfeework)
{
    vector< pair<CScript, int64_t> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));
    int nChangePos;
    bool rv = CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet,
                                nChangePos, coinControl, pfeework);
    return rv;
}


bool CWallet::NewStealthAddress(std::string& sError, std::string& sLabel, CStealthAddress& sxAddr)
{
    ec_secret scan_secret;
    ec_secret spend_secret;

    if (GenerateRandomSecret(scan_secret) != 0
        || GenerateRandomSecret(spend_secret) != 0)
    {
        sError = "GenerateRandomSecret failed.";
        printf("Error CWallet::NewStealthAddress - %s\n", sError.c_str());
        return false;
    };

    ec_point scan_pubkey, spend_pubkey;
    if (SecretToPublicKey(scan_secret, scan_pubkey) != 0)
    {
        sError = "Could not get scan public key.";
        printf("Error CWallet::NewStealthAddress - %s\n", sError.c_str());
        return false;
    };

    if (SecretToPublicKey(spend_secret, spend_pubkey) != 0)
    {
        sError = "Could not get spend public key.";
        printf("Error CWallet::NewStealthAddress - %s\n", sError.c_str());
        return false;
    };

    if (fDebug)
    {
        printf("getnewstealthaddress: ");
        printf("scan_pubkey ");
        for (uint32_t i = 0; i < scan_pubkey.size(); ++i)
          printf("%02x", scan_pubkey[i]);
        printf("\n");

        printf("spend_pubkey ");
        for (uint32_t i = 0; i < spend_pubkey.size(); ++i)
          printf("%02x", spend_pubkey[i]);
        printf("\n");
    };


    sxAddr.label = sLabel;
    sxAddr.scan_pubkey = scan_pubkey;
    sxAddr.spend_pubkey = spend_pubkey;

    sxAddr.scan_secret.resize(32);
    memcpy(&sxAddr.scan_secret[0], &scan_secret.e[0], 32);
    sxAddr.spend_secret.resize(32);
    memcpy(&sxAddr.spend_secret[0], &spend_secret.e[0], 32);

    return true;
}
bool CWallet::AddStealthAddress(CStealthAddress& sxAddr)
{
    LOCK(cs_wallet);

    // must add before changing spend_secret
    stealthAddresses.insert(sxAddr);

    bool fOwned = sxAddr.scan_secret.size() == ec_secret_size;



    if (fOwned)
    {
        // -- owned addresses can only be added when wallet is unlocked
        if (IsLocked())
        {
            printf("Error: CWallet::AddStealthAddress wallet must be unlocked.\n");
            stealthAddresses.erase(sxAddr);
            return false;
        };

        if (IsCrypted())
        {
            std::vector<unsigned char> vchCryptedSecret;
            CSecret vchSecret;
            vchSecret.resize(32);
            memcpy(&vchSecret[0], &sxAddr.spend_secret[0], 32);

            uint256 iv = Hash(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end());
            if (!EncryptSecret(vMasterKey, vchSecret, iv, vchCryptedSecret))
            {
                printf("Error: Failed encrypting stealth key %s\n", sxAddr.Encoded().c_str());
                stealthAddresses.erase(sxAddr);
                return false;
            };
            sxAddr.spend_secret = vchCryptedSecret;
        };
    };


    bool rv = CWalletDB(strWalletFile).WriteStealthAddress(sxAddr);

    if (rv)
        NotifyAddressBookChanged(this, sxAddr, sxAddr.label, fOwned, CT_NEW);

    return rv;
}

bool CWallet::UnlockStealthAddresses(const CKeyingMaterial& vMasterKeyIn)
{
    // -- decrypt spend_secret of TimeVortex Addresses
    std::set<CStealthAddress>::iterator it;
    for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
    {
        if (it->scan_secret.size() < 32)
            continue; // TimeVortex Address is not owned

        // -- CStealthAddress are only sorted on spend_pubkey
        CStealthAddress &sxAddr = const_cast<CStealthAddress&>(*it);

        if (fDebug)
            printf("Decrypting stealth key %s\n", sxAddr.Encoded().c_str());

        CSecret vchSecret;
        uint256 iv = Hash(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end());
        if(!DecryptSecret(vMasterKeyIn, sxAddr.spend_secret, iv, vchSecret)
            || vchSecret.size() != 32)
        {
            printf("Error: Failed decrypting stealth key %s\n", sxAddr.Encoded().c_str());
            continue;
        };

        ec_secret testSecret;
        memcpy(&testSecret.e[0], &vchSecret[0], 32);
        ec_point pkSpendTest;

        if (SecretToPublicKey(testSecret, pkSpendTest) != 0
            || pkSpendTest != sxAddr.spend_pubkey)
        {
            printf("Error: Failed decrypting stealth key, public key mismatch %s\n", sxAddr.Encoded().c_str());
            continue;
        };

        sxAddr.spend_secret.resize(32);
        memcpy(&sxAddr.spend_secret[0], &vchSecret[0], 32);
    };

    CryptedKeyMap::iterator mi = mapCryptedKeys.begin();
    for (; mi != mapCryptedKeys.end(); ++mi)
    {
        CPubKey &pubKey = (*mi).second.first;
        std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
        if (vchCryptedSecret.size() != 0)
            continue;

        CKeyID ckid = pubKey.GetID();
        CBitcoinAddress addr(ckid);

        StealthKeyMetaMap::iterator mi = mapStealthKeyMeta.find(ckid);
        if (mi == mapStealthKeyMeta.end())
        {
            printf("Error: No metadata found to add secret for %s\n", addr.ToString().c_str());
            continue;
        };

        CStealthKeyMetadata& sxKeyMeta = mi->second;

        CStealthAddress sxFind;
        sxFind.scan_pubkey = sxKeyMeta.pkScan.Raw();

        std::set<CStealthAddress>::iterator si = stealthAddresses.find(sxFind);
        if (si == stealthAddresses.end())
        {
            printf("No stealth key found to add secret for %s\n", addr.ToString().c_str());
            continue;
        };

        if (fDebug)
            printf("Expanding secret for %s\n", addr.ToString().c_str());

        ec_secret sSpendR;
        ec_secret sSpend;
        ec_secret sScan;

        if (si->spend_secret.size() != ec_secret_size
            || si->scan_secret.size() != ec_secret_size)
        {
            printf("TimeVortex Stealth Address has no secret key for %s\n", addr.ToString().c_str());
            continue;
        }
        memcpy(&sScan.e[0], &si->scan_secret[0], ec_secret_size);
        memcpy(&sSpend.e[0], &si->spend_secret[0], ec_secret_size);

        ec_point pkEphem = sxKeyMeta.pkEphem.Raw();
        if (StealthSecretSpend(sScan, pkEphem, sSpend, sSpendR) != 0)
        {
            printf("StealthSecretSpend() failed.\n");
            continue;
        };

        ec_point pkTestSpendR;
        if (SecretToPublicKey(sSpendR, pkTestSpendR) != 0)
        {
            printf("SecretToPublicKey() failed.\n");
            continue;
        };

        CSecret vchSecret;
        vchSecret.resize(ec_secret_size);

        memcpy(&vchSecret[0], &sSpendR.e[0], ec_secret_size);
        CKey ckey;

        try {
            ckey.SetSecret(vchSecret, true);
        } catch (std::exception& e) {
            printf("ckey.SetSecret() threw: %s.\n", e.what());
            continue;
        };

        CPubKey cpkT = ckey.GetPubKey();

        if (!cpkT.IsValid())
        {
            printf("cpkT is invalid.\n");
            continue;
        };

        if (cpkT != pubKey)
        {
            printf("Error: Generated secret does not match.\n");
            if (fDebug)
            {
                printf("cpkT   %s\n", HexStr(cpkT.Raw()).c_str());
                printf("pubKey %s\n", HexStr(pubKey.Raw()).c_str());
            };
            continue;
        };

        if (!ckey.IsValid())
        {
            printf("Reconstructed key is invalid.\n");
            continue;
        };

        if (fDebug)
        {
            CKeyID keyID = cpkT.GetID();
            CBitcoinAddress coinAddress(keyID);
            printf("Adding secret to key %s.\n", coinAddress.ToString().c_str());
        };

        if (!AddKey(ckey))
        {
            printf("AddKey failed.\n");
            continue;
        };

        if (!CWalletDB(strWalletFile).EraseStealthKeyMeta(ckid))
            printf("EraseStealthKeyMeta failed for %s\n", addr.ToString().c_str());
    };
    return true;
}

bool CWallet::UpdateStealthAddress(std::string &addr, std::string &label, bool addIfNotExist)
{
    if (fDebug)
        printf("UpdateStealthAddress %s\n", addr.c_str());


    CStealthAddress sxAddr;

    if (!sxAddr.SetEncoded(addr))
        return false;

    std::set<CStealthAddress>::iterator it;
    it = stealthAddresses.find(sxAddr);

    ChangeType nMode = CT_UPDATED;
    CStealthAddress sxFound;
    if (it == stealthAddresses.end())
    {
        if (addIfNotExist)
        {
            sxFound = sxAddr;
            sxFound.label = label;
            stealthAddresses.insert(sxFound);
            nMode = CT_NEW;
        } else
        {
            printf("UpdateStealthAddress %s, not in set\n", addr.c_str());
            return false;
        };
    } else
    {
        sxFound = const_cast<CStealthAddress&>(*it);

        if (sxFound.label == label)
        {
            // no change
            return true;
        };

        it->label = label; // update in .stealthAddresses

        if (sxFound.scan_secret.size() == ec_secret_size)
        {
            printf("UpdateStealthAddress: todo - update owned TimeVortex Address.\n");
            return false;
        };
    };

    sxFound.label = label;

    if (!CWalletDB(strWalletFile).WriteStealthAddress(sxFound))
    {
        printf("UpdateStealthAddress(%s) Write to db failed.\n", addr.c_str());
        return false;
    };

    bool fOwned = sxFound.scan_secret.size() == ec_secret_size;
    NotifyAddressBookChanged(this, sxFound, sxFound.label, fOwned, nMode);

    return true;
}

bool CWallet::CreateStealthTransaction(CScript scriptPubKey, int64_t nValue, std::vector<uint8_t>& P, std::vector<uint8_t>& narr, std::string& sNarr, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, const CCoinControl* coinControl)
{
    vector< pair<CScript, int64_t> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));

    CScript scriptP = CScript() << OP_RETURN << P;
    if (narr.size() > 0)
        scriptP = scriptP << OP_RETURN << narr;

    vecSend.push_back(make_pair(scriptP, 0));

    // -- shuffle inputs, change output won't mix enough as it must be not fully random for plantext narrations
    CRandGen rg;
    std::shuffle(vecSend.begin(), vecSend.end(), rg);

    int nChangePos;
    bool rv = CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, nChangePos, coinControl);

    // -- the change txn is inserted in a random pos, check here to match narr to output
    if (rv && narr.size() > 0)
    {
        for (unsigned int k = 0; k < wtxNew.vout.size(); ++k)
        {
            if (wtxNew.vout[k].scriptPubKey != scriptPubKey
                || wtxNew.vout[k].nValue != nValue)
                continue;

            char key[64];
            if (snprintf(key, sizeof(key), "n_%u", k) < 1)
            {
                printf("CreateStealthTransaction(): Error creating narration key.");
                break;
            };
            wtxNew.mapValue[key] = sNarr;
            break;
        };
    };

    return rv;
}

string CWallet::SendStealthMoney(CScript scriptPubKey, int64_t nValue, std::vector<uint8_t>& P, std::vector<uint8_t>& narr, std::string& sNarr, CWalletTx& wtxNew, bool fAskFee)
{
    CReserveKey reservekey(this);
    int64_t nFeeRequired;

    if (IsLocked())
    {
        string strError = _("Error: Wallet locked, unable to create transaction  ");
        printf("SendStealthMoney() : %s", strError.c_str());
        return strError;
    }
    if (fWalletUnlockMintOnly)
    {
        string strError = _("Error: Wallet unlocked for staking only, unable to create transaction.");
        printf("SendStealthMoney() : %s", strError.c_str());
        return strError;
    }
    if (!CreateStealthTransaction(scriptPubKey, nValue, P, narr, sNarr, wtxNew, reservekey, nFeeRequired))
    {
        string strError;
        if (nValue + nFeeRequired > GetBalance())
            strError = strprintf(_("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds  "), FormatMoney(nFeeRequired).c_str());
        else
            strError = _("Error: Transaction creation failed  ");
        printf("SendStealthMoney() : %s", strError.c_str());
        return strError;
    }

    if (fAskFee && !uiInterface.ThreadSafeAskFee(nFeeRequired, _("Sending...")))
        return "ABORTED";

    if (!CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");

    return "";
}

bool CWallet::SendStealthMoneyToDestination(CStealthAddress& sxAddress, int64_t nValue, std::string& sNarr, CWalletTx& wtxNew, std::string& sError, bool fAskFee)
{
    // -- Check amount
    if (nValue <= 0)
    {
        sError = "Invalid amount";
        return false;
    };
    if (nValue + nTransactionFee > GetBalance())
    {
        sError = "Insufficient funds";
        return false;
    };


    ec_secret ephem_secret;
    ec_secret secretShared;
    ec_point pkSendTo;
    ec_point ephem_pubkey;

    if (GenerateRandomSecret(ephem_secret) != 0)
    {
        sError = "GenerateRandomSecret failed.";
        return false;
    };

    if (StealthSecret(ephem_secret, sxAddress.scan_pubkey, sxAddress.spend_pubkey, secretShared, pkSendTo) != 0)
    {
        sError = "Could not generate receiving public key.";
        return false;
    };

    CPubKey cpkTo(pkSendTo);
    if (!cpkTo.IsValid())
    {
        sError = "Invalid public key generated.";
        return false;
    };

    CKeyID ckidTo = cpkTo.GetID();

    CBitcoinAddress addrTo(ckidTo);

    if (SecretToPublicKey(ephem_secret, ephem_pubkey) != 0)
    {
        sError = "Could not generate ephem public key.";
        return false;
    };

    if (fDebug)
    {
        printf("Stealth send to generated pubkey %" PRIszu ": %s\n", pkSendTo.size(), HexStr(pkSendTo).c_str());
        printf("hash %s\n", addrTo.ToString().c_str());
        printf("ephem_pubkey %" PRIszu ": %s\n", ephem_pubkey.size(), HexStr(ephem_pubkey).c_str());
    };

    std::vector<unsigned char> vchNarr;
    if (sNarr.length() > 0)
    {
        //SecMsgCrypter crypter;
        //crypter.SetKey(&secretShared.e[0], &ephem_pubkey[0]);

        //if (!crypter.Encrypt((uint8_t*)&sNarr[0], sNarr.length(), vchNarr))
        //{
        //    sError = "Narration encryption failed.";
        //    return false;
        //};

        if (vchNarr.size() > 48)
        {
            sError = "Encrypted narration is too long.";
            return false;
        };
    };

    // -- Parse Bitcoin address
    CScript scriptPubKey;
    scriptPubKey.SetDestination(addrTo.Get());

    if ((sError = SendStealthMoney(scriptPubKey, nValue, ephem_pubkey, vchNarr, sNarr, wtxNew, fAskFee)) != "")
        return false;


    return true;
}

bool CWallet::FindStealthTransactions(const CTransaction& tx, mapValue_t& mapNarr)
{
    if (fDebug)
        printf("FindStealthTransactions() tx: %s\n", tx.GetHash().GetHex().c_str());

    mapNarr.clear();

    LOCK(cs_wallet);
    ec_secret sSpendR;
    ec_secret sSpend;
    ec_secret sScan;
    ec_secret sShared;

    ec_point pkExtracted;

    std::vector<uint8_t> vchEphemPK;
    std::vector<uint8_t> vchDataB;
    std::vector<uint8_t> vchENarr;
    opcodetype opCode;
    char cbuf[256];

    int32_t nOutputIdOuter = -1;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        nOutputIdOuter++;
        // -- for each OP_RETURN need to check all other valid outputs

        //printf("txout scriptPubKey %s\n",  txout.scriptPubKey.ToString().c_str());
        CScript::const_iterator itTxA = txout.scriptPubKey.begin();

        if (!txout.scriptPubKey.GetOp(itTxA, opCode, vchEphemPK)
            || opCode != OP_RETURN)
            continue;
        else
        if (!txout.scriptPubKey.GetOp(itTxA, opCode, vchEphemPK)
            || vchEphemPK.size() != 33)
        {
            // -- look for plaintext narrations
            if (vchEphemPK.size() > 1
                && vchEphemPK[0] == 'n'
                && vchEphemPK[1] == 'p')
            {
                if (txout.scriptPubKey.GetOp(itTxA, opCode, vchENarr)
                    && opCode == OP_RETURN
                    && txout.scriptPubKey.GetOp(itTxA, opCode, vchENarr)
                    && vchENarr.size() > 0)
                {
                    std::string sNarr = std::string(vchENarr.begin(), vchENarr.end());
                     // plaintext narration always matches preceding value output
                    snprintf(cbuf, sizeof(cbuf), "n_%d", nOutputIdOuter-1);
                    mapNarr[cbuf] = sNarr;
                } else
                {
                    printf(("Warning: FindStealthTransactions() tx: %s, "
                            "Could not extract plaintext narration.\n"),
                           tx.GetHash().GetHex().c_str());
                };
            }

            continue;
        }

        nStealth++;
        BOOST_FOREACH(const CTxOut& txoutB, tx.vout)
        {
            if (&txoutB == &txout)
                continue;

            bool txnMatch = false; // only 1 txn will match an ephem pk

            CTxDestination address;
            if (!ExtractDestination(txoutB.scriptPubKey, address))
                continue;

            if (address.type() != typeid(CKeyID))
                continue;

            CKeyID ckidMatch = boost::get<CKeyID>(address);

            if (HaveKey(ckidMatch)) // no point checking if already have key
                continue;

            std::set<CStealthAddress>::iterator it;
            for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
            {
                if (it->scan_secret.size() != ec_secret_size)
                {
                    continue;
                }

                memcpy(&sScan.e[0], &it->scan_secret[0], ec_secret_size);

                if (StealthSecret(sScan, vchEphemPK, it->spend_pubkey, sShared, pkExtracted) != 0)
                {
                    printf("StealthSecret failed.\n");
                    continue;
                };

                CPubKey cpkE(pkExtracted);

                if (!cpkE.IsValid())
                    continue;
                CKeyID ckidE = cpkE.GetID();

                if (ckidMatch != ckidE)
                    continue;

                if (fDebug)
                    printf("Found stealth txn to address %s\n", it->Encoded().c_str());

                if (IsLocked())
                {
                    if (fDebug)
                        printf("Wallet is locked, adding key without secret.\n");

                    // -- add key without secret
                    std::vector<uint8_t> vchEmpty;
                    AddCryptedKey(cpkE, vchEmpty);
                    CKeyID keyId = cpkE.GetID();
                    CBitcoinAddress coinAddress(keyId);
                    std::string sLabel = it->Encoded();
                    SetAddressBookName(keyId, sLabel);

                    CPubKey cpkEphem(vchEphemPK);
                    CPubKey cpkScan(it->scan_pubkey);
                    CStealthKeyMetadata lockedSkMeta(cpkEphem, cpkScan);

                    if (!CWalletDB(strWalletFile).WriteStealthKeyMeta(keyId, lockedSkMeta))
                        printf("WriteStealthKeyMeta failed for %s\n", coinAddress.ToString().c_str());

                    mapStealthKeyMeta[keyId] = lockedSkMeta;
                    nFoundStealth++;
                } else
                {
                    if (it->spend_secret.size() != ec_secret_size)
                        continue;
                    memcpy(&sSpend.e[0], &it->spend_secret[0], ec_secret_size);


                    if (StealthSharedToSecretSpend(sShared, sSpend, sSpendR) != 0)
                    {
                        printf("StealthSharedToSecretSpend() failed.\n");
                        continue;
                    };

                    ec_point pkTestSpendR;
                    if (SecretToPublicKey(sSpendR, pkTestSpendR) != 0)
                    {
                        printf("SecretToPublicKey() failed.\n");
                        continue;
                    };

                    CSecret vchSecret;
                    vchSecret.resize(ec_secret_size);

                    memcpy(&vchSecret[0], &sSpendR.e[0], ec_secret_size);
                    CKey ckey;

                    try {
                        ckey.SetSecret(vchSecret, true);
                    } catch (std::exception& e) {
                        printf("ckey.SetSecret() threw: %s.\n", e.what());
                        continue;
                    };

                    CPubKey cpkT = ckey.GetPubKey();
                    if (!cpkT.IsValid())
                    {
                        printf("cpkT is invalid.\n");
                        continue;
                    };

                    if (!ckey.IsValid())
                    {
                        printf("Reconstructed key is invalid.\n");
                        continue;
                    };

                    CKeyID keyID = cpkT.GetID();
                    if (fDebug)
                    {
                        CBitcoinAddress coinAddress(keyID);
                        printf("Adding key %s.\n", coinAddress.ToString().c_str());
                    };

                    if (!AddKey(ckey))
                    {
                        printf("AddKey failed.\n");
                        continue;
                    };

                    std::string sLabel = it->Encoded();
                    SetAddressBookName(keyID, sLabel);
                    nFoundStealth++;
                };

                txnMatch = true;
                break;
            };
            if (txnMatch)
                break;
        };
    };

    return true;
};
bool CWallet::GetStakeWeight(const CKeyStore& keystore, uint64_t& nMinWeight, uint64_t& nMaxWeight, uint64_t& nWeight)
{
    // Choose coins to use
    int64_t nBalance = GetBalance();

    int64_t nReserveBalance = 0;
    if (mapArgs.count("-reservebalance") && !ParseMoney(mapArgs["-reservebalance"], nReserveBalance))
        return error("GetStakeWeight : invalid reserve balance amount");
    if (nBalance <= nReserveBalance)
        return false;

    set<pair<const CWalletTx*,unsigned int> > setCoins;
    vector<const CWalletTx*> vwtxPrev;
    int64_t nValueIn = 0;

    if (!SelectCoins(nBalance - nReserveBalance, GetTime(), setCoins, nValueIn))
        return false;

    if (setCoins.empty())
        return false;

    CTxDB txdb("r");
    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
    {
        CTxIndex txindex;
        {
            LOCK2(cs_main, cs_wallet);
            if (!txdb.ReadTxIndex(pcoin.first->GetHash(), txindex))
                continue;
        }

        int64_t nTimeWeight = GetWeight((int64_t)pcoin.first->GetTxTime(), (int64_t)GetTime());
        CBigNum bnCoinDayWeight = CBigNum(pcoin.first->vout[pcoin.second].nValue) * nTimeWeight / COIN / (24 * 60 * 60);

        // Weight is greater than zero
        if (nTimeWeight > 0)
        {
            nWeight += bnCoinDayWeight.getuint64();
        }

        // Weight is greater than zero, but the maximum value isn't reached yet
        if (nTimeWeight > 0 && nTimeWeight < nStakeMaxAge)
        {
            nMinWeight += bnCoinDayWeight.getuint64();
        }

        // Maximum weight was reached
        if (nTimeWeight == nStakeMaxAge)
        {
            nMaxWeight += bnCoinDayWeight.getuint64();
        }
    }

    return true;
}

bool CWallet::CreateCoinStake(const CKeyStore& keystore,unsigned int nBits,
                              int64_t nSearchInterval, CTransaction& txNew,
                              unsigned int &nCoinStakeTime)
{
    CBlockIndex* pindexPrev = pindexBest;
    CBigNum bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);

    txNew.vin.clear();
    txNew.vout.clear();
    // Mark coin stake transaction
    CScript scriptEmpty;
    scriptEmpty.clear();
    txNew.vout.push_back(CTxOut(0, scriptEmpty));
    // Choose coins to use
    int64_t nBalance = GetBalance();
    int64_t nReserveBalance = 0;
    if (mapArgs.count("-reservebalance") && !ParseMoney(mapArgs["-reservebalance"], nReserveBalance))
        return error("CreateCoinStake : invalid reserve balance amount");
    if (nBalance <= nReserveBalance)
        return false;

    set<pair<const CWalletTx*,unsigned int> > setCoins;
    vector<const CWalletTx*> vwtxPrev;
    int64_t nValueIn = 0;
    // upon XST_FORK006, CTransactions do not have dependable timestamps,
    // however for the data structure timestamp can be used temporarily
    // for now CTransaction timestamp inits as the adjusted time
    if (!SelectCoins(nBalance - nReserveBalance,
                     nCoinStakeTime, setCoins, nValueIn))
        return false;

    if (setCoins.empty())
        return false;

    int64_t nCredit = 0;
    CScript scriptPubKeyKernel;
    CTxDB txdb("r");
    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
    {
        CTxIndex txindex;
        {
            LOCK2(cs_main, cs_wallet);
            if (!txdb.ReadTxIndex(pcoin.first->GetHash(), txindex))
                continue;
        }

        // Read block header
        CBlock block;
        {
            LOCK2(cs_main, cs_wallet);
            if (!block.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
                continue;
        }

        static int nMaxStakeSearchInterval = 60;

        // upon XST_FORK006, CTransactions do not have dependable timestamps,
        // however for the data structure timestamp can be used temporarily
        // for now CTransaction timestamp inits as the adjusted time
        if (block.GetBlockTime() + nStakeMinAge > nCoinStakeTime - nMaxStakeSearchInterval)
            continue; // only count coins meeting min age requirement

        bool fKernelFound = false;
        for (unsigned int n=0; n<min(nSearchInterval,(int64_t)nMaxStakeSearchInterval) && !fKernelFound && !fShutdown && pindexPrev == pindexBest; n++)
        {
            // printf(">> In.....\n");
            // Search backward in time from the given txNew timestamp
            // Search nSearchInterval seconds back up to nMaxStakeSearchInterval
            uint256 hashProofOfStake = 0;
            COutPoint prevoutStake = COutPoint(pcoin.first->GetHash(), pcoin.second);
            if (CheckStakeKernelHash(nBits,
                                     block,
                                     txindex.pos.nTxPos - txindex.pos.nBlockPos,
                                     *pcoin.first,
                                     prevoutStake,
                                     nCoinStakeTime - n,  // re timestamps, see above
                                     hashProofOfStake))
            {
                // Found a kernel
                if (fDebug && GetBoolArg("-printcoinstake"))
                {
                    printf("CreateCoinStake : kernel found\n");
                }
                vector<valtype> vSolutions;
                txnouttype whichType;
                CScript scriptPubKeyOut;
                scriptPubKeyKernel = pcoin.first->vout[pcoin.second].scriptPubKey;
                if (!Solver(scriptPubKeyKernel, whichType, vSolutions))
                {
                    if (fDebug && GetBoolArg("-printcoinstake"))
                    {
                        printf("CreateCoinStake : failed to parse kernel\n");
                    }
                    break;
                }
                if (fDebug && GetBoolArg("-printcoinstake"))
                {
                    printf("CreateCoinStake : parsed kernel type=%d\n", whichType);
                }
                if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH)
                {
                    if (fDebug && GetBoolArg("-printcoinstake"))
                    {
                        printf("CreateCoinStake : no support for kernel type=%d\n", whichType);
                    }
                    break;  // only support pay to public key and pay to address
                }
                CKey key;
                if (whichType == TX_PUBKEYHASH) // pay to address type
                {
                    // convert to pay to public key type
                    if (!keystore.GetKey(uint160(vSolutions[0]), key))
                    {
                        if (fDebug && GetBoolArg("-printcoinstake"))
                        {
                            printf("CreateCoinStake : failed to get key for kernel type=%d\n", whichType);
                        }
                        break;  // unable to find corresponding public key
                    }
                    scriptPubKeyOut << key.GetPubKey() << OP_CHECKSIG;
                }
                if (whichType == TX_PUBKEY)
                {
                    valtype& vchPubKey = vSolutions[0];
                    if (!keystore.GetKey(Hash160(vchPubKey), key))
                    {
                        if (fDebug && GetBoolArg("-printcoinstake"))
                        {
                            printf("CreateCoinStake : failed to get key for kernel type=%d\n", whichType);
                        }
                        break;  // unable to find corresponding public key
                    }
                    if (key.GetPubKey() != vchPubKey)
                    {
                        if (fDebug && GetBoolArg("-printcoinstake"))
                        {
                            printf("CreateCoinStake : invalid key for kernel type=%d\n", whichType);
                        }
                        break; // keys mismatch
                    }
                    scriptPubKeyOut = scriptPubKeyKernel;
                }

                nCoinStakeTime -= n;
                if (txNew.HasTimestamp())
                {
                    txNew.AdjustTime(-n);
                }
                txNew.vin.push_back(CTxIn(pcoin.first->GetHash(), pcoin.second));
                nCredit += pcoin.first->vout[pcoin.second].nValue;

                vwtxPrev.push_back(pcoin.first);
                txNew.vout.push_back(CTxOut(0, scriptPubKeyOut));

                // upon XST_FORK006, CTransactions do not have dependable timestamps,
                // however for the data structure timestamp can be used temporarily
                // for now CTransaction timestamp inits as the adjusted time
                if (GetWeight(block.GetBlockTime(), (int64_t) nCoinStakeTime) <
                    nStakeSplitAge)
                {
                    txNew.vout.push_back(
                        CTxOut(0, scriptPubKeyOut));  // split stake
                }

                if (fDebug && GetBoolArg("-printcoinstake"))
                {
                    printf("CreateCoinStake : added kernel type=%d\n",
                           whichType);
                }
                fKernelFound = true;
                break;
            }
        }
        if (fKernelFound || fShutdown)
            break; // if kernel is found stop searching
    }
    if (nCredit == 0 || nCredit > nBalance - nReserveBalance)
    {
        // printf(">> Wallet: CreateCoinStake: nCredit = %" PRI64d ", nBalance = %" PRI64d ", nReserveBalance = %" PRI64d "\n", nCredit, nBalance, nReserveBalance);
        return false;
    }

    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
    {
        // Attempt to add more inputs
        // Only add coins of the same key/address as kernel
        if (txNew.vout.size() == 2 && ((pcoin.first->vout[pcoin.second].scriptPubKey == scriptPubKeyKernel || pcoin.first->vout[pcoin.second].scriptPubKey == txNew.vout[1].scriptPubKey))
            && pcoin.first->GetHash() != txNew.vin[0].prevout.hash)
        {
            int64_t nTimeWeight = GetWeight((int64_t)pcoin.first->GetTxTime(), nCoinStakeTime);
            // Stop adding more inputs if already too many inputs
            if (txNew.vin.size() >= 100)
                break;
            // Stop adding more inputs if value is already pretty significant
            if (nCredit >= nStakeCombineThreshold)
                break;
            // Stop adding inputs if reached reserve limit
            if (nCredit + pcoin.first->vout[pcoin.second].nValue > nBalance - nReserveBalance)
                break;
            // Do not add additional significant input
            if (pcoin.first->vout[pcoin.second].nValue >= nStakeCombineThreshold)
                continue;
            // Do not add input that is still too young
            if (nTimeWeight < 3600)
                continue;
            txNew.vin.push_back(CTxIn(pcoin.first->GetHash(), pcoin.second));
            nCredit += pcoin.first->vout[pcoin.second].nValue;
            vwtxPrev.push_back(pcoin.first);
        }
    }
    // Calculate coin age reward
    {
        uint64_t nCoinAge;
        CTxDB txdb("r");

        // nCoinStakeTime will be block time after FORK006
        if (!txNew.GetCoinAge(txdb, nCoinStakeTime, nCoinAge))
            return error("CreateCoinStake : failed to calculate coin age");
        int64_t nReward = GetProofOfStakeReward(nCoinAge, nBits);
        if (nReward <= 0) {
            return false;
        }

        nCredit += nReward;
    }

    int64_t nMinFee = 0;
    while (true)
    {
        // Set output amount
        if (txNew.vout.size() == 3)
        {
            txNew.vout[1].nValue = ((nCredit - nMinFee) / 2 / CENT) * CENT;
            txNew.vout[2].nValue = nCredit - nMinFee - txNew.vout[1].nValue;
        }
        else
            txNew.vout[1].nValue = nCredit - nMinFee;

        // Sign
        int nIn = 0;
        BOOST_FOREACH(const CWalletTx* pcoin, vwtxPrev)
        {
            if (SignSignature(*this, *pcoin, txNew, nIn++) != 0)
            {
                return error("CreateCoinStake : failed to sign coinstake");
            }
        }

        // Limit size
        unsigned int nBytes = ::GetSerializeSize(txNew,
                                                 SER_NETWORK,
                                                 PROTOCOL_VERSION);
        if (nBytes >= chainParams.MAX_BLOCK_SIZE_GEN / 5)
        {
            return error("CreateCoinStake : exceeded coinstake size limit");
        }

        // Check enough fee is paid
        if (nMinFee < txNew.GetMinFee() - chainParams.MIN_TX_FEE)
        {
            nMinFee = txNew.GetMinFee() - chainParams.MIN_TX_FEE;
            continue; // try signing again
        }
        else
        {
            if (fDebug && GetBoolArg("-printfee"))
            {
                printf("CreateCoinStake : fee for coinstake %s\n",
                       FormatMoney(nMinFee).c_str());
            }
            break;
        }
    }

    // Successfully generated coinstake
    return true;
}


// Call after CreateTransaction unless you want to abort
bool CWallet::CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey)
{
    mapValue_t mapNarr;
    FindStealthTransactions(wtxNew, mapNarr);

    if (!mapNarr.empty())
    {
        BOOST_FOREACH(const PAIRTYPE(string,string)& item, mapNarr)
            wtxNew.mapValue[item.first] = item.second;
    }
    {
        LOCK2(cs_main, cs_wallet);
        printf("CommitTransaction:\n%s", wtxNew.ToString().c_str());
        {
            // This is only to keep the database open to defeat the auto-flush for the
            // duration of this scope.  This is the only place where this optimization
            // maybe makes sense; please don't do it anywhere else.
            CWalletDB* pwalletdb = fFileBacked ? new CWalletDB(strWalletFile,"r") : NULL;

            // Take key pair from key pool so it won't be used again
            reservekey.KeepKey();

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew);

            // Mark old coins as spent
            set<CWalletTx*> setCoins;
            BOOST_FOREACH(const CTxIn& txin, wtxNew.vin)
            {
                CWalletTx &coin = mapWallet[txin.prevout.hash];
                coin.BindWallet(this);
                coin.MarkSpent(txin.prevout.n);
                coin.WriteToDisk();
                NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
            }

            if (fFileBacked)
                delete pwalletdb;
        }

        // Track how many getdata requests our transaction gets
        mapRequestCount[wtxNew.GetHash()] = 0;

        // Broadcast
        if (!wtxNew.AcceptToMemoryPool())
        {
            // This must not fail. The transaction has already been signed and recorded.
            printf("CommitTransaction() : Error: Transaction not valid\n");
            return false;
        }
        wtxNew.RelayWalletTransaction();
    }
    return true;
}


string CWallet::SendMoney(CScript scriptPubKey,
                          int64_t nValue,
                          CWalletTx& wtxNew,
                          bool fAskFee,
                          Feework* pfeework)
{
    CReserveKey reservekey(this);
    int64_t nFeeRequired;

    if (pfeework)
    {
         // no reason to confirm a fee of 0
         fAskFee = false;
    }

    if (IsLocked())
    {
        string strError = _("Error: Wallet locked, unable to create transaction  ");
        printf("SendMoney() : %s", strError.c_str());
        return strError;
    }
    if (fWalletUnlockMintOnly)
    {
        string strError = _("Error: Wallet unlocked for block minting only, unable to create transaction.");
        printf("SendMoney() : %s", strError.c_str());
        return strError;
    }
    if (!CreateTransaction(scriptPubKey, nValue, wtxNew,
                           reservekey, nFeeRequired, NULL, pfeework))
    {
        string strError;
        if (nValue + nFeeRequired > GetBalance())
        {
            strError = strprintf(_(
                          "Error: This transaction requires a transaction fee "
                             "of at least %s because of its amount, "
                             "complexity, or use of recently received funds"),
                          FormatMoney(nFeeRequired).c_str());
        }
        else
        {
            strError = _("Error: Transaction creation failed");
        }
        printf("SendMoney() : %s", strError.c_str());
        return strError;
    }

    if (fAskFee && !uiInterface.ThreadSafeAskFee(nFeeRequired, _("Sending...")))
    {
        return "ABORTED";
    }

    // FIXME: this is for testing, remove if found after feeless is active
    if (pfeework && (GetFork(nBestHeight) < XST_FORKFEELESS))
    {
        return "";
    }
    if (!CommitTransaction(wtxNew, reservekey))
    {
        return _("Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
    }

    return "";
}


string CWallet::SendMoneyToDestination(const CTxDestination& address,
                                       int64_t nValue,
                                       CWalletTx& wtxNew,
                                       bool fAskFee,
                                       Feework* pfeework)
{
    // Check amount
    if (nValue <= 0)
        return _("Invalid amount");
    if (pfeework)
    {
        if (nValue > GetBalance())
            return _("Insufficient funds (feeless)");
    }
    else
    {
        if (nValue + nTransactionFee > GetBalance())
            return _("Insufficient funds with fee");
    }

    // Parse Bitcoin address
    CScript scriptPubKey;
    scriptPubKey.SetDestination(address);

    return SendMoney(scriptPubKey, nValue, wtxNew, fAskFee, pfeework);
}


string CWallet::CheckQPoSEssentials(const string &txid,
                                    unsigned int nOut,
                                    unsigned int nID,
                                    const valtype &vchPubKey)
{
    if (IsLocked())
    {
        return  _("CheckQPoSEssentials(): Wallet locked");
    }
    if (fWalletUnlockMintOnly)
    {
        return _("CheckQPoSEssentials(): Wallet unlocked for minting only.");
    }

    if (txid.size() != 64)
    {
        return _("CheckQPoSEssentials(): TxID is wrong length");
    }

    if (nID > 0)
    {
        if(!pregistryMain->IsQualifiedStaker(nID))
        {
            return _("CheckQPoSEssentials(): No such staker");
        }
    }

    // empty pubkey means it is unused
    if ((!vchPubKey.empty()) && (vchPubKey.size() != 33))
    {
        return _("CheckQPoSEssentials(): PubKey is not compressed");
    }

    return "";
}


// works roughly like to SendMoney + CreateTransaction
string CWallet::CreateQPoSTx(const string &txid,
                             unsigned int nOut,
                             const CScript &scriptQPoS,
                             int64_t nValue,
                             txnouttype typetxo,
                             CWalletTx &wtxNew)
{
    uint256 hash;
    hash.SetHex(txid);

    // return change to originator
    if (mapWallet.count(hash) == 0)
    {
        return _("CreateQPoSTx(): Wallet has no such transaction.");
    }

    const CWalletTx& wtxPrev = mapWallet[hash];

    if (nOut >= wtxPrev.vout.size())
    {
        return _("CreateQPoSTx(): Wallet has no such output.");
    }

    CTxOut prevout = wtxPrev.vout[nOut];

    wtxNew.BindWallet(this);

    bool fIsPurchase = ((typetxo == TX_PURCHASE1) || (typetxo == TX_PURCHASE4));
    bool fIsClaim = (typetxo == TX_CLAIM);

    int64_t nValueOut0 = 0;
    int64_t nPrice = 0;

    if (fIsPurchase)
    {
        nPrice = nValue;
        // pointless check, but could be useful info for the buyer
        if (prevout.nValue <= nPrice)
        {
            return _("CreateQPoSTx(): Price exceeds funds");
        }
        if (prevout.nValue < (nPrice + nTransactionFee))
        {
            return _("CreateQPoSTx(): Insufficient funds to cover price + min fees");
        }
    }
    else if (fIsClaim)
    {
        // no use to claim something where you don't get the min output value
        if ((prevout.nValue + nValue) < (nTransactionFee + CENT))
        {
            return _("CreateQPoSTx(): Insufficient claim + funds for min fees");
        }
        nValueOut0 = nValue + prevout.nValue - nTransactionFee;
    }
    else
    {
        if (prevout.nValue < nTransactionFee)
        {
            return _("CreateQPoSTx(): Insufficient funds to cover min fees");
        }
    }

    {
        LOCK2(cs_main, cs_wallet);

        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");

        wtxNew.vout.clear();
        wtxNew.vin.clear();

        CTxOut txoutQPoS(nValueOut0, scriptQPoS);
        wtxNew.vout.push_back(txoutQPoS);

        // change is already added to claim output
        if (typetxo != TX_CLAIM)
        {
            // price is 0 for non-purchases
            int64_t nChange = prevout.nValue - (nPrice + nTransactionFee);
            if (nChange >= CENT)
            {
                CTxOut txoutChange(nChange, prevout.scriptPubKey);
                wtxNew.vout.push_back(txoutChange);
            }
        }

        // works for any scriptPubKey:
        // 1. sign the tx
        // 2. see how big it is
        // 3. calculate the fee from the size
        // 4. clear the vout
        // 5. adjust the fee
        // 6. push back the change txout if needed
        // 7. sign it again with the new fee
        //      - Solver() clears the scriptSig
        //      - one and only one input
        CTxIn txin(hash, nOut);
        wtxNew.vin.push_back(txin);
        if (SignSignature(*this, wtxPrev, wtxNew, 0) != 0)
        {
            return _("CreateQPoSTx(): could not sign tx");
        }

        unsigned int nBytes = ::GetSerializeSize(*(CTransaction*)&wtxNew,
                                                 SER_NETWORK,
                                                 PROTOCOL_VERSION);
        if (nBytes >= chainParams.MAX_BLOCK_SIZE_GEN / 5)
        {
            return _("CreateQPoSTx(): too many bytes");
        }

        int64_t nPayFee = nTransactionFee * (1 + (int64_t)nBytes / 1000);
        int64_t nMinFee = wtxNew.GetMinFee(1, GMF_SEND, nBytes);
        int64_t nFee = max(nPayFee, nMinFee);

        // check fees again now that they are properly estimated
        if (fIsPurchase)
        {
            if (prevout.nValue < (nPrice + nFee))
            {
                return _("CreateQPoSTx(): Insufficient funds to cover price + min fees");
            }
        }
        else if (fIsClaim)
        {
            if ((prevout.nValue + nValue) < (nFee + CENT))
            {
                return _("CreateQPoSTx(): Insufficient claim + funds for min fees");
            }
            nValueOut0 = nValue + prevout.nValue - nFee;
            txoutQPoS.nValue = nValueOut0;
        }
        else
        {
            if (prevout.nValue < nFee)
            {
                return _("CreateQPoSTx(): Insufficient funds to cover min fees");
            }
        }

        wtxNew.vout.clear();
        wtxNew.vout.push_back(txoutQPoS);  // hasn't changed except maybe for claim

        // change is already added to claim output
        if (typetxo != TX_CLAIM)
        {
            // price is 0 for non-purchases
            int64_t nChange = prevout.nValue - (nPrice + nFee);
            if (nChange >= CENT)
            {
                CTxOut txoutChange(nChange, prevout.scriptPubKey);
                wtxNew.vout.push_back(txoutChange);
            }
        }

        wtxNew.vin.clear();
        wtxNew.vin.push_back(txin);
        if (SignSignature(*this, wtxPrev, wtxNew, 0) != 0)
        {
            return _("CreateQPoSTx(): could not sign tx");
        }

        wtxNew.AddSupportingTransactions(txdb);
        wtxNew.fTimeReceivedIsTxTime = true;

        CReserveKey reservekeyUnused(this);
        // explicitly ensure unused because change goes back to funding source
        reservekeyUnused.ReturnKey();

        std::vector<QPTxDetails> vDeets;
        wtxNew.GetQPTxDetails(0, vDeets);
        if (vDeets.empty())
        {
            return _("CreateQPoSTx(): no qPoS details gotten");
        }
        MapPrevTx mapInputs;
        CTxIndex txIndexUnused;
        mapInputs[hash] = make_pair(txIndexUnused, wtxPrev);

        map<string, qpos_purchase> mapPurchasesTx;
        map<unsigned int, vector<qpos_setkey> > mapSetKeysTx;
        map<CPubKey, vector<qpos_claim> > mapClaimsTx;
        map<unsigned int, vector<qpos_setmeta> > mapSetMetasTx;
        std::vector<QPTxDetails> vDeetsTx;

        if (!wtxNew.CheckQPoS(pregistryMain,
                              mapInputs,
                              GetAdjustedTime(),
                              vDeets,
                              pindexBest,
                              mapPurchasesTx,
                              mapSetKeysTx,
                              mapClaimsTx,
                              mapSetMetasTx,
                              vDeetsTx))
        {
            return _("CreateQPoSTx(): checking qPoS failed");
        }

        if (!CommitTransaction(wtxNew, reservekeyUnused))
        {
            return _("CreateQPoSTx(): The qPoS transaction was rejected.\n"
                     "  This might happen if some of the coins in your wallet\n"
                     "  were already spent, such as if you staked the input\n"
                     "  that you selected as a source of funds.");
        }
    }

    return "";
}


// works roughly like to SendMoneyToDestination
string CWallet::PurchaseStaker(const string &txid,
                               unsigned int nOut,
                               const string &sAlias,
                               const valtype &vchOwnerKey,
                               const valtype &vchManagerKey,
                               const valtype &vchDelegateKey,
                               const valtype &vchControllerKey,
                               int64_t nPrice, uint32_t nPcm,
                               CWalletTx &wtxNew)
{
    string sError = CheckQPoSEssentials(txid, nOut, 0, vchOwnerKey);
    if (sError != "")
    {
        return sError;
    }

    string sLC;
    if (!pregistryMain->AliasIsAvailable(sAlias, sLC))
    {
        return _("PurchaseStaker(): Alias is not available");
    }

    if (mapNftLookup.count(sLC))
    {
        unsigned int nNftID;
        if (!pregistryMain->NftIsAvailable(sLC, nNftID))
        {
            return _("PurchaseStaker(): NFT is not available");
        }
    }

    bool fIsPurchase4 = (nPcm > 0);

    if (fIsPurchase4)
    {
        if (vchManagerKey.size() != 33)
        {
            return _("PurchaseStaker(): Manager pubkey is not compressed");
        }
        if (vchDelegateKey.size() != 33)
        {
            return _("PurchaseStaker(): Delegate pubkey is not compressed");
        }
        if (vchControllerKey.size() != 33)
        {
            return _("PurchaseStaker(): Controller pubkey is not compressed");
        }
        if (nPcm > 100000)
        {
            return _("PurchaseStaker(): Payout too high");
        }
    }

    uint32_t N = static_cast<uint32_t>(
                    pregistryMain->GetNumberQualified());
    int nFork = GetFork(pindexBest->nHeight + 1);
    if (nPrice < GetStakerPrice(N, pindexBest->nMoneySupply, nFork))
    {
        return _("PurchaseStaker(): Price too low");
    }

    valtype vchPurchase(0);
    valtype vchPrice = *vchnum(static_cast<uint64_t>(nPrice)).Get();

    EXTEND(vchPurchase, vchPrice);
    EXTEND(vchPurchase, vchOwnerKey);

    if (fIsPurchase4)
    {
        EXTEND(vchPurchase, vchDelegateKey);
        // this is a testnet hack to avoid a lot of forking logic
        //    elsewhere just for testnet
        if (GetFork(nBestHeight) >= XST_FORKMISSFIX)
        {
            EXTEND(vchPurchase, vchManagerKey);
        }
        EXTEND(vchPurchase, vchControllerKey);

        valtype vchPcm = *vchnum(nPcm).Get();
        EXTEND(vchPurchase, vchPcm);
    }

    EXTEND(vchPurchase, sAlias);

    for (unsigned int i = sAlias.size(); i < 16; ++i)
    {
        vchPurchase.push_back(static_cast<unsigned char>(0));
    }

    // 1 pubkey + purchase price + alias
    unsigned int nSize = 57;  // 57 = 33 + 8 + 16
    if (fIsPurchase4)
    {
        // more testnet hack
        if (GetFork(nBestHeight) >= XST_FORKMISSFIX)
        {
            // 3 more pubkeys + payout
            nSize += 103;  // 103 = 33 + 33 + 33 + 4
        }
        else
        {
            // 2 more pubkeys + payout
            nSize += 70;    //  70 = 33 + 33 + 4
        }
    }

    if (vchPurchase.size() != nSize)
    {
        // this should never happen
        return _("PurchaseStaker(): TSNH scriptPurchase length mismatch");
    }

    txnouttype typetxo = TX_PURCHASE1;

    CScript scriptPurchase;
    scriptPurchase << vchPurchase;
    if (fIsPurchase4)
    {
        scriptPurchase << OP_PURCHASE4;
        typetxo = TX_PURCHASE4;
    }
    else
    {
        scriptPurchase << OP_PURCHASE1;
    }

    return CreateQPoSTx(txid, nOut, scriptPurchase, nPrice, typetxo, wtxNew);
}


// works roughly like to SendMoneyToDestination
string CWallet::SetStakerKey(const string &txid,
                             unsigned int nOut,
                             unsigned int nID,
                             const valtype &vchPubKey,
                             opcodetype opSetKey, uint32_t nPcm,
                             CWalletTx &wtxNew)
{
    string sError = CheckQPoSEssentials(txid, nOut, nID, vchPubKey);
    if (sError != "")
    {
        return sError;
    }

    txnouttype typetxo = TX_SETOWNER;
    bool fIsSetDelegate = false;
    if (opSetKey == OP_SETMANAGER)
    {
        typetxo = TX_SETMANAGER;
    }
    if (opSetKey == OP_SETDELEGATE)
    {
        typetxo = TX_SETDELEGATE;
        fIsSetDelegate = true;
    }
    else if (opSetKey == OP_SETCONTROLLER)
    {
        typetxo = TX_SETCONTROLLER;
    }

    if (fIsSetDelegate)
    {
        if (nPcm > 100000)
        {
            return _("SetStakerKey(): Payout too high");
        }
    }

    valtype vchSetKey(0);

    valtype vchID = *vchnum(static_cast<uint32_t>(nID)).Get();
    EXTEND(vchSetKey, vchID);

    EXTEND(vchSetKey, vchPubKey);

    if (fIsSetDelegate)
    {
        valtype vchPcm = *vchnum(nPcm).Get();
        EXTEND(vchSetKey, vchPcm);
    }

    // staker id + pubkey
    unsigned int nSize = 37;  // 37 = 4 + 33
    if (fIsSetDelegate)
    {
        // payout
        nSize += 4;
    }

    if (vchSetKey.size() != nSize)
    {
        // this should never happen
        return _("SetStakerKey(): TSNH scriptSetKey length mismatch");
    }

    CScript scriptSetKey;
    scriptSetKey << vchSetKey << opSetKey;

    return CreateQPoSTx(txid, nOut, scriptSetKey, 0, typetxo, wtxNew);
}


// works roughly like to SendMoneyToDestination
string CWallet::SetStakerState(const string &txid,
                               unsigned int nOut,
                               unsigned int nID,
                               const valtype &vchPubKey,
                               bool fEnable,
                               CWalletTx &wtxNew)
{
    string sError = CheckQPoSEssentials(txid, nOut, nID, vchPubKey);
    if (sError != "")
    {
        return sError;
    }

    if (fEnable)
    {
        if (pregistryMain->IsEnabledStaker(nID))
        {
            return _("SetStakerState(): staker already enabled");
        }
        if (!pregistryMain->IsQualifiedStaker(nID))
        {
            return _("SetStakerState(): staker is not qualified");
        }
        else if (!pregistryMain->CanEnableStaker(nID, nBestHeight))
        {
            return _("SetStakerState(): staker can't be enabled");
        }
    }

    valtype vchSetState(0);

    valtype vchID = *vchnum(static_cast<uint32_t>(nID)).Get();
    EXTEND(vchSetState, vchID);

    EXTEND(vchSetState, vchPubKey);

    // staker id
    static const unsigned int nSize = 4;

    if (vchSetState.size() != nSize)
    {
        // this should never happen
        return _("SetStakerState(): TSNH scriptSetState length mismatch");
    }

    txnouttype typetxo = fEnable ? TX_ENABLE : TX_DISABLE;
    opcodetype opSetState = fEnable ? OP_ENABLE : OP_DISABLE;

    CScript scriptSetState;
    scriptSetState << vchSetState << opSetState;

    return CreateQPoSTx(txid, nOut, scriptSetState, 0, typetxo, wtxNew);
}

// works roughly like to SendMoneyToDestination
string CWallet::ClaimQPoSBalance(const string &txid,
                                 unsigned int nOut,
                                 int64_t nValue,
                                 CWalletTx &wtxNew)
{
    valtype vchUnused(0);
    string sError = CheckQPoSEssentials(txid, nOut, 0, vchUnused);
    if (sError != "")
    {
        return sError;
    }

    uint256 hash;
    hash.SetHex(txid);

    // return change to originator
    if (mapWallet.count(hash) == 0)
    {
        return _("ClaimQPoSBalance(): Wallet has no such transaction.");
    }

    const CWalletTx& wtxPrev = mapWallet[hash];

    if (nOut >= wtxPrev.vout.size())
    {
        return _("ClaimQPoSBalance(): Wallet has no such output.");
    }

    CTxOut prevout = wtxPrev.vout[nOut];

    txnouttype typetxo;
    vector<valtype> vSolutions;
    if (!Solver(prevout.scriptPubKey, typetxo, vSolutions))
    {
        return _("ClaimQPoSBalance(): The input destination is not valid.");
    }

    CKeyID keyIDClaimant;  // becomes the recipient of the claim
    switch (typetxo)
    {
    case TX_PUBKEY:
        keyIDClaimant = CPubKey(vSolutions[0]).GetID();
        break;
    case TX_PUBKEYHASH:
    case TX_CLAIM:
        keyIDClaimant = CKeyID(uint160(vSolutions.back()));
        break;
    default:
        return _("ClaimQPoSBalance(): The input type is not valid.");
    }

    CPubKey pubKeyClaimant;
    if (!(CKeyStore *)this->GetPubKey(keyIDClaimant, pubKeyClaimant))
    {
        return _("ClaimQPoSBalance(): Wallet does not own claimant key.");
    }

    if (!pregistryMain->CanClaim(pubKeyClaimant, nValue))
    {
        return _("ClaimQPoSBalance(): Claim is not valid.");
    }

    valtype vchClaimantKey = pubKeyClaimant.Raw();

    valtype vchClaim(0);
    EXTEND(vchClaim, vchClaimantKey);

    valtype vchValue = *vchnum(static_cast<uint64_t>(nValue)).Get();
    EXTEND(vchClaim, vchValue);

    // value + pubkey
    unsigned int nSize = 41;  // 41 = 8 + 33
    if (vchClaim.size() != nSize)
    {
        // should never happen
        return _("ClaimQPoSBalance(): TSNH scriptClaim length mismatch");
    }

    CScript scriptClaim;
    scriptClaim << vchClaim << OP_CLAIM
                << OP_DUP << OP_HASH160 << keyIDClaimant
                << OP_EQUALVERIFY << OP_CHECKSIG;

    return CreateQPoSTx(txid, nOut, scriptClaim, nValue, TX_CLAIM, wtxNew);
}



// works roughly like to SendMoneyToDestination
string CWallet::SetStakerMeta(const string &txid,
                              unsigned int nOut,
                              unsigned int nID,
                              const valtype &vchPubKey,
                              const string &sKey,
                              const string &sValue,
                              CWalletTx &wtxNew)
{
    string sError = CheckQPoSEssentials(txid, nOut, nID, vchPubKey);
    if (sError != "")
    {
        return sError;
    }

    valtype vchSetMeta(0);

    valtype vchID = *vchnum(static_cast<uint32_t>(nID)).Get();
    EXTEND(vchSetMeta, vchID);

    EXTEND(vchSetMeta, vchPubKey);

    EXTEND(vchSetMeta, sKey);
    for (unsigned int i = sKey.size(); i < 16; ++i)
    {
        vchSetMeta.push_back(static_cast<unsigned char>(0));
    }

    EXTEND(vchSetMeta, sValue);
    for (unsigned int i = sValue.size(); i < 40; ++i)
    {
        vchSetMeta.push_back(static_cast<unsigned char>(0));
    }

    // staker id
    static const unsigned int nSize = 60;  // 4 + 16 + 40

    if (vchSetMeta.size() != nSize)
    {
        // should never happen
        return _("SetStakerMeta(): TSNH scriptSetMeta length mismatch");
    }

    CScript scriptSetMeta;
    scriptSetMeta << vchSetMeta << OP_SETMETA;

    return CreateQPoSTx(txid, nOut, scriptSetMeta, 0, TX_SETMETA, wtxNew);
}


DBErrors CWallet::LoadWallet(bool& fFirstRunRet)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    fFirstRunRet = false;
    DBErrors nLoadWalletRet = CWalletDB(strWalletFile,"cr+").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE)
    {
        if (CDB::Rewrite(strWalletFile, "\x04pool"))
        {
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // the requires a new key.
        }
    }

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;
    fFirstRunRet = !vchDefaultKey.IsValid();

    NewThread(ThreadFlushWalletDB, &strWalletFile);
    return DB_LOAD_OK;
}


bool CWallet::SetAddressBookName(const CTxDestination& address, const string& strName)
{
    bool fOwned;
    ChangeType nMode;
    {
        LOCK(cs_wallet);
        std::map<CTxDestination, std::string>::iterator mi = mapAddressBook.find(address);
        nMode = (mi == mapAddressBook.end()) ? CT_NEW : CT_UPDATED;
        fOwned = ::IsMine(*this, address);
       mapAddressBook[address] = strName;
    }
    NotifyAddressBookChanged(this, address, strName, fOwned, nMode);
    if (!fFileBacked)
        return false;
    return CWalletDB(strWalletFile).WriteName(CBitcoinAddress(address).ToString(), strName);
}

bool CWallet::DelAddressBookName(const CTxDestination& address)
{
    {
        LOCK(cs_wallet);
    mapAddressBook.erase(address);
    }
    bool fOwned = ::IsMine(*this, address);
    string sName = "";
    NotifyAddressBookChanged(this, address, "", fOwned, CT_DELETED);
    if (!fFileBacked)
        return false;
    return CWalletDB(strWalletFile).EraseName(CBitcoinAddress(address).ToString());
}


void CWallet::PrintWallet(const CBlock& block)
{
    {
        LOCK(cs_wallet);
        if (block.IsProofOfWork() && mapWallet.count(block.vtx[0].GetHash()))
        {
            CWalletTx& wtx = mapWallet[block.vtx[0].GetHash()];
            printf("    mine:  %d  %d  %" PRId64 "",
                   wtx.GetDepthInMainChain(), wtx.GetBlocksToMaturity(), wtx.GetCredit());
        }
        if (block.IsProofOfStake() && mapWallet.count(block.vtx[1].GetHash()))
        {
            CWalletTx& wtx = mapWallet[block.vtx[1].GetHash()];
            printf("    stake: %d  %d  %" PRId64 "",
                   wtx.GetDepthInMainChain(), wtx.GetBlocksToMaturity(), wtx.GetCredit());
         }

    }
    printf("\n");
}

bool CWallet::GetTransaction(const uint256 &hashTx, CWalletTx& wtx)
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
        {
            wtx = (*mi).second;
            return true;
        }
    }
    return false;
}

bool CWallet::SetDefaultKey(const CPubKey &vchPubKey)
{
    if (fFileBacked)
    {
        if (!CWalletDB(strWalletFile).WriteDefaultKey(vchPubKey))
            return false;
    }
    vchDefaultKey = vchPubKey;
    return true;
}

bool GetWalletFile(CWallet* pwallet, string &strWalletFileOut)
{
    if (!pwallet->fFileBacked)
        return false;
    strWalletFileOut = pwallet->strWalletFile;
    return true;
}

//
// Mark old keypool keys as used,
// and generate all new keys
//
bool CWallet::NewKeyPool()
{
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(strWalletFile);
        BOOST_FOREACH(int64_t nIndex, setKeyPool)
            walletdb.ErasePool(nIndex);
        setKeyPool.clear();

        if (IsLocked())
            return false;

        int64_t nKeys = max(GetArg("-keypool", chainParams.DEFAULT_KEYPOOL),
                            (int64_t)0);
        for (int i = 0; i < nKeys; i++)
        {
            int64_t nIndex = i+1;
            walletdb.WritePool(nIndex, CKeyPool(GenerateNewKey()));
            setKeyPool.insert(nIndex);
        }
        printf("CWallet::NewKeyPool wrote %" PRId64 " new keys\n", nKeys);
    }
    return true;
}

bool CWallet::TopUpKeyPool(unsigned int nSize)
{
    {
        LOCK(cs_wallet);

        if (IsLocked())
        {
            return false;
        }

        CWalletDB walletdb(strWalletFile);

        // Top up key pool
        unsigned int nTargetSize;
        if (nSize > 0)
        {
            nTargetSize = nSize;
        }
        else
        {
            nTargetSize = max(GetArg("-keypool", chainParams.DEFAULT_KEYPOOL),
                              (int64_t)0);
        }

        while (setKeyPool.size() < (nTargetSize + 1))
        {
            int64_t nEnd = 1;
            if (!setKeyPool.empty())
            {
                nEnd = *(--setKeyPool.end()) + 1;
            }
            if (!walletdb.WritePool(nEnd, CKeyPool(GenerateNewKey())))
            {
                throw runtime_error("TopUpKeyPool() : writing generated key failed");
            }
            setKeyPool.insert(nEnd);
            printf("keypool added key %" PRId64 ", size=%" PRIszu "\n", nEnd, setKeyPool.size());
        }
    }
    return true;
}

void CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool)
{
    nIndex = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool();

        // Get the oldest key
        if(setKeyPool.empty())
            return;

        CWalletDB walletdb(strWalletFile);

        nIndex = *(setKeyPool.begin());
        setKeyPool.erase(setKeyPool.begin());
        if (!walletdb.ReadPool(nIndex, keypool))
            throw runtime_error("ReserveKeyFromKeyPool() : read failed");
        if (!HaveKey(keypool.vchPubKey.GetID()))
            throw runtime_error("ReserveKeyFromKeyPool() : unknown key in key pool");
        assert(keypool.vchPubKey.IsValid());
        if (fDebug && GetBoolArg("-printkeypool"))
            printf("keypool reserve %" PRId64 "\n", nIndex);
    }
}

int64_t CWallet::AddReserveKey(const CKeyPool& keypool)
{
    {
        LOCK2(cs_main, cs_wallet);
        CWalletDB walletdb(strWalletFile);

        int64_t nIndex = 1 + *(--setKeyPool.end());
        if (!walletdb.WritePool(nIndex, keypool))
            throw runtime_error("AddReserveKey() : writing added key failed");
        setKeyPool.insert(nIndex);
        return nIndex;
    }
    return -1;
}

void CWallet::KeepKey(int64_t nIndex)
{
    // Remove from key pool
    if (fFileBacked)
    {
        CWalletDB walletdb(strWalletFile);
        walletdb.ErasePool(nIndex);
    }
    if(fDebug)
        printf("keypool keep %" PRId64 "\n", nIndex);
}

void CWallet::ReturnKey(int64_t nIndex)
{
    // Return to key pool
    {
        LOCK(cs_wallet);
        setKeyPool.insert(nIndex);
    }
    //if(fDebug)
        //printf("keypool return %" PRI64d "\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result, bool fAllowReuse)
{
    int64_t nIndex = 0;
    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex == -1)
        {
            if (fAllowReuse && vchDefaultKey.IsValid())
            {
                result = vchDefaultKey;
                return true;
            }
            if (IsLocked()) return false;
            result = GenerateNewKey();
            return true;
        }
        KeepKey(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

int64_t CWallet::GetOldestKeyPoolTime()
{
    int64_t nIndex = 0;
    CKeyPool keypool;
    ReserveKeyFromKeyPool(nIndex, keypool);
    if (nIndex == -1)
        return GetTime();
    ReturnKey(nIndex);
    return keypool.nTime;
}

std::map<CTxDestination, int64_t> CWallet::GetAddressBalances()
{
    map<CTxDestination, int64_t> balances;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH(PAIRTYPE(uint256, CWalletTx) walletEntry, mapWallet)
        {
            CWalletTx *pcoin = &walletEntry.second;

            if (!pcoin->IsFinal() || !pcoin->IsConfirmed())
                continue;

            if ((pcoin->IsCoinBase() || pcoin->IsCoinStake()) && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < (pcoin->IsFromMe() ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            {
                CTxDestination addr;
                if (!IsMine(pcoin->vout[i]))
                    continue;
                if(!ExtractDestination(pcoin->vout[i].scriptPubKey, addr))
                    continue;

                int64_t n = pcoin->IsSpent(i) ? 0 : pcoin->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

set< set<CTxDestination> > CWallet::GetAddressGroupings()
{
    set< set<CTxDestination> > groupings;
    set<CTxDestination> grouping;

    BOOST_FOREACH(PAIRTYPE(uint256, CWalletTx) walletEntry, mapWallet)
    {
        CWalletTx *pcoin = &walletEntry.second;

        if (pcoin->vin.size() > 0 && IsMine(pcoin->vin[0]))
        {
            // group all input addresses with each other
            BOOST_FOREACH(CTxIn txin, pcoin->vin)
            {
                CTxDestination address;
                if(!ExtractDestination(mapWallet[txin.prevout.hash].vout[txin.prevout.n].scriptPubKey, address))
                    continue;
                grouping.insert(address);
            }

            // group change with input addresses
            BOOST_FOREACH(CTxOut txout, pcoin->vout)
                if (IsChange(txout))
                {
                    CWalletTx tx = mapWallet[pcoin->vin[0].prevout.hash];
                    CTxDestination txoutAddr;
                    if(!ExtractDestination(txout.scriptPubKey, txoutAddr))
                        continue;
                    grouping.insert(txoutAddr);
                }
            groupings.insert(grouping);
            grouping.clear();
        }

        // group lone addrs by themselves
        for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            if (IsMine(pcoin->vout[i]))
            {
                CTxDestination address;
                if(!ExtractDestination(pcoin->vout[i].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    set< set<CTxDestination>* > uniqueGroupings; // a set of pointers to groups of addresses
    map< CTxDestination, set<CTxDestination>* > setmap;  // map addresses to the unique group containing it
    BOOST_FOREACH(set<CTxDestination> grouping, groupings)
    {
        // make a set of all the groups hit by this new group
        set< set<CTxDestination>* > hits;
        map< CTxDestination, set<CTxDestination>* >::iterator it;
        BOOST_FOREACH(CTxDestination address, grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        set<CTxDestination>* merged = new set<CTxDestination>(grouping);
        BOOST_FOREACH(set<CTxDestination>* hit, hits)
        {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        BOOST_FOREACH(CTxDestination element, *merged)
            setmap[element] = merged;
    }

    set< set<CTxDestination> > ret;
    BOOST_FOREACH(set<CTxDestination>* uniqueGrouping, uniqueGroupings)
    {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}

// ppcoin: check 'spent' consistency between wallet and txindex
// ppcoin: fix wallet spent state according to txindex
void CWallet::FixSpentCoins(int& nMismatchFound, int64_t& nBalanceInQuestion, bool fCheckOnly)
{
    nMismatchFound = 0;
    nBalanceInQuestion = 0;

    LOCK(cs_wallet);
    vector<CWalletTx*> vCoins;
    vCoins.reserve(mapWallet.size());
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        vCoins.push_back(&(*it).second);

    CTxDB txdb("r");
    BOOST_FOREACH(CWalletTx* pcoin, vCoins)
    {
        // Find the corresponding transaction index
        CTxIndex txindex;
        if (!txdb.ReadTxIndex(pcoin->GetHash(), txindex))
            continue;
        for (unsigned int n=0; n < pcoin->vout.size(); n++)
        {
            if (IsMine(pcoin->vout[n]) && pcoin->IsSpent(n) && (txindex.vSpent.size() <= n || txindex.vSpent[n].IsNull()))
            {
                printf("FixSpentCoins found lost coin %s XST %s[%d], %s\n",
                       FormatMoney(pcoin->vout[n].nValue).c_str(),
                       pcoin->GetHash().ToString().c_str(),
                       n,
                       fCheckOnly? "repair not attempted" : "repairing");
                nMismatchFound++;
                nBalanceInQuestion += pcoin->vout[n].nValue;
                if (!fCheckOnly)
                {
                    pcoin->MarkUnspent(n);
                    pcoin->WriteToDisk();
                }
            }
            else if (IsMine(pcoin->vout[n]) && !pcoin->IsSpent(n) && (txindex.vSpent.size() > n && !txindex.vSpent[n].IsNull()))
            {
                printf("FixSpentCoins found spent coin %s XST %s[%d], %s\n",
                       FormatMoney(pcoin->vout[n].nValue).c_str(),
                       pcoin->GetHash().ToString().c_str(),
                       n,
                       fCheckOnly? "repair not attempted" : "repairing");
                nMismatchFound++;
                nBalanceInQuestion += pcoin->vout[n].nValue;
                if (!fCheckOnly)
                {
                    pcoin->MarkSpent(n);
                    pcoin->WriteToDisk();
                }
            }
        }
    }
}

// ppcoin: disable transaction (only for coinstake)
void CWallet::DisableTransaction(const CTransaction &tx)
{
    if (!tx.IsCoinStake() || !IsFromMe(tx))
        return; // only disconnecting coinstake requires marking input unspent

    LOCK(cs_wallet);
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size() && IsMine(prev.vout[txin.prevout.n]))
            {
                prev.MarkUnspent(txin.prevout.n);
                prev.WriteToDisk();
            }
        }
    }
}

CPubKey CReserveKey::GetReservedKey()
{
    if (nIndex == -1)
    {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else {
            if (pwallet->vchDefaultKey.IsValid()) {
                printf("CReserveKey::GetReservedKey(): Warning: Using default key instead of a new key, top up your keypool!");
                vchPubKey = pwallet->vchDefaultKey;
            }

        }
    }
    assert(vchPubKey.IsValid());
    return vchPubKey;

}


void CReserveKey::KeepKey()
{
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey()
{
    if (nIndex != -1)
        pwallet->ReturnKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CWallet::GetAllReserveKeys(set<CKeyID>& setAddress) const
{
    setAddress.clear();

    CWalletDB walletdb(strWalletFile);

    LOCK2(cs_main, cs_wallet);
    BOOST_FOREACH(const int64_t& id, setKeyPool)
    {
        CKeyPool keypool;
        if (!walletdb.ReadPool(id, keypool))
            throw runtime_error("GetAllReserveKeyHashes() : read failed");
        assert(keypool.vchPubKey.IsValid());
        CKeyID keyID = keypool.vchPubKey.GetID();
        if (!HaveKey(keyID))
            throw runtime_error("GetAllReserveKeyHashes() : unknown key in key pool");
        setAddress.insert(keyID);
    }
}

void CWallet::UpdatedTransaction(const uint256 &hashTx)
{
    {
        LOCK(cs_wallet);
        // Only notify UI if this transaction is in this wallet
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
            NotifyTransactionChanged(this, hashTx, CT_UPDATED);
    }
}
void CWallet::GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const {
    mapKeyBirth.clear();

    // get birth times for keys with metadata
    for (std::map<CKeyID, CKeyMetadata>::const_iterator it = mapKeyMetadata.begin(); it != mapKeyMetadata.end(); it++)
        if (it->second.nCreateTime)
            mapKeyBirth[it->first] = it->second.nCreateTime;

    // map in which we'll infer heights of other keys
    CBlockIndex *pindexMax = FindBlockByHeight(std::max(0, nBestHeight - 144)); // the tip can be reorganised; use a 144-block safety margin
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    std::set<CKeyID> setKeys;
    GetKeys(setKeys);
    BOOST_FOREACH(const CKeyID &keyid, setKeys) {
        if (mapKeyBirth.count(keyid) == 0)
            mapKeyFirstBlock[keyid] = pindexMax;
    }
    setKeys.clear();

    // if there are no such keys, we're done
    if (mapKeyFirstBlock.empty())
        return;

    // find first block that affects those keys, if there are any left
    std::vector<CKeyID> vAffected;
    for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); it++) {
        // iterate over all wallet transactions...
        const CWalletTx &wtx = (*it).second;
        std::map<uint256, CBlockIndex*>::const_iterator blit = mapBlockIndex.find(wtx.hashBlock);
        if (blit != mapBlockIndex.end() && blit->second->IsInMainChain()) {
            // ... which are already in a block
            int nHeight = blit->second->nHeight;
            BOOST_FOREACH(const CTxOut &txout, wtx.vout) {
                // iterate over all their outputs
                ::ExtractAffectedKeys(*this, txout.scriptPubKey, vAffected);
                BOOST_FOREACH(const CKeyID &keyid, vAffected) {
                    // ... and all their affected keys
                    std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                    if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                        rit->second = blit->second;
                }
                vAffected.clear();
            }
        }
    }

    // Extract block timestamps for those keys
    for (std::map<CKeyID, CBlockIndex*>::const_iterator it = mapKeyFirstBlock.begin(); it != mapKeyFirstBlock.end(); it++)
        mapKeyBirth[it->first] = it->second->nTime - 7200; // block times can be 2h off
}
