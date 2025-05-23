
// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPPOWERELEMENT_H_
#define _QPPOWERELEMENT_H_ 1

#include "serialize.h"

class QPPowerElement
{
public:
    static const int QPOS_VERSION = 1;
    static const int CURRENT_VERSION = QPOS_VERSION;

    // persistent
    unsigned int nVersion;
    unsigned int nStakerID;
    unsigned int nWeight;
    bool fDidProduceBlock;

    QPPowerElement();
    QPPowerElement(unsigned int nStakerIDIn,
                   unsigned int nWeightIn,
                   bool fDidProduceBlockIn);

    void SetNull();

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nSerVersion = this->nVersion;

        READWRITE(nStakerID);
        READWRITE(nWeight);
        READWRITE(fDidProduceBlock);
    )
};

#endif  /* _QPPOWERELEMENT_H_ */
