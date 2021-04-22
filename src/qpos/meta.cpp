// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "meta.hpp"

#include <boost/assign/list_of.hpp> // for 'map_list_of()'

#include <map>


typedef std::map<std::string, QPKeyType> MapMetaKeys;

static const MapMetaKeys mapMetaKeys =
    boost::assign::map_list_of
        (    "certified_node",  QPKEY_OMD )
                ;

QPKeyType CheckMetaKey(const string &sKey)
{
    MapMetaKeys::const_iterator it;
    for (it = mapMetaKeys.begin(); it != mapMetaKeys.end(); ++it)
    {
        if (sKey == it->first)
        {
            return it->second;
        }
    }
    return QPKEY_NONE;
}

bool CheckMetaValue(const string &sValue)
{
    const size_t N = sValue.size();
    if (N > QP_MAX_META_VALUE_LENGTH)
    {
        return false;
    }
    for (unsigned int n = 0; n < N; ++n)
    {
        switch (sValue[n])
        {
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
            case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
            case 's': case 't': case 'u': case 'v': case 'w': case 'x':
            case 'y': case 'z':
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
            case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
            case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
            case 'Y': case 'Z':
            case '0': case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9':
            case '.': case '_': case ':': case ' ': case '<': case '>':
            case '/': case '@': case '#': case ',': case '+': case '-':
                break;
            default:
                return false;
        }
    }
    return true;
}

