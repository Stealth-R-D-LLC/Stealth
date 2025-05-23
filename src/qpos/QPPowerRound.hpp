// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPPOWERROUND_H_
#define _QPPOWERROUND_H_ 1

#include "QPPowerElement.hpp"

#include "serialize.h"

#include <stdint.h>
#include <vector>

class QPPowerRound
{
private:
    // persistent
    int nVersion;
    std::vector<QPPowerElement> vElements;

public:
    static const int QPOS_VERSION = 1;
    static const int CURRENT_VERSION = QPOS_VERSION;

    QPPowerRound();

    void SetNull();
    unsigned int GetWeight() const;
    unsigned int GetTotalWeight() const;
    uint64_t GetPicoPower() const;
    unsigned int Size() const;
    bool IsEmpty() const;

    void Copy(const QPPowerRound &other);
    void PushBack(unsigned int nStakerIDIn,
                  unsigned int nWeightIn,
                  bool fDidProduceBlockIn);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nSerVersion = this->nVersion;

        READWRITE(vElements);
    )
};

#endif  /* _QPPOWERROUND_H_ */
