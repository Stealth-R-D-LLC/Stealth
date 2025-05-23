// Copyright (c) 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPWINDOW_H_
#define _QPWINDOW_H_ 1

#include "serialize.h"

class QPWindow
{
private:
    int nVersion;

public:
    static const int QPOS_VERSION = 1;
    static const int CURRENT_VERSION = QPOS_VERSION;

    unsigned int start;
    unsigned int end;

    QPWindow()
    {
        nVersion = QPWindow::CURRENT_VERSION;
        start = 0;
        end = 0;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nSerVersion = this->nVersion;
        READWRITE(start);
        READWRITE(end);
    )
};


#endif  /* _QPWINDOW_H_ */
