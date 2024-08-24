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

#include "secp256k1_openssl.h"
#include "hash.h"

#include <string>
#include <cassert>
#include <stdexcept>

#include <openssl/err.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>

using namespace std;
using namespace CoinCrypto;

const uchar_vector SECP256K1_FIELD_MOD(
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F");
const uchar_vector SECP256K1_GROUP_ORDER(
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141");
const uchar_vector SECP256K1_GROUP_HALFORDER(
    "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF5D576E7357A4501DDFE92F46681B20A0");

static void handleErrors()
{
    throw runtime_error(ERR_error_string(ERR_get_error(), NULL));
}

///////////////////////////////////////////////////////////////////////
// Helper functions for point operations
///////////////////////////////////////////////////////////////////////
void initializeOpenSSLContexts(OSSL_LIB_CTX*& libctx,
                               EVP_PKEY_CTX*& pctx,
                               EC_GROUP*& group)
{
    libctx = OSSL_LIB_CTX_new();
    if (!libctx)
    {
        handleErrors();
    }

    pctx = EVP_PKEY_CTX_new_from_name(libctx, "EC", NULL);
    if (!pctx)
    {
        OSSL_LIB_CTX_free(libctx);
        handleErrors();
    }

    if (EVP_PKEY_keygen_init(pctx) <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        OSSL_LIB_CTX_free(libctx);
        handleErrors();
    }

    OSSL_PARAM params[] = {
        OSSL_PARAM_utf8_string("group", const_cast<char*>("secp256k1"), 0),
        OSSL_PARAM_END
    };

    if (EVP_PKEY_CTX_set_params(pctx, params) <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        OSSL_LIB_CTX_free(libctx);
        handleErrors();
    }

    group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    if (!group)
    {
        EVP_PKEY_CTX_free(pctx);
        OSSL_LIB_CTX_free(libctx);
        handleErrors();
    }
}

vector<unsigned char> getPublicKeyData(EVP_PKEY *pkey)
{
    size_t len = 0;
    if (EVP_PKEY_get_octet_string_param(pkey,
                                        OSSL_PKEY_PARAM_PUB_KEY,
                                        NULL, 0, &len) <= 0)
    {
        handleErrors();
    }

    vector<unsigned char> data(len);
    if (EVP_PKEY_get_octet_string_param(pkey,
                                        OSSL_PKEY_PARAM_PUB_KEY,
                                        data.data(), len, NULL) <= 0)
    {
        handleErrors();
    }

    return data;
}

EC_POINT* octetToECPoint(const EC_GROUP *group,
                         const vector<unsigned char>& data)
{
    EC_POINT *point = EC_POINT_new(group);
    if (!point)
    {
        handleErrors();
    }

    if (EC_POINT_oct2point(group, point,
                           data.data(), data.size(), NULL) != 1)
    {
        EC_POINT_free(point);
        handleErrors();
    }

    return point;
}

vector<unsigned char> ECPointToOctet(const EC_GROUP *group,
                                          const EC_POINT *point)
{
    size_t len = EC_POINT_point2oct(group, point,
                                    POINT_CONVERSION_COMPRESSED,
                                    NULL, 0, NULL);

    vector<unsigned char> data(len);

    if (EC_POINT_point2oct(group, point,
                           POINT_CONVERSION_COMPRESSED,
                           data.data(), len, NULL) != len)
    {
        handleErrors();
    }

    return data;
}

EVP_PKEY* createNewPKEY(EVP_PKEY_CTX *pctx,
                        const vector<unsigned char>& result_data)
{
    EVP_PKEY *new_pkey = NULL;
    if (EVP_PKEY_fromdata_init(pctx) <= 0)
    {
        handleErrors();
    }

    OSSL_PARAM key_params[] = {
        OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                               const_cast<char*>("secp256k1"),
                               0),
        OSSL_PARAM_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                const_cast<unsigned char*>(result_data.data()),
                                result_data.size()),
        OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_EC_POINT_CONVERSION_FORMAT,
                               const_cast<char*>("compressed"),
                               0),
        OSSL_PARAM_END
    };

    if (EVP_PKEY_fromdata(pctx, &new_pkey, EVP_PKEY_PUBLIC_KEY, key_params) <= 0)
    {
        handleErrors();
    }

    return new_pkey;
}

