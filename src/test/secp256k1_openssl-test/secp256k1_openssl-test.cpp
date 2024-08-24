#include "secp256k1_openssl.h"

#include "test-utils.hpp"


using namespace CoinCrypto;
using namespace std;


int main(int argc, char **argv)
{
    set_debug(argc, argv);

# ifdef DO_RFC6979_TESTS
    print_note("Including RFC6979 Tests.");
# endif

    print_info("Testing in debug mode with extra output.");

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


class Secp256k1Test : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};


bytes_t random_point(bool compressed=true)
{
    bytes_t p;
    randomPoint(p, compressed);
    return p;
}


// secp256k1_key tests
TEST_F(Secp256k1Test, KeyGenerationAndRetrieval)
{
    secp256k1_key key;
    print_info("Generating new key.");
    ASSERT_NO_THROW(key.newKey());

    bytes_t privKey = key.getPrivKey();
    print_info("Testing new generated key size.");
    ASSERT_EQ(privKey.size(), 32);

    PrintTestingData("Key Generation and Retrival",
                     "Random Private Key",
                     privKey);

    print_info("Getting compressed public key.");
    ASSERT_NO_THROW(key.getPubKey());
    bytes_t pubKey = key.getPubKey();

    PrintTestingData("Key Generation and Retrival",
                     "Compressed Public Key",
                     pubKey);

    print_info("Testing compressed public key size.");
    // Compressed public key
    ASSERT_EQ(pubKey.size(), 33);

    print_info("Getting uncompressed public key.");
    ASSERT_NO_THROW(key.getPubKey(false));
    bytes_t uncompressedPubKey = key.getPubKey(false);

    PrintTestingData("Key Generation and Retrival",
                     "Unompressed Public Key",
                     uncompressedPubKey);

    print_info("Testing uncompressed public key size.");
    ASSERT_EQ(uncompressedPubKey.size(), 65);
}

TEST_F(Secp256k1Test, PrivateKeySetAndGet)
{
    secp256k1_key key;
    bytes_t testPrivKey = {
        0x32, 0xe2, 0xee, 0x62, 0x75, 0xa6, 0x7d, 0xba, 0x82, 0x5e, 0x36,
        0x6b, 0x4d, 0x80, 0x4d, 0x57, 0xed, 0x8d, 0x86, 0x99, 0x03, 0xbe,
        0x73, 0x6d, 0x79, 0x04, 0xe4, 0xd1, 0xcb, 0xc2, 0x91, 0xfd };

    print_info("Setting test private key from data.");
    ASSERT_NO_THROW(key.setPrivKey(testPrivKey));

    PrintTestingData("Private Key Set and Get",
                     "Test Private Key",
                     testPrivKey);

    print_info("Retrieving private key from key set from data..");
    ASSERT_NO_THROW(key.getPrivKey());
    bytes_t retrievedPrivKey = key.getPrivKey();

    PrintTestingData("Private Key Set and Get",
                     "Retrieved Private Key",
                     retrievedPrivKey);

    print_info("Testing identity of retrieved private key.");
    ASSERT_EQ(testPrivKey, retrievedPrivKey);

    bytes_t testCompressedPubKey = {
        0x03, 0x32, 0x26, 0x85, 0x9d, 0x72, 0xa6, 0x4d, 0xda, 0x5c, 0x37,
        0x10, 0x84, 0x3a, 0x91, 0xb1, 0x9e, 0x06, 0x9b, 0xa2, 0x01, 0xf7,
        0x77, 0x8a, 0x50, 0xfc, 0xfe, 0x0d, 0x1c, 0xf2, 0xdb, 0x93, 0x6f };

    print_info("Getting public key from key set from data.");
    ASSERT_NO_THROW(key.getPubKey());
    bytes_t pubKey = key.getPubKey();

    print_info("Testing identity of public key from data.");
    ASSERT_EQ(pubKey, testCompressedPubKey);

    bytes_t testUncompressedPubKey = {
        0x04, 0x32, 0x26, 0x85, 0x9d, 0x72, 0xa6, 0x4d, 0xda, 0x5c, 0x37,
        0x10, 0x84, 0x3a, 0x91, 0xb1, 0x9e, 0x06, 0x9b, 0xa2, 0x01, 0xf7,
        0x77, 0x8a, 0x50, 0xfc, 0xfe, 0x0d, 0x1c, 0xf2, 0xdb, 0x93, 0x6f,
        0xfc, 0x5c, 0x09, 0xdc, 0xbf, 0x46, 0xd6, 0x38, 0xf4, 0xdc, 0xf9,
        0x0d, 0x6e, 0xd7, 0x29, 0xbd, 0xa0, 0xe9, 0x25, 0x11, 0x7b, 0xf6,
        0x3c, 0x6b, 0x6d, 0xb7, 0xd1, 0x71, 0x34, 0x8c, 0x9d, 0xdd };

    bytes_t uncompressedPubKey = key.getPubKey(false);
    print_info("Testing identity of uncompressed public key from data.");
    ASSERT_EQ(uncompressedPubKey, testUncompressedPubKey);
}

