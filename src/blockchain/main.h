// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_MAIN_H
#define BITCOIN_MAIN_H

#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "script.h"
#include "toradapter.h"

#include "hashblock/hashblock.h"

#include "QPRegistry.hpp"

#include "feeless.hpp"

#include <list>

class CWallet;
class CBlock;
class CBlockIndex;
class CKeyItem;
class CReserveKey;
class COutPoint;

class CAddress;
class CInv;
class CRequestTracker;
class CNode;

inline bool MoneyRange(int64_t nValue)
{
    return ((nValue >= 0) &&
            (nValue <= chainParams.MAX_MONEY));
}

#ifdef USE_UPNP
static const int fHaveUPnP = true;
#else
static const int fHaveUPnP = false;
#endif

// blackcoin pos 2.0 drift recommendations
// back to time of previous block in the past
// inline int64_t PastDrift(CBlockIndex *pprev) {return pprev->nTime; }
// up to FUTURE_DRIFT_* sec in the future
inline int64_t FutureDrift(int64_t nTime) {
                  return fTestNet ? nTime + chainParams.FUTURE_DRIFT_MAINNET  :
                                    nTime + chainParams.FUTURE_DRIFT_TESTNET; }

inline int GetPoWCutoff()
{
    // too many problems with PoW+PoS and big hashes, was block 20421
    return fTestNet ? chainParams.CUTOFF_POW_T :
                      chainParams.CUTOFF_POW_M;
}

inline int GetPurchaseStart()
{
    // allow staker purchases
    return fTestNet ? chainParams.START_PURCHASE_T :
                      chainParams.START_PURCHASE_M;
}

inline int GetQPoSStart()
{
    // switch to qPoS
    return fTestNet ? chainParams.START_QPOS_T :
                      chainParams.START_QPOS_M;
}



extern CScript COINBASE_FLAGS;

extern CCriticalSection cs_main;

extern std::map<uint256, CBlockIndex*> mapBlockIndex;
extern std::map<int, CBlockIndex*> mapBlockLookup;

extern std::set<std::pair<COutPoint, unsigned int> > setStakeSeen;
extern uint256 hashGenesisBlock;
extern CBlockIndex* pindexGenesisBlock;
extern unsigned int nStakeMinAge;
extern int nCoinbaseMaturity;
extern int nBestHeight;
extern CBigNum bnBestChainTrust;
extern CBigNum bnBestInvalidTrust;
extern uint256 hashBestChain;
extern CBlockIndex* pindexBest;
extern unsigned int nTransactionsUpdated;
extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockSize;
extern int64_t nLastCoinStakeSearchInterval;
extern double dHashesPerSec;
extern int64_t nHPSTimerStart;
extern int64_t nTimeBestReceived;
extern CCriticalSection cs_setpwalletRegistered;
extern std::set<CWallet*> setpwalletRegistered;

extern std::map<uint256, CBlock*> mapOrphanBlocks;

// Settings
extern int64_t nTransactionFee;
extern unsigned int nDerivationMethodIndex;
extern int64_t nReserveBalance;

enum BlockCreationResult
{
    BLOCKCREATION_OK,
    BLOCKCREATION_QPOS_IN_REPLAY,
    BLOCKCREATION_NOT_CURRENTSTAKER,
    BLOCKCREATION_QPOS_BLOCK_EXISTS,
    BLOCKCREATION_INSTANTIATION_FAIL,
    BLOCKCREATION_REGISTRY_FAIL,
    BLOCKCREATION_PURCHASE_FAIL,
    BLOCKCREATION_CLAIM_FAIL,
    BLOCKCREATION_PROOFTYPE_FAIL
};

class CReserveKey;
class CTxDB;
class CTxIndex;


const char* DescribeBlockCreationResult(BlockCreationResult r);

int GetTargetSpacing(const int nHeight);

void RegisterWallet(CWallet* pwalletIn);
void UnregisterWallet(CWallet* pwalletIn);
void SyncWithWallets(const CTransaction& tx, const CBlock* pblock = NULL, bool fUpdate = false, bool fConnect = true);
bool ProcessBlock(CNode* pfrom, CBlock* pblock,
                  bool fIsBootstrap=false, bool fJustCheck=false, bool fIsMine=false);
bool CheckDiskSpace(uint64_t nAdditionalBytes=0);
FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode="rb");
FILE* AppendBlockFile(unsigned int& nFileRet);
bool LoadBlockIndex(bool fAllowNew=true);
void PrintBlockTree();
CBlockIndex* FindBlockByHeight(int nHeight);
bool ProcessMessages(CNode* pfrom);
bool SendMessages(CNode* pto, bool fSendTrickle);
bool LoadExternalBlockFile(FILE* fileIn);
BlockCreationResult CreateNewBlock(CWallet* pwallet,
                              ProofTypes fTypeOfProof,
                              AUTO_PTR<CBlock> &pblockRet);
// CBlock* CreateNewBlock(CWallet* pwallet, bool fProofOfStake=false);
#ifdef WITH_MINER
void GenerateXST(bool fGenerate, CWallet* pwallet);
#endif  /* WITH_MINER */
void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce);
void FormatHashBuffers(CBlock* pblock, char* pmidstate, char* pdata, char* phash1);
bool CheckWork(CBlock* pblock, CWallet& wallet,
               CReserveKey& reservekey, const CBlockIndex *pindexPrev);
bool CheckProofOfWork(uint256 hash, unsigned int nBits);
int64_t GetProofOfWorkReward(int nHeight, int64_t nFees);
int64_t GetProofOfStakeReward(int64_t nCoinAge, unsigned int nBits);
int64_t GetQPoSReward(const CBlockIndex *pindexPrev);
int64_t GetStakerPrice(uint32_t N,
                       int64_t nSupply,
                       bool fPurchase=false);
unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime);
unsigned int ComputeMinStake(unsigned int nBase, int64_t nTime, unsigned int nBlockTime);
int GetNumBlocksOfPeers();
bool IsInitialBlockDownload();
std::string GetWarnings(std::string strFor);
bool GetTransaction(const uint256 &hash, CTransaction &tx,
                    uint256 &hashBlock, unsigned int &nTimeBlock);
uint256 WantedByOrphan(const CBlock* pblockOrphan);
const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake);
void StealthMinter(CWallet *pwallet, ProofTypes fTypeOfProof);
void ResendWalletTransactions();

void GetRegistrySnapshot(CTxDB &txdb,
                         int nReplay,
                         QPRegistry *pregistryTemp);

bool RewindRegistry(CTxDB &txdb,
                    CBlockIndex *pindexRewind,
                    QPRegistry *pregistry,
                    CBlockIndex* &pindexCurrentRet);

/** Position on disk for a particular transaction. */
class CDiskTxPos
{
public:
    unsigned int nFile;
    unsigned int nBlockPos;
    unsigned int nTxPos;

    CDiskTxPos()
    {
        SetNull();
    }

    CDiskTxPos(unsigned int nFileIn, unsigned int nBlockPosIn, unsigned int nTxPosIn)
    {
        nFile = nFileIn;
        nBlockPos = nBlockPosIn;
        nTxPos = nTxPosIn;
    }

    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    void SetNull() { nFile = (unsigned int) -1; nBlockPos = 0; nTxPos = 0; }
    bool IsNull() const { return (nFile == (unsigned int) -1); }

