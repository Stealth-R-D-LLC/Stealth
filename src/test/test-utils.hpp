#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "uint256.h"
#include "valtype.hpp"

#include <gtest/gtest.h>

#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/rand.h>

#include <iomanip>
#include <iostream>
#include <sstream>


#define TU_INFO "[   INFO   ]"
#define TU_PART "[   PART   ]"
#define TU_NOTE "[   NOTE   ]"
#define TU_WARNING "[ WARNING  ]"


extern bool DEBUG;

void set_debug(int argc, char **argv);

void print_info(const std::string& strContent,
                bool fForcePrint=false,
                const std::string& strName=TU_INFO,
                const std::string& strColor="77ff77",
                const std::string& strPad=" ");

void print_part(const std::string& strContent);

void print_note(const std::string& strContent);

void print_warning(const std::string& strContent);

template<typename T>
std::string FormatHex(const T& data)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < data.size(); ++i)
    {
        if (i == 0)
        {
            oss << "        ";
        }
        else if (i % 12 == 0)
        {
            oss << "\n        ";
        }
        oss << "0x" << std::setw(2) << static_cast<int>(data[i]);
        if (i < data.size() - 1)
        {
            oss << ", ";
        }
    }
    return oss.str();
}

std::string FormatUint256(const uint256& value);
std::string FormatUint160(const uint160& value);


std::string FormatTestingDataTitle(const std::string& strTestName,
                                   const std::string& strDataName,
                                   const std::string& strTitleColor);

std::string FormatTestingData(const std::string& strTestName,
                         const std::string& strDataName,
                         const uint256& value,
                         const std::string& strTitleColor,
                         const std::string& strDataColor);

std::string FormatTestingData(const std::string& strTestName,
                         const std::string& strDataName,
                         const uint160& value,
                         const std::string& strTitleColor,
                         const std::string& strDataColor);

template<typename T>
std::string FormatTestingData(const std::string& strTestName,
                              const std::string& strDataName,
                              const T& data,
                              const std::string& strTitleColor,
                              const std::string& strDataColor)
{
    std::ostringstream oss;
    oss << FormatTestingDataTitle(strTestName, strDataName, strTitleColor);

    std::string dataColorCode = "\033[38;2;" +
        std::to_string(stoi(strDataColor.substr(0,2), nullptr, 16)) + ";" +
        std::to_string(stoi(strDataColor.substr(2,2), nullptr, 16)) + ";" +
        std::to_string(stoi(strDataColor.substr(4,2), nullptr, 16)) + "m";
    std::string resetCode = "\033[0m";

    oss << dataColorCode << FormatHex(data) << resetCode << "\n\n";
    return oss.str();
}

void PrintTestingData(const std::string& strTestName,
                      const std::string& strDataName,
                      const uint256& value,
                      const std::string& strTitleColor="ff9900",
                      const std::string& strDataColor="00ccff");

void PrintTestingData(const std::string& strTestName,
                      const std::string& strDataName,
                      const uint160& value,
                      const std::string& strTitleColor="ff9900",
                      const std::string& strDataColor="00ccff");

template<typename T>
void PrintTestingData(const std::string& strTestName,
                      const std::string& strDataName,
                      const T& value,
                      const std::string& strTitleColor="ff9900",
                      const std::string& strDataColor="00ccff")
{
    if (DEBUG)
    {
        std::cout << FormatTestingData(strTestName,
                                       strDataName,
                                       value,
                                       strTitleColor,
                                       strDataColor);
    }
}

template<typename VECTOR>
void generateRandomData(size_t size, VECTOR& vData)
{
    vData.resize(size);
    RAND_bytes(vData.data(), size);
}

EC_GROUP* get_secp256k1_group();

void randomPoint(EC_POINT* &point, EC_GROUP* &group, bool compressed=true);

void randomPoint(EC_POINT* &point, bool compressed=true);

template<typename Vector>
void randomPoint(Vector &vPoint, EC_GROUP* &group, bool compressed=true)
{

    if (!group)
    {
        throw std::runtime_error("Group is NULL.");
    }

    const point_conversion_form_t compression = compressed ?
                                      POINT_CONVERSION_COMPRESSED :
                                      POINT_CONVERSION_UNCOMPRESSED;

    EC_POINT* point = NULL;
    randomPoint(point, group, compressed);
    if (!point)
    {
        throw std::runtime_error("Failed to create point");
    }

    size_t buf_len = EC_POINT_point2oct(group, point,
                                        compression,
                                        NULL, 0, NULL);
    vPoint.resize(buf_len);

    if (EC_POINT_point2oct(group, point,
                           compression,
                           vPoint.data(), buf_len, NULL) != buf_len)
    {
        EC_POINT_free(point);
        throw std::runtime_error("Failed to serialize point");
    }

    EC_POINT_free(point);
}


template<typename Vector>
void randomPoint(Vector& vPoint, bool compressed=true)
{
    EC_GROUP* group = get_secp256k1_group();
    randomPoint(vPoint, group, compressed);
    EC_GROUP_free(group);
}


#endif  // TEST_UTILS_H
