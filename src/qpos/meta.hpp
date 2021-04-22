// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef _QPOS_META_H_
#define _QPOS_META_H_ 1

#include "aliases.hpp"

#include <string>


using namespace std;

static const std::string META_KEY_CERTIFIED_NODE = "certified_node";

QPKeyType CheckMetaKey(const std::string &sKey);
bool CheckMetaValue(const std::string &sValue);

#endif  /* _QPOS_META_H_ */
