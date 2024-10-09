
#include "key.h"

#include "test-utils.hpp"

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>

#ifndef LEGACY_PKEY
#include <openssl/core_names.h>
#endif


using namespace std;


#ifdef LEGACY_PKEY
int EC_KEY_regenerate_key(EC_KEY *eckey, BIGNUM *priv_key);
#else
int EVP_PKEY_regenerate_EC_key(EVP_PKEY **ppkey, BIGNUM *priv_key);
#endif


class KeyTest : public ::testing::Test
{
protected:
    void SetUp() override {}
};


int main(int argc, char **argv)
{

    set_debug(argc, argv);

#ifdef LEGACY_PKEY
    print_note("Testing with LEGACY_PKEY.");
#endif

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}


#ifdef LEGACY_PKEY
#else  // LEGACY_PKEY
TEST_F(KeyTest, Reset)
{
    CKey SUBJECT;

    EXPECT_NO_THROW(SUBJECT.Reset());

    EXPECT_NE(SUBJECT.GetEVPKey(), nullptr);

    EVP_PKEY* pkeyOld = EVP_PKEY_dup(SUBJECT.GetEVPKey());
    SUBJECT.Reset();
    EVP_PKEY* pkeyNew = SUBJECT.GetEVPKey();
    ASSERT_FALSE(EVP_PKEY_eq(pkeyOld, pkeyNew));
    EVP_PKEY_free(pkeyOld);
    pkeyOld = NULL;

    EXPECT_EQ(EVP_PKEY_id(pkeyNew), EVP_PKEY_EC);

    for (int i = 0; i < 128; ++i)
    {
        EXPECT_NO_THROW(SUBJECT.Reset());
        EXPECT_NE(SUBJECT.GetEVPKey(), nullptr);
    }
}
#endif  // LEGACY_PKEY

#ifdef LEGACY_PKEY
TEST_F(KeyTest, ECKeyRegenerateKey)
{
    EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    ASSERT_NE(eckey, nullptr);

    vector<unsigned char> priv_key(32);
    RAND_bytes(priv_key.data(), 32);

    PrintTestingData("EC_KEY_regenerate_key", "Private Key", priv_key);

    BIGNUM* bn = BN_bin2bn(priv_key.data(), 32, nullptr);

    ASSERT_NE(bn, nullptr);

    ASSERT_EQ(EC_KEY_regenerate_key(eckey, bn), 1);

    const EC_POINT* pub_key = EC_KEY_get0_public_key(eckey);
    ASSERT_NE(pub_key, nullptr);

    // Compressed public key
    vector<unsigned char> pub_key_bytes(33);
    ASSERT_NE(EC_POINT_point2oct(EC_KEY_get0_group(eckey), pub_key,
                                 POINT_CONVERSION_COMPRESSED,
                                 pub_key_bytes.data(), 33, nullptr), 0);

    PrintTestingData("EC_KEY_regenerate_key", "Public Key", pub_key_bytes);

    BN_free(bn);
    EC_KEY_free(eckey);
}
#else  // LEGACY_PKEY
TEST_F(KeyTest, EVPKeyRegenerateECKey)
{
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    ASSERT_NE(ctx, nullptr);

    ASSERT_GT(EVP_PKEY_keygen_init(ctx), 0);
    ASSERT_GT(EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_secp256k1), 0);
    ASSERT_GT(EVP_PKEY_keygen(ctx, &pkey), 0);

    EVP_PKEY_CTX_free(ctx);

    vector<unsigned char> priv_key = {
        0x32, 0xe2, 0xee, 0x62, 0x75, 0xa6, 0x7d, 0xba, 0x82, 0x5e, 0x36,
        0x6b, 0x4d, 0x80, 0x4d, 0x57, 0xed, 0x8d, 0x86, 0x99, 0x03, 0xbe,
        0x73, 0x6d, 0x79, 0x04, 0xe4, 0xd1, 0xcb, 0xc2, 0x91, 0xfd };

    PrintTestingData("EVP_PKEY_regenerate_EC_key", "Private Key", priv_key);

    BIGNUM* bn = BN_bin2bn(priv_key.data(), 32, nullptr);
    ASSERT_NE(bn, nullptr);

    size_t nBinSize = BN_num_bytes(bn);

    unsigned char pchBin[nBinSize];

    size_t nBinSteps = sizeof(pchBin) / sizeof(pchBin[0]);

    BN_bn2bin(bn, pchBin);

    vector<unsigned char> vBin(pchBin, pchBin + nBinSteps);


    PrintTestingData("EC_KEY_regenerate_key",
                     "Private Key Recovered from BIGNUM",
                     vBin);

    ASSERT_EQ(EVP_PKEY_regenerate_EC_key(&pkey, bn), 1);

    BIGNUM *bnRecovered = NULL;
    bool fResult = EVP_PKEY_get_bn_param(pkey,
                                         OSSL_PKEY_PARAM_PRIV_KEY,
                                         &bnRecovered);

    ASSERT_TRUE(fResult);

    valtype vchPrivKeyRecovered(32, 0);
    BN_bn2binpad(bnRecovered, vchPrivKeyRecovered.data(), 32);
    BN_free(bnRecovered);

    PrintTestingData("EC_KEY_regenerate_key",
                     "Private Key Recovered from EVP_PKEY",
                     vchPrivKeyRecovered);


    unsigned char* pub_key_bytes = nullptr;
    size_t pub_key_len = 0;
    ASSERT_GT(EVP_PKEY_get_octet_string_param(pkey,
                                              OSSL_PKEY_PARAM_PUB_KEY,
                                              nullptr, 0, &pub_key_len), 0);

    pub_key_bytes = static_cast<unsigned char*>(OPENSSL_malloc(pub_key_len));
    ASSERT_NE(pub_key_bytes, nullptr);
    ASSERT_GT(EVP_PKEY_get_octet_string_param(pkey,
                                              OSSL_PKEY_PARAM_PUB_KEY,
                                              pub_key_bytes,
                                              pub_key_len,
                                              &pub_key_len), 0);

    vector<unsigned char> pub_key_vec(pub_key_bytes,
                                      pub_key_bytes + pub_key_len);
    PrintTestingData("EVP_PKEY_regenerate_EC_key", "Public Key", pub_key_vec);

    OPENSSL_free(pub_key_bytes);
    BN_free(bn);
    EVP_PKEY_free(pkey);
}
#endif  // LEGACY_PKEY