TEST_F(Secp256k1Test, PublicKeySetAndGet)
{
    secp256k1_key key;
    bytes_t testPubKey = {
        0x03, 0x32, 0x26, 0x85, 0x9d, 0x72, 0xa6, 0x4d, 0xda, 0x5c, 0x37,
        0x10, 0x84, 0x3a, 0x91, 0xb1, 0x9e, 0x06, 0x9b, 0xa2, 0x01, 0xf7,
        0x77, 0x8a, 0x50, 0xfc, 0xfe, 0x0d, 0x1c, 0xf2, 0xdb, 0x93, 0x6f };

    PrintTestingData("Public Key Set and Get",
                     "Test Public Key",
                     testPubKey);

    print_info("Setting a public key from data.");
    ASSERT_NO_THROW(key.setPubKey(testPubKey));


    print_info("Retrieving the public key set from data.");
    ASSERT_NO_THROW(key.getPubKey());
    bytes_t retrievedPubKey = key.getPubKey();

    PrintTestingData("Public Key Set and Get",
                     "Retrieved Test Public Key",
                     retrievedPubKey);

    print_info("Testing identity of retrieved public key.");
    ASSERT_EQ(testPubKey, retrievedPubKey);
}

// secp256k1_point tests
TEST_F(Secp256k1Test, PointOperations)
{
    // bytes_t testBytes1 = random_point();
    bytes_t testBytes1 = {
        0x03, 0x78, 0x97, 0x5e, 0x91, 0x02, 0xcd, 0xf8, 0x6c, 0xa4, 0xb5,
        0x86, 0x33, 0x34, 0x1f, 0x34, 0x57, 0x57, 0x93, 0xcf, 0x6e, 0x70,
        0xe7, 0xb1, 0x2d, 0x77, 0x48, 0x4d, 0x0e, 0xa3, 0x00, 0x63, 0xb1 };

    PrintTestingData("Point Operations", "Test Bytes 1", testBytes1);

    secp256k1_point testPoint1(testBytes1);

    print_info("Getting bytes from test point 1.");
    ASSERT_NO_THROW(testPoint1.bytes());
    bytes_t testPoint1Bytes = testPoint1.bytes();

    PrintTestingData("Point Operations", "Point 1 Bytes", testPoint1Bytes);

    print_info("Testing identity of test point 1 bytes");
    ASSERT_EQ(testBytes1, testPoint1Bytes);

    // bytes_t testBytes2 = random_point();
    bytes_t testBytes2 = {
        0x02, 0xe3, 0x5c, 0xe6, 0xe6, 0xd7, 0x27, 0x20, 0xe8, 0x99, 0xb7,
        0x23, 0xae, 0xb8, 0xef, 0xe8, 0x4b, 0x5d, 0x8d, 0x16, 0xb8, 0x70,
        0x5d, 0xba, 0x24, 0xfe, 0x67, 0xa4, 0xda, 0xda, 0x23, 0xca, 0xe3 };

    PrintTestingData("Point Operations", "Test Bytes 2", testBytes2);

    secp256k1_point testPoint2(testBytes2);

    print_info("Getting bytes from test point 2.");
    ASSERT_NO_THROW(testPoint2.bytes());
    bytes_t testPoint2Bytes = testPoint2.bytes();

    PrintTestingData("Point Operations", "Point 2 Bytes", testPoint2Bytes);

    print_info("Testing addition of test point 2 to test point 1.");
    ASSERT_NO_THROW(testPoint1 + testPoint2);
    secp256k1_point sumTestPoint = testPoint1 + testPoint2;

    print_info("Getting bytes from summation test point.");
    ASSERT_NO_THROW(sumTestPoint.bytes());
    bytes_t sumTestPointBytes = sumTestPoint.bytes();

    PrintTestingData("Point Operations",
                     "Sum Test Point Bytes",
                     sumTestPointBytes);

    bytes_t sumPointBytes = {
        0x02, 0xf1, 0xb8, 0xe8, 0x58, 0x67, 0x9c, 0x5e, 0x25, 0xe3, 0xb8,
        0x73, 0x16, 0x03, 0xfc, 0xfd, 0x03, 0x6c, 0x74, 0x70, 0x95, 0x5a,
        0xa9, 0xdd, 0xbc, 0x52, 0x9a, 0x4f, 0x19, 0x26, 0x36, 0x83, 0x57 };

    PrintTestingData("Point Operations", "Sum Point Bytes", sumPointBytes);

    print_info("Testing identity of summation test point.");
    ASSERT_EQ(sumTestPointBytes, sumPointBytes);

    bytes_t scalar = {
        0x91, 0x33, 0xab, 0x22, 0x67, 0xe8, 0x5a, 0x96, 0x61, 0x8a, 0x32,
        0xa0, 0x3d, 0x6c, 0x47, 0x17, 0x24, 0x69, 0xc0, 0x2b, 0xcc, 0x2f,
        0xed, 0xa5, 0x27, 0xfe, 0x61, 0xaa, 0xd9, 0x2b, 0x19, 0xf6 };

    print_info("Testing multiplication of test point with scalar.");
    ASSERT_NO_THROW(testPoint1 * scalar);
    secp256k1_point mulTestPoint = testPoint1 * scalar;

    print_info("Getting bytes from product test point.");
    ASSERT_NO_THROW(mulTestPoint.bytes());
    bytes_t mulTestPointBytes = mulTestPoint.bytes();

    PrintTestingData("Point Operations",
                     "Multiply Test Point Bytes",
                     mulTestPointBytes);

    bytes_t mulPointBytes = {
        0x02, 0x96, 0x1a, 0x91, 0xe3, 0x54, 0x70, 0x7e, 0x83, 0x84, 0x09,
        0xb7, 0xba, 0x58, 0x97, 0xf9, 0x4c, 0xd1, 0x2e, 0x43, 0x40, 0xe5,
        0xc9, 0xe7, 0x28, 0x80, 0x3f, 0xad, 0xe0, 0x00, 0x09, 0x84, 0x64 };

    PrintTestingData("Point Operations",
                     "Multiply Point Bytes",
                     mulPointBytes);

    print_info("Testing identity of product test point.");
    ASSERT_EQ(mulTestPointBytes, mulPointBytes);
}