secp256k1_key::secp256k1_key() : pKey(nullptr), bSet(false)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (!ctx)
    {
        handleErrors();
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        handleErrors();
    }

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_secp256k1) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        handleErrors();
    }

    if (EVP_PKEY_keygen(ctx, &pKey) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        handleErrors();
    }

    EVP_PKEY_CTX_free(ctx);
    bSet = false;
}

EVP_PKEY* secp256k1_key::newKey()
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_pkey(NULL, pKey, NULL);
    if (!ctx)
    {
        handleErrors();
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        handleErrors();
    }

    if (EVP_PKEY_keygen(ctx, &pKey) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        handleErrors();
    }

    EVP_PKEY_CTX_free(ctx);
    bSet = true;
    return pKey;
}

bytes_t secp256k1_key::getPrivKey() const
{
    if (!bSet)
    {
        throw runtime_error("secp256k1_key::getPrivKey() : "
                               "key is not set.");
    }

    BIGNUM *priv_key = NULL;
    bool fResult = EVP_PKEY_get_bn_param(pKey,
                                         OSSL_PKEY_PARAM_PRIV_KEY,
                                         &priv_key);

    if (!fResult)
    {
        handleErrors();
    }

    bytes_t privKey(32, 0);
    BN_bn2binpad(priv_key, privKey.data(), 32);
    BN_free(priv_key);

    return privKey;
}

EVP_PKEY* secp256k1_key::setPrivKey(const bytes_t& privkey)
{
    EVP_PKEY* pKeyNew = NULL;
    EVP_PKEY_CTX* ctx = NULL;
    OSSL_PARAM_BLD* param_bld = NULL;
    OSSL_PARAM* params = NULL;
    BIGNUM* bn = NULL;
    EC_GROUP* group = NULL;
    EC_POINT* pub_key = NULL;
    unsigned char* pub_key_bytes = NULL;
    size_t pub_key_len = 0;

    bn = BN_bin2bn(privkey.data(), privkey.size(), NULL);
    if (bn == NULL)
    {
        handleErrors();
    }

    group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    if (group == NULL)
    {
        BN_free(bn);
        handleErrors();
    }

    pub_key = EC_POINT_new(group);
    if (pub_key == NULL)
    {
        EC_GROUP_free(group);
        BN_free(bn);
        handleErrors();
    }

    if (EC_POINT_mul(group, pub_key, bn, NULL, NULL, NULL) != 1)
    {
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        BN_free(bn);
        handleErrors();
    }

    pub_key_len = EC_POINT_point2oct(group, pub_key,
                                     POINT_CONVERSION_UNCOMPRESSED,
                                     NULL, 0, NULL);

    pub_key_bytes = (unsigned char*)OPENSSL_malloc(pub_key_len);
    if (pub_key_bytes == NULL)
    {
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        BN_free(bn);
        handleErrors();
    }

    if (EC_POINT_point2oct(group, pub_key,
                           POINT_CONVERSION_UNCOMPRESSED,
                           pub_key_bytes, pub_key_len, NULL) != pub_key_len)
    {
        OPENSSL_free(pub_key_bytes);
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        BN_free(bn);
        handleErrors();
    }

    param_bld = OSSL_PARAM_BLD_new();
    if (param_bld == NULL)
    {
        OPENSSL_free(pub_key_bytes);
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        BN_free(bn);
        handleErrors();
    }

    if (OSSL_PARAM_BLD_push_utf8_string(param_bld,
                                        OSSL_PKEY_PARAM_GROUP_NAME,
                                        "secp256k1", 0) != 1 ||
        OSSL_PARAM_BLD_push_BN(param_bld,
                               OSSL_PKEY_PARAM_PRIV_KEY,
                               bn) != 1 ||
        OSSL_PARAM_BLD_push_octet_string(param_bld,
                                         OSSL_PKEY_PARAM_PUB_KEY,
                                         pub_key_bytes, pub_key_len) != 1)
   {
        OSSL_PARAM_BLD_free(param_bld);
        OPENSSL_free(pub_key_bytes);
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        BN_free(bn);
        handleErrors();
    }

    params = OSSL_PARAM_BLD_to_param(param_bld);
    if (params == NULL)
    {
        OSSL_PARAM_BLD_free(param_bld);
        OPENSSL_free(pub_key_bytes);
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        BN_free(bn);
        handleErrors();
    }

    ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    if (ctx == NULL)
    {
        OSSL_PARAM_free(params);
        OSSL_PARAM_BLD_free(param_bld);
        OPENSSL_free(pub_key_bytes);
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        BN_free(bn);
        handleErrors();
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
        EVP_PKEY_fromdata(ctx, &pKeyNew, EVP_PKEY_KEYPAIR, params) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(params);
        OSSL_PARAM_BLD_free(param_bld);
        OPENSSL_free(pub_key_bytes);
        EC_POINT_free(pub_key);
        EC_GROUP_free(group);
        BN_free(bn);
        handleErrors();
    }

    // Swap keys
    if (pKey)
    {
        EVP_PKEY_free(pKey);
    }
    pKey = pKeyNew;

    EVP_PKEY_CTX_free(ctx);
    OSSL_PARAM_free(params);
    OSSL_PARAM_BLD_free(param_bld);
    OPENSSL_free(pub_key_bytes);
    EC_POINT_free(pub_key);
    EC_GROUP_free(group);
    BN_free(bn);

    bSet = true;
    return pKeyNew;
}