TEST_F(KeyTest, SetPrivKey)
{
    CKey SUBJECT;

    vector<unsigned char> vchPrivKey = {
        0x30, 0x54, 0x02, 0x01, 0x01, 0x04, 0x20, 0xa3, 0x15, 0x92, 0x2e, 0x23,
        0xad, 0x5d, 0x48, 0x76, 0x30, 0xc3, 0x2a, 0x09, 0x0c, 0xff, 0xcb, 0x3e,
        0x25, 0xf9, 0xc8, 0x46, 0x18, 0xfb, 0xb3, 0xcc, 0xc3, 0xde, 0xfc, 0x78,
        0x0d, 0x21, 0x6b, 0xa0, 0x07, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x0a,
        0xa1, 0x24, 0x03, 0x22, 0x00, 0x03, 0x24, 0x40, 0xf9, 0x2f, 0x5c, 0xd1,
        0x5d, 0x1b, 0xea, 0xa6, 0x03, 0xbd, 0x9e, 0xeb, 0x2e, 0x87, 0x39, 0xd2,
        0xa3, 0x09, 0x4e, 0x45, 0x09, 0x0c, 0xb4, 0x0c, 0x4a, 0x79, 0x8f, 0xdf,
        0x07, 0xb8,
        // uncompressed public key
        0x04, 0x24, 0x40, 0xf9, 0x2f, 0x5c, 0xd1, 0x5d, 0x1b, 0xea, 0xa6, 0x03,
        0xbd, 0x9e, 0xeb, 0x2e, 0x87, 0x39, 0xd2, 0xa3, 0x09, 0x4e, 0x45, 0x09,
        0x0c, 0xb4, 0x0c, 0x4a, 0x79, 0x8f, 0xdf, 0x07, 0xb8, 0x26, 0xb4, 0xce,
        0xbe, 0xcf, 0x75, 0xb6, 0xb2, 0x2e, 0x29, 0xc6, 0x38, 0x60, 0xbe, 0x64,
        0x27, 0x40, 0x31, 0x50, 0x4c, 0x5d, 0xb1, 0xbb, 0x5b, 0x84, 0x04, 0x5c,
        0xed, 0x1f, 0xe5, 0x9c, 0x99 };

    PrintTestingData("SetPrivKey", "Random Private Key Data", vchPrivKey);

    ASSERT_TRUE(
        SUBJECT.SetPrivKey(CPrivKey(vchPrivKey.begin(), vchPrivKey.end())));

#ifdef LEGACY_PKEY
    EC_KEY* pkey = SUBJECT.GetECKey();
    ASSERT_NE(pkey, nullptr);

    const BIGNUM* priv_bn = EC_KEY_get0_private_key(pkey);
    ASSERT_NE(priv_bn, nullptr);
    vector<unsigned char> priv_key(32);
    ASSERT_EQ(BN_bn2bin(priv_bn, priv_key.data()), 32);

    PrintTestingData("SetPrivKey", "Recovered Private Key", priv_key);

    const EC_POINT* pub_key = EC_KEY_get0_public_key(pkey);
    ASSERT_NE(pub_key, nullptr);

    // Compressed public key
    vector<unsigned char> compressed_pub_key(33);
    ASSERT_NE(EC_POINT_point2oct(EC_KEY_get0_group(pkey), pub_key,
                                 POINT_CONVERSION_COMPRESSED,
                                 compressed_pub_key.data(), 33, nullptr), 0);

    PrintTestingData("SetPrivKey", "Compressed Public Key", compressed_pub_key);
#else  // LEGACY_PKEY
    EVP_PKEY* pkey = SUBJECT.GetEVPKey();
    ASSERT_NE(pkey, nullptr);

    BIGNUM* priv_bn = nullptr;
    ASSERT_GT(EVP_PKEY_get_bn_param(pkey,
                                    OSSL_PKEY_PARAM_PRIV_KEY,
                                    &priv_bn), 0);
    ASSERT_NE(priv_bn, nullptr);

    vector<unsigned char> priv_key(32);
    ASSERT_EQ(BN_bn2binpad(priv_bn, priv_key.data(), 32), 32);

    PrintTestingData("SetPrivKey", "Recovered Private Key", priv_key);

    unsigned char* pub_key_bytes = nullptr;
    size_t pub_key_len = 0;
    ASSERT_GT(EVP_PKEY_get_octet_string_param(pkey,
                                              OSSL_PKEY_PARAM_PUB_KEY,
                                              nullptr, 0, &pub_key_len), 0);

    pub_key_bytes = static_cast<unsigned char*>(OPENSSL_malloc(pub_key_len));
    ASSERT_NE(pub_key_bytes, nullptr);
    ASSERT_GT(EVP_PKEY_get_octet_string_param(pkey,
                                              OSSL_PKEY_PARAM_PUB_KEY,
                                              pub_key_bytes,
                                              pub_key_len,
                                              &pub_key_len), 0);

    vector<unsigned char> compressed_pub_key(pub_key_bytes,
                                             pub_key_bytes + pub_key_len);
    PrintTestingData("SetPrivKey",
                     "Compressed Public Key",
                     compressed_pub_key);

    OPENSSL_free(pub_key_bytes);
    BN_free(priv_bn);
#endif


    bool fCompr;
    CSecret secret = SUBJECT.GetSecret(fCompr);
    CKey testKey;
    testKey.SetSecret(secret, fCompr);

    valtype pubKey = SUBJECT.GetPubKey().Raw();
    valtype testPubKey = testKey.GetPubKey().Raw();

    PrintTestingData("SetPrivKey",
                     fCompr ? "Compressed Public Key" :
                              "Uncompressed Public Key",
                     pubKey);
    PrintTestingData("SetPrivKey",
                     fCompr ? "Compressed Public Key" :
                              "Uncompressed Public Key",
                     testPubKey);

    int nResult = 0;
    bool fValid = SUBJECT.IsValid(&nResult);
    ASSERT_TRUE(fValid);
}


