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
const std::string EXPLORE_SENTINEL_LABEL = "";
const exploreKey_t EXPLORE_SENTINEL(EXPLORE_KEY, EXPLORE_SENTINEL_LABEL);

// Addr Qty
const std::string ADDR_QTY_INPUT_LABEL = "AQI";
const exploreKey_t ADDR_QTY_INPUT(EXPLORE_KEY, ADDR_QTY_INPUT_LABEL);

const std::string ADDR_QTY_OUTPUT_LABEL = "AQO";
const exploreKey_t ADDR_QTY_OUTPUT(EXPLORE_KEY, ADDR_QTY_OUTPUT_LABEL);

const std::string ADDR_QTY_INOUT_LABEL = "AQIO";
const exploreKey_t ADDR_QTY_INOUT(EXPLORE_KEY, ADDR_QTY_INOUT_LABEL);

const std::string ADDR_QTY_UNSPENT_LABEL = "AQU";
const exploreKey_t ADDR_QTY_UNSPENT(EXPLORE_KEY, ADDR_QTY_UNSPENT_LABEL);

// Addr Tx
const std::string ADDR_TX_INPUT_LABEL = "ATI";
const exploreKey_t ADDR_TX_INPUT(EXPLORE_KEY, ADDR_TX_INPUT_LABEL);

const std::string ADDR_TX_OUTPUT_LABEL = "ATO";
const exploreKey_t ADDR_TX_OUTPUT(EXPLORE_KEY, ADDR_TX_OUTPUT_LABEL);

const std::string ADDR_TX_INOUT_LABEL = "ATIO";
const exploreKey_t ADDR_TX_INOUT(EXPLORE_KEY, ADDR_TX_INOUT_LABEL);

// Addr Lookup
const std::string ADDR_LOOKUP_OUTPUT_LABEL = "ALO";
const exploreKey_t ADDR_LOOKUP_OUTPUT(EXPLORE_KEY, ADDR_LOOKUP_OUTPUT_LABEL);

// Addr Value
const std::string ADDR_BALANCE_LABEL = "AB";
const exploreKey_t ADDR_BALANCE(EXPLORE_KEY, ADDR_BALANCE_LABEL);

const std::string ADDR_VALUEIN_LABEL = "AVI";
const exploreKey_t ADDR_VALUEIN(EXPLORE_KEY, ADDR_VALUEIN_LABEL);

const std::string ADDR_VALUEOUT_LABEL = "AVO";
const exploreKey_t ADDR_VALUEOUT(EXPLORE_KEY, ADDR_VALUEOUT_LABEL);

// Addr Set
const std::string ADDR_SET_BAL_LABEL = "ASB";
const exploreKey_t ADDR_SET_BAL(EXPLORE_KEY, ADDR_SET_BAL_LABEL);

// Tx Info
const std::string TX_INFO_LABEL = "TI";
const exploreKey_t TX_INFO(EXPLORE_KEY, TX_INFO_LABEL);


#endif  // _EXPLORECONSTANTS_H_