    friend bool operator==(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return (a.nFile     == b.nFile &&
                a.nBlockPos == b.nBlockPos &&
                a.nTxPos    == b.nTxPos);
    }

    friend bool operator!=(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return !(a == b);
    }


    std::string ToString() const
    {
        if (IsNull())
            return "null";
        else
            return strprintf("(nFile=%u, nBlockPos=%u, nTxPos=%u)", nFile, nBlockPos, nTxPos);
    }

    void print() const
    {
        printf("%s", ToString().c_str());
    }
};



/** An inpoint - a combination of a transaction and an index n into its vin */
class CInPoint
{
public:
    CTransaction* ptx;
    unsigned int n;

    CInPoint() { SetNull(); }
    CInPoint(CTransaction* ptxIn, unsigned int nIn) { ptx = ptxIn; n = nIn; }
    void SetNull() { ptx = NULL; n = (unsigned int) -1; }
    bool IsNull() const { return (ptx == NULL && n == (unsigned int) -1); }
};



/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256 hash;
    unsigned int n;

    COutPoint() { SetNull(); }
    COutPoint(uint256 hashIn, unsigned int nIn) { hash = hashIn; n = nIn; }
    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    void SetNull() { hash = 0; n = (unsigned int) -1; }
    bool IsNull() const { return (hash == 0 && n == (unsigned int) -1); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash < b.hash || (a.hash == b.hash && a.n < b.n));
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const
    {
        return strprintf("COutPoint(%s, %u)", hash.ToString().c_str(), n);
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};




/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    unsigned int nSequence;

    CTxIn()
    {
        nSequence = std::numeric_limits<unsigned int>::max();
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=std::numeric_limits<unsigned int>::max())
    {
        prevout = prevoutIn;
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    CTxIn(uint256 hashPrevTx, unsigned int nOut, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=std::numeric_limits<unsigned int>::max())
    {
        prevout = COutPoint(hashPrevTx, nOut);
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(prevout);
        READWRITE(scriptSig);
        READWRITE(nSequence);
    )

    bool IsFinal() const
    {
        return (nSequence == std::numeric_limits<unsigned int>::max());
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToStringShort() const
    {
        return strprintf(" %s %d", prevout.hash.ToString().c_str(), prevout.n);
    }

    std::string ToString() const
    {
        std::string str;
        str += "CTxIn(";
        str += prevout.ToString();
        if (prevout.IsNull())
            str += strprintf(", coinbase %s", HexStr(scriptSig).c_str());
        else
            str += strprintf(", scriptSig=%s", scriptSig.ToString().c_str());
        if (nSequence != std::numeric_limits<unsigned int>::max())
            str += strprintf(", nSequence=%u", nSequence);
        str += ")";
        return str;
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }


};




/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    int64_t nValue;
    CScript scriptPubKey;

    CTxOut()
    {
        SetNull();
    }

    CTxOut(int64_t nValueIn, CScript scriptPubKeyIn)
    {
        nValue = nValueIn;
        scriptPubKey = scriptPubKeyIn;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nValue);
        READWRITE(scriptPubKey);
    )

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
    }

    bool IsNull()
    {
        return (nValue == -1);
    }

    void SetEmpty()
    {
        nValue = 0;
        scriptPubKey.clear();
    }

    bool IsEmpty() const
    {
        return (nValue == 0 && scriptPubKey.empty());
    }

    bool IsOpReturn() const {
       std::vector<uint8_t> vchR;
       opcodetype opCode;
       CScript scriptPK = this->scriptPubKey;
       CScript::const_iterator pc = scriptPK.begin();
       if (!vchR.empty()) {
             vchR.clear();
       }
       if (scriptPK.GetOp(pc, opCode, vchR) && (opCode == OP_RETURN)) {
              return true;
       }
       return false;
    }

    uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToStringShort() const
    {
        return strprintf(" out %s %s", FormatMoney(nValue).c_str(), scriptPubKey.ToString(true).c_str());
    }

    std::string ToString() const
    {
        if (IsEmpty()) return "CTxOut(empty)";
        if (scriptPubKey.size() < 6)
            return "CTxOut(error)";
        return strprintf("CTxOut(nValue=%s, scriptPubKey=%s)", FormatMoney(nValue).c_str(), scriptPubKey.ToString().c_str());
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};




enum GetMinFee_mode
{
    GMF_BLOCK,
    GMF_RELAY,
    GMF_SEND,
};

typedef std::map<uint256, std::pair<CTxIndex, CTransaction> > MapPrevTx;

/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
private:
    unsigned int _nTime;
