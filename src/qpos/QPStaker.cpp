// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "QPStaker.hpp"
#include "main.h"

using namespace json_spirit;
using namespace std;

extern CBlockIndex* pindexBest;
extern Value ValueFromAmount(int64_t amount);

void BlockAsJSONLite(const CBlockIndex *pindex, Object& objRet);

QPStaker::QPStaker() {
    Reset();
}

QPStaker::QPStaker(const QPTxDetails& deet) {
    pubkeyOwner = deet.keys[0];
    pubkeyManager = pubkeyOwner;
    pubkeyDelegate = pubkeyOwner;
    pubkeyController = pubkeyOwner;
    hashBlockCreated = deet.hash;
    hashTxCreated = deet.txid;
    nOutCreated = static_cast<unsigned int>(deet.n);
    nPrice = deet.value;
    Reset();
}

void QPStaker::Reset() {
    nVersion = QPStaker::CURRENT_VERSION;
    bRecentBlocks.set();
    bPrevRecentBlocks.set();
    nBlocksProduced = 0;
    nBlocksMissed = 0;
    nBlocksDocked = 0;
    nBlocksAssigned = 0;
    nBlocksSeen = 0;
    hashBlockMostRecent = 0;
    nPrevBlocksMissed = 0;
    nPcmDelegatePayout = 0;
    nHeightDisabled = -1;
    fQualified = true;
    nTotalEarned = 0;
    sAlias = "";
    mapMeta.clear();
}

const QPRecentBlocks& QPStaker::GetRecentBlocks() const {
    return bRecentBlocks;
}

const QPRecentBlocks& QPStaker::GetPrevRecentBlocks() const {
    return bPrevRecentBlocks;
}

uint32_t QPStaker::GetRecentBlocksProduced() const {
    return static_cast<uint32_t>(bRecentBlocks.count());
}

uint32_t QPStaker::GetPrevRecentBlocksProduced() const {
    return static_cast<uint32_t>(bPrevRecentBlocks.count());
}

uint32_t QPStaker::GetRecentBlocksMissed() const {
    uint32_t m = QP_STAKER_RECENT_BLOCKS - GetRecentBlocksProduced();
    return min(nBlocksMissed, m);
}

uint32_t QPStaker::GetPrevRecentBlocksMissed() const {
    uint32_t m = QP_STAKER_RECENT_BLOCKS - GetPrevRecentBlocksProduced();
    return min(nPrevBlocksMissed, m);
}

uint256 QPStaker::GetHashBlockCreated() const {
    return hashBlockCreated;
}

const CBlockIndex* QPStaker::GetBlockCreated() const {
    return GetBlockIndex(hashBlockCreated);
}

uint256 QPStaker::GetHashTxCreated() const {
    return hashTxCreated;
}

unsigned int QPStaker::GetNOutCreated() const {
    return nOutCreated;
}

int64_t QPStaker::GetPrice() const {
    return nPrice;
}

uint32_t QPStaker::GetBlocksProduced() const {
    return nBlocksProduced;
}

uint32_t QPStaker::GetBlocksMissed() const {
    return nBlocksMissed;
}

uint32_t QPStaker::GetBlocksDocked() const {
    return nBlocksDocked;
}

uint32_t QPStaker::GetBlocksAssigned() const {
    return nBlocksAssigned;
}

uint32_t QPStaker::GetBlocksSeen() const {
    return nBlocksSeen;
}

uint32_t QPStaker::GetNetBlocks() const {
    return (nBlocksDocked >= nBlocksProduced) ? 0 : nBlocksProduced - nBlocksDocked;
}

uint256 QPStaker::GetHashBlockMostRecent() const {
    return hashBlockMostRecent;
}

const CBlockIndex* QPStaker::GetBlockMostRecent() const {
    return GetBlockIndex(hashBlockMostRecent);
}

bool QPStaker::DidMissMostRecentBlock() const {
    return !bRecentBlocks[0];
}

bool QPStaker::DidProduceMostRecentBlock() const {
    return bRecentBlocks[0];
}

unsigned int QPStaker::GetWeight(unsigned int nSeniority) const {
    uint32_t net = GetNetBlocks();
    return (net > 0) ? uisqrt(net + (nSeniority * nSeniority)) : nSeniority;
}

uint32_t QPStaker::GetDelegatePayout() const {
    return nPcmDelegatePayout;
}

bool QPStaker::IsProductive() const {
    return GetTotalEarned() > 0;
}

bool QPStaker::IsEnabled() const {
    return fQualified && !nHeightDisabled;
}

bool QPStaker::IsDisabled() const {
    return nHeightDisabled || !fQualified;
}

bool QPStaker::IsQualified() const {
    return fQualified;
}

bool QPStaker::IsDisqualified() const {
    return !fQualified;
}

bool QPStaker::ShouldBeDisabled(int nHeight) const {
    unsigned int nMaxMiss = GetStakerMaxMisses(nHeight);
    unsigned int nPrevMaxMiss = nMaxMiss / 2;
    bool fDisable = true;
    for (unsigned int i = 0; i < nMaxMiss; ++i) {
        if (bRecentBlocks[i]) {
            fDisable = false;
            break;
        }
    }
    if (fDisable) {
        return true;
    }
    fDisable = true;
    for (unsigned int i = 0; i < nPrevMaxMiss; ++i) {
        if (bPrevRecentBlocks[i]) {
            fDisable = false;
            break;
        }
    }
    return fDisable;
}

bool QPStaker::ShouldBeDisqualified(uint32_t nPrevRecentBlocksMissedMax) const {
    return (GetPrevRecentBlocksMissed() > nPrevRecentBlocksMissedMax) ||
           ((nBlocksSeen > QP_NOOB_BLOCKS) &&
            ((GetNetBlocks() <= 0) ||
             (GetRecentBlocksMissed() > (QP_STAKER_RECENT_BLOCKS / 2))));
}

int64_t QPStaker::GetTotalEarned() const {
    return nTotalEarned;
}

string QPStaker::GetAlias() const {
    return sAlias;
}

bool QPStaker::HasMeta(const string &key) const {
    return mapMeta.count(key) != 0;
}

bool QPStaker::GetMeta(const string &key, string &valueRet) const {
    auto it = mapMeta.find(key);
    if (it == mapMeta.end()) {
        valueRet.clear();
        return false;
    }
    valueRet = it->second;
    return true;
}

void QPStaker::CopyMeta(map<string, string> &mapRet) const {
    mapRet = mapMeta;
}

void QPStaker::AsJSON(unsigned int n
