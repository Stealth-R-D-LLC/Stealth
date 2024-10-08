////////////////////////////////////////////////////////////////////////////////
//
// secp256k1_openssl.h
//
// Copyright (c) 2013-2014 Eric Lombrozo
// Copyright (c) 2011-2016 Ciphrex Corp.
// Copyright (c) 2024 Stealth R&D LLC
//
// Some portions taken from bitcoin/bitcoin,
//      Copyright (c) 2009-2013 Satoshi Nakamoto, the Bitcoin developers
//
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.
//

#pragma once

#include <stdexcept>

#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/bn.h>

#include "typedefs.h"

namespace CoinCrypto
{

class secp256k1_key
{
public:
    secp256k1_key();
    ~secp256k1_key() { EVP_PKEY_free(pKey); }

    EVP_PKEY* getKey() const { return bSet ? pKey : nullptr; }
    EVP_PKEY* newKey();
    bytes_t getPrivKey() const;
    EVP_PKEY* setPrivKey(const bytes_t& privkey);
    bytes_t getPubKey(bool bCompressed = true) const;
    EVP_PKEY* setPubKey(const bytes_t& pubkey);

private:
    EVP_PKEY* pKey;
    bool bSet;
};

class secp256k1_point
{
public:
    secp256k1_point() { init(); }
    secp256k1_point(const secp256k1_point& source);
    secp256k1_point(const bytes_t& bytes);
    ~secp256k1_point();

    secp256k1_point& operator=(const secp256k1_point& rhs);

    void bytes(const bytes_t& bytes);
    bytes_t bytes() const;

    secp256k1_point& operator+=(const secp256k1_point& rhs);
    secp256k1_point& operator*=(const bytes_t& rhs);

    const secp256k1_point operator+(const secp256k1_point& rhs) const
    {
        return secp256k1_point(*this) += rhs;
    }

    const secp256k1_point operator*(const bytes_t& rhs) const
    {
        return secp256k1_point(*this) *= rhs;
    }

    // Computes n*G + K where K is this and G is the group generator
    void generator_mul(const bytes_t& n);

    // Sets to n*G
    void set_generator_mul(const bytes_t& n);

    bool is_at_infinity() const
    {
        return EC_POINT_is_at_infinity(group, point);
    }

    void set_to_infinity()
    {
        EC_POINT_set_to_infinity(group, point);
    }

    const EC_GROUP* getGroup() const { return group; }
    const EC_POINT* getPoint() const { return point; }

protected:
    void init();

private:
    EC_GROUP* group;
    EC_POINT* point;
    BN_CTX*   ctx;
};


enum SignatureFlag
{
    SIGNATURE_ENFORCE_LOW_S = 0x1,
};

bytes_t secp256k1_sigToLowS(const bytes_t& signature);

bytes_t secp256k1_sign(const secp256k1_key& key,
                      const bytes_t& sha256_hash);

bool secp256k1_verify(const secp256k1_key& key,
                      const bytes_t& sha256_hash,
                      const bytes_t& signature,
                      int flags = 0);

}  // CoinCrypto namespace
