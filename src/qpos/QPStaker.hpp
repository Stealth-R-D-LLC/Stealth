// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPSTAKER_H_
#define _QPSTAKER_H_ 1

#include "QPConstants.hpp"
#include "QPTxDetails.hpp"

#include "key.h"
#include "serialize.h"

#include "json/json_spirit_utils.h"

#include <bitset>

typedef std::bitset<QP_STAKER_RECENT_BLOCKS> QPRecentBlocks;

struct qpos_authorities
{
    CPubKey owner;
    CPubKey manager;
    CPubKey delegate;
    CPubKey controller;
};

class CBlockIndex;

class QPStaker
{
private:
    int nVersion;
    QPRecentBlocks bRecentBlocks;
    QPRecentBlocks bPrevRecentBlocks;
    uint256 hashBlockCreated;
    uint256 hashTxCreated;
    unsigned int nOutCreated;
    int64_t nPrice;
    uint32_t nBlocksProduced;
    uint32_t nBlocksMissed;
    uint32_t nBlocksDocked;
    uint32_t nBlocksAssigned;
    uint32_t nBlocksSeen;
    uint256 hashBlockMostRecent;
    uint32_t nPrevBlocksMissed;
    uint32_t nPcmDelegatePayout;
    int nHeightDisabled;
    bool fQualified;
    int64_t nTotalEarned;
    std::string sAlias;
    std::map<std::string, std::string> mapMeta;
public:
    static const int QPOS_VERSION = 1;
    static const int CURRENT_VERSION = QPOS_VERSION;

    CPubKey pubkeyOwner;
    CPubKey pubkeyManager;
    CPubKey pubkeyDelegate;
    CPubKey pubkeyController;
    QPStaker();
    QPStaker(const QPTxDetails& deet);
    void Reset();
    const QPRecentBlocks& GetRecentBlocks() const;
    const QPRecentBlocks& GetPrevRecentBlocks() const;
    uint32_t GetRecentBlocksProduced() const;
    uint32_t GetPrevRecentBlocksProduced() const;
    uint256 GetHashBlockCreated() const;
    const CBlockIndex* GetBlockCreated() const;
    uint256 GetHashTxCreated() const;
    unsigned int GetNOutCreated() const;
    int64_t GetPrice() const;
    uint32_t GetRecentBlocksMissed() const;
    uint32_t GetPrevRecentBlocksMissed() const;
    uint32_t GetBlocksProduced() const;
    uint32_t GetBlocksMissed() const;
    uint32_t GetBlocksDocked() const;
    uint32_t GetBlocksAssigned() const;
    uint32_t GetBlocksSeen() const;
    uint32_t GetNetBlocks() const;
    uint256 GetHashBlockMostRecent() const;
    const CBlockIndex* GetBlockMostRecent() const;
    bool DidMissMostRecentBlock() const;
    bool DidProduceMostRecentBlock() const;
    unsigned int GetWeight(unsigned int nSeniority) const;
    uint32_t GetDelegatePayout() const;
    bool IsProductive() const;
    bool IsEnabled() const;
    bool IsDisabled() const;
    bool IsQualified() const;
    bool IsDisqualified() const;
    bool ShouldBeDisabled(int nHeight) const;
    bool ShouldBeDisqualified(uint32_t nPrevRecentBlocksMissedMax) const;
    int64_t GetTotalEarned() const;
    std::string GetAlias() const;
    bool HasMeta(const std::string &key) const;
    bool GetMeta(const std::string &key, std::string &valueRet) const;
    void CopyMeta(std::map<std::string, std::string> &mapRet) const;

    void AsJSON(unsigned int nID,
                unsigned int nSeniority,
                json_spirit::Object &objRet,
                bool fWithRecentBlocks=false,
                unsigned int nNftID=0) const;

    void ProducedBlock(const uint256 *const phashBlock,
                       int64_t nBlockReward,
                       bool fPrevDidProduceBlock,
                       int64_t& nReward,
                       int64_t& nDelegateReward);
    void MissedBlock(bool fPrevDidProduceBlock, int nFork);
    void SawBlock();
    void UpdatePrevRecentBlocks(bool fPrevDidProduceBlock);
    bool SetDelegatePayout(uint32_t pcm);
    int GetEnableHeight(int nHeight) const;
    bool CanBeEnabled(int nHeight) const;
    bool Enable();
    void Disable(int nHeight);
    void Disqualify();
    void Requalify(bool fEnable);
    bool SetAlias(const std::string &sAliasIn);
    void SetMeta(const std::string &key, const std::string &value);
    void ResetDocked();


    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nSerVersion = this->nVersion;
        READWRITE(bRecentBlocks);
        READWRITE(bPrevRecentBlocks);
        READWRITE(hashBlockCreated);
        READWRITE(hashTxCreated);
        READWRITE(nOutCreated);
        READWRITE(nPrice);
        READWRITE(nBlocksProduced);
        READWRITE(nBlocksMissed);
        READWRITE(nBlocksDocked);
        READWRITE(nBlocksAssigned);
        READWRITE(nBlocksSeen);
        READWRITE(hashBlockMostRecent);
        READWRITE(nPrevBlocksMissed);
        READWRITE(nPcmDelegatePayout);
        READWRITE(nHeightDisabled);
        READWRITE(fQualified);
        READWRITE(nTotalEarned);
        READWRITE(sAlias);
        READWRITE(pubkeyOwner);
        READWRITE(pubkeyManager);
        READWRITE(pubkeyDelegate);
        READWRITE(pubkeyController);
        READWRITE(mapMeta);
    )
};

#endif  /* _QPSTAKER_H_ */