bytes_t secp256k1_key::getPubKey(bool bCompressed) const
{
    if (!bSet)
    {
        throw runtime_error("secp256k1_key::getPubKey() : key is not set.");
    }

    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string(
                       OSSL_PKEY_PARAM_EC_POINT_CONVERSION_FORMAT,
                       bCompressed ? const_cast<char*>("compressed") :
                                     const_cast<char*>("uncompressed"),
                       0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_PKEY_set_params(pKey, params) <= 0)
    {
        handleErrors();
    }

    unsigned char *pubkey_buf = NULL;
    size_t pubkey_len = 0;

    if (EVP_PKEY_get_octet_string_param(pKey,
                                        OSSL_PKEY_PARAM_PUB_KEY,
                                        NULL,
                                        0,
                                        &pubkey_len) <= 0)
    {
        handleErrors();
    }
    pubkey_buf = (unsigned char*)OPENSSL_malloc(pubkey_len);
    if (!pubkey_buf)
    {
        handleErrors();
    }

    if (EVP_PKEY_get_octet_string_param(pKey,
                                        OSSL_PKEY_PARAM_PUB_KEY,
                                        pubkey_buf,
                                        pubkey_len,
                                        &pubkey_len) <= 0)
    {
        OPENSSL_free(pubkey_buf);
        handleErrors();
    }

    bytes_t pubKey(pubkey_buf, pubkey_buf + pubkey_len);
    OPENSSL_free(pubkey_buf);

    return pubKey;
}


EVP_PKEY* secp256k1_key::setPubKey(const bytes_t& pubkey)
{
    EVP_PKEY* pKeyNew = NULL;
    EVP_PKEY_CTX* ctx = NULL;
    OSSL_PARAM_BLD* param_bld = NULL;
    OSSL_PARAM* params = NULL;

    param_bld = OSSL_PARAM_BLD_new();
    if (param_bld == NULL)
    {
        handleErrors();
    }

    if (OSSL_PARAM_BLD_push_utf8_string(param_bld,
                                        OSSL_PKEY_PARAM_GROUP_NAME,
                                        "secp256k1", 0) != 1 ||
        OSSL_PARAM_BLD_push_octet_string(param_bld,
                                         OSSL_PKEY_PARAM_PUB_KEY,
                                         pubkey.data(),
                                         pubkey.size()) != 1)
    {
        OSSL_PARAM_BLD_free(param_bld);
        handleErrors();
    }

    params = OSSL_PARAM_BLD_to_param(param_bld);
    if (params == NULL)
    {
        OSSL_PARAM_BLD_free(param_bld);
        handleErrors();
    }

    ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    if (ctx == NULL)
    {
        OSSL_PARAM_free(params);
        OSSL_PARAM_BLD_free(param_bld);
        handleErrors();
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
        EVP_PKEY_fromdata(ctx, &pKeyNew, EVP_PKEY_PUBLIC_KEY, params) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(params);
        OSSL_PARAM_BLD_free(param_bld);
        handleErrors();
    }

    EVP_PKEY_CTX_free(ctx);
    OSSL_PARAM_free(params);
    OSSL_PARAM_BLD_free(param_bld);

    // Swap keys
    if (pKey)
    {
        EVP_PKEY_free(pKey);
    }
    pKey = pKeyNew;

    bSet = true;
    return pKey;
}

