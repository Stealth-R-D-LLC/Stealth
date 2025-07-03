#ifndef CLIENTVERSION_H
#define CLIENTVERSION_H

//
// client versioning
//

// These need to be macros, as version.cpp's and bitcoin-qt.rc's voodoo requires it
#define CLIENT_VERSION_MAJOR       3
#define CLIENT_VERSION_MINOR       2
#define CLIENT_VERSION_REVISION    1
#define CLIENT_VERSION_BUILD       3

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
    XST_FORKPURCHASE,   // fork 7
    XST_FORKPURCHASE2,  // fork 8
    XST_FORKPURCHASE3,  // fork 9
    XST_FORKQPOS,       // fork 9
    XST_FORKQPOSB,      // fork 9
    XST_FORKNFT,        // fork 9
    XST_FORKFEELESS,    // fork 10
    XST_FORKMISSFIX,    // fork 11
    XST_FORKREINSTATE,  // fork 12
    XST_FORKMISSFIX2,   // fork 13
    XST_FORKFEELESS2,   // fork 14
    XST_FORKIP64,       // fork 15
    TOTAL_FORKS
};

//
// network protocol versioning
//
static const int CLIENT_PROTOCOL_VERSION = 64200;

// proto   version   notes
// -----   -------    ----------------------------------------------------------
//       : 3.2.1.3  : Fixed building with BOOST_ASIO_VERSION > 102802 (1.28.2)
//       : 3.2.1.2  : Making QPRegistry thread safe / forks from 16 byte IP
// 64200 : fork 15  : XST_FORKIP64
// 64200 : 3.2.0.0  : IP Addresses up to 64 bytes for Tor v3
// 64000 : fork 14  : XST_FORKFEELESS2
// 64000 : fork 13  : XST_FORKMISSFIX2
//       : 3.1.8.0  : liststakerauthorities and small fixes for multisigs
//       : 3.1.7.12 : Fixed typo for max block size
//       : 3.1.7.11 : Fixed building with miner
//       : 3.1.7.10 : Fixed ownership of claims
//       : 3.1.7.9  : Added watch addresses (RPC importaddress)
//       : 3.1.7.8  : Future blocks in feeless won't DoS if init block download
//       : 3.1.7.7  : Another small fix to productivity reporting
//       : 3.1.7.6  : Fixing productivity reporting in Staker JSON
//       : 3.1.7.5  : Fixing staker misses after chain halting
//       : 3.1.7.0  : Enforcing that feeless inputs are confirmed (FEELESS2)
// 63900 : fork 12  : XST_FORKREINSTATE
//         3.1.6.7  : Improving feeless priority calculation, fixed gettxvolume
//         3.1.6.6  : Fixing ungraceful overflow in feework diff calculation
//         3.1.6.5  : Fixing fatal reorgs and even more fatal block index load
//         3.1.6.4  : Enabling exitreplay RPC on mainnet
//         3.1.6.3  : Fixing inconsistent feework checking
//                    Fixing inclusion of disconnected blocks in some chains
//         3.1.6.2  : Eliminate some global state dependence from the registry
//         3.1.6.1  : Fixing crash on start if no blocks
//         3.1.6.0  : Reinstating stakers and resetting docked blocks
//                  : Fixing low balance purge
//                  : RPC imporvements: getqueuesummary
// 63800 : fork 11  : XST_FORKMISSFIX
//         3.1.5.1  : Fixing case when best chain is not properly linked
//                  : Rate limiting unsecured node disconnects
//         fork 10  : XST_FORKFEELESS
//         fork 9   : XST_FORKNFT
//                    XST_FORKQPOSB
//         3.1.5.0  : Fixing erroneous staker termination on mainnet
// 63700              XST_FORKQPOS
//         3.1.4.0  : Fixing stall on transition to Junaeth
// 63600   3.1.3.4  : Fixing erroneous full checking of nonsequential blocks
//         3.1.3.3  : Fixing bad qPoS inputs in block production
//         3.1.3.2  : Fixing stale qPoS txs in block production
//                    XST_FORKPURCHASE3
//         3.1.3.1  : Fixing NFT prices
//         3.1.3.0  : Fixing NFT purchases
// 63500 : fork 8   : XST_FORKPURCHASE2
//         3.1.2.3  : Moving failed purchase/claim removal from CreateNewBlock
//         3.1.2.2  : Fixing block creation for failed purchase or claim
//         3.1.2.1  : Fixing purchases
// 63000 : fork 7   : XST_FORKPURCHASE
//         3.1.1.3  : Added gethdaddresses RPC
//         3.1.1.2  : Fixed mint calculation for PoW blocks
//         3.1.1.1  : Adding getcharacterspg
//         3.1.1.0  : Increasing staker acceptable missed blocks on testnet
//                    Adding NFTs to test and mainnets
//         3.1.0.5  : Feeless tx added during block creation
//         3.1.0.3  : Fixing erroneous rejection of all feework
//         3.1.0.2  : Fixes to feeless
//         3.1.0.1  : Adding feeless transactions
//                    Adding nBlockSize to block indices
//         3.0.3.5  : Changes to StealthExplore DB for HD account handling
//         3.0.3.4  : Changes to ExploreTxInfo (StealthExplore Database)
//         3.0.3.3  : Many changes to StealthExplore Database
//         3.0.3.2  : Adding nPicoPower to block indices
//         3.0.3.1  : Combining message handling and qPoS
//         3.0.3.0  : Forking the tx authority check earlier (forking)
//         3.0.2.0  : Fixed qPoS transaction authority checking (forking)
//         3.0.1.0  : Testnet version
//                    Fixing many qPoS consensus issues
//         3.0.0.0  : Testnet version
//                    Staker purchasing and qPoS
//                    Immaleable transaction IDs
// 62200 : fork 6   : XST_FORK006
//         2.2.0.0  : Removing all traces of tx timestamp
// 62100 : fork 5
//         2.1.0.4  : OpenSSL v1.1 compatibility
//         2.1.0.1  : SignSignature fix
//         2.1.0.0  : Clockdrift improvements & Checklocktimeverify (forking)
// 61300 : fork 4
//         1.3.0.0  : Stealth Addresses
// 61040 : fork 3
//         1.0.4.0  : Adjust max bits computation for 60s block times
// 61021 : fork 2
//         1.0.2.1  : Kill PoW at 5460 to deal with forks on pools
//                    Was block 20421
// 61011 : fork 1
//         1.0.1.1  : Fix PoS difficulty adjustment during PoW period
// 61001 : [Genesis]


// Converts the parameter X to a string after macro replacement on X has been performed.
// Don't merge these into one macro!
#define STRINGIZE(X) DO_STRINGIZE(X)
#define DO_STRINGIZE(X) #X


#define CLIENT_VERSION_FULL CLIENT_VERSION_MAJOR, \
                            CLIENT_VERSION_MINOR, \
                            CLIENT_VERSION_REVISION, \
                            CLIENT_VERSION_BUILD
#define CLIENT_VERSION_FULL_STR STRINGIZE(CLIENT_VERSION_MAJOR) "." \
                                STRINGIZE(CLIENT_VERSION_MINOR) "." \
                                STRINGIZE(CLIENT_VERSION_REVISION) "." \
                                STRINGIZE(CLIENT_VERSION_BUILD)
#define V_CLIENT_VERSION_FULL_STR "v" CLIENT_VERSION_FULL_STR

#endif // CLIENTVERSION_H