public:
    static const int GENESIS_VERSION=1;
    static const int NOTXTIME_VERSION=2;
    static const int IMMALLEABLE_VERSION=3;
    static const int FEELESS_VERSION=4;
    static const int CURRENT_VERSION=FEELESS_VERSION;

    int nVersion;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    unsigned int nLockTime;

    // Denial-of-service detection:
    mutable int nDoS;
    bool DoS(int nDoSIn, bool fIn) const { nDoS += nDoSIn; return fIn; }

    CTransaction()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (

        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        if (this->nVersion < CTransaction::NOTXTIME_VERSION)
        {
            READWRITE(_nTime);
        }
        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);
    )

    void SetNull()
    {
        int nFork = GetFork(nBestHeight);
        if (nFork >= XST_FORKFEELESS)
        {
            nVersion = CTransaction::FEELESS_VERSION;
        }
        else if (nFork >= XST_FORKPURCHASE)
        {
            nVersion = CTransaction::IMMALLEABLE_VERSION;
        }
        else if (nFork >= XST_FORK006)
        {
             nVersion = CTransaction::NOTXTIME_VERSION;
        }
        else
        {
             nVersion = CTransaction::GENESIS_VERSION;
        }
        _nTime = GetAdjustedTime();
        vin.clear();
        vout.clear();
        nLockTime = 0;
        nDoS = 0;  // Denial-of-service prevention
    }

    bool IsNull() const
    {
        return (vin.empty() && vout.empty());
    }

    // upon XST_FORK006 txid hash does not include signatures
    // this allows the tx hash to be signed,
    // making the txid immutable without invalidating sigs
    uint256 GetHash() const
    {
        // mining software expects filled coinbase script sigs
        if (HasTimestamp() || IsCoinBase())
        {
            return SerializeHash(*this);
        }
        else
        {
            CTransaction txTmp(*this);
            // Blank the sigs
            for (unsigned int i = 0; i < txTmp.vin.size(); i++)
            {
                   txTmp.vin[i].scriptSig = CScript();
            }
            return SerializeHash(txTmp);
        }
    }

    bool IsFinal(int nBlockHeight=0, int64_t nBlockTime=0) const
    {
        // Time based nLockTime implemented in 0.1.6
        if (nLockTime == 0)
            return true;
        if (nBlockHeight == 0)
            nBlockHeight = nBestHeight;
        if (nBlockTime == 0)
            nBlockTime = GetAdjustedTime();
        if ((int64_t)nLockTime < ((int64_t)nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
            return true;
        BOOST_FOREACH(const CTxIn& txin, vin)
            if (!txin.IsFinal())
                return false;
        return true;
    }

    bool IsNewerThan(const CTransaction& old) const
    {
        if (vin.size() != old.vin.size())
            return false;
        for (unsigned int i = 0; i < vin.size(); i++)
            if (vin[i].prevout != old.vin[i].prevout)
                return false;

        bool fNewer = false;
        unsigned int nLowest = std::numeric_limits<unsigned int>::max();
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            if (vin[i].nSequence != old.vin[i].nSequence)
            {
                if (vin[i].nSequence <= nLowest)
                {
                    fNewer = false;
                    nLowest = vin[i].nSequence;
                }
                if (old.vin[i].nSequence < nLowest)
                {
                    fNewer = true;
                    nLowest = old.vin[i].nSequence;
                }
            }
        }
        return fNewer;
    }

    bool IsCoinBase() const
    {
        return ((vin.size() == 1) &&
                vin[0].prevout.IsNull() &&
                (vout.size() >= 1));
    }

    bool IsCoinStake() const
    {
        // ppcoin: the coin stake transaction is marked with the first output empty
        return ((vin.size() > 0) &&
                (!vin[0].prevout.IsNull()) &&
                (vout.size() >= 2) &&
                vout[0].IsEmpty());
    }

    bool IsCoinBaseOrStake() const
    {
        return (IsCoinBase() || IsCoinStake());
    }

    bool IsQPoSTx() const;

    /** Check for standard transaction types
        @return True if all outputs (scriptPubKeys) use only standard transaction forms
    */
    bool IsStandard(int nNewHeight=-1) const;

    /** Check for standard transaction types
        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return True if all inputs (scriptSigs) use only standard transaction forms
        @see CTransaction::FetchInputs
    */
    bool AreInputsStandard(const MapPrevTx& mapInputs) const;

    /** Count ECDSA signature operations the old-fashioned (pre-0.6) way
        @return number of sigops this transaction's outputs will produce when spent
        @see CTransaction::FetchInputs
    */
    unsigned int GetLegacySigOpCount() const;

    /** Count ECDSA signature operations in pay-to-script-hash inputs.

        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return maximum number of sigops required to validate this transaction's inputs
        @see CTransaction::FetchInputs
     */
    unsigned int GetP2SHSigOpCount(const MapPrevTx& mapInputs) const;

    /** Amount of XST spent by this transaction.
        @return sum of all outputs (note: does not include fees)
     */
    int64_t GetValueOut() const
    {
        int64_t nValueOut = 0;
        BOOST_FOREACH(const CTxOut& txout, vout)
        {
            nValueOut += txout.nValue;
            if (!MoneyRange(txout.nValue) || !MoneyRange(nValueOut))
                throw std::runtime_error("CTransaction::GetValueOut() : value out of range");
        }
        return nValueOut;
    }

    /** Amount of XST coming in to this transaction
        Note that lightweight clients may not know anything besides the hash of previous transactions,
        so may not be able to calculate this.

        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return	Sum of value of all inputs (scriptSigs)
        @see CTransaction::FetchInputs
     */
    int64_t GetValueIn(const MapPrevTx& mapInputs, int64_t nClaim=0) const;

    /** Amount of XST coming in to this transaction via qPoS claims.
        This method performs no checks.

        @return Sum of value of all claims
     */
    int64_t GetClaimIn() const;

    static bool AllowFree(double dPriority)
    {
        // Large (in bytes) low-priority (new, small-coin) transactions
        // need a fee.
        // 1440 blocks a day. Priority cutoff is 1 XST day/ 250 bytes
        return dPriority > COIN * 1440 / 250;
    }

    int64_t GetMinFee(unsigned int nBlockSize = 1,
                      enum GetMinFee_mode mode = GMF_BLOCK,
                      unsigned int nBytes = 0) const;

    uint32_t GetFeeworkHardness(unsigned int nBlockSize = 1,
                                enum GetMinFee_mode mode = GMF_BLOCK,
                                unsigned int nBytes = 0) const;

    uint64_t GetFeeworkLimit(unsigned int nBlockSize = 1,
                             enum GetMinFee_mode mode = GMF_BLOCK,
                             unsigned int nBytes = 0) const;

    bool CheckFeework(Feework &feework,
                      bool fRequired,
                      FeeworkBuffer& buffer,
                      unsigned int nBlockSize = 1,
                      enum GetMinFee_mode mode = GMF_BLOCK,
                      bool fCheckDepth = true) const;

    bool ReadFromDisk(CDiskTxPos pos, FILE** pfileRet=NULL)
    {
        CAutoFile filein = CAutoFile(OpenBlockFile(pos.nFile, 0,
                                                   pfileRet ? "rb+" : "rb"),
                                     SER_DISK,
                                     CLIENT_VERSION);
        if (!filein)
            return error("CTransaction::ReadFromDisk() : OpenBlockFile failed");

        // Read transaction
        if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
            return error("CTransaction::ReadFromDisk() : fseek failed");

        try {
            filein >> *this;
        }
        catch (std::exception &e) {
            return error("%s() : deserialize or I/O error", __PRETTY_FUNCTION__);
        }

        // Return file pointer
        if (pfileRet)
        {
            if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
                return error("CTransaction::ReadFromDisk() : second fseek failed");
            *pfileRet = filein.release();
        }
        return true;
    }

    bool HasTimestamp() const
    {
        return (nVersion < CTransaction::NOTXTIME_VERSION);
    }

    unsigned int GetTxTime() const
    {
        return _nTime;
    }

    void SetTxTime(unsigned int nNewTime)
    {
        _nTime = nNewTime;
    }

    void AdjustTime(int nAdjustment)
    {
        // take care not to wrap an unsigned int
        if (nAdjustment < 0)
        {
            if (-nAdjustment >= (int)_nTime)
            {
                _nTime = 0;
            }
            else
            {
                _nTime += nAdjustment;
            }
        }
        // sanity check
        else if (nAdjustment <= (int)_nTime)
        {
            _nTime += nAdjustment;
        }
        else
        {
            _nTime += _nTime;
        }
    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        if (a.HasTimestamp() && b.HasTimestamp() &&
            (a.GetTxTime() != b.GetTxTime()))
		{
            return false;
        }
        return (a.nVersion    == b.nVersion &&
                a.vin         == b.vin &&
                a.vout        == b.vout &&
                a.nLockTime   == b.nLockTime);
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return !(a == b);
    }

    std::string ToStringShort() const
    {
        std::string str;
        str += strprintf("%s %s", GetHash().ToString().c_str(), IsCoinBase()? "base" : (IsCoinStake()? "stake" : "user"));
        return str;
    }

    std::string ToString() const
    {
        std::string str;
        str += IsCoinBase()? "Coinbase" : (IsCoinStake()? "Coinstake" : "CTransaction");
        str += strprintf("(hash=%s, nTime=%d, ver=%d, vin.size=%" PRIszu ", vout.size=%" PRIszu ", nLockTime=%d)\n",
            GetHash().ToString().substr(0,10).c_str(),
            _nTime,
            nVersion,
            vin.size(),
            vout.size(),
            nLockTime
			);

        for (unsigned int i = 0; i < vin.size(); i++)
            str += "    " + vin[i].ToString() + "\n";
        for (unsigned int i = 0; i < vout.size(); i++)
            str += "    " + vout[i].ToString() + "\n";
        return str;
    }

    void print() const
    {
        printf("%s", ToString().c_str());
    }

   /**************************************************************************
    *  qPoS
    **************************************************************************/
    // idx is input index
    bool GetSignatory(const MapPrevTx &mapInputs,
                      unsigned int idx, CPubKey &keyRet) const;
    bool ValidateSignatory(const MapPrevTx &mapInputs,
                           int idx, CPubKey &key) const;
    bool ValidateSignatory(const MapPrevTx &mapInputs,
                           int idx, std::vector<CPubKey> &vKeys) const;
    bool ValidateSignatory(const QPRegistry *pregistry,
                           const MapPrevTx &mapInputs,
                           int idx, unsigned int nStakerID,
                           QPKeyType fKeyTypes) const;
    // Check* procedures not only validate, but also return operational data
    bool CheckPurchases(const QPRegistry *pregistry,
                        int64_t nStakerPrice,
                        std::map<std::string, qpos_purchase> &mapRet) const;
    bool CheckSetKeys(const QPRegistry *pregistry,
                      const MapPrevTx &mapInputs,
                      std::vector<qpos_setkey> &vRet) const;
    bool CheckSetState(const QPRegistry *pregistry,
                       const MapPrevTx &mapInputs,
                       qpos_setstate &setstateRet) const;
    bool CheckClaim(const QPRegistry *pregistry,
                    const MapPrevTx &mapInputs,
                    qpos_claim &claimRet) const;
    bool CheckSetMetas(const QPRegistry *pregistry,
                       const MapPrevTx &mapInputs,
                       std::vector<qpos_setmeta> &vRet) const;

    // Checks all qPoS operations
    bool CheckQPoS(const QPRegistry *pregistryTemp,
                   const MapPrevTx &mapInputs,
                   unsigned int nTime,
                   const std::vector<QPTxDetails> &vDeets,
                   const CBlockIndex *pindexPrev,
                   std::map<string, qpos_purchase> &mapPurchasesRet,
                   std::map<unsigned int, std::vector<qpos_setkey> > &mapSetKeysRet,
                   std::map<CPubKey, std::vector<qpos_claim> > &mapClaimsRet,
                   std::map<unsigned int, std::vector<qpos_setmeta> > &mapSetMetasRet,
                   std::vector<QPTxDetails> &vDeetsRet) const;

    // faster, returns operational data only, not for validation
    bool GetQPTxDetails(const uint256& hashBlock,
                        std::vector<QPTxDetails> &vDeets) const;

    bool HasFeework() const;

    bool ReadFromDisk(CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet);
    bool ReadFromDisk(CTxDB& txdb, COutPoint prevout);
    bool ReadFromDisk(COutPoint prevout);
    bool DisconnectInputs(CTxDB& txdb);

    /** Fetch from memory and/or disk. inputsRet keys are transaction hashes.

     @param[in] txdb	Transaction database
     @param[in] mapTestPool	List of pending changes to the transaction index database
     @param[in] fBlock	True if being called to add a new best-block to the chain
     @param[in] fMiner	True if being called by CreateNewBlock
     @param[out] inputsRet	Pointers to this transaction's inputs
     @param[out] fInvalid	returns true if transaction is invalid
     @return	Returns true if all inputs are in txdb or mapTestPool
     */
    bool FetchInputs(CTxDB& txdb, const std::map<uint256, CTxIndex>& mapTestPool,
                     bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid) const;

    /** Sanity check previous transactions, then, if all checks succeed,
        mark them as spent by this transaction.

        @param[in] inputs	Previous transactions (from FetchInputs)
        @param[out] mapTestPool	Keeps track of inputs that need to be updated on disk
        @param[in] posThisTx	Position of this transaction on disk
        @param[in] pindexBlock
        @param[in] fBlock	true if called from ConnectBlock
        @param[in] fMiner	true if called from CreateNewBlock
        @param[in] fStrictPayToScriptHash	true if fully validating p2sh transactions
        @return Returns true if all checks succeed
     */
    bool ConnectInputs(CTxDB& txdb, MapPrevTx inputs,
                       std::map<uint256, CTxIndex>& mapTestPool,
                       const CDiskTxPos& posThisTx,
                       const CBlockIndex* pindexBlock,
                       bool fBlock, bool fMiner,
                       unsigned int flags,
                       int64_t nValuePurchases, int64_t nClaim,
                       Feework& feework);
    bool ClientConnectInputs();
    bool CheckTransaction(int nNewHeight=-1) const;
    bool AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs=true, bool* pfMissingInputs=NULL);
    // ppcoin: get transaction coin age
    bool GetCoinAge(CTxDB& txdb, unsigned int nBlockTime, uint64_t& nCoinAge) const;
};





