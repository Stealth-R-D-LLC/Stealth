// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

// Allow switching between LevelDB and BerkelyDB here in case we need to temporarily
// go back to BDB for any reason. Once we're confident enough with LevelDB to stick
// with it, this can be deleted.


#include "txdb-leveldb.h"

// Sets up whatever database layer was chosen for in-memory only access. Used by the
// the unit test framework.
// extern void MakeMockTXDB();

#endif  // BITCOIN_TXDB_H
