////////////////////////////////////////////////////////////////////////////////
// core-hashes.cpp
//
// Copyright (c) 2024 Stealth R&D LLC
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
////////////////////////////////////////////////////////////////////////////////

#include "core-hashes.hpp"

#include <stdexcept>
#include <cstring>

using namespace std;

unsigned int CoreHashes::SHA3_256(const unsigned char* pdata,
                                  unsigned int nbytes,
                                  unsigned char* pdigest)
{
    if (pdigest == nullptr)
    {
        throw runtime_error(
                   "CoreHashes::SHA3_256(): "
                   "Pointer to 32 byte digest array is NULL.");
    }

    sha3_context ctx;
    sha3_Init256(&ctx);
    sha3_Update(&ctx, pdata, nbytes);
    // no need to worry about destroying, it is a pointer to ctx.sb
    const void* hash = sha3_Finalize(&ctx);

    memcpy(pdigest, hash, SHA3_256_DIGEST_LENGTH_);

    return SHA3_256_DIGEST_LENGTH_;
}

unsigned int CoreHashes::SHA256(const unsigned char* pdata,
                                unsigned int nbytes,
                                unsigned char* pdigest)
{
    if (pdigest == nullptr)
    {
        throw runtime_error(
                   "CoreHashes::SHA256(): "
                   "Pointer to 32 byte digest array is NULL.");
    }

    CORE_SHA256_CTX ctx;
    sha256_Init(&ctx);
    sha256_Update(&ctx, pdata, nbytes);
    sha256_Final(&ctx, pdigest);

    return SHA256_DIGEST_LENGTH_;
}

unsigned int CoreHashes::SHA1(const unsigned char* pdata,
                              unsigned int nbytes,
                              unsigned char* pdigest)
{
    if (pdigest == nullptr)
    {
        throw runtime_error(
                   "CoreHashes::SHA1(): "
                   "Pointer to 20 byte digest array is NULL.");
    }

    CORE_SHA1_CTX ctx;
    sha1_Init(&ctx);
    sha1_Update(&ctx, pdata, nbytes);
    sha1_Final(&ctx, pdigest);

    return SHA1_DIGEST_LENGTH_;
}

unsigned int CoreHashes::RIPEMD160(const unsigned char* pdata,
                                   unsigned int nbytes,
                                   unsigned char* pdigest)
{
    if (pdigest == nullptr)
    {
        throw runtime_error(
                   "CoreHashes::RIPEMD160(): "
                   "Pointer to 20 byte digest array is NULL.");
    }

    CORE_RIPEMD160_CTX ctx;
    ripemd160_Init(&ctx);
    ripemd160_Update(&ctx, pdata, nbytes);
    ripemd160_Final(&ctx, pdigest);

    return RIPEMD160_DIGEST_LENGTH_;
}
