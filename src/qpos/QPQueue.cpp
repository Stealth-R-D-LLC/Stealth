// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "QPQueue.hpp"
#include "util.h"

#include <sstream>

using namespace std;

QPQueue::QPQueue()
{
    SetNull();
}


QPQueue::QPQueue(unsigned int nSlotTime0In,
        const vector<unsigned int>& vStakerIDsIn)
{
    nVersion = QPQueue::CURRENT_VERSION;
    nCurrentSlot = 0;
    nSlotTime0 = nSlotTime0In;
    vStakerIDs = vStakerIDsIn;
}

void QPQueue::SetNull()
{
    nVersion = QPQueue::CURRENT_VERSION;
    nCurrentSlot = 0;
    nSlotTime0 = 0;
    vStakerIDs.clear();
}

bool QPQueue::GetIDForSlot(unsigned int slot, unsigned int &nIDRet) const
{
    if (slot >= vStakerIDs.size())
    {
         return false;
    }
    nIDRet = vStakerIDs[slot];
    return true;
}
bool QPQueue::GetSlotForID(unsigned int nID, unsigned int& nSlotRet) const
{
    bool found = false;
    unsigned int slot = 0;
    QPQueueIterator it;
    for (it = vStakerIDs.begin(); it != vStakerIDs.end(); ++it)
    {
        if (*it == nID)
        {
            nSlotRet = slot;
            found = true;
            break;
        }
        ++slot;
    }
    return found;
}

bool QPQueue::GetWindowForID(unsigned int nID, QPWindow& windowRet) const
{
    unsigned int slot;
    if (!GetSlotForID(nID, slot))
    {
        return false;
    }
    unsigned int start = nSlotTime0 + (QP_TARGET_TIME * slot);
        windowRet.start = start;
        windowRet.end = start + QP_TARGET_TIME - 1;
    return true;
}

bool QPQueue::GetWindowForSlot(unsigned int nSlot, QPWindow& windowRet) const
{
    if (nSlot >= vStakerIDs.size())
    {
        return false;
    }
    unsigned int start = nSlotTime0 + (QP_TARGET_TIME * nSlot);
    windowRet.start = start;
        windowRet.end = start + QP_TARGET_TIME - 1;
    return true;
}

unsigned int QPQueue::GetCurrentSlot() const
{
    return nCurrentSlot;
}

unsigned int QPQueue::GetMinTime() const
{
    return nSlotTime0;
}

unsigned int QPQueue::GetMaxTime() const
{
    unsigned int nSize = static_cast<unsigned int>(vStakerIDs.size());
    return nSlotTime0 + (QP_TARGET_TIME * nSize) - 1;
}


bool QPQueue::GetSlotStartTime(unsigned int nSlot, unsigned int &nTimeRet) const
{
    if (nSlot >= vStakerIDs.size())
    {
        return false;
    }
    nTimeRet = nSlotTime0 + (QP_TARGET_TIME * (unsigned int)nSlot);
    return true;
}

bool QPQueue::GetSlotForTime(unsigned int nTime, unsigned int &nSlotRet) const
{
    if (nTime < nSlotTime0)
    {
        return error("GetSlotForTime(): time %u too early", nTime);
    }
    if (nTime > GetMaxTime())
    {
        return error("GetSlotForTime(): time %u too late", nTime);
    }
    nSlotRet = (nTime - nSlotTime0) / QP_TARGET_TIME;
    return true;
}


unsigned int QPQueue::GetCurrentSlotStart() const
{
    return nSlotTime0 + (QP_TARGET_TIME * nCurrentSlot);
}

unsigned int QPQueue::GetCurrentSlotEnd() const
{
    unsigned int start = nSlotTime0 + (QP_TARGET_TIME * nCurrentSlot);
    return start + QP_TARGET_TIME - 1;
}

QPWindow QPQueue::GetCurrentSlotWindow() const
{
    unsigned int start = nSlotTime0 + (QP_TARGET_TIME * nCurrentSlot);
    QPWindow w;
    w.start = start;
    w.end = start + QP_TARGET_TIME - 1;
    return w;
}

bool QPQueue::TimeIsInCurrentSlotWindow(unsigned int nTime) const
{
    QPWindow w = GetCurrentSlotWindow();
    return ((w.start <= nTime) && (nTime <= w.end));
}


unsigned int QPQueue::GetCurrentID() const
{
    return vStakerIDs[nCurrentSlot];
}

unsigned int QPQueue::GetLastID() const
{
    return vStakerIDs.back();
}

bool QPQueue::IsOnLastSlot() const
{
    return (nCurrentSlot == (vStakerIDs.size() - 1));
}

QPQueueIterator QPQueue::Begin() const
{
    return vStakerIDs.begin();
}

QPQueueIterator QPQueue::End() const
{
    return vStakerIDs.end();
}

unsigned int QPQueue::Size() const
{
    return vStakerIDs.size();
}

bool QPQueue::IsEmpty() const
{
    return vStakerIDs.empty();
}

string QPQueue::ToString() const
{
    int n = vStakerIDs.size();
    int last = n - 1;
    ostringstream ss;
    for (int i = 0; i < n; ++i)
    {
        ss << i << ":" << vStakerIDs[i];
        if (i < last)
        {
            ss << ",";
        }
    }
    string sSlots = ss.str();
    return strprintf("QPQueue: start=%u, current_slot=%u,\n     slots=%s",
                     nSlotTime0, nCurrentSlot, sSlots.c_str());
}

bool QPQueue::IncrementSlot()
{
    if (nCurrentSlot + 1 >= vStakerIDs.size())
    {
        return false;
    }
    nCurrentSlot += 1;
    return true;
}

void QPQueue::Reset()
{
    nCurrentSlot = 0;
}
