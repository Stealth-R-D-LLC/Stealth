// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "script.h"
#include "vchnum.hpp"

#define U1 static_cast<uint64_t>(1)

using namespace std;

vchnum::vchnum(const vchnum &vchnumIn)
{
    Set(vchnumIn.Get());
}

vchnum::vchnum(const valtype &vchIn)
{
    Set(vchIn);
}

vchnum::vchnum(valtype::const_iterator first, valtype::const_iterator last)
{
    valtype vchIn = valtype(first, last);
    Set(vchIn);
}

void vchnum::Set(const valtype &vchIn)
{
    unsigned int nSizeIn = vchIn.size();
    unsigned int nSize;
    if (nSizeIn > 4)
    {
        nSize = 8;
    }
    else if (nSizeIn >= 2)
    {
        nSize = 4;
    }
    else
    {
        nSize = 2;
    }

    unsigned int delta = nSize - nSizeIn;

    vch.clear();
    vch.resize(nSize, 0);

    for (unsigned int i = 0; i < nSizeIn; ++i)
    {
        vch[i + delta] = vchIn[i];
    }
}

vchnum::vchnum(uint64_t nValue)
{
    SetNum(nValue, 8);
}

vchnum::vchnum(uint32_t nValue)
{
    SetNum(nValue, 4);
}

vchnum::vchnum(uint16_t nValue)
{
    SetNum(nValue, 2);
}

void vchnum::SetNum(uint64_t nValue, unsigned int nSize)
{
    vch.clear();
    vch.resize(nSize);
    unsigned int last = nSize - 1;
    unsigned char ch;
    for (unsigned int i = 0; i < nSize; ++i)
    {
        ch = 0;
        int k = 8 * i;
        for (int j = 0; j < 8; ++j)
        {
            ch = ch | ((unsigned char)((nValue & (U1 << (k + j))) >> k));
        }
        vch[last - i] = ch;
    }
}

valtype vchnum::Get() const
{
    return vch;
}

unsigned int vchnum::Size() const
{
    return vch.size();
}

valtype::const_iterator vchnum::Begin() const
{
    return vch.begin();
}


valtype::const_iterator vchnum::End() const
{
    return vch.end();
}

uint64_t vchnum::GetValue() const
{
    uint64_t nValue = 0;
    unsigned int last = vch.size() - 1;
    unsigned char ch;
    for (unsigned int i = 0; i < vch.size(); ++i)
    {
        ch = vch[last - i];
        int k = 8 * i;
        for (int j = 0; j < 8; ++j)
        {
            nValue += (((uint64_t)(ch & (1 << j))) << k);
        }
    }
    return nValue;
}
