// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#include "uint256.h"
#include "serialize.h"
#include "core-hashes.hpp"

#ifndef WIN32
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <inttypes.h>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <openssl/sha.h>

#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>

// ensure functions like fseek() can handle large files
#if LONG_MAX < 9223372036854775807LL
    #error "long int is less than 64 bits on this system"
#endif


static const int64_t COIN = 1000000;
static const int64_t CENT = 10000;

#define LOOP                for (;;)
#define BEGIN(a)            ((char*)&(a))
#define END(a)              ((char*)&((&(a))[1]))
#define UBEGIN(a)           ((unsigned char*)&(a))
#define UEND(a)             ((unsigned char*)&((&(a))[1]))
#define ARRAYLEN(array)     (sizeof(array)/sizeof((array)[0]))

#define UVOIDBEGIN(a)        ((void*)&(a))
#define CVOIDBEGIN(a)        ((const void*)&(a))
#define UINTBEGIN(a)        ((uint32_t*)&(a))
#define CUINTBEGIN(a)        ((const uint32_t*)&(a))

#define EXTEND(a, b) (a.insert(a.end(), b.begin(), b.end()))

#ifndef PRI64d
#if defined(_MSC_VER) || defined(__MSVCRT__)
#define PRI64d  "I64d"
#define PRI64u  "I64u"
#define PRI64x  "I64x"
#else
#define PRI64d  "lld"
#define PRI64u  "llu"
#define PRI64x  "llx"
#endif
#endif

#ifndef THROW_WITH_STACKTRACE
#define THROW_WITH_STACKTRACE(exception)  \
{                                         \
    LogStackTrace();                      \
    throw (exception);                    \
}
void LogStackTrace();
#endif


/* Format characters for (s)size_t and ptrdiff_t */
#if defined(_MSC_VER) || defined(__MSVCRT__)
  /* (s)size_t and ptrdiff_t have the same size specifier in MSVC:
     http://msdn.microsoft.com/en-us/library/tcxf1dw6%28v=vs.100%29.aspx
   */
  #define PRIszx    "Ix"
  #define PRIszu    "Iu"
  #define PRIszd    "Id"
  #define PRIpdx    "Ix"
  #define PRIpdu    "Iu"
  #define PRIpdd    "Id"
#else /* C99 standard */
  #define PRIszx    "zx"
  #define PRIszu    "zu"
  #define PRIszd    "zd"
  #define PRIpdx    "tx"
  #define PRIpdu    "tu"
  #define PRIpdd    "td"
#endif

// This is needed because the foreach macro can't get over the comma in pair<t1, t2>
#define PAIRTYPE(t1, t2)    std::pair<t1, t2>

// Align by increasing pointer, must have extra space at end of buffer
template <size_t nBytes, typename T>
T* alignup(T* p)
{
    union
    {
        T* ptr;
        size_t n;
    } u;
    u.ptr = p;
    u.n = (u.n + (nBytes-1)) & ~(nBytes-1);
    return u.ptr;
}

#ifdef WIN32
#define MSG_NOSIGNAL        0
#define MSG_DONTWAIT        0

#ifndef S_IRUSR
#define S_IRUSR             0400
#define S_IWUSR             0200
#endif
#else
#ifdef __APPLE__
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL        0
#endif
#endif
#define MAX_PATH            1024
#endif

inline void MilliSleep(uint64_t n)
{
    /*Boost has a year 2038 problem— if the request sleep time is past
      epoch+2^31 seconds the sleep returns instantly. So we clamp our sleeps
      here to 10 years and hope that boost is fixed by 2028.*/
    boost::thread::sleep(boost::get_system_time() +
                         boost::posix_time::milliseconds(
                             n > 315576000000LL ? 315576000000LL : n));
}

/* This GNU C extension enables the compiler to check the format string against
 * the parameters provided. X is the number of the "format string" parameter,
 * and Y is the number of the first variadic parameter. Parameters count
 * from 1.
 */
#ifdef __GNUC__
#define ATTR_WARN_PRINTF(X,Y) __attribute__((format(printf,X,Y)))
#else
#define ATTR_WARN_PRINTF(X,Y)
#endif


