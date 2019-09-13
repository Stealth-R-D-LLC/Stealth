// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "QPStaker.hpp"

using namespace json_spirit;
using namespace std;

extern Value ValueFromAmount(int64_t amount);

QPStaker::QPStaker()
{
    pubkeyOwner = CPubKey();
    pubkeyDelegate = CPubKey();
    pubkeyController = CPubKey();
    Reset();
}

QPStaker::QPStaker(CPubKey pubkeyIn)
{
    pubkeyOwner = pubkeyIn;
    pubkeyDelegate = pubkeyIn;
    pubkeyController = pubkeyIn;
    Reset();
}

void QPStaker::Reset()
{
    nVersion = QPStaker::CURRENT_VERSION;
    nBlocksProduced = 0;
    nBlocksMissed = 0;
    nBlocksAssigned = 0;
    nBlocksSeen = 0;
    nPrevBlocksMissed = 0;
    nPcmDelegatePayout = 0;
    fEnabled = false;
    fDisqualified = false;
    nTotalEarned = 0;
    sAlias = "";
    mapMeta.clear();
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
    uint32_t m = QP_RECENT_BLOCKS - GetRecentBlocksProduced();
    return min(nBlocksMissed, m);
}

uint32_t QPStaker::GetPrevRecentBlocksMissed() const
{
    uint32_t m = QP_RECENT_BLOCKS - GetPrevRecentBlocksProduced();
    return min(nPrevBlocksMissed, m);
}

uint32_t QPStaker::GetBlocksProduced() const
{
    return nBlocksProduced;
}

uint32_t QPStaker::GetBlocksMissed() const
{
    return nBlocksMissed;
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
    if (nBlocksMissed >= nBlocksProduced)
    {
        return 0;
    }
    return nBlocksProduced - nBlocksMissed;
}

// Returns 1 even when missed blocks outnumber produced blocks.
// The reason is that noobs deserve a chance to get on their feet.
// MissedBlock() deactivates any who have a net 0 blocks after
// seeing at least 4x QP_RECENT_BLOCKS (QP_NOOB_BLOCKS), so the
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

bool QPStaker::IsEnabled() const
{
    return fEnabled;
}

bool QPStaker::IsDisqualified() const
{
    return fDisqualified;
}


bool QPStaker::ShouldBeDisqualified(uint32_t nPrevRecentBlocksMissedMax) const
{
    return (GetPrevRecentBlocksMissed() > nPrevRecentBlocksMissedMax) ||
           ((nBlocksSeen > QP_NOOB_BLOCKS) &&
            ((GetNetBlocks() <= 0) ||
             (GetRecentBlocksMissed() > (QP_RECENT_BLOCKS / 2))));
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
                      bool fWithRecentBlocks) const
{
    Object objKeys;
    objKeys.push_back(Pair("owner_key", HexStr(pubkeyOwner.Raw())));
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
    objRet.push_back(Pair("qualified", !fDisqualified));
    objRet.push_back(Pair("enabled", fEnabled));
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
    objRet.push_back(Pair("blocks_assigned",
                          static_cast<int64_t>(nBlocksAssigned)));
    objRet.push_back(Pair("blocks_seen",
                          static_cast<int64_t>(nBlocksSeen)));
    objRet.push_back(Pair("prev_blocks_missed",
                          static_cast<int64_t>(nPrevBlocksMissed)));

    if (!objMeta.empty())
    {
        objRet.push_back(Pair("meta", objMeta));
    }

    if (fWithRecentBlocks)
    {
        Array aryRecentBlocks, aryPrevRecentBlocks;
        for (unsigned int i = 0; i < QP_RECENT_BLOCKS; ++i)
        {
            aryRecentBlocks.push_back((int)bRecentBlocks[i]);
            aryPrevRecentBlocks.push_back((int)bPrevRecentBlocks[i]);
        }
        objRet.push_back(Pair("recent_blocks", aryRecentBlocks));
        objRet.push_back(Pair("prev_recent_blocks", aryPrevRecentBlocks));
    }
}

void QPStaker::ProducedBlock(int64_t nBlockReward,
                             bool fPrevDidProduceBlock,
                             int64_t& nOwnerRewardRet,
                             int64_t& nDelegateRewardRet)
{
    nBlocksProduced += 1;
    nBlocksAssigned += 1;
    bRecentBlocks <<= 1;        // FIXME: use modular
    bRecentBlocks[0] = true;    // FIXME: use modular

    nPcmDelegatePayout = min(100000u, nPcmDelegatePayout);

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

void QPStaker::MissedBlock(bool fPrevDidProduceBlock)
{
    nBlocksAssigned += 1;
    nBlocksMissed += 1;
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

bool QPStaker::Enable()
{
    if (IsDisqualified())
    {
        return error("Enable(): can not enable a disqualified staker");
    }
    fEnabled = true;
    return true;
}

void QPStaker::Disable()
{
    fEnabled = false;
}

void QPStaker::Disqualify()
{
    fEnabled = false;
    fDisqualified = true;
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
