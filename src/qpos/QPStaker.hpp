// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPSTAKER_H_
#define _QPSTAKER_H_ 1

#include "QPConstants.hpp"
#include "key.h"
#include "serialize.h"

#include "json/json_spirit_utils.h"

#include <bitset>


class QPStaker
{
private:
    int nVersion;
    std::bitset<QP_RECENT_BLOCKS> bRecentBlocks;
    std::bitset<QP_RECENT_BLOCKS> bPrevRecentBlocks;
    uint32_t nBlocksProduced;
    uint32_t nBlocksMissed;
    uint32_t nBlocksAssigned;
    uint32_t nBlocksSeen;
    uint32_t nPrevBlocksMissed;
    uint32_t nPcmDelegatePayout;
    bool fEnabled;
    bool fDisqualified;
    int64_t nTotalEarned;
    std::string sAlias;
    std::map<std::string, std::string> mapMeta;
public:
    static const int QPOS_VERSION = 1;
    static const int CURRENT_VERSION = QPOS_VERSION;

    CPubKey pubkeyOwner;
    CPubKey pubkeyDelegate;
    CPubKey pubkeyController;
    QPStaker();
    QPStaker(CPubKey pubkeyIn);
    void Reset();
    uint32_t GetRecentBlocksProduced() const;
    uint32_t GetPrevRecentBlocksProduced() const;
    uint32_t GetRecentBlocksMissed() const;
    uint32_t GetPrevRecentBlocksMissed() const;
    uint32_t GetBlocksProduced() const;
    uint32_t GetBlocksMissed() const;
    uint32_t GetBlocksAssigned() const;
    uint32_t GetBlocksSeen() const;
    uint32_t GetNetBlocks() const;
    unsigned int GetWeight(unsigned int nSeniority) const;
    uint32_t GetDelegatePayout() const;
    bool IsEnabled() const;
    bool IsDisqualified() const;
    bool ShouldBeDisqualified(uint32_t nPrevRecentBlocksMissedMax) const;
    int64_t GetTotalEarned() const;
    std::string GetAlias() const;
    bool HasMeta(const std::string &key) const;
    bool GetMeta(const std::string &key, std::string &valueRet) const;
    void CopyMeta(std::map<std::string, std::string> &mapRet) const;

    void AsJSON(unsigned int nID,
                unsigned int nSeniority,
                json_spirit::Object &objRet,
                bool fWithRecentBlocks=false) const;

    void ProducedBlock(int64_t nBlockReward,
                       bool fPrevDidProduceBlock,
                       int64_t& nReward,
                       int64_t& nDelegateReward);
    void MissedBlock(bool fPrevDidProduceBlock);
    void SawBlock();
    void UpdatePrevRecentBlocks(bool fPrevDidProduceBlock);
    bool SetDelegatePayout(uint32_t pcm);
    bool Enable();
    void Disable();
    void Disqualify();
    bool SetAlias(const std::string &sAliasIn);
    void SetMeta(const std::string &key, const std::string &value);


    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(bRecentBlocks);
        READWRITE(bPrevRecentBlocks);
        READWRITE(nBlocksProduced);
        READWRITE(nBlocksMissed);
        READWRITE(nBlocksAssigned);
        READWRITE(nBlocksSeen);
        READWRITE(nPrevBlocksMissed);
        READWRITE(nPcmDelegatePayout);
        READWRITE(fEnabled);
        READWRITE(fDisqualified);
        READWRITE(nTotalEarned);
        READWRITE(sAlias);
        READWRITE(pubkeyOwner);
        READWRITE(pubkeyDelegate);
        READWRITE(pubkeyController);
        READWRITE(mapMeta);
    )
};

#endif  /* _QPSTAKER_H_ */
