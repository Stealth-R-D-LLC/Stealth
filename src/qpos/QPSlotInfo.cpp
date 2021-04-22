// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "QPSlotInfo.hpp"

#include "util.h"

using namespace json_spirit;
using namespace std;

void QPSlotInfo::SetNull()
{
    nTime = 0;
    nSlot = 0;
    nStakerID = 0;
    window.start = 0;
    window.end = 0;
    status = QPSLOT_INVALID;
}

QPSlotInfo::QPSlotInfo()
{
    SetNull();
}

void QPSlotInfo::Set(int64_t nTimeIn, unsigned int nSlotIn, const QPQueue& q)
{
    nTime = nTimeIn;
    nSlot = nSlotIn;
    if (!q.GetIDForSlot(nSlot, nStakerID))
    {
         nStakerID = 0;
         window.start = 0;
         window.end = 0;
         status = QPSLOT_INVALID;
    }
    else if (!q.GetWindowForSlot(nSlot, window))
    {
         window.start = 0;
         window.end = 0;
         status = QPSLOT_INVALID;
    }
    else
    {
         q.GetStatusForSlot(nSlot, status);
    }
}

QPSlotInfo::QPSlotInfo(int64_t nTimeIn, unsigned int nSlotIn, const QPQueue& q)
{
    Set(nTimeIn, nSlotIn, q);
}

const char*  QPSlotInfo::GetStatusType() const
{
    return GetSlotStatusType(status);
}

const char*  QPSlotInfo::GetRelativeStatusType(int64_t nTimeIn,
                                               bool fQueueIsOld) const
{
    if ((status == QPSLOT_CURRENT) || (status == QPSLOT_FUTURE))
    {
        if (fQueueIsOld)
        {
            // An old queue can't have a current slot.
            return "missed";
        }
        else if (nTimeIn > (int64_t)window.end)
        {
            return "late";
        }
        else if (nTimeIn >= (int64_t)window.start)
        {
            return "current";
        }
    }
    return GetSlotStatusType(status);
}

const char*  QPSlotInfo::GetRelativeStatusType(bool fQueueIsOld) const
{
    return GetRelativeStatusType(nTime, fQueueIsOld);
}

int64_t QPSlotInfo::GetTimeUntil(int64_t nTimeIn) const
{
    return window.start - nTimeIn;
}

int64_t QPSlotInfo::GetTimeUntil() const
{
    return window.start - nTime;
}

bool QPSlotInfo::IsValid() const
{
    return status != QPSLOT_INVALID;
}

void QPSlotInfo::AsJSON(int64_t nTimeIn, Object &objRet) const
{
    objRet.clear();
    objRet.push_back(Pair("slot", static_cast<int64_t>(nSlot)));
    objRet.push_back(Pair("id", static_cast<int64_t>(nStakerID)));
    objRet.push_back(Pair("start_time", static_cast<int64_t>(window.start)));
    objRet.push_back(Pair("end_time", static_cast<int64_t>(window.end)));
    string strStatus(GetStatusType());
    objRet.push_back(Pair("status", strStatus));
    string strRelativeStatus(GetRelativeStatusType(nTime));
    objRet.push_back(Pair("relative_status", strRelativeStatus));
    objRet.push_back(Pair("time_until", GetTimeUntil(nTimeIn)));
}

void QPSlotInfo::AsJSON(Object &objRet) const
{
    return AsJSON(nTime, objRet);
}
