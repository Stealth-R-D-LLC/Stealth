// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLOREINOUTLIST_H_
#define _EXPLOREINOUTLIST_H_ 1

#include "serialize.h"

class ExploreInOutList
{
public:
    typedef std::vector<int> storedlist_t;

    int height;
    int vtx;
    ExploreInOutList::storedlist_t vinouts;

    void SetNull();
    ExploreInOutList();

    void Clear(const int heightIn, const int vtxIn);
    ExploreInOutList(const int heightIn, const int vtxIn);

    bool operator < (const ExploreInOutList& other) const;
    bool operator > (const ExploreInOutList& other) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(height);
        READWRITE(vtx);
        READWRITE(vinouts);
    )
};


#endif  /* _EXPLOREINOUTLIST_H_ */