TEST_F(Secp256k1Test, GeneratorMultiplication)
{
    // bytes_t testGenMulPointBytes = random_point();
    bytes_t testGenMulPointBytes = {
        0x03, 0x2f, 0x94, 0x9f, 0x17, 0x35, 0x3a, 0x92, 0x5f, 0x55, 0x49,
        0x59, 0xcc, 0xdf, 0xb2, 0x71, 0x60, 0xa3, 0xf2, 0x70, 0x9e, 0x5c,
        0xa2, 0xef, 0xd3, 0xe1, 0x00, 0x87, 0xb0, 0xcf, 0x2b, 0x7c, 0x56 };

    secp256k1_point testGenMulPoint(testGenMulPointBytes);

    PrintTestingData("Generator Multiplication",
                     "Test Generator Mul Point",
                     testGenMulPointBytes);

    bytes_t scalar = {
        0x0d, 0x74, 0x5e, 0xbe, 0x8f, 0xf2, 0x2e, 0x53, 0x58, 0x87, 0x5c,
        0x77, 0x20, 0xc1, 0x4d, 0xe5, 0xfc, 0x8b, 0xb1, 0x5d, 0xd3, 0x94,
        0x74, 0xfe, 0xcc, 0x8c, 0x9a, 0xd0, 0x9d, 0x5d, 0x86, 0xf4 };

    print_info("Test set generator multiplication.");
    ASSERT_NO_THROW(testGenMulPoint.set_generator_mul(scalar));

    bytes_t testGenMulResultBytes = testGenMulPoint.bytes();

    PrintTestingData("Generator Multiplication",
                     "Test Generator Mul Result",
                     testGenMulResultBytes);

    bytes_t genMulResultBytes = {
        0x02, 0x68, 0x9a, 0xde, 0xb8, 0x61, 0x17, 0xc9, 0xce, 0x33, 0xef,
        0xee, 0x2f, 0xbe, 0xf1, 0x65, 0xb7, 0xe9, 0x50, 0x45, 0xbe, 0xe7,
        0x11, 0xde, 0x71, 0xd3, 0x97, 0x2c, 0xbb, 0x0b, 0xf5, 0x88, 0x99 };

    PrintTestingData("Generator Multiplication",
                     "Generator Mul Result",
                     genMulResultBytes);

    print_info("Test identity of generator multiplication result.");
    ASSERT_EQ(testGenMulResultBytes, genMulResultBytes);

    print_info("Test generator multiplication result is not at infinity.");
    ASSERT_FALSE(testGenMulPoint.is_at_infinity());

    testGenMulPoint.set_to_infinity();
    print_info("Test infinity point is at infinity.");
    ASSERT_TRUE(testGenMulPoint.is_at_infinity());
}