extern std::map<std::string, std::string> mapArgs;
extern std::map<std::string, std::vector<std::string> > mapMultiArgs;
extern bool fTestFeature;
extern bool fDebug;
extern bool fStaking;
extern bool fDebugNet;
extern bool fPrintToConsole;
extern bool fPrintToDebugger;
extern bool fRequestShutdown;
extern bool fShutdown;
extern bool fDaemon;
extern bool fServer;
extern bool fCommandLine;
extern std::string strMiscWarning;
extern bool fTestNet;
extern bool nTestNet;
extern bool fNoListen;
extern bool fLogTimestamps;
extern bool fReopenDebugLog;

extern int nMaxHeight;

extern bool fDebugQPoS;
extern bool fDebugBlockCreation;

extern int nBestHeight;
extern int GetFork(int nHeight);

void RandAddSeed();
void RandAddSeedPerfmon();
int ATTR_WARN_PRINTF(1,2) OutputDebugStringF(const char* pszFormat, ...);

/*
  Rationale for the real_strprintf / strprintf construction:
    It is not allowed to use va_start with a pass-by-reference argument.
    (C++ standard, 18.7, paragraph 3). Use a dummy argument to work around
  this, and use a macro to keep similar semantics.
*/

/** Overload strprintf for char*, so that GCC format type warnings can be given
 */
std::string ATTR_WARN_PRINTF(1, 3)
    real_strprintf(const char* format, int dummy, ...);
/** Overload strprintf for std::string, to be able to use it with _
 * (translation). This will not support GCC format type warnings (-Wformat) so
 * be careful.
 */
std::string real_strprintf(const std::string &format, int dummy, ...);
#define strprintf(format, ...) real_strprintf(format, 0, __VA_ARGS__)
std::string vstrprintf(const char *format, va_list ap);

bool ATTR_WARN_PRINTF(1,2) error(const char *format, ...);
bool error(const std::string& str);
bool ATTR_WARN_PRINTF(1,2) warning(const char *format, ...);

/* Redefine printf so that it directs output to debug.log
 *
 * Do this *after* defining the other printf-like functions,
 *    because otherwise the
 *         __attribute__((format(printf,X,Y)))
 *    gets expanded to
 *         __attribute__((format(OutputDebugStringF,X,Y)))
 *    which confuses gcc.
 */
#define printf OutputDebugStringF

void LogException(std::exception* pex, const char* pszThread);
void PrintException(std::exception* pex, const char* pszThread);
void PrintExceptionContinue(std::exception* pex, const char* pszThread);
void ParseString(const std::string& str, char c, std::vector<std::string>& v);
std::string FormatMoney(int64_t n, bool fPlus=false);
bool ParseMoney(const std::string& str, int64_t& nRet);
bool ParseMoney(const char* pszIn, int64_t& nRet);
std::string SanitizeString(const std::string& str);
std::vector<unsigned char> ParseHex(const char* psz);
std::vector<unsigned char> ParseHex(const std::string& str);
bool IsHex(const std::string& str);
std::vector<unsigned char> DecodeBase64(const char* p, bool* pfInvalid = NULL);
std::string DecodeBase64(const std::string& str);
std::string EncodeBase64(const unsigned char* pch, size_t len);
std::string EncodeBase64(const std::string& str);
std::vector<unsigned char> DecodeBase32(const char* p, bool* pfInvalid = NULL);
std::string DecodeBase32(const std::string& str);
std::string EncodeBase32(const unsigned char* pch,
                         size_t len,
                         bool pad = true);
std::string EncodeBase32(const std::string& str, bool pad = true);
void ParseParameters(int argc, const char*const argv[]);
bool WildcardMatch(const char* psz, const char* mask);
bool WildcardMatch(const std::string& str, const std::string& mask);
void FileCommit(FILE *fileout);
long int GetFilesize(FILE* file);
bool RenameOver(boost::filesystem::path src, boost::filesystem::path dest);
boost::filesystem::path GetDefaultDataDir();
const boost::filesystem::path &GetDataDir(bool fNetSpecific = true);
boost::filesystem::path GetConfigFile();
boost::filesystem::path GetPidFile();
void CreatePidFile(const boost::filesystem::path &path, pid_t pid);
void ReadConfigFile(
    std::map<std::string, std::string>& mapSettingsRet,
    std::map<std::string, std::vector<std::string>>& mapMultiSettingsRet);
