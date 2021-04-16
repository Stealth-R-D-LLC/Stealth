// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <iterator>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>

#include "QPRegistry.hpp"

#include "txdb-leveldb.h"

using namespace std;
using namespace json_spirit;

extern bool fDebugQPoS;
extern bool fDebugBlockCreation;
extern Value ValueFromAmount(int64_t amount);

QPRegistry::QPRegistry()
{
    SetNull();
}

QPRegistry::QPRegistry(const QPRegistry *const pother)
{
    Copy(pother);
}

void QPRegistry::SetNull()
{
    nVersion = QPRegistry::CURRENT_VERSION;
    nRound = 0;
    nRoundSeed = 0;
    mapStakers.clear();
    mapBalances.clear();
    mapLastClaim.clear();
    mapActive.clear();
    mapAliases.clear();
    bRecentBlocks.reset();
    nIDCounter = 0;
    nIDSlotPrev = 0;
    fCurrentBlockWasProduced = false;
    // can't penalize the very first staker ever to have a slot
    fPrevBlockWasProduced = true;
    nBlockHeight = 0;
    hashBlock = (fTestNet ? chainParams.hashGenesisBlockTestNet :
                            hashGenesisBlock);
    hashBlockLastSnapshot = hashBlock;
    hashLastBlockPrev1Queue = hashBlock;
    hashLastBlockPrev2Queue = hashBlock;
    hashLastBlockPrev3Queue = hashBlock;
    nHeightExitedReplay = 0;
    queue.SetNull();
    queuePrev.SetNull();
    powerRoundPrev.SetNull();
    powerRoundCurrent.SetNull();
    fShouldRollback = false;
}

unsigned int QPRegistry::GetRound() const
{
    return nRound;
}

uint32_t QPRegistry::GetRoundSeed() const
{
    return nRoundSeed;
}

unsigned int QPRegistry::Size() const
{
    return mapStakers.size();
}

bool QPRegistry::IsInReplayMode() const
{
    return !nHeightExitedReplay;
}

bool QPRegistry::OutOfReplayMode() const
{
    return (bool)nHeightExitedReplay;
}

int QPRegistry::GetBlockHeight() const
{
    return nBlockHeight;
}

uint256 QPRegistry::GetBlockHash() const
{
    return hashBlock;
}

uint256 QPRegistry::GetHashLastBlockPrev1Queue() const
{
    return hashLastBlockPrev1Queue;
}

uint256 QPRegistry::GetHashLastBlockPrev2Queue() const
{
    return hashLastBlockPrev2Queue;
}

uint256 QPRegistry::GetHashLastBlockPrev3Queue() const
{
    return hashLastBlockPrev3Queue;
}

int64_t QPRegistry::GetTotalEarned() const
{
    int64_t total = 0;
    QPRegistryIterator it;
    for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
    {
        total += it->second.GetTotalEarned();
    }
    return total;
}



unsigned int QPRegistry::GetNumberOf(bool (QPStaker::* f)() const) const
{
    unsigned int count = 0;
    QPRegistryIterator it;
    for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
    {
        if ((it->second.*f)())
        {
            count += 1;
        }
    }
    return count;
}

unsigned int QPRegistry::GetNumberProductive() const
{
    return GetNumberOf(&QPStaker::IsProductive);
}

unsigned int QPRegistry::GetNumberEnabled() const
{
    return GetNumberOf(&QPStaker::IsEnabled);
}

unsigned int QPRegistry::GetNumberDisabled() const
{
    return GetNumberOf(&QPStaker::IsDisabled);
}

unsigned int QPRegistry::GetNumberQualified() const
{
    return GetNumberOf(&QPStaker::IsQualified);
}

unsigned int QPRegistry::GetNumberDisqualified() const
{
    return GetNumberOf(&QPStaker::IsDisqualified);
}

bool QPRegistry::IsEnabledStaker(unsigned int nStakerID) const
{
    const QPStaker* pstaker = GetStaker(nStakerID);
    if (!pstaker)
    {
        return false;
    }
    return pstaker->IsEnabled();
}

bool QPRegistry::CanEnableStaker(unsigned int nStakerID, int nHeight) const
{
    const QPStaker* pstaker = GetStaker(nStakerID);
    if (!pstaker)
    {
        return false;
    }
    return pstaker->CanBeEnabled(nHeight);
}

bool QPRegistry::IsQualifiedStaker(unsigned int nStakerID) const
{
    const QPStaker* pstaker = GetStaker(nStakerID);
    if (!pstaker)
    {
        return false;
    }
    return pstaker->IsQualified();
}

bool QPRegistry::TimestampIsValid(unsigned int nStakerID,
                                  unsigned int nTime) const
{
    bool fIsValid = true;
    QPWindow window;
    if (!queue.GetWindowForID(nStakerID, window))
    {
        fIsValid = error("TimestampIsValid(): invalid staker ID");
    }
    else if (nTime < window.start)
    {
        fIsValid = error("TimestampIsValid(): timestamp too early for staker");
    }
    else if (nTime > window.end)
    {
        fIsValid = error("TimestampIsValid(): timestamp too late for staker");
    }
    else if (nTime > GetAdjustedTime())
    {
        fIsValid = error("TimestampIsValid(): qPoS block timestamp in future");
    }
    if (!fIsValid)
    {
        unsigned int slot;
        queue.GetSlotForID(nStakerID, slot);
        if (fDebugQPoS)
        {
            printf("TimestampIsValid(): staker: %u, staker_slot: %u\n"
                   "  staker_window: %u-%u, time: %u, round: %u, seed: %u\n"
                   "  Queue: %s\n",
                   nStakerID, slot, window.start, window.end, nTime,
                   nRound, nRoundSeed, queue.ToString().c_str());
        }
    }
    return fIsValid;
}

unsigned int QPRegistry::GetQueueMinTime() const
{
    return queue.GetMinTime();
}

unsigned int QPRegistry::GetCurrentSlot() const
{
    return queue.GetCurrentSlot();
}

unsigned int QPRegistry::GetCurrentSlotStart() const
{
    return queue.GetCurrentSlotStart();
}

unsigned int QPRegistry::GetCurrentSlotEnd() const
{
    return queue.GetCurrentSlotEnd();
}

bool QPRegistry::TimeIsInCurrentSlotWindow(unsigned int nTime) const
{
    return queue.TimeIsInCurrentSlotWindow(nTime);
}

unsigned int QPRegistry::GetCurrentID() const
{
    return queue.GetCurrentID();
}

unsigned int QPRegistry::GetCurrentQueueSize() const
{
    return queue.Size();
}

unsigned int QPRegistry::GetPreviousQueueSize() const
{
    return queuePrev.Size();
}

void QPRegistry::GetSlotsInfo(int64_t nTime,
                              unsigned int nSlotFirst,
                              const QPQueue& q,
                              vector<QPSlotInfo> &vRet) const
{
    vRet.clear();
    for (unsigned int slot = nSlotFirst; slot < q.Size(); ++slot)
    {
        vRet.push_back(QPSlotInfo(nTime, slot, q));
    }
}

void QPRegistry::GetCurrentSlotsInfo(int64_t nTime,
                                     unsigned int nSlotFirst,
                                     vector<QPSlotInfo> &vRet) const
{
    return GetSlotsInfo(nTime, nSlotFirst, queue, vRet);
}

void QPRegistry::GetPreviousSlotsInfo(int64_t nTime,
                                      unsigned int nSlotFirst,
                                      vector<QPSlotInfo> &vRet) const
{
    return GetSlotsInfo(nTime, nSlotFirst, queuePrev, vRet);
}


bool QPRegistry::CurrentBlockWasProduced() const
{
    return fCurrentBlockWasProduced;
}

