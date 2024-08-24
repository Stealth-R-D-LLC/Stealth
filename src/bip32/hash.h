////////////////////////////////////////////////////////////////////////////////
//
// hash.h
//
// Copyright (c) 2011-2012 Eric Lombrozo
// Copyright (c) 2011-2016 Ciphrex Corp.
//
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.
//

#ifndef __HASH_H___
#define __HASH_H___

#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

#include "uchar_vector.h"

#include "scrypt.h" // for scrypt_1024_1_1_256

#include "hashblock/hashblock.h" // for Hash9

// All inputs and outputs are big endian

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> sha256(const basic_uchar_vector<Allocator>& data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);
#else
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
#endif

    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, &data[0], data.size());
    unsigned int len;
    EVP_DigestFinal_ex(ctx, hash, &len);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX_cleanup(&ctx);
#else
    EVP_MD_CTX_free(ctx);
#endif

    return basic_uchar_vector<Allocator>(hash, SHA256_DIGEST_LENGTH);
}

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> sha256d(const basic_uchar_vector<Allocator>& data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);
#else
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
#endif

    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, &data[0], data.size());
    unsigned int len;
    EVP_DigestFinal_ex(ctx, hash, &len);

    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, hash, SHA256_DIGEST_LENGTH);
    EVP_DigestFinal_ex(ctx, hash, &len);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX_cleanup(&ctx);
#else
    EVP_MD_CTX_free(ctx);
#endif

    return basic_uchar_vector<Allocator>(hash, SHA256_DIGEST_LENGTH);
}

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> sha256_2(const basic_uchar_vector<Allocator>& data)
{
    return sha256d(data);
}

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> ripemd160(const basic_uchar_vector<Allocator>& data)
{
    unsigned char hash[RIPEMD160_DIGEST_LENGTH];
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);
#else
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
#endif

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_MD *md = EVP_MD_fetch(NULL, "RIPEMD160", NULL);
    if (md == NULL) {
        throw std::runtime_error("RIPEMD160 not available. Ensure OpenSSL is configured with RIPEMD160 enabled.");
    }
    EVP_DigestInit_ex(ctx, md, NULL);
#else
    EVP_DigestInit_ex(ctx, EVP_ripemd160(), NULL);
#endif

    EVP_DigestUpdate(ctx, &data[0], data.size());
    unsigned int len;
    EVP_DigestFinal_ex(ctx, hash, &len);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX_cleanup(&ctx);
#else
    EVP_MD_CTX_free(ctx);
#endif

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_MD_free(md);
#endif

    return basic_uchar_vector<Allocator>(hash, RIPEMD160_DIGEST_LENGTH);
}

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> hash160(const basic_uchar_vector<Allocator>& data)
{
    return ripemd160(sha256(data));
}

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> mdsha(const basic_uchar_vector<Allocator>& data)
{
    return ripemd160(sha256(data));
}

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> sha1(const basic_uchar_vector<Allocator>& data)
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned int len;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);
    EVP_DigestInit_ex(&ctx, EVP_sha1(), NULL);
    EVP_DigestUpdate(&ctx, &data[0], data.size());
    EVP_DigestFinal_ex(&ctx, hash, &len);
    EVP_MD_CTX_cleanup(&ctx);
#else
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha1(), NULL);
    EVP_DigestUpdate(ctx, &data[0], data.size());
    EVP_DigestFinal_ex(ctx, hash, &len);
    EVP_MD_CTX_free(ctx);
#endif

    return basic_uchar_vector<Allocator>(hash, SHA_DIGEST_LENGTH);
}


