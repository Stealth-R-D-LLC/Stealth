////////////////////////////////////////////////////////////////////////////////
//
// hdkeys.h
//
// Copyright (c) 2013-2014 Eric Lombrozo
// Copyright (c) 2011-2016 Ciphrex Corp.
//
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.
//

#ifndef COIN_HDKEYS_H
#define COIN_HDKEYS_H

#include "hash.h"
#include "typedefs.h"

#include <stdexcept>

namespace Bip32 {

const uchar_vector_secure BITCOIN_SEED("426974636f696e2073656564");

class HDSeed
{
public:
    HDSeed(const secure_bytes_t& seed,
           const uchar_vector_secure& coin_seed = BITCOIN_SEED)
    {
        uchar_vector_secure hmac = hmac_sha512(coin_seed,
                                               UCHAR_VECTOR_SECURE(seed));
        master_key_.assign(hmac.begin(), hmac.begin() + 32);
        master_chain_code_.assign(hmac.begin() + 32, hmac.end());
    }

    const secure_bytes_t& getSeed() const
    {
        return seed_;
    }

    const secure_bytes_t& getMasterKey() const
    {
        return master_key_;
    }

    const secure_bytes_t& getMasterChainCode() const
    {
        return master_chain_code_;
    }

    secure_bytes_t getExtendedKey(bool bPrivate = false) const;

private:
    secure_bytes_t seed_;
    secure_bytes_t master_key_;
    secure_bytes_t master_chain_code_;
};

class InvalidHDKeychainException : public std::runtime_error
{
public:
    InvalidHDKeychainException()
        : std::runtime_error("Keychain is invalid.") { }
};

class InvalidHDKeychainPathException : public std::runtime_error
{
public:
    InvalidHDKeychainPathException()
        : std::runtime_error("Keychain path is invalid.") { }
};

class HDKeychain
{
public:
    HDKeychain() { }
    HDKeychain(const secure_bytes_t& key,
               const secure_bytes_t& chain_code,
               uint32_t child_num = 0,
               uint32_t parent_fp = 0,
               uint32_t depth = 0);
    HDKeychain(const secure_bytes_t& extkey);
    HDKeychain(const HDKeychain& source);

    HDKeychain& operator=(const HDKeychain& rhs);

    explicit operator bool() const { return valid_; }


    bool operator==(const HDKeychain& rhs) const;
    bool operator!=(const HDKeychain& rhs) const;

    // Serialization
    secure_bytes_t extkey() const;

    // Accessor Methods
    uint32_t version() const
    {
        return version_;
    }

    int depth() const
    {
        return depth_;
    }

    uint32_t parent_fp() const
    {
        return parent_fp_;
    }

    uint32_t child_num() const
    {
        return child_num_;
    }

    const secure_bytes_t& chain_code() const
    {
        return chain_code_;
    }

    const secure_bytes_t& key() const
    {
        return key_;
    }

    secure_bytes_t privkey() const;

    const secure_bytes_t& pubkey() const
    {
        return pubkey_;
    }

    secure_bytes_t uncompressed_pubkey() const;

    bool isPrivate() const { return (key_.size() == 33 && key_[0] == 0x00); }
    secure_bytes_t hash() const; // hash is ripemd160(sha256(pubkey))
    uint32_t fp() const; // fingerprint is first 32 bits of hash
    secure_bytes_t full_hash() const; // full_hash is ripemd160(sha256(pubkey + chain_code))

    HDKeychain getPublic() const;
    HDKeychain getChild(uint32_t i) const;
    HDKeychain getChild(const std::string& path) const;

    HDKeychain getChildNode(uint32_t i, bool private_derivation = false) const
    {
        uint32_t mask = private_derivation ? 0x80000000ull : 0x00000000ull;
        return getChild(mask).getChild(i);
    }

    // Precondition: i >= 1
    secure_bytes_t getPrivateSigningKey(uint32_t i) const
    {
        // if (i == 0) throw std::runtime_error(
        //                          "Signing key index cannot be zero.");
        return getChild(i).privkey();
    }

    // Precondition: i >= 1
    secure_bytes_t getPublicSigningKey(uint32_t i,
                                       bool bCompressed = true) const
    {
        //  if (i == 0) throw std::runtime_error(
        //                          "Signing key index cannot be zero.");
        return bCompressed ? getChild(i).pubkey()
                           : getChild(i).uncompressed_pubkey();
    }

    static void setVersions(uint32_t priv_version, uint32_t pub_version)
    {
        priv_version_ = priv_version;
        pub_version_ = pub_version;
    }

    std::string toString() const;

private:
    static uint32_t priv_version_;
    static uint32_t pub_version_;

    uint32_t version_;
    unsigned char depth_;
    uint32_t parent_fp_;
    uint32_t child_num_;
    secure_bytes_t chain_code_; // 32 bytes
    secure_bytes_t key_;        // 33 bytes, first byte is 0x00 for private key

    secure_bytes_t pubkey_;

    bool valid_;

    void updatePubkey();
};
}

#endif // COIN_HDKEYS_H
