// Copyright (c) 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLORECONSTANTS_H_
#define _EXPLORECONSTANTS_H_ 1

#include <string>
#include <map>

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
// Addr Qty
const std::string ADDR_QTY_INPUT = "AddrQtyInput";
const std::string ADDR_QTY_OUTPUT = "AddrQtyOutput";
const std::string ADDR_QTY_INOUT = "AddrQtyInOut";
const std::string ADDR_QTY_UNSPENT = "AddrQtyUnspent";
// Addr Tx
const std::string ADDR_TX_INPUT = "AddrTxInput";
const std::string ADDR_TX_OUTPUT = "AddrTxOutput";
const std::string ADDR_TX_INOUT = "AddrTxInOut";
// Addr Lookup
const std::string ADDR_LOOKUP_OUTPUT = "AddrLookupOutput";
// Addr Balance
const std::string ADDR_BALANCE = "AddrBalance";
// Addr Set
const std::string ADDR_SET_BAL = "AddrSetBal";
// Tx Info
const std::string TX_INFO = "TxInfo";


#endif  // _EXPLORECONSTANTS_H_
