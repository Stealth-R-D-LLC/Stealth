// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>

#include "QPRegistry.hpp"

#include "txdb-leveldb.h"

using namespace std;
using namespace json_spirit;

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
    mapStakers.clear();
    mapBalances.clear();
    mapLastClaim.clear();
    mapActive.clear();
    mapAliases.clear();
    nIDCounter = 0;
    nIDSlotPrev = 0;
    fCurrentBlockWasProduced = false;
    // can't penalize the very first staker ever to have a slot
    fPrevBlockWasProduced = true;
    nBlockHeight = 0;
    hashBlock = (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet);
    hashBlockLastSnapshot = hashBlock;
    hashLastBlockPrevQueue = hashBlock;
    fIsInReplayMode = true;
    queue.SetNull();
    powerRoundPrev.SetNull();
    powerRoundCurrent.SetNull();
    fShouldRollback = false;
}

unsigned int QPRegistry::GetRound() const
{
    return nRound;
}

unsigned int QPRegistry::Size() const
{
    return mapStakers.size();
}

bool QPRegistry::IsInReplayMode() const
{
    return fIsInReplayMode;
}

int QPRegistry::GetBlockHeight() const
{
    return nBlockHeight;
}

uint256 QPRegistry::GetBlockHash() const
{
    return hashBlock;
}

uint256 QPRegistry::GetHashLastBlockPrevQueue() const
{
    return hashLastBlockPrevQueue;
}

unsigned int QPRegistry::GetNumberQualified() const
{
    unsigned int count = 0;
    QPRegistryIterator it;
    for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
    {
        if (!it->second.IsDisqualified())
        {
            count += 1;
        }
    }
    return count;
}

bool QPRegistry::IsQualifiedStaker(unsigned int nStakerID) const
{
    QPStaker staker;
    if (!GetStaker(nStakerID, staker))
    {
        return false;
    }
    return !staker.IsDisqualified();
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
        printf("TimestampIsValid(): window: %u-%u, time: %u "
                                   "staker: %u, round: %u\n",
               window.start, window.end, nTime, nStakerID, nRound);
    }
    return fIsValid;
}

unsigned int QPRegistry::GetCurrentRound() const
{
    return nRound;
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

bool QPRegistry::CurrentBlockWasProduced() const
{
    return fCurrentBlockWasProduced;
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
        return false;
    }
    it = mapLastClaim.find(key);
    if (it != mapLastClaim.end())
    {
        if (nClaimTime < (it->second + QP_MIN_SECS_PER_CLAIM))
        {
            return false;
        }
    }
    return true;
}

bool QPRegistry::GetOwnerKey(unsigned int nStakerID, CPubKey &keyRet) const
{
    QPStaker staker;
    if (!GetStaker(nStakerID, staker))
    {
        return error("GetOwnerKey(): no such staker %u", nStakerID);
    }
    keyRet = staker.pubkeyOwner;
    return true;
}

bool QPRegistry::GetDelegateKey(unsigned int nStakerID, CPubKey &keyRet) const
{
    QPStaker staker;
    if (!GetStaker(nStakerID, staker))
    {
        return error("GetDelegateKey(): no such staker %u", nStakerID);
    }
    keyRet = staker.pubkeyDelegate;
    return true;
}

bool QPRegistry::GetControllerKey(unsigned int nStakerID,
                                  CPubKey &keyRet) const
{
    QPStaker staker;
    if (!GetStaker(nStakerID, staker))
    {
        return error("GetControllerKey(): no such staker %u", nStakerID);
    }
    keyRet = staker.pubkeyController;
    return true;
}

bool QPRegistry::GetStakerWeight(unsigned int nStakerID,
                           unsigned int &nWeightRet) const
{
    QPStaker staker;
    if (!GetStaker(nStakerID, staker))
    {
        return error("GetKeyForID() : no staker for ID %d", nStakerID);
    }
    nWeightRet = staker.GetWeight();
    return true;
}