unsigned int QPRegistry::GetRecentBlocksSize() const
{
    return bRecentBlocks.size();
}

int QPRegistry::GetRecentBlock(unsigned int n) const
{
    if (n >= bRecentBlocks.size())
    {
        return -1;
    }
    return bRecentBlocks[n];
}

unsigned int QPRegistry::GetCurrentIDCounter() const
{
    return nIDCounter;
}

unsigned int QPRegistry::GetNextIDCounter() const
{
    return nIDCounter + 1;
}

bool QPRegistry::GetBalanceForPubKey(const CPubKey &key,
                                     int64_t &nBalanceRet) const
{
    map<CPubKey, int64_t>::const_iterator it = mapBalances.find(key);
    if (it == mapBalances.end())
    {
        return error("GetBalanceForPubKey(): pubkey not found");
    }
    nBalanceRet = it->second;
    return true;
}

bool QPRegistry::PubKeyIsInLedger(const CPubKey &key) const
{
    return (mapBalances.count(key) != 0);
}

bool QPRegistry::PubKeyActiveCount(const CPubKey &key, int &nCountRet) const
{
    map<CPubKey, int>::const_iterator it = mapActive.find(key);
    if (it == mapActive.end())
    {
        return error("PubKeyActiveCount(): pubkey not found");
    }
    nCountRet = it->second;
    return true;
}

bool QPRegistry::PubKeyIsInactive(const CPubKey &key, bool &fInActiveRet) const
{
    map<CPubKey, int>::const_iterator it = mapActive.find(key);
    if (it == mapActive.end())
    {
        return error("PubKeyIsInactive(): pubkey not found");
    }
    fInActiveRet = (it->second <= 0);
    return true;
}

bool QPRegistry::CanClaim(const CPubKey &key,
                          int64_t nValue,
                          int64_t nClaimTime) const
{
    int64_t nTime = GetAdjustedTime();
    // need to specify time as an input for blockchain sync/validation
    if (nClaimTime > nTime)
    {
        return error("CanClaim(): can't predict future");
    }
    if (nClaimTime <= 0)
    {
        nClaimTime = nTime;
    }
    map<CPubKey, int64_t>::const_iterator it = mapBalances.find(key);
    if (it == mapBalances.end())
    {
        return error("CanClaim(): no such balance in ledger");
    }
    if (nValue > it->second)
    {
        return error("CanClaim(): claim exceeds balance");
    }
    it = mapLastClaim.find(key);
    if (it != mapLastClaim.end())
    {
        if ((!fTestNet) && (nClaimTime < (it->second + QP_MIN_SECS_PER_CLAIM)))
        {
            return error("CanClaim(): too soon to claim");
        }
    }
    return true;
}

bool QPRegistry::GetOwnerKey(unsigned int nStakerID, CPubKey &keyRet) const
{
    const QPStaker* pstaker = GetStaker(nStakerID);
    if (!pstaker)
    {
        return error("GetOwnerKey(): no such staker %u", nStakerID);
    }
    if (pstaker->IsDisqualified())
    {
        return error("GetOwnerKey(): staker is disqualified %u", nStakerID);
    }
    keyRet = pstaker->pubkeyOwner;
    return true;
}


bool QPRegistry::GetManagerKey(unsigned int nStakerID, CPubKey &keyRet) const
{
    const QPStaker* pstaker = GetStaker(nStakerID);
    if (!pstaker)
    {
        return error("GetManagerKey(): no such staker %u", nStakerID);
    }
    if (pstaker->IsDisqualified())
    {
        return error("GetManagerKey(): staker is disqualified %u", nStakerID);
    }
    keyRet = pstaker->pubkeyManager;
    return true;
}

bool QPRegistry::GetDelegateKey(unsigned int nStakerID, CPubKey &keyRet) const
{
    const QPStaker* pstaker = GetStaker(nStakerID);
    if (!pstaker)
    {
        return error("GetDelegateKey(): no such staker %u", nStakerID);
    }
    if (pstaker->IsDisqualified())
    {
        return error("GetDelegateKey(): staker is disqualified %u", nStakerID);
    }
    keyRet = pstaker->pubkeyDelegate;
    return true;
}

bool QPRegistry::GetControllerKey(unsigned int nStakerID,
                                  CPubKey &keyRet) const
{
    const QPStaker* pstaker = GetStaker(nStakerID);
    if (!pstaker)
    {
        return error("GetControllerKey(): no such staker %u", nStakerID);
    }
    if (pstaker->IsDisqualified())
    {
        return error("GetControllerKey(): staker is disqualified %u",
                     nStakerID);
    }
    keyRet = pstaker->pubkeyController;
    return true;
}

bool QPRegistry::GetStakerWeight(unsigned int nStakerID,
                           unsigned int &nWeightRet) const
{
    const QPStaker* pstaker = GetStaker(nStakerID);
    if (!pstaker)
    {
        return error("GetKeyForID() : no staker for ID %d", nStakerID);
    }
    if (pstaker->IsDisqualified())
    {
        return error("GetStakerWeight(): staker is disqualified %u",
                     nStakerID);
    }
    // sanity assertion, should never fail
    assert ((nIDCounter + 1) > nStakerID);
    // higher number equates to more seniority
    unsigned int nSeniority = (nIDCounter + 1) - nStakerID;
    nWeightRet = pstaker->GetWeight(nSeniority);
    return true;
}

void QPRegistry::AsJSON(Object &objRet) const
{
    objRet.clear();
    Array aryQueue;
    for (unsigned int slot = 0; slot < queue.Size(); ++slot)
    {
        unsigned int nID;
        queue.GetIDForSlot(slot, nID);
        QPWindow w;
        queue.GetWindowForSlot(slot, w);
        Object objSlot;
        objSlot.push_back(Pair("id", static_cast<int64_t>(nID)));
        objSlot.push_back(Pair("start_time", static_cast<int64_t>(w.start)));
        objSlot.push_back(Pair("end_time", static_cast<int64_t>(w.end)));

        QPSlotStatus status;
        queue.GetStatusForSlot(slot, status);
        string strStatus(GetSlotStatusType(status));
        objSlot.push_back(Pair("status", strStatus));
        aryQueue.push_back(objSlot);
    }
    unsigned int nCurrentSlot = queue.GetCurrentSlot();

    vector<pair<unsigned int, const QPStaker*> > vIDs;
    QPRegistryIterator mit;
    for (mit = mapStakers.begin(); mit != mapStakers.end(); ++mit)
    {
        if (mit->second.IsQualified())
        {
            vIDs.push_back(make_pair(mit->first, &(mit->second)));
        }
    }

    sort(vIDs.begin(), vIDs.end());

    Object objStakers;
    vector<pair<unsigned int, const QPStaker*> >::const_iterator vit;
    for (vit = vIDs.begin(); vit != vIDs.end(); ++vit)
    {
        Object objStkr;
        unsigned int nSeniority = (nIDCounter + 1) - vit->first;
        vit->second->AsJSON(vit->first, nSeniority, objStkr);
        string sID = to_string(vit->first);
        objStakers.push_back(Pair(sID, objStkr));
    }

    map<CPubKey, int64_t>::const_iterator kit;
    Object objBalances;
    for (kit = mapBalances.begin(); kit != mapBalances.end(); ++kit)
    {
        CPubKey key = kit->first;
        int64_t amt = kit->second;

        Object objBal;

        objBal.push_back(Pair("balance", ValueFromAmount(amt)));

        map<CPubKey, int64_t>::const_iterator it;
        it = mapLastClaim.find(key);
        if (it != mapLastClaim.end())
        {
            objBal.push_back(Pair("last_claim", it->second));
        }

        map<CPubKey, int>::const_iterator jt;
        jt = mapActive.find(key);
        if (jt != mapActive.end())
        {
            objBal.push_back(Pair("active_count",
                                  static_cast<int64_t>(jt->second)));
        }
        objBalances.push_back(Pair(HexStr(key.Raw()), objBal));
    }

    objRet.push_back(Pair("version", nVersion));
    objRet.push_back(Pair("round", static_cast<int64_t>(nRound)));
    objRet.push_back(Pair("round_seed", static_cast<int64_t>(nRoundSeed)));
    objRet.push_back(Pair("block_height", nBlockHeight));
    objRet.push_back(Pair("block_hash", hashBlock.GetHex()));
    objRet.push_back(Pair("last_block_hash_prev_1_queue",
                           hashLastBlockPrev1Queue.GetHex()));
    objRet.push_back(Pair("last_block_hash_prev_2_queue",
                           hashLastBlockPrev2Queue.GetHex()));
    objRet.push_back(Pair("last_block_hash_prev_3_queue",
                           hashLastBlockPrev3Queue.GetHex()));
    objRet.push_back(Pair("in_replay", IsInReplayMode()));
    objRet.push_back(Pair("should_roll_back", fShouldRollback));
    objRet.push_back(Pair("counter_next",
                          static_cast<int64_t>(nIDCounter + 1)));
    objRet.push_back(Pair("recent_blocks",
                          BitsetAsHex(bRecentBlocks)));
    objRet.push_back(Pair("current_block_was_produced",
                          fCurrentBlockWasProduced));
    objRet.push_back(Pair("prev_block_was_produced",
                          fPrevBlockWasProduced));
    objRet.push_back(Pair("current_slot",
                          static_cast<int64_t>(nCurrentSlot)));
    objRet.push_back(Pair("pico_power", GetPicoPower()));
    objRet.push_back(Pair("prev_pico_power", GetPicoPowerPrev()));
    objRet.push_back(Pair("current_pico_power", GetPicoPowerCurrent()));
    objRet.push_back(Pair("queue", aryQueue));
    objRet.push_back(Pair("queue_blocks", HexStr(queue.GetBlockStats())));
    objRet.push_back(Pair("stakers", objStakers));
    objRet.push_back(Pair("balances", objBalances));
}

