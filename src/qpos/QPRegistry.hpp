// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPREGISTRY_H_
#define _QPREGISTRY_H_ 1

#include "qPoS.hpp"
#include "QPQueue.hpp"
#include "QPSlotInfo.hpp"
#include "QPPowerRound.hpp"
#include "aliases.hpp"
#include "meta.hpp"
#include "nfts.hpp"

#include <boost/random/uniform_int.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lockable_adapter.hpp>

class CBlockIndex;
class CTxDB;

typedef std::map<unsigned int, QPStaker> QPMapStakers;
typedef QPMapStakers::iterator QPRegistryIterator;
typedef QPMapStakers::const_iterator QPRegistryConstIterator;

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
    QPMapStakers mapStakers;
    std::map<CPubKey, int64_t> mapBalances;
    std::map<CPubKey, int64_t> mapLastClaim;
    std::map<CPubKey, int> mapActive;
    std::map<std::string, std::pair<unsigned int, std::string> > mapAliases;
    QPQueue queue;
    QPQueue queuePrev;
    std::bitset<QP_REGISTRY_RECENT_BLOCKS> bRecentBlocks;
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
    // key: staker ID  |  value: nft ID
    QPMapNftOwnership mapNftOwners;
    // key: nftID  |  value: staker ID
    QPMapNftOwnership mapNftOwnerLookup;

    // not persistent
    unsigned int nHeightExitedReplay;
    bool fShouldRollback;
    std::map<unsigned int, string> mapCertifiedNodes;

    void CopyInternal(const QPRegistry *const pother);

    unsigned int Size() const;
    bool GetPrevRecentBlocksMissedMax(unsigned int nID,
                                      uint32_t &nMaxRet) const;

    unsigned int GetNumberOf(bool (QPStaker::* f)() const) const;
    unsigned int GetNumberProductiveInternal() const;
    unsigned int GetNumberEnabledInternal() const;
    unsigned int GetNumberDisabledInternal() const;
    unsigned int GetNumberQualifiedInternal() const;
    unsigned int GetNumberDisqualifiedInternal() const;

    QPStaker* GetStakerInternal(unsigned int nID);
    const QPStaker* GetStakerInternal(unsigned int nID) const;

    QPStaker* GetStakerForAlias(const std::string &sAlias);

    bool StakerProducedBlock(const CBlockIndex *pindex,
                             int64_t nReward);
    bool StakerMissedBlock(unsigned int nID, int nHeight);
    bool DisableStaker(unsigned int nID, int nHeight);
    bool DisableStakerIfNecessary(unsigned int nID,
                                  const QPStaker* pstaker,
                                  int nHeight);
    bool DisqualifyStaker(unsigned int nID);
    bool DisqualifyStakerIfNecessary(unsigned int nID,
                                     const QPStaker* pstaker);

    void ActivatePubKey(const CPubKey &key, const CBlockIndex* pindex);
    bool DeactivatePubKey(const CPubKey &key);
    bool SetStakerAlias(unsigned int nID, const std::string &sAlias);
    bool SetStakerNft(unsigned int nStakerID, unsigned int nNftID);

    bool IsNftKey(const string &sCharKey) const;

    bool NftIsAvailableInternal(const unsigned int nID) const;
    bool NftIsAvailableInternal(const unsigned int nID,
                                std::string &sCharKeyRet) const;
    bool NftIsAvailableInternal(const std::string sCharKey,
                                unsigned int& nIDRet) const;

    bool DockInactiveKeys(int64_t nMoneySupply);
    void PurgeLowBalances(int64_t nMoneySupply);
    bool NewQueue(unsigned int nTime0, const uint256 &prevHash);
    unsigned int IncrementID();

    bool AliasIsAvailableInternal(const std::string &sAlias,
                                  std::string &sKeyRet) const;
    bool GetIDForAliasInternal(const std::string &sAlias,
                               unsigned int &nIDRet) const;
    bool GetAliasForIDInternal(unsigned int nID,
                               std::string &sAliasRet) const;
    std::string GetAliasForIDInternal(unsigned int nID) const;

    unsigned int GetIDForPrevSlot() const;
    unsigned int GetIDForCurrentSlot() const;

    uint64_t GetPicoPowerInternal() const;
    uint64_t GetPicoPowerPrevInternal() const;
    uint64_t GetPicoPowerCurrentInternal() const;
    bool HasEnoughPower() const;

    bool CanClaimInternal(const CPubKey &key,
                          int64_t nValue,
                          int64_t nClaimTime=0) const;

    bool ApplyPurchase(const QPTxDetails& deet, const CBlockIndex* pindex);
    bool ApplySetKey(const QPTxDetails &deet, const CBlockIndex* pindex);
    bool ApplySetState(const QPTxDetails &deet, int nHeight);
    bool ApplyClaim(const QPTxDetails &deet, int64_t nBlockTime);
    bool ApplySetMeta(const QPTxDetails &deet);
    int ApplyOps(const CBlockIndex *const pindex);

    void GetSlotsInfoInternal(int64_t nTime,
                              unsigned int nSlotFirst,
                              const QPQueue& q,
                              std::vector<QPSlotInfo> &vRet) const;

    bool ReadSnapshotInternal(CTxDB& txdb, int nHeight);
    bool WriteSnapshotInternal(CTxDB& txdb, int nHeight) const;


    bool UpdateOnNewTimeInternal(unsigned int nTime,
                                 const CBlockIndex* const pindex,
                                 int nSnapshotType,
                                 bool fWriteLog = false);

    unsigned int UpdateCertifiedNodesList();

    void ExitReplayModeInternal();

    template<typename Selector>
    void GetStakersWhere(QPMapStakers& mapRet, Selector select) const
    {
        boost::lock_guard<const QPRegistry> lock(*this);

        mapRet.clear();
        for (const auto& pair : mapStakers)
        {
            if (select(pair.second))
            {
                mapRet[pair.first] = pair.second;
            }
        }
    }



