// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPQUEUE_H_
#define _QPQUEUE_H_ 1

#include "QPStaker.hpp"

#include <utility>

#define QPQueueIterator std::vector<unsigned int>::const_iterator

class QPWindow
{
public:
    unsigned int start;
    unsigned int end;
    QPWindow()
    {
        start = 0;
        end = 0;
    }
};


class QPQueue
{
private:
    int nVersion;
    unsigned int nCurrentSlot;
    unsigned int nSlotTime0;
    std::vector<unsigned int> vStakerIDs;
public:
    static const int QPOS_VERSION = 1;
    static const int CURRENT_VERSION = QPOS_VERSION;

    QPQueue();
    QPQueue(unsigned int nSlotTime0In,
            const std::vector<unsigned int>& vStakerIDsIn);
    void SetNull();
    bool GetIDForSlot(unsigned int slot, unsigned int &nIDRet) const;
    bool GetSlotForID(unsigned int nID, unsigned int &nSlotRet) const;
    bool GetWindowForID(unsigned int nID, QPWindow &pairWindow) const;
    bool GetWindowForSlot(unsigned int nSlot, QPWindow &pairWindow) const;
    unsigned int GetCurrentSlot() const;
    unsigned int GetMinTime() const;
    unsigned int GetMaxTime() const;
    bool GetSlotStartTime(unsigned int nSlot, unsigned int &nTimeRet) const;
    bool GetSlotForTime(unsigned int nTime, unsigned int &nSlotRet) const;
    unsigned int GetCurrentSlotStart() const;
    unsigned int GetCurrentSlotEnd() const;
    QPWindow GetCurrentSlotWindow() const;
    bool TimeIsInCurrentSlotWindow(unsigned int nTime) const;
    unsigned int GetCurrentID() const;
    unsigned int GetLastID() const;
    bool IsOnLastSlot() const;
    QPQueueIterator Begin() const;
    QPQueueIterator End() const;
    unsigned int Size() const;
    bool IsEmpty() const;
    bool IncrementSlot();
    void Reset();

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nCurrentSlot);
        READWRITE(nSlotTime0);
        READWRITE(vStakerIDs);
    )
};

#endif  /* _QPQUEUE_H_ */