void QPRegistry::GetStakerAsJSON(unsigned int nID,
                                 Object &objRet,
                                 bool fWithRecentBlocks) const
{
    const QPStaker* pstaker = GetStaker(nID);
    if (pstaker)
    {
        unsigned int nSeniority = pstaker->IsDisqualified() ?
                                              0 : (nIDCounter + 1) - nID;
        unsigned int nNftID = 0;
        if (mapNftOwners.count(nID))
        {
            nNftID = mapNftOwners.at(nID);
        }
        pstaker->AsJSON(nID, nSeniority, objRet, fWithRecentBlocks, nNftID);
    }
    else
    {
        objRet.clear();
    }
}

void QPRegistry::CheckSynced()
{
    if (IsInReplayMode())
    {
        int nTime = GetAdjustedTime();
        if (HasEnoughPower() && queue.TimeIsInCurrentSlotWindow(nTime))
        {
            printf("QPRegistry::CheckSynced(): exiting replay mode\n");
            ExitReplayMode();
        }
    }
}

void QPRegistry::Copy(const QPRegistry *const pother)
{
    nVersion = pother->nVersion;
    nRound = pother->nRound;
    nRoundSeed = pother->nRoundSeed;
    mapStakers = pother->mapStakers;
    mapBalances = pother->mapBalances;
    mapLastClaim = pother->mapLastClaim;
    mapActive = pother->mapActive;
    mapAliases = pother->mapAliases;
    queue = pother->queue;
    queuePrev = pother->queuePrev;
    bRecentBlocks = pother->bRecentBlocks;
    nIDCounter = pother->nIDCounter;
    nIDSlotPrev = pother->nIDSlotPrev;
    fCurrentBlockWasProduced = pother->fCurrentBlockWasProduced;
    fPrevBlockWasProduced = pother->fPrevBlockWasProduced;
    nBlockHeight = pother->nBlockHeight;
    hashBlock = pother->hashBlock;
    hashBlockLastSnapshot = pother->hashBlockLastSnapshot;
    hashLastBlockPrev1Queue = pother->hashLastBlockPrev1Queue;
    hashLastBlockPrev2Queue = pother->hashLastBlockPrev2Queue;
    hashLastBlockPrev3Queue = pother->hashLastBlockPrev3Queue;
    powerRoundPrev = pother->powerRoundPrev;
    powerRoundCurrent = pother->powerRoundCurrent;
    mapNftOwners = pother->mapNftOwners;
    mapNftOwnerLookup = pother->mapNftOwnerLookup;

    nHeightExitedReplay = pother->nHeightExitedReplay;
    fShouldRollback = pother->fShouldRollback;
}

void QPRegistry::ActivatePubKey(const CPubKey &key,
                                const CBlockIndex* pindex)
{
    if (!mapActive.count(key))
    {
        mapActive[key] = 0;
    }
    if (mapActive[key] == 0)
    {
        mapBalances[key] = 0;
        mapLastClaim[key] = pindex->nTime;
    }
    mapActive[key] += 1;
}

bool QPRegistry::DeactivatePubKey(const CPubKey &key)
{
    map<CPubKey, int>::iterator it = mapActive.find(key);
    if (it == mapActive.end())
    {
        return error("DeactivatePubKey(): pubkey not found");
    }
    if (it->second < 1)
    {
        return error("DeactivatePubKey(): Can't deactivate inactive key");
    }
    it->second -= 1;
    return true;
}

bool QPRegistry::SetStakerAlias(unsigned int nID, const string &sAlias)
{
    string sKey;
    if (!AliasIsAvailable(sAlias, sKey))
    {
        return false;
    }
    QPStaker* pstaker = GetStakerForID(nID);
    if (!pstaker)
    {
        return error("SetStakerAlias(): no staker with ID %u", nID);
    }
    if (pstaker->IsDisqualified())
    {
        return error("SetStakerAlias(): staker %u is disqualified", nID);
    }
    if (!pstaker->SetAlias(sAlias))
    {
        return error("SetStakerAlias(): can't set to %s", sAlias.c_str());
    }
    mapAliases[sKey] = make_pair(nID, sAlias);
    return true;
}

bool QPRegistry::NftIsAvailable(const unsigned int nID) const
{
    return (mapNfts.count(nID) && !mapNftOwners.count(nID));
}

bool QPRegistry::NftIsAvailable(const unsigned int nID,
                                string &sCharKeyRet) const
{
    sCharKeyRet.clear();
    if (mapNfts.count(nID) && !mapNftOwners.count(nID))
    {
        sCharKeyRet = mapNfts[nID].strCharKey;
    }
    return (bool)sCharKeyRet.size();
}

bool QPRegistry::NftIsAvailable(const string sCharKey,
                                unsigned int& nIDRet) const
{
    nIDRet = 0;
    // FIXME: this can be removed after testing
    if (sCharKey != ToLowercaseSafe(sCharKey))
    {
        return error("NftIsAvailable(): TSNH alias is not lowercase");
    }
    if (mapNftLookup.count(sCharKey))
    {
        nIDRet = mapNftLookup[sCharKey];
    }
    if (mapNftOwnerLookup.count(nIDRet))
    {
        nIDRet = 0;
    }
    return (bool)nIDRet;
}

unsigned int QPRegistry::GetNftOwner(const unsigned int nID) const
{
    unsigned int nStakerID = 0;
    if (mapNftOwnerLookup.count(nID))
    {
        nStakerID = mapNftOwners.at(nID);
    }
    return nStakerID;
}

