#ifndef CLIENTVERSION_H
#define CLIENTVERSION_H

//
// client versioning
//

// These need to be macros, as version.cpp's and bitcoin-qt.rc's voodoo requires it
#define CLIENT_VERSION_MAJOR       3
#define CLIENT_VERSION_MINOR       0
#define CLIENT_VERSION_REVISION    3
#define CLIENT_VERSION_BUILD       1

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
    XST_FORKPURCHASE,
    XST_FORKQPOS,
    XST_FORKQPOSB,
    TOTAL_FORKS
};

static const int N_PROTOCOL_VERSIONS = 6;

//
// network protocol versioning
//
static const int CLIENT_PROTOCOL_VERSION = 63300;

// proto   version   notes
// -----   -------   ----------------------------------------------------------
//       : fork 7
// 63300 : 3.0.3.1 : Combining message handling and qPoS
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
