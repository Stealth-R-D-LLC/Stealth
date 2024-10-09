// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2011-2017 The Peercoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <map>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#include <openssl/ecdsa.h>
#endif

#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/core_names.h>

#include <openssl/param_build.h>

#include <openssl/obj_mac.h>

#include "key.h"

#define MAX_GROUP_NAME_LEN 256


int EVP_PKEY_regenerate_EC_key(EVP_PKEY **ppkey, BIGNUM *priv_key)
{
    if (ppkey == nullptr)
    {
        return -10;
    }
    if (*ppkey == nullptr)
    {
        return -20;
    }
    if (priv_key == nullptr)
    {
        return -30;
    }

    if (EVP_PKEY_get_base_id(*ppkey) != EVP_PKEY_EC)
    {
        return -40;
    }

    char group_name[MAX_GROUP_NAME_LEN];
    size_t group_name_len = 0;
    if (EVP_PKEY_get_group_name(*ppkey,
                                group_name,
                                sizeof(group_name),
                                &group_name_len) != 1)
    {
        return -50;
    }

    EC_GROUP *group = EC_GROUP_new_by_curve_name(OBJ_txt2nid(group_name));
    if (group == nullptr)
    {
        return -60;
    }

    EC_POINT *pub_key = EC_POINT_new(group);
    if (pub_key == nullptr)
    {
        EC_GROUP_free(group);
        return -70;
    }


    if (EC_POINT_mul(group, pub_key, priv_key, nullptr, nullptr, nullptr) != 1)
    {
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        return -80;
    }


    unsigned char *pub_key_bytes = nullptr;
    size_t pub_key_bytes_len = EC_POINT_point2oct(group, pub_key,
                                                  POINT_CONVERSION_UNCOMPRESSED,
                                                  nullptr, 0, nullptr);
    pub_key_bytes = (unsigned char *)OPENSSL_malloc(pub_key_bytes_len);
    if (pub_key_bytes == nullptr)
    {
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        return -90;
    }


    if (EC_POINT_point2oct(group, pub_key,
                           POINT_CONVERSION_UNCOMPRESSED,
                           pub_key_bytes,
                           pub_key_bytes_len,
                           nullptr) != pub_key_bytes_len)
    {
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        return -100;
    }

    OSSL_PARAM_BLD *param_bld = OSSL_PARAM_BLD_new();
    if (param_bld == nullptr)
    {
        EC_POINT_free(pub_key);
        OPENSSL_free(pub_key_bytes);
        EC_GROUP_free(group);
        return -110;
    }

    if (OSSL_PARAM_BLD_push_utf8_string(param_bld,
                                        OSSL_PKEY_PARAM_GROUP_NAME,
                                        group_name, 0) != 1 ||
        OSSL_PARAM_BLD_push_BN(param_bld,
                               OSSL_PKEY_PARAM_PRIV_KEY,
                               priv_key) != 1 ||
        OSSL_PARAM_BLD_push_octet_string(param_bld,
                                         OSSL_PKEY_PARAM_PUB_KEY,
                                         pub_key_bytes,
                                         pub_key_bytes_len) != 1)
    {
        OPENSSL_free(pub_key_bytes);
        OSSL_PARAM_BLD_free(param_bld);
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        return -120;
    }

    OSSL_PARAM *params = OSSL_PARAM_BLD_to_param(param_bld);
    if (params == nullptr)
    {
        OPENSSL_free(pub_key_bytes);
        OSSL_PARAM_BLD_free(param_bld);
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        return -130;
    }

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
    if (ctx == nullptr)
    {
        OPENSSL_free(pub_key_bytes);
        OSSL_PARAM_BLD_free(param_bld);
        OSSL_PARAM_free(params);
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        return -140;
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0)
    {
        OPENSSL_free(pub_key_bytes);
        OSSL_PARAM_BLD_free(param_bld);
        OSSL_PARAM_free(params);
        EVP_PKEY_CTX_free(ctx);
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        return -150;
    }

    EVP_PKEY *pnew_key = nullptr;
    int fFromDataResult = EVP_PKEY_fromdata(ctx,
                                            &pnew_key,
                                            EVP_PKEY_KEYPAIR,
                                            params);
    if (fFromDataResult <= 0)
    {
        OPENSSL_free(pub_key_bytes);
        OSSL_PARAM_BLD_free(param_bld);
        OSSL_PARAM_free(params);
        EVP_PKEY_CTX_free(ctx);
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        return -160;
    }

    // Swap *ppkey
    EVP_PKEY_free(*ppkey);
    *ppkey = pnew_key;

    OPENSSL_free(pub_key_bytes);
    OSSL_PARAM_BLD_free(param_bld);
    OSSL_PARAM_free(params);
    EVP_PKEY_CTX_free(ctx);
    EC_POINT_free(pub_key);
    EC_GROUP_free(group);

    return 1;
}