bool QPRegistry::GetNftIDForAlias(const string &sAlias,
                                  unsigned int& nIDRet) const
{
    string sKey = ToLowercaseSafe(sAlias);
    if (mapNftLookup.count(sKey))
    {
        nIDRet = mapNftLookup[sKey];
    }
    else
    {
        nIDRet = 0;
    }
    return (bool)nIDRet;
}

bool QPRegistry::GetNftNickForID(const unsigned int nID,
                                  string &sNickRet) const
{
    if (mapNfts.count(nID))
    {
        sNickRet = mapNfts[nID].strNickname;
    }
    else
    {
        sNickRet = "";
    }
    return (bool)sNickRet.size();
}

const QPStaker* QPRegistry::GetStaker(unsigned int nID) const
{
    if ((nID > 0) && (nID <= nIDCounter))
    {
        QPRegistryIterator iter = mapStakers.find(nID);
        if (iter != mapStakers.end())
        {
            return &(iter->second);
        }
    }
    return NULL;
}

const QPStaker* QPRegistry::GetNewestStaker() const
{
    return GetStaker(nIDCounter);
}

void QPRegistry::GetEnabledStakers(vector<const QPStaker*> &vRet) const
{
    vRet.clear();
    QPRegistryIterator it;
    for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
    {
        if (it->second.IsEnabled())
        {
            vRet.push_back(&(it->second));
        }
    }
}


void QPRegistry::GetDisabledStakers(vector<const QPStaker*> &vRet) const
{
    vRet.clear();
    QPRegistryIterator it;
    for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
    {
        if (it->second.IsDisabled())
        {
            vRet.push_back(&(it->second));
        }
    }
}

void QPRegistry::GetStakers(vector<const QPStaker*> &vRet) const
{
    vRet.clear();
    for (unsigned int n = 1; n <= nIDCounter; ++n)
    {
        QPRegistryIterator it = mapStakers.find(n);
        if (it == mapStakers.end())
        {
            // this should never happen: no such ID
            printf("GetStakers(): TSNH No such ID %u\n", n);
        }
        else
        {
            vRet.push_back(&(it->second));
        }
    }
}

void QPRegistry::GetStakers(QPMapPStakers &mapRet) const
{
    mapRet.clear();
    QPRegistryIterator it;
    for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
    {
        mapRet[it->first] = &(it->second);
    }
}

unsigned int QPRegistry::GetIDForCurrentSlot() const
{
    if (queue.Size() == 0)
    {
        return 0;
    }
    return queue.GetCurrentID();
}

uint64_t QPRegistry::GetPicoPower() const
{
    // will not overflow for a long time
    //    e.g. if staker net blocks is 1 billion (633 yr with just 4 stakers):
    //       4 * sqrt(1e9) = 126491.10
    //       126491.10 * 1e12 = 1.265e+17
    //       (2**64 - 1) / 1.265e+17 = 145.8 > 1
    //    eg. 500 yr with 30 stakers: 59.95 > 1
    uint64_t w = static_cast<uint64_t>(powerRoundPrev.GetWeight() +
                                       powerRoundCurrent.GetWeight());
    uint64_t t = static_cast<uint64_t>(powerRoundPrev.GetTotalWeight() +
                                       powerRoundCurrent.GetTotalWeight());
    if (t == 0)
    {
        return 0;
    }
    return (w * TRIL) / t;
}

uint64_t QPRegistry::GetPicoPowerPrev() const
{
    uint64_t w = static_cast<uint64_t>(powerRoundPrev.GetWeight());
    uint64_t t = static_cast<uint64_t>(powerRoundPrev.GetTotalWeight());
    if (t == 0)
    {
        return 0;
    }
    return (w * TRIL) / t;
}

uint64_t QPRegistry::GetPicoPowerCurrent() const
{
    uint64_t w = static_cast<uint64_t>(powerRoundCurrent.GetWeight());
    uint64_t t = static_cast<uint64_t>(powerRoundCurrent.GetTotalWeight());
    if (t == 0)
    {
        return 0;
    }
    return (w * TRIL) / t;
}

bool QPRegistry::HasEnoughPower() const
{
    return (powerRoundPrev.IsEmpty() ||
            (GetPicoPower() >= QP_MIN_PICO_POWER));
}

bool QPRegistry::ShouldRollback() const
{
    return fShouldRollback;
}

void QPRegistry::GetCertifiedNodes(vector<string> &vNodesRet) const
{
    QPRegistryIterator it;
    for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
    {
        if (it->second.IsDisqualified())
        {
            continue;
        }
        string sValue;
        if (it->second.GetMeta(META_KEY_CERTIFIED_NODE, sValue))
        {
            vNodesRet.push_back(sValue);
        }
    }
}

bool QPRegistry::IsCertifiedNode(const string &sNodeAddress) const
{
    QPRegistryIterator it;
    for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
    {
        if (it->second.IsDisqualified())
        {
            continue;
        }
        string sValue;
        if (it->second.GetMeta(META_KEY_CERTIFIED_NODE, sValue))
        {
            if (sValue == sNodeAddress)
            {
                return true;
            }
        }
    }
    return false;
}

// returns true if the current block should be produced
// this is the key procedure for keeping the registry
//   synced when not in replay
bool QPRegistry::GetIDForCurrentTime(CBlockIndex *pindex,
                                     unsigned int &nIDRet,
                                     unsigned int &nTimeRet)
{
    nTimeRet = static_cast<unsigned int>(GetAdjustedTime());

    // queue has gotten ahead some how
    if (nTimeRet < queue.GetCurrentSlotStart())
    {
        // this should never happen
        printf("GetIDForCurrentTime(): TSNH queue has gotten ahead\n");
        nIDRet = queue.GetCurrentID();
        return false;
    }

    if (nTimeRet <= queue.GetMaxTime())
    {
        unsigned int nSlot;
        if (queue.GetSlotForTime(nTimeRet, nSlot))
        {
            nIDRet = queue.GetCurrentID();
            unsigned int nCurrentSlot = queue.GetCurrentSlot();
            // return the current ID if the queue is up to date
            if (nSlot == nCurrentSlot)
            {
                return !fCurrentBlockWasProduced;
            }
            else if (nSlot < nCurrentSlot)
            {
                printf("GetIDForCurrentTime(): queue is ahead of time\n");
                return false;
            }
        }
        else
        {
            // this should never happen
            printf("GetIDForCurrentTime(): TSNH queue state is inconsistent\n");
            nIDRet = 0;
            return false;
        }
    }

    // Apparently there is a need to advance the registry,
    // which only happens when blocks are connected.
    // Here, advance a tempRegistry as if it were the real registry
    // to determine which ID should sign the next block.
    // Using a tempRegistry allows for reorganization in the case
    // of missing blocks, where they suddenly arrive and are on a chain
    // with greater trust.
    QPRegistry *pregistryTemp = new QPRegistry(this);
    if (fDebugBlockCreation)
    {
        printf("GetIDForCurrentTime(): advancing temp registry at %d\n"
               "   this height=%d, current_slot: %u\n"
               "   temp height=%d, current_slot: %u\n",
               pindex->nHeight,
               this->nBlockHeight,
               this->GetCurrentSlot(),
               pregistryTemp->GetBlockHeight(),
               pregistryTemp->GetCurrentSlot());
    }
    // don't take snapshots of the temp registry
    pregistryTemp->UpdateOnNewTime(nTimeRet, pindex, false);
    nIDRet = pregistryTemp->GetIDForCurrentSlot();
    delete pregistryTemp;
    return true;
}

unsigned int QPRegistry::GetIDForPrevSlot() const
{
    return nIDSlotPrev;
}

