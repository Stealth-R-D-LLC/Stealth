
#include "core-hashes.hpp"

#include "test-utils.hpp"


using namespace std;


size_t TEST_MESSAGE_SIZE = 32;


class CoreHashesTest : public ::testing::Test
{
protected:
    void SetUp() override {}
};


int main(int argc, char **argv)
{

    set_debug(argc, argv);

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

valtype RandomMessage(size_t length=32)
{
    valtype vchMessage;
    generateRandomData(length, vchMessage);
    return vchMessage;
}


TEST_F(CoreHashesTest, SHA256)
{
    // Generate random private key
    // valtype vchMessage = RandomMessage();
    valtype vchMessage = {
        0x4d, 0x75, 0xbf, 0x29, 0x4b, 0xdd, 0x55, 0x78, 0x87, 0x9f, 0x47, 0x35,
        0xf4, 0xd4, 0xf7, 0x21, 0xb8, 0x69, 0xb0, 0x5d, 0x5d, 0x75, 0x0d, 0xb5,
        0x0c, 0x80, 0xca, 0x51, 0x45, 0xc2, 0xfc, 0xad };

    PrintTestingData("SHA256", "Test Message", vchMessage);

    valtype vchDigest(SHA256_SIZE);

    print_info("Testing hash calculation.");
    ASSERT_NO_THROW(CoreHashes::SHA256(vchMessage.data(),
                                       vchMessage.size(),
                                       vchDigest.data()));

    PrintTestingData("SHA256", "Calculated Hash", vchDigest);

    valtype vchExpected = {
        0xea, 0xb3, 0xa5, 0x11, 0xeb, 0x67, 0xc4, 0xfc, 0xec, 0x3e, 0x9a, 0xd9,
        0x47, 0xfa, 0x86, 0xd8, 0x8d, 0x18, 0x07, 0xb2, 0x65, 0x23, 0x14, 0x74,
        0xf9, 0x6d, 0x39, 0xb8, 0xdc, 0xc3, 0x25, 0x9e };

    PrintTestingData("SHA256", "Expected Hash", vchExpected);

    print_info("Testing identity of calculated hash.");
    ASSERT_EQ(vchDigest, vchExpected);
}

TEST_F(CoreHashesTest, SHA1)
{
    // Generate random private key
    // valtype vchMessage = RandomMessage();
    valtype vchMessage = {
        0x41, 0x89, 0xaf, 0x41, 0x35, 0x69, 0x32, 0x5d, 0x99, 0x40, 0x75, 0xd8,
        0x8f, 0x35, 0x44, 0x4f, 0x6b, 0x76, 0x74, 0x03, 0xe8, 0x4d, 0xbe, 0x51,
        0x3e, 0x94, 0xcc, 0x6c, 0x98, 0xbe, 0xd8, 0x69 };

    PrintTestingData("SHA1", "Test Message", vchMessage);

    valtype vchDigest(SHA1_SIZE);

    print_info("Testing hash calculation.");
    ASSERT_NO_THROW(CoreHashes::SHA1(vchMessage.data(),
                                     vchMessage.size(),
                                     vchDigest.data()));

    PrintTestingData("SHA1", "Calculated Hash", vchDigest);

    valtype vchExpected = {
        0xe8, 0xd0, 0xc7, 0x47, 0x33, 0x48, 0x03, 0xfd, 0x39, 0xd2, 0xb8, 0x71,
        0x37, 0xbd, 0x29, 0x85, 0x4d, 0xd6, 0xb4, 0xb8 };

    PrintTestingData("SHA1", "Expected Hash", vchExpected);

    print_info("Testing identity of calculated hash.");
    ASSERT_EQ(vchDigest, vchExpected);
}


TEST_F(CoreHashesTest, SIPEMD160)
{
    // Generate random private key
    // valtype vchMessage = RandomMessage();
    valtype vchMessage = {
        0xa6, 0x68, 0xda, 0x38, 0x58, 0xac, 0x33, 0xef, 0xed, 0x38, 0x98, 0xfe,
        0xbf, 0x17, 0xa7, 0xb0, 0x8d, 0x29, 0x7d, 0x7b, 0xe2, 0x5d, 0xb6, 0xdc,
        0xc6, 0x6b, 0x10, 0x1a, 0xcf, 0x56, 0x6b, 0x6b };

    PrintTestingData("RIPEMD160", "Test Message", vchMessage);

    valtype vchDigest(RIPEMD160_SIZE);

    print_info("Testing hash calculation.");
    ASSERT_NO_THROW(CoreHashes::RIPEMD160(vchMessage.data(),
                                          vchMessage.size(),
                                          vchDigest.data()));


    PrintTestingData("RIPEMD160", "Calculated Hash", vchDigest);

    valtype vchExpected = {
        0xbf, 0x77, 0xed, 0x9a, 0x2e, 0x63, 0x7c, 0xda, 0xaa, 0x9e, 0x84, 0x23,
        0xa0, 0xb8, 0x46, 0x3d, 0x3f, 0x0e, 0xd2, 0xae };

    PrintTestingData("RIPEMD160", "Expected Hash", vchExpected);

    print_info("Testing identity of calculated hash.");
    ASSERT_EQ(vchDigest, vchExpected);
}