public:
    enum SnapType
    {
        NO_SNAPS = 0,
        SPARSE_SNAPS,
        ALL_SNAPS
    };

    static const int QPOS_VERSION = 1;
    static const int CURRENT_VERSION = QPOS_VERSION;

    QPRegistry();
    QPRegistry(const QPRegistry *const pother);
    void SetNull();
    bool IsInReplayMode() const;
    bool OutOfReplayMode() const;
    unsigned int GetRound() const;
    unsigned int GetRoundSeed() const;
    int GetBlockHeight() const;
    uint256 GetBlockHash() const;
    uint256 GetHashLastBlockPrev1Queue() const;
    uint256 GetHashLastBlockPrev2Queue() const;
    uint256 GetHashLastBlockPrev3Queue() const;
    int64_t GetTotalEarned() const;
    unsigned int GetNumberProductive() const;
    unsigned int GetNumberEnabled() const;
    unsigned int GetNumberDisabled() const;
    unsigned int GetNumberQualified() const;
    unsigned int GetNumberDisqualified() const;
    bool IsEnabledStaker(unsigned int nStakerID) const;
    bool CanEnableStaker(unsigned int nStakerID, int nHeight) const;
    bool IsQualifiedStaker(unsigned int nStakerID) const;
    bool TimestampIsValid(unsigned int nStakerID, unsigned int nTime) const;
    unsigned int GetQueueMinTime() const;
    unsigned int GetCurrentSlot() const;
    unsigned int GetCurrentSlotStart() const;
    unsigned int GetCurrentSlotEnd() const;
    bool TimeIsInCurrentSlotWindow(unsigned int nTime) const;
    unsigned int GetCurrentID() const;
    unsigned int GetCurrentQueueSize() const;
    unsigned int GetPreviousQueueSize() const;
    void GetCurrentSlotsInfo(int64_t nTime,
                             unsigned int nSlotFirst,
                             std::vector<QPSlotInfo> &vRet) const;
    void GetPreviousSlotsInfo(int64_t nTime,
                              unsigned int nSlotFirst,
                              std::vector<QPSlotInfo> &vRet) const;
    bool CurrentBlockWasProduced() const;
    unsigned int GetRecentBlocksSize() const;
    int GetRecentBlock(unsigned int n) const;
    unsigned int GetCurrentIDCounter() const;
    unsigned int GetNextIDCounter() const;
    bool GetBalanceForPubKey(const CPubKey &key, int64_t &nBalanceRet) const;
    bool PubKeyIsInLedger(const CPubKey &key) const;
    bool PubKeyActiveCount(const CPubKey &key, int &nCountRet) const;
    bool PubKeyIsInactive(const CPubKey &key, bool &fInActiveRet) const;
    bool CanClaim(const CPubKey &key,
                  int64_t nValue,
                  int64_t nClaimTime=0) const;
    bool GetStakerAuthorities(unsigned int nStakerID,
                              qpos_authorities &ret) const;
    bool GetOwnerKey(unsigned int nStakerID, CPubKey &keyRet) const;
    bool GetManagerKey(unsigned int nStakerID, CPubKey &keyRet) const;
    bool GetDelegateKey(unsigned int nStakerID, CPubKey &keyRet) const;
    bool GetControllerKey(unsigned int nStakerID, CPubKey &keyRet) const;
    bool GetStakerWeight(unsigned int nStakerID,
                         unsigned int &nWeightRet) const;
    bool AliasIsAvailable(const std::string &sAlias,
                          std::string &sKeyRet) const;
    bool GetIDForAlias(const std::string &sAlias,
                       unsigned int &nIDRet) const;
    bool GetAliasForID(unsigned int nID,
                       std::string &sAliasRet) const;
    std::string GetAliasForID(unsigned int nID) const;
    void AsJSON(json_spirit::Object &objRet) const;
    void GetStakerAsJSON(unsigned int nID,
                         json_spirit::Object &objRet,
                         bool fWithRecentBlocks=false) const;
    uint64_t GetPicoPower() const;
    uint64_t GetPicoPowerPrev() const;
    uint64_t GetPicoPowerCurrent() const;
    bool ShouldRollback() const;

    void GetCertifiedNodes(
                std::map<unsigned int, std::string>& mapNodesRet) const;
    void GetCertifiedNodes(std::vector<std::string>& vNodesRet) const;
    bool IsCertifiedNode(const std::string &sNodeAddress) const;

    AUTO_PTR<QPStaker> GetStaker(unsigned int nID) const;
    AUTO_PTR<QPStaker> GetStakerCopy(unsigned int nID) const;

    AUTO_PTR<QPStaker> GetNewestStaker() const;

    void GetEnabledStakers(QPMapStakers &vRet) const;
    void GetDisabledStakers(QPMapStakers &vRet) const;
    void GetQualifiedStakers(QPMapStakers &vRet) const;

    void GetStakers(QPMapStakers &mapRet) const;

    void CheckSynced();
    void Copy(const QPRegistry *const pother);

    bool DockInactiveBalances();

    bool NftIsAvailable(const unsigned int nID) const;
    bool NftIsAvailable(const unsigned int nID,
                        std::string &sCharKeyRet) const;
    bool NftIsAvailable(const std::string sCharKey,
                        unsigned int& nIDRet) const;
    unsigned int GetNftOwner(const unsigned int nID) const;
    bool GetNftIDForAlias(const std::string &sAlias,
                          unsigned int& nIDRet) const;
    bool GetNftNickForID(const unsigned int nID,
                         std::string &sNick) const;

    bool ReadSnapshot(CTxDB& txdb, int nHeight);
    bool WriteSnapshot(CTxDB& txdb, int nHeight) const;

    bool UpdateOnNewBlock(const CBlockIndex *const pindex,
                          int nSnapshotType,
                          bool fWriteLog=false,
                          bool fJustCheck=false);
    bool UpdateOnNewTime(unsigned int nTime,
                         const CBlockIndex *const pindex,
                         int nSnapshotType,
                         bool fWriteLog=false);

    void EnterReplayMode();
    void ExitReplayMode();

    bool GetIDForCurrentTime(CBlockIndex *pindex,
                             unsigned int &nIDRet,
                             unsigned int &nTimeRet);

    std::string GetQueueAsString() const;

    void GetQueueSummaryAsJSON(int nHeight,
                               json_spirit::Object &objRet) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nSerVersion = this->nVersion;

        READWRITE(nRound);
        READWRITE(nRoundSeed);
        READWRITE(mapStakers);
        READWRITE(mapBalances);
        READWRITE(mapLastClaim);
        READWRITE(mapActive);
        READWRITE(mapAliases);
        READWRITE(queue);
        READWRITE(queuePrev);
        READWRITE(bRecentBlocks);
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
        READWRITE(mapNftOwners);
        READWRITE(mapNftOwnerLookup);
    )
};

#endif  /* _QPREGISTRY_H_ */