TEST_F(KeyTest, SetSecret)
{
    CKey SUBJECT;
    vector<unsigned char> vchSecret(32);
    RAND_bytes(vchSecret.data(), 32);

    PrintTestingData("SetSecret", "Random Secret", vchSecret);

    ASSERT_TRUE(
        SUBJECT.SetSecret(CSecret(vchSecret.begin(), vchSecret.end()), true));

#ifdef LEGACY_PKEY
    EC_KEY* pkey = SUBJECT.GetECKey();
    ASSERT_NE(pkey, nullptr);

    const BIGNUM* priv_bn = EC_KEY_get0_private_key(pkey);
    ASSERT_NE(priv_bn, nullptr);
    vector<unsigned char> priv_key(32);
    ASSERT_EQ(BN_bn2bin(priv_bn, priv_key.data()), 32);

    PrintTestingData("SetSecret", "Recovered Private Key", priv_key);

    const EC_POINT* pub_key = EC_KEY_get0_public_key(pkey);
    ASSERT_NE(pub_key, nullptr);

    // Compressed public key
    vector<unsigned char> pub_key_bytes(33);

    ASSERT_NE(EC_POINT_point2oct(EC_KEY_get0_group(pkey),
                                 pub_key,
                                 POINT_CONVERSION_COMPRESSED,
                                 pub_key_bytes.data(), 33, nullptr), 0);

    PrintTestingData("SetSecret", "Recovered Public Key", pub_key_bytes);
#else  // LEGACY_PKEY
    EVP_PKEY* pkey = SUBJECT.GetEVPKey();
    ASSERT_NE(pkey, nullptr);

    BIGNUM* priv_bn = nullptr;
    ASSERT_GT(EVP_PKEY_get_bn_param(pkey,
                                    OSSL_PKEY_PARAM_PRIV_KEY,
                                    &priv_bn), 0);
    ASSERT_NE(priv_bn, nullptr);

    vector<unsigned char> priv_key(32);
    ASSERT_EQ(BN_bn2binpad(priv_bn, priv_key.data(), 32), 32);

    PrintTestingData("SetSecret", "Recovered Private Key", priv_key);

    unsigned char* pub_key_bytes = nullptr;
    size_t pub_key_len = 0;
    ASSERT_GT(EVP_PKEY_get_octet_string_param(pkey,
                                              OSSL_PKEY_PARAM_PUB_KEY,
                                              nullptr, 0, &pub_key_len), 0);

    pub_key_bytes = static_cast<unsigned char*>(OPENSSL_malloc(pub_key_len));
    ASSERT_NE(pub_key_bytes, nullptr);
    ASSERT_GT(EVP_PKEY_get_octet_string_param(pkey,
                                              OSSL_PKEY_PARAM_PUB_KEY,
                                              pub_key_bytes,
                                              pub_key_len,
                                              &pub_key_len), 0);

    vector<unsigned char> recovered_pub_key(pub_key_bytes,
                                            pub_key_bytes + pub_key_len);
    PrintTestingData("SetSecret", "Recovered Public Key", recovered_pub_key);

    OPENSSL_free(pub_key_bytes);
    BN_free(priv_bn);
#endif  // LEGACY_PKEY
}