// signature-related tests
TEST_F(Secp256k1Test, SignAndVerify)
{
    secp256k1_key key;
    key.newKey();

    bytes_t privKey = key.getPrivKey();

    PrintTestingData("Sign and Verify",
                     "Signing Private Key Random Bytes",
                     privKey);

    bytes_t data = {
        0x92, 0x94, 0x46, 0x39, 0x69, 0x12, 0x73, 0x81, 0x6a, 0xc4, 0x63,
        0x78, 0xa4, 0x2c, 0x5c, 0x2b, 0xde, 0x10, 0x89, 0x57, 0x64, 0x05,
        0xca, 0x09, 0xc2, 0xbb, 0xb8, 0x9c, 0xbe, 0xe0, 0x07, 0xc5 };

    print_info("Test signing.");
    ASSERT_NO_THROW(secp256k1_sign(key, data));
    bytes_t testSignature = secp256k1_sign(key, data);

    PrintTestingData("Sign and Verify",
                     "Test Signature",
                     testSignature);

    print_info("Testing signature exists.");
    ASSERT_FALSE(testSignature.empty());

    bool verified = secp256k1_verify(key, data, testSignature);
    print_info("Testing signature is verified.");
    ASSERT_TRUE(verified);

    bytes_t truncatedData = {0xde, 0xad, 0xbe, 0xef};
    print_info("Testing truncated data throws on verify.");
    ASSERT_THROW(secp256k1_verify(key, truncatedData, testSignature),
                 std::invalid_argument);

    bytes_t invalidData = {
        0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
        0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
        0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef };

    verified = secp256k1_verify(key, invalidData, testSignature);
    print_info("Testing invalid data against signature.");
    ASSERT_FALSE(verified);

    bytes_t privKeyBytes = {
        0x81, 0xad, 0xee, 0x36, 0xc4, 0xcb, 0xe1, 0x2d, 0xf0, 0xca, 0x3f,
        0x54, 0xd4, 0x7a, 0x6f, 0x0e, 0x5f, 0xb4, 0x5a, 0x3c, 0xeb, 0xcb,
        0x07, 0xff, 0x8d, 0x7b, 0x87, 0x2a, 0xba, 0x70, 0x7c, 0xd2 };

    PrintTestingData("Sign and Verify",
                     "Signing Private Key Bytes",
                     privKeyBytes);

    bytes_t pubKeyBytes = {
        0x03, 0xd6, 0x28, 0xf3, 0x46, 0x38, 0x5f, 0xd5, 0x45, 0xcb, 0x6c,
        0x96, 0x93, 0xc9, 0xad, 0x17, 0x82, 0x3c, 0x2e, 0x6a, 0xa9, 0x90,
        0x0b, 0xd4, 0xaf, 0x14, 0x61, 0x8d, 0x55, 0x2c, 0x3c, 0x9c, 0x68 };


    PrintTestingData("Sign and Verify",
                     "Signing Public Key Bytes",
                     pubKeyBytes);

    print_info("Setting private key to verify signature.");
    ASSERT_NO_THROW(key.setPrivKey(privKeyBytes));

    bytes_t setPrivKeyBytes = key.getPrivKey();

    PrintTestingData("Sign and Verify",
                     "Set Private Key Bytes",
                     setPrivKeyBytes);

    print_info("Testing identity of set private key.");
    ASSERT_EQ(setPrivKeyBytes, privKeyBytes);

    bytes_t setPubKeyBytes = key.getPubKey();

    PrintTestingData("Sign and Verify",
                     "Set Public Key Bytes",
                     setPubKeyBytes);

    print_info("Testing identity of public key from set private key.");
    ASSERT_EQ(setPubKeyBytes, pubKeyBytes);

    print_info("Test signing with set private key.");
    ASSERT_NO_THROW(secp256k1_sign(key, data));
    bytes_t setPrivKeySignature = secp256k1_sign(key, data);

    PrintTestingData("Sign and Verify",
                     "Signature from Set Private Key",
                     setPrivKeySignature);

    verified = secp256k1_verify(key, data, setPrivKeySignature);
    print_info("Testing set private key verifies its own signature");
    ASSERT_TRUE(verified);

    bytes_t signature = {
        0x30, 0x45, 0x02, 0x21, 0x00, 0xc8, 0xb0, 0x07, 0xa9, 0x99, 0xbf,
        0x74, 0x4d, 0x5b, 0xae, 0xb3, 0x63, 0xbe, 0xc3, 0xe3, 0xa0, 0x3f,
        0xea, 0x73, 0xbb, 0xd9, 0x1a, 0xc8, 0x5a, 0x5d, 0x5f, 0x41, 0xe1,
        0x62, 0x5a, 0xb9, 0x76, 0x02, 0x20, 0x4d, 0x06, 0xf4, 0x0e, 0xab,
        0xae, 0x3a, 0x9b, 0xc7, 0xed, 0xe9, 0x22, 0x2a, 0x9e, 0x82, 0x1d,
        0x25, 0x73, 0xae, 0x5a, 0xfd, 0x1b, 0xfe, 0xf1, 0x76, 0xd6, 0xd1,
        0xe1, 0x78, 0xa5, 0x37, 0x2e };

    PrintTestingData("Sign and Verify", "Signature", signature);

    verified = secp256k1_verify(key, data, signature);

    print_info("Testing set private key verifies signature with public key.");
    ASSERT_TRUE(verified);

    verified = secp256k1_verify(key, invalidData, signature);
    print_info("Testing signature is not valid for invalid data.");
    ASSERT_FALSE(verified);
}