secp256k1_point::secp256k1_point(const secp256k1_point& source)
{
    init();
    if (!EC_GROUP_copy(group, source.group))
    {
        throw runtime_error("secp256k1_point::secp256k1_point() - "
                               "EC_GROUP_copy failed.");
    }
    if (!EC_POINT_copy(point, source.point))
    {
        throw runtime_error("secp256k1_point::secp256k1_point() - "
                               "EC_POINT_copy failed.");
    }
}

secp256k1_point::secp256k1_point(const bytes_t& bytes)
{
    init();
    this->bytes(bytes);
}


secp256k1_point::~secp256k1_point()
{
    if (point)
    {
        EC_POINT_free(point);
    }
    if (group)
    {
        EC_GROUP_free(group);
    }
    if (ctx)
    {
        BN_CTX_free(ctx);
    }
}

secp256k1_point& secp256k1_point::operator=(const secp256k1_point& rhs)
{
    if (!EC_GROUP_copy(group, rhs.group))
    {
        throw runtime_error("secp256k1_point::operator= - "
                               "EC_GROUP_copy failed.");
    }
    if (!EC_POINT_copy(point, rhs.point))
    {
        throw runtime_error("secp256k1_point::operator= - "
                               "EC_POINT_copy failed.");
    }

    return *this;
}

typedef vector<unsigned char> bytes_t;
void secp256k1_point::bytes(const bytes_t& bytes)
{
    string err;
    BIGNUM* bn = NULL;
    BIGNUM* x = NULL;
    BIGNUM* y = NULL;

    if (bytes.empty())
    {
        err = "Input bytes are empty.";
        goto finish;
    }

    // Check the format based on the first byte
    if (bytes[0] == 0x04)
    {
        // Uncompressed format: 0x04 <x> <y>
        if (bytes.size() != 65)
        {
            err = "Invalid length for uncompressed point.";
            goto finish;
        }

        x = BN_bin2bn(&bytes[1], 32, NULL);
        y = BN_bin2bn(&bytes[33], 32, NULL);
        if (!x || !y)
        {
            err = "BN_bin2bn failed for x or y.";
            goto finish;
        }

        if (!EC_POINT_set_affine_coordinates(group, point, x, y, ctx))
        {
            err = "EC_POINT_set_affine_coordinates failed.";
            goto finish;
        }
    }
    else if (bytes[0] == 0x02 || bytes[0] == 0x03)
    {
        // Compressed format: <0x02 or 0x03> <x>
        if (bytes.size() != 33)
        {
            err = "Invalid length for compressed point.";
            goto finish;
        }

        bn = BN_bin2bn(&bytes[1], 32, NULL);
        if (!bn)
        {
            err = "BN_bin2bn failed.";
            goto finish;
        }

        if (!EC_POINT_set_compressed_coordinates(group, point,
                                                 bn, bytes[0] & 1, ctx))
        {
            err = "EC_POINT_set_compressed_coordinates failed.";
            goto finish;
        }
    }
    else
    {
        err = "Invalid format: first byte should be 0x02, 0x03, or 0x04.";
        goto finish;
    }

    if (!EC_POINT_is_on_curve(group, point, ctx))
    {
        err = "Resulting point is not on the curve.";
        goto finish;
    }

finish:
    if (bn) BN_clear_free(bn);
    if (x) BN_clear_free(x);
    if (y) BN_clear_free(y);

    if (!err.empty())
    {
        throw runtime_error(string("secp256k1_point::bytes() - ") + err);
    }
}

bytes_t secp256k1_point::bytes() const
{
    bytes_t bytes(33);

    string err;

    size_t len = EC_POINT_point2oct(group, point,
                                    POINT_CONVERSION_COMPRESSED,
                                    nullptr, 0, ctx);
    if (len != 33)
    {
        err = "EC_POINT_point2oct returned unexpected length.";
        goto finish;
    }

    len = EC_POINT_point2oct(group, point,
                             POINT_CONVERSION_COMPRESSED,
                             bytes.data(), bytes.size(), ctx);
    if (len != 33)
    {
        err = "EC_POINT_point2oct failed.";
        goto finish;
    }

finish:
    if (!err.empty())
    {
        throw runtime_error(string("secp256k1_point::bytes() - ") + err);
    }

    return bytes;
}