TEST_F(KeyTest, GetSecret)
{
    CKey SUBJECT;
    vector<unsigned char> vchSecret(32);
    RAND_bytes(vchSecret.data(), 32);

    PrintTestingData("GetSecret", "Random Secret", vchSecret);

    ASSERT_TRUE(
        SUBJECT.SetSecret(CSecret(vchSecret.begin(), vchSecret.end()), true));

    bool fCompressed;
    CSecret secret = SUBJECT.GetSecret(fCompressed);

    PrintTestingData("GetSecret", "Recovered Secret", secret);

    ASSERT_EQ(secret, CSecret(vchSecret.begin(), vchSecret.end()));
    ASSERT_TRUE(fCompressed);
}


TEST_F(KeyTest, GetPrivKey)
{
    CKey SUBJECT;
    CPrivKey vchSecret(32);
    RAND_bytes(vchSecret.data(), 32);

    PrintTestingData("GetPrivKey", "Random Secret", vchSecret);

    ASSERT_TRUE(
        SUBJECT.SetSecret(CSecret(vchSecret.begin(), vchSecret.end()), true));

    CPrivKey privKey = SUBJECT.GetPrivKey();

    PrintTestingData("GetPrivKey", "Recovered Private Key", privKey);

    // Verify that the private key can be used to create a valid CKey
    CKey testKey;
    ASSERT_TRUE(testKey.SetPrivKey(privKey));

    int nResult = 0;
    bool fValid = testKey.IsValid(&nResult);
    ASSERT_TRUE(fValid);
}

TEST_F(KeyTest, SetPubKey)
{
    CKey randomKey;
    randomKey.MakeNewKey(true);
    CPubKey vchPubKey = randomKey.GetPubKey();
    valtype vchPubKeyRaw = vchPubKey.Raw();

    PrintTestingData("SetPubKey", "Random Public Key", vchPubKeyRaw);

    CKey SUBJECT;
    ASSERT_TRUE(SUBJECT.SetPubKey(vchPubKey));

    CPubKey recoveredPubKey = SUBJECT.GetPubKey();
    valtype vchRecoveredRaw = recoveredPubKey.Raw();

    PrintTestingData("SetPubKey", "Recovered Public Key", vchRecoveredRaw);

    ASSERT_EQ(vchPubKey, recoveredPubKey);
}