/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction
{
private:
    int GetDepthInMainChainINTERNAL(CBlockIndex* &pindexRet) const;
public:
    uint256 hashBlock;
    std::vector<uint256> vMerkleBranch;
    int nIndex;

    // memory only
    mutable bool fMerkleVerified;


    CMerkleTx()
    {
        Init();
    }

    CMerkleTx(const CTransaction& txIn) : CTransaction(txIn)
    {
        Init();
    }

    void Init()
    {
        hashBlock = 0;
        nIndex = -1;
        fMerkleVerified = false;
    }


    IMPLEMENT_SERIALIZE
    (
        nSerSize += SerReadWrite(s, *(CTransaction*)this, nType, nVersion, ser_action);
        nVersion = this->nVersion;
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    )


    int SetMerkleBranch(const CBlock* pblock=NULL);
    int GetDepthInMainChain(CBlockIndex* &pindexRet) const;
    int GetDepthInMainChain() const {
            CBlockIndex *pindexRet;
            return GetDepthInMainChain(pindexRet);
    }
     bool IsInMainChain() const {
           CBlockIndex *pindexRet;
           return GetDepthInMainChainINTERNAL(pindexRet) > 0;
    }
    int GetBlocksToMaturity() const;
    bool AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs=true);
    bool AcceptToMemoryPool();
};




/**  A txdb record that contains the disk location of a transaction and the
 * locations of transactions that spend its outputs.  vSpent is really only
 * used as a flag, but having the location is very helpful for debugging.
 */
class CTxIndex
{
public:
    CDiskTxPos pos;
    std::vector<CDiskTxPos> vSpent;

    CTxIndex()
    {
        SetNull();
    }

    CTxIndex(const CDiskTxPos& posIn, unsigned int nOutputs)
    {
        pos = posIn;
        vSpent.resize(nOutputs);
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(pos);
        READWRITE(vSpent);
    )

    void SetNull()
    {
        pos.SetNull();
        vSpent.clear();
    }

    bool IsNull()
    {
        return pos.IsNull();
    }

    friend bool operator==(const CTxIndex& a, const CTxIndex& b)
    {
        return (a.pos    == b.pos &&
                a.vSpent == b.vSpent);
    }

    friend bool operator!=(const CTxIndex& a, const CTxIndex& b)
    {
        return !(a == b);
    }
    int GetDepthInMainChain() const;

};





/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 *
 * Blocks are appended to blk0001.dat files on disk.  Their location on disk
 * is indexed by CBlockIndex objects in memory.
 */
