// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPQUEUE_H_
#define _QPQUEUE_H_ 1

#include "QPStaker.hpp"
#include "QPWindow.hpp"

#include "valtype.hpp"

#include <utility>


#define QPQueueIterator std::vector<unsigned int>::const_iterator


unsigned int BitIsOn(const valtype& vch, unsigned int nBit);
unsigned int SetBit(valtype& vch, unsigned int nBit);
const char* GetSlotStatusType(const QPSlotStatus status);
const char* GetSlotStatusAbbrev(const QPSlotStatus status);


class QPQueue
{
private:
    int nVersion;
    unsigned int nCurrentSlot;
    unsigned int nSlotTime0;
    std::vector<unsigned int> vStakerIDs;
    valtype vchBlockStats;
public:
    static const int QPOS_VERSION = 1;
    static const int CURRENT_VERSION = QPOS_VERSION;

    QPQueue();
    QPQueue(unsigned int nSlotTime0In,
            const std::vector<unsigned int>& vStakerIDsIn);
    void SetNull();
    unsigned int GetCurrentSlot() const;
    const std::vector<unsigned int>& GetStakerIDs() const;
    const valtype& GetBlockStats() const;
    bool GetIDForSlot(unsigned int slot, unsigned int &nIDRet) const;
    bool GetSlotForID(unsigned int nID, unsigned int &nSlotRet) const;
    bool GetWindowForID(unsigned int nID, QPWindow &pairWindow) const;
    bool GetWindowForSlot(unsigned int nSlot, QPWindow &pairWindow) const;
    bool GetStatusForSlot(unsigned int nSlot, QPSlotStatus &status) const;
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
    std::string ToString() const;
    bool IsEmpty() const;
    unsigned int SlotProduced(unsigned int nSlot);
    bool IncrementSlot();
    void Reset();
    void SummaryAsJSON(int nHeight, json_spirit::Object &objRet) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nSerVersion = this->nVersion;
        READWRITE(nCurrentSlot);
        READWRITE(nSlotTime0);
        READWRITE(vStakerIDs);
        READWRITE(vchBlockStats);
    )
};

#endif  /* _QPQUEUE_H_ */
