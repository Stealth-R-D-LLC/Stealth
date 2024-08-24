#include "test-utils.hpp"

#include <openssl/objects.h>

using namespace std;

bool DEBUG = false;

void set_debug(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0)
        {
            if (i + 1 < argc)
            {
                string value = argv[++i];
                if (value == "1" || value == "true")
                {
                    DEBUG = true;
                }
                else if (value == "0" || value == "false")
                {
                    DEBUG = false;
                }
                else
                {
                    cerr << "Invalid value for debug flag. "
                             "Using default (false)."
                         << endl;
                }
            }
            else
            {
                DEBUG = true;
            }
        }
    }
}

string FormatUint256(const uint256& value)
{
    return "        " + value.GetHex();
}

string FormatUint160(const uint160& value)
{
    return "        " + value.GetHex();
}

void print_info(const string& strContent,
                bool fForcePrint,
                const string& strName,
                const string& strColor,
                const string& strPad)
{
    if (fForcePrint || DEBUG)
    {
        unsigned int rgb;
        istringstream(strColor) >> hex >> rgb;
        int r = (rgb >> 16) & 0xFF;
        int g = (rgb >> 8) & 0xFF;
        int b = rgb & 0xFF;
        cout << "\033[38;2;" << r << ";" << g << ";" << b << "m"
             << strName << "\033[0m";
        cout << strPad;
        cout << strContent << endl;
    }
}

void print_part(const string& strContent)
{
    print_info(strContent, false, TU_PART, "ffffaa");
}

void print_note(const string& strContent)
{
    print_info(strContent, true, TU_NOTE, "ff55ff");
}

void print_warning(const string& strContent)
{
    print_info(strContent, true, TU_WARNING, "ff5555");
}

string FormatTestingDataTitle(const string& strTestName,
                              const string& strDataName,
                              const string& strTitleColor)
{
    ostringstream oss;
    string title = "Test of " + strTestName + " - " + strDataName;
    string line(title.length(), '=');

    string titleColorCode = "\033[38;2;" +
        to_string(stoi(strTitleColor.substr(0,2), nullptr, 16)) + ";" +
        to_string(stoi(strTitleColor.substr(2,2), nullptr, 16)) + ";" +
        to_string(stoi(strTitleColor.substr(4,2), nullptr, 16)) + "m";
    string resetCode = "\033[0m";

    oss << titleColorCode << line << "\n"
        << title << "\n"
        << line << resetCode << "\n";

    return oss.str();
}

string FormatTestingData(const string& strTestName,
                         const string& strDataName,
                         const uint256& value,
                         const string& strTitleColor,
                         const string& strDataColor)
{
    ostringstream oss;
    oss << FormatTestingDataTitle(strTestName, strDataName, strTitleColor);

    string dataColorCode = "\033[38;2;" +
        to_string(stoi(strDataColor.substr(0,2), nullptr, 16)) + ";" +
        to_string(stoi(strDataColor.substr(2,2), nullptr, 16)) + ";" +
        to_string(stoi(strDataColor.substr(4,2), nullptr, 16)) + "m";
    string resetCode = "\033[0m";

    oss << dataColorCode << FormatUint256(value) << resetCode << "\n\n";
    return oss.str();
}

string FormatTestingData(const string& strTestName,
                         const string& strDataName,
                         const uint160& value,
                         const string& strTitleColor,
                         const string& strDataColor)
{
    ostringstream oss;
    oss << FormatTestingDataTitle(strTestName, strDataName, strTitleColor);

    string dataColorCode = "\033[38;2;" +
        to_string(stoi(strDataColor.substr(0,2), nullptr, 16)) + ";" +
        to_string(stoi(strDataColor.substr(2,2), nullptr, 16)) + ";" +
        to_string(stoi(strDataColor.substr(4,2), nullptr, 16)) + "m";
    string resetCode = "\033[0m";

    oss << dataColorCode << FormatUint160(value) << resetCode << "\n\n";
    return oss.str();
}

void PrintTestingData(const string& strTestName,
                      const string& strDataName,
                      const uint256& value,
                      const string& strTitleColor,
                      const string& strDataColor)
{
    if (DEBUG)
    {
        cout << FormatTestingData(strTestName,
                                  strDataName,
                                  value,
                                  strTitleColor,
                                  strDataColor);
    }
}

void PrintTestingData(const string& strTestName,
                      const string& strDataName,
                      const uint160& value,
                      const string& strTitleColor,
                      const string& strDataColor)
{
    if (DEBUG)
    {
        cout << FormatTestingData(strTestName,
                                  strDataName,
                                  value,
                                  strTitleColor,
                                  strDataColor);
    }
}

// necessary to test against older version of OpenSSL
EC_GROUP* get_secp256k1_group()
{
    EC_GROUP *group;
    int nid = OBJ_sn2nid("secp256k1");
    if (nid == NID_undef)
    {
        throw runtime_error("Failed to locate secp256k1");
    }
    else
    {
        group = EC_GROUP_new_by_curve_name(nid);
        if (group == NULL)
        {
            throw runtime_error("Failed to create secp256k1 group");
        }
    }
    return group;
}


void randomPoint(EC_POINT* &point, EC_GROUP* &group, bool compressed)
{

    if (!group)
    {
        throw runtime_error("Group is NULL");
    }

    const point_conversion_form_t compression = compressed ?
                                      POINT_CONVERSION_COMPRESSED :
                                      POINT_CONVERSION_UNCOMPRESSED;

    if (point)
    {
        EC_POINT_free(point);
    }

    point = EC_POINT_new(group);
    if (!point)
    {
        EC_GROUP_free(group);
        throw runtime_error("Failed to create EC_POINT");
    }

    BIGNUM* private_key = BN_new();
    if (!private_key)
    {
        EC_POINT_free(point);
        EC_GROUP_free(group);
        throw runtime_error("Failed to create BIGNUM");
    }

    if (BN_rand_range(private_key, EC_GROUP_get0_order(group)) != 1)
    {
        BN_free(private_key);
        EC_POINT_free(point);
        EC_GROUP_free(group);
        throw runtime_error("Failed to generate random number");
    }

    if (EC_POINT_mul(group, point, private_key, NULL, NULL, NULL) != 1)
    {
        BN_free(private_key);
        EC_POINT_free(point);
        EC_GROUP_free(group);
        throw runtime_error("Failed to compute public key");
    }

    BN_free(private_key);
}

void randomPoint(EC_POINT* &point, bool compressed)
{
    EC_GROUP* group = get_secp256k1_group();
    randomPoint(point, group, compressed);
    EC_GROUP_free(group);
}
