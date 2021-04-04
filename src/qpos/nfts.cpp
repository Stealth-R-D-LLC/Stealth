// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "nfts.hpp"

extern bool fTestNet;

extern uint256 hashOfNftHashesMain;
extern uint256 hashOfNftHashesTest;

extern QPMapNft mapNftsMain;
extern QPMapNft mapNftsTest;

extern QPMapNftLookup mapNftLookupMain;
extern QPMapNftLookup mapNftLookupTest;

uint256 hashOfNftHashes;
QPMapNft mapNfts;
QPMapNftLookup mapNftLookup;

void InitNfts()
{
    mapNfts.clear();
    mapNftLookup.clear();
    if (fTestNet)
    {
        hashOfNftHashes = hashOfNftHashesTest;
        mapNfts.insert(mapNftsTest.begin(), mapNftsTest.end());
        mapNftLookup.insert(mapNftLookupTest.begin(),
                            mapNftLookupTest.end());
    }
    else
    {
        hashOfNftHashes = hashOfNftHashesMain;
        mapNfts.insert(mapNftsMain.begin(), mapNftsMain.end());
        mapNftLookup.insert(mapNftLookupMain.begin(),
                            mapNftLookupMain.end());
    }
}
