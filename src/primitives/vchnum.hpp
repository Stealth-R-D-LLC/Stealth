// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _VCHNUM_H_
#define _VCHNUM_H_ 1

#include "valtype.hpp"

#include <cstdint>
#include <cstddef>


#define GETUINT16(first, last) \
   static_cast<uint16_t>(vchnum(first, last).GetValue())
#define GETUINT(first, last) \
   static_cast<unsigned int>(vchnum(first, last).GetValue())
#define GETINT(first, last) \
   static_cast<int>(vchnum(first, last).GetValue())
#define GETUINT32(first, last) \
   static_cast<uint32_t>(vchnum(first, last).GetValue())
#define GETINT32(first, last) \
   static_cast<int32_t>(vchnum(first, last).GetValue())
#define GETUINT64(first, last) vchnum(first, last).GetValue()
#define GETINT64(first, last) \
   static_cast<int64_t>(vchnum(first, last).GetValue())


// platform independent storage of <= 64 bit unsigned integers
// storage is big endian for compatibility
class vchnum
{
private:
    valtype vch;

    void Set(const valtype *pvchIn);
    void Set(const valtype &vchIn);
    void Set(const void* ptr, size_t num);
    void SetNum(uint64_t nValue, unsigned int nSize);
public:
    vchnum(const vchnum &vchnumIn);
    vchnum(const valtype &vchIn);
    vchnum(valtype::const_iterator a, valtype::const_iterator b);
    vchnum(const void* ptr, size_t num);
    vchnum(int nSize);
    vchnum(size_t nSize, uint64_t nValue);
    vchnum(uint64_t nValue);
    vchnum(uint32_t nValue);
    vchnum(uint16_t nValue);
    vchnum& operator = (uint64_t nValue);
    vchnum& operator = (uint32_t nValue);
    vchnum& operator = (uint16_t nValue);
    const valtype* Get() const;
    void Get(valtype& vchRet) const;
    void Get(const unsigned char*& pchRet) const;
    bool Get(void* ptr, size_t num) const;
    unsigned int Size() const;
    uint64_t GetValue() const;

    valtype::const_iterator Begin() const;
    valtype::const_iterator End() const;

    unsigned char* BeginData();
};

#endif  /* _VCHNUM_H_ */
