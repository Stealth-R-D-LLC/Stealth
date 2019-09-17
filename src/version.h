// Copyright (c) 2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

#include "clientversion.h"
#include <string>

//
// client versioning
//

static const int CLIENT_VERSION =
                           1000000 * CLIENT_VERSION_MAJOR
                         +   10000 * CLIENT_VERSION_MINOR
                         +     100 * CLIENT_VERSION_REVISION
                         +       1 * CLIENT_VERSION_BUILD;

extern const std::string CLIENT_NAME;
extern const std::string CLIENT_BUILD;
extern const std::string CLIENT_NUMBERS;
extern const std::string CLIENT_DATE;

// display version
#define DISPLAY_VERSION_MAJOR       3
#define DISPLAY_VERSION_MINOR       0
#define DISPLAY_VERSION_REVISION    2
#define DISPLAY_VERSION_BUILD       0

//
// network protocol versioning
//

// 61001 : [Genesis]
// 61011 : fork 1
//         1.0.1.1 : Fix PoS difficulty adjustment during PoW period
// 61021 : fork 2
//         1.0.2.1 : Kill PoW at 5460 to deal with forks on pools
//                   Was block 20421
// 61040 : fork 3
//         1.0.4.0 : Adjust max bits computation for 60s block times
// 61300 : fork 4
//         1.3.0.0 : Stealth Addresses
// 62100 : fork 5
//         2.1.0.0 : Clockdrift improvements & Checklocktimeverify (forking)
//         2.1.0.1 : SignSignature fix
//         2.1.0.4 : OpenSSL v1.1 compatibility
// 62200 : fork 6
//         2.2.0.0 : Removing all traces of tx timestamp
// 63000 : fork 7
//         3.0.0.0 : Testnet version
//                   Staker purchasing and qPoS
//                   Immaleable transaction IDs
//         3.0.1.0 : Testnet version
//                   Fixing many qPoS consensus issues
// 63200 : 3.0.2.0 : Fixed qPoS transaction authority checking
static const int PROTOCOL_VERSION = 63200;

// intial proto version, to be increased after version/verack negotiation
static const int INIT_PROTO_VERSION = 61300;


// This was MIN_PROTO_VERSION and has now bifurcated to
// MIN_PEER_PROTO_VERSION and INIT_PROTO_VERSION.
// This is replaced by a function, peers will be dropped that do not
// support the same chain as this node.
// earlier versions not supported as of Feb 2012, and are disconnected
// static const int MIN_PEER_PROTO_VERSION = 61300;

// nTime field added to CAddress, starting with this version;
// if possible, avoid requesting addresses nodes older than this
static const int CADDR_TIME_VERSION = 61001;

// only request blocks from nodes outside this range of versions
static const int NOBLKS_VERSION_START = 0;
static const int NOBLKS_VERSION_END = 62010;

// BIP 0031, pong message, is enabled for all versions AFTER this one
static const int BIP0031_VERSION = 60000;

// "mempool" command, enhanced "getdata" behavior starts with this version:
static const int MEMPOOL_GD_VERSION = 60002;

static const int DATABASE_VERSION = 61201;

#endif