secp256k1_point& secp256k1_point::operator+=(const secp256k1_point& rhs)
{
    if (!EC_POINT_add(group, point, point, rhs.point, ctx))
    {
        throw runtime_error("secp256k1_point::operator+= - "
                               "EC_POINT_add failed.");
    }
    return *this;
}

secp256k1_point& secp256k1_point::operator*=(const bytes_t& rhs)
{
    BIGNUM* bn = BN_bin2bn(&rhs[0], rhs.size(), NULL);
    if (!bn)
    {
        throw runtime_error("secp256k1_point::operator*= - "
                               "BN_bin2bn failed.");
    }

    int rval = EC_POINT_mul(group, point, NULL, point, bn, ctx);
    BN_clear_free(bn);

    if (rval == 0)
    {
        throw runtime_error("secp256k1_point::operator*= - "
                               "EC_POINT_mul failed.");
    }

    return *this;
}

// Computes n*G + K where K is this and G is the group generator
void secp256k1_point::generator_mul(const bytes_t& n)
{
    BIGNUM* bn = BN_bin2bn(&n[0], n.size(), NULL);
    if (!bn)
    {
        throw runtime_error("secp256k1_point::generator_mul() - "
                               "BN_bin2bn failed.");
    }

    // int rval = EC_POINT_mul(group, point, bn,
    //                         (is_at_infinity() ? NULL : point),
    //                         BN_value_one(), ctx);
    int rval = EC_POINT_mul(group, point, bn, point, BN_value_one(), ctx);
    BN_clear_free(bn);

    if (rval == 0)
    {
        throw runtime_error("secp256k1_point::generator_mul() - "
                               "EC_POINT_mul failed.");
    }
}

// Sets to n*G
void secp256k1_point::set_generator_mul(const bytes_t& n)
{
    BIGNUM* bn = BN_bin2bn(&n[0], n.size(), NULL);
    if (!bn)
    {
        throw runtime_error("secp256k1_point::set_generator_mul() - "
                               "BN_bin2bn failed.");
    }

    int rval = EC_POINT_mul(group, point, bn, NULL, NULL, ctx);
    BN_clear_free(bn);

    if (rval == 0)
    {
        throw runtime_error("secp256k1_point::set_generator_mul() - "
                               "EC_POINT_mul failed.");
    }
}

void secp256k1_point::init()
{
    string err;

    group = NULL;
    point = NULL;
    ctx   = NULL;

    group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    if (!group)
    {
        err = "EC_GROUP_new_by_curve_name failed.";
        goto finish;
    }

    point = EC_POINT_new(group);
    if (!point)
    {
        err = "EC_POINT_new failed.";
        goto finish;
    }

    ctx = BN_CTX_new();
    if (!ctx)
    {
        err = "BN_CTX_new failed.";
        goto finish;
    }

    return;

finish:
    if (group) EC_GROUP_free(group);
    if (point) EC_POINT_free(point);

    throw runtime_error(string("secp256k1_point::init() - ") + err);
}