// Perform ECDSA key recovery (see SEC1 4.1.6) for curves over (mod p)-fields
// recid selects which key is recovered
// if check is non-zero, additional checks are performed
int ECDSA_SIG_recover_key_GFp_EVP(EVP_PKEY *evpeckey,
                                  ECDSA_SIG *ecsig,
                                  const unsigned char *msg,
                                  int msglen,
                                  int recid,
                                  int check)
{
    if (!evpeckey) return 0;

    const BIGNUM *sig_r, *sig_s;
    ECDSA_SIG_get0(ecsig, &sig_r, &sig_s);

    int ret = 0;
    BN_CTX *ctx = NULL;
    EC_GROUP *group = NULL;
    BIGNUM *x = NULL;
    BIGNUM *e = NULL;
    BIGNUM *order = NULL;
    BIGNUM *sor = NULL;
    BIGNUM *eor = NULL;
    BIGNUM *field = NULL;
    EC_POINT *R = NULL;
    EC_POINT *O = NULL;
    EC_POINT *Q = NULL;
    BIGNUM *rr = NULL;
    BIGNUM *zero = NULL;
    int n = 0;
    int i = recid / 2;
    char group_name[MAX_GROUP_NAME_LEN];
    size_t group_name_len = 0;
    EVP_PKEY_CTX *pkey_ctx = NULL;
    OSSL_PARAM_BLD *param_bld = NULL;
    unsigned char *pub_key_oct = NULL;
    size_t pub_key_oct_len = 0;
    OSSL_PARAM *params = NULL;

    if (EVP_PKEY_get_group_name(evpeckey,
                                group_name,
                                sizeof(group_name),
                                &group_name_len) != 1)
    {
        ret = -50;
        goto err;
    }

    group = EC_GROUP_new_by_curve_name(OBJ_txt2nid(group_name));
    if (group == NULL)
    {
        ret = -60;
        goto err;
    }

    if ((ctx = BN_CTX_new()) == NULL)
    {
        ret = -1;
        goto err;
    }
    BN_CTX_start(ctx);

    order = BN_CTX_get(ctx);
    x = BN_CTX_get(ctx);
    field = BN_CTX_get(ctx);
    e = BN_CTX_get(ctx);
    zero = BN_CTX_get(ctx);
    rr = BN_CTX_get(ctx);
    sor = BN_CTX_get(ctx);
    eor = BN_CTX_get(ctx);

    if (!order || !x || !field || !e || !zero || !rr || !sor || !eor)
    {
        ret = -1;
        goto err;
    }

    if (!EC_GROUP_get_order(group, order, ctx))
    {
       ret = -2;
       goto err;
    }

    if (!BN_copy(x, order))
    {
        ret = -1;
        goto err;
    }
    if (!BN_mul_word(x, i))
    {
        ret = -1;
        goto err;
    }
    if (!BN_add(x, x, sig_r))
    {
        ret = -1;
        goto err;
    }

    if (!EC_GROUP_get_curve(group, field, NULL, NULL, ctx))
    {
        ret = -2;
        goto err;
    }

    if (BN_cmp(x, field) >= 0)
    {
        ret = 0;
        goto err;
    }

    R = EC_POINT_new(group);
    if (R == NULL)
    {
        ret = -2;
        goto err;
    }

    if (!EC_POINT_set_compressed_coordinates(group, R, x, recid % 2, ctx))
    {
        ret = 0;
        goto err;
    }

    if (check)
    {
        O = EC_POINT_new(group);
        if (O == NULL)
        {
            ret = -2;
            goto err;
        }

        if (!EC_POINT_mul(group, O, NULL, R, order, ctx))
        {
            ret = -2;
            goto err;
        }
        if (!EC_POINT_is_at_infinity(group, O))
        {
            ret = 0;
            goto err;
        }
    }

    Q = EC_POINT_new(group);
    if (Q == NULL)
    {
        ret = -2;
        goto err;
    }

    n = EC_GROUP_get_degree(group);

    if (!BN_bin2bn(msg, msglen, e))
    {
        ret = -1;
        goto err;
    }
    if (8 * msglen > n)
    {
        BN_rshift(e, e, 8 - (n & 7));
    }

    BN_zero(zero);
    if (BN_is_zero(zero) == 0)
    {
        ret = -1;
        goto err;
    }
    if (!BN_mod_sub(e, zero, e, order, ctx))
    {
        ret = -1;
        goto err;
    }
    if (!BN_mod_inverse(rr, sig_r, order, ctx))
    {
        ret = -1;
        goto err;
    }
    if (!BN_mod_mul(sor, sig_s, rr, order, ctx))
    {
        ret = -1;
        goto err;
    }
    if (!BN_mod_mul(eor, e, rr, order, ctx))
    {
        ret = -1;
        goto err;
    }
    if (!EC_POINT_mul(group, Q, eor, R, sor, ctx))
    {
        ret = -2;
        goto err;
    }

    pkey_ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    if (pkey_ctx == NULL)
    {
        ret = -90;
        goto err;
    }

    if (EVP_PKEY_fromdata_init(pkey_ctx) <= 0)
    {
        ret = -100;
        goto err;
    }

    param_bld = OSSL_PARAM_BLD_new();
    if (param_bld == NULL)
    {
        ret = -110;
        goto err;
    }

    pub_key_oct_len = EC_POINT_point2oct(group, Q,
                                         POINT_CONVERSION_COMPRESSED,
                                         NULL, 0, NULL);
    pub_key_oct = (unsigned char *)OPENSSL_malloc(pub_key_oct_len);
    if (pub_key_oct == NULL)
    {
        ret = -120;
        goto err;
    }

    if (EC_POINT_point2oct(group, Q,
                           POINT_CONVERSION_COMPRESSED,
                           pub_key_oct, pub_key_oct_len, NULL) == 0)
    {
        ret = -130;
        goto err;
    }

    if (OSSL_PARAM_BLD_push_utf8_string(param_bld,
                                        OSSL_PKEY_PARAM_GROUP_NAME,
                                        group_name, 0) != 1 ||
        OSSL_PARAM_BLD_push_octet_string(param_bld,
                                         OSSL_PKEY_PARAM_PUB_KEY,
                                         pub_key_oct, pub_key_oct_len) != 1)
    {
        ret = -160;
        goto err;
    }

    params = OSSL_PARAM_BLD_to_param(param_bld);
    if (params == NULL)
    {
        ret = -170;
        goto err;
    }

    if (EVP_PKEY_fromdata(pkey_ctx, &evpeckey, EVP_PKEY_PUBLIC_KEY, params) <= 0)
    {
        ret = -180;
        goto err;
    }

    ret = 1;

err:
    if (ctx)
    {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
    EC_POINT_free(R);
    EC_POINT_free(O);
    EC_POINT_free(Q);
    EC_GROUP_free(group);
    OSSL_PARAM_BLD_free(param_bld);
    EVP_PKEY_CTX_free(pkey_ctx);
    OSSL_PARAM_free(params);
    OPENSSL_free(pub_key_oct);

    return ret;
}

int CKey::SetPubKeyCompression(bool fCompressed)
{
    if (pkey == nullptr)
    {
        return -1;
    }

    if (EVP_PKEY_id(pkey) != EVP_PKEY_EC)
    {
        return -2;
    }

    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string(
                    OSSL_PKEY_PARAM_EC_POINT_CONVERSION_FORMAT,
                    fCompressed ? const_cast<char*>("compressed") :
                                  const_cast<char*>("uncompressed"),
                    0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_PKEY_set_params(pkey, params) <= 0)
    {
        return 0;
    }

    fCompressedPubKey = fCompressed;
    return 1;
}

void CKey::SetCompressedPubKey()
{
    SetPubKeyCompression(true);
    fCompressedPubKey = true;
}

void CKey::SetUnCompressedPubKey()
{
    SetPubKeyCompression(false);
    fCompressedPubKey = false;
}

EVP_PKEY* CKey::GetEVPKey()
{
    return pkey;
}

void CKey::Reset()
{
    fCompressedPubKey = false;
    if (pkey != NULL)
    {
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (ctx == NULL)
    {
        throw key_error("CKey::Reset() : EVP_PKEY_CTX_new_id failed");
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        throw key_error("CKey::Reset() : EVP_PKEY_keygen_init failed");
    }

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_secp256k1) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        throw key_error("CKey::Reset() : EVP_PKEY_CTX_set_ec_paramgen_curve_nid failed");
    }

    if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        throw key_error("CKey::Reset() : EVP_PKEY_keygen failed");
    }

    EVP_PKEY_CTX_free(ctx);
    fSet = false;
}

