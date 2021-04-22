// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPNFT_H_
#define _QPNFT_H_ 1

#include "uint256.h"

#include "json/json_spirit_utils.h"

#include <cstdint>
#include <string>

class QPNft
{
public:
    // sha256d hash
    uint256 hash;
    std::string strImageUrl;
    std::string strCollection;
    std::string strArtist;
    unsigned int nNumber;
    unsigned int nOf;
    std::string strArtistUrl;
    std::string strCharKey;
    std::string strFullName;
    std::string strNickname;
    uint8_t nMoxie;
    uint8_t nIrreverance;
    uint8_t nNarcissism;
    std::string strNarrative;

    QPNft();

    QPNft(std::string strHashIn,
          std::string strImageUrlIn,
          std::string strCollectionIn,
          std::string strArtistIn,
          unsigned int nNumberIn,
          unsigned int nOfIn,
          std::string strArtistUrlIn,
          std::string strCharKeyIn,
          std::string strFullNameIn,
          std::string strNicknameIn,
          uint8_t nMoxieIn,
          uint8_t nIrreveranceIn,
          uint8_t nNarcissismIn,
          std::string strNarrativeIn);

    void AsJSON(json_spirit::Object& objRet) const;
};



#endif  // _QPNFT_H_
