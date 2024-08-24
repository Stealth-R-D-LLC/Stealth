
#include "test-utils.hpp"

#include "util.h"


using namespace std;


size_t TEST_MESSAGE_SIZE = 32;


class UtilTest : public ::testing::Test
{
protected:
    void SetUp() override {}
};


int main(int argc, char **argv)
{

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    print_note("Testing with OpenSSL version < 3.0.");
#endif

    set_debug(argc, argv);

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}


EC_POINT* random_point(EC_GROUP* group, bool compressed=true)
{
    EC_POINT* p = NULL;
    randomPoint(p, group, compressed);
    return p;
}

void random_point(valtype& vchPoint, EC_GROUP* group, bool compressed=true)
{
    const point_conversion_form_t compression = compressed ?
                                      POINT_CONVERSION_COMPRESSED :
                                      POINT_CONVERSION_UNCOMPRESSED;
    EC_POINT* point = random_point(group, compressed);

    size_t nSize = EC_POINT_point2oct(group, point,
                                      compression,
                                      NULL, 0, NULL);
    vchPoint.resize(nSize);
    if (EC_POINT_point2oct(group, point,
                           compression,
                           vchPoint.data(), nSize, NULL) != nSize)
    {
        EC_POINT_free(point);
        throw std::runtime_error("Failed to serialize new point");
    }

    EC_POINT_free(point);
}

#ifndef USING_EXTERNAL_STEALTH
void round_trip(bool compressed, string& strUnit)
{
    const point_conversion_form_t compression = compressed ?
                                      POINT_CONVERSION_COMPRESSED :
                                      POINT_CONVERSION_UNCOMPRESSED;

    EC_GROUP* group = get_secp256k1_group();

    valtype vchPoint;
    random_point(vchPoint, group, compressed);

    PrintTestingData(strUnit, "Point Data", vchPoint);

    //////////////////////////////////////////////////////////////////////////
    // OCT ==> POINT  |  (vchPoint ==> point)
    EC_POINT* point = EC_POINT_new(group);
    if (point == NULL)
    {
        EC_GROUP_free(group);
        EC_POINT_free(point);
        throw std::runtime_error("Failed to create new EC_POINT");
    }

    if (EC_POINT_oct2point(group, point,
                           vchPoint.data(), vchPoint.size(), NULL) != 1)
    {
        EC_GROUP_free(group);
        EC_POINT_free(point);
        throw std::runtime_error("Failed to de-serialize point");
    }
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // ** POINT ==> BIGNUM **  |  (point ==> bnPoint)
    BIGNUM* bnPoint = BN_new();
    if (!bnPoint)
    {
        EC_GROUP_free(group);
        EC_POINT_free(point);
        throw std::runtime_error("Failed to create new BIGNUM");
    }

    BN_CTX* ctx = BN_CTX_new();
    if (!ctx)
    {
        EC_GROUP_free(group);
        EC_POINT_free(point);
        BN_free(bnPoint);
        throw std::runtime_error("Failed to create new BIGNUM context");
    }

    BIGNUM* p2bResult = ECPoint2BIGNUM(group, point,
                                       compression,
                                       bnPoint, ctx);

    if (!p2bResult)
    {
        EC_GROUP_free(group);
        EC_POINT_free(point);
        BN_CTX_free(ctx);
        BN_free(bnPoint);
        throw std::runtime_error("ECPoint2BIGNUM failed");
    }

    ASSERT_EQ(p2bResult, bnPoint);
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // BIGNUM ==> OCT  |  bnPoint ==> vBN
    int nBNSize = BN_num_bytes(bnPoint);
    valtype vchBN(nBNSize);
    BN_bn2bin(bnPoint, vchBN.data());

    PrintTestingData(strUnit, "BIGNUM Data", vchBN);

    print_info("Testing Identity of BIGNUM (ECPoint2BIGNUM)");
    if (vchBN != vchPoint)
    {
        EC_GROUP_free(group);
        EC_POINT_free(point);
        BN_CTX_free(ctx);
        BN_free(bnPoint);
        throw std::runtime_error("BIGNUM from Point is Incorrect");
    }
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // ** BIGNUM ==> POINT **  |  (bnPoint ==> recovered)
    EC_POINT* recovered = EC_POINT_new(group);
    if (recovered == NULL)
    {
        EC_GROUP_free(group);
        EC_POINT_free(point);
        BN_CTX_free(ctx);
        BN_free(bnPoint);
        throw std::runtime_error("Failed to create recovered point");
    }

    EC_POINT* b2pResult = BIGNUM2ECPoint(group, bnPoint, recovered, ctx);
    if (b2pResult == NULL)
    {
        EC_GROUP_free(group);
        EC_POINT_free(point);
        BN_CTX_free(ctx);
        BN_free(bnPoint);
        EC_POINT_free(recovered);
        throw std::runtime_error("ECPoint2BIGNUM failed");
    }

    ASSERT_EQ(b2pResult, recovered);

    print_info("Testing Identity of Recovered Point (BIGNUM2ECPoint)");
    bool fRecoveryResult = EC_POINT_cmp(group, point, recovered, NULL);

    if (fRecoveryResult != 0)
    {
        EC_GROUP_free(group);
        EC_POINT_free(point);
        BN_CTX_free(ctx);
        BN_free(bnPoint);
        EC_POINT_free(recovered);
        throw std::runtime_error("Recovered point doesn't match original");
    }
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // POINT==>OCT
    size_t nRecSize = EC_POINT_point2oct(group, recovered,
                                         compression,
                                         NULL, 0, NULL);
    valtype vchRec(nRecSize);
    if (EC_POINT_point2oct(group, recovered,
                           compression,
                           vchRec.data(), nRecSize, NULL) != nRecSize)
    {
        EC_GROUP_free(group);
        EC_POINT_free(point);
        BN_CTX_free(ctx);
        BN_free(bnPoint);
        EC_POINT_free(recovered);
        throw std::runtime_error("Failed to serialize point");
    }
    PrintTestingData(strUnit, "Recovered Data", vchRec);

    print_info("Testing Identity of Recovered Point Data (BIGNUM2ECPoint)");
    if (vchRec != vchPoint)
    {
        EC_GROUP_free(group);
        EC_POINT_free(point);
        BN_CTX_free(ctx);
        BN_free(bnPoint);
        EC_POINT_free(recovered);
        throw std::runtime_error("Recovered point data "
                                    "doesn't match original");
    }
    //////////////////////////////////////////////////////////////////////////

    EC_GROUP_free(group);
    EC_POINT_free(point);
    BN_CTX_free(ctx);
    BN_free(bnPoint);
    EC_POINT_free(recovered);
}

