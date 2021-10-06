// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "QPStaker.hpp"

#include "main.h"

using namespace json_spirit;
using namespace std;

extern CBlockIndex* pindexBest;

extern Value ValueFromAmount(int64_t amount);


void BlockAsJSONLite(const CBlockIndex *pindex, Object& objRet)
{
    objRet.clear();
    objRet.push_back(Pair("hash", pindex->phashBlock->GetHex()));
    if (pindex)
    {
        objRet.push_back(Pair("height",
                              static_cast<int64_t>(pindex->nHeight)));
        objRet.push_back(Pair("time",
                              static_cast<int64_t>(pindex->nTime)));
        if (pindex->IsInMainChain())
        {
            objRet.push_back(Pair("isinmainchain", true));
            objRet.push_back(Pair("confirmations",
                                 pindexBest->nHeight + 1 - pindex->nHeight));
        }
        else
        {
            objRet.push_back(Pair("isinmainchain", false));
        }
    }
    else
    {
        // this should never happen
        objRet.push_back(Pair("height", -1));
    }
}


QPStaker::QPStaker()
{
    pubkeyOwner = CPubKey();
    pubkeyManager = CPubKey();
    pubkeyDelegate = CPubKey();
    pubkeyController = CPubKey();
    Reset();
}

QPStaker::QPStaker(const QPTxDetails& deet)
{
    pubkeyOwner = deet.keys[0];
    pubkeyManager = pubkeyOwner;
    pubkeyDelegate = pubkeyOwner;
    pubkeyController = pubkeyOwner;
    hashBlockCreated = deet.hash;
    hashTxCreated  = deet.txid;
    nOutCreated = static_cast<unsigned int>(deet.n);
    nPrice = deet.value;
    Reset();
}

