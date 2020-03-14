// Copyright (c) 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CHAINPARAMS_H
#define CHAINPARAMS_H

#include "math.h"
#include "bignum.h"

#include <map>

class ChainParams;

typedef std::map<int, int> mapIntInt_t;
typedef std::map<int, unsigned int> mapIntUInt_t;
typedef std::map<int, uint256> mapIntUInt256_t;

// borrowed from https://tinyurl.com/wmual38
// fixes assignment collision with newer c++11 compilers
template<typename T>
mapIntInt_t MakeMapIntInt(const T& mapInit)
{
    mapIntInt_t m = mapInit;
    return m;
}

template<typename T>
mapIntUInt_t MakeMapIntUInt(const T& mapInit)
{
    mapIntUInt_t m = mapInit;
    return m;
}

template<typename T>
mapIntUInt256_t MakeMapIntUInt256(const T& mapInit)
{
    mapIntUInt256_t m = mapInit;
    return m;
}


extern const ChainParams chainParams;

class ChainParams
{
public:

    ChainParams();

    //////////////////////////////////////////////////////////////////////////////
    // Forks

    mapIntInt_t mapProtocolVersions;

    int CUTOFF_POW_M;
    int START_PURCHASE_M;
    int CUTOFF_POS_M;

    mapIntInt_t mapForksMainNet;


    //////////////////////////////////////////////////////////////////////////////
    // Network

    unsigned char pchMessageStartMainNet[4];

    std::string strMainKey;


    //////////////////////////////////////////////////////////////////////////////
    // Chain

    unsigned int MAX_BLOCK_SIZE;
    unsigned int MAX_BLOCK_SIZE_GEN;
    unsigned int MAX_STANDARD_TX_SIZE;
    unsigned int MAX_BLOCK_SIGOPS;
    unsigned int MAX_ORPHAN_TRANSACTIONS;
    unsigned int MAX_INV_SZ;
    int64_t MIN_TX_FEE;
    int64_t MIN_RELAY_TX_FEE;
    int64_t MIN_TXOUT_AMOUNT;

    int64_t MAX_MONEY;
    int64_t CIRCULATION_MONEY;
    double TAX_PERCENTAGE;

    int64_t nMaxClockDrift;

    int64_t FUTURE_DRIFT_MAINNET;

    CBigNum bnProofOfWorkLimitMainNet;

    // 60 sec block spacing
    unsigned int nTargetSpacingMainNet;

    int nCoinbaseMaturityMainNet;

    std::string strMessageMagic;


    //////////////////////////////////////////////////////////////////////////////
    //
    // Genesis block
    //

    uint256 hashGenesisBlockMainNet;

    int64_t nChainStartTime;

    int nIgma;
    CBigNum bnIgma;

    unsigned int nTimeGenesisBlock;

    unsigned int nNonceGenesisBlock;

    std::string strTimestamp;

    uint256 hashMerkleRootMainNet;


    //////////////////////////////////////////////////////////////////////////////
    // PoS

    unsigned int MODIFIER_INTERVAL_MAINNET;

    int MODIFIER_INTERVAL_RATIO_MAINNET;

    int64_t MAX_STEALTH_PROOF_OF_STAKE_MAINNET;

    CBigNum MAX_COIN_SECONDS;

    CBigNum bnProofOfStakeLimitMainNet;

    unsigned int nStakeMinAgeMainNet;
    unsigned int nStakeMaxAgeMainNet;


    //////////////////////////////////////////////////////////////////////////////
    // Checkpoints

    int CHECKPOINT_MAX_SPAN;

    mapIntUInt256_t mapCheckpointsMainNet;

    mapIntUInt_t mapStakeModifierCheckpoints;

    std::string strCheckpointMasterPubKey;


    //////////////////////////////////////////////////////////////////////////////
    // Client

    uint64_t nMinDiskSpace;

    int GETBLOCKS_LIMIT;

    int MAX_OUTBOUND_CONNECTIONS;

    std::string DEFAULT_CONF;
    std::string DEFAULT_PID;
    int DEFAULT_DBCACHE;
    int DEFAULT_DBLOGSIZE;
    int DEFAULT_TIMEOUT;
    int DEFAULT_PORT_MAINNET;
    int DEFAULT_PROXY_MAINNET;
    int DEFAULT_TORPORT;
    int DEFAULT_MAXCONNECTIONS;
    int DEFAULT_BANSCORE;
    int DEFAULT_MAXRECEIVEBUFFER;
    int DEFAULT_MAXSENDBUFFER;
    int DEFAULT_BANTIME;
    int DEFAULT_RPCPORT_MAINNET;
    int DEFAULT_KEYPOOL;
    int DEFAULT_CHECKBLOCKS;
    int DEFAULT_CHECKLEVEL;
    int DEFAULT_BLOCKMINSIZE;
    int DEFAULT_BLOCKMAXSIZE;
    int DEFAULT_BLOCKPRIORITYSIZE;
    int64_t DEFAULT_MAXDUST;

    float MAX_TXFEE;


    //////////////////////////////////////////////////////////////////////////////
    // TestNet

    int CUTOFF_POW_T;
    int CUTOFF_POS_T;
    int START_PURCHASE_T;

    mapIntInt_t mapForksTestNet;

    unsigned char pchMessageStartTestNet[4];

    uint256 hashGenesisBlockTestNet;

    uint256 hashMerkleRootTestNet;

    int64_t MAX_STEALTH_PROOF_OF_STAKE_TESTNET;

    int nCoinbaseMaturityTestNet;

    unsigned int nStakeMinAgeTestNet;
    unsigned int nStakeMaxAgeTestNet;

    unsigned int MODIFIER_INTERVAL_TESTNET;
    int MODIFIER_INTERVAL_RATIO_TESTNET;

    CBigNum bnProofOfWorkLimitTestNet;

    CBigNum bnProofOfStakeLimitTestNet;

    unsigned int nTargetSpacingTestNet;

    // TestNet alerts pubKey
    std::string strTestKey;

    mapIntUInt256_t mapCheckpointsTestNet;

    int DEFAULT_PORT_TESTNET;

    int DEFAULT_PROXY_TESTNET;

    int DEFAULT_RPCPORT_TESTNET;

    int64_t FUTURE_DRIFT_TESTNET;
};


#endif  // CHAINPARAMS_H