class CBlock
{
public:
    static const int GENESIS_VERSION=6;
    static const int PURCHASE_VERSION=7;
    static const int QPOS_VERSION=8;
    static const int CURRENT_VERSION=QPOS_VERSION;

    /* start header */
    int nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    unsigned int nTime;
    unsigned int nBits;
    unsigned int nNonce;
    /* end header GENESIS_VERSION */
    int nHeight;
    unsigned int nStakerID;
    /* end header QPOS_VERSION */

    // network and disk
    std::vector<CTransaction> vtx;

    // ppcoin: block signature - signed by one of the coin base txout[N]'s owner
    std::vector<unsigned char> vchBlockSig;

    // memory only
    mutable std::vector<uint256> vMerkleTree;

    // Denial-of-service detection:
    mutable int nDoS;
    bool DoS(int nDoSIn, bool fIn) const { nDoS += nDoSIn; return fIn; }

    CBlock()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
        if (nVersion >= QPOS_VERSION)
        {
            READWRITE(nHeight);
            READWRITE(nStakerID);
        }

        // ConnectBlock depends on vtx following header to generate CDiskTxPos
        if (!(nType & (SER_GETHASH|SER_BLOCKHEADERONLY)))
        {
            READWRITE(vtx);
            READWRITE(vchBlockSig);
        }
        else if (fRead)
        {
            const_cast<CBlock*>(this)->vtx.clear();
            const_cast<CBlock*>(this)->vchBlockSig.clear();
        }
    )

    void SetNull()
    {
        int nFork = GetFork(nBestHeight + 1);
        if (nFork < XST_FORKPURCHASE)
        {
            nVersion = CBlock::GENESIS_VERSION;
        }
        else if (nFork < XST_FORKQPOS)
        {
            nVersion = CBlock::PURCHASE_VERSION;
        }
        else
        {
            nVersion = CBlock::CURRENT_VERSION;
        }
        hashPrevBlock = 0;
        hashMerkleRoot = 0;
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        nHeight = 0;
        nStakerID = 0;
        vtx.clear();
        vchBlockSig.clear();
        vMerkleTree.clear();
        nDoS = 0;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const
    {
        // unfortunately start of NFTs came in a different fork for testnet
        static const int NFTHEIGHT = (fTestNet ? chainParams.START_MISSFIX_T :
                                                 chainParams.START_NFT_M);

        if (nHeight == NFTHEIGHT)
        {
            CDataStream ss(SER_DISK, CLIENT_VERSION);
            ss << hashOfNftHashes;  // prepend the hash of NFT hashes
            ss << Hash9(BEGIN(nVersion), END(nStakerID));
            return Hash9(ss.begin(), ss.end());
        }
        else if (nVersion < QPOS_VERSION)
        {
            return Hash9(BEGIN(nVersion), END(nNonce));
        }
        else
        {
            return Hash9(BEGIN(nVersion), END(nStakerID));
        }
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    void UpdateTime(const CBlockIndex* pindexPrev);

    unsigned int GetTxVolume()
    {
        if (IsProofOfStake())
        {
            return vtx.size() - 1;
        }
        else
        {
            return vtx.size();
        }
    }

    int64_t GetValueOut()
    {
        int64_t v = 0;
        for (unsigned int i = 0; i < vtx.size(); ++i)
        {
            v += vtx[i].GetValueOut();
        }
        return v;
    }

    // ppcoin: entropy bit for stake modifier if chosen by modifier
    unsigned int GetStakeEntropyBit(unsigned int nHeight) const
    {
        // Take last bit of block hash as entropy bit
        unsigned int nEntropyBit = ((GetHash().Get64()) & 1llu);
        if (fDebug && GetBoolArg("-printstakemodifier"))
            printf("GetStakeEntropyBit: nHeight=%u hashBlock=%s nEntropyBit=%u\n", nHeight, GetHash().ToString().c_str(), nEntropyBit);
        return nEntropyBit;
    }

    // ppcoin: two types of block: proof-of-work or proof-of-stake
    // xst: 3 types of blocks: PoW, PoS, qPoS
    // 0 is reserved for no staker, so nonzero nStakerID is qPoS block
    bool IsQuantumProofOfStake() const
    {
        return (nStakerID > 0);
    }

    bool IsProofOfStake() const
    {
        return (vtx.size() > 1 && vtx[1].IsCoinStake());
    }

    bool IsProofOfWork() const
    {
        return !(IsProofOfStake() || IsQuantumProofOfStake());
    }

    std::pair<COutPoint, unsigned int> GetProofOfStake() const
    {
        if (IsProofOfStake())
        {
            unsigned int nTxTime = vtx[1].HasTimestamp() ? vtx[1].GetTxTime() : nTime;
            return std::make_pair(vtx[1].vin[0].prevout, nTxTime);
        }
        else
        {
            return std::make_pair(COutPoint(), (unsigned int)0);
        }
    }

    // ppcoin: get max transaction timestamp
    int64_t GetMaxTransactionTime() const
    {
        int64_t maxTransactionTime = 0;
        BOOST_FOREACH(const CTransaction& tx, vtx)
        {
            if (tx.HasTimestamp())
            {
                 maxTransactionTime = std::max(maxTransactionTime,
                                               (int64_t)tx.GetTxTime());
            }
        }
        if ((maxTransactionTime == 0) && ((vtx.size() > 0)))
        {
             maxTransactionTime = nTime;
        }
        return maxTransactionTime;
    }

    uint256 BuildMerkleTree() const
    {
        vMerkleTree.clear();
        BOOST_FOREACH(const CTransaction& tx, vtx)
            vMerkleTree.push_back(tx.GetHash());
        int j = 0;
        for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
        {
            for (int i = 0; i < nSize; i += 2)
            {
                int i2 = std::min(i+1, nSize-1);
                vMerkleTree.push_back(Hash(BEGIN(vMerkleTree[j+i]),  END(vMerkleTree[j+i]),
                                           BEGIN(vMerkleTree[j+i2]), END(vMerkleTree[j+i2])));
            }
            j += nSize;
        }
        return (vMerkleTree.empty() ? 0 : vMerkleTree.back());
    }

    std::vector<uint256> GetMerkleBranch(int nIndex) const
    {
        if (vMerkleTree.empty())
            BuildMerkleTree();
        std::vector<uint256> vMerkleBranch;
        int j = 0;
        for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
        {
            int i = std::min(nIndex^1, nSize-1);
            vMerkleBranch.push_back(vMerkleTree[j+i]);
            nIndex >>= 1;
            j += nSize;
        }
        return vMerkleBranch;
    }

    static uint256 CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex)
    {
        if (nIndex == -1)
            return 0;
        BOOST_FOREACH(const uint256& otherside, vMerkleBranch)
        {
            if (nIndex & 1)
                hash = Hash(BEGIN(otherside), END(otherside), BEGIN(hash), END(hash));
            else
                hash = Hash(BEGIN(hash), END(hash), BEGIN(otherside), END(otherside));
            nIndex >>= 1;
        }
        return hash;
    }


    bool WriteToDisk(unsigned int& nFileRet, unsigned int& nBlockPosRet)
    {
        // Open history file to append
        CAutoFile fileout = CAutoFile(AppendBlockFile(nFileRet), SER_DISK, CLIENT_VERSION);
        if (!fileout)
            return error("CBlock::WriteToDisk() : AppendBlockFile failed");

        // Write index header
        unsigned int nSize = fileout.GetSerializeSize(*this);
        fileout << FLATDATA(pchMessageStart) << nSize;

        // Write block
        long fileOutPos = ftell(fileout);
        if (fileOutPos < 0)
            return error("CBlock::WriteToDisk() : ftell failed");
        nBlockPosRet = fileOutPos;
        fileout << *this;

        // Flush stdio buffers and commit to disk before returning
        fflush(fileout);
        if (!IsInitialBlockDownload() || (nBestHeight+1) % 500 == 0)
            FileCommit(fileout);

        return true;
    }

    bool ReadFromDisk(unsigned int nFile, unsigned int nBlockPos, bool fReadTransactions=true)
    {
        SetNull();

        // Open history file to read
        CAutoFile filein = CAutoFile(OpenBlockFile(nFile, nBlockPos, "rb"), SER_DISK, CLIENT_VERSION);
        if (!filein)
            return error("CBlock::ReadFromDisk() : OpenBlockFile failed");
        if (!fReadTransactions)
            filein.nType |= SER_BLOCKHEADERONLY;

        // Read block
        try {
            filein >> *this;
        }
        catch (std::exception &e) {
            return error("%s() : deserialize or I/O error", __PRETTY_FUNCTION__);
        }

        // Check the header
        if (fReadTransactions && IsProofOfWork() && !CheckProofOfWork(GetHash(), nBits))
            return error("CBlock::ReadFromDisk() : errors in block header");

        return true;
    }



    void print() const
    {
        printf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, "
               "nTime=%u, nBits=%08x, nNonce=%u, vtx=%" PRIszu ", vchBlockSig=%s)\n",
            GetHash().ToString().c_str(),
            nVersion,
            hashPrevBlock.ToString().c_str(),
            hashMerkleRoot.ToString().c_str(),
            nTime, nBits, nNonce,
            vtx.size(),
            HexStr(vchBlockSig.begin(), vchBlockSig.end()).c_str());
        if (IsQuantumProofOfStake())
        {
            printf("  QPoS: nHeight=%d, nStakerID=%d\n", nHeight, nStakerID);
        }
        for (unsigned int i = 0; i < vtx.size(); i++)
        {
            printf("  ");
            vtx[i].print();
        }
        printf("  vMerkleTree: ");
        for (unsigned int i = 0; i < vMerkleTree.size(); i++)
            printf("%s ", vMerkleTree[i].ToString().substr(0,10).c_str());
        printf("\n");
    }


    bool DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex);
    bool ConnectBlock(CTxDB& txdb, CBlockIndex* pindex,
                      QPRegistry *pregistryTemp, bool fJustCheck=false);
    bool ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions=true);
    bool SetBestChain(CTxDB& txdb,
                      CBlockIndex* pindexNew,
                      QPRegistry *pregistryTemp,
                      bool &fReorganizedRet);
    bool SetBestChain(CTxDB& txdb, CBlockIndex* pindexNew);
    bool AddToBlockIndex(unsigned int nFile,
                         unsigned int nBlockPos,
                         const uint256& hashProof,
                         QPRegistry *pregistryTemp);
    bool CheckBlock(QPRegistry *pregistryTemp,
                    std::vector<QPTxDetails> &vDeetsRet,
                    CBlockIndex* pindexPrev,
                    bool fCheckPOW=true,
                    bool fCheckMerkleRoot=true,
                    bool fCheckSig=true,
                    bool fCheckQPoS=true) const;
    bool AcceptBlock(QPRegistry *pregistryTemp,
                     bool fIsMine=false,
                     bool fIsBootstrap=false);
    // ppcoin: calculate total coin age spent in block
    bool GetCoinAge(uint64_t& nCoinAge) const;
    bool SignBlock(const CKeyStore& keystore, const QPRegistry *pregistry);
    bool CheckBlockSignature(const QPRegistry *pregistry) const;

