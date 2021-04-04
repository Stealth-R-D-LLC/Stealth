// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "nfts.hpp"

using namespace std;
using namespace json_spirit;

QPNft::QPNft()
    : hash(0),
      strImageUrl(""),
      strCollection(""),
      strArtist(""),
      nNumber(0),
      nOf(0),
      strArtistUrl(""),
      strCharKey(""),
      strFullName(""),
      strNickname(""),
      nMoxie(0),
      nIrreverance(0),
      nNarcissism(0) {}

QPNft::QPNft(string strHashIn,
             string strImageUrlIn,
             string strCollectionIn,
             string strArtistIn,
             unsigned int nNumberIn,
             unsigned int nOfIn,
             string strArtistUrlIn,
             string strCharKeyIn,
             string strFullNameIn,
             string strNicknameIn,
             uint8_t nMoxieIn,
             uint8_t nIrreveranceIn,
             uint8_t nNarcissismIn)
    : hash(strHashIn),
      strImageUrl(strImageUrlIn),
      strCollection(strCollectionIn),
      strArtist(strArtistIn),
      nNumber(nNumberIn),
      nOf(nOfIn),
      strArtistUrl(strArtistUrlIn),
      strCharKey(strCharKeyIn),
      strFullName(strFullNameIn),
      strNickname(strNicknameIn),
      nMoxie(nMoxieIn),
      nIrreverance(nIrreveranceIn),
      nNarcissism(nNarcissismIn) {}

void QPNft::AsJSON(Object& objRet, unsigned int nNftID) const
{
    objRet.push_back(Pair("nickname", strNickname));
    objRet.push_back(Pair("full_name", strFullName));
    objRet.push_back(Pair("artist", strArtist));
    objRet.push_back(Pair("collection", strCollection));
    objRet.push_back(Pair("number", (int64_t)nNumber));
    objRet.push_back(Pair("of", (int64_t)nOf));
    objRet.push_back(Pair("moxie", (int64_t)nMoxie));
    objRet.push_back(Pair("irreverance", (int64_t)nIrreverance));
    objRet.push_back(Pair("narcissism", (int64_t)nNarcissism));
    objRet.push_back(Pair("image_url", strImageUrl));
    objRet.push_back(Pair("artist_url", strArtistUrl));
    objRet.push_back(Pair("hash", hash.ToString()));
    if (nNftID)
    {
        objRet.push_back(Pair("character_id", (int64_t)nNftID));
        objRet.push_back(Pair("character_key", strCharKey));
    }
}