TEST_F(UtilTest, ECPointToBigNumToECPoint)
{
    string strUnit("ECPointToBigNumToECPoint");

    print_part("Testing Compressed EC Point");
    round_trip(true, strUnit);

    print_part("Testing Uncompressed EC Point");
    round_trip(false, strUnit);
}
#endif


TEST_F(UtilTest, SerializeHash)
{
   // valtype vchData;
   // generateRandomData(32, vchData);

   vector<unsigned char> vchData = {
        0x59, 0x71, 0xa2, 0xba, 0x7f, 0x93, 0x80, 0xcf, 0x66, 0x15, 0x0a, 0xc3,
        0x2c, 0x31, 0x02, 0xfe, 0x31, 0x01, 0x84, 0xe8, 0xf9, 0x64, 0xab, 0x0d,
        0x9b, 0x90, 0x01, 0xfe, 0xd2, 0x55, 0x0b, 0x36 };

   PrintTestingData("SerializeHash", "Data", vchData);

   uint256 hash = SerializeHash(vchData);

   PrintTestingData("SerializeHash", "Serialized Hash", hash);

   uint256 hashExpected(
       "0x917f40d4e79a617515cf3124fa28b4f9679121abe79309132ec4de72645e6777");

   PrintTestingData("SerializeHash", "Expected Hash", hashExpected);

   // valtype vchHash((const char*) &hash,
   //                 (const char*) &hash + (hash.size() * sizeof(char)));
   // PrintTestingData("CHashWriter", "Serialized Hash Data", vchHash);

   print_info("Testing Identity of CHashWriter Hash");
   ASSERT_EQ(hash, hashExpected);
}


TEST_F(UtilTest, CHashWriterWrite)
{
   vector<unsigned char> vchData = {
        0x59, 0x71, 0xa2, 0xba, 0x7f, 0x93, 0x80, 0xcf, 0x66, 0x15, 0x0a, 0xc3,
        0x2c, 0x31, 0x02, 0xfe, 0x31, 0x01, 0x84, 0xe8, 0xf9, 0x64, 0xab, 0x0d,
        0x9b, 0x90, 0x01, 0xfe, 0xd2, 0x55, 0x0b, 0x36 };

   PrintTestingData("CHashWriterWrite", "Data", vchData);

   CHashWriter writer(SER_GETHASH, PROTOCOL_VERSION);
   writer.write((const char*) vchData.data(), vchData.size());
   uint256 write_hash = writer.GetHash();

   PrintTestingData("CHashWriterWrite", "Write Hash", write_hash);

   uint256 writeHashExpected(
       "0x906fd3209a26ad4e35350c387337748a85dc9aacfca6ddfe02e985f83cb3d9f8");

   PrintTestingData("CHashWriterWrite", "Expected Write Hash", writeHashExpected);

   // valtype vchWriteHash((const char*) &write_hash,
   //                      (const char*) &write_hash +
   //                          (write_hash.size() * sizeof(char)));
   // PrintTestingData("CHashWriter", "Write Hash Data", vchWriteHash);

   print_info("Testing Identity of CHashWriter Write Hash");
   ASSERT_EQ(write_hash, writeHashExpected);
}


