////////////////////////////////////////////////////////////////////////////////
// core-hashes.cpp
//
// Copyright (c) 2024 Stealth R&D LLC
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
////////////////////////////////////////////////////////////////////////////////

#include "core-hashes.hpp"

#include <openssl/evp.h>
#include <openssl/opensslv.h>

#include <stdexcept>

using namespace std;

unsigned int CoreHashes::SHA256(const unsigned char* pdata,
                                unsigned int nbytes,
                                unsigned char* pdigest)
{
    if (pdigest == NULL)
    {
        throw runtime_error(
                   "StealthCrypto::SHA256(): "
                   "Pointer to 32 byte digest array is NULL.");
    }
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);
#else
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
#endif

    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, pdata, nbytes);
    unsigned int len;
    EVP_DigestFinal_ex(ctx, pdigest, &len);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX_cleanup(&ctx);
#else
    EVP_MD_CTX_free(ctx);
#endif

    return len;
}  // SHA256


unsigned int CoreHashes::SHA1(const unsigned char* pdata,
                              unsigned int nbytes,
                              unsigned char* pdigest)
{
    if (pdigest == NULL)
    {
        throw runtime_error(
                   "StealthCrypto::SHA1(): "
                   "Pointer to 20 byte digest array is NULL.");
    }

    unsigned int len;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);
    EVP_DigestInit_ex(&ctx, EVP_sha1(), NULL);
    EVP_DigestUpdate(&ctx, pdata, nbytes);
    EVP_DigestFinal_ex(&ctx, hash, &len);
    EVP_MD_CTX_cleanup(&ctx);
#else
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha1(), NULL);
    EVP_DigestUpdate(ctx, pdata, nbytes);
    EVP_DigestFinal_ex(ctx, pdigest, &len);
    EVP_MD_CTX_free(ctx);
#endif

    return len;
}  // SHA1


unsigned int CoreHashes::RIPEMD160(const unsigned char* pdata,
                                   unsigned int nbytes,
                                   unsigned char* pdigest)
{
    if (pdigest == NULL)
    {
        throw runtime_error(
                   "StealthCrypto::RIPEMD160(): "
                   "Pointer to 20 byte digest array is NULL.");
    }
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);
#else
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
#endif

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_MD *md = EVP_MD_fetch(NULL, "RIPEMD160", NULL);
    if (md == NULL)
    {
        throw runtime_error(
                "StealthCrypto::RIPEMD160(): "
                "OpenSSL RIPEMD160 isn't available. "
                "Ensure OpenSSL has RIPEMD160 enabled.");
    }
    EVP_DigestInit_ex(ctx, md, NULL);
#else
    EVP_DigestInit_ex(ctx, EVP_ripemd160(), NULL);
#endif

    EVP_DigestUpdate(ctx, pdata, nbytes);
    unsigned int len;
    EVP_DigestFinal_ex(ctx, pdigest, &len);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX_cleanup(&ctx);
#else
    EVP_MD_CTX_free(ctx);
#endif

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_MD_free(md);
#endif

    return len;
}  // RIPEMD160
