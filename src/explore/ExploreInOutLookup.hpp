// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLOREINOUTLOOKUP_H_
#define _EXPLOREINOUTLOOKUP_H_ 1

#include "serialize.h"


class ExploreInOutLookup
{
private:
    int nData;

public:
    void SetNull();

    ExploreInOutLookup();

    ExploreInOutLookup(int nDataIn);

    ExploreInOutLookup(int nIDIn, bool fIsInputIn);

    int GetID() const;

    int Get() const;

    bool IsInput() const;

    bool IsOutput() const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nData);
    )

};

#endif  /* _EXPLOREINOUTLOOKUP_H_ */