bool QPRegistry::GetPrevRecentBlocksMissedMax(unsigned int nID,
                                              uint32_t &nMaxRet) const
{
    static const uint32_t MAX_STDDEVS = 8;
    uint32_t n = 0;
    uint32_t total = 0;
    vector<uint32_t> vMissed;
    QPRegistryIterator it;
    for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
    {
        if (it->first == nID)
        {
            continue;
        }
        QPStaker staker = it->second;
        if (staker.IsEnabled())
        {
            n += 1;
            uint32_t m = staker.GetPrevRecentBlocksMissed();
            vMissed.push_back(m);
            total += m;
        }
    }
    if (n == 0)
    {
        return error("GetPrevRecentBlocksMissedMax(): 0 enabled stakers");
    }
    uint32_t mean = total / n;
    uint32_t sum_of_delta_sq = 0;
    vector<uint32_t>::const_iterator jt;
    for (jt = vMissed.begin(); jt != vMissed.end(); ++jt)
    {
        uint32_t m = *jt;
        uint32_t delta = (mean > m) ? (mean - m) : (m - mean);
        sum_of_delta_sq += delta * delta;
    }
    uint32_t stdev = uisqrt(sum_of_delta_sq / n);
    nMaxRet = mean + (stdev * MAX_STDDEVS);
    return true;
}

bool QPRegistry::AliasIsAvailable(const string &sAlias,
                                  string &sKeyRet) const
{
    if (!AliasIsValid(sAlias))
    {
        return false;
    }
    sKeyRet = ToLowercaseSafe(sAlias);
    return (mapAliases.count(sKeyRet) == 0);
}

bool QPRegistry::GetIDForAlias(const string &sAlias,
                               unsigned int &nIDRet) const
{
    string sKey = ToLowercaseSafe(sAlias);
    map<string, pair<unsigned int, string> >::const_iterator it;
    it = mapAliases.find(sKey);
    if (it == mapAliases.end())
    {
        return false;
    }
    nIDRet = it->second.first;
    return true;
}

bool QPRegistry::GetAliasForID(unsigned int nID,
                               string &sAliasRet) const
{
    const QPStaker* pstaker = GetStaker(nID);
    if (!pstaker)
    {
        return error("GetAliasForID(): no such staker");
    }
    sAliasRet = pstaker->GetAlias();
    return true;
}

string QPRegistry::GetAliasForID(unsigned int nID)
{
    if (nID == 0)
    {
        return STRING_NO_ALIAS;
    }
    const QPStaker* pstaker = GetStaker(nID);
    if (!pstaker)
    {
        printf("GetAliasForID(): ERROR: no such staker");
        return STRING_NO_ALIAS;
    }
    return pstaker->GetAlias();
}


QPStaker* QPRegistry::GetStakerForID(unsigned int nID)
{
    if ((nID > 0) && (nID <= nIDCounter))
    {
        QPMapStakers::iterator iter = mapStakers.find(nID);
        if (iter != mapStakers.end())
        {
            return &(iter->second);
        }
    }
    return NULL;
}

QPStaker* QPRegistry::GetStakerForAlias(const string &sAlias)
{
    unsigned int nID;
    if (!GetIDForAlias(sAlias, nID))
    {
        return NULL;
    }
    QPStaker* pstaker = GetStakerForID(nID);
    if (!pstaker)
    {
        printf("GetStakerForAlias(): alias and ID mismatch\n");
        return NULL;
    }
    return pstaker;
}


// this will only be called for a block that is being added to the end of the
// chain so that the Registry state is synchronous with the block
// same goes for StakerMissedBlock
bool QPRegistry::StakerProducedBlock(const CBlockIndex *pindex,
                                     int64_t nReward)
{
    unsigned int nID = pindex->nStakerID;
    QPStaker* pstaker = GetStakerForID(nID);
    if (!pstaker)
    {
        return error("StakerProducedBlock(): unknown ID %d", nID);
    }
    if (pstaker->IsDisqualified())
    {
        return error("StakerProducedBlock(): TSNH staker %d disqualified", nID);
    }
    pstaker = &mapStakers[nID];
    unsigned int nSeniority = (nIDCounter + 1) - nID;
    powerRoundCurrent.PushBack(nID, pstaker->GetWeight(nSeniority), true);
    int64_t nOwnerReward, nDelegateReward;
    pstaker->ProducedBlock(pindex->phashBlock,
                           nReward,
                           fPrevBlockWasProduced,
                           nOwnerReward,
                           nDelegateReward);
    if (fTestNet && (GetFork(nBestHeight) < XST_FORKMISSFIX))
    {
        mapBalances[pstaker->pubkeyOwner] += nOwnerReward;
    }
    else
    {
        mapBalances[pstaker->pubkeyManager] += nOwnerReward;
    }
    if (nDelegateReward > 0)
    {
        mapBalances[pstaker->pubkeyDelegate] += nDelegateReward;
    }
    DisqualifyStakerIfNecessary(nID, pstaker);
    bRecentBlocks <<= 1;
    bRecentBlocks[0] = true;
    fCurrentBlockWasProduced = true;
    fPrevBlockWasProduced = true;
    return true;
}

bool QPRegistry::StakerMissedBlock(unsigned int nID, int nHeight)
{
    QPStaker* pstaker = GetStakerForID(nID);
    if (!pstaker)
    {
        return error("StakerMissedBlock(): unknown ID %d", nID);
    }
    if (pstaker->IsDisqualified())
    {
        return error("StakerMissedBlock(): TSNH staker %d disqualified", nID);
    }
    unsigned int nSeniority = (nIDCounter + 1) - nID;
    powerRoundCurrent.PushBack(nID, pstaker->GetWeight(nSeniority), false);
    pstaker->MissedBlock(fPrevBlockWasProduced);
    if (OutOfReplayMode() && fDebugBlockCreation)
    {
        printf("StakerMissedBlock(): staker=%s, round=%d, seed=%u, slot=%d, "
               "window=%d-%d, picopower=%" PRIu64 ", registryheight=%d\n",
               GetAliasForID(nID).c_str(),
               nRound,
               nRoundSeed,
               queue.GetCurrentSlot(),
               queue.GetCurrentSlotStart(),
               queue.GetCurrentSlotEnd(),
               GetPicoPower(),
               nBlockHeight);
    }
    DisqualifyStakerIfNecessary(nID, pstaker);
    DisableStakerIfNecessary(nID, pstaker, nHeight);
    bRecentBlocks <<= 1;
    fPrevBlockWasProduced = false;
    return true;
}

bool QPRegistry::DisableStaker(unsigned int nID, int nHeight)
{
    QPStaker* pstaker = GetStakerForID(nID);
    if (!pstaker)
    {
        return error("DisableStaker(): unknown ID");
    }
    if (pstaker->IsDisabled())
    {
        return error("DisableStaker(): staker already disabled");
    }
    pstaker->Disable(nHeight);
    return true;
}

bool QPRegistry::DisableStakerIfNecessary(unsigned int nID,
                                          const QPStaker* pstaker,
                                          int nHeight)
{
    bool fResult = true;
    // auto disable kicks in with feeless transactions
    if (GetFork(nBestHeight) < XST_FORKFEELESS)
    {
        return true;
    }
    if (pstaker->ShouldBeDisabled(nHeight))
    {
        // want at least 3 enabled stakers in times of disaster
        if (GetNumberEnabled() > 3)
        {
            fResult = DisableStaker(nID, nHeight);
        }
    }
    return fResult;
}

bool QPRegistry::DisqualifyStaker(unsigned int nID)
{
    QPStaker* pstaker = GetStakerForID(nID);
    if (!pstaker)
    {
        return error("DisqualifyStaker(): unknown ID");
    }
    if (pstaker->IsDisqualified())
    {
        return error("DisqualifyStaker(): staker already disqualified");
    }
    pstaker->Disqualify();
    return true;
}

