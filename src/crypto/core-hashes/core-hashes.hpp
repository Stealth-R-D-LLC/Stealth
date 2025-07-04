////////////////////////////////////////////////////////////////////////////////
// core-hashes.hpp
//
// Copyright (c) 2024 Stealth R&D LLC
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
////////////////////////////////////////////////////////////////////////////////

/******************************************************************************
 * The intent of core-hashes is to provide a standard interface for hashes
 *    sha256, sha1, and ripemd160, requiring no external dependencies.
 * The goal is to avoid breakage resulting from OpenSSL's ever-changing
 *    lower-level APIs.
 *
 * Parameters:
 *    `pdata` : pointer to data
 *    `nbytes` : length of data at `pdata`
 *    `pdigest` : pointer to digest, which must have at least 20 bytes of space
 *
 * Returns:
 *    size of the digest in bytes (`size_t`)
 ******************************************************************************/

#pragma once

extern "C"
{
#include "sha3.h"
#include "sha2.h"
#include "ripemd160.h"
}

#include "uint256.h"

#include <stddef.h>


namespace CoreHashes
{

unsigned int SHA3_256(const unsigned char* pdata,
                      unsigned int nbytes,
                      unsigned char* pdigest);

unsigned int SHA256(const unsigned char* pdata,
                    unsigned int nbytes,
                    unsigned char* pdigest);

unsigned int SHA256D(const unsigned char* pdata,
                     unsigned int nbytes,
                     unsigned char* pdigest);

unsigned int SHA1(const unsigned char* pdata,
                  unsigned int nbytes,
                  unsigned char* pdigest);

unsigned int RIPEMD160(const unsigned char* pdata,
                       unsigned int nbytes,
                       unsigned char* pdigest);

}  // namespace CoreHashes


template<typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1];
    uint256 hash;
    CoreHashes::SHA256D((pbegin == pend ? pblank :
                                          (unsigned char*) &pbegin[0]),
            (pend - pbegin) * sizeof(pbegin[0]),
            (unsigned char*) &hash);
    return hash;
}

template<typename T1>
inline uint256 ProofHash(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1];
    uint256 hash;
    CoreHashes::SHA256D((pbegin == pend ? pblank :
                                          (unsigned char*) &pbegin[0]),
            (pend - pbegin) * sizeof(pbegin[0]),
            (unsigned char*) &hash);
    return hash;
}