TEST_F(Secp256k1Test, SigToLowS)
{
    bytes_t highSSignature = {
        0x30, 0x46, 0x02, 0x21, 0x00, 0xcc, 0xba, 0xe4, 0x42, 0x4d, 0x86, 0xe6,
        0x10, 0xc1, 0xac, 0x03, 0xa8, 0x3e, 0xfa, 0x58, 0x4b, 0x0d, 0x78, 0xa0,
        0x6c, 0xc9, 0xff, 0x97, 0x0f, 0xb6, 0x0c, 0xe2, 0x95, 0xc0, 0x86, 0x2b,
        0x18, 0x02, 0x21, 0x00, 0xf4, 0x6f, 0x80, 0x63, 0x90, 0x19, 0x7b, 0x3a,
        0x35, 0x32, 0xbf, 0x60, 0x5e, 0x5c, 0xd7, 0x8f, 0x65, 0xfd, 0x4f, 0xd9,
        0x76, 0x8f, 0x3c, 0x2b, 0xf7, 0x94, 0xf6, 0x88, 0xb1, 0x41, 0x14, 0x51 };

    PrintTestingData("Signature to LowS",
                     "High S Signature",
                     highSSignature);

    ASSERT_NO_THROW(secp256k1_sigToLowS(highSSignature));
    bytes_t testLowSSignature = secp256k1_sigToLowS(highSSignature);

    PrintTestingData("Signature to LowS",
                     "Test Low S Signature",
                     testLowSSignature);

    ASSERT_NE(highSSignature, testLowSSignature);

    // Verify that applying secp256k1_sigToLowS again doesn't change the signature
    ASSERT_NO_THROW(secp256k1_sigToLowS(testLowSSignature));
    bytes_t unchangedSignature = secp256k1_sigToLowS(testLowSSignature);
    ASSERT_EQ(testLowSSignature, unchangedSignature);

    bytes_t lowSSignature = {
        0x30, 0x45, 0x02, 0x21, 0x00, 0xcc, 0xba, 0xe4, 0x42, 0x4d, 0x86,
        0xe6, 0x10, 0xc1, 0xac, 0x03, 0xa8, 0x3e, 0xfa, 0x58, 0x4b, 0x0d,
        0x78, 0xa0, 0x6c, 0xc9, 0xff, 0x97, 0x0f, 0xb6, 0x0c, 0xe2, 0x95,
        0xc0, 0x86, 0x2b, 0x18, 0x02, 0x20, 0x0b, 0x90, 0x7f, 0x9c, 0x6f,
        0xe6, 0x84, 0xc5, 0xca, 0xcd, 0x40, 0x9f, 0xa1, 0xa3, 0x28, 0x6f,
        0x54, 0xb1, 0x8d, 0x0d, 0x38, 0xb9, 0x64, 0x0f, 0xc8, 0x3d, 0x68,
        0x04, 0x1e, 0xf5, 0x2c, 0xf0 };

    PrintTestingData("Signature to LowS",
                     "Low S Signature",
                     lowSSignature);

    ASSERT_EQ(testLowSSignature, lowSSignature);
}