private:
    bool SetBestChainInner(CTxDB& txdb,
                           CBlockIndex *pindexNew,
                           QPRegistry *pregistryTemp);
};






/** The block chain is a tree shaped structure starting with the
 * genesis block at the root, with each block potentially having multiple
 * candidates to be the next block.  pprev and pnext link a path through the
 * main/longest chain.  A blockindex may have multiple pprev pointing back
 * to it, but pnext will only point forward to the longest branch, or will
 * be null if the block is not part of the longest chain.
 */
class CBlockIndex
{
public:
    const uint256* phashBlock;
    CBlockIndex* pprev;
    CBlockIndex* pnext;
    unsigned int nFile;
    unsigned int nBlockPos;
    CBigNum bnChainTrust; // ppcoin: trust score of block chain

    int64_t nMint;
    int64_t nMoneySupply;

    unsigned int nFlags;  // ppcoin: block index flags
    enum
    {
        BLOCK_PROOF_OF_STAKE = (1 << 0), // is proof-of-stake block
        BLOCK_STAKE_ENTROPY  = (1 << 1), // entropy bit for stake modifier
        BLOCK_STAKE_MODIFIER = (1 << 2), // regenerated stake modifier
        BLOCK_QPOS           = (1 << 3), // is qPoS block
    };

    uint64_t nStakeModifier; // hash modifier for proof-of-stake
    unsigned int nStakeModifierChecksum; // checksum of index; in-memeory only

    // proof-of-stake specific fields
    COutPoint prevoutStake;
    unsigned int nStakeTime;
    uint256 hashProofOfStake;

    // block stats
    unsigned int nTxVolume;
    int64_t nXSTVolume;
    uint64_t nPicoPower;
    unsigned int nBlockSize;

    // block header: all blocks
    int nVersion;
    uint256 hashMerkleRoot;
    unsigned int nTime;
    unsigned int nBits;
    unsigned int nNonce;
    // qPoS blocks additional header info
    int nHeight;
    int nStakerID;
    // non-header info
    std::vector<QPTxDetails> vDeets;


    CBlockIndex()
    {
        phashBlock = NULL;
        pprev = NULL;
        pnext = NULL;
        nFile = 0;
        nBlockPos = 0;
        bnChainTrust = 0;
        nMint = 0;
        nMoneySupply = 0;
        nFlags = 0;
        nStakeModifier = 0;
        nStakeModifierChecksum = 0;
        hashProofOfStake = 0;
        prevoutStake.SetNull();
        nStakeTime = 0;

        // block stats
        nTxVolume = 0;
        nXSTVolume = 0;
        nPicoPower = 0;
        nBlockSize = 0;

        nVersion       = 0;
        hashMerkleRoot = 0;
        nTime          = 0;
        nBits          = 0;
        nNonce         = 0;
        // qPoS additional header info
        nHeight        = 0;
        nStakerID      = 0;
        // qPoS non-header info
        vDeets.clear();
    }

