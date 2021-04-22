// Copyright (c) 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "QPTxDetails.hpp"

#include "script.h"

using namespace std;

void QPTxDetails::Clear()
{
    id = 0;
    alias = "";
    keys.clear();
    pcm = 0;
    value = 0;
    meta_key = "";
    meta_value = "";
}

void QPTxDetails::SetNull()
{
    t = TX_NONSTANDARD;
    hash = 0;
    txid = 0;
    n = -1;
    Clear();
}

QPTxDetails::QPTxDetails()
{
    SetNull();
}

string QPTxDetails::ToString() const
{
    string s1 = strprintf("deet id=%u, alias=%s\n", id, alias.c_str());
    string s2 = "";
    if (keys.size() == 4)
    {
        s2 = strprintf("  owner=%s\n  manager=%s\n  delegate=%s\n  controller=%s\n",
                          HexStr(keys[0].Raw()).c_str(),
                          HexStr(keys[1].Raw()).c_str(),
                          HexStr(keys[2].Raw()).c_str(),
                          HexStr(keys[3].Raw()).c_str());
    }
    else if (keys.size() == 3)
    {
        s2 = strprintf("  owner=%s\n  manager=%s\n  delegate=%s\n  controller=%s\n",
                          HexStr(keys[0].Raw()).c_str(),
                          HexStr(keys[1].Raw()).c_str(),
                          HexStr(keys[1].Raw()).c_str(),
                          HexStr(keys[2].Raw()).c_str());
    }
    else if (keys.size() == 1)
    {
        s2 = strprintf("  owner=%s\n  manager=%s\n  delegate=%s\n  controller=%s\n",
                          HexStr(keys[0].Raw()).c_str(),
                          HexStr(keys[0].Raw()).c_str(),
                          HexStr(keys[0].Raw()).c_str(),
                          HexStr(keys[0].Raw()).c_str());
    }
    string s3 = strprintf("   pcm=%u, value=%" PRId64 ",  meta=%s:%s",
                          pcm, value, meta_key.c_str(), meta_value.c_str());
    return s1 + s2 + s3;
}
    
