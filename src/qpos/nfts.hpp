// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPOS_NFTS_H_
#define _QPOS_NFTS_H_ 1


#include "QPNft.hpp"

#include <map>

typedef std::map<unsigned int, const QPNft> QPMapNft;
typedef QPMapNft::const_iterator QPMapNftIterator;
typedef std::map<std::string, unsigned int> QPMapNftLookup;

typedef std::map<unsigned int, unsigned int> QPMapNftOwnership;
typedef QPMapNftOwnership::const_iterator QPMapNftOwnershipIterator;

extern uint256 hashOfNftHashes;
// key: nft ID, value: nft
extern QPMapNft mapNfts;
// key: alias, value: nft ID
extern QPMapNftLookup mapNftLookup;

void InitNfts();


#endif  // _QPOS_NFTS_H_