TEST_F(UtilTest, Hash)
{
   // valtype vchData1;
   // generateRandomData(32, vchData1);
   valtype vchData1 = {
        0x11, 0xff, 0x6d, 0xcf, 0x06, 0xbd, 0x9b, 0xd9, 0x8c, 0xb6, 0xb2, 0xd8,
        0x39, 0x57, 0x2f, 0xc5, 0xc1, 0x86, 0x7b, 0x5c, 0x35, 0xf1, 0xf8, 0x61,
        0x9a, 0x1c, 0xe5, 0x99, 0xe0, 0x37, 0xab, 0x77 };

   // valtype vchData2;
   // generateRandomData(32, vchData2);
   valtype vchData2 = {
        0x61, 0x75, 0x41, 0xfa, 0x53, 0x90, 0xa2, 0x04, 0xf7, 0x06, 0xbf, 0x49,
        0xf0, 0x4d, 0xce, 0x64, 0xf2, 0x8f, 0x70, 0x5a, 0xdb, 0x83, 0xa1, 0xf6,
        0xa9, 0x79, 0x50, 0x37, 0xa7, 0xcc, 0x6f, 0x3c };

   // valtype vchData3;
   // generateRandomData(32, vchData3);
   valtype vchData3 = {
        0x1e, 0xec, 0x2f, 0x3a, 0xda, 0xac, 0x3e, 0x5c, 0x80, 0x9c, 0xfd, 0xa1,
        0x10, 0xf8, 0xd2, 0xd8, 0x24, 0xfe, 0xee, 0xa6, 0x31, 0xc0, 0x1e, 0x6f,
        0x28, 0x28, 0x1d, 0xd3, 0xf3, 0xed, 0x4f, 0x2a };

   PrintTestingData("Hash", "Data1", vchData1);
   PrintTestingData("Hash", "Data2", vchData2);
   PrintTestingData("Hash", "Data3", vchData3);

   uint256 hash2 = Hash(vchData1.data(),
                        vchData1.data() + (vchData1.size() * sizeof(char)),
                        vchData2.data(),
                        vchData2.data() + (vchData2.size() * sizeof(char)));

   PrintTestingData("Hash", "Hash 2 (Data1 & Data2)", hash2);

   uint256 hash2Expected(
       "0x38648df81909d5779bfc024cbac7d0ab2779b7292ba3974cbd3c6318408a2cb1");

   PrintTestingData("Hash", "Expected Hash 2", hash2Expected);

   print_info("Testing Identity of Hash 2");
   ASSERT_EQ(hash2, hash2Expected);

   uint256 hash3 = Hash(vchData1.data(),
                        vchData1.data() + (vchData1.size() * sizeof(char)),
                        vchData2.data(),
                        vchData2.data() + (vchData2.size() * sizeof(char)),
                        vchData3.data(),
                        vchData3.data() + (vchData3.size() * sizeof(char)));

   PrintTestingData("Hash", "Hash 3 (Data1, Data2, & Data3)", hash3);

   uint256 hash3Expected(
       "0xceff2e1d690d8de08741241a4dfba63867f3f280e5f5ad84ed2201f98850d000");

   PrintTestingData("Hash", "Expected Hash 3", hash3Expected);

   print_info("Testing Identity of Hash 3");
   ASSERT_EQ(hash3, hash3Expected);
}

TEST_F(UtilTest, Hash160)
{
   vector<unsigned char> vchData = {
        0x59, 0x71, 0xa2, 0xba, 0x7f, 0x93, 0x80, 0xcf, 0x66, 0x15, 0x0a, 0xc3,
        0x2c, 0x31, 0x02, 0xfe, 0x31, 0x01, 0x84, 0xe8, 0xf9, 0x64, 0xab, 0x0d,
        0x9b, 0x90, 0x01, 0xfe, 0xd2, 0x55, 0x0b, 0x36 };

   PrintTestingData("Hash160", "Data", vchData);

   uint160 hash160 = Hash160(vchData);

   PrintTestingData("Hash160", "Hash160", hash160);

   uint160 hash160Expected("0x1830fdab88d74e196f8605b1a0ed9daecb511a84");

   PrintTestingData("Hash160", "Expected Hash160", hash160Expected);

   print_info("Testing Identity of Hash160");
   ASSERT_EQ(hash160, hash160Expected);
}