CKey::CKey()
{
    pkey = NULL;
    Reset();
}

CKey::CKey(const CKey& other)
{
    pkey = EVP_PKEY_dup(other.pkey);
    if (pkey == NULL)
    {
        throw key_error("CKey::CKey(const CKey&) : EVP_PKEY_dup failed");
    }
    fSet = other.fSet;
    fCompressedPubKey = other.fCompressedPubKey;
}

CKey& CKey::operator=(const CKey& other)
{
    if (this != &other)
    {
        EVP_PKEY_free(pkey);
        pkey = EVP_PKEY_dup(other.pkey);
        if (!pkey)
        {
            throw key_error(
                "CKey::operator=(const CKey&) : EVP_PKEY_dup failed");
        }
        fSet = other.fSet;
        fCompressedPubKey = other.fCompressedPubKey;
    }
    return *this;
}

CKey::~CKey()
{
    if (pkey != NULL) {
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }
}

bool CKey::IsNull() const
{
    return !fSet;
}

bool CKey::IsSet() const
{
    return fSet;
}

bool CKey::IsCompressed() const
{
    return fCompressedPubKey;
}

int CompareBigEndian(const unsigned char* c1,
                     size_t c1len,
                     const unsigned char* c2,
                     size_t c2len)
{
    while (c1len > c2len)
    {
        if (*c1)
        {
            return 1;
        }
        c1++;
        c1len--;
    }
    while (c2len > c1len)
    {
        if (*c2)
        {
            return -1;
        }
        c2++;
        c2len--;
    }
    while (c1len > 0)
    {
        if (*c1 > *c2)
        {
            return 1;
        }
        if (*c2 > *c1)
        {
            return -1;
        }
        c1++;
        c2++;
        c1len--;
    }
    return 0;
}

