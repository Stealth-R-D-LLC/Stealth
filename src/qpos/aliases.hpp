// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _STAKER_ALIASES_H_
#define _STAKER_ALIASES_H_ 1

#include "QPConstants.hpp"

#include <string>

std::string ToLowercaseSafe(const std::string &s);
std::string Despace(const std::string &s);
bool AliasIsValid(const std::string &sAlias);

#endif  /* _STAKER_ALIASES_H_ */