    CBlockIndex(unsigned int nFileIn, unsigned int nBlockPosIn, CBlock& block)
    {
        phashBlock = NULL;
        pprev = NULL;
        pnext = NULL;
        nFile = nFileIn;
        nBlockPos = nBlockPosIn;
        bnChainTrust = 0;
        nMint = 0;
        nMoneySupply = 0;
        nFlags = 0;
        nStakeModifier = 0;
        nStakeModifierChecksum = 0;
        hashProofOfStake = 0;
        if (block.IsProofOfStake())
        {
            SetProofOfStake();
            prevoutStake = block.vtx[1].vin[0].prevout;
            if (block.vtx[1].HasTimestamp())
            {
                nStakeTime = block.vtx[1].GetTxTime();
            }
            else
            {
                nStakeTime = nTime;
            }
        }
        else
        {
            prevoutStake.SetNull();
            nStakeTime = 0;
        }

        nTxVolume = block.GetTxVolume();
        nXSTVolume = block.GetValueOut();
        nPicoPower = 0;
        nBlockSize = GetSerializeSize(block, SER_DISK, CLIENT_VERSION);

        nVersion       = block.nVersion;
        hashMerkleRoot = block.hashMerkleRoot;
        nTime          = block.nTime;
        nBits          = block.nBits;
        nNonce         = block.nNonce;
        nHeight        = block.nHeight;
        nStakerID      = block.nStakerID;
        if (block.IsQuantumProofOfStake())
        {
            SetQuantumProofOfStake();
        }
        uint256 hashBlock(block.GetHash());
        vDeets.clear();
        BOOST_FOREACH(const CTransaction &tx, block.vtx)
        {
           std::vector<QPTxDetails> vDeetsTx;
           tx.GetQPTxDetails(hashBlock, vDeetsTx);
           vDeets.insert(vDeets.end(), vDeetsTx.begin(), vDeetsTx.end());
        }
    }

    CBlock GetBlockHeader() const
    {
        CBlock block;
        block.nVersion       = nVersion;
        if (pprev)
            block.hashPrevBlock = pprev->GetBlockHash();
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        if (IsQuantumProofOfStake())
        {
            block.nHeight        = nHeight;
            block.nStakerID      = nStakerID;
        }
        else
        {
            block.nHeight        = 0;
            block.nStakerID      = 0;
        }
        return block;
    }

    uint256 GetBlockHash() const
    {
        return *phashBlock;
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    CBigNum GetBlockTrust(const QPRegistry *pregistry) const;

    bool IsInMainChain() const
    {
        return (pnext || this == pindexBest);
    }

    bool CheckIndex() const
    {
        return true;
    }

    int64_t GetPastTimeLimit() const
    {
        if (GetFork(this->nHeight) >= XST_FORK005)
        {
             return GetBlockTime();
        }
        else
        {
             return GetMedianTimePast();
        }
    }

    enum { nMedianTimeSpan=11 };

    int64_t GetMedianTimePast() const
    {
        int64_t pmedian[nMedianTimeSpan];
        int64_t* pbegin = &pmedian[nMedianTimeSpan];
        int64_t* pend = &pmedian[nMedianTimeSpan];

        const CBlockIndex* pindex = this;
        for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev)
            *(--pbegin) = pindex->GetBlockTime();

        std::sort(pbegin, pend);
        return pbegin[(pend - pbegin)/2];
    }

    int64_t GetMedianTime() const
    {
        const CBlockIndex* pindex = this;
        for (int i = 0; i < nMedianTimeSpan/2; i++)
        {
            if (!pindex->pnext)
                return GetBlockTime();
            pindex = pindex->pnext;
        }
        return pindex->GetMedianTimePast();
    }

    /**
     * Returns true if there are nRequired or more blocks of minVersion or above
     * in the last nToCheck blocks, starting at pstart and going backwards.
     */
    static bool IsSuperMajority(int minVersion, const CBlockIndex* pstart,
                                unsigned int nRequired, unsigned int nToCheck);

    bool IsProofOfWork() const
    {
        return !(nFlags & (BLOCK_PROOF_OF_STAKE | BLOCK_QPOS));
    }

    bool IsProofOfStake() const
    {
        return (nFlags & BLOCK_PROOF_OF_STAKE);
    }

    bool IsQuantumProofOfStake() const
    {
        return (nFlags & BLOCK_QPOS);
    }

    void SetProofOfStake()
    {
        nFlags |= BLOCK_PROOF_OF_STAKE;
    }

    void SetQuantumProofOfStake()
    {
        nFlags |= BLOCK_QPOS;
    }

    unsigned int GetStakeEntropyBit() const
    {
        return ((nFlags & BLOCK_STAKE_ENTROPY) >> 1);
    }

    bool SetStakeEntropyBit(unsigned int nEntropyBit)
    {
        if (nEntropyBit > 1)
            return false;
        nFlags |= (nEntropyBit? BLOCK_STAKE_ENTROPY : 0);
        return true;
    }

    bool GeneratedStakeModifier() const
    {
        return (nFlags & BLOCK_STAKE_MODIFIER);
    }

    void SetStakeModifier(uint64_t nModifier, bool fGeneratedStakeModifier)
    {
        nStakeModifier = nModifier;
        if (fGeneratedStakeModifier)
            nFlags |= BLOCK_STAKE_MODIFIER;
    }

    std::string ToString() const
    {
        std::string strStakerID("");
        if (IsQuantumProofOfStake())
        {
            strStakerID = strprintf(" nStakerID=%d,", nStakerID);
        }
        return strprintf("CBlockIndex(nprev=%p, pnext=%p, nFile=%u, "
                         "nBlockPos=%-6d nHeight=%d,%s nMint=%s, "
                         "nMoneySupply=%s, nFlags=(%s)(%d)(%s), "
                         "nStakeModifier=%016" PRIx64 ", "
                         "nStakeModifierChecksum=%08x, hashProofOfStake=%s, "
                         "prevoutStake=(%s), nStakeTime=%d merkle=%s, "
                         "hashBlock=%s)",
            pprev, pnext, nFile, nBlockPos, nHeight, strStakerID.c_str(),
            FormatMoney(nMint).c_str(), FormatMoney(nMoneySupply).c_str(),
            GeneratedStakeModifier() ? "MOD" : "-", GetStakeEntropyBit(),
            IsProofOfStake() ? "PoS" : IsQuantumProofOfStake() ? "QPoS" : "PoW",
            nStakeModifier, nStakeModifierChecksum,
            hashProofOfStake.ToString().c_str(),
            prevoutStake.ToString().c_str(), nStakeTime,
            hashMerkleRoot.ToString().c_str(),
            GetBlockHash().ToString().c_str());
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};



/** Used to marshal pointers into hashes for db storage. */
class CDiskBlockIndex : public CBlockIndex
{
private:
    uint256 blockHash;
public:
    uint256 hashPrev;
    uint256 hashNext;

    CDiskBlockIndex()
    {
        hashPrev = 0;
        hashNext = 0;
        blockHash = 0;
    }

