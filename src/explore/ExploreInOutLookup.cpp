// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ExploreConstants.hpp"
#include "ExploreInOutLookup.hpp"


using namespace std;

void ExploreInOutLookup::SetNull()
{
    nVersion = ExploreInOutLookup::CURRENT_VERSION;
    nID = -1;
}

ExploreInOutLookup::ExploreInOutLookup()
{
    SetNull();
}

ExploreInOutLookup::ExploreInOutLookup(int nIDIn)
{
    nVersion = ExploreInOutLookup::CURRENT_VERSION;
    nID = nIDIn;
}

ExploreInOutLookup::ExploreInOutLookup(int nIDIn, bool fIsInputIn)
{
    nVersion = ExploreInOutLookup::CURRENT_VERSION;
    nID = nIDIn;
    if (fIsInputIn)
    {
        nID |= FLAG_ADDR_TX;
    }
}

int ExploreInOutLookup::GetID() const
{
    return nID & MASK_ADDR_TX;
}

int ExploreInOutLookup::Get() const
{
    return nID;
}

bool ExploreInOutLookup::IsInput() const
{
    return (nID & FLAG_ADDR_TX) == FLAG_ADDR_TX;
}

bool ExploreInOutLookup::IsOutput() const
{
    return (nID & FLAG_ADDR_TX) == 0;
}