// Order of secp256k1's generator minus 1.
const unsigned char vchMaxModOrder[32] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,
    0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,0x3B,
    0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x40
};

// Half of the order of secp256k1's generator minus 1.
const unsigned char vchMaxModHalfOrder[32] = {
    0x7F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x5D,0x57,0x6E,0x73,0x57,0xA4,0x50,0x1D,
    0xDF,0xE9,0x2F,0x46,0x68,0x1B,0x20,0xA0
};

const unsigned char vchZero[0] = {};

bool CKey::CheckSignatureElement(const unsigned char *vch, int len, bool half) {
    return CompareBigEndian(vch, len, vchZero, 0) > 0 &&
           CompareBigEndian(vch, len, half ? vchMaxModHalfOrder : vchMaxModOrder, 32) <= 0;
}


void CKey::MakeNewKey(bool fCompressed)
{
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (!ctx)
    {
        throw key_error("CKey::MakeNewKey() : EVP_PKEY_CTX_new_id failed");
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        throw key_error("CKey::MakeNewKey() : EVP_PKEY_keygen_init failed");
    }

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_secp256k1) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        throw key_error("CKey::MakeNewKey() : "
                           "EVP_PKEY_CTX_set_ec_paramgen_curve_nid failed");
    }

    if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        throw key_error("CKey::MakeNewKey() : EVP_PKEY_keygen failed");
    }

    EVP_PKEY_CTX_free(ctx);
    ctx = NULL;

    if (fCompressed)
    {
        SetCompressedPubKey();
    }

    fSet = true;
}

bool CKey::SetPrivKey(const CPrivKey& vchPrivKey)
{
    const unsigned char* pbegin = vchPrivKey.data();
    EVP_PKEY* new_pkey = d2i_PrivateKey(EVP_PKEY_EC,
                                        nullptr,
                                        &pbegin,
                                        vchPrivKey.size());

    if (new_pkey)
    {
        // d2i_ECPrivateKey may create a new key,
        // but fill in pkey with a key that fails checks
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(new_pkey, nullptr);
        if (ctx)
        {
            if (EVP_PKEY_private_check(ctx) == 1)
            {
                if (pkey)
                {
                    EVP_PKEY_free(pkey);
                }

                pkey = new_pkey;
                fSet = true;
                EVP_PKEY_CTX_free(ctx);
                return true;
            }
            EVP_PKEY_CTX_free(ctx);
            ctx = NULL;
        }

        EVP_PKEY_free(new_pkey);
        new_pkey = NULL;
    }

    // If we've reached here, something went wrong
    Reset();
    return false;
}

bool CKey::SetSecret(const CSecret& vchSecret, bool fCompressed)
{
    if (vchSecret.size() != 32)
    {
        throw key_error("CKey::SetSecret() : secret must be 32 bytes");
    }

    BIGNUM *bn = BN_bin2bn(&vchSecret[0], 32, BN_new());
    if (bn == NULL)
    {
        throw key_error("CKey::SetSecret() : BN_bin2bn failed");
    }

    if (pkey == NULL)
    {
        pkey = EVP_PKEY_new();
        if (pkey == NULL)
        {
            BN_clear_free(bn);
            throw key_error("CKey::SetSecret() : EVP_PKEY_new failed");
        }
    }

    int result = EVP_PKEY_regenerate_EC_key(&pkey, bn);
    if (result != 1)
    {
        BN_clear_free(bn);
        throw key_error("CKey::SetSecret() : EVP_PKEY_regenerate_EC_key failed");
    }

    BN_clear_free(bn);
    bn = NULL;

    fSet = true;

    if (fCompressed || fCompressedPubKey)
    {
        SetCompressedPubKey();
    }

    return true;
}

