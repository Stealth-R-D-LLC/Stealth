// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_SERIALIZE_H
#define BITCOIN_SERIALIZE_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <bitset>
#include <cassert>
#include <limits>
#include <cstring>
#include <cstdio>
#include <cstdint>

#include <boost/type_traits/is_fundamental.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/tuple/tuple_io.hpp>

#include "allocators.h"
#include "version.h"


#if __cplusplus >= 201103L
    #define AUTO_PTR std::unique_ptr
#else
    #define AUTO_PTR std::auto_ptr
#endif

// for berkeley-db
typedef uint32_t u_int32_t;
typedef uint16_t u_int16_t;
typedef uint8_t u_int8_t;


class CScript;
class CDataStream;
class CAutoFile;
static const unsigned int MAX_SIZE = 0x02000000;



// Used to bypass the rule against non-const reference to temporary
// where it makes sense with wrappers such as CFlatData or CTxDB
template<typename T>
inline T& REF(const T& val)
{
    return const_cast<T&>(val);
}


template<typename T>
std::string HexStr(const T itbegin, const T itend, bool fSpaces=false)
{
    std::string rv;
    static const char hexmap[16] = {
                         '0', '1', '2', '3', '4', '5', '6', '7',
                         '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    rv.reserve((itend-itbegin)*3);
    for(T it = itbegin; it < itend; ++it)
    {
        unsigned char val = (unsigned char)(*it);
        if (fSpaces && it != itbegin)
        {
            rv.push_back(' ');
        }
        rv.push_back(hexmap[val>>4]);
        rv.push_back(hexmap[val&15]);
    }
    return rv;
}

inline std::string HexStr(const std::vector<unsigned char>& vch,
                          bool fSpaces = false)
{
    return HexStr(vch.begin(), vch.end(), fSpaces);
}

inline std::string ChunkHex(const std::string& strHex,
                            size_t nChunk = 8,
                            const std::string& strIndent = "",
                            bool add0xPrefix = false)
{
    std::vector<std::string> vBytes;
    std::istringstream iss(strHex);
    std::string strByte;

    while (iss >> strByte)
    {
        if (add0xPrefix && strByte.substr(0, 2) != "0x")
        {
            vBytes.push_back("0x" + strByte);
        }
        else
        {
            vBytes.push_back(strByte);
        }
    }

    std::ostringstream ossResult;
    for (size_t i = 0; i < vBytes.size(); ++i)
    {
        if (i % nChunk == 0)
        {
            if (i > 0)
            {
                ossResult << '\n';
            }
            ossResult << strIndent;
        }
        ossResult << vBytes[i] << ' ';
    }

    return ossResult.str();
}


/////////////////////////////////////////////////////////////////
//
// Templates for serializing to anything that looks like a stream,
// i.e. anything that supports .read(char*, int) and .write(char*, int)
//

enum
{
    // primary actions
    SER_NETWORK         = (1 << 0),
    SER_DISK            = (1 << 1),
    SER_GETHASH         = (1 << 2),

    // modifiers
    SER_SKIPSIG         = (1 << 16),
    SER_BLOCKHEADERONLY = (1 << 17),
};

#define IMPLEMENT_SERIALIZE(statements)    \
    unsigned int GetSerializeSize(int nType, int nSerVersion) const  \
    {                                           \
        CSerActionGetSerializeSize ser_action;  \
        const bool fGetSize = true;             \
        const bool fWrite = false;              \
        const bool fRead = false;               \
        unsigned int nSerSize = 0;              \
        ser_streamplaceholder s;                \
        (void)(fGetSize && fWrite && fRead); /* suppress warning */ \
        s.nType = nType;                        \
        s.nVersion = nSerVersion;               \
        {statements}                            \
        return nSerSize;                        \
    }                                           \
    template<typename Stream>                   \
    void Serialize(Stream& s, int nType, int nSerVersion) const  \
    {                                           \
        CSerActionSerialize ser_action;         \
        const bool fGetSize = false;            \
        const bool fWrite = true;               \
        const bool fRead = false;               \
        unsigned int nSerSize = 0;              \
        (void)(fGetSize && fWrite && fRead); /* suppress warning */ \
        {statements}                            \
    }                                           \
    template<typename Stream>                   \
    void Unserialize(Stream& s, int nType, int nSerVersion)  \
    {                                           \
        CSerActionUnserialize ser_action;       \
        const bool fGetSize = false;            \
        const bool fWrite = false;              \
        const bool fRead = true;                \
        unsigned int nSerSize = 0;              \
        (void)(fGetSize && fWrite && fRead); /* suppress warning */ \
        {statements}                            \
    }

#define READWRITE(obj)      (nSerSize += ::SerReadWrite(s, (obj), nType, nSerVersion, ser_action))






//
// Basic types
//
#define WRITEDATA(s, obj)   s.write((char*)&(obj), sizeof(obj))
#define READDATA(s, obj)    s.read((char*)&(obj), sizeof(obj))

inline unsigned int GetSerializeSize(char a,           int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(signed char a,    int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(unsigned char a,  int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(signed short a,   int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(unsigned short a, int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(signed int a,     int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(unsigned int a,   int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(signed long a,    int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(unsigned long a,  int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(signed long long a,  int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(unsigned long long a,  int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(float a,          int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(double a,         int, int=0) { return sizeof(a); }

template<typename Stream> inline void Serialize(Stream& s, char a,           int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, signed char a,    int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, unsigned char a,  int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, signed short a,   int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, unsigned short a, int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, signed int a,     int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, unsigned int a,   int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, signed long a,    int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, unsigned long a,  int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, signed long long a,  int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, unsigned long long a,  int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, float a,          int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, double a,         int, int=0) { WRITEDATA(s, a); }

template<typename Stream> inline void Unserialize(Stream& s, char& a,           int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, signed char& a,    int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, unsigned char& a,  int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, signed short& a,   int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, unsigned short& a, int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, signed int& a,     int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, unsigned int& a,   int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, signed long& a,    int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, unsigned long& a,  int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, signed long long& a,  int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, unsigned long long& a,  int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, float& a,          int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, double& a,         int, int=0) { READDATA(s, a); }

inline unsigned int GetSerializeSize(bool a, int, int=0)                          { return sizeof(char); }
template<typename Stream> inline void Serialize(Stream& s, bool a, int, int=0)    { char f=a; WRITEDATA(s, f); }
template<typename Stream> inline void Unserialize(Stream& s, bool& a, int, int=0) { char f; READDATA(s, f); a=f; }


#ifndef THROW_WITH_STACKTRACE
#define THROW_WITH_STACKTRACE(exception)  \
{                                         \
    LogStackTrace();                      \
    throw (exception);                    \
}
void LogStackTrace();
#endif

//
// Compact size
//  size <  253        -- 1 byte
//  size <= USHRT_MAX  -- 3 bytes  (253 + 2 bytes)
//  size <= UINT_MAX   -- 5 bytes  (254 + 4 bytes)
//  size >  UINT_MAX   -- 9 bytes  (255 + 8 bytes)
//
inline unsigned int GetSizeOfCompactSize(uint64_t nSize)
{
    if (nSize < 253)             return sizeof(unsigned char);
    else if (nSize <= std::numeric_limits<unsigned short>::max()) return sizeof(unsigned char) + sizeof(unsigned short);
    else if (nSize <= std::numeric_limits<unsigned int>::max())  return sizeof(unsigned char) + sizeof(unsigned int);
    else                         return sizeof(unsigned char) + sizeof(uint64_t);
}

template<typename Stream>
void WriteCompactSize(Stream& os, uint64_t nSize)
{
    if (nSize < 253)
    {
        unsigned char chSize = nSize;
        WRITEDATA(os, chSize);
    }
    else if (nSize <= std::numeric_limits<unsigned short>::max())
    {
        unsigned char chSize = 253;
        unsigned short xSize = nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    }
    else if (nSize <= std::numeric_limits<unsigned int>::max())
    {
        unsigned char chSize = 254;
        unsigned int xSize = nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    }
    else
    {
        unsigned char chSize = 255;
        uint64_t xSize = nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    }
    return;
}

template<typename Stream>
uint64_t ReadCompactSize(Stream& is)
{
    unsigned char chSize;
    READDATA(is, chSize);
    uint64_t nSizeRet = 0;
    if (chSize < 253)
    {
        nSizeRet = chSize;
    }
    else if (chSize == 253)
    {
        unsigned short xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
    }
    else if (chSize == 254)
    {
        unsigned int xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
    }
    else
    {
        uint64_t xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
    }
    if (nSizeRet > (uint64_t)MAX_SIZE)
        THROW_WITH_STACKTRACE(std::ios_base::failure("ReadCompactSize() : size too large"));
    return nSizeRet;
}



#define FLATDATA(obj)   REF(CFlatData((char*)&(obj), (char*)&(obj) + sizeof(obj)))

/** Wrapper for serializing arrays and POD.
 */
class CFlatData
{
protected:
    char* pbegin;
    char* pend;
public:
    CFlatData(void* pbeginIn, void* pendIn) : pbegin((char*)pbeginIn), pend((char*)pendIn) { }
    char* begin() { return pbegin; }
    const char* begin() const { return pbegin; }
    char* end() { return pend; }
    const char* end() const { return pend; }

    unsigned int GetSerializeSize(int, int=0) const
    {
        return pend - pbegin;
    }

    template<typename Stream>
    void Serialize(Stream& s, int, int=0) const
    {
        s.write(pbegin, pend - pbegin);
    }

    template<typename Stream>
    void Unserialize(Stream& s, int, int=0)
    {
        s.read(pbegin, pend - pbegin);
    }
};

//
// Forward declarations
//

// string
template<typename C> unsigned int GetSerializeSize(const std::basic_string<C>& str, int, int=0);
template<typename Stream, typename C> void Serialize(Stream& os, const std::basic_string<C>& str, int, int=0);
template<typename Stream, typename C> void Unserialize(Stream& is, std::basic_string<C>& str, int, int=0);

// vector
template<typename T, typename A> unsigned int GetSerializeSize_impl(const std::vector<T, A>& v, int nType, int nSerVersion, const boost::true_type&);
template<typename T, typename A> unsigned int GetSerializeSize_impl(const std::vector<T, A>& v, int nType, int nSerVersion, const boost::false_type&);
template<typename T, typename A> inline unsigned int GetSerializeSize(const std::vector<T, A>& v, int nType, int nSerVersion);
template<typename Stream, typename T, typename A> void Serialize_impl(Stream& os, const std::vector<T, A>& v, int nType, int nSerVersion, const boost::true_type&);
template<typename Stream, typename T, typename A> void Serialize_impl(Stream& os, const std::vector<T, A>& v, int nType, int nSerVersion, const boost::false_type&);
template<typename Stream, typename T, typename A> inline void Serialize(Stream& os, const std::vector<T, A>& v, int nType, int nSerVersion);
template<typename Stream, typename T, typename A> void Unserialize_impl(Stream& is, std::vector<T, A>& v, int nType, int nSerVersion, const boost::true_type&);
template<typename Stream, typename T, typename A> void Unserialize_impl(Stream& is, std::vector<T, A>& v, int nType, int nSerVersion, const boost::false_type&);
template<typename Stream, typename T, typename A> inline void Unserialize(Stream& is, std::vector<T, A>& v, int nType, int nSerVersion);

// others derived from vector
extern inline unsigned int GetSerializeSize(const CScript& v, int nType, int nSerVersion);
template<typename Stream> void Serialize(Stream& os, const CScript& v, int nType, int nSerVersion);
template<typename Stream> void Unserialize(Stream& is, CScript& v, int nType, int nSerVersion);

// pair
template<typename K, typename T> unsigned int GetSerializeSize(const std::pair<K, T>& item, int nType, int nSerVersion);
template<typename Stream, typename K, typename T> void Serialize(Stream& os, const std::pair<K, T>& item, int nType, int nSerVersion);
template<typename Stream, typename K, typename T> void Unserialize(Stream& is, std::pair<K, T>& item, int nType, int nSerVersion);

// 3 tuple
template<typename T0, typename T1, typename T2> unsigned int GetSerializeSize(const boost::tuple<T0, T1, T2>& item, int nType, int nSerVersion);
template<typename Stream, typename T0, typename T1, typename T2> void Serialize(Stream& os, const boost::tuple<T0, T1, T2>& item, int nType, int nSerVersion);
template<typename Stream, typename T0, typename T1, typename T2> void Unserialize(Stream& is, boost::tuple<T0, T1, T2>& item, int nType, int nSerVersion);

// 4 tuple
template<typename T0, typename T1, typename T2, typename T3> unsigned int GetSerializeSize(const boost::tuple<T0, T1, T2, T3>& item, int nType, int nSerVersion);
template<typename Stream, typename T0, typename T1, typename T2, typename T3> void Serialize(Stream& os, const boost::tuple<T0, T1, T2, T3>& item, int nType, int nSerVersion);
template<typename Stream, typename T0, typename T1, typename T2, typename T3> void Unserialize(Stream& is, boost::tuple<T0, T1, T2, T3>& item, int nType, int nSerVersion);

// map
template<typename K, typename T, typename Pred, typename A> unsigned int GetSerializeSize(const std::map<K, T, Pred, A>& m, int nType, int nSerVersion);
template<typename Stream, typename K, typename T, typename Pred, typename A> void Serialize(Stream& os, const std::map<K, T, Pred, A>& m, int nType, int nSerVersion);
template<typename Stream, typename K, typename T, typename Pred, typename A> void Unserialize(Stream& is, std::map<K, T, Pred, A>& m, int nType, int nSerVersion);

// set
template<typename K, typename Pred, typename A> unsigned int GetSerializeSize(const std::set<K, Pred, A>& m, int nType, int nSerVersion);
template<typename Stream, typename K, typename Pred, typename A> void Serialize(Stream& os, const std::set<K, Pred, A>& m, int nType, int nSerVersion);
template<typename Stream, typename K, typename Pred, typename A> void Unserialize(Stream& is, std::set<K, Pred, A>& m, int nType, int nSerVersion);

// bitset
template<size_t N>
  unsigned int GetSerializeSize(const std::bitset<N>& m, int nType, int nSerVersion);
template<typename Stream, size_t N>
  void Serialize(Stream& os, const std::bitset<N>& m, int nType, int nSerVersion);
template<typename Stream, size_t N>
  void Unserialize(Stream& is, std::bitset<N>& m, int nType, int nSerVersion);


//
// If none of the specialized versions above matched, default to calling member function.
// "int nType" is changed to "long nType" to keep from getting an ambiguous overload error.
// The compiler will only cast int to long if none of the other templates matched.
// Thanks to Boost serialization for this idea.
//
template<typename T>
inline unsigned int GetSerializeSize(const T& a, long nType, int nSerVersion)
{
    return a.GetSerializeSize((int)nType, nSerVersion);
}

template<typename Stream, typename T>
inline void Serialize(Stream& os, const T& a, long nType, int nSerVersion)
{
    a.Serialize(os, (int)nType, nSerVersion);
}

template<typename Stream, typename T>
inline void Unserialize(Stream& is, T& a, long nType, int nSerVersion)
{
    a.Unserialize(is, (int)nType, nSerVersion);
}





//
// string
//
template<typename C>
unsigned int GetSerializeSize(const std::basic_string<C>& str, int, int)
{
    return GetSizeOfCompactSize(str.size()) + str.size() * sizeof(str[0]);
}

template<typename Stream, typename C>
void Serialize(Stream& os, const std::basic_string<C>& str, int, int)
{
    WriteCompactSize(os, str.size());
    if (!str.empty())
        os.write((char*)&str[0], str.size() * sizeof(str[0]));
}

template<typename Stream, typename C>
void Unserialize(Stream& is, std::basic_string<C>& str, int, int)
{
    unsigned int nSize = ReadCompactSize(is);
    str.resize(nSize);
    if (nSize != 0)
        is.read((char*)&str[0], nSize * sizeof(str[0]));
}



//
// vector
//
template<typename T, typename A>
unsigned int GetSerializeSize_impl(const std::vector<T, A>& v, int nType, int nSerVersion, const boost::true_type&)
{
    return (GetSizeOfCompactSize(v.size()) + v.size() * sizeof(T));
}

template<typename T, typename A>
unsigned int GetSerializeSize_impl(const std::vector<T, A>& v, int nType, int nSerVersion, const boost::false_type&)
{
    unsigned int nSize = GetSizeOfCompactSize(v.size());
    for (typename std::vector<T, A>::const_iterator vi = v.begin(); vi != v.end(); ++vi)
        nSize += GetSerializeSize((*vi), nType, nSerVersion);
    return nSize;
}

template<typename T, typename A>
inline unsigned int GetSerializeSize(const std::vector<T, A>& v, int nType, int nSerVersion)
{
    return GetSerializeSize_impl(v, nType, nSerVersion, boost::is_fundamental<T>());
}


template<typename Stream, typename T, typename A>
void Serialize_impl(Stream& os, const std::vector<T, A>& v, int nType, int nSerVersion, const boost::true_type&)
{
    WriteCompactSize(os, v.size());
    if (!v.empty())
        os.write((char*)&v[0], v.size() * sizeof(T));
}

template<typename Stream, typename T, typename A>
void Serialize_impl(Stream& os, const std::vector<T, A>& v, int nType, int nSerVersion, const boost::false_type&)
{
    WriteCompactSize(os, v.size());
    for (typename std::vector<T, A>::const_iterator vi = v.begin(); vi != v.end(); ++vi)
        ::Serialize(os, (*vi), nType, nSerVersion);
}

template<typename Stream, typename T, typename A>
inline void Serialize(Stream& os, const std::vector<T, A>& v, int nType, int nSerVersion)
{
    Serialize_impl(os, v, nType, nSerVersion, boost::is_fundamental<T>());
}


template<typename Stream, typename T, typename A>
void Unserialize_impl(Stream& is, std::vector<T, A>& v, int nType, int nSerVersion, const boost::true_type&)
{
    // Limit size per read so bogus size value won't cause out of memory
    v.clear();
    unsigned int nSize = ReadCompactSize(is);
    unsigned int i = 0;
    while (i < nSize)
    {
        unsigned int blk = std::min(nSize - i, (unsigned int)(1 + 4999999 / sizeof(T)));
        v.resize(i + blk);
        is.read((char*)&v[i], blk * sizeof(T));
        i += blk;
    }
}

template<typename Stream, typename T, typename A>
void Unserialize_impl(Stream& is, std::vector<T, A>& v, int nType, int nSerVersion, const boost::false_type&)
{
    v.clear();
    unsigned int nSize = ReadCompactSize(is);
    unsigned int i = 0;
    unsigned int nMid = 0;
    while (nMid < nSize)
    {
        nMid += 5000000 / sizeof(T);
        if (nMid > nSize)
            nMid = nSize;
        v.resize(nMid);
        for (; i < nMid; i++)
            Unserialize(is, v[i], nType, nSerVersion);
    }
}

template<typename Stream, typename T, typename A>
inline void Unserialize(Stream& is, std::vector<T, A>& v, int nType, int nSerVersion)
{
    Unserialize_impl(is, v, nType, nSerVersion, boost::is_fundamental<T>());
}



//
// others derived from vector
//
inline unsigned int GetSerializeSize(const CScript& v, int nType, int nSerVersion)
{
    return GetSerializeSize((const std::vector<unsigned char>&)v, nType, nSerVersion);
}

template<typename Stream>
void Serialize(Stream& os, const CScript& v, int nType, int nSerVersion)
{
    Serialize(os, (const std::vector<unsigned char>&)v, nType, nSerVersion);
}

template<typename Stream>
void Unserialize(Stream& is, CScript& v, int nType, int nSerVersion)
{
    Unserialize(is, (std::vector<unsigned char>&)v, nType, nSerVersion);
}



//
// pair
//
template<typename K, typename T>
unsigned int GetSerializeSize(const std::pair<K, T>& item, int nType, int nSerVersion)
{
    return GetSerializeSize(item.first, nType, nSerVersion) + GetSerializeSize(item.second, nType, nSerVersion);
}

template<typename Stream, typename K, typename T>
void Serialize(Stream& os, const std::pair<K, T>& item, int nType, int nSerVersion)
{
    Serialize(os, item.first, nType, nSerVersion);
    Serialize(os, item.second, nType, nSerVersion);
}

template<typename Stream, typename K, typename T>
void Unserialize(Stream& is, std::pair<K, T>& item, int nType, int nSerVersion)
{
    Unserialize(is, item.first, nType, nSerVersion);
    Unserialize(is, item.second, nType, nSerVersion);
}



//
// 3 tuple
//
template<typename T0, typename T1, typename T2>
unsigned int GetSerializeSize(const boost::tuple<T0, T1, T2>& item, int nType, int nSerVersion)
{
    unsigned int nSize = 0;
    nSize += GetSerializeSize(boost::get<0>(item), nType, nSerVersion);
    nSize += GetSerializeSize(boost::get<1>(item), nType, nSerVersion);
    nSize += GetSerializeSize(boost::get<2>(item), nType, nSerVersion);
    return nSize;
}

template<typename Stream, typename T0, typename T1, typename T2>
void Serialize(Stream& os, const boost::tuple<T0, T1, T2>& item, int nType, int nSerVersion)
{
    Serialize(os, boost::get<0>(item), nType, nSerVersion);
    Serialize(os, boost::get<1>(item), nType, nSerVersion);
    Serialize(os, boost::get<2>(item), nType, nSerVersion);
}

template<typename Stream, typename T0, typename T1, typename T2>
void Unserialize(Stream& is, boost::tuple<T0, T1, T2>& item, int nType, int nSerVersion)
{
    Unserialize(is, boost::get<0>(item), nType, nSerVersion);
    Unserialize(is, boost::get<1>(item), nType, nSerVersion);
    Unserialize(is, boost::get<2>(item), nType, nSerVersion);
}



//
// 4 tuple
//
template<typename T0, typename T1, typename T2, typename T3>
unsigned int GetSerializeSize(const boost::tuple<T0, T1, T2, T3>& item, int nType, int nSerVersion)
{
    unsigned int nSize = 0;
    nSize += GetSerializeSize(boost::get<0>(item), nType, nSerVersion);
    nSize += GetSerializeSize(boost::get<1>(item), nType, nSerVersion);
    nSize += GetSerializeSize(boost::get<2>(item), nType, nSerVersion);
    nSize += GetSerializeSize(boost::get<3>(item), nType, nSerVersion);
    return nSize;
}

template<typename Stream, typename T0, typename T1, typename T2, typename T3>
void Serialize(Stream& os, const boost::tuple<T0, T1, T2, T3>& item, int nType, int nSerVersion)
{
    Serialize(os, boost::get<0>(item), nType, nSerVersion);
    Serialize(os, boost::get<1>(item), nType, nSerVersion);
    Serialize(os, boost::get<2>(item), nType, nSerVersion);
    Serialize(os, boost::get<3>(item), nType, nSerVersion);
}

template<typename Stream, typename T0, typename T1, typename T2, typename T3>
void Unserialize(Stream& is, boost::tuple<T0, T1, T2, T3>& item, int nType, int nSerVersion)
{
    Unserialize(is, boost::get<0>(item), nType, nSerVersion);
    Unserialize(is, boost::get<1>(item), nType, nSerVersion);
    Unserialize(is, boost::get<2>(item), nType, nSerVersion);
    Unserialize(is, boost::get<3>(item), nType, nSerVersion);
}



//
// map
//
template<typename K, typename T, typename Pred, typename A>
unsigned int GetSerializeSize(const std::map<K, T, Pred, A>& m, int nType, int nSerVersion)
{
    unsigned int nSize = GetSizeOfCompactSize(m.size());
    for (typename std::map<K, T, Pred, A>::const_iterator mi = m.begin(); mi != m.end(); ++mi)
        nSize += GetSerializeSize((*mi), nType, nSerVersion);
    return nSize;
}

template<typename Stream, typename K, typename T, typename Pred, typename A>
void Serialize(Stream& os, const std::map<K, T, Pred, A>& m, int nType, int nSerVersion)
{
    WriteCompactSize(os, m.size());
    for (typename std::map<K, T, Pred, A>::const_iterator mi = m.begin(); mi != m.end(); ++mi)
        Serialize(os, (*mi), nType, nSerVersion);
}

template<typename Stream, typename K, typename T, typename Pred, typename A>
void Unserialize(Stream& is, std::map<K, T, Pred, A>& m, int nType, int nSerVersion)
{
    m.clear();
    unsigned int nSize = ReadCompactSize(is);
    typename std::map<K, T, Pred, A>::iterator mi = m.begin();
    for (unsigned int i = 0; i < nSize; i++)
    {
        std::pair<K, T> item;
        Unserialize(is, item, nType, nSerVersion);
        mi = m.insert(mi, item);
    }
}



//
// set
//
template<typename K, typename Pred, typename A>
unsigned int GetSerializeSize(const std::set<K, Pred, A>& m, int nType, int nSerVersion)
{
    unsigned int nSize = GetSizeOfCompactSize(m.size());
    for (typename std::set<K, Pred, A>::const_iterator it = m.begin(); it != m.end(); ++it)
        nSize += GetSerializeSize((*it), nType, nSerVersion);
    return nSize;
}

template<typename Stream, typename K, typename Pred, typename A>
void Serialize(Stream& os, const std::set<K, Pred, A>& m, int nType, int nSerVersion)
{
    WriteCompactSize(os, m.size());
    for (typename std::set<K, Pred, A>::const_iterator it = m.begin(); it != m.end(); ++it)
        Serialize(os, (*it), nType, nSerVersion);
}

template<typename Stream, typename K, typename Pred, typename A>
void Unserialize(Stream& is, std::set<K, Pred, A>& m, int nType, int nSerVersion)
{
    m.clear();
    unsigned int nSize = ReadCompactSize(is);
    typename std::set<K, Pred, A>::iterator it = m.begin();
    for (unsigned int i = 0; i < nSize; i++)
    {
        K key;
        Unserialize(is, key, nType, nSerVersion);
        it = m.insert(it, key);
    }
}

//
// bitset
//
template<size_t N>
unsigned int GetSerializeSize(const std::bitset<N>& m, int nType, int nSerVersion)
{
    return sizeof(m);
}

template<typename Stream, size_t N>
void Serialize(Stream& os, const std::bitset<N>& m, int nType, int nSerVersion)
{
    WRITEDATA(os, m);
}

template<typename Stream, size_t N>
void Unserialize(Stream& is, std::bitset<N>& m, int nType, int nSerVersion)
{
    READDATA(is, m);
}

//
// Support for IMPLEMENT_SERIALIZE and READWRITE macro
//
class CSerActionGetSerializeSize { };
class CSerActionSerialize { };
class CSerActionUnserialize { };

template<typename Stream, typename T>
inline unsigned int SerReadWrite(Stream& s, const T& obj, int nType, int nSerVersion, CSerActionGetSerializeSize ser_action)
{
    return ::GetSerializeSize(obj, nType, nSerVersion);
}

template<typename Stream, typename T>
inline unsigned int SerReadWrite(Stream& s, const T& obj, int nType, int nSerVersion, CSerActionSerialize ser_action)
{
    ::Serialize(s, obj, nType, nSerVersion);
    return 0;
}

template<typename Stream, typename T>
inline unsigned int SerReadWrite(Stream& s, T& obj, int nType, int nSerVersion, CSerActionUnserialize ser_action)
{
    ::Unserialize(s, obj, nType, nSerVersion);
    return 0;
}

struct ser_streamplaceholder
{
    int nType;
    int nVersion;
};












/** Double ended buffer combining vector and stream-like interfaces.
 *
 * >> and << read and write unformatted data using the above serialization templates.
 * Fills with data in linear time; some stringstream implementations take N^2 time.
 */
class CDataStream
{
protected:
    typedef std::vector<char, zero_after_free_allocator<char> > vector_type;
    vector_type vch;
    unsigned int nReadPos;
    short state;
    short exceptmask;
public:
    int nType;
    int nVersion;

    typedef vector_type::allocator_type   allocator_type;
    typedef vector_type::size_type        size_type;
    typedef vector_type::difference_type  difference_type;
    typedef vector_type::reference        reference;
    typedef vector_type::const_reference  const_reference;
    typedef vector_type::value_type       value_type;
    typedef vector_type::iterator         iterator;
    typedef vector_type::const_iterator   const_iterator;
    typedef vector_type::reverse_iterator reverse_iterator;

    explicit CDataStream(int nTypeIn, int nVersionIn)
    {
        Init(nTypeIn, nVersionIn);
    }

    CDataStream(const_iterator pbegin, const_iterator pend, int nTypeIn, int nVersionIn) : vch(pbegin, pend)
    {
        Init(nTypeIn, nVersionIn);
    }

#if !defined(_MSC_VER) || _MSC_VER >= 1300
    CDataStream(const char* pbegin, const char* pend, int nTypeIn, int nVersionIn) : vch(pbegin, pend)
    {
        Init(nTypeIn, nVersionIn);
    }
#endif

    CDataStream(const vector_type& vchIn, int nTypeIn, int nVersionIn) : vch(vchIn.begin(), vchIn.end())
    {
        Init(nTypeIn, nVersionIn);
    }

    CDataStream(const std::vector<char>& vchIn, int nTypeIn, int nVersionIn) : vch(vchIn.begin(), vchIn.end())
    {
        Init(nTypeIn, nVersionIn);
    }

    CDataStream(const std::vector<unsigned char>& vchIn, int nTypeIn, int nVersionIn) : vch((char*)&vchIn.begin()[0], (char*)&vchIn.end()[0])
    {
        Init(nTypeIn, nVersionIn);
    }

    void Init(int nTypeIn, int nVersionIn)
    {
        nReadPos = 0;
        nType = nTypeIn;
        nVersion = nVersionIn;
        state = 0;
        exceptmask = std::ios::badbit | std::ios::failbit;
    }

    CDataStream& operator+=(const CDataStream& b)
    {
        vch.insert(vch.end(), b.begin(), b.end());
        return *this;
    }

    friend CDataStream operator+(const CDataStream& a, const CDataStream& b)
    {
        CDataStream ret = a;
        ret += b;
        return (ret);
    }

    std::string str() const
    {
        return (std::string(begin(), end()));
    }

    std::string full_str() const
    {
        return (std::string(begin0(), end()));
    }

    std::string hex(size_t nChunk = 0, const std::string& strIndent = "") const
    {
        std::string strHex = HexStr(begin(), end(), true);
        if ((nChunk > 0) || (strIndent != ""))
        {
            return ChunkHex(strHex, nChunk, strIndent);
        }
        return strHex;
    }

    //
    // Vector subset
    //
    const_iterator begin0() const                    { return vch.begin(); }
    iterator begin0()                                { return vch.begin(); }
    const char* begindata() const                    { return &(*vch.begin()); }
    const_iterator begin() const                     { return vch.begin() + nReadPos; }
    iterator begin()                                 { return vch.begin() + nReadPos; }
    const char* enddata() const                      { return &(*vch.end()); }
    const_iterator end() const                       { return vch.end(); }
    iterator end()                                   { return vch.end(); }
    size_type size() const                           { return vch.size() - nReadPos; }
    int in_avail() const                             { return size(); }
    size_type sizeall() const                        { return vch.size(); }
    size_type at() const                             { return nReadPos; }
    bool empty() const                               { return vch.size() == nReadPos; }
    void resize(size_type n, value_type c=0)         { vch.resize(n + nReadPos, c); }
    void reserve(size_type n)                        { vch.reserve(n + nReadPos); }
    const_reference operator[](size_type pos) const  { return vch[pos + nReadPos]; }
    reference operator[](size_type pos)              { return vch[pos + nReadPos]; }
    void clear()                                     { vch.clear(); nReadPos = 0; }
    iterator insert(iterator it, const char& x=char()) { return vch.insert(it, x); }
    void insert(iterator it, size_type n, const char& x) { vch.insert(it, n, x); }


    void insert(iterator it, std::vector<char>::const_iterator first, std::vector<char>::const_iterator last)
    {
        assert(last - first >= 0);
        if (it == vch.begin() + nReadPos && (unsigned int)(last - first) <= nReadPos)
        {
            // special case for inserting at the front when there's room
            nReadPos -= (last - first);
            memcpy(&vch[nReadPos], &first[0], last - first);
        }
        else
            vch.insert(it, first, last);
    }

#if !defined(_MSC_VER) || _MSC_VER >= 1300
    void insert(iterator it, const char* first, const char* last)
    {
        assert(last - first >= 0);
        if (it == vch.begin() + nReadPos && (unsigned int)(last - first) <= nReadPos)
        {
            // special case for inserting at the front when there's room
            nReadPos -= (last - first);
            memcpy(&vch[nReadPos], &first[0], last - first);
        }
        else
            vch.insert(it, first, last);
    }
#endif

    iterator erase(iterator it)
    {
        if (it == vch.begin() + nReadPos)
        {
            // special case for erasing from the front
            if (++nReadPos >= vch.size())
            {
                // whenever we reach the end, we take the opportunity to clear the buffer
                nReadPos = 0;
                return vch.erase(vch.begin(), vch.end());
            }
            return vch.begin() + nReadPos;
        }
        else
            return vch.erase(it);
    }

    iterator erase(iterator first, iterator last)
    {
        if (first == vch.begin() + nReadPos)
        {
            // special case for erasing from the front
            if (last == vch.end())
            {
                nReadPos = 0;
                return vch.erase(vch.begin(), vch.end());
            }
            else
            {
                nReadPos = (last - vch.begin());
                return last;
            }
        }
        else
            return vch.erase(first, last);
    }

    inline void Compact()
    {
        vch.erase(vch.begin(), vch.begin() + nReadPos);
        nReadPos = 0;
    }

    bool Rewind(size_type n = std::numeric_limits<size_type>::max())
    {
        if (n == std::numeric_limits<size_type>::max())
        {
            nReadPos = 0;
            return true;
        }
        // Rewind by n characters if the buffer hasn't been compacted yet
        if (n > nReadPos)
        {
            return false;
        }
        nReadPos -= n;
        return true;
    }

    //
    // Stream subset
    //
    void setstate(short bits, const char* psz)
    {
        state |= bits;
        if (state & exceptmask)
            THROW_WITH_STACKTRACE(std::ios_base::failure(psz));
    }

    bool eof() const             { return size() == 0; }
    bool fail() const            { return state & (std::ios::badbit | std::ios::failbit); }
    bool good() const            { return !eof() && (state == 0); }
    void clear(short n)          { state = n; }  // name conflict with vector clear()
    short exceptions()           { return exceptmask; }
    short exceptions(short mask) { short prev = exceptmask; exceptmask = mask; setstate(0, "CDataStream"); return prev; }
    CDataStream* rdbuf()         { return this; }

    void SetType(int n)          { nType = n; }
    int GetType()                { return nType; }
    void SetVersion(int n)       { nVersion = n; }
    int GetVersion()             { return nVersion; }
    void ReadVersion()           { *this >> nVersion; }
    void WriteVersion()          { *this << nVersion; }

    CDataStream& read(char* pch, int nSize)
    {
        // Read from the beginning of the buffer
        assert(nSize >= 0);
        unsigned int nReadPosNext = nReadPos + nSize;
        if (nReadPosNext >= vch.size())
        {
            if (nReadPosNext > vch.size())
            {
                setstate(std::ios::failbit, "CDataStream::read() : end of data");
                memset(pch, 0, nSize);
                nSize = vch.size() - nReadPos;
            }
            memcpy(pch, &vch[nReadPos], nSize);
            nReadPos = 0;
            vch.clear();
            return (*this);
        }
        memcpy(pch, &vch[nReadPos], nSize);
        nReadPos = nReadPosNext;
        return (*this);
    }

    CDataStream& ignore(int nSize)
    {
        // Ignore from the beginning of the buffer
        assert(nSize >= 0);
        unsigned int nReadPosNext = nReadPos + nSize;
        if (nReadPosNext >= vch.size())
        {
            if (nReadPosNext > vch.size())
            {
                setstate(std::ios::failbit, "CDataStream::ignore() : end of data");
                nSize = vch.size() - nReadPos;
            }
            nReadPos = 0;
            vch.clear();
            return (*this);
        }
        nReadPos = nReadPosNext;
        return (*this);
    }

    CDataStream& write(const char* pch, int nSize)
    {
        // Write to the end of the buffer
        assert(nSize >= 0);
        vch.insert(vch.end(), pch, pch + nSize);
        return (*this);
    }

    template<typename Stream>
    void Serialize(Stream& s, int nType, int nSerVersion) const
    {
        // Special case: stream << stream concatenates like stream += stream
        if (!vch.empty())
            s.write((char*)&vch[0], vch.size() * sizeof(vch[0]));
    }

    template<typename T>
    unsigned int GetSerializeSize(const T& obj)
    {
        // Tells the size of the object if serialized to this stream
        return ::GetSerializeSize(obj, nType, nVersion);
    }

    template<typename T>
    CDataStream& operator<<(const T& obj)
    {
        // Serialize to this stream
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }

    template<typename T>
    CDataStream& operator>>(T& obj)
    {
        // Unserialize from this stream
        ::Unserialize(*this, obj, nType, nVersion);
        return (*this);
    }
};


/** RAII wrapper for FILE*.
 *
 * Will automatically close the file when it goes out of scope if not null.
 * If you're returning the file pointer, return file.release().
 * If you need to close the file early, use file.fclose() instead of fclose(file).
 */
class CAutoFile
{
protected:
    FILE* file;
    short state;
    short exceptmask;
public:
    int nType;
    int nVersion;

    CAutoFile(FILE* filenew, int nTypeIn, int nVersionIn)
    {
        file = filenew;
        nType = nTypeIn;
        nVersion = nVersionIn;
        state = 0;
        exceptmask = std::ios::badbit | std::ios::failbit;
    }

    ~CAutoFile()
    {
        fclose();
    }

    void fclose()
    {
        if (file != NULL && file != stdin && file != stdout && file != stderr)
            ::fclose(file);
        file = NULL;
    }

    FILE* release()             { FILE* ret = file; file = NULL; return ret; }
    operator FILE*()            { return file; }
    FILE* operator->()          { return file; }
    FILE& operator*()           { return *file; }
    FILE** operator&()          { return &file; }
    FILE* operator=(FILE* pnew) { return file = pnew; }
    bool operator!()            { return (file == NULL); }


    //
    // Stream subset
    //
    void setstate(short bits, const char* psz)
    {
        state |= bits;
        if (state & exceptmask)
            THROW_WITH_STACKTRACE(std::ios_base::failure(psz));
    }

    bool fail() const            { return state & (std::ios::badbit | std::ios::failbit); }
    bool good() const            { return state == 0; }
    void clear(short n = 0)      { state = n; }
    short exceptions()           { return exceptmask; }
    short exceptions(short mask) { short prev = exceptmask; exceptmask = mask; setstate(0, "CAutoFile"); return prev; }

    void SetType(int n)          { nType = n; }
    int GetType()                { return nType; }
    void SetVersion(int n)       { nVersion = n; }
    int GetVersion()             { return nVersion; }
    void ReadVersion()           { *this >> nVersion; }
    void WriteVersion()          { *this << nVersion; }

    CAutoFile& read(char* pch, size_t nSize)
    {
        if (!file)
            throw std::ios_base::failure("CAutoFile::read : file handle is NULL");
        if (fread(pch, 1, nSize, file) != nSize)
            setstate(std::ios::failbit, feof(file) ? "CAutoFile::read : end of file" : "CAutoFile::read : fread failed");
        return (*this);
    }

    CAutoFile& write(const char* pch, size_t nSize)
    {
        if (!file)
            throw std::ios_base::failure("CAutoFile::write : file handle is NULL");
        if (fwrite(pch, 1, nSize, file) != nSize)
            setstate(std::ios::failbit, "CAutoFile::write : write failed");
        return (*this);
    }

    template<typename T>
    unsigned int GetSerializeSize(const T& obj)
    {
        // Tells the size of the object if serialized to this stream
        return ::GetSerializeSize(obj, nType, nVersion);
    }

    template<typename T>
    CAutoFile& operator<<(const T& obj)
    {
        // Serialize to this stream
        if (!file)
            throw std::ios_base::failure("CAutoFile::operator<< : file handle is NULL");
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }

    template<typename T>
    CAutoFile& operator>>(T& obj)
    {
        // Unserialize from this stream
        if (!file)
            throw std::ios_base::failure("CAutoFile::operator>> : file handle is NULL");
        ::Unserialize(*this, obj, nType, nVersion);
        return (*this);
    }
};

#endif