#ifdef WIN32
boost::filesystem::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif
void ShrinkDebugFile();
int GetRandInt(int nMax);
uint64_t GetRand(uint64_t nMax);
uint256 GetRandHash();
int64_t GetTime();
void SetMockTime(int64_t nMockTimeIn);
long hex2long(const unsigned char* hexString);
std::string FormatClientVersion();
std::string FormatFullVersion();
std::string FormatVersionNumbers();
std::string FormatSubVersion(const std::string& name,
                             int nClientVersion,
                             const std::vector<std::string>& comments);
BIGNUM* ECPoint2BIGNUM(const EC_GROUP* group,
                       const EC_POINT* point,
                       point_conversion_form_t form,
                       BIGNUM* bn,
                       BN_CTX* ctx);
EC_POINT* BIGNUM2ECPoint(const EC_GROUP* group,
                         const BIGNUM* bn,
                         EC_POINT* point,
                         BN_CTX* ctx);
void runCommand(std::string strCommand);


class CRandGen {
public:
    using result_type = uint64_t;

    static constexpr result_type min()
    {
        return 0;
    }
    static constexpr result_type max()
    {
        return std::numeric_limits<result_type>::max();
    }

    result_type operator()()
    {
        return GetRand(max());
    }
};


inline std::string i64tostr(int64_t n)
{
    return strprintf("%" PRId64, n);
}

inline std::string itostr(int n)
{
    return strprintf("%d", n);
}

inline int64_t atoi64(const char* psz)
{
#ifdef _MSC_VER
    return _atoi64(psz);
#else
    return strtoll(psz, NULL, 10);
#endif
}

inline int64_t atoi64(const std::string& str)
{
#ifdef _MSC_VER
    return _atoi64(str.c_str());
#else
    return strtoll(str.c_str(), NULL, 10);
#endif
}

inline int atoi(const std::string& str)
{
    return atoi(str.c_str());
}

inline int roundint(double d)
{
    return (int)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64_t roundint64(double d)
{
    return (int64_t)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64_t abs64(int64_t n)
{
    return (n >= 0 ? n : -n);
}


template<typename T>
void PrintHex(const T pbegin,
              const T pend,
              const char* pszFormat = "%s",
              bool fSpaces = true)
{
    printf(pszFormat, HexStr(pbegin, pend, fSpaces).c_str());
}

inline void PrintHex(const std::vector<unsigned char>& vch,
                     const char* pszFormat = "%s",
                     bool fSpaces = true)
{
    printf(pszFormat, HexStr(vch, fSpaces).c_str());
}


// Represent a bitset as hex.
// The returned value is essentially a hex number.
// Low bits of this hex number are low indices of the bitset.
template<typename T>
std::string BitsetAsHex(const T& b)
{
    std::string strResult;
    unsigned int nSize = b.size();
    unsigned int nBytes = nSize / 8;
    char psz[3];
    // This loop is bitwise because one cannot
    // rely on the precise implementation of a bitset.
    for (unsigned int i = 0; i < nBytes; ++i)
    {
        unsigned int byte = 0;
        for (unsigned int j = 0; j < 8; ++j)
        {
            if (j)
            {
                byte <<= 1;
            }
            unsigned int idx = nSize - (i * 8 + j) - 1;
            if (b[idx])
            {
                byte |= 1;
            }
        }
        snprintf(psz, 3, "%02x", byte);
        strResult += std::string(psz, psz + 2);
    }
    return strResult;
}


inline int64_t GetPerformanceCounter()
{
    int64_t nCounter = 0;
#ifdef WIN32
    QueryPerformanceCounter((LARGE_INTEGER*)&nCounter);
#else
    timeval t;
    gettimeofday(&t, NULL);
    nCounter = (int64_t) t.tv_sec * 1000000 + t.tv_usec;
#endif
    return nCounter;
}

inline int64_t GetTimeMillis()
{
    return (boost::posix_time::ptime(
                boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1)))
        .total_milliseconds();
}

inline std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime)
{
    time_t n = nTime;
    struct tm* ptmTime = gmtime(&n);
    char pszTime[200];
    strftime(pszTime, sizeof(pszTime), pszFormat, ptmTime);
    return pszTime;
}

static const std::string strTimestampFormat = "%Y-%m-%d %H:%M:%S UTC";
inline std::string DateTimeStrFormat(int64_t nTime)
{
    return DateTimeStrFormat(strTimestampFormat.c_str(), nTime);
}