    explicit CDiskBlockIndex(CBlockIndex* pindex) : CBlockIndex(*pindex)
    {
        hashPrev = (pprev ? pprev->GetBlockHash() : 0);
        hashNext = (pnext ? pnext->GetBlockHash() : 0);
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
        {
            READWRITE(nVersion);
        }

        READWRITE(hashNext);
        READWRITE(nFile);
        READWRITE(nBlockPos);
        READWRITE(nHeight);
        READWRITE(nMint);
        READWRITE(nMoneySupply);
        READWRITE(nFlags);
        READWRITE(nStakeModifier);
        if (IsProofOfStake())
        {
            READWRITE(prevoutStake);
            READWRITE(nStakeTime);
            READWRITE(hashProofOfStake);
        }
        else if (fRead)
        {
            const_cast<CDiskBlockIndex*>(this)->prevoutStake.SetNull();
            const_cast<CDiskBlockIndex*>(this)->nStakeTime = 0;
            const_cast<CDiskBlockIndex*>(this)->hashProofOfStake = 0;
        }

        // block stats
        READWRITE(nTxVolume);
        READWRITE(nXSTVolume);
        READWRITE(nPicoPower);
        READWRITE(nBlockSize);

        // block header
        READWRITE(this->nVersion);
        READWRITE(hashPrev);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
        if (IsQuantumProofOfStake())
        {
            READWRITE(nStakerID);
        }
        READWRITE(blockHash);
        if (GetFork(this->nHeight) >= XST_FORKPURCHASE)
        {
            READWRITE(vDeets);
        }
    )

    uint256 GetBlockHash() const
    {
        if((nTime < GetAdjustedTime() - 24 * 60 * 60) && blockHash != 0)
            return blockHash;
        CBlock block;
        block.nVersion        = nVersion;
        block.hashPrevBlock   = hashPrev;
        block.hashMerkleRoot  = hashMerkleRoot;
        block.nTime           = nTime;
        block.nBits           = nBits;
        block.nNonce          = nNonce;
        if (IsQuantumProofOfStake())
        {
            block.nHeight     = nHeight;
            block.nStakerID   = nStakerID;
        }
        const_cast<CDiskBlockIndex*>(this)->blockHash = block.GetHash();
        return blockHash;
    }

    std::string ToString() const
    {
        std::string str = "CDiskBlockIndex(";
        str += CBlockIndex::ToString();
        str += strprintf("\n                hashBlock=%s, hashPrev=%s, hashNext=%s)",
            GetBlockHash().ToString().c_str(),
            hashPrev.ToString().c_str(),
            hashNext.ToString().c_str());
        return str;
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};


/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
class CBlockLocator
{
protected:
    std::vector<uint256> vHave;
public:

    CBlockLocator()
    {
    }

    explicit CBlockLocator(const CBlockIndex* pindex)
    {
        Set(pindex);
    }

    explicit CBlockLocator(uint256 hashBlock)
    {
        std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end())
            Set((*mi).second);
    }

    CBlockLocator(const std::vector<uint256>& vHaveIn)
    {
        vHave = vHaveIn;
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    )

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull()
    {
        return vHave.empty();
    }

    void Set(const CBlockIndex* pindex)
    {
        static const uint256 HASH_GENESIS = fTestNet ?
                                     chainParams.hashGenesisBlockTestNet :
                                     hashGenesisBlock;

        vHave.clear();

        if (!pindex->pprev)
        {
            if (pindex)
            {
                vHave.push_back(HASH_GENESIS);
            }
            return;
        }

        int nHeight = pindex->nHeight;

        int nStep = 1;
        while (pindex)
        {
            vHave.push_back(pindex->GetBlockHash());

            nHeight -= nStep;
            if (nHeight < 1)
            {
                break;
            }

            if (nStep < 21)
            {
                // given >2M blocks, it is cheaper to step through pprev
                //    than search a map until step is around 20
                for (int i = 0; pindex && i < nStep; i++)
                {
                    pindex = pindex->pprev;
                }
            }
            else if (mapBlockLookup.count(nHeight))
            {
                pindex = mapBlockLookup[nHeight];
            }
            else
            {
                printf("CBlockLocator::Set(): TSNH no block found at %d\n",
                       nHeight);
                break;
            }


            // Exponentially larger steps back
            if (vHave.size() > 10)
            {
                nStep *= 2;
            }
        } 
        vHave.push_back(HASH_GENESIS);
    }

    int GetDistanceBack()
    {
        // Retrace how far back it was in the sender's branch
        int nDistance = 0;
        int nStep = 1;
        BOOST_FOREACH(const uint256& hash, vHave)
        {
            std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end())
            {
                CBlockIndex* pindex = (*mi).second;
                if (pindex->IsInMainChain())
                    return nDistance;
            }
            nDistance += nStep;
            if (nDistance > 10)
                nStep *= 2;
        }
        return nDistance;
    }

    CBlockIndex* GetBlockIndex()
    {
        // Find the first block the caller has in the main chain
        BOOST_FOREACH(const uint256& hash, vHave)
        {
            std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end())
            {
                CBlockIndex* pindex = (*mi).second;
                if (pindex->IsInMainChain())
                    return pindex;
            }
        }
        return pindexGenesisBlock;
    }

    uint256 GetBlockHash()
    {
        // Find the first block the caller has in the main chain
        BOOST_FOREACH(const uint256& hash, vHave)
        {
            std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end())
            {
                CBlockIndex* pindex = (*mi).second;
                if (pindex->IsInMainChain())
                    return hash;
            }
        }
        return (fTestNet ? chainParams.hashGenesisBlockTestNet :
                           hashGenesisBlock);
    }

    int GetHeight()
    {
        CBlockIndex* pindex = GetBlockIndex();
        if (!pindex)
            return 0;
        return pindex->nHeight;
    }
};


typedef std::map<int, std::set<uint256> > MapFeeless;

class CTxMemPool
{
public:
    mutable CCriticalSection cs;
    std::map<uint256, CTransaction> mapTx;
    std::map<COutPoint, CInPoint> mapNextTx;

    // cache for qPoS reward claiming transactions <tx hash, key>
    // CPubKey.IsCompressed() == true means it is a claim
    std::map<uint256, CPubKey> mapClaims;

    // cache for staker registration aliases <tx hash, <alias>>
    // string.empty() == false means it is a registration
    std::map<uint256, std::vector<std::string> > mapRegistrations;

    // cache for feeless transactions in the mempool
    // key is the height hashed into the feework
    MapFeeless mapFeeless;


    bool accept(CTxDB& txdb, CTransaction &tx,
                bool fCheckInputs, bool* pfMissingInputs = NULL);
    bool addUnchecked(const uint256& hash, CTransaction &tx);
    bool remove(const CTransaction &tx, bool fRecursive = false);
    bool removeConflicts(const CTransaction &tx);
    void clear();
    void queryHashes(std::vector<uint256>& vtxid);

    unsigned long size()
    {
        LOCK(cs);
        return mapTx.size();
    }

    bool exists(uint256 hash)
    {
        return (mapTx.count(hash) != 0);
    }

    // don't use the following without checking if it exists first
    CTransaction& lookup(uint256 hash)
    {
        return mapTx[hash];
    }

    bool lookup(uint256 hash, CTransaction& result) const
    {
        std::map<uint256, CTransaction>::const_iterator i = mapTx.find(hash);
        if (i==mapTx.end()) return false;
        result = i->second;
        return true;
    }

    int removeInvalidPurchases();

    bool addFeeless(const int nHeight, const uint256& txid);
    int removeOldFeeless();
};

const CTxOut& GetOutputFor(const CTxIn& input, const MapPrevTx& inputs);

extern CTxMemPool mempool;

#endif
