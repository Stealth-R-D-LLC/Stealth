// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "aliases.hpp"

using namespace std;

string Despace(const string &s)
{
    string result;
    for (unsigned int i = 0; i < s.size(); ++i)
    {
        if (s[i] != ' ')
        {
            result.push_back(s[i]);
        }
    }
    return result;
}

string ToLowercaseSafe(const string &s)
{
    string t = s;
    for (unsigned int i = 0; i < t.size(); ++i)
    {
        switch(t[i])
        {
            case 'A': t[i] = 'a'; break;
            case 'B': t[i] = 'b'; break;
            case 'C': t[i] = 'c'; break;
            case 'D': t[i] = 'd'; break;
            case 'E': t[i] = 'e'; break;
            case 'F': t[i] = 'f'; break;
            case 'G': t[i] = 'g'; break;
            case 'H': t[i] = 'h'; break;
            case 'I': t[i] = 'i'; break;
            case 'J': t[i] = 'j'; break;
            case 'K': t[i] = 'k'; break;
            case 'L': t[i] = 'l'; break;
            case 'M': t[i] = 'm'; break;
            case 'N': t[i] = 'n'; break;
            case 'O': t[i] = 'o'; break;
            case 'P': t[i] = 'p'; break;
            case 'Q': t[i] = 'q'; break;
            case 'R': t[i] = 'r'; break;
            case 'S': t[i] = 's'; break;
            case 'T': t[i] = 't'; break;
            case 'U': t[i] = 'u'; break;
            case 'V': t[i] = 'v'; break;
            case 'W': t[i] = 'w'; break;
            case 'X': t[i] = 'x'; break;
            case 'Y': t[i] = 'y'; break;
            case 'Z': t[i] = 'z'; break;
            default: continue;
        }
    }
    return t;
}


bool AliasIsValid(const string &sAlias)
{
    const size_t N = sAlias.size();
    if ((N < QP_MIN_ALIAS_LENGTH) || (N > QP_MAX_ALIAS_LENGTH))
    {
        return false;
    }
    switch (sAlias[0])
    {
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
        case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
        case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
        case 'v': case 'w': case 'x': case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
        case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
        case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
        case 'V': case 'W': case 'X': case 'Y': case 'Z':
            break;
        default:
            return false;
    }
    for (unsigned int n = 1; n < N; ++n)
    {
        switch (sAlias[n])
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
                break;
            default:
                return false;
        }
    }
    return true;
}
