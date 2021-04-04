#ifndef CLIENTVERSION_H
#define CLIENTVERSION_H

//
// client versioning
//

// These need to be macros, as version.cpp's and bitcoin-qt.rc's voodoo requires it
#define CLIENT_VERSION_MAJOR       3
#define CLIENT_VERSION_MINOR       1
#define CLIENT_VERSION_REVISION    1
#define CLIENT_VERSION_BUILD       0

// cloners: add your new forks higher than highest here
//          keep existing
//          also, rewrite GetFork
enum ForkNumbers
{
    XST_GENESIS = 0,
    XST_FORK002,
    XST_FORK004,
    XST_FORK005,
    XST_FORK006,
    XST_FORKNFT,       // fork 7
    XST_FORKPURCHASE,  // fork 7
    XST_FORKQPOS,      // fork 7
    XST_FORKQPOSB,     // fork 7
    XST_FORKFEELESS,   // fork 8
    XST_FORKMISSFIX,   // fork 9 (testnet)
    TOTAL_FORKS
};

//
// network protocol versioning
//
static const int CLIENT_PROTOCOL_VERSION = 63500;

// proto   version   notes
// -----   -------   ----------------------------------------------------------
// 63500 : fork 9    XST_FORKMISSFIX
//       : 3.1.1.0 : Increasing staker acceptable missed blocks on testnet
//       :         : Adding NFTs to test and mainnets
//       : fork 8    XST_FORKFEELESS
// 63400 : 3.1.0.5 : Feeless tx added during block creation
//         3.1.0.3 : Fixing erroneous rejection of all feework
//         3.1.0.2 : Fixes to feeless
//         3.1.0.1 : Adding feeless transactions
//                 : Adding nBlockSize to block indices
//       : fork 7
// 63300 : 3.0.3.5 : Changes to StealthExplore DB for HD account handling
//         3.0.3.4 : Changes to ExploreTxInfo (StealthExplore Database)
//         3.0.3.3 : Many changes to StealthExplore Database
//         3.0.3.2 : Adding nPicoPower to block indices
//         3.0.3.1 : Combining message handling and qPoS
//         3.0.3.0 : Forking the tx authority check earlier (forking)
// 63200 : 3.0.2.0 : Fixed qPoS transaction authority checking (forking)
// 63000 : 3.0.1.0 : Testnet version
//                   Fixing many qPoS consensus issues
//         3.0.0.0 : Testnet version
//                   Staker purchasing and qPoS
//                   Immaleable transaction IDs
// 62200 : fork 6
//         2.2.0.0 : Removing all traces of tx timestamp
// 62100 : fork 5
//         2.1.0.4 : OpenSSL v1.1 compatibility
//         2.1.0.1 : SignSignature fix
//         2.1.0.0 : Clockdrift improvements & Checklocktimeverify (forking)
// 61300 : fork 4
//         1.3.0.0 : Stealth Addresses
// 61040 : fork 3
//         1.0.4.0 : Adjust max bits computation for 60s block times
// 61021 : fork 2
//         1.0.2.1 : Kill PoW at 5460 to deal with forks on pools
//                   Was block 20421
// 61011 : fork 1
//         1.0.1.1 : Fix PoS difficulty adjustment during PoW period
// 61001 : [Genesis]



// Converts the parameter X to a string after macro replacement on X has been performed.
// Don't merge these into one macro!
#define STRINGIZE(X) DO_STRINGIZE(X)
#define DO_STRINGIZE(X) #X

#endif // CLIENTVERSION_H