template<typename T>
void skipspaces(T& it)
{
    while (isspace(*it))
        ++it;
}

inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

/**
 * Return string argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. "1")
 * @return command-line argument or default value
 */
std::string GetArg(const std::string& strArg, const std::string& strDefault);

/**
 * Return integer argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. 1)
 * @return command-line argument (0 if invalid number) or default value
 */
int64_t GetArg(const std::string& strArg, int64_t nDefault);

/**
 * Return boolean argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (true or false)
 * @return command-line argument or default value
 */
bool GetBoolArg(const std::string& strArg, bool fDefault=false);

/**
 * Set an argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param strValue Value (e.g. "1")
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetArg(const std::string& strArg, const std::string& strValue);

/**
 * Set a boolean argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param fValue Value (e.g. false)
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetBoolArg(const std::string& strArg, bool fValue);


class CHashWriter
{
private:
    EVP_MD_CTX *ctx;
    const EVP_MD *md;

public:
    int nType;
    int nVersion;

    void Init() {
// sha256
#if OPENSSL_VERSION_NUMBER < 0x10100000L
        if (ctx) {
            EVP_MD_CTX_cleanup(ctx);
        } else {
            ctx = EVP_MD_CTX_create();
        }
#else
        if (ctx) {
            EVP_MD_CTX_reset(ctx);
        } else {
            ctx = EVP_MD_CTX_new();
        }
#endif
        md = EVP_sha256();
        EVP_DigestInit_ex(ctx, md, NULL);
    }

    CHashWriter(int nTypeIn, int nVersionIn)
        : ctx(NULL), nType(nTypeIn), nVersion(nVersionIn)
    {
        Init();
    }

    ~CHashWriter() {
        if (ctx) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
            EVP_MD_CTX_destroy(ctx);
#else
            EVP_MD_CTX_free(ctx);
#endif
        }
    }

    CHashWriter& write(const char *pch, size_t size) {
        EVP_DigestUpdate(ctx, pch, size);
        return (*this);
    }

    // invalidates the object
    uint256 GetHash() {
        uint256 hash1;
        unsigned int len;
        EVP_DigestFinal_ex(ctx, (unsigned char*)&hash1, &len);

        uint256 hash2;
        EVP_MD_CTX *ctx2 = EVP_MD_CTX_create();
        EVP_DigestInit_ex(ctx2, md, NULL);
        EVP_DigestUpdate(ctx2, (unsigned char*)&hash1, sizeof(hash1));
        EVP_DigestFinal_ex(ctx2, (unsigned char*)&hash2, &len);
        EVP_MD_CTX_destroy(ctx2);

        return hash2;
    }

    template<typename T>
    CHashWriter& operator<<(const T& obj) {
        // Serialize to this stream
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }
};

template<typename T1, typename T2>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end)
{
    static unsigned char pblank[1];
    uint256 hash1;
    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    const EVP_MD *md = EVP_sha256();
    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx,
                     (p1begin == p1end ? pblank
                                       : (unsigned char*) &p1begin[0]),
                     (p1end - p1begin) * sizeof(p1begin[0]));
    EVP_DigestUpdate(ctx,
                     (p2begin == p2end ? pblank
                                       : (unsigned char*) &p2begin[0]),
                     (p2end - p2begin) * sizeof(p2begin[0]));
    unsigned int len;
    EVP_DigestFinal_ex(ctx, (unsigned char*)&hash1, &len);
    EVP_MD_CTX_destroy(ctx);

    uint256 hash2;
    ctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx, (unsigned char*)&hash1, sizeof(hash1));
    EVP_DigestFinal_ex(ctx, (unsigned char*)&hash2, &len);
    EVP_MD_CTX_destroy(ctx);

    return hash2;
}

template<typename T1, typename T2, typename T3>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end,
                    const T3 p3begin, const T3 p3end)
{
    static unsigned char pblank[1];
    uint256 hash1;
    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    const EVP_MD *md = EVP_sha256();
    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx,
                     (p1begin == p1end ? pblank
                                       : (unsigned char*) &p1begin[0]),
                     (p1end - p1begin) * sizeof(p1begin[0]));
    EVP_DigestUpdate(ctx,
                     (p2begin == p2end ? pblank
                                       : (unsigned char*) &p2begin[0]),
                     (p2end - p2begin) * sizeof(p2begin[0]));
    EVP_DigestUpdate(ctx,
                     (p3begin == p3end ? pblank
                                       : (unsigned char*) &p3begin[0]),
                     (p3end - p3begin) * sizeof(p3begin[0]));
    unsigned int len;
    EVP_DigestFinal_ex(ctx, (unsigned char*)&hash1, &len);
    EVP_MD_CTX_destroy(ctx);

    uint256 hash2;
    ctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx, (unsigned char*)&hash1, sizeof(hash1));
    EVP_DigestFinal_ex(ctx, (unsigned char*)&hash2, &len);
    EVP_MD_CTX_destroy(ctx);

    return hash2;
}

template<typename T>
uint256 SerializeHash(const T& obj,
                      int nType = SER_GETHASH,
                      int nVersion = PROTOCOL_VERSION)
{
    CHashWriter ss(nType, nVersion);
    ss << obj;
    return ss.GetHash();
}

inline uint160 Hash160(const std::vector<unsigned char>& vch)
{
    uint256 hash1;
    CoreHashes::SHA256(vch.data(),
                       vch.size(),
                       (unsigned char*)&hash1);
    uint160 hash2;
    CoreHashes::RIPEMD160((unsigned char*) &hash1,
                          sizeof(hash1),
                          (unsigned char*) &hash2);
    return hash2;
}

/** Median filter over a stream of values.
 * Returns the median of the last N numbers
 */