void QPRegistry::AsJSON(Object &objRet) const
{
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
        aryQueue.push_back(objSlot);
    }
    unsigned int nCurrentSlot = queue.GetCurrentSlot();

    vector<pair<unsigned int, const QPStaker*> > vIDs;
    QPRegistryIterator mit;
    for (mit = mapStakers.begin(); mit != mapStakers.end(); ++mit)
    {
        vIDs.push_back(make_pair(mit->first, &(mit->second)));
    }

    sort(vIDs.begin(), vIDs.end());

    Object objStakers;
    vector<pair<unsigned int, const QPStaker*> >::const_iterator vit;
    for (vit = vIDs.begin(); vit != vIDs.end(); ++vit)
    {
        Object objStkr;
        vit->second->AsJSON(vit->first, objStkr);
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

        std::map<CPubKey, int64_t>::const_iterator it;
        it = mapLastClaim.find(key);
        if (it != mapLastClaim.end())
        {
            objBal.push_back(Pair("last_claim", it->second));
        }

        std::map<CPubKey, int>::const_iterator jt;
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
    objRet.push_back(Pair("block_height", nBlockHeight));
    objRet.push_back(Pair("block_hash", hashBlock.GetHex()));
    objRet.push_back(Pair("last_block_hash_prev_queue",
                           hashLastBlockPrevQueue.GetHex()));
    objRet.push_back(Pair("in_replay", fIsInReplayMode));
    objRet.push_back(Pair("should_roll_back", fShouldRollback));
    objRet.push_back(Pair("counter_next",
                          static_cast<int64_t>(nIDCounter + 1)));
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
    objRet.push_back(Pair("stakers", objStakers));
    objRet.push_back(Pair("balances", objBalances));
}

void QPRegistry::GetStakerAsJSON(unsigned int nID,
                                 Object &objRet,
                                 bool fWithRecentBlocks) const
{
    QPStaker staker;
    if (GetStaker(nID, staker))
    {
        staker.AsJSON(nID, objRet, true);
    }
}

void QPRegistry::CheckSynced()
{
    if (fIsInReplayMode)
    {
        int nTime = GetAdjustedTime();
        if (HasEnoughPower() && queue.TimeIsInCurrentSlotWindow(nTime))
        {
            printf("QPRegistry::CheckSynced(): exiting replay mode\n");
            fIsInReplayMode = false;
        }
    }
}

void QPRegistry::Copy(const QPRegistry *const pother)
{
    nVersion = pother->nVersion;
    nRound = pother->nRound;
    mapStakers = pother->mapStakers;
    mapBalances = pother->mapBalances;
    mapLastClaim = pother->mapLastClaim;
    mapActive = pother->mapActive;
    mapAliases = pother->mapAliases;
    queue = pother->queue;
    nIDCounter = pother->nIDCounter;
    nIDSlotPrev = pother->nIDSlotPrev;
    fCurrentBlockWasProduced = pother->fCurrentBlockWasProduced;
    fPrevBlockWasProduced = pother->fPrevBlockWasProduced;
    nBlockHeight = pother->nBlockHeight;
    hashBlock = pother->hashBlock;
    hashBlockLastSnapshot = pother->hashBlockLastSnapshot;
    hashLastBlockPrevQueue = pother->hashLastBlockPrevQueue;
    powerRoundPrev = pother->powerRoundPrev;
    powerRoundCurrent = pother->powerRoundCurrent;

    fIsInReplayMode = pother->fIsInReplayMode;
    fShouldRollback = pother->fShouldRollback;
}

void QPRegistry::ActivatePubKey(const CPubKey &key)
{
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
    QPStaker *pstaker;
    if (!GetStaker(nID, pstaker))
    {
        return error("SetStakerAlias(): no staker with ID %u", nID);
    }
    if (!pstaker->SetAlias(sAlias))
    {
        return error("SetStakerAlias(): can't set to %s", sAlias.c_str());
    }
    mapAliases[sKey] = make_pair(nID, sAlias);
    return true;
}

bool QPRegistry::GetStaker(unsigned int nID, QPStaker* &pstakerRet)
{
    bool result = false;
    if ((nID > 0) && (nID <= nIDCounter))
    {
        std::map<unsigned int, QPStaker>::iterator iter;
        for (iter = mapStakers.begin(); iter != mapStakers.end(); ++iter)
        {
            if (iter->first == nID)
            {
                result = true;
                pstakerRet = &(iter->second);
                break;
            }
        }
    }
    return result;
}