bool QPRegistry::DisqualifyStakerIfNecessary(unsigned int nID,
                                             const QPStaker* pstaker)
{
    bool fResult = true;
    if (fTestNet)
    {
        return true;
    }
    uint32_t nPrevRecentBlocksMissedMax;
    if (!GetPrevRecentBlocksMissedMax(nID, nPrevRecentBlocksMissedMax))
    {
        fResult = false;
    }
    if (pstaker->ShouldBeDisqualified(nPrevRecentBlocksMissedMax))
    {
        fResult = DisqualifyStaker(nID);
    }
    return fResult;
}

bool QPRegistry::NewQueue(unsigned int nTime0, const uint256& prevHash)
{
    vector<unsigned int> vIDs;
    QPRegistryIterator iter;
    for (iter = mapStakers.begin(); iter != mapStakers.end(); ++iter)
    {
        if (iter->second.IsEnabled())
        {
            vIDs.push_back(iter->first);
        }
    }
    if (vIDs.size() < 1)
    {
        return error("NewQueue(): No qualified stakers");
    }
    sort(vIDs.begin(), vIDs.end());
    uint256 hash(prevHash);
    if (fTestNet)
    {
        hash = SerializeHash(hash);
    }
    else
    {
        for (int i = 0; i < QP_ROUNDS; ++i)
        {
           hash = Hash9(hash.begin(), hash.end());
        }
    }
    valtype vch;
    for (unsigned int i = 0; i < 4; ++i)
    {
        vch.push_back(*(hash.begin() + i));
    }
    uint32_t seed = static_cast<uint32_t>(vchnum(vch).GetValue());
    boost::mt19937 gen(seed);
    boost::uniform_int<> dist;
    QPShuffler shuffler(gen, dist);

    // cannot count on gcc STL to have standardized random_shuffle
    typedef vector<uint32_t>::iterator vi_type;
    vi_type first = vIDs.begin();
    vi_type last = vIDs.end();
    if (first == last)
    {
        return error("NewQueue(): TSNH No qualified stakers");
    }
    for (vi_type i = first + 1; i != last; ++i)
    {
        vi_type j = first + shuffler((i - first) + 1);
        if (i != j)
        {
            iter_swap(i, j);
        }
    }

    queuePrev = queue;
    queue = QPQueue(nTime0, vIDs);
    nRound += 1;
    nRoundSeed = seed;
    powerRoundPrev.Copy(powerRoundCurrent);
    powerRoundCurrent.SetNull();
    hashLastBlockPrev3Queue = hashLastBlockPrev2Queue;
    hashLastBlockPrev2Queue = hashLastBlockPrev1Queue;
    hashLastBlockPrev1Queue = prevHash;
    return true;
}

unsigned int QPRegistry::IncrementID()
{
    nIDCounter += 1;
    return nIDCounter;
}


bool QPRegistry::DockInactiveKeys(int64_t nMoneySupply)
{
    int64_t nDockValue = nMoneySupply / DOCK_INACTIVE_FRACTION;
    bool fResult = true;
    map<CPubKey, int>::const_iterator it;
    for (it = mapActive.begin(); it != mapActive.end(); ++it)
    {
        if (it->second < 1)
        {
            map<CPubKey, int64_t>::iterator jt = mapBalances.find(it->first);
            if (jt == mapBalances.end())
            {
                fResult = error("DockInactivKeys(): %s missing in balances",
                                HexStr(it->first.Raw()).c_str());
            }
            else
            {
                jt->second -= nDockValue;
            }
        }
    }
    return fResult;
}

void QPRegistry::PurgeLowBalances(int64_t nMoneySupply)
{
    int64_t nDockValue = nMoneySupply / DOCK_INACTIVE_FRACTION;
    vector<CPubKey> vPurge;
    map<CPubKey, int64_t>::const_iterator it;
    for (it = mapBalances.begin(); it != mapBalances.end(); ++it)
    {
        if ((it->second < nDockValue) && (!mapActive.count(it->first)))
        {
            vPurge.push_back(it->first);
        }
    }
    vector<CPubKey>::const_iterator jt;
    for (jt = vPurge.begin(); jt != vPurge.end(); ++jt)
    {
        mapBalances.erase(*jt);
        mapActive.erase(*jt);
    }
}


bool QPRegistry::UpdateOnNewTime(unsigned int nTime,
                                 const CBlockIndex *const pindex,
                                 int nSnapshotType,
                                 bool fWriteLog)
{
    static const unsigned int N = BLOCKS_PER_SNAPSHOT * RECENT_SNAPSHOTS;
    // blocks per sparse snapshot
    static const unsigned int M = BLOCKS_PER_SNAPSHOT *
                                  PERMANENT_SNAPSHOT_RATIO;

    int nHeight = pindex->nHeight;

    bool fWriteSnapshot = true;
    bool fEraseSnapshot = false;
    if ((nSnapshotType == QPRegistry::NO_SNAPS) ||
        (GetFork(nHeight) < XST_FORKPURCHASE))
    {
        fWriteSnapshot = false;
    }
    else if (nSnapshotType == QPRegistry::SPARSE_SNAPS)
    {
        fWriteSnapshot = ((nHeight % M) == 0);
        fEraseSnapshot = !fWriteSnapshot;
    }
    else
    {
        // all snaps
        fWriteSnapshot = ((nHeight % BLOCKS_PER_SNAPSHOT) == 0);
    }

    uint256 hash = *(pindex->phashBlock);

    fWriteSnapshot = fWriteSnapshot && (hash != hashBlockLastSnapshot);

    CTxDB txdb;
    fEraseSnapshot = fEraseSnapshot && txdb.RegistrySnapshotIsViable(nHeight);

    // take snapshot here because the registry should be fully caught up
    // with the best (previous) block
    if (fWriteSnapshot)
    {
        hashBlockLastSnapshot = hash;
        txdb.WriteRegistrySnapshot(nHeight, *this);

        unsigned int nStakerSlot = 0;
        queue.GetSlotForID(pindex->nStakerID, nStakerSlot);

        printf("UpdateOnNewTime(): writing snapshot at %d\n"
               "   hash=%s\n"
               "   height=%d, time=%" PRId64 ", "
                  "staker_id=%d, staker_slot=%d\n"
               "   round=%d, seed=%u, window=%d-%d, picopower=%" PRIu64 "\n"
               "   %s\n",
               nHeight,
               hash.ToString().c_str(),
               nHeight,
               pindex->GetBlockTime(),
               pindex->nStakerID,
               nStakerSlot,
               nRound,
               nRoundSeed,
               queue.GetCurrentSlotStart(),
               queue.GetCurrentSlotEnd(),
               GetPicoPower(),
               queue.ToString().c_str());

        int nHeightErase = nHeight - N;
        if (nHeightErase % M != 0)
        {
            txdb.EraseRegistrySnapshot(nHeightErase);
        }
    }
    else if (fEraseSnapshot)
    {
        txdb.EraseRegistrySnapshot(nHeight);
    }

    if (GetFork(nHeight + 1) >= XST_FORKQPOS)
    {
        unsigned int nNewQueues = 0;
        if (queue.IsEmpty())
        {
            // qPoS shall begin
            printf("UpdateOnNewTime(): Starting qPoS at block\n   %s\n",
                   hash.ToString().c_str());
            if (!NewQueue(pindex->nTime + 1, hash))
            {
                return error("UpdateOnNewTime(): New queue failed.");
            }
            nNewQueues += 1;
        }
        while (queue.GetCurrentSlotEnd() < nTime)
        {
            if (!fCurrentBlockWasProduced)
            {
                StakerMissedBlock(queue.GetCurrentID(), nHeight);
            }
            if (!queue.IncrementSlot())
            {
                DockInactiveKeys(pindex->nMoneySupply);
                PurgeLowBalances(pindex->nMoneySupply);
                if (!NewQueue(queue.GetMaxTime() + 1, hashBlock))
                {
                    return error("UpdateOnNewTime(): New queue failed.");
                }
                nNewQueues += 1;
            }
            fCurrentBlockWasProduced = false;
            if (HasEnoughPower())
            {
                if (fShouldRollback)
                {
                    fShouldRollback = false;
                }
            }
            else
            {
                if (!fShouldRollback)
                {
                    fShouldRollback = true;
                }
                if (OutOfReplayMode())
                {
                    ExitReplayMode();
                }
            }
        }
        if (nNewQueues > 0)
        {
            if (fWriteLog)
            {
                printf("UpdateOnNewTime(): %u new queues with %s\n",
                       nNewQueues, hashBlock.ToString().c_str());
                printf("   newest queue starts at %d\n", queue.GetMinTime());
            }
        }
    }

    return true;
}

