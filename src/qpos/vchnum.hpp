// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _VCHNUM_H_
#define _VCHNUM_H_ 1

#include <vector>

typedef std::vector<unsigned char> valtype;

// platform independent storage of <=64 bit unsigned integers
// storage is big endian for compatibility
class vchnum
{
private:
    valtype vch;

    void Set(const valtype &vchIn);
    void SetNum(uint64_t nValue, unsigned int nSize);
public:
    vchnum(const vchnum &vchnumIn);
    vchnum(const valtype &vchIn);
    vchnum(valtype::const_iterator a, valtype::const_iterator b);
    vchnum(uint64_t nValue);
    vchnum(uint32_t nValue);
    vchnum(uint16_t nValue);

    valtype Get() const;
    unsigned int Size() const;
    uint64_t GetValue() const;

    valtype::const_iterator Begin() const;
    valtype::const_iterator End() const;
};

#endif  /* _VCHNUM_H_ */
