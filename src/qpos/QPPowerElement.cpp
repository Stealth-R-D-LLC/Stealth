// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "QPPowerElement.hpp"

QPPowerElement::QPPowerElement()
{
    SetNull();
}

QPPowerElement::QPPowerElement(unsigned int nStakerIDIn,
                               unsigned int nWeightIn,
                               bool fDidProduceBlockIn)
{
    nVersion = QPPowerElement::CURRENT_VERSION;
    nStakerID = nStakerIDIn;
    nWeight = nWeightIn;
    fDidProduceBlock = fDidProduceBlockIn;
}

void QPPowerElement::SetNull()
{
    nVersion = QPPowerElement::CURRENT_VERSION;
    nStakerID = 0;
    nWeight = 0;
    fDidProduceBlock = false;
}