bool QPRegistry::GetStaker(unsigned int nID, QPStaker &stakerRet) const
{
    bool result = false;
    if ((nID > 0) && (nID <= nIDCounter))
    {
        std::map<unsigned int, QPStaker>::const_iterator iter;
        for (iter = mapStakers.begin(); iter != mapStakers.end(); ++iter)
        {
            if (iter->first == nID)
            {
                result = true;
                stakerRet = iter->second;
                break;
            }
        }
    }
    return result;
}

unsigned int QPRegistry::GetCurrentIDCounter() const
{
    return nIDCounter;
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

// returns true if the current block should be produced
// this is the key procedure for keeping the registry
//   synced when not in replay
bool QPRegistry::GetIDForCurrentTime(CBlockIndex *pindex,
                                     unsigned int &nIDRet)
{
    unsigned int nTime = GetAdjustedTime();

    // queue has gotten ahead some how
    if (nTime < queue.GetCurrentSlotStart())
    {
        // should never happen
        printf("GetIDForCurrentTime(): queue has gotten ahead\n");
        nIDRet = queue.GetCurrentID();
        return false;
    }

    if (nTime <= queue.GetMaxTime())
    {
        unsigned int nSlot;
        if (queue.GetSlotForTime(nTime, nSlot))
        {
            unsigned int nCurrentSlot = queue.GetCurrentSlot();
            // return the current ID if the queue is up to date
            if (nSlot == nCurrentSlot)
            {
                nIDRet = queue.GetCurrentID();
                return !fCurrentBlockWasProduced;
            }
        }
        else
        {
            // should never happen
            printf("GetIDForCurrentTime(): queue state is inconsistent\n");
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
    // don't take snapshots of the temp registry
    pregistryTemp->UpdateOnNewTime(nTime, pindex, false);
    nIDRet = pregistryTemp->GetIDForCurrentSlot();
    delete pregistryTemp;
    return true;
}

unsigned int QPRegistry::GetIDForPrevSlot() const
{
    return nIDSlotPrev;
}

// asdf
// bool QPRegistry::GetKeyForCurrentSlot(CKeyID &keyRet) const
// {
//     unsigned int nID = queue.GetCurrentID();
//     return GetKeyForID(nID, keyRet);
// }

bool QPRegistry::GetPrevRecentBlocksMissedMax(unsigned int nID,
                                              uint32_t &nMaxRet) const
{
    static const uint32_t MAX_STDDEVS = 8;
    uint32_t n = 0;
    uint32_t total = 0;
    vector<uint32_t> vMissed;
    map<unsigned int, QPStaker>::const_iterator it;
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
        printf("asdf alias %s isn't valid\n", sAlias.c_str());
        return false;
    }
    sKeyRet = ToLowercaseSafe(sAlias);
    printf("asdf map aliases count of %s is %d\n", sKeyRet.c_str(), (int)mapAliases.count(sKeyRet));
    return (mapAliases.count(sKeyRet) == 0);
}

bool QPRegistry::GetIDForAlias(const string &sAlias,
                               unsigned int &nIDRet) const
{
    string sKey = ToLowercaseSafe(sAlias);
    map<string, pair<unsigned int, string> >::const_iterator it;
    for (it = mapAliases.begin(); it != mapAliases.end(); ++it)
    {
        if (it->first == sKey)
        {
            nIDRet = it->second.first;
            return true;
        }
    }
    return false;
}

bool QPRegistry::GetAliasForID(unsigned int nID,
                               string &sAliasRet) const
{
    QPStaker staker;
    if (!GetStaker(nID, staker))
    {
        return error("GetAliasForID(): no such staker");
    }
    sAliasRet = staker.GetAlias();
    return true;
}

std::string QPRegistry::GetAliasForID(unsigned int nID)
{
    if (nID == 0)
    {
        return STRING_NO_ALIAS;
    }
    QPStaker *pstaker;
    if (!GetStaker(nID, pstaker))
    {
        printf("GetAliasForID(): ERROR: no such staker");
        return STRING_NO_ALIAS;
    }
    return pstaker->GetAlias();
}

bool QPRegistry::GetStakerForAlias(const string &sAlias,
                                   QPStaker &qStakerRet) const
{
    unsigned int nID;
    if (!GetIDForAlias(sAlias, nID))
    {
        return false;
    }
    QPStaker staker;
    if (!GetStaker(nID, staker))
    {
            return error("GetStakerForAlias(): alias and ID mismatch");
    }
    qStakerRet = staker;
    return true;
}


// this will only be called for a block that is being added to the end of the
// chain so that the Registry state is synchronous with the block
// same goes for StakerMissedBlock
bool QPRegistry::StakerProducedBlock(unsigned int nID, int64_t nReward)
{
    QPStaker *pstaker;
    if (!GetStaker(nID, pstaker))
    {
        return error("StakerProducedBlock(): unknown ID %d", nID);
    }
    pstaker = &(mapStakers[nID]);
    powerRoundCurrent.PushBack(nID, pstaker->GetWeight(), true);
    int64_t nOwnerReward, nDelegateReward;
    pstaker->ProducedBlock(nReward,
                           fPrevBlockWasProduced,
                           nOwnerReward,
                           nDelegateReward);
    mapBalances[pstaker->pubkeyOwner] += nOwnerReward;
    if (nDelegateReward > 0)
    {
        mapBalances[pstaker->pubkeyDelegate] += nDelegateReward;
    }
    DisqualifyStakerIfNecessary(nID, pstaker);
    fCurrentBlockWasProduced = true;
    fPrevBlockWasProduced = true;
    return true;
}

bool QPRegistry::StakerMissedBlock(unsigned int nID)
{
    QPStaker *pstaker;
    if (!GetStaker(nID, pstaker))
    {
        return error("StakerMissedBlock(): unknown ID %d", nID);
    }
    pstaker = &(mapStakers[nID]);
    powerRoundCurrent.PushBack(nID, pstaker->GetWeight(), false);
    pstaker->MissedBlock(fPrevBlockWasProduced);
    DisqualifyStakerIfNecessary(nID, pstaker);
    fPrevBlockWasProduced = false;
    return true;
}

bool QPRegistry::DisqualifyStaker(unsigned int nID)
{
    QPStaker *pstaker;
    if (!GetStaker(nID, pstaker))
    {
        return error("StakerMissedBlock(): unknown ID");
    }
    pstaker->Disqualify();
    return true;
}

bool QPRegistry::TerminateStaker(unsigned int nID)
{
    unsigned int j = mapStakers.erase(nID);
    if (j < 1)
    {
        return error("TerminateStaker(): no such staker %d", nID);
    }
    else if (j > 1)
    {
        return error("TerminateStaker(): more than one staker %d", nID);
    }
    printf("TerminateStaker(): terminating staker %u\n", nID);
    return true;
}


bool QPRegistry::DisqualifyStakerIfNecessary(unsigned int nID,
                                             const QPStaker *pstaker)
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
asdfjkl; sort
    uint256 hash(prevHash);
#ifdef ASDF
    for (int i = 0; i < QP_ROUNDS; ++i)
    {
       hash = Hash9(hash.begin(), hash.end());
    }
#else
    hash = SerializeHash(hash);
#endif
    valtype vch;
    for (unsigned int i = 0; i < 4; ++i)
    {
        vch.push_back(*(hash.begin() + i));
    }
    uint32_t seed = static_cast<uint32_t>(vchnum(vch).GetValue());
    boost::mt19937 gen(seed);
    boost::uniform_int<> dist;
    QPShuffler shuffler(gen, dist);
    random_shuffle(vIDs.begin(), vIDs.end(), shuffler);
    queue = QPQueue(nTime0, vIDs);
    nRound += 1;
    powerRoundPrev.Copy(powerRoundCurrent);
    powerRoundCurrent.SetNull();
    hashLastBlockPrevQueue = prevHash;
    return true;
}

unsigned int QPRegistry::IncrementID()
{
    nIDCounter += 1;
    return nIDCounter;
}

void QPRegistry::TerminateDisqualifiedStakers()
{
    vector<unsigned int> vTerminate;
    map<unsigned int, QPStaker>::const_iterator it;
    for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
    {
        if (it->second.IsDisqualified())
        {
            vTerminate.push_back(it->first);
        }
    }
    vector<unsigned int>::const_iterator jt;
    for (jt = vTerminate.begin(); jt != vTerminate.end(); ++jt)
    {
        TerminateStaker(*jt);
    }
}

bool QPRegistry::DockInactiveKeys(int64_t nMoneySupply)
{
    int64_t nDockValue = nMoneySupply / DOCK_INACTIVE_FRACTION;
    bool fResult = true;
    map<CPubKey, int>::const_iterator it;
    for (it = mapActive.begin(); it != mapActive.end(); ++it)
    {
        if (it->second <= 0)
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
        if (it->second < nDockValue)
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
                                 bool fWriteSnapshot)
{
    uint256 hash = *(pindex->phashBlock);
    if (GetFork(pindex->nHeight + 1) >= XST_FORKASDF)
    {
        unsigned int nNewQueues = 0;
        if (queue.IsEmpty())
        {
            // qPoS shall begin
            printf("UpdateOnNewTime(): Starting qPoS at block %s\n",
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
                StakerMissedBlock(queue.GetCurrentID());
            }
            if (!queue.IncrementSlot())
            {
                TerminateDisqualifiedStakers();
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
                if (!fIsInReplayMode)
                {
                    printf("UpdateOnNewTime(): entering replay mode\n");
                    fIsInReplayMode = true;
                }
            }
        }
        if (nNewQueues > 0)
        {
            printf("UpdateOnNewTime: %u new queues with %s\n",
                   nNewQueues, hashBlock.ToString().c_str());
            printf("   newest queue starts at %d\n", queue.GetMinTime());
            printf("asdf staker produced block = %d\n", fCurrentBlockWasProduced);
        }
    }

    // take snapshot here because it gives the registry time to catch up
    // with the best block (registry state will be ready for next block)
    if (((pindex->nHeight % BLOCKS_PER_SNAPSHOT) == 0) &&
        fWriteSnapshot &&
        (hash != hashBlockLastSnapshot))
    {
        CTxDB txdb;
        hashBlockLastSnapshot = hash;
        txdb.WriteRegistrySnapshot(pindex->nHeight, *this);
        printf("UpdateOnNewTime(): writing snapshot %d\n",
                (int)pindex->nHeight);
        unsigned int n = BLOCKS_PER_SNAPSHOT * SNAPSHOTS_TO_KEEP;
        txdb.EraseRegistrySnapshot(pindex->nHeight - n);
    }
    return true;
}

bool QPRegistry::UpdateOnNewBlock(const CBlockIndex *const pindex,
                                  bool fWriteSnapshot)
{
    const CBlockIndex *pindexPrev = (pindex->pprev ? pindex->pprev : pindex);
    if (!UpdateOnNewTime(pindex->nTime, pindexPrev, fWriteSnapshot))
    {
        return error("UpdateOnNewBlock: could not update on new time for %s",
                     pindex->phashBlock->ToString().c_str());
    }
    if (GetFork(pindex->nHeight) >= XST_FORKPURCHASE)
    {
        if (!pindex->vDeets.empty())
        {
            printf("asdf apply ops for block %d\n", pindex->nHeight);
            ApplyOps(pindex);
        }
    }
    if (GetFork(pindex->nHeight) >= XST_FORKASDF)
    {
        printf("UpdateOnNewBlock(): round=%d, slot=%d, window=%d-%d, "
                  "picopower=%" PRIu64 "\n   hash=%s, height=%d\n",
               nRound,
               queue.GetCurrentSlot(),
               queue.GetCurrentSlotStart(),
               queue.GetCurrentSlotEnd(),
               GetPicoPower(),
               pindex->phashBlock->ToString().c_str(),
               pindex->nHeight);

        if (fCurrentBlockWasProduced && !fIsInReplayMode)
        {
            return error("UpdateOnNewBlock(): "
                         "block already produced for this slot");
        }
        int64_t nReward = GetQPoSReward(pindex->pprev);
        if (!StakerProducedBlock(pindex->nStakerID, nReward))
        {
            return error("UpdateOnNewBlock(): no staker with ID %u",
                         pindex->nStakerID);
        }

        std::map<unsigned int, QPStaker>::iterator it;
        for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
        {
            it->second.SawBlock();
        }
    }
    nBlockHeight = pindex->nHeight;
    hashBlock = *(pindex->phashBlock);
    return true;
}

// this is where new stakers are born
bool QPRegistry::ApplyPurchase(const QPoSTxDetails &deet)
{
    if (deet.keys.empty())
    {
        return error("ApplyPurchase(): no keys");
    }
    QPStaker staker(deet.keys[0]);

    if (deet.t == TX_PURCHASE1)
    {
        if (deet.keys.size() != 1)
        {
            return error("ApplyPurchase(): not 1 key");
        }
    }
    else if (deet.t == TX_PURCHASE3)
    {
        if (deet.keys.size() != 3)
        {
            return error("ApplyPurchase(): not 3 keys");
        }
        staker.pubkeyDelegate = deet.keys[1];
        staker.pubkeyController = deet.keys[2];
        if (!staker.SetDelegatePayout(deet.pcm))
        {
            return error("ApplyPurchase(): bad payout");
        }
    }
    else
    {
        // should never happen
        return error("ApplyPurchase(): not a purchase");
    }
    string sKey;
    if (!AliasIsAvailable(deet.alias, sKey))
    {
        return error("ApplyPurchase(): alias not valid");
    }
    if (!staker.SetAlias(deet.alias))
    {
        return error("ApplyPurchase(): staker can't set alias");
    }
    unsigned int nID = IncrementID();
    mapStakers[nID] = staker;
    ActivatePubKey(deet.keys[0]);
    if (deet.t == TX_PURCHASE3)
    {
        ActivatePubKey(deet.keys[1]);
    }
    mapAliases[sKey] = make_pair(nID, deet.alias);
    return true;
}

bool QPRegistry::ApplySetKey(const QPoSTxDetails &deet)
{
    if (!mapStakers.count(deet.id))
    {
        return error("ApplySetKey(): no such staker");
    }
    if (deet.keys.size() != 1)
    {
        return error("ApplySetKey(): wrong number of keys");
    }
    switch (deet.t)
    {
    case TX_SETOWNER:
        mapStakers[deet.id].pubkeyOwner = deet.keys[0];
        break;
    case TX_SETDELEGATE:
        if (!mapStakers[deet.id].SetDelegatePayout(deet.pcm))
        {
            return error("ApplySetKey(): bad payout");
        }
        mapStakers[deet.id].pubkeyDelegate = deet.keys[0];
        break;
    case TX_SETCONTROLLER:
        mapStakers[deet.id].pubkeyController = deet.keys[0];
        break;
    default:
        // should never happen
        return error("ApplySetKey(): Not a setkey");
    }
    return true;
}

bool QPRegistry::ApplySetState(const QPoSTxDetails &deet)
{
    if (!mapStakers.count(deet.id))
    {
        return error("ApplySetState(): no such staker");
    }
    if (deet.t == TX_ENABLE)
    {
        if (!mapStakers[deet.id].Enable())
        {
            return error("ApplySetState(): can't enable");
        }
    }
    else if (deet.t == TX_DISABLE)
    {
        mapStakers[deet.id].Disable();
    }
    else
    {
        return error("ApplySetState(): not a setstate");
    }
    return true;
}

bool QPRegistry::ApplyClaim(const QPoSTxDetails &deet, int64_t nBlockTime)
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
        return error("ApplyClaim(): key can't claim value");
    }
    mapLastClaim[key] = nBlockTime;
    mapBalances[key] -= deet.value;
    return true;
}



// any failed ops terminate with an assertion because there is no good
// way to continue
void QPRegistry::ApplyOps(const CBlockIndex *const pindex)
{
    BOOST_FOREACH(const QPoSTxDetails &deet, pindex->vDeets)
    {
        switch (deet.t)
        {
        case TX_PURCHASE1:
        case TX_PURCHASE3:
            assert (ApplyPurchase(deet));
            break;
        case TX_SETOWNER:
        case TX_SETDELEGATE:
        case TX_SETCONTROLLER:
            assert (ApplySetKey(deet));
            break;
        case TX_ENABLE:
        case TX_DISABLE:
            assert (ApplySetState(deet));
            break;
        case TX_CLAIM:
            assert (ApplyClaim(deet, pindex->nTime));
            break;
        default:
            assert (error("ApplyQPoSOps(): No such operation"));
        }
    }
}

void QPRegistry::ExitReplayMode()
{
    // Explict exit of replay is only necessary to kickstart
    // block production, and that should only be necessary on testnet.
    if (fTestNet)
    {
        fIsInReplayMode = false;
    }
}
