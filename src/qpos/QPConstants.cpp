// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "QPConstants.hpp"

unsigned int GetStakerMaxMisses(int nHeight)
{
    if (fTestNet)
    {
       return (GetFork(nHeight) < XST_FORKMISSFIX) ? QP_STAKER_MAX_MISSES_1_T :
                                                     QP_STAKER_MAX_MISSES_2_T;
    }
    return QP_STAKER_MAX_MISSES_M;
}