template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> hmac_sha256(const basic_uchar_vector<Allocator>& key, const basic_uchar_vector<Allocator>& data)
{
    unsigned char* digest = HMAC(EVP_sha256(), (unsigned char*)&key[0], key.size(), (unsigned char*)&data[0], data.size(), NULL, NULL);
    return basic_uchar_vector<Allocator>(digest, 32);
}

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> hmac_sha512(const basic_uchar_vector<Allocator>& key, const basic_uchar_vector<Allocator>& data)
{
    unsigned char* digest = HMAC(EVP_sha512(), (unsigned char*)&key[0], key.size(), (unsigned char*)&data[0], data.size(), NULL, NULL);
    return basic_uchar_vector<Allocator>(digest, 64);
}

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> hash9(const basic_uchar_vector<Allocator>& data)
{
    uint256 hash = Hash9((unsigned char*)&data[0], (unsigned char*)&data[0] + data.size());
    return basic_uchar_vector<Allocator>((unsigned char*)&hash, (unsigned char*)&hash + 32);
}

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> sha3_256(const uchar_vector& data)
{
    basic_uchar_vector<Allocator> hash(32);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
    {
        throw std::runtime_error("sha3_256(): failed to create context");
    }

    const EVP_MD* md = EVP_sha3_256();
    if (!md)
    {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("sha3_256(): Failed to get algorithm");
    }

    if (EVP_DigestInit_ex(ctx, md, NULL) != 1)
    {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("sha3_256(): Failed to initialize");
    }

    if (EVP_DigestUpdate(ctx, data.data(), data.size()) != 1)
    {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("sha3_256(): Failed to update");
    }

    unsigned int len;
    if (EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char*>(hash.data()), &len) != 1)
    {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("sha3_256(): Failed to finalize");
    }

    EVP_MD_CTX_free(ctx);

    if (len != hash.size())
    {
        throw std::runtime_error("sha3_256(): Unexpected digest length");
    }

    return hash;
}

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> keccak_256(const basic_uchar_vector<Allocator>& data)
{
    basic_uchar_vector<Allocator> hash(32);
    sph_keccak256_context ctx_keccak;
    sph_keccak256_init(&ctx_keccak);
    sph_keccak256(&ctx_keccak, (unsigned char*)&data[0], data.size());
    sph_keccak256_close(&ctx_keccak, (unsigned char*)&hash[0]);
    return hash;
}

template<typename Allocator = std::allocator<unsigned char> >
inline basic_uchar_vector<Allocator> scrypt_1024_1_1_256(const basic_uchar_vector<Allocator>& data)
{
#if OPENSSL_VERSION_NUMBER < 0x10101000L
    if (data.size() != 80)
    {
        throw std::runtime_error("scrypt_1024_1_1_256(): data size should be 80");
    }
    uint256 hash;
    scrypt_1024_1_1_256_((const char*)&data[0], (char*)&hash);
    return basic_uchar_vector<Allocator>((unsigned char*)&hash, (unsigned char*)&hash + 32);
#else

    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_SCRYPT, nullptr);
    if (pctx == nullptr)
    {
        throw std::runtime_error("scrypt_1024_1_1_256(): couldn't create context");
    }

    if (EVP_PKEY_derive_init(pctx) <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("scrypt_1024_1_1_256(): couldn't init context");
    }

    if (EVP_PKEY_CTX_set1_pbe_pass(pctx, (const char*)data.data(), data.size()) <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("scrypt_1024_1_1_256(): couldn't set password");
    }

    if (EVP_PKEY_CTX_set1_scrypt_salt(pctx, (const unsigned char*)data.data(), data.size()) <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("scrypt_1024_1_1_256(): couldn't set salt");
    }

    if (EVP_PKEY_CTX_set_scrypt_N(pctx, 1024) <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("scrypt_1024_1_1_256(): couldn't set cost");
    }

    if (EVP_PKEY_CTX_set_scrypt_r(pctx, 1) <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("scrypt_1024_1_1_256(): couldn't set r");
    }

    if (EVP_PKEY_CTX_set_scrypt_p(pctx, 1) <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("scrypt_1024_1_1_256(): couldn't set p");
    }

    size_t outlen = 32;
    basic_uchar_vector<Allocator> output(outlen);
    if (EVP_PKEY_derive(pctx, (unsigned char*)output.data(), &outlen) <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("scrypt_1024_1_1_256(): couldn't derive key");
    }

    EVP_PKEY_CTX_free(pctx);

    return basic_uchar_vector<Allocator>(output.begin(), output.end());
#endif
}

#endif  // __HASH_H__
