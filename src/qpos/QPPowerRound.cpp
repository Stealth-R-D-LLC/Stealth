// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "QPConstants.hpp"
#include "QPPowerRound.hpp"

using namespace std;


QPPowerRound::QPPowerRound()
{
    SetNull();
}

void QPPowerRound::SetNull()
{
    nVersion = QPPowerRound::CURRENT_VERSION;
    vElements.clear();
}

unsigned int QPPowerRound::GetWeight() const
{
    unsigned int nWeight = 0;
    vector<QPPowerElement>::const_iterator it;
    for (it = vElements.begin(); it != vElements.end(); ++it)
    {
        if (it->fDidProduceBlock)
        {
            nWeight += it->nWeight;
        }
    }
    return nWeight;
}

unsigned int QPPowerRound::GetTotalWeight() const
{
    unsigned int nWeight = 0;
    vector<QPPowerElement>::const_iterator it;
    for (it = vElements.begin(); it != vElements.end(); ++it)
    {
        nWeight += it->nWeight;
    }
    return nWeight;
}

uint64_t QPPowerRound::GetPicoPower() const
{
    uint64_t nWeight = static_cast<uint64_t>(GetWeight());
    uint64_t nTotalWeight = static_cast<uint64_t>(GetTotalWeight());
    if (nTotalWeight == 0)
    {
        return 0;
    }
    return (nWeight * TRIL) / nTotalWeight;
}

unsigned int QPPowerRound::Size() const
{
    return vElements.size();
}

bool QPPowerRound::IsEmpty() const
{
    return vElements.empty();
}

void QPPowerRound::Copy(const QPPowerRound &other)
{
    vElements = other.vElements;
}

void QPPowerRound::PushBack(unsigned int nStakerIDIn,
                            unsigned int nWeightIn,
                            bool fDidProduceBlockIn)
{
    QPPowerElement element(nStakerIDIn, nWeightIn, fDidProduceBlockIn);
    vElements.push_back(element);
}