void QPStaker::Reset()
{
    nVersion = QPStaker::CURRENT_VERSION;
    // start as if staker hit all blocks
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


const QPRecentBlocks& QPStaker::GetRecentBlocks() const
{
    return bRecentBlocks;
}

const QPRecentBlocks& QPStaker::GetPrevRecentBlocks() const
{
    return bPrevRecentBlocks;
}


uint32_t QPStaker::GetRecentBlocksProduced() const
{
    return static_cast<uint32_t>(bRecentBlocks.count());
}

uint32_t QPStaker::GetPrevRecentBlocksProduced() const
{
    return static_cast<uint32_t>(bPrevRecentBlocks.count());
}

uint32_t QPStaker::GetRecentBlocksMissed() const
{
    uint32_t m = QP_STAKER_RECENT_BLOCKS - GetRecentBlocksProduced();
    return min(nBlocksMissed, m);
}

uint32_t QPStaker::GetPrevRecentBlocksMissed() const
{
    uint32_t m = QP_STAKER_RECENT_BLOCKS - GetPrevRecentBlocksProduced();
    return min(nPrevBlocksMissed, m);
}

uint256 QPStaker::GetHashBlockCreated() const
{
    return hashBlockCreated;
}

const CBlockIndex* QPStaker::GetBlockCreated() const
{
    map<uint256, CBlockIndex*>::iterator mi =
                         mapBlockIndex.find(hashBlockCreated);
    if (mi != mapBlockIndex.end())
    {
        return (*mi).second;
    }
    return NULL;
}

uint256 QPStaker::GetHashTxCreated() const
{
    return hashTxCreated;
}

unsigned int QPStaker::GetNOutCreated() const
{
    return nOutCreated;
}

int64_t QPStaker::GetPrice() const
{
    return nPrice;
}

uint32_t QPStaker::GetBlocksProduced() const
{
    return nBlocksProduced;
}

uint32_t QPStaker::GetBlocksMissed() const
{
    return nBlocksMissed;
}

uint32_t QPStaker::GetBlocksDocked() const
{
    return nBlocksDocked;
}

uint32_t QPStaker::GetBlocksAssigned() const
{
    return nBlocksAssigned;
}

uint32_t QPStaker::GetBlocksSeen() const
{
    return nBlocksSeen;
}

uint32_t QPStaker::GetNetBlocks() const
{
    if (nBlocksDocked >= nBlocksProduced)
    {
        return 0;
    }
    return nBlocksProduced - nBlocksDocked;
}

uint256 QPStaker::GetHashBlockMostRecent() const
{
    return hashBlockMostRecent;
}

const CBlockIndex* QPStaker::GetBlockMostRecent() const
{
    map<uint256, CBlockIndex*>::iterator mi =
                         mapBlockIndex.find(hashBlockMostRecent);
    if (mi != mapBlockIndex.end())
    {
        return (*mi).second;
    }
    return NULL;
}


bool QPStaker::DidMissMostRecentBlock() const
{
    return !bRecentBlocks[0];
}

bool QPStaker::DidProduceMostRecentBlock() const
{
    return bRecentBlocks[0];
}

// Returns 1 even when missed blocks outnumber produced blocks.
// The reason is that noobs deserve a chance to get on their feet.
// MissedBlock() deactivates any who have a net 0 blocks after
// seeing at least 4x QP_STAKER_RECENT_BLOCKS (QP_NOOB_BLOCKS), so the
// opportunity to miss a lot of blocks is limited.
unsigned int QPStaker::GetWeight(unsigned int nSeniority) const
{
    uint32_t net = GetNetBlocks();
    return (net > 0) ? uisqrt(net + (nSeniority * nSeniority)) : nSeniority;
}

uint32_t QPStaker::GetDelegatePayout() const
{
    return nPcmDelegatePayout;
}

bool QPStaker::IsProductive() const
{
    return GetTotalEarned() > 0;
}

bool QPStaker::IsEnabled() const
{
    return fQualified && !nHeightDisabled;
}

bool QPStaker::IsDisabled() const
{
    return nHeightDisabled || !fQualified;
}

bool QPStaker::IsQualified() const
{
    return fQualified;
}

bool QPStaker::IsDisqualified() const
{
    return !fQualified;
}

bool QPStaker::ShouldBeDisabled(int nHeight) const
{
    unsigned int nMaxMiss = GetStakerMaxMisses(nHeight);
    unsigned int nPrevMaxMiss = nMaxMiss / 2;
    bool fDisable = true;
    for (unsigned int i=0; i < nMaxMiss; ++i)
    {
       if (bRecentBlocks[i])
       {
          fDisable = false;
          break;
       }
    }
    if (fDisable)
    {
        return true;
    }
    fDisable = true;
    for (unsigned int i=0; i < nPrevMaxMiss; ++i)
    {
       if (bPrevRecentBlocks[i])
       {
          fDisable = false;
          break;
       }
    }
    return fDisable;
}

bool QPStaker::ShouldBeDisqualified(uint32_t nPrevRecentBlocksMissedMax) const
{
    return (GetPrevRecentBlocksMissed() > nPrevRecentBlocksMissedMax) ||
           ((nBlocksSeen > QP_NOOB_BLOCKS) &&
            ((GetNetBlocks() <= 0) ||
             (GetRecentBlocksMissed() > (QP_STAKER_RECENT_BLOCKS / 2))));
}

int64_t QPStaker::GetTotalEarned() const
{
    return nTotalEarned;
}

string QPStaker::GetAlias() const
{
    return sAlias;
}

bool QPStaker::HasMeta(const string &key) const
{
    return (mapMeta.count(key) != 0);
}

bool QPStaker::GetMeta(const string &key, string &valueRet) const
{
    bool fResult;
    map<string, string>::const_iterator it = mapMeta.find(key);
    if (it == mapMeta.end())
    {
        valueRet.clear();
        fResult = false;
    }
    else
    {
        valueRet = it->second;
        fResult = true;
    }
    return fResult;
}

void QPStaker::CopyMeta(map<string, string> &mapRet) const
{
    mapRet = mapMeta;
}


void QPStaker::AsJSON(unsigned int nID,
                      unsigned int nSeniority,
                      Object &objRet,
                      bool fWithRecentBlocks,
                      unsigned int nNftID) const
{
    objRet.clear();
    Object objKeys;
    objKeys.push_back(Pair("owner_key", HexStr(pubkeyOwner.Raw())));
    objKeys.push_back(Pair("manager_key", HexStr(pubkeyManager.Raw())));
    objKeys.push_back(Pair("delegate_key", HexStr(pubkeyDelegate.Raw())));
    objKeys.push_back(Pair("controller_key", HexStr(pubkeyController.Raw())));

    Object objMeta;
    map<string, string>::const_iterator mit;
    for (mit = mapMeta.begin(); mit != mapMeta.end(); ++mit)
    {
        objMeta.push_back(Pair(mit->first, mit->second));
    }

    objRet.push_back(Pair("alias", sAlias));
    objRet.push_back(Pair("id", static_cast<int64_t>(nID)));
    objRet.push_back(Pair("version", nVersion));
    Object objBlockCreated;
    BlockAsJSONLite(GetBlockCreated(), objBlockCreated);
    objRet.push_back(Pair("block_created", objBlockCreated));
    objRet.push_back(Pair("txid_created", hashTxCreated.GetHex()));
    objRet.push_back(Pair("vout_created", static_cast<int64_t>(nOutCreated)));
    objRet.push_back(Pair("price", ValueFromAmount(nPrice)));
    objRet.push_back(Pair("qualified", fQualified));
    if (nHeightDisabled)
    {
        objRet.push_back(Pair("enabled", false));
        objRet.push_back(Pair("height_disabled",
                              static_cast<int64_t>(nHeightDisabled)));
        objRet.push_back(Pair("enable_height",
                              static_cast<int64_t>(GetEnableHeight(nBestHeight))));
    }
    else
    {
        objRet.push_back(Pair("enabled", true));
    }
    objRet.push_back(Pair("weight",
                          static_cast<int64_t>(GetWeight(nSeniority))));
    objRet.push_back(Pair("keys", objKeys));
    objRet.push_back(Pair("delegate_payout_pcm",
                          static_cast<int64_t>(nPcmDelegatePayout)));
    objRet.push_back(Pair("total_earned", ValueFromAmount(nTotalEarned)));
    objRet.push_back(Pair("blocks_produced",
                          static_cast<int64_t>(nBlocksProduced)));
    objRet.push_back(Pair("blocks_missed",
                          static_cast<int64_t>(nBlocksMissed)));
    objRet.push_back(Pair("blocks_docked",
                          static_cast<int64_t>(nBlocksDocked)));
    objRet.push_back(Pair("blocks_assigned",
                          static_cast<int64_t>(nBlocksAssigned)));
    objRet.push_back(Pair("blocks_seen",
                          static_cast<int64_t>(nBlocksSeen)));
    objRet.push_back(Pair("prev_blocks_missed",
                          static_cast<int64_t>(nPrevBlocksMissed)));

    double dProductivity = 0;
    if ((nBlocksProduced + nBlocksDocked) > 0)
    {
        dProductivity = (static_cast<double>(nBlocksProduced) /
                         (static_cast<double>(nBlocksProduced) +
                          static_cast<double>(nBlocksDocked))) * 100;
        objRet.push_back(Pair("percent_productivity",
                              strprintf("%0.4f", dProductivity)));
    }
    else
    {
        objRet.push_back(Pair("percent_productivity", "0.0000"));
    }

    objRet.push_back(Pair("latest_assignment",
                          bool(bRecentBlocks[0])));

    if (hashBlockMostRecent != 0)
    {
        Object objBlockMR;
        BlockAsJSONLite(GetBlockMostRecent(), objBlockMR);
        objRet.push_back(Pair("most_recent_block", objBlockMR));
    }

    if (!objMeta.empty())
    {
        objRet.push_back(Pair("meta", objMeta));
    }

    if (nNftID && mapNfts.count(nNftID))
    {
        Object objNft;
        mapNfts[nNftID].AsJSON(objNft);
        objNft.push_back(Pair("character_id", static_cast<int64_t>(nNftID)));
        objNft.push_back(Pair("owner_alias", sAlias));
        objNft.push_back(Pair("owner_id", static_cast<int64_t>(nID)));
        objRet.push_back(Pair("character", objNft));
    }

    if (fWithRecentBlocks)
    {
        objRet.push_back(Pair("recent_blocks",
                              BitsetAsHex(bRecentBlocks)));
        objRet.push_back(Pair("prev_recent_blocks",
                              BitsetAsHex(bPrevRecentBlocks)));
    }
}

void QPStaker::ProducedBlock(const uint256 *const phashBlock,
                             int64_t nBlockReward,
                             bool fPrevDidProduceBlock,
                             int64_t& nOwnerRewardRet,
                             int64_t& nDelegateRewardRet)
{
    nBlocksProduced += 1;
    nBlocksAssigned += 1;
    hashBlockMostRecent = *phashBlock;
    bRecentBlocks <<= 1;        // FIXME: use modular
    bRecentBlocks[0] = true;    // FIXME: use modular

    nPcmDelegatePayout = min(100000u, nPcmDelegatePayout);

    // FIXME: following test should be delegate v. manager
    if ((nPcmDelegatePayout == 0) || (pubkeyDelegate == pubkeyOwner))
    {
       nDelegateRewardRet = 0;
       nOwnerRewardRet = nBlockReward;
    }
    else
    {
       nDelegateRewardRet = (nBlockReward * nPcmDelegatePayout) / 100000;
       nOwnerRewardRet = nBlockReward - nDelegateRewardRet;
    }
    nTotalEarned += nBlockReward;
    UpdatePrevRecentBlocks(fPrevDidProduceBlock);
}

void QPStaker::MissedBlock(bool fPrevDidProduceBlock, int nFork)
{
    nBlocksAssigned += 1;
    nBlocksMissed += 1;
    if (nFork >= XST_FORKMISSFIX)
    {
        nBlocksDocked += 1;
    }
    bRecentBlocks <<= 1;  // FIXME: use modular
    UpdatePrevRecentBlocks(fPrevDidProduceBlock);
}

void QPStaker::SawBlock()
{
    nBlocksSeen += 1;
}

void QPStaker::UpdatePrevRecentBlocks(bool fPrevDidProduceBlock)
{
    bPrevRecentBlocks <<= 1;  // FIXME: use modular
    if (fPrevDidProduceBlock)
    {
        bPrevRecentBlocks[0] = true;
    }
    else
    {
        nPrevBlocksMissed += 1;
    }
}

bool QPStaker::SetDelegatePayout(uint32_t pcm)
{
    bool result = true;
    if (pcm > 100000)
    {
        nPcmDelegatePayout = 100000;
        result = false;
    }
    else
    {
       nPcmDelegatePayout = pcm;
    }
    return result;
}

int QPStaker::GetEnableHeight(int nHeight) const
{
    if (fTestNet)
    {
        if (GetFork(nHeight) < XST_FORKMISSFIX)
        {
            return 0;
        }
        return (nHeightDisabled + QP_STAKER_REENABLE_WAIT_T);
    }
    if (!nHeightDisabled)
    {
        return 0;
    }
    return (nHeightDisabled + QP_STAKER_REENABLE_WAIT_M);
}

bool QPStaker::CanBeEnabled(int nHeight) const
{
    if (nHeight >= GetEnableHeight(nHeight))
    {
        return true;
    }
    return false;
}

bool QPStaker::Enable()
{
    if (IsDisqualified())
    {
        return error("Enable(): can not enable a disqualified staker");
    }
    nHeightDisabled = 0;
    return true;
}

void QPStaker::Disable(int nHeight)
{
    nHeightDisabled = nHeight;
}

void QPStaker::Disqualify()
{
    nHeightDisabled = std::numeric_limits<int>::max() / 2;
    fQualified = false;
}

void QPStaker::Requalify(bool fEnable)
{
    if (fEnable)
    {
        nHeightDisabled = 0;
    }
    else
    {
        nHeightDisabled = -1;
    }
    fQualified = true;
}

bool QPStaker::SetAlias(const string &sAliasIn)
{
    if (sAlias != "")
    {
        return error("SetAlias(): Staker alias already %s", sAlias.c_str());
    }
    sAlias = sAliasIn;
    return true;
}

void QPStaker::SetMeta(const string &key, const string &value)
{
    if (value.empty())
    {
        mapMeta.erase(key);
    }
    else
    {
        mapMeta[key] = value;
    }
}

void QPStaker::ResetDocked()
{
    nBlocksDocked = 0;
}
