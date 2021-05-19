// Copyright (c) 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.hpp"

#include <boost/assign/list_of.hpp>


using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// forks
//

int GetFork(int nHeight)
{
    static const map<int, int> mapForks = fTestNet ? chainParams.mapForksTestNet :
                                                     chainParams.mapForksMainNet;
    static const map<int, int>::const_iterator b = mapForks.begin();
    static const map<int, int>::const_iterator e = mapForks.end();
    assert (b != e);

    int nFork = b->second;
    // loop has strange logic, but if fork i height is greater than nHeight
    // then you are on fork i-1
    // we can do it this way because maps are sorted
    for (map<int, int>::const_iterator it = b; it != e; ++it)
    {
       if (it->first > nHeight)
       {
           break;
       }
       nFork = it->second;
    }
    return nFork;
}

ChainParams::ChainParams()
{
    //////////////////////////////////////////////////////////////////////////////
    //
    // Forks
    //

    // Make sure forks are ascending!

    // MinPeerProtocol
    mapProtocolVersions =  MakeMapIntInt(
        boost::assign::map_list_of
        //                            Protocol
        //                     Fork,   Version
        (               XST_GENESIS,     62020 )
        (               XST_FORK005,     62100 )
        (               XST_FORK006,     62200 )
        (          XST_FORKPURCHASE,     63000 )
        (         XST_FORKPURCHASE2,     63500 )
        (         XST_FORKPURCHASE3,     63600 )
        (              XST_FORKQPOS,     63600 )
        (             XST_FORKQPOSB,     63600 )
        (           XST_FORKFEELESS,     63600 )
        (           XST_FORKMISSFIX,     63600 )
               );

    CUTOFF_POW_M = 5460;
    START_PURCHASE_M = 3657600;  // original start of staker purchases
    START_PURCHASE2_M = 3673500;  // new start of staker purchases
    START_PURCHASE3_M = 3683900;  // purchase fix for NFTs
    START_QPOS_M = 3695100;      // end of PoS and start of qPoS
    START_QPOSB_M = 3695100;      // placeholder for fork on testnet
    START_NFT_M = 3695200;       // add NFT commitment to blockchain
    START_FEELESS_M = 3702300;   // start of feeless transactions
    START_MISSFIX_M = 3702301;   // placeholder for fork on testnet

    mapForksMainNet = MakeMapIntInt(
        boost::assign::map_list_of
        /* MAIN NET */                  /*            Height, Fork Number       */
        /* Jul  4 02:47:04 MST 2014 */   (                 0, XST_GENESIS       )
        /* Jul 11 18:33:08 MST 2014 */   (      CUTOFF_POW_M, XST_FORK002       )
        /* Oct  9 00:00:42 MST 2014 */   (            130669, XST_FORK004       )
        /* Aug 16 10:23:28 MST 2017 */   (           1732201, XST_FORK005       )
        /* Nov 14 08:09:53 MDT 2018 */   (           2378000, XST_FORK006       )
        /* Approx May    4, 2021    */   (  START_PURCHASE_M, XST_FORKPURCHASE  )
        /* Approx May   16, 2021    */   ( START_PURCHASE2_M, XST_FORKPURCHASE2 )
        /* Approx May   23, 2021    */   ( START_PURCHASE3_M, XST_FORKPURCHASE3 )
        /* Approx May   31, 2021    */   (      START_QPOS_M, XST_FORKQPOS      )
        /* Approx May   31, 2021    */   (     START_QPOSB_M, XST_FORKQPOSB     )
        /* Approx May   31, 2021    */   (       START_NFT_M, XST_FORKNFT       )
        /* Approx June   1, 2021    */   (   START_FEELESS_M, XST_FORKFEELESS   )
        /* Approx June   1, 2021    */   (   START_MISSFIX_M, XST_FORKMISSFIX   )
                                                 );


    //////////////////////////////////////////////////////////////////////////////
    //
    // Network
    //

    // The message start string is designed to be unlikely to occur in normal data.
    // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
    // a large 4-byte int at any alignment.
    pchMessageStartMainNet[0] = 0x70;
    pchMessageStartMainNet[1] = 0x35;
    pchMessageStartMainNet[2] = 0x22;
    pchMessageStartMainNet[3] = 0x05;

    // Alert pubkey
    strMainKey = "04bf2dc2aac67e7d58365a3aa68946"
                 "e40026cd5c97a2606f41991b9614a0"
                 "818c905c28b344e3bbbb2cb072ba18"
                 "0a8974bba93d8bdc53bca40418bd87"
                 "9a733bb064";

    //////////////////////////////////////////////////////////////////////////////
    //
    // Chain
    //

    MAX_BLOCK_SIZE = 1000000;
    MAX_BLOCK_SIZE_GEN = MAX_BLOCK_SIZE / 2;
    MAX_STANDARD_TX_SIZE = MAX_BLOCK_SIZE_GEN / 5;
    MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE / 50;
    MAX_ORPHAN_TRANSACTIONS = MAX_BLOCK_SIZE / 100;
    MAX_INV_SZ = 50000;
    MIN_TX_FEE = 1 * CENT;
    MIN_RELAY_TX_FEE = 1 * CENT;
    MIN_TXOUT_AMOUNT = MIN_TX_FEE;

    // MAX_MONEY is for consistency checking
    // The actual PoW coins is about 23,300,000
    // This number will go up over time.
    // Code Reviewers: Don't look here for the money supply limit!!!!!!
    // Go look at GetProofOfWorkReward and do the math yourself
    // Don't quote the value of MAX_MONEY as an indicator
    // of the true money supply or you will look foolish!!!!!!
    // By coincidence, original max money of 43.3M is good for almost
    // exactly 1 decade after transition to qPoS (1% per year inflation).
    MAX_MONEY = COIN * 43300000;  // 23.3 Million -> 43.3 bumped to be safe
    CIRCULATION_MONEY = MAX_MONEY;
    TAX_PERCENTAGE = 0.00; // no tax

    // two hours
    nMaxClockDrift = 2 * 60 * 60;

    // PoW/PoS: 2 hr (120 blocks on mainnet)
    LATEST_INITIAL_BLOCK_DOWNLOAD_TIME = 2 * 60 * 60;
    // qPoS: 6 min (72 blocks)
    LATEST_INITIAL_BLOCK_DOWNLOAD_TIME_QPOS = 6 * 60;

    FUTURE_DRIFT_MAINNET = 17;

    bnProofOfWorkLimitMainNet = CBigNum(~uint256(0) >> 20);

    // 60 sec block spacing
    nTargetSpacingMainNet = 60;

    nCoinbaseMaturityMainNet = 40;

    strMessageMagic = "StealthCoin Signed Message:\n";


    //////////////////////////////////////////////////////////////////////////////
    //
    // Genesis block
    //

    hashGenesisBlockMainNet = uint256(
       "0x1aaa07c5805c4ea8aee33c9f16a057215bc06d59f94fc12132c6135ed2d9712a");

    // timestamp of first transaction
    nChainStartTime = 1403684997;

    nIgma = 486604799;
    bnIgma = CBigNum(9999);

    // timestamp of genesis block
    nTimeGenesisBlock = 1403668979;

    // perfect nonce
    nNonceGenesisBlock = 4204204204LL;

    strTimestamp = "20140615 Stealth proves that pzTimestamp is overkill.";

    hashMerkleRootMainNet = uint256(
       "0xe3de7c386d5b82f62ff24c6d2351539c22b17c6ffab0e267b3cdd72fda82bd83");


    //////////////////////////////////////////////////////////////////////////////
    //
    // PoS
    //

    // 5 minutes
    // MODIFIER_INTERVAL: time to elapse before new modifier is computed
    MODIFIER_INTERVAL_MAINNET = 5 * 60;

    // MODIFIER_INTERVAL_RATIO:
    // ratio of group interval length between the last group and the first group
    MODIFIER_INTERVAL_RATIO_MAINNET = 3;

    // main net: 20% annual interest
    MAX_STEALTH_PROOF_OF_STAKE_MAINNET = 0.20 * COIN;

    // Maximum age of a coin, to encourage open wallet
    MAX_COIN_SECONDS = 9 * 24 * 60 * 60;  // 9 days

    bnProofOfStakeLimitMainNet = CBigNum(~uint256(0) >> 2);

    //minimum age for coin age:  3 day
    nStakeMinAgeMainNet = 60 * 60 * 24 * 3;
    //stake age of full weight:  9 day
    nStakeMaxAgeMainNet = 60 * 60 * 24 * 9;


    //////////////////////////////////////////////////////////////////////////////
    //
    // Feeless
    //

    // max of a unsigned 64 bit integer
    TX_FEEWORK_ABSOLUTE_LIMIT = 0xffffffffffffffffull;

    // max limit, decreasing limit means increasing difficulty
    TX_FEEWORK_LIMIT = 0x0006ffffffffffffull;
    RELAY_TX_FEEWORK_LIMIT = 0x0006ffffffffffffull;

    // number of steps of hardness/difficulty as block fills
    FEEWORK_BLOCK_PARTS = 31;

    // for dynamic memory hardness:
    //    each step requires 10% more memory than the previous
    //    max cost is about 17x min cost at 31 steps
    FEEWORK_COST_PCT_JUMP_PER_PART = 10;

    // for dynamic difficulty (decay of the max hash value, aka work limit)
    //    each step permits 91% lower hash than the previous step
    //    note that 91% ~ 1 / 1.10 (i.e. 10% difficulty jump each step)
    // Dynamic memory hardness and dynamic difficulty vary demand for different
    // resources but at the same rate. However, dynamic memory hardness depletes
    // relatively scarce resources on a GPU (memory) while dynamic difficulty
    // depletes relatively scarce resources on a CPU (computing cycles), making
    // dynamic memory hardness preferrable to stifle GPU-based spam.
    FEEWORK_LIMIT_PCT_DECAY_PER_PART = 91;


    FEELESS_WORKLEN = 8;
    FEELESS_HASHLEN = 8;

    FEELESS_TCOST = 1;                // 1-pass computation
    FEELESS_MCOST_MIN = 1<<8;         // 4 mebibytes memory usage
    RELAY_FEELESS_MCOST_MIN = 1<<8;   // 4 mebibytes memory usage
    // 1 thread makes it more likely that gpus will run out of memory before
    // running out of cores
    FEELESS_PARALLELISM = 1;   // 1 thread


    // 1. prevents larger feeless transactions at very full blocks
    // 2. keeps validation time under 3 ms on reasonable hardware
    // This cutoff should allow feeless for a simple tx in a 90% full block.
    FEEWORK_MAX_MULTIPLIER = 18;
    FEEWORK_MAX_MCOST = FEEWORK_MAX_MULTIPLIER * FEELESS_MCOST_MIN;

    // Feeless transactions reference a prior block in the best chain.
    // This block can be no more than the following depth:
    FEELESS_MAX_DEPTH = 24;

    // We reserve the last 4/5 block for money fee transactions,
    // so there is no way to spam blocks full with feeless transactions.
    FEELESS_MAX_BLOCK_SIZE = MAX_BLOCK_SIZE / 5;


    //////////////////////////////////////////////////////////////////////////////
    //
    // Checkpoints
    //

    // max 4 hours before latest block
    CHECKPOINT_MAX_SPAN = 60 * 60 * 4;

    // What makes a good checkpoint block?
    // + Is surrounded by blocks with reasonable timestamps
    //   (no blocks before with a timestamp after, none after with
    //    timestamp before)
    // + Contains no strange transactions
    mapCheckpointsMainNet = MakeMapIntUInt256(
        boost::assign::map_list_of
        (           0, hashGenesisBlockMainNet )
        (          15, uint256("0x00000b3b7e584a1de0e079652b1c0e4c3d7a72e2385b070c3a75f333335533a4"))
        (        4500, uint256("0x000000000029c93f05b4e139e426af1cb0cf4b94fe9e1911d1d26bea254a79f9"))
        (        4521, uint256("0x0000000000436eedb310de941903ff85be8827e1319200c93b1b9db7f22ef190"))
        (        4550, uint256("0x000000000084facef94dd16245a75275e5c2c26ff5a13ebad2469ec3da42c5a8"))
        (        4600, uint256("0x90c9e67baa7aad3ec9ca21b465c26eee2216c93be48e56655fefdd6f7582a113"))
        (        4650, uint256("0xaee22d2974526a4fb6a068013bed841699b6269490937cf62fa6bb3fec7acb2c"))
        (        4700, uint256("0x00000000002be5106442cbff52a2868c3f1fa3bbea4ff147696e62e2d1ce7746"))
        (        4750, uint256("0x00000000014b19ccf351ef277633134958a791d9faf337db4954410d5fbdc7d7"))
        (        4800, uint256("0x0000000001bdb29c93b4033ac7c907100a5e58bcdbe0f1b377f1f6c6812440a6"))
        (        4850, uint256("0x4fa8326b35cf85271c419f487fe530626044efd98be053ff1f84d2c585c24bcb"))
        (        4900, uint256("0x0000000000dd3d701b40d8485850c9adfd8a3408844b1caf3df79643f801bdde"))
        (        5112, uint256("0x00000000010655f2b03d26740e822f7307491e1997735a5d37e0ee88fedd21bb"))
        (        6920, uint256("0x72129f1943af505f450e32e815e783870e33bd26b990222dcb91a837194b0a96"))
        (       15000, uint256("0x53d630587fcce591fac0fd71955558914036d06da1664ae36d8fef0f7ddd0d23"))
        (       19000, uint256("0x211c68aba319ca333a18dd8e339f430bf538908e6bed32a501aac35fa446a855"))
        (       51100, uint256("0x2b5c8f77181948e88b793ab76dfd28b8483f047de5555905d9f6a7e058decf13"))
        (       67000, uint256("0xa08c5d7ae3a662b964969a6a05cbfdd76f26f6124c79f3f0feaf33497c748f32"))
        (       83500, uint256("0xdf42ca49b49683e6e6cf79228e072322eb80578f94dc01af08e068eabc7cb487"))
        (      100500, uint256("0x80749a6d5ed2ab45312c11206a355f81c9ec6e3effd2f835ec007caf40865c6f"))
        (      105000, uint256("0xacc3cc5ecf67849d7616f9f0f6e955e7288be2b93a614e0be52b7bd540327071"))
        (      347000, uint256("0x4aaa94b5b7018607a19301e7ba63d40cc3024f091c1bcffaf2b64ef0e1ac5bcb"))
        (      769000, uint256("0x7424eff7b5800cfc59e1420c4c32611bceaebb34959a1df93ca678bb5c614582"))
        (     2107000, uint256("0x3e1be0deee5db79f2de34ccdea3b3891cfb158902b1f8f8215c69c616d719eb1"))
           );

    // Hard checkpoints of stake modifiers to ensure they are deterministic
    mapStakeModifierCheckpoints = MakeMapIntUInt(
        boost::assign::map_list_of
        (           0, 0xfd11f4e7u )
        (      114200, 0xe2232be0u )
           );

    // stealth: sync-checkpoint master key (520 bits, 130 hex)
    // this is the public key for a secp256k1 private key
    // e.g.: openssl ecparam -name secp256k1 -genkey -noout -out private-key.pem
    //       openssl ec -in private-key.pem -pubout -text -out ecpubkey.txt
    // hex is the "pub:" section in ecpubkey.txt with colons removed
    strCheckpointMasterPubKey =
                                    "0484f4bcfce3238a1ebcbf0d4ac99c"
                                    "d3e8f75e96e73325783c807f9068c1"
                                    "e9191b210f7f875fd8e63551989dac"
                                    "fa22f1d9b365a13f2f5a135c87d001"
                                    "0e46d6a576";



    //////////////////////////////////////////////////////////////////////////////
    //
    // Client
    //

    // Minimum disk space required - used in CheckDiskSpace()
    nMinDiskSpace = 52428800;

    GETBLOCKS_LIMIT = 2000;

    MAX_OUTBOUND_CONNECTIONS = 12;

    // configuration file name
    DEFAULT_CONF = "StealthCoin.conf";

    // pid file name
    DEFAULT_PID = "StealthCoin.pid";

    // leveldb default cashe size (MB)
    DEFAULT_DBCACHE = 25;

    // bdb default log file lize (MB)
    DEFAULT_DBLOGSIZE = 100;

    // client connection timeout (ms)
    DEFAULT_TIMEOUT = 5000;

    DEFAULT_PORT_MAINNET = 4437;

    DEFAULT_PROXY_MAINNET = 9050;

    DEFAULT_TORPORT = 9060;

    DEFAULT_MAXCONNECTIONS = 125;

    DEFAULT_BANSCORE = 100;

    // max per connection receive buffer, x1000 bytes
    DEFAULT_MAXRECEIVEBUFFER = 10000;

    // max per connection send buffer, x1000 bytes
    DEFAULT_MAXSENDBUFFER = 4000;

    // default 24 hr ban (seconds)
    DEFAULT_BANTIME = 60 * 60 * 24;

    DEFAULT_RPCPORT_MAINNET = 46502;

    // number of keys to generate for keypool refill
    DEFAULT_KEYPOOL = 100;

    // number of blocks to check (backwards from best height)
    DEFAULT_CHECKBLOCKS = 2500;

    // how rigorously to check blocks (higher is more rigorous)
    DEFAULT_CHECKLEVEL = 1;

    // minimum block size in bytes
    DEFAULT_BLOCKMINSIZE = 0;
    // maximum block size in bytes
    DEFAULT_BLOCKMAXSIZE = MAX_BLOCK_SIZE_GEN / 2;

    // max size of high-priority/low-fee transactions in bytes
    DEFAULT_BLOCKPRIORITYSIZE = 27000;

    // maximum tx fee in XST
    MAX_TXFEE = COIN / 4;



    //////////////////////////////////////////////////////////////////////////////
    //
    // Explore
    //

    // maximum coin value considered "dust" (for explore API)
    DEFAULT_MAXDUST = 5 * MIN_TXOUT_AMOUNT;

    // maximum number of external/change addresses for a tracked HD account
    MAX_HD_CHILDREN = 1024;
    // maximum number of inputs + outputs for a tracked HD account
    MAX_HD_INOUTS = 65535;
    // maximum number of transactions for a tracked HD account
    MAX_HD_TXS = 16383;


    //////////////////////////////////////////////////////////////////////////////
    //
    // TestNet
    //

    CUTOFF_POW_T = 120;
    START_PURCHASE_T = 4204;
    START_PURCHASE2_T = 4205;  // placeholder for mainnet
    START_PURCHASE3_T = 4205;  // placeholder for mainnet
    START_QPOS_T = 17400;
    START_QPOSB_T = 22500;
    START_NFT_T = 22501;       // placeholder for mainnet
    START_FEELESS_T = 3965963;
    START_MISSFIX_T = 4768119;

    // should be similar to aryForksMainNet
    mapForksTestNet = MakeMapIntInt(
        boost::assign::map_list_of
        /* TEST NET */                      /*            Height, Fork Number       */
                                             (                 0, XST_GENESIS       )
        /*                          */       (      CUTOFF_POW_T, XST_FORK002       )
        /*                          */       (               130, XST_FORK004       )
        /*                          */       (               140, XST_FORK005       )
        /*                          */       (               145, XST_FORK006       )
        /*                          */       (  START_PURCHASE_T, XST_FORKPURCHASE  )
        /*                          */       ( START_PURCHASE2_T, XST_FORKPURCHASE2 )
        /*                          */       ( START_PURCHASE3_T, XST_FORKPURCHASE3 )
        /*                          */       (      START_QPOS_T, XST_FORKQPOS      )
        /*                          */       (     START_QPOSB_T, XST_FORKQPOSB     )
        /*                          */       (       START_NFT_T, XST_FORKNFT       )
        /*                          */       (   START_FEELESS_T, XST_FORKFEELESS   )
        /*                          */       (   START_MISSFIX_T, XST_FORKMISSFIX   )
                                                    );


    pchMessageStartTestNet[0] = 0xcf;
    pchMessageStartTestNet[1] = 0xed;
    pchMessageStartTestNet[2] = 0xff;
    pchMessageStartTestNet[3] = 0xfd;

    hashGenesisBlockTestNet = uint256(
       "0x3dd6302f58a524d7c0bf7a8ee945cab05e2367bed482193eddecbb2a4c3bc634");

    hashMerkleRootTestNet = uint256(
       "0xe3de7c386d5b82f62ff24c6d2351539c22b17c6ffab0e267b3cdd72fda82bd83");

    MAX_STEALTH_PROOF_OF_STAKE_TESTNET = 20 * COIN;

    nCoinbaseMaturityTestNet = 10; // test maturity is 10 blocks

    nStakeMinAgeTestNet = 1 * 60; // test net min age is 1 min
    nStakeMaxAgeTestNet = 40 * 60; // test net max age is 40 min

    MODIFIER_INTERVAL_TESTNET = 30;
    MODIFIER_INTERVAL_RATIO_TESTNET = 3;

    bnProofOfWorkLimitTestNet = CBigNum(~uint256(0) >> 16);

    bnProofOfStakeLimitTestNet = CBigNum(~uint256(0) >> 2);

    nTargetSpacingTestNet = 20; // test block spacing is 20 seconds

    // TestNet alerts pubKey
    std::string strTestKey = "04bf2dc2aac67e7d58365a3aa68946"
                             "e40026cd5c97a2606f41991b9614a0"
                             "818c905c28b344e3bbbb2cb072ba18"
                             "0a8974bba93d8bdc53bca40418bd87"
                             "9a733bb064";

    mapCheckpointsTestNet = MakeMapIntUInt256(
        boost::assign::map_list_of
        (           0, hashGenesisBlockTestNet )
        (        8974, uint256("0x00cd9141d0dedc9ed68739c4d0ff98367edafee810e9be8835a71523928e5908"))
           );

    DEFAULT_PORT_TESTNET = 4438;

    DEFAULT_PROXY_TESTNET = 19050;

    DEFAULT_RPCPORT_TESTNET = 46503;

    FUTURE_DRIFT_TESTNET = 7;

}

const ChainParams chainParams;