bool QPRegistry::UpdateOnNewBlock(const CBlockIndex *const pindex,
                                  int nSnapshotType,
                                  bool fWriteLog)
{
    const CBlockIndex *pindexPrev = (pindex->pprev ? pindex->pprev : pindex);
    // Q: Why do we update on new time before updating the registry
    //    for the new block?
    // A: Because we can't advance the queue until we have an event (block)
    //    that says to what time we should advance the queue.
    if (!UpdateOnNewTime(pindex->nTime, pindexPrev, nSnapshotType, fWriteLog))
    {
        return error("UpdateOnNewBlock(): could not update on new time for %s",
                     pindex->phashBlock->ToString().c_str());
    }
    if (GetFork(pindex->nHeight) >= XST_FORKPURCHASE)
    {
        if (!pindex->vDeets.empty())
        {
            if (fWriteLog)
            {
                printf("UpdateOnNewBlock(): apply ops for block %d\n",
                       pindex->nHeight);
            }
            ApplyOps(pindex);
        }
    }
    if (GetFork(pindex->nHeight) >= XST_FORKQPOS)
    {
        unsigned int nStakerSlot = 0;
        if (!queue.GetSlotForID(pindex->nStakerID, nStakerSlot))
        {
            return error("UpdateOnNewBlock(): no such staker ID %u",
                         pindex->nStakerID);
        }

        if (fWriteLog)
        {
            printf("UpdateOnNewBlock(): hash=%s\n"
                   "   height=%d, time=%" PRId64 ", "
                   "staker_id=%d, staker_slot=%d\n"
                   "   round=%d, seed=%u, window=%d-%d, picopower=%" PRIu64 "\n"
                   "   %s\n",
                   pindex->phashBlock->ToString().c_str(),
                   pindex->nHeight,
                   pindex->GetBlockTime(),
                   pindex->nStakerID,
                   nStakerSlot,
                   nRound,
                   nRoundSeed,
                   queue.GetCurrentSlotStart(),
                   queue.GetCurrentSlotEnd(),
                   GetPicoPower(),
                   queue.ToString().c_str());
        }

        if (nStakerSlot != queue.GetCurrentSlot())
        {
            return error("UpdateOnNewBlock(): staker slot (%d) "
                         "is not current slot (%d)",
                         nStakerSlot,
                         queue.GetCurrentSlot());
        }

        if (fCurrentBlockWasProduced && OutOfReplayMode())
        {
            return error("UpdateOnNewBlock(): "
                         "block already produced for this slot");
        }
        int64_t nReward = GetQPoSReward(pindex->pprev);
        if (!StakerProducedBlock(pindex, nReward))
        {
            return error("UpdateOnNewBlock(): no staker with ID %u",
                         pindex->nStakerID);
        }
        if (queue.SlotProduced(nStakerSlot) == 0)
        {
            // this should never happen because the slot was already checked
            return error("UpdateOnNewBlock(): TSNH no such slot %u",
                         nStakerSlot);
        }
        QPMapStakers::iterator it;
        for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
        {
            if (it->second.IsQualified())
            {
                it->second.SawBlock();
            }
        }
    }
    nBlockHeight = pindex->nHeight;
    hashBlock = *(pindex->phashBlock);
    return true;
}

// this is where new stakers are born
bool QPRegistry::ApplyPurchase(const QPTxDetails &deet,
                               const CBlockIndex* pindex)
{
     
    if (deet.keys.empty())
    {
        return error("ApplyPurchase(): no keys");
    }

    QPStaker staker(deet);
 
    unsigned int nKeys = deet.keys.size();

    if (deet.t == TX_PURCHASE1)
    {
        if (nKeys != 1)
        {
            return error("ApplyPurchase(): not 1 key");
        }
    }
    else if (deet.t == TX_PURCHASE4)
    {
        if (nKeys == 4)
        {
            staker.pubkeyManager = deet.keys[1];
            staker.pubkeyDelegate = deet.keys[2];
            staker.pubkeyController = deet.keys[3];
        }
        else if (nKeys == 3)
        {
            staker.pubkeyManager = deet.keys[1];
            staker.pubkeyDelegate = deet.keys[1];
            staker.pubkeyController = deet.keys[2];
        }
        else
        {
            return error("ApplyPurchase(): not 3 or 4 keys");
        }
        if (!staker.SetDelegatePayout(deet.pcm))
        {
            return error("ApplyPurchase(): bad payout");
        }
    }
    else
    {
        // this should never happen
        return error("ApplyPurchase(): TSNH not a purchase");
    }
    string sKey;
    if (!AliasIsAvailable(deet.alias, sKey))
    {
        return error("ApplyPurchase(): alias not valid");
    }
    unsigned int nID = IncrementID();
    mapStakers[nID] = staker;

    // if not 0, then id specifies an NFT
    if (deet.id)
    {
        string sKeyUnused;
        if (!NftIsAvailable(deet.id, sKeyUnused))
        {
            // this should rarely happen
            return error("ApplyPurchase(): NFT isn't available (by ID)");
        }
        if (sKeyUnused != sKey)
        {
            // this should never happen
            return error("ApplyPurchase(): TSNH staker and NFT alias mismatch");
        }
        if (!SetStakerNft(nID, deet.id))
        {
            // this should never happen
            return error("ApplyPurchase(): TSNH couldn't set staker NFT (id)");
        }
        if (!mapStakers[nID].SetAlias(mapNfts[deet.id].strNickname))
        {
            return error("ApplyPurchase(): TSNH staker can't set alias to nick");
        }
    }
    else if (mapNftLookup.count(sKey))
    {
        unsigned int nNftID;
        if (!NftIsAvailable(sKey, nNftID))
        {
            return error("ApplyPurchase(): NFT isn't available (by key)");
        }
        if (!SetStakerNft(nID, nNftID))
        {
            // this should never happen
            return error("ApplyPurchase(): TSNH couldn't set staker NFT (key)");
        }
        if (!mapStakers[nID].SetAlias(mapNfts[nNftID].strNickname))
        {
            return error("ApplyPurchase(): TSNH can't set staker alias to nick");
        }
    }
    else
    {
        // grab next nft
        QPMapNftIterator it;
        for (it = mapNfts.begin(); it != mapNfts.end(); ++it)
        {
            if (!mapNftOwnerLookup.count(it->first))
            {
                break;
            }
        }
        if (it == mapNfts.end())
        {
            // this should never happen
            return error("ApplyPurchase(): TSNH no NFTs available");
        }
        if (!SetStakerNft(nID, it->first))
        {
            // this should never happen
            return error("ApplyPurchase(): TSNH couldn't set next NFT");
        }
        // user took next available character, so gets to choose staker alias
        if (!mapStakers[nID].SetAlias(deet.alias))
        {
            return error("ApplyPurchase(): TSNH staker can't set alias");
        }
    }

    if (fTestNet)
    {
        if (GetFork(pindex->nHeight) < XST_FORKMISSFIX)
        {
            ActivatePubKey(staker.pubkeyOwner, pindex);
        }
    }
    ActivatePubKey(staker.pubkeyManager, pindex);
    // for purposes of counting references, it's necessary to activate
    // the delegate even if it isn't getting a payout
    ActivatePubKey(staker.pubkeyDelegate, pindex);
   
    mapAliases[sKey] = make_pair(nID, deet.alias);
    return true;
}

