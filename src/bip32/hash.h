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

inline uchar_vector sha256(const uchar_vector& data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, &data[0], data.size());
    SHA256_Final(hash, &sha256);
    uchar_vector rval(hash, SHA256_DIGEST_LENGTH);
    return rval;
}

inline uchar_vector sha256d(const uchar_vector& data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, &data[0], data.size());
    SHA256_Final(hash, &sha256);
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, hash, SHA256_DIGEST_LENGTH);
    SHA256_Final(hash, &sha256);
    uchar_vector rval(hash, SHA256_DIGEST_LENGTH);
    return rval;
}

inline uchar_vector sha256_2(const uchar_vector& data)
{
    return sha256d(data);
}

inline uchar_vector ripemd160(const uchar_vector& data)
{
    unsigned char hash[RIPEMD160_DIGEST_LENGTH];
    RIPEMD160_CTX ripemd160;
    RIPEMD160_Init(&ripemd160);
    RIPEMD160_Update(&ripemd160, &data[0], data.size());
    RIPEMD160_Final(hash, &ripemd160);
    uchar_vector rval(hash, RIPEMD160_DIGEST_LENGTH);
    return rval;
}

inline uchar_vector hash160(const uchar_vector& data)
{
    return ripemd160(sha256(data));
}

inline uchar_vector mdsha(const uchar_vector& data)
{
    return ripemd160(sha256(data));
}

inline uchar_vector sha1(const uchar_vector& data)
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA_CTX sha1;
    SHA1_Init(&sha1);
    SHA1_Update(&sha1, &data[0], data.size());
    SHA1_Final(hash, &sha1);
    uchar_vector rval(hash, SHA_DIGEST_LENGTH);
    return rval;
}

inline uchar_vector hmac_sha256(const uchar_vector& key, const uchar_vector& data)
{
    unsigned char* digest = HMAC(EVP_sha256(), (unsigned char*)&key[0], key.size(), (unsigned char*)&data[0], data.size(), NULL, NULL);
    return uchar_vector(digest, 32);
}

inline uchar_vector hmac_sha512(const uchar_vector& key, const uchar_vector& data)
{
    unsigned char* digest = HMAC(EVP_sha512(), (unsigned char*)&key[0], key.size(), (unsigned char*)&data[0], data.size(), NULL, NULL);
    return uchar_vector(digest, 64);
}

inline uchar_vector hash9(const uchar_vector& data)
{
    uint256 hash = Hash9((unsigned char*)&data[0], (unsigned char*)&data[0] + data.size());
    return uchar_vector((unsigned char*)&hash, (unsigned char*)&hash + 32);
}

inline uchar_vector sha3_256(const uchar_vector& data)
{
    uchar_vector hash(32);
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

inline uchar_vector keccak_256(const uchar_vector& data)
{
    uchar_vector hash(32);
    sph_keccak256_context ctx_keccak;
    sph_keccak256_init(&ctx_keccak);
    sph_keccak256(&ctx_keccak, (unsigned char*)&data[0], data.size());
    sph_keccak256_close(&ctx_keccak, (unsigned char*)&hash[0]);
    return hash;
}

inline uchar_vector scrypt_1024_1_1_256(const uchar_vector& data)
{
#if OPENSSL_VERSION_NUMBER < 0x10101000L
    uint256 hash;
    if (data.size() != 80)
    {
        throw std::runtime_error("scrypt_1024_1_1_256(): data size should be 80");
    }
    scrypt_1024_1_1_256_((const char*)&data[0], (char*)&hash);
    return uchar_vector((unsigned char*)&hash, (unsigned char*)&hash + 32);
#else
    uchar_vector output(32);

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

    if (EVP_PKEY_CTX_set1_pbe_pass(pctx, data.data(), data.size()) <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("scrypt_1024_1_1_256(): couldn't set password");
    }

    if (EVP_PKEY_CTX_set1_scrypt_salt(pctx, data.data(), data.size()) <= 0)
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

    size_t outlen = output.size();
    if (EVP_PKEY_derive(pctx, output.data(), &outlen) <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("scrypt_1024_1_1_256(): couldn't derive key");
    }

    EVP_PKEY_CTX_free(pctx);

    return output;
#endif
}

#endif  // __HASH_H__
