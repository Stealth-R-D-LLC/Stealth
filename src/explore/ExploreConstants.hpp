// Copyright (c) 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLORECONSTANTS_H_
#define _EXPLORECONSTANTS_H_ 1

#include <string>
#include <map>

typedef std::pair<std::string, std::string> exploreKey_t;

// sorted in descending order
typedef std::map<int64_t, unsigned int,
                 std::greater<int64_t> > MapBalanceCounts;

//////////////////////////////////////////////////////////////////////////////
//
// address transaction flag
//
// Is Input
const int FLAG_ADDR_TX = 1 << 30;
// FLAG_ADDR_TX is used on vin/vout n, which should
//    never even be close to (1<<30)-1 elements
const int MASK_ADDR_TX = ~FLAG_ADDR_TX;


//////////////////////////////////////////////////////////////////////////////
//
// leveldb record strings for the explore API
//

const std::string EXPLORE_KEY = "EXPK";

// Explore Sentinel lexigraphical first key for iterating
const exploreKey_t EXPLORE_SENTINEL(EXPLORE_KEY, "");
// Addr Qty
const exploreKey_t ADDR_QTY_INPUT(EXPLORE_KEY, "AQI");
const exploreKey_t ADDR_QTY_OUTPUT(EXPLORE_KEY, "AQO");
const exploreKey_t ADDR_QTY_INOUT(EXPLORE_KEY, "AQIO");
const exploreKey_t ADDR_QTY_UNSPENT(EXPLORE_KEY, "AQU");
// Addr Tx
const exploreKey_t ADDR_TX_INPUT(EXPLORE_KEY, "ATI");
const exploreKey_t ADDR_TX_OUTPUT(EXPLORE_KEY, "ATO");
const exploreKey_t ADDR_TX_INOUT(EXPLORE_KEY, "ATIO");
// Addr Lookup
const exploreKey_t ADDR_LOOKUP_OUTPUT(EXPLORE_KEY, "ALO");
// Addr Balance
const exploreKey_t ADDR_BALANCE(EXPLORE_KEY, "AB");
// Addr Set
const exploreKey_t ADDR_SET_BAL(EXPLORE_KEY, "ASB");
// Tx Info
const exploreKey_t TX_INFO(EXPLORE_KEY, "TI");


#endif  // _EXPLORECONSTANTS_H_