CSecret CKey::GetSecret(bool &fCompressed) const
{
    CSecret vchRet;
    vchRet.resize(32);

    BIGNUM *bn = NULL;
    if (!EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY, &bn))
    {
        throw key_error("CKey::GetSecret() : EVP_PKEY_get_bn_param failed");
    }

    int nBytes = BN_num_bytes(bn);
    if (nBytes > 32)
    {
        BN_free(bn);
        throw key_error("CKey::GetSecret() : private key is too large");
    }

    int n = BN_bn2binpad(bn, vchRet.data(), 32);
    BN_free(bn);
    bn = NULL;

    if (n != 32)
    {
        throw key_error("CKey::GetSecret(): BN_bn2binpad failed");
    }

    fCompressed = fCompressedPubKey;
    return vchRet;
}

CPrivKey CKey::GetPrivKey() const
{
    if (!pkey)
    {
        throw key_error("CKey::GetPrivKey() : pkey is null");
    }

    unsigned char *buffer = nullptr;
    int size = i2d_PrivateKey(pkey, &buffer);

    if (size <= 0)
    {
        OPENSSL_free(buffer);
        throw key_error("CKey::GetPrivKey() : i2d_PrivateKey failed");
    }

    CPrivKey vchPrivKey(buffer, buffer + size);
    OPENSSL_free(buffer);
    buffer = NULL;

    return vchPrivKey;
}