TEST_F(KeyTest, GetPubKey)
{
    CKey SUBJECT;
    SUBJECT.MakeNewKey(true);

    CPubKey pubKey = SUBJECT.GetPubKey();
    valtype vchPubKey = pubKey.Raw();

    PrintTestingData("GetPubKey", "Public Key", vchPubKey);

    ASSERT_TRUE(pubKey.IsValid());
    ASSERT_TRUE(pubKey.IsCompressed());
}

TEST_F(KeyTest, Sign)
{
    CKey SUBJECT;
    SUBJECT.MakeNewKey(true);
    secure_valtype vchPrivKey = SUBJECT.GetPrivKey();
    valtype vchPubKey = SUBJECT.GetPubKey().Raw();

    PrintTestingData("Sign", "Private Key", vchPrivKey);
    PrintTestingData("Sign", "Public Key", vchPubKey);

    vector<unsigned char> vchData(32);
    RAND_bytes(vchData.data(), 32);

    PrintTestingData("Sign", "Random Data to Hash", vchData);

    uint256 hash = Hash(vchData.begin(), vchData.end());

    PrintTestingData("Sign", "Hash to Sign", hash);

    vector<unsigned char> vchSig;
    ASSERT_TRUE(SUBJECT.Sign(hash, vchSig));

    PrintTestingData("Sign", "Signature", vchSig);

    ASSERT_TRUE(SUBJECT.Verify(hash, vchSig));
}

TEST_F(KeyTest, SignCompact)
{
    CKey SUBJECT;
    SUBJECT.MakeNewKey(true);
    secure_valtype vchPrivKey = SUBJECT.GetPrivKey();
    valtype vchPubKey = SUBJECT.GetPubKey().Raw();

    PrintTestingData("SignCompact", "Private Key", vchPrivKey);
    PrintTestingData("SignCompact", "Public Key", vchPubKey);

    vector<unsigned char> vchData(32);
    RAND_bytes(vchData.data(), 32);

    PrintTestingData("SignCompact", "Random Data to Hash", vchData);

    uint256 hash = Hash(vchData.begin(), vchData.end());

    PrintTestingData("SignCompact", "Hash to Sign", hash);

    vector<unsigned char> vchSig;
    ASSERT_TRUE(SUBJECT.SignCompact(hash, vchSig));

    PrintTestingData("SignCompact", "Signature", vchSig);

    ASSERT_TRUE(SUBJECT.VerifyCompact(hash, vchSig));
}

TEST_F(KeyTest, SetCompactSignature)
{
    CKey ckeyTest;
    ckeyTest.MakeNewKey(true);
    secure_valtype vchPrivKey = ckeyTest.GetPrivKey();
    valtype vchPubKey = ckeyTest.GetPubKey().Raw();

    PrintTestingData("SetCompactSignature", "Private Key", vchPrivKey);
    PrintTestingData("SetCompactSignature", "Public Key", vchPubKey);

    vector<unsigned char> vchData(32);
    RAND_bytes(vchData.data(), 32);

    PrintTestingData("SetCompactSignature", "Random Data to Hash", vchData);

    uint256 hash = Hash(vchData.begin(), vchData.end());

    PrintTestingData("SetCompactSignature", "Hash to Sign", hash);

    vector<unsigned char> vchSig;
    ASSERT_TRUE(ckeyTest.SignCompact(hash, vchSig));

    PrintTestingData("SetCompactSignature", "Signature", vchSig);

    CKey SUBJECT;
    ASSERT_TRUE(SUBJECT.SetCompactSignature(hash, vchSig));


    valtype vchRecoveredPubKey = ckeyTest.GetPubKey().Raw();

    PrintTestingData("SetCompactSignature",
                     "Recovered Public Key",
                     vchRecoveredPubKey);

    ASSERT_EQ(SUBJECT.GetPubKey(), ckeyTest.GetPubKey());
}

