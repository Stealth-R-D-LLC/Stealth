// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "nfts.hpp"


// sha256(sha256(nft_1) + sha256(nft_2) + ...), e.g.:
//    >>> import glob, hashlib
//    >>> d = glob.glob('testnet-nft*.png')
//    >>> d.sort()
//    >>> h = hashlib.sha256()
//    >>> for f in d[:100]:
//    ...  h.update(hashlib.sha256(open(f, "rb").read()).digest())
//    ...
//    >>> h.hexdigest()
//    'b807539f05f859e6234b53d752b5855ae1acebd53744389aa641fd23d99a17a0'
uint256 hashOfNftHashesMain(0);

QPMapNft mapNftsMain;
QPMapNftLookup mapNftLookupMain;