template <typename T> class CMedianFilter
{
private:
    std::vector<T> vValues;
    std::vector<T> vSorted;
    unsigned int nSize;
    T tInitial;
public:
    CMedianFilter(unsigned int size, T initial_value):
        nSize(size)
    {
        vValues.reserve(size);
        vValues.push_back(initial_value);
        vSorted = vValues;
        tInitial = initial_value;
    }

    void input(T value)
    {
        if(vValues.size() == nSize)
        {
            vValues.erase(vValues.begin());
        }
        vValues.push_back(value);

        vSorted.resize(vValues.size());
        std::copy(vValues.begin(), vValues.end(), vSorted.begin());
        std::sort(vSorted.begin(), vSorted.end());
    }

    // remove last instance of a value
    void removeLast(T value)
    {
        for (int i = vValues.size()-1; i >= 0; --i)
        {
            if (vValues[i] == value)
            {
                vValues.erase(vValues.begin()+i);
                break;
            }
        }
        if (vValues.empty())
        {
            vValues.push_back(tInitial);
        }

        vSorted.resize(vValues.size());
        std::copy(vValues.begin(), vValues.end(), vSorted.begin());
        std::sort(vSorted.begin(), vSorted.end());
    }

    T median() const
    {
        int size = vSorted.size();
        assert(size>0);
        if(size & 1) // Odd number of elements
        {
            return vSorted[size/2];
        }
        else // Even number of elements
        {
            return (vSorted[size/2-1] + vSorted[size/2]) / 2;
        }
    }

    int size() const
    {
        return vValues.size();
    }

    std::vector<T> sorted () const
    {
        return vSorted;
    }
};

bool NewThread(void(*pfn)(void*), void* parg);

#ifdef WIN32
inline void SetThreadPriority(int nPriority)
{
    SetThreadPriority(GetCurrentThread(), nPriority);
}
#else

#define THREAD_PRIORITY_LOWEST          PRIO_MAX
#define THREAD_PRIORITY_BELOW_NORMAL    2
#define THREAD_PRIORITY_NORMAL          0
#define THREAD_PRIORITY_ABOVE_NORMAL    0

inline void SetThreadPriority(int nPriority)
{
    // It's unclear if it's even possible to change thread priorities on Linux,
    // but we really and truly need it for the generation threads.
#ifdef PRIO_THREAD
    setpriority(PRIO_THREAD, 0, nPriority);
#else
    setpriority(PRIO_PROCESS, 0, nPriority);
#endif
}

inline void ExitThread(size_t nExitCode)
{
    pthread_exit((void*)nExitCode);
}
#endif

void RenameThread(const char* name);

inline uint32_t ByteReverse(uint32_t value)
{
    value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
    return (value<<16) | (value>>16);
}

#endif