TEST_F(KeyTest, Verify)
{
    CKey SUBJECT;
    SUBJECT.MakeNewKey(true);
    secure_valtype vchPrivKey = SUBJECT.GetPrivKey();
    valtype vchPubKey = SUBJECT.GetPubKey().Raw();

    PrintTestingData("Verify", "Private Key", vchPrivKey);
    PrintTestingData("Verify", "Public Key", vchPubKey);

    vector<unsigned char> vchData(32);
    RAND_bytes(vchData.data(), 32);

    PrintTestingData("Verify", "Random Data to Hash", vchData);

    uint256 hash = Hash(vchData.begin(), vchData.end());

    PrintTestingData("Verify", "Hash to Sign", hash);

    vector<unsigned char> vchSig;
    ASSERT_TRUE(SUBJECT.Sign(hash, vchSig));

    PrintTestingData("Verify", "Signature", vchSig);

    ASSERT_TRUE(SUBJECT.Verify(hash, vchSig));

    uint256 incorrectHash = hash ^ uint256("0x1");
    ASSERT_FALSE(SUBJECT.Verify(incorrectHash, vchSig));

    // Test with incorrect signature
    vchSig[0] ^= 0x01;
    ASSERT_FALSE(SUBJECT.Verify(hash, vchSig));
}

TEST_F(KeyTest, VerifyCompact)
{
    CKey SUBJECT;
    SUBJECT.MakeNewKey(true);
    secure_valtype vchPrivKey = SUBJECT.GetPrivKey();
    valtype vchPubKey = SUBJECT.GetPubKey().Raw();

    PrintTestingData("VerifyCompact", "Private Key", vchPrivKey);
    PrintTestingData("VerifyCompact", "Public Key", vchPubKey);

    vector<unsigned char> vchData(32);
    RAND_bytes(vchData.data(), 32);

    PrintTestingData("VerifyCompact", "Random Data to Hash", vchData);

    uint256 hash = Hash(vchData.begin(), vchData.end());

    PrintTestingData("VerifyCompact", "Hash to Sign", hash);

    vector<unsigned char> vchSig;
    ASSERT_TRUE(SUBJECT.SignCompact(hash, vchSig));

    PrintTestingData("VerifyCompact", "Signature", vchSig);

    ASSERT_TRUE(SUBJECT.VerifyCompact(hash, vchSig));

    uint256 incorrectHash = hash ^ uint256("0x1");
    ASSERT_FALSE(SUBJECT.VerifyCompact(incorrectHash, vchSig));

    // Test with incorrect signature
    vchSig[0] ^= 0x01;
    ASSERT_FALSE(SUBJECT.VerifyCompact(hash, vchSig));
}

TEST_F(KeyTest, IsValid)
{
    CKey newKey;
    newKey.MakeNewKey(true);
    ASSERT_TRUE(newKey.IsValid());

    CKey SUBJECT;
    vector<unsigned char> goodPrivKey = {
        0x30, 0x54, 0x02, 0x01, 0x01, 0x04, 0x20, 0x92, 0x0e, 0xb1, 0x8c, 0x8b,
        0x3a, 0x00, 0xdf, 0x75, 0x4c, 0x31, 0x9e, 0xd7, 0x7d, 0x60, 0x8b, 0xed,
        0x75, 0x2c, 0x85, 0xae, 0x19, 0x91, 0x68, 0x72, 0xbd, 0xb2, 0x69, 0x9f,
        0x44, 0xa5, 0xf5, 0xa0, 0x07, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x0a,
        0xa1, 0x24, 0x03, 0x22, 0x00, 0x03, 0x8c, 0xa0, 0xc5, 0x3d, 0x10, 0xc5,
        0x5a, 0x05, 0xbd, 0xb3, 0xc0, 0xec, 0x27, 0xe7, 0x0a, 0x3a, 0xa8, 0x9e,
        0xca, 0x63, 0xf6, 0xd3, 0xd6, 0x85, 0xdf, 0x07, 0x2d, 0xa3, 0x71, 0x8c,
        0x01, 0x22 };

    ASSERT_TRUE(
        SUBJECT.SetPrivKey(CPrivKey(goodPrivKey.begin(), goodPrivKey.end())));
    ASSERT_TRUE(SUBJECT.IsValid());

    // Test with an invalid key (all zeros)
    CKey zeroKey;
    vector<unsigned char> zeroSecret(32, 0);
    ASSERT_TRUE(zeroKey.SetSecret(CPrivKey(zeroSecret.begin(), zeroSecret.end())));
    ASSERT_FALSE(zeroKey.IsValid());

    // Test with a key that has not been set
    CKey unsetKey;
    ASSERT_FALSE(unsetKey.IsValid());
}