/**********************************************************************
 * Please add new tests above this comment.
 **********************************************************************/


# ifdef DO_RFC6979_TESTS
// Support for rfc6979 has been dropped, but these tests are
//    kept here because the Ciphrex standard implemented them.
TEST_F(Secp256k1Test, SignAndVerifyRFC6979)
{
    secp256k1_key key;
    key.newKey();

    bytes_t privKey = key.getPrivKey();

    PrintTestingData("Sign and Verify RFC6979",
                     "Random Signing Private Key",
                     privKey);

    bytes_t data = {
        0x92, 0x94, 0x46, 0x39, 0x69, 0x12, 0x73, 0x81, 0x6a, 0xc4, 0x63,
        0x78, 0xa4, 0x2c, 0x5c, 0x2b, 0xde, 0x10, 0x89, 0x57, 0x64, 0x05,
        0xca, 0x09, 0xc2, 0xbb, 0xb8, 0x9c, 0xbe, 0xe0, 0x07, 0xc5 };

    ASSERT_NO_THROW(secp256k1_sign_rfc6979(key, data));
    bytes_t testSignature = secp256k1_sign(key, data);

    PrintTestingData("Sign and Verify RFC6979",
                     "Test Signature",
                     testSignature);

    ASSERT_FALSE(testSignature.empty());
    bool verified = secp256k1_verify(key, data, testSignature);
    ASSERT_TRUE(verified);

    bytes_t truncatedData = {0xde, 0xad, 0xbe, 0xef};
    print_info("Testing truncated data throws on verify.");
    ASSERT_THROW(secp256k1_verify(key, truncatedData, testSignature),
                 std::invalid_argument);

    bytes_t invalidData = {
        0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
        0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
        0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef };

    verified = secp256k1_verify(key, invalidData, testSignature);
    ASSERT_FALSE(verified);

    bytes_t privKeyBytes = {
        0x80, 0xd3, 0xab, 0xaa, 0x62, 0x20, 0x81, 0xb1, 0x24, 0xd1, 0x50,
        0x17, 0x16, 0x9f, 0xb4, 0x33, 0x8c, 0x08, 0x07, 0x9d, 0x15, 0x05,
        0x60, 0xdb, 0x0a, 0xc4, 0x9f, 0x74, 0xa0, 0x81, 0x4e, 0xbc };

    PrintTestingData("Sign and Verify RFC6979",
                     "Signing Private Key Bytes",
                     privKeyBytes);

    ASSERT_NO_THROW(key.setPrivKey(privKeyBytes));

    // rfc6979 signature
    bytes_t signature = {
        0x30, 0x44, 0x02, 0x20, 0x2c, 0xf3, 0xd3, 0x2e, 0xe6, 0xd1, 0xfe,
        0xac, 0x62, 0xd7, 0x89, 0x0a, 0x2f, 0x26, 0x36, 0x68, 0x98, 0x88,
        0x10, 0x4f, 0x6c, 0x66, 0xa8, 0x23, 0xf2, 0xf2, 0xc3, 0xa7, 0xbb,
        0x25, 0xb3, 0x5d, 0x02, 0x20, 0x73, 0xc0, 0x5a, 0x29, 0x90, 0x1c,
        0xe7, 0xd0, 0xb1, 0xa8, 0x48, 0xcf, 0x6b, 0xae, 0x2b, 0x9a, 0xd1,
        0x1b, 0x7e, 0x90, 0x15, 0xed, 0x57, 0x03, 0xc1, 0xf6, 0xf8, 0x5b,
        0x73, 0xa3, 0x16, 0xc7 };

    PrintTestingData("Sign and Verify RFC6979", "Signature", signature);

    verified = secp256k1_verify(key, data, signature);
    ASSERT_TRUE(verified);

    verified = secp256k1_verify(key, invalidData, signature);
    ASSERT_FALSE(verified);
}

/**********************************************************************
 * Please add new tests above this latter section (RFC6979).
 **********************************************************************/

# endif
