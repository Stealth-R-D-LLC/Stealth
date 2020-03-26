// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPREGISTRY_H_
#define _QPREGISTRY_H_ 1

#include "qPoS.hpp"
#include "QPQueue.hpp"
#include "QPPowerRound.hpp"
#include "aliases.hpp"
#include "meta.hpp"

#include <boost/random.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lockable_adapter.hpp>

class CBlockIndex;

typedef std::map<unsigned int, QPStaker>::const_iterator QPRegistryIterator;

typedef boost::variate_generator<boost::mt19937&,
                                 boost::uniform_int<> > QPShuffler;

static const uint64_t SYNCREG_SLEEP_MS = 1;

class QPRegistry
    : public boost::basic_lockable_adapter<boost::mutex>
{
private:
    // persistent
    int nVersion;
    unsigned int nRound;
    uint32_t nRoundSeed;
    std::map<unsigned int, QPStaker> mapStakers;
    std::map<CPubKey, int64_t> mapBalances;
    std::map<CPubKey, int64_t> mapLastClaim;
    std::map<CPubKey, int> mapActive;
    std::map<std::string, std::pair<unsigned int, std::string> > mapAliases;
    QPQueue queue;
    unsigned int nIDCounter;
    unsigned int nIDSlotPrev;
    bool fCurrentBlockWasProduced;
    bool fPrevBlockWasProduced;
    int nBlockHeight;
    uint256 hashBlock;
    uint256 hashBlockLastSnapshot;
    uint256 hashLastBlockPrev1Queue;
    uint256 hashLastBlockPrev2Queue;
    uint256 hashLastBlockPrev3Queue;
    QPPowerRound powerRoundPrev;
    QPPowerRound powerRoundCurrent;

    // not persistent
    bool fIsInReplayMode;
    bool fShouldRollback;

    unsigned int Size() const;
    unsigned int GetCurrentIDCounter() const;
    unsigned int GetIDForPrevSlot() const;
    bool GetPrevRecentBlocksMissedMax(unsigned int nID,
                                      uint32_t &nMaxRet) const;
    bool GetStakerForAlias(const std::string &sAlias,
                           QPStaker &qStakerRet) const;
    bool StakerProducedBlock(const CBlockIndex *const pindex,
                             int64_t nReward);
    bool StakerMissedBlock(unsigned int nID);
    bool DisqualifyStaker(unsigned int nID);
    bool TerminateStaker(unsigned int nID);
    bool DisqualifyStakerIfNecessary(unsigned int nID,
                                     const QPStaker* staker);
    void TerminateDisqualifiedStakers();
    bool DockInactiveKeys(int64_t nMoneySupply);
    void PurgeLowBalances(int64_t nMoneySupply);
    bool NewQueue(unsigned int nTime0, const uint256 &prevHash);
    unsigned int IncrementID();
    bool ApplyPurchase(const QPTxDetails& deet);
    bool ApplySetKey(const QPTxDetails &deet);
    bool ApplySetState(const QPTxDetails &deet);
    bool ApplyClaim(const QPTxDetails &deet, int64_t nBlockTime);

    bool ApplySetMeta(const QPTxDetails &deet);
public:
    static const int QPOS_VERSION = 1;
    static const int CURRENT_VERSION = QPOS_VERSION;

    QPRegistry();
    QPRegistry(const QPRegistry *const pother);
    void SetNull();
    bool IsInReplayMode() const;
    unsigned int GetRound() const;
    unsigned int GetRoundSeed() const;
    int GetBlockHeight() const;
    uint256 GetBlockHash() const;
    uint256 GetHashLastBlockPrev1Queue() const;
    uint256 GetHashLastBlockPrev2Queue() const;
    uint256 GetHashLastBlockPrev3Queue() const;
    unsigned int GetNumberQualified() const;
    bool IsQualifiedStaker(unsigned int nStakerID) const;
    bool TimestampIsValid(unsigned int nStakerID, unsigned int nTime) const;
    bool TimeIsInCurrentSlotWindow(unsigned int nTime) const;
    unsigned int GetQueueMinTime() const;
    unsigned int GetCurrentSlot() const;
    unsigned int GetCurrentSlotStart() const;
    unsigned int GetCurrentSlotEnd() const;
    bool CurrentBlockWasProduced() const;
    bool GetBalanceForPubKey(const CPubKey &key, int64_t &nBalanceRet) const;
    bool PubKeyIsInLedger(const CPubKey &key) const;
    bool PubKeyActiveCount(const CPubKey &key, int &nCountRet) const;
    bool PubKeyIsInactive(const CPubKey &key, bool &fInActiveRet) const;
    bool CanClaim(const CPubKey &key,
                  int64_t nValue,
                  int64_t nClaimTime=0) const;
    bool GetOwnerKey(unsigned int nStakerID, CPubKey &keyRet) const;
    bool GetDelegateKey(unsigned int nStakerID, CPubKey &keyRet) const;
    bool GetControllerKey(unsigned int nStakerID, CPubKey &keyRet) const;
    bool GetStakerWeight(unsigned int nStakerID,
                         unsigned int &nWeightRet) const;
    bool AliasIsAvailable(const std::string &sAlias,
                          std::string &sKeyRet) const;
    bool GetIDForAlias(const std::string &sAlias,
                       unsigned int &nIDRet) const;
    bool GetAliasForID(unsigned int nID,
                       std::string &nAliasRet) const;
    std::string GetAliasForID(unsigned int nID);
    void AsJSON(json_spirit::Object &objRet) const;
    void GetStakerAsJSON(unsigned int nID,
                         json_spirit::Object &objRet,
                         bool fWithRecentBlocks=false) const;
    unsigned int GetIDForCurrentSlot() const;
    uint64_t GetPicoPower() const;
    uint64_t GetPicoPowerPrev() const;
    uint64_t GetPicoPowerCurrent() const;
    bool HasEnoughPower() const;
    bool ShouldRollback() const;

    void GetCertifiedNodes(std::vector<std::string> &vNodesRet) const;
    bool IsCertifiedNode(const std::string &sNodeAddress) const;

    bool GetStaker(unsigned int nID, QPStaker &StakerRet) const;
    bool GetStaker(unsigned int nID, QPStaker* &pStakerRet);

    void CheckSynced();
    void Copy(const QPRegistry *const pother);
    void ActivatePubKey(const CPubKey &key);
    bool DeactivatePubKey(const CPubKey &key);
    bool SetStakerAlias(unsigned int nID, const std::string &sAlias);

    bool DockInactiveBalances();

    bool UpdateOnNewBlock(const CBlockIndex *const pindex,
                          bool fWriteSnapshot,
                          bool fWriteLog=false);
    bool UpdateOnNewTime(unsigned int nTime,
                         const CBlockIndex *const pindex,
                         bool fWriteSnapshot,
                         bool fWriteLog=false);

    void ApplyOps(const CBlockIndex *const pindex);

    void ExitReplayMode();

    bool GetIDForCurrentTime(CBlockIndex *pindex,
                             unsigned int &nIDRet,
                             unsigned int &nTimeRet);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;

        READWRITE(nRound);
        READWRITE(nRoundSeed);
        READWRITE(mapStakers);
        READWRITE(mapBalances);
        READWRITE(mapAliases);
        READWRITE(queue);
        READWRITE(nIDCounter);
        READWRITE(nIDSlotPrev);
        READWRITE(fCurrentBlockWasProduced);
        READWRITE(fPrevBlockWasProduced);
        READWRITE(nBlockHeight);
        READWRITE(hashBlock);
        READWRITE(hashBlockLastSnapshot);
        READWRITE(hashLastBlockPrev1Queue);
        READWRITE(hashLastBlockPrev2Queue);
        READWRITE(hashLastBlockPrev3Queue);
        READWRITE(powerRoundPrev);
        READWRITE(powerRoundCurrent);
    )
};

#endif  /* _QPREGISTRY_H_ */
