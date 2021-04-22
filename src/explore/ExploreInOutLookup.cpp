// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ExploreConstants.hpp"
#include "ExploreInOutLookup.hpp"


using namespace std;

void ExploreInOutLookup::SetNull()
{
    nData = -1;
}

ExploreInOutLookup::ExploreInOutLookup()
{
    SetNull();
}

ExploreInOutLookup::ExploreInOutLookup(int nDataIn)
{
    nData = nDataIn;
}

ExploreInOutLookup::ExploreInOutLookup(int nIndexIn, bool fIsInputIn)
{
    nData = nIndexIn;
    if (fIsInputIn)
    {
        nData |= FLAG_ADDR_TX;
    }
}

int ExploreInOutLookup::GetID() const
{
    return nData & MASK_ADDR_TX;
}

int ExploreInOutLookup::Get() const
{
    return nData;
}

bool ExploreInOutLookup::IsInput() const
{
    return (nData & FLAG_ADDR_TX) == FLAG_ADDR_TX;
}

bool ExploreInOutLookup::IsOutput() const
{
    return (nData & FLAG_ADDR_TX) == 0;
}

