// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "QPQueue.hpp"
#include "util.h"

#include <sstream>

using namespace std;


unsigned int BitIsOn(const valtype& vch, unsigned int nBit)
{
    unsigned int nByte = nBit / 8;
    unsigned int nSize = vch.size();
    if (nByte >= nSize)
    {
        return 0;
    }
    return vch[nByte] & (0x80 >> (nBit % 8));
}

unsigned int SetBit(valtype& vch, unsigned int nBit)
{
     unsigned int nByte = nBit / 8;
     unsigned int nSize = vch.size();
     if (nByte >= nSize)
     {
         return 0;
     }
     unsigned char& chByte = vch[nByte];
     unsigned int nBitValue = 0x80 >> (nBit % 8);
     chByte |= nBitValue;
     return nBitValue;
}


const char* GetSlotStatusType(const QPSlotStatus status)
{
    switch (status)
    {
    case QPSLOT_INVALID:
        return "invalid";
    case QPSLOT_MISSED:
        return "missed";
    case QPSLOT_HIT:
        return "produced";
    case QPSLOT_CURRENT:
        return "current";
    case QPSLOT_FUTURE:
        return "future";
    }
}

const char* GetSlotStatusAbbrev(const QPSlotStatus status)
{
    switch (status)
    {
    case QPSLOT_INVALID:
        return "!";
    case QPSLOT_MISSED:
        return "x";
    case QPSLOT_HIT:
        return "+";
    case QPSLOT_CURRENT:
        return "-";
    case QPSLOT_FUTURE:
        return "o";
    }
}

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
    unsigned int nSize = vStakerIDs.size();
    if (nSize % 8 == 0)
    {
        vchBlockStats.resize(nSize / 8);
    }
    else
    {
        vchBlockStats.resize((nSize / 8) + 1);
    }
    fill(vchBlockStats.begin(), vchBlockStats.end(), 0);
}

void QPQueue::SetNull()
{
    nVersion = QPQueue::CURRENT_VERSION;
    nCurrentSlot = 0;
    nSlotTime0 = 0;
    vStakerIDs.clear();
    vchBlockStats.clear();
}

unsigned int QPQueue::GetCurrentSlot() const
{
    return nCurrentSlot;
}

const std::vector<unsigned int>& QPQueue::GetStakerIDs() const
{
    return vStakerIDs;
}

const valtype& QPQueue::GetBlockStats() const
{
    return vchBlockStats;
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

bool QPQueue::GetStatusForSlot(unsigned int nSlot, QPSlotStatus& status) const
{
    if (nSlot >= vStakerIDs.size())
    {
        status = QPSLOT_INVALID;
        return false;
    }
    if (nSlot > nCurrentSlot)
    {
        status = QPSLOT_FUTURE;
    }
    else
    {
        unsigned int nBit = nSlot +
                            (vchBlockStats.size() * 8) -
                            vStakerIDs.size();
        if (BitIsOn(vchBlockStats, nBit))
        {
            status = QPSLOT_HIT;
        }
        else if (nSlot == nCurrentSlot)
        {
            status = QPSLOT_CURRENT;
        }
        else
        {
            status = QPSLOT_MISSED;
        }
    }
    return true;
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
        QPSlotStatus status;
        if (!GetStatusForSlot(i, status))
        {
            // this should never happen: practically impossible
            printf("QPQueue::ToString() TSNH No such slot\n");
        }
        const char* chStat = GetSlotStatusAbbrev(status);
        ss << chStat << i << ":" << vStakerIDs[i];
        if (i < last)
        {
            ss << ",";
        }
    }
    string sSlots = ss.str();
    return strprintf("QPQueue: start=%u, current_slot=%u,\n     slots=%s",
                     nSlotTime0, nCurrentSlot, sSlots.c_str());
}

unsigned int QPQueue::SlotProduced(unsigned int nSlot)
{
    unsigned int nBit = nSlot + (vchBlockStats.size() * 8) - vStakerIDs.size();
    return SetBit(vchBlockStats, nBit);
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
