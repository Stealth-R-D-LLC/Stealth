// Copyright (c) 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _STEALTHEXPLORE_H_
#define _STEALTHEXPLORE_H_ 1

#include <string>


class CBlock;
class CTransaction;

// sorted in descending order
typedef std::map<int64_t, unsigned int,
                 std::greater<int64_t> > MapBalanceCounts;

extern const std::string ADDR_QTY_INPUT;
extern const std::string ADDR_QTY_OUTPUT;
extern const std::string ADDR_QTY_INOUT;
extern const std::string ADDR_QTY_UNSPENT;
extern const std::string ADDR_TX_INPUT;
extern const std::string ADDR_TX_OUTPUT;
extern const std::string ADDR_TX_INOUT;
extern const std::string ADDR_LOOKUP_OUTPUT;
extern const std::string ADDR_BALANCE;
extern const std::string ADDR_SET_BAL;

extern int64_t nMaxDust;

extern bool fWithExploreAPI;
extern bool fDebugExplore;
extern MapBalanceCounts mapAddressBalances;

void UpdateMapAddressBalances(const MapBalanceCounts& mapAddressBalancesAdd,
                              const std::set<int64_t>& setAddressBalancesRemove,
                              MapBalanceCounts& mapAddressBalancesRet);
 
bool ExploreConnectInput(CTxDB& txdb,
                         const CTransaction& tx,
                         const unsigned int& n,
                         const MapPrevTx& mapInputs,
                         const uint256& txid,
                         MapBalanceCounts& mapAddressBalancesAddRet,
                         std::set<int64_t>& setAddressBalancesRemoveRet);

bool ExploreConnectOutput(CTxDB& txdb,
                          const CTransaction& tx,
                          const unsigned int& n,
                          const uint256& txid,
                          MapBalanceCounts& mapAddressBalancesAddRet,
                          std::set<int64_t>& setAddressBalancesRemoveRet);

bool ExploreDisconnectOutput(CTxDB& txdb,
                             const CTransaction& tx,
                             const int& n,
                             const uint256& txid,
                             MapBalanceCounts& mapAddressBalancesAddRet,
                             std::set<int64_t>& setAddressBalancesRemoveRet);

bool ExploreDisconnectInput(CTxDB& txdb,
                            const CTransaction& tx,
                            const int& n,
                            const MapPrevTx& mapInputs,
                            const uint256& txid,
                            MapBalanceCounts& mapAddressBalancesAddRet,
                            std::set<int64_t>& setAddressBalancesRemoveRet);

bool ExploreConnectTx(CTxDB& txdb, const CTransaction &tx);
bool ExploreConnectBlock(CTxDB& txdb, const CBlock *const block);

bool ExploreDisconnectTx(CTxDB& txdb, const CTransaction &tx);
bool ExploreDisconnectBlock(CTxDB& txdb, const CBlock *const block);

#endif  // _STEALTHEXPLORE_H_
