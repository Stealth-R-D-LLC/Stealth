// Copyright (c) 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "QPTxDetails.hpp"

#include "script.h"

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
