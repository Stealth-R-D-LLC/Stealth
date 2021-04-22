// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLOREDESTINATION_H_
#define _EXPLOREDESTINATION_H_ 1

#include "serialize.h"

#include "json/json_spirit_utils.h"


class ExploreDestination
{
public:
    std::vector<std::string> addresses;
    int required;
    int64_t amount;
    std::string type;

    void SetNull();

    ExploreDestination();

    ExploreDestination(const std::vector<std::string>& addressesIn,
                       const int requiredIn,
                       const int64_t& amountIn,
                       const std::string& typeIn);

    void AsJSON(json_spirit::Object& objRet) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(addresses);
        READWRITE(required);
        READWRITE(amount);
        READWRITE(type);
    )
};

#endif  /* _EXPLOREDESTINATION_H_ */