bool CKey::SetPubKey(const CPubKey& vchPubKey,
                     const char* pchGroupName)
{
    EVP_PKEY_free(pkey);
    pkey = NULL;

    OSSL_PARAM_BLD* param_bld = OSSL_PARAM_BLD_new();
    if (param_bld == NULL)
    {
        return false;
    }

    if (OSSL_PARAM_BLD_push_utf8_string(param_bld,
                                        OSSL_PKEY_PARAM_GROUP_NAME,
                                        pchGroupName, 0) != 1 ||
        OSSL_PARAM_BLD_push_octet_string(param_bld,
                                         OSSL_PKEY_PARAM_PUB_KEY,
                                         vchPubKey.Data(),
                                         vchPubKey.Size()) != 1)
    {
        OSSL_PARAM_BLD_free(param_bld);
        return false;
    }

    OSSL_PARAM* params = OSSL_PARAM_BLD_to_param(param_bld);
    if (params == NULL)
    {
        OSSL_PARAM_BLD_free(param_bld);
        return false;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    if (ctx == NULL)
    {
        OSSL_PARAM_free(params);
        OSSL_PARAM_BLD_free(param_bld);
        return false;
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
        EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(params);
        OSSL_PARAM_BLD_free(param_bld);
        return false;
    }

    EVP_PKEY_CTX_free(ctx);
    ctx = NULL;
    OSSL_PARAM_free(params);
    params = NULL;
    OSSL_PARAM_BLD_free(param_bld);
    param_bld = NULL;

    fSet = true;
    if (vchPubKey.Size() == 33)
    {
        SetCompressedPubKey();
    }
    else
    {
        SetUnCompressedPubKey();
    }
    return true;

}

CPubKey CKey::GetPubKey() const
{
    if (!pkey)
    {
        throw key_error("CKey::GetPubKey() : pkey is null");
    }

    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string(
                        OSSL_PKEY_PARAM_EC_POINT_CONVERSION_FORMAT,
                        fCompressedPubKey ?
                              const_cast<char*>("compressed") :
                              const_cast<char*>("uncompressed"),
                        0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_PKEY_set_params(pkey, params) <= 0)
    {
        throw key_error("CKey::GetPubKey() : "
                           "EVP_PKEY_set_params failed");
    }

    size_t pubkey_len = 0;
    if (EVP_PKEY_get_octet_string_param(pkey,
                                        OSSL_PKEY_PARAM_PUB_KEY,
                                        NULL,
                                        0,
                                        &pubkey_len) <= 0)
    {
        throw key_error("CKey::GetPubKey() : "
                           "EVP_PKEY_get_octet_string_param "
                           "failed to get length");
    }

    std::vector<unsigned char> vchPubKey(pubkey_len, 0);

    if (EVP_PKEY_get_octet_string_param(pkey,
                                        OSSL_PKEY_PARAM_PUB_KEY,
                                        vchPubKey.data(),
                                        pubkey_len,
                                        &pubkey_len) <= 0)
    {
        throw key_error("CKey::GetPubKey() : "
                           "EVP_PKEY_get_octet_string_param "
                           "failed to get key");
    }

    return CPubKey(vchPubKey);
}

// The data to be signed, `hash`, is expected to be an SHA256 digest of the
//    original message.
bool CKey::Sign(const uint256& hash, std::vector<unsigned char>& vchSig)
{
    vchSig.clear();

    char group_name[MAX_GROUP_NAME_LEN];
    size_t group_name_len = 0;
    if (EVP_PKEY_get_group_name(pkey, group_name, sizeof(group_name), &group_name_len) != 1)
    {
        return false;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!ctx)
    {
        return false;
    }

    if (EVP_PKEY_sign_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    size_t siglen;
    if (EVP_PKEY_sign(ctx,
                      nullptr,
                      &siglen,
                      reinterpret_cast<const unsigned char*>(&hash),
                      sizeof(hash)) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    vchSig.resize(siglen);
    if (EVP_PKEY_sign(ctx,
                      vchSig.data(),
                      &siglen,
                      reinterpret_cast<const unsigned char*>(&hash),
                      sizeof(hash)) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    EVP_PKEY_CTX_free(ctx);
    ctx = NULL;

    const unsigned char* sigdata = vchSig.data();
    ECDSA_SIG* sig = d2i_ECDSA_SIG(nullptr, &sigdata, vchSig.size());
    if (!sig)
    {
        return false;
    }

    BIGNUM* order = BN_new();
    BIGNUM* halforder = BN_new();
    EC_GROUP* group = EC_GROUP_new_by_curve_name(OBJ_txt2nid(group_name));

    if (!order || !halforder || !group)
    {
        ECDSA_SIG_free(sig);
        BN_free(order);
        BN_free(halforder);
        EC_GROUP_free(group);
        return false;
    }

    if (EC_GROUP_get_order(group, order, nullptr) != 1)
    {
        ECDSA_SIG_free(sig);
        BN_free(order);
        BN_free(halforder);
        EC_GROUP_free(group);
        return false;
    }

    BN_rshift1(halforder, order);

    const BIGNUM* sig_r;
    const BIGNUM* sig_s;
    ECDSA_SIG_get0(sig, &sig_r, &sig_s);

    if (BN_cmp(sig_s, halforder) > 0)
    {
        BIGNUM* s_new = BN_new();
        if (!s_new)
        {
            ECDSA_SIG_free(sig);
            BN_free(order);
            BN_free(halforder);
            EC_GROUP_free(group);
            return false;
        }

        BN_sub(s_new, order, sig_s);

        BIGNUM* r_new = BN_dup(sig_r);
        if (!r_new)
        {
            BN_free(s_new);
            ECDSA_SIG_free(sig);
            BN_free(order);
            BN_free(halforder);
            EC_GROUP_free(group);
            return false;
        }

        ECDSA_SIG_set0(sig, r_new, s_new);
    }

    unsigned char* pos = &vchSig[0];
    int nSize = i2d_ECDSA_SIG(sig, &pos);

    ECDSA_SIG_free(sig);
    sig = NULL;
    BN_free(order);
    order = NULL;
    BN_free(halforder);
    halforder = NULL;
    EC_GROUP_free(group);
    group = NULL;

    if (nSize <= 0)
    {
        return false;
    }

    vchSig.resize(nSize);
    return true;
}

// create a compact signature (65 bytes), which allows reconstructing the used public key
// The data to be signed, `hash`, is expected to be an SHA256 digest of the original message.
// The format is one header byte, followed by two times 32 bytes for the serialized r and s values.
// The header byte: 0x1B = first key with even y, 0x1C = first key with odd y,
//                  0x1D = second key with even y, 0x1E = second key with odd y
bool CKey::SignCompact(uint256 hash, std::vector<unsigned char>& vchSig)
{
    bool fOk = false;
    vchSig.clear();
    vchSig.resize(65, 0);

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx)
    {
        return false;
    }

    if (EVP_PKEY_sign_init(ctx) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    if (EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    size_t siglen;
    if (EVP_PKEY_sign(ctx,
                      NULL,
                      &siglen,
                      (unsigned char*) &hash,
                      sizeof(hash)) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    std::vector<unsigned char> signature(siglen);
    if (EVP_PKEY_sign(ctx,
                      signature.data(),
                      &siglen,
                      (unsigned char*) &hash,
                      sizeof(hash)) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    EVP_PKEY_CTX_free(ctx);
    ctx = NULL;

    const unsigned char* p = signature.data();
    ECDSA_SIG* sig = d2i_ECDSA_SIG(NULL, &p, siglen);
    if (!sig)
    {
        return false;
    }

    const BIGNUM *sig_r, *sig_s;
    ECDSA_SIG_get0(sig, &sig_r, &sig_s);

    int nBitsR = BN_num_bits(sig_r);
    int nBitsS = BN_num_bits(sig_s);
    if (nBitsR <= 256 && nBitsS <= 256)
    {
        int nRecId = -1;
        for (int i = 0; i < 4; i++)
        {
            CKey keyRec;
            keyRec.fSet = true;
            if (fCompressedPubKey)
            {
                keyRec.SetCompressedPubKey();
            }
            if (ECDSA_SIG_recover_key_GFp_EVP(keyRec.pkey, sig,
                                              (unsigned char*) &hash,
                                              sizeof(hash),
                                              i, 1) == 1)
            {
                if (keyRec.GetPubKey() == this->GetPubKey())
                {
                    nRecId = i;
                    break;
                }
            }
        }

        if (nRecId == -1)
        {
            ECDSA_SIG_free(sig);
            throw key_error(
                "CKey::SignCompact() : unable to construct recoverable key");
        }

        vchSig[0] = nRecId + 27 + (fCompressedPubKey ? 4 : 0);
        BN_bn2bin(sig_r, &vchSig[33 - (nBitsR + 7) / 8]);
        BN_bn2bin(sig_s, &vchSig[65 - (nBitsS + 7) / 8]);
        fOk = true;
    }
    ECDSA_SIG_free(sig);
    return fOk;
}

bool CKey::SetCompactSignature(uint256 hash,
                               const std::vector<unsigned char>& vchSig)
{
    if (vchSig.size() != 65)
    {
        return false;
    }
    int nV = vchSig[0];
    if (nV < 27 || nV >= 35)
    {
        return false;
    }

    ECDSA_SIG* sig = ECDSA_SIG_new();
    if (!sig)
    {
        return false;
    }

    BIGNUM* sig_r = BN_bin2bn(&vchSig[1], 32, NULL);
    BIGNUM* sig_s = BN_bin2bn(&vchSig[33], 32, NULL);
    if (!sig_r || !sig_s)
    {
        BN_free(sig_r);
        BN_free(sig_s);
        ECDSA_SIG_free(sig);
        return false;
    }

    ECDSA_SIG_set0(sig, sig_r, sig_s);

    EVP_PKEY_free(pkey);
    pkey = NULL;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (!ctx)
    {
        ECDSA_SIG_free(sig);
        return false;
    }

    if (EVP_PKEY_paramgen_init(ctx) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        ECDSA_SIG_free(sig);
        return false;
    }

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_secp256k1) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        ECDSA_SIG_free(sig);
        return false;
    }

    if (EVP_PKEY_paramgen(ctx, &pkey) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        ECDSA_SIG_free(sig);
        return false;
    }

    EVP_PKEY_CTX_free(ctx);
    ctx = NULL;

    if (nV >= 31)
    {
        SetCompressedPubKey();
        nV -= 4;
    }

    if (ECDSA_SIG_recover_key_GFp_EVP(pkey,
                                      sig,
                                      (unsigned char*) &hash,
                                      sizeof(hash),
                                      nV - 27,
                                      0) == 1)
    {
        fSet = true;
        ECDSA_SIG_free(sig);
        return true;
    }
    ECDSA_SIG_free(sig);
    return false;
}

static bool ParseLength(const std::vector<unsigned char>::iterator& begin,
                        const std::vector<unsigned char>::iterator& end,
                        size_t& nLengthRet,
                        size_t& nLengthSizeRet)
{
    std::vector<unsigned char>::iterator it = begin;
    if (it == end)
    {
        return false;
    }

    nLengthRet = *it;
    nLengthSizeRet = 1;

    if (!(nLengthRet & 0x80))
    {
        return true;
    }

    unsigned char nLengthBytes = nLengthRet & 0x7f;

    // Lengths on more than 8 bytes are rejected by OpenSSL 64 bits
    if (nLengthBytes > 8)
    {
        return false;
    }

    int64_t nLength = 0;
    for (unsigned char i = 0; i < nLengthBytes; i++)
    {
        it++;
        if (it == end)
        {
            return false;
        }
        nLength = (nLength << 8) | *it;
        if (nLength > 0x7fffffff)
        {
            return false;
        }
        nLengthSizeRet++;
    }
    nLengthRet = nLength;
    return true;
}

static std::vector<unsigned char> EncodeLength(size_t nLength)
{
    std::vector<unsigned char> vchRet;
    if (nLength < 0x80)
    {
        vchRet.push_back(nLength);
    }
    else
    {
        vchRet.push_back(0x84);
        vchRet.push_back((nLength >> 24) & 0xff);
        vchRet.push_back((nLength >> 16) & 0xff);
        vchRet.push_back((nLength >> 8) & 0xff);
        vchRet.push_back(nLength & 0xff);
    }
    return vchRet;
}

static bool NormalizeSignature(std::vector<unsigned char>& vchSig)
{
    // Prevent the problem described here:
    // https://lists.linuxfoundation.org/pipermail/bitcoin-dev/2015-July/009697.html
    // by removing the extra length bytes

    if (vchSig.size() < 2 || vchSig[0] != 0x30)
    {
        return false;
    }

    size_t nTotalLength, nTotalLengthSize;
    if (!ParseLength(vchSig.begin() + 1,
                     vchSig.end(),
                     nTotalLength,
                     nTotalLengthSize))
    {
        return false;
    }

    size_t nRStart = 1 + nTotalLengthSize;
    if (vchSig.size() < nRStart + 2 || vchSig[nRStart] != 0x02)
    {
        return false;
    }

    size_t nRLength, nRLengthSize;
    if (!ParseLength(vchSig.begin() + nRStart + 1,
                     vchSig.end(),
                     nRLength,
                     nRLengthSize))
    {
        return false;
    }
    const size_t nRDataStart = nRStart + 1 + nRLengthSize;
    std::vector<unsigned char> R(vchSig.begin() + nRDataStart,
                                 vchSig.begin() + nRDataStart + nRLength);

    size_t nSStart = nRStart + 1 + nRLengthSize + nRLength;
    if (vchSig.size() < nSStart + 2 || vchSig[nSStart] != 0x02)
    {
        return false;
    }

    size_t nSLength, nSLengthSize;
    if (!ParseLength(vchSig.begin() + nSStart + 1,
                     vchSig.end(),
                     nSLength,
                     nSLengthSize))
    {
        return false;
    }
    const size_t nSDataStart = nSStart + 1 + nSLengthSize;
    std::vector<unsigned char> S(vchSig.begin() + nSDataStart,
                                 vchSig.begin() + nSDataStart + nSLength);

    std::vector<unsigned char> vchRLength = EncodeLength(R.size());
    std::vector<unsigned char> vchSLength = EncodeLength(S.size());

    nTotalLength = 1 + vchRLength.size() + R.size() + 1 + vchSLength.size() +
                   S.size();
    std::vector<unsigned char> vchTotalLength = EncodeLength(nTotalLength);

    vchSig.clear();
    vchSig.reserve(1 + vchTotalLength.size() + nTotalLength);
    vchSig.push_back(0x30);
    vchSig.insert(vchSig.end(), vchTotalLength.begin(), vchTotalLength.end());

    vchSig.push_back(0x02);
    vchSig.insert(vchSig.end(), vchRLength.begin(), vchRLength.end());
    vchSig.insert(vchSig.end(), R.begin(), R.end());

    vchSig.push_back(0x02);
    vchSig.insert(vchSig.end(), vchSLength.begin(), vchSLength.end());
    vchSig.insert(vchSig.end(), S.begin(), S.end());

    return true;
}

// The data against which to verify, `hash`,
//     should be an SHA256 digest of the original message.
bool CKey::Verify(const uint256& hash,
                  const std::vector<unsigned char>& vchSigParam)
{
    std::vector<unsigned char> vchSig(vchSigParam.begin(), vchSigParam.end());

    if (!NormalizeSignature(vchSig))
    {
        return false;
    }

    if (vchSig.empty())
    {
        return false;
    }

    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!pctx)
    {
        return false;
    }

    bool ret = false;

    do
    {
        if (EVP_PKEY_verify_init(pctx) <= 0)
        {
            break;
        }

        if (EVP_PKEY_CTX_set_signature_md(pctx, EVP_sha256()) <= 0)
        {
            break;
        }

        if (EVP_PKEY_verify(pctx,
                            vchSig.data(),
                            vchSig.size(),
                            reinterpret_cast<const unsigned char*>(&hash),
                            sizeof(hash)) != 1)
        {
            break;
        }

        ret = true;
    } while (0);

    EVP_PKEY_CTX_free(pctx);
    return ret;
}

// The data against which to verify, `hash`,
//     should be an SHA256 digest of the original message.
bool CKey::VerifyCompact(uint256 hash, const std::vector<unsigned char>& vchSig)
{
    CKey key;
    if (!key.SetCompactSignature(hash, vchSig))
    {
        return false;
    }
    if (GetPubKey() != key.GetPubKey())
    {
        return false;
    }

    return true;
}

// We use a pointer here so usage is backwards compatible.
// *pErrorCodeRet is 0 on success and negative on failure.
bool CKey::IsValid(int* pErrorCodeRet)
{
    static bool (*set_error)(int*, int) = [](int* pRet, int code)
    {
        if (pRet)
        {
            *pRet = code;
        }
        return (code >= 0);
    };

    if (!fSet)
    {
        return set_error(pErrorCodeRet, -10);
    }
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!ctx)
    {
        return set_error(pErrorCodeRet, -20);
    }

    int result = EVP_PKEY_check(ctx);

    EVP_PKEY_CTX_free(ctx);
    ctx = NULL;

    if (result != 1)
    {
        return set_error(pErrorCodeRet, -30);
    }

    bool fCompr;
    CSecret secret = GetSecret(fCompr);
    CKey key2;
    key2.SetSecret(secret, fCompr);

    if (GetPubKey() != key2.GetPubKey())
    {
        return set_error(pErrorCodeRet, -40);
    }

    return set_error(pErrorCodeRet, 0);
}

bool ECC_InitSanityCheck()
{
    EVP_PKEY* pkey = EVP_EC_gen("secp256k1");
    if (pkey == NULL)
    {
        return false;
    }
    EVP_PKEY_free(pkey);

    // TODO Is there more EC functionality that could be missing?
    return true;
}