bytes_t CoinCrypto::secp256k1_sigToLowS(const bytes_t& signature)
{
    const unsigned char* pvch = (const unsigned char*)&signature[0];
    ECDSA_SIG* sig = d2i_ECDSA_SIG(NULL, &pvch, signature.size());
    if (!sig)
    {
        throw runtime_error("secp256k1_sigToLowS(): d2i_ECDSA_SIG failed.");
    }

    BIGNUM* order = BN_bin2bn(&SECP256K1_GROUP_ORDER[0],
                              SECP256K1_GROUP_ORDER.size(),
                              NULL);
    if (!order)
    {
        ECDSA_SIG_free(sig);
        throw runtime_error("secp256k1_sigToLowS(): BN_bin2bn failed.");
    }

    BIGNUM* halforder = BN_bin2bn(&SECP256K1_GROUP_HALFORDER[0],
                                  SECP256K1_GROUP_HALFORDER.size(),
                                  NULL);
    if (!halforder)
    {
        ECDSA_SIG_free(sig);
        BN_clear_free(order);
        throw runtime_error("secp256k1_sigToLowS(): BN_bin2bn failed.");
    }

#if OPENSSL_VERSION_NUMBER > 0x1000ffffL
    const BIGNUM *sig_r, *sig_s;
    ECDSA_SIG_get0(sig, &sig_r, &sig_s);
    if (BN_cmp(sig_s, halforder) > 0)
    {
        BIGNUM *sig_r_new, *sig_s_new;
        sig_r_new = BN_new();
        sig_s_new = BN_new();
        // enforce low S values, by negating the value (modulo the order)
        //    if above order/2.
        BN_sub(sig_s_new, order, sig_s);
        BN_copy(sig_r_new, sig_r);
        // no need to free sig_*_new according to OpenSSL docs
        //    https://www.openssl.org/docs/man1.1.0/crypto/ECDSA_SIG_set0.html
        ECDSA_SIG_set0(sig, sig_r_new, sig_s_new);
    }
#else
    if (BN_cmp(sig->s, halforder) > 0)
    {
        // enforce low S values, by negating the value (modulo the order)
        //    if above order/2.
        BN_sub(sig->s, order, sig->s);
    }
#endif

    BN_clear_free(order);
    BN_clear_free(halforder);

    unsigned char buffer[1024];
    unsigned char* pos = &buffer[0];
    int nSize = i2d_ECDSA_SIG(sig, &pos);
    ECDSA_SIG_free(sig);

    return bytes_t(buffer, buffer + nSize);
}

/******************************************************************************
 * Create an secp256k1 Signature
 *
 * Parameters:
 *    `key` : secp256k1 containing a private key to sign with
 *    `sha256_hash` : 32 byte SHA256 digest of the original message

 * Returns:
 *    signature
 ******************************************************************************/
bytes_t CoinCrypto::secp256k1_sign(const secp256k1_key& key,
                                   const bytes_t& sha256_hash)
{
    if (sha256_hash.size() != 32)
    {
        throw invalid_argument("CoinCrypto::secp256k1_sign(): "
                                    "Input must be a 32-byte SHA256 digest");
    }

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key.getKey(), NULL);
    if (!ctx)
    {
        handleErrors();
    }

    if (EVP_PKEY_sign_init(ctx) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        handleErrors();
    }

    if (EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        handleErrors();
    }

    size_t siglen;
    if (EVP_PKEY_sign(ctx, NULL, &siglen,
                      sha256_hash.data(), sha256_hash.size()) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        handleErrors();
    }

    bytes_t signature(siglen);
    if (EVP_PKEY_sign(ctx, signature.data(), &siglen,
                      sha256_hash.data(), sha256_hash.size()) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        handleErrors();
    }

    EVP_PKEY_CTX_free(ctx);
    return secp256k1_sigToLowS(signature);
}

/******************************************************************************
 * Verify an secp256k1 Signature
 *
 * Parameters:
 *    `key` : secp256k1 key containing a public key to verify with
 *    `sha256_hash` : 32 byte SHA256 digest of the original message
 *    `signature` :The signature to verify
 *    `flags` Additional flags (e.g., SIGNATURE_ENFORCE_LOW_S)
 *
 * Returns:
 *    signature
 ******************************************************************************/
bool CoinCrypto::secp256k1_verify(const secp256k1_key& key,
                                  const bytes_t& sha256_hash,
                                  const bytes_t& signature,
                                  int flags)
{
    if (sha256_hash.size() != 32)
    {
        throw invalid_argument("CoinCrypto::secp256k1_verify(): "
                                    "Input must be a 32-byte SHA256 digest");
    }

    if (flags & SIGNATURE_ENFORCE_LOW_S)
    {
        if (signature != secp256k1_sigToLowS(signature))
        {
            return false;
        }
    }

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key.getKey(), NULL);
    if (!ctx)
    {
        handleErrors();
    }

    if (EVP_PKEY_verify_init(ctx) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        handleErrors();
    }

    if (EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        handleErrors();
    }

    int result = EVP_PKEY_verify(ctx, signature.data(), signature.size(),
                                 sha256_hash.data(), sha256_hash.size());

    EVP_PKEY_CTX_free(ctx);

    if (result < 0)
    {
        handleErrors();
    }

    return (result == 1);
}
