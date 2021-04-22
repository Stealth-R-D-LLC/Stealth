// Copyright (c) 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPSLOTINFO_H_
#define _QPSLOTINFO_H_ 1

#include "QPConstants.hpp"
#include "QPQueue.hpp"
#include "QPWindow.hpp"

#include "serialize.h"

#include "json/json_spirit_utils.h"

class QPSlotInfo
{
public:
    static const int QPOS_VERSION = 1;
    static const int CURRENT_VERSION = QPOS_VERSION;

    int64_t nTime;
    unsigned int nSlot;
    unsigned int nStakerID;
    QPWindow window;
    QPSlotStatus status;


    QPSlotInfo();
    QPSlotInfo(int64_t nTimeIn, unsigned int nSlotIn, const QPQueue& q);
    void SetNull();
    void Set(int64_t nTimeIn, unsigned int nSlotIn, const QPQueue& q);

    bool IsValid() const;

    const char*  GetStatusType() const;
    const char*  GetRelativeStatusType(int64_t nTimeIn,
                                       bool fQueueIsOld) const;
    const char*  GetRelativeStatusType(bool fQueueIsOld) const;

    int64_t GetTimeUntil(int64_t nTimeIn) const;
    int64_t GetTimeUntil() const;

    void AsJSON(json_spirit::Object &objRet) const;
    void AsJSON(int64_t nTime, json_spirit::Object &objRet) const;
};

#endif  /* _QPSLOTINFO_H_ */