bool QPRegistry::ApplySetKey(const QPTxDetails &deet,
                             const CBlockIndex* pindex)
{
    if (!mapStakers.count(deet.id))
    {
        return error("ApplySetKey(): no such staker");
    }
    if (mapStakers[deet.id].IsDisqualified())
    {
        return error("ApplySetKey(): staker is disqualified");
    }
    if (deet.keys.size() != 1)
    {
        return error("ApplySetKey(): wrong number of keys");
    }
    int nFork = GetFork(pindex->nHeight);
    QPStaker* pstaker = &mapStakers[deet.id];
    const CPubKey keyNew = deet.keys[0];
    switch (deet.t)
    {
    case TX_SETOWNER:
      {
        const CPubKey keyOld = pstaker->pubkeyOwner;
        if (keyNew != keyOld)
        {
            if (fTestNet)
            {
                // testnet hack for vestigal activated owner keys:
                //    don't fail if the old key wasn't active
                if (!DeactivatePubKey(keyOld))
                {
                    printf("ApplySetKey(): old testnet owner not active");
                }
                if (nFork < XST_FORKMISSFIX)
                {
                    ActivatePubKey(keyNew, pindex);
                }
            }
            pstaker->pubkeyOwner = keyNew;
        }    
        break;
      }
    case TX_SETMANAGER:
      {
        const CPubKey keyOld = pstaker->pubkeyManager;
        if (keyNew != keyOld)
        {
            if (!DeactivatePubKey(keyOld))
            {
                return error("ApplySetKey(): TSNH old manager not active");
            }
            ActivatePubKey(keyNew, pindex);
        }
        pstaker->pubkeyManager = keyNew;
        break;
      }
    case TX_SETDELEGATE:
      {
        const CPubKey keyOld = pstaker->pubkeyDelegate;
        if (!pstaker->SetDelegatePayout(deet.pcm))
        {
            return error("ApplySetKey(): bad payout");
        }
        if (keyNew != keyOld)
        {
            if (!DeactivatePubKey(keyOld))
            {
                return error("ApplySetKey(): TSNH old delegate not activated");
            }
            ActivatePubKey(keyNew, pindex);
            pstaker->pubkeyDelegate = keyNew;
        }
        break;
      }
    case TX_SETCONTROLLER:
        pstaker->pubkeyController = keyNew;
        break;
    default:
        // should never happen
        return error("ApplySetKey(): TSNH Not a setkey");
    }
    return true;
}

bool QPRegistry::ApplySetState(const QPTxDetails &deet, int nHeight)
{
    if (!mapStakers.count(deet.id))
    {
        return error("ApplySetState(): no such staker");
    }
    QPStaker& staker = mapStakers[deet.id];
    if (staker.IsDisqualified())
    {
        return error("ApplySetState(): staker is disqualified");
    }
    if (deet.t == TX_ENABLE)
    {
        if (staker.CanBeEnabled(nBestHeight))
        {
            if (!staker.Enable())
            {
                return error("ApplySetState(): can't enable");
            }
        }
        else
        {
           // enabling too soon is a no-op
           printf("ApplySetState(): too soon to enable, ignoring\n");
        }
    }
    else if (deet.t == TX_DISABLE)
    {
        staker.Disable(nHeight);
    }
    else
    {
        return error("ApplySetState(): not a setstate");
    }
    return true;
}

bool QPRegistry::ApplyClaim(const QPTxDetails &deet, int64_t nBlockTime)
{
    if (deet.t != TX_CLAIM)
    {
        return error("ApplyClaim(): not a claim");
    }
    if (deet.keys.size() != 1)
    {
        return error("ApplyClaim(): not 1 key");
    }
    CPubKey key = deet.keys[0];
    if (!CanClaim(key, deet.value, nBlockTime))
    {
        return error("ApplyClaim(): key %s can't claim %" PRIu64,
                     HexStr(key.Raw()).c_str(), deet.value);
    }
    mapLastClaim[key] = nBlockTime;
    mapBalances[key] -= deet.value;
    return true;
}

bool QPRegistry::ApplySetMeta(const QPTxDetails &deet)
{
    if (deet.t != TX_SETMETA)
    {
        return error("ApplySetMeta(): not a setmeta");
    }
    if (!mapStakers.count(deet.id))
    {
        return error("ApplySetMeta(): no such staker");
    }
    if (mapStakers[deet.id].IsDisqualified())
    {
        return error("ApplySetMeta(): staker is disqualified");
    }
    mapStakers[deet.id].SetMeta(deet.meta_key, deet.meta_value);
    return true;
}


bool QPRegistry::SetStakerNft(unsigned int nStakerID,
                              unsigned int nNftID)
{
    if (!mapStakers.count(nStakerID))
    {
        return error("SetStakerNft(): no such staker");
    }
    if (!mapNfts.count(nNftID))
    {
        return error("SetStakerNft(): no such NFT");
    }
    if (mapNftOwners.count(nStakerID))
    {
        return error("SetStakerNft(): staker already assigned NFT");
    }
    if (mapNftOwnerLookup.count(nNftID))
    {
        return error("SetStakerNft(): NFT already assigned to staker");
    }
    mapNftOwners[nStakerID] = nNftID;
    mapNftOwnerLookup[nNftID] = nStakerID;
    return true;
}

// any failed ops terminate with an assertion because there is no good
// way to continue
void QPRegistry::ApplyOps(const CBlockIndex *pindex)
{
    BOOST_FOREACH(const QPTxDetails &deet, pindex->vDeets)
    {
        switch (deet.t)
        {
        case TX_PURCHASE1:
        case TX_PURCHASE4:
            assert (ApplyPurchase(deet, pindex));
            break;
        case TX_SETOWNER:
        case TX_SETMANAGER:
        case TX_SETDELEGATE:
        case TX_SETCONTROLLER:
            assert (ApplySetKey(deet, pindex));
            break;
        case TX_ENABLE:
        case TX_DISABLE:
            assert (ApplySetState(deet, pindex->nHeight));
            break;
        case TX_CLAIM:
            assert (ApplyClaim(deet, pindex->nTime));
            break;
        case TX_SETMETA:
            assert (ApplySetMeta(deet));
            break;
        default:
            assert (error("ApplyQPoSOps(): No such operation"));
        }
    }
}

// This is not ideal. Registry should decide for itself.
void QPRegistry::EnterReplayMode()
{
    if (OutOfReplayMode())
    {
        // wait for enough blocks to roll in to advance the queue
        //   before going back into replay
        if (nBlockHeight > (int)(nHeightExitedReplay + (1 + queue.Size())))
        {
            printf("QPRegistry::EnterReplayMode(): entering replay mode\n");
            nHeightExitedReplay = 0;
        }
    }
}

void QPRegistry::ExitReplayMode()
{
    // Explict exit of replay is only necessary to kickstart
    // block production (typically not needed on mainnet).
    nHeightExitedReplay = nBlockHeight;
}
