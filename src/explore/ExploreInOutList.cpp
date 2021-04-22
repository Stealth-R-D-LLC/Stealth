// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ExploreInOutList.hpp"


using namespace std;


void ExploreInOutList::SetNull()
{
    height = -1;
    vtx = -1;
    vinouts.clear();
}

ExploreInOutList::ExploreInOutList()
{
    SetNull();
}


void ExploreInOutList::Clear(const int heightIn, const int vtxIn)
{
    height = heightIn;
    vtx = vtxIn;
    vinouts.clear();
}

ExploreInOutList::ExploreInOutList(const int heightIn, const int vtxIn)
{
    Clear(heightIn, vtxIn);
}

bool ExploreInOutList::operator < (const ExploreInOutList& other) const
{
    if (height != other.height)
    {
        return (height < other.height);
    }
    return (vtx < other.vtx);
}

bool ExploreInOutList::operator > (const ExploreInOutList& other) const
{
    if (height != other.height)
    {
        return (height > other.height);
    }
    return (vtx > other.vtx);
}

