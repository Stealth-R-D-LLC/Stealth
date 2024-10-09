// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "netbase.h"
#include "util.h"
#include "sync.h"
#include "ui_interface.h"

#ifndef WIN32
#include <sys/fcntl.h>
#endif

#include "bitcoin-strlcpy.h"
#include "core-hashes.hpp"

#include <boost/foreach.hpp>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()


#define NFILL IP64_MARKER_START


using namespace std;

bool fNetworks = NO_NETWORK;

const char PSZ_NULL_IP[] = "0.0.0.0";
const CService CSERVICE_NULL = CService(string(PSZ_NULL_IP), 0);

const size_t ONION_V2_PUBKEY = 10;
const size_t ONION_V3_PUBKEY = 32;
const size_t GARLIC_PUBKEY = 10;

// Random bytes that versions >=64200 put at the end of 64 byte
//    addresses serialization.
const unsigned char PCH_IP64_MARKER[IP64_MARKER_SIZE] = {
                                      0x28, 0x5c, 0x59, 0xde, 0x91,
                                      0x18, 0x9e, 0xca, 0xc2, 0x81,
                                      0x47, 0x8a, 0xf7, 0x96, 0x7f,
                                      0x1b, 0xe5, 0x8a, 0xee, 0xf9 };


//////////////////////////////////////////////////////////////////////////////
// Hidden Service Constants
//////////////////////////////////////////////////////////////////////////////
// Leader bytes that are only used within CNetAddr as markers,
//    although apparently they may get their values from some standard
static const unsigned char pchOnionCat[] = { 0xFD, 0x87, 0xD8,
                                             0x7E, 0xEB, 0x43 };
static const unsigned char pchGarlicCat[] = { 0xFD, 0x60, 0xDB,
                                              0x4D, 0xDD, 0xB5 };

// pre-compute a bunch of terms that are used a lot
// tor constants
static const size_t TOR_ADDRESS_VERSION = 3;
static const size_t ONION_CAT_BYTES = sizeof(pchOnionCat);
static const size_t ONION_V2_BYTES = ONION_CAT_BYTES + ONION_V2_PUBKEY;
static const size_t ONION_V3_BYTES = ONION_CAT_BYTES + ONION_V3_PUBKEY + 3;
static const size_t ONION_BYTES = (TOR_ADDRESS_VERSION == 3) ?
                                           ONION_V3_BYTES : ONION_V2_BYTES;
static const size_t ONION_ADDRESS = ONION_BYTES - ONION_CAT_BYTES;
static const string STR_ONION_SUFFIX(".onion");
static const size_t ONION_SUFFIX = STR_ONION_SUFFIX.size();

// i2p constants
static const size_t GARLIC_CAT_BYTES = sizeof(pchGarlicCat);
static const size_t GARLIC_BYTES = GARLIC_PUBKEY + GARLIC_CAT_BYTES;
static const size_t GARLIC_ADDRESS = GARLIC_BYTES - GARLIC_CAT_BYTES;
static const string STR_GARLIC_SUFFIX(".oc.b32.i2p");
static const size_t GARLIC_SUFFIX = STR_GARLIC_SUFFIX.size();
//////////////////////////////////////////////////////////////////////////////


// Settings
static proxyType proxyInfo[NET_MAX];
static proxyType nameproxyInfo;
static CCriticalSection cs_proxyInfos;
int nConnectTimeout = 5000;
bool fNameLookup = false;

static const unsigned char pchIPv4[12] = { 0, 0, 0, 0, 0,    0,
                                           0, 0, 0, 0, 0xff, 0xff };

CMedianFilter<int64_t> vTimeOffsets(200,0);


const char* GetNetworkType(Network n)
{
    switch (n)
    {
    case NET_UNROUTABLE: return "unroutable";
    case NET_IPV4: return "ipv4";
    case NET_IPV6: return "ipv6";
    case NET_TOR: return "tor";
    case NET_I2P: return "i2p";
    default: return "none";
    }
    return NULL;
}

enum Network ParseNetwork(string net)
{
    boost::to_lower(net);
    if (net == "ipv4")
    {
        return NET_IPV4;
    }
    if (net == "ipv6")
    {
        return NET_IPV6;
    }
    if (net == "tor")
    {
        return NET_TOR;
    }
    if (net == "i2p")
    {
        return NET_I2P;
    }
    return NET_UNROUTABLE;
}

void SplitHostPort(string in, int& portOut, string& hostOut)
{
    size_t colon = in.find_last_of(':');
    // if a : is found, and it either follows a [...],
    //    or no other : is in the string, treat it as port separator
    bool fHaveColon = colon != in.npos;
    // if there is a colon, and in[0]=='[',
    //    colon is not 0, so in[colon-1] is safe
    bool fBracketed = fHaveColon &&
                      ((in[0] == '[') &&
                       (in[colon - 1] == ']'));
    bool fMultiColon = fHaveColon &&
                       (in.find_last_of(':', colon - 1) != in.npos);
    if (fHaveColon && (colon == 0 || fBracketed || !fMultiColon))
    {
        char* endp = NULL;
        int n = strtol(in.c_str() + colon + 1, &endp, 10);
        if (endp && *endp == 0 && n >= 0)
        {
            in = in.substr(0, colon);
            if (n > 0 && n < 0x10000)
            {
                portOut = n;
            }
        }
    }
    if (in.size() > 0 && in[0] == '[' && in[in.size() - 1] == ']')
    {
        hostOut = in.substr(1, in.size() - 2);
    }
    else
    {
        hostOut = in;
    }
}

static int64_t nTimeOffset = 0;
int64_t GetAdjustedTime()
{
    if (GetFork(nBestHeight + 1) >= XST_FORKQPOS)
    {
        return GetTime();
    }
    else
    {
        return GetTime() + nTimeOffset;
    }
}

void AddTimeData(const CNetAddr& ip, int64_t nTime)
{
    int64_t nOffsetSample = nTime - GetTime();

    // Ignore duplicates
    static set<CNetAddr> setKnown;
    if (!setKnown.insert(ip).second)
    {
        return;
    }

    // Add data
    vTimeOffsets.input(nOffsetSample);
    printf("Added time data, samples %d, offset %+" PRId64 " (%+" PRId64
           " minutes)\n",
           vTimeOffsets.size(),
           nOffsetSample,
           nOffsetSample / 60);
    if (vTimeOffsets.size() >= 5 && vTimeOffsets.size() % 2 == 1)
    {
        int64_t nMedian = vTimeOffsets.median();
        vector<int64_t> vSorted = vTimeOffsets.sorted();
        // Only let other nodes change our time by so much
        if (abs64(nMedian) < 70 * 60)
        {
            nTimeOffset = nMedian;
        }
        else
        {
            nTimeOffset = 0;

            static bool fDone;
            if (!fDone)
            {
                // If nobody has a time different than ours but
                //  within 5 minutes of ours, give a warning
                bool fMatch = false;
                BOOST_FOREACH (int64_t nOffset, vSorted)
                {
                    if (nOffset != 0 && abs64(nOffset) < 5 * 60)
                    {
                        fMatch = true;
                    }
                }

                if (!fMatch)
                {
                    fDone = true;
                    // Junaeth (qPoS) does not rely on clock offset
                    //    so the warning is useles.
                    if (GetFork(nBestHeight + 1) < XST_FORKQPOS)
                    {
                        string strMessage = _(
                            "Warning: Please check that your computer's "
                            "date and time are correct! If your clock is "
                            "wrong, Stealth will not work properly.");
                        strMiscWarning = strMessage;
                        printf("*** %s\n", strMessage.c_str());
                        uiInterface.ThreadSafeMessageBox(
                            strMessage + " ",
                            string("Stealth"),
                            (CClientUIInterface::OK |
                             CClientUIInterface::ICON_EXCLAMATION));
                    }
                }
            }
        }
        if (fDebug)
        {
            BOOST_FOREACH (int64_t n, vSorted)
            {
                printf("%+" PRId64 "  ", n);
            }
            printf("|  ");
        }
        printf("nTimeOffset = %+" PRId64 "  (%+" PRId64 " minutes)\n",
               nTimeOffset,
               nTimeOffset / 60);
    }
}

size_t GetTorV3Checksum(
                const unsigned char* pchPubkey,
                unsigned char (&pchHash)[SHA3_256_DIGEST_LENGTH_])
{
    const char* pszLeader = ".onion checksum";
    const size_t nLeaderSize = strlen(pszLeader);

    const unsigned char nVersion = '\x03';

    // Leader (15 byte string) + Pubkey (32 bytes) + Version (1 byte)
    const size_t nOnionDataSize = nLeaderSize + ONION_V3_PUBKEY + 1;
    vector<unsigned char> vchOnionData(nOnionDataSize);

    memcpy(vchOnionData.data(), pszLeader, nLeaderSize);
    memcpy(vchOnionData.data() + nLeaderSize, pchPubkey, ONION_V3_PUBKEY);
    vchOnionData[nOnionDataSize - 1] = nVersion;

    return CoreHashes::SHA3_256(vchOnionData.data(), nOnionDataSize, pchHash);
}


bool IPIsTorV3(const unsigned char* pchIP)
{
    if (memcmp(pchIP, pchOnionCat, ONION_CAT_BYTES) != 0)
    {
        return false;
    }

    if (pchIP[ONION_CAT_BYTES + ONION_V3_PUBKEY + 2] != 0x03)
    {
        return false;
    }

    unsigned char pchDigest[SHA3_256_DIGEST_LENGTH_];
    if (GetTorV3Checksum(pchIP + ONION_CAT_BYTES, pchDigest) !=
        SHA3_256_DIGEST_LENGTH_)
    {
        printf("IPIsTorV3(): TSNH: couldn't SHA3-256 hash pubkey\n");
        return false;
    }

    if ((pchIP[ONION_CAT_BYTES + ONION_V3_PUBKEY] != pchDigest[0]) ||
        (pchIP[ONION_CAT_BYTES + ONION_V3_PUBKEY + 1] != pchDigest[1]))
    {
        return false;
    }

    return true;
}

string MakeTorV3Address(const unsigned char (&pchPubkey)[TOR_V3_PUBKEY_SIZE])
{
    unsigned char pchHash[SHA3_256_DIGEST_LENGTH_];
    GetTorV3Checksum(pchPubkey, pchHash);

    const unsigned char nVersion = '\x03';

    vector<unsigned char> vchAddr(ONION_V3_PUBKEY + 3);
    memcpy(vchAddr.data(), pchPubkey, ONION_V3_PUBKEY);
    memcpy(vchAddr.data() + ONION_V3_PUBKEY, pchHash, 2);
    vchAddr[ONION_V3_PUBKEY + 2] = nVersion;

    return EncodeBase32(vchAddr.data(), vchAddr.size(), false) +
              STR_ONION_SUFFIX;
}

bool static LookupIntern(const char* pszName,
                         vector<CNetAddr>& vIP,
                         unsigned int nMaxSolutions,
                         bool fAllowLookup)
{
    vIP.clear();

    {
        CNetAddr addr;
        if (addr.SetSpecial(string(pszName)))
        {
            vIP.push_back(addr);
            return true;
        }
    }

    struct addrinfo aiHint;
    memset(&aiHint, 0, sizeof(struct addrinfo));

    aiHint.ai_socktype = SOCK_STREAM;
    aiHint.ai_protocol = IPPROTO_TCP;
#ifdef WIN32
#  ifdef USE_IPV6
    aiHint.ai_family = AF_UNSPEC;
#  else
    aiHint.ai_family = AF_INET;
#  endif
    aiHint.ai_flags = fAllowLookup ? 0 : AI_NUMERICHOST;
#else
#  ifdef USE_IPV6
    aiHint.ai_family = AF_UNSPEC;
#  else
    aiHint.ai_family = AF_INET;
#  endif
    aiHint.ai_flags = fAllowLookup ? AI_ADDRCONFIG : AI_NUMERICHOST;
#endif
    struct addrinfo *aiRes = NULL;
    int nErr = getaddrinfo(pszName, NULL, &aiHint, &aiRes);
    if (nErr)
    {
        return false;
    }

    struct addrinfo *aiTrav = aiRes;
    while (aiTrav != NULL &&
           (nMaxSolutions == 0 || vIP.size() < nMaxSolutions))
    {
        if (aiTrav->ai_family == AF_INET)
        {
            assert(aiTrav->ai_addrlen >= sizeof(sockaddr_in));
            vIP.push_back(
                CNetAddr(((struct sockaddr_in*) (aiTrav->ai_addr))->sin_addr));
        }

#ifdef USE_IPV6
        if (aiTrav->ai_family == AF_INET6)
        {
            assert(aiTrav->ai_addrlen >= sizeof(sockaddr_in6));
            vIP.push_back(CNetAddr(
                ((struct sockaddr_in6*) (aiTrav->ai_addr))->sin6_addr));
        }
#endif

        aiTrav = aiTrav->ai_next;
    }

    freeaddrinfo(aiRes);

    return (vIP.size() > 0);
}

bool LookupHost(const char* pszName,
                vector<CNetAddr>& vIP,
                unsigned int nMaxSolutions,
                bool fAllowLookup)
{
    if (pszName[0] == 0)
    {
        return false;
    }
    char psz[256];
    char* pszHost = psz;
    bitcoin_strlcpy(psz, pszName, sizeof(psz));
    if (psz[0] == '[' && psz[strlen(psz) - 1] == ']')
    {
        pszHost = psz + 1;
        psz[strlen(psz) - 1] = 0;
    }

    return LookupIntern(pszHost, vIP, nMaxSolutions, fAllowLookup);
}

bool LookupHostNumeric(const char* pszName,
                       vector<CNetAddr>& vIP,
                       unsigned int nMaxSolutions)
{
    return LookupHost(pszName, vIP, nMaxSolutions, false);
}

bool Lookup(const char* pszName,
            vector<CService>& vAddr,
            int portDefault,
            bool fAllowLookup,
            unsigned int nMaxSolutions)
{
    if (pszName[0] == 0)
    {
        return false;
    }
    int port = portDefault;
    string hostname = "";
    SplitHostPort(string(pszName), port, hostname);

    vector<CNetAddr> vIP;
    bool fRet = LookupIntern(hostname.c_str(),
                             vIP,
                             nMaxSolutions,
                             fAllowLookup);
    if (!fRet)
    {
        return false;
    }
    vAddr.resize(vIP.size());
    for (unsigned int i = 0; i < vIP.size(); i++)
    {
        vAddr[i] = CService(vIP[i], port);
    }
    return true;
}

bool Lookup(const char* pszName,
            CService& addr,
            int portDefault,
            bool fAllowLookup)
{
    vector<CService> vService;
    bool fRet = Lookup(pszName, vService, portDefault, fAllowLookup, 1);
    if (!fRet)
    {
        return false;
    }
    addr = vService[0];
    return true;
}

bool LookupNumeric(const char* pszName, CService& addr, int portDefault)
{
    return Lookup(pszName, addr, portDefault, false);
}

bool static Socks4(const CService& addrDest, SOCKET& hSocket)
{
    printf("SOCKS4 connecting %s\n", addrDest.ToString().c_str());
    if (!addrDest.IsIPv4())
    {
        closesocket(hSocket);
        return error("Proxy destination is not IPv4");
    }
    char pszSocks4IP[] = "\4\1\0\0\0\0\0\0user";
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (!addrDest.GetSockAddr((struct sockaddr*) &addr, &len) ||
        addr.sin_family != AF_INET)
    {
        closesocket(hSocket);
        return error("Cannot get proxy destination address");
    }
    memcpy(pszSocks4IP + 2, &addr.sin_port, 2);
    memcpy(pszSocks4IP + 4, &addr.sin_addr, 4);
    char* pszSocks4 = pszSocks4IP;
    int nSize = sizeof(pszSocks4IP);

    int ret = send(hSocket, pszSocks4, nSize, MSG_NOSIGNAL);
    if (ret != nSize)
    {
        closesocket(hSocket);
        return error("Error sending to proxy");
    }
    char pchRet[8];
    if (recv(hSocket, pchRet, 8, 0) != 8)
    {
        closesocket(hSocket);
        return error("Error reading proxy response");
    }
    if (pchRet[1] != 0x5a)
    {
        closesocket(hSocket);
        if (pchRet[1] != 0x5b)
        {
            printf("ERROR: Proxy returned error %d\n", pchRet[1]);
        }
        return false;
    }
    printf("SOCKS4 connected %s\n", addrDest.ToString().c_str());
    return true;
}

bool static Socks5(string strDest, int port, SOCKET& hSocket)
{
    printf("SOCKS5 connecting %s\n", strDest.c_str());
    if (strDest.size() > 255)
    {
        closesocket(hSocket);
        return error("Hostname too long");
    }
    char pszSocks5Init[] = "\5\1\0";
    if (fDebugNet)
    {
        printf("Socks5(): pszSocks5Init: %s\n", pszSocks5Init);
    }
    char* pszSocks5 = pszSocks5Init;
    ssize_t nSize = sizeof(pszSocks5Init) - 1;

    ssize_t ret = send(hSocket, pszSocks5, nSize, MSG_NOSIGNAL);
    if (ret != nSize)
    {
        closesocket(hSocket);
        return error("Error sending to proxy");
    }
    char pchRet1[2];
    if (recv(hSocket, pchRet1, 2, 0) != 2)
    {
        closesocket(hSocket);
        return error("Error reading proxy response");
    }

    if (fDebugNet)
    {
        printf("Socks5(): pchRet1: %s\n", pchRet1);
    }

    if (pchRet1[0] != 0x05 || pchRet1[1] != 0x00)
    {
        closesocket(hSocket);
        return error("Proxy failed to initialize");
    }
    string strSocks5("\5\1");

    if (fDebugNet)
    {
        printf("Socks5(): strSocks5: %s\n", strSocks5.c_str());
    }

    strSocks5 += '\000';
    strSocks5 += '\003';
    strSocks5 += static_cast<char>(min((int) strDest.size(), 255));
    strSocks5 += strDest;
    strSocks5 += static_cast<char>((port >> 8) & 0xFF);
    strSocks5 += static_cast<char>((port >> 0) & 0xFF);
    ret = send(hSocket, strSocks5.c_str(), strSocks5.size(), MSG_NOSIGNAL);
    if (ret != (ssize_t) strSocks5.size())
    {
        closesocket(hSocket);
        return error("Error sending to proxy");
    }
    char pchRet2[4];
    if (recv(hSocket, pchRet2, 4, 0) != 4)
    {
        closesocket(hSocket);
        return error("Error reading proxy response");
    }
    if (fDebugNet)
    {
        printf("Socks5(): pchRet2: %s\n", pchRet2);
    }
    if (pchRet2[0] != 0x05)
    {
        closesocket(hSocket);
        return error("Proxy failed to accept request");
    }
    if (pchRet2[1] != 0x00)
    {
        closesocket(hSocket);
        switch (pchRet2[1])
        {
            case 0x01:
                return error("Proxy error: general failure");
            case 0x02:
                return error("Proxy error: connection not allowed");
            case 0x03:
                return error("Proxy error: network unreachable");
            case 0x04:
                return error("Proxy error: host unreachable");
            case 0x05:
                return error("Proxy error: connection refused");
            case 0x06:
                return error("Proxy error: TTL expired");
            case 0x07:
                return error("Proxy error: protocol error");
            case 0x08:
                return error("Proxy error: address type not supported");
            default:
                return error("Proxy error: unknown");
        }
    }
    if (pchRet2[2] != 0x00)
    {
        closesocket(hSocket);
        return error("Error: malformed proxy response");
    }
    char pchRet3[256];
    switch (pchRet2[3])
    {
        case 0x01:
            ret = recv(hSocket, pchRet3, 4, 0) != 4;
            break;
        case 0x04:
            ret = recv(hSocket, pchRet3, 16, 0) != 16;
            break;
        case 0x03:
        {
            ret = recv(hSocket, pchRet3, 1, 0) != 1;
            if (ret)
            {
                closesocket(hSocket);
                return error("Error reading from proxy");
            }
            int nRecv = pchRet3[0];
            ret = recv(hSocket, pchRet3, nRecv, 0) != nRecv;
            break;
        }
        default:
            closesocket(hSocket);
            return error("Error: malformed proxy response");
    }
    if (ret)
    {
        closesocket(hSocket);
        return error("Error reading from proxy");
    }
    if (recv(hSocket, pchRet3, 2, 0) != 2)
    {
        closesocket(hSocket);
        return error("Error reading from proxy");
    }
    printf("SOCKS5 connected %s\n", strDest.c_str());
    return true;
}

bool static ConnectSocketDirectly(const CService &addrConnect, SOCKET& hSocketRet, int nTimeout)
{
    hSocketRet = INVALID_SOCKET;

#ifdef USE_IPV6
    struct sockaddr_storage sockaddr;
#else
    struct sockaddr sockaddr;
#endif
    socklen_t len = sizeof(sockaddr);
    if (!addrConnect.GetSockAddr((struct sockaddr*) &sockaddr, &len))
    {
        printf("Cannot connect to %s: unsupported network\n",
               addrConnect.ToString().c_str());
        return false;
    }

    SOCKET hSocket = socket(((struct sockaddr*) &sockaddr)->sa_family,
                            SOCK_STREAM,
                            IPPROTO_TCP);
    if (hSocket == INVALID_SOCKET)
    {
        return false;
    }
#ifdef SO_NOSIGPIPE
    int set = 1;
    setsockopt(hSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*) &set, sizeof(int));
#endif

#ifdef WIN32
    u_long fNonblock = 1;
    if (ioctlsocket(hSocket, FIONBIO, &fNonblock) == SOCKET_ERROR)
#else
    int fFlags = fcntl(hSocket, F_GETFL, 0);
    if (fcntl(hSocket, F_SETFL, fFlags | O_NONBLOCK) == -1)
#endif
    {
        closesocket(hSocket);
        return false;
    }

    if (connect(hSocket, (struct sockaddr*)&sockaddr, len) == SOCKET_ERROR)
    {
        // WSAEINVAL is here because some legacy version of winsock uses it
        if (WSAGetLastError() == WSAEINPROGRESS ||
            WSAGetLastError() == WSAEWOULDBLOCK ||
            WSAGetLastError() == WSAEINVAL)
        {
            struct timeval timeout;
            timeout.tv_sec  = nTimeout / 1000;
            timeout.tv_usec = (nTimeout % 1000) * 1000;

            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(hSocket, &fdset);
            int nRet = select(hSocket + 1, NULL, &fdset, NULL, &timeout);
            if (nRet == 0)
            {
                printf("connection timeout\n");
                closesocket(hSocket);
                return false;
            }
            if (nRet == SOCKET_ERROR)
            {
                printf("select() for connection failed: %i\n",
                       WSAGetLastError());
                closesocket(hSocket);
                return false;
            }
            socklen_t nRetSize = sizeof(nRet);
#ifdef WIN32
            if (getsockopt(hSocket,
                           SOL_SOCKET,
                           SO_ERROR,
                           (char*) (&nRet),
                           &nRetSize) == SOCKET_ERROR)
#else
            if (getsockopt(hSocket, SOL_SOCKET, SO_ERROR, &nRet, &nRetSize) ==
                SOCKET_ERROR)
#endif
            {
                printf("getsockopt() for connection failed: %i\n",
                       WSAGetLastError());
                closesocket(hSocket);
                return false;
            }
            if (nRet != 0)
            {
                printf("connect() failed after select(): %s\n",
                       strerror(nRet));
                closesocket(hSocket);
                return false;
            }
        }
#ifdef WIN32
        else if (WSAGetLastError() != WSAEISCONN)
#else
        else
#endif
        {
            printf("connect() failed: %i\n", WSAGetLastError());
            closesocket(hSocket);
            return false;
        }
    }

    // this isn't even strictly necessary
    // CNode::ConnectNode immediately turns the socket back to non-blocking
    // but we'll turn it back to blocking just in case
#ifdef WIN32
    fNonblock = 0;
    if (ioctlsocket(hSocket, FIONBIO, &fNonblock) == SOCKET_ERROR)
#else
    fFlags = fcntl(hSocket, F_GETFL, 0);
    if (fcntl(hSocket, F_SETFL, fFlags & ~O_NONBLOCK) == SOCKET_ERROR)
#endif
    {
        closesocket(hSocket);
        return false;
    }

    hSocketRet = hSocket;
    return true;
}

bool SetProxy(enum Network net, CService addrProxy, int nSocksVersion)
{
    printf("SetProxy(): nSocksVersion is %d\n", nSocksVersion);
    assert(net >= 0 && net < NET_MAX);
    if (nSocksVersion != 0 && nSocksVersion != 4 && nSocksVersion != 5)
    {
        return false;
    }
    if (nSocksVersion != 0 && !addrProxy.IsValid())
    {
        return false;
    }
    LOCK(cs_proxyInfos);
    proxyInfo[net] = make_pair(addrProxy, nSocksVersion);
    return true;
}

bool GetProxy(enum Network net, proxyType& proxyInfoOut)
{
    if (fDebugNet)
    {
        printf("GetProxy(): net is %s\n", GetNetworkType(net));
    }
    assert(net >= 0 && net < NET_MAX);
    LOCK(cs_proxyInfos);
    if (!proxyInfo[net].second)
    {
        if (fDebugNet)
        {
            printf("GetProxy(): no proxy for %s\n", GetNetworkType(net));
        }
        return false;
    }
    proxyInfoOut = proxyInfo[net];
    return true;
}

bool SetNameProxy(CService addrProxy, int nSocksVersion)
{
    if (nSocksVersion != 0 && nSocksVersion != 5)
    {
        return false;
    }
    if (nSocksVersion != 0 && !addrProxy.IsValid())
    {
        return false;
    }
    LOCK(cs_proxyInfos);
    nameproxyInfo = make_pair(addrProxy, nSocksVersion);
    return true;
}

bool GetNameProxy(proxyType& nameproxyInfoOut)
{
    LOCK(cs_proxyInfos);
    if (!nameproxyInfo.second)
    {
        return false;
    }
    nameproxyInfoOut = nameproxyInfo;
    return true;
}

bool HaveNameProxy()
{
    LOCK(cs_proxyInfos);
    return nameproxyInfo.second != 0;
}

bool IsProxy(const CNetAddr& addr)
{
    LOCK(cs_proxyInfos);
    for (int i = 0; i < NET_MAX; i++)
    {
        if (proxyInfo[i].second && (addr == (CNetAddr) proxyInfo[i].first))
        {
            return true;
        }
    }
    return false;
}

bool ConnectSocket(const CService& addrDest, SOCKET& hSocketRet, int nTimeout)
{
    if (fDebugNet)
    {
        printf("ConnectSocket(): attempting %s\n",
               addrDest.ToString().c_str());
    }
    proxyType proxy;

    // no proxy needed
    if (!GetProxy(addrDest.GetNetwork(), proxy))
    {
        return ConnectSocketDirectly(addrDest, hSocketRet, nTimeout);
    }

    if (fDebugNet)
    {
        printf("ConnectSocket(): proxy\n");
    }

    SOCKET hSocket = INVALID_SOCKET;

    // first connect to proxy server
    if (!ConnectSocketDirectly(proxy.first, hSocket, nTimeout))
    {
        return false;
    }

    if (fDebugNet)
    {
        printf("ConnectSocket(): connected %s to proxy (%d)\n",
               addrDest.ToString().c_str(),
               proxy.second);
    }

    // do socks negotiation
    switch (proxy.second)
    {
        case 4:
            if (!Socks4(addrDest, hSocket))
            {
                return false;
            }
            if (fDebugNet)
            {
                printf("ConnectSocket(): connected Socks 5\n");
            }
            break;
        case 5:
            if (!Socks5(addrDest.ToStringIP(), addrDest.GetPort(), hSocket))
            {
                if (fDebugNet)
                {
                    printf("ConnectSocket(): Socks 5 failed %s\n",
                           addrDest.ToString().c_str());
                }
                return false;
            }
            if (fDebugNet)
            {
                printf("ConnectSocket(): connected Socks 5\n");
            }
            break;
        default:
            if (fDebugNet)
            {
                printf("ConnectSocket(): second is %d\n", proxy.second);
            }
            return false;
    }

    if (fDebugNet)
    {
        printf("ConnectSocket(): connection succeeded\n");
    }

    hSocketRet = hSocket;
    return true;
}

bool ConnectSocketByName(CService& addr,
                         SOCKET& hSocketRet,
                         const char* pszDest,
                         int portDefault,
                         int nTimeout)
{
    if (fDebugNet)
    {
        printf("ConnectSocketByName(): attempting \"%s\"\n", pszDest);
    }
    string strDest;
    int port = portDefault;
    SplitHostPort(string(pszDest), port, strDest);

    SOCKET hSocket = INVALID_SOCKET;

    proxyType nameproxy;
    GetNameProxy(nameproxy);

    CService addrResolved(CNetAddr(strDest,
                                   fNameLookup && !nameproxy.second),
                          port);

    if (fDebugNet)
    {
        printf("ConnectSocketByName(): Resolved \"%s\"   from %s\n%s\n",
               addrResolved.ToString().c_str(),
               strDest.c_str(),
               addrResolved.GetHex(16, "      ").c_str());
    }

    if (addrResolved.IsValid())
    {
        addr = addrResolved;
        printf("ConnectSocketByName(): address resolved: %s\n",
               addr.ToString().c_str());
        return ConnectSocket(addr, hSocketRet, nTimeout);
    }
    else
    {
        printf("ConnectSocketByName(): destination not resolved: %s\n", pszDest);
    }
    addr = CSERVICE_NULL;
    if (!nameproxy.second)
    {
        return false;
    }
    printf("ConnectSocketByName(): does have second name proxy\n");
    if (!ConnectSocketDirectly(nameproxy.first, hSocket, nTimeout))
    {
        return false;
    }

    printf("ConnectSocketByName(): connected second name proxy\n");

    switch (nameproxy.second)
    {
        default:
        case 4:
            return false;
        case 5:
            if (!Socks5(strDest, port, hSocket))
            {
                return false;
            }
            printf("ConnectSocketByName(): connected socks 5\n");
            break;
    }

    hSocketRet = hSocket;
    return true;
}

void CNetAddr::InitIP()
{
    assert (NFILL >= 16);
    memset(ip, 0, NFILL);
    memcpy(ip + NFILL, PCH_IP64_MARKER, IP64_MARKER_SIZE);
}

void CNetAddr::SetIP(const CNetAddr& ipIn)
{
    // don't pave over the ip64 marker
    memcpy(ip, ipIn.ip, NFILL);
}

void CNetAddr::SetIPv4Null()
{
    InitIP();
    memcpy(ip, pchIPv4, 12);
}

void CNetAddr::SetTorV3Placeholder()
{
    memcpy(ip, pchOnionCat, ONION_CAT_BYTES);
    memset(ip + ONION_CAT_BYTES, 0, 7);
    // If we have a checksum, it will be in the 33rd and 34th bytes.
    // Copy those to the 13th and 14th bytes.
    memcpy(ip + 13, ip + 32, 2);
    // Set version byte for placeholder explicitly.
    ip[15] = 3;
}

void CNetAddr::MarkIP64()
{
    memcpy(ip + NFILL, PCH_IP64_MARKER, IP64_MARKER_SIZE);
}

bool CNetAddr::SetSpecial(const string &strName)
{

    // tor addresses: remove suffix, prepend onion cat, store to ip
    int nSuffixStart;

    nSuffixStart = strName.size() - ONION_SUFFIX;
    if ((nSuffixStart > 0) &&
        (strName.substr(nSuffixStart, ONION_SUFFIX) == STR_ONION_SUFFIX))
    {
        string strAddr = strName.substr(0, nSuffixStart);
        vector<unsigned char> vchAddr = DecodeBase32(strAddr.c_str());
        if (vchAddr.size() != ONION_ADDRESS)
        {
            return false;
        }
        memcpy(ip, pchOnionCat, ONION_CAT_BYTES);
        for (unsigned int i=0; i < ONION_ADDRESS; i++)
        {
            ip[i + ONION_CAT_BYTES] = vchAddr[i];
        }
        return true;
    }

    // i2p addresses: remove suffix, prepend onion cat, store to ip
    nSuffixStart = strName.size() - GARLIC_SUFFIX;
    if ((nSuffixStart > 0) &&
        (strName.substr(nSuffixStart, GARLIC_SUFFIX) == STR_GARLIC_SUFFIX))
    {
        string strAddr = strName.substr(0, nSuffixStart);
        vector<unsigned char> vchAddr = DecodeBase32(strAddr.c_str());
        if (vchAddr.size() != GARLIC_ADDRESS)
        {
            return false;
        }
        memcpy(ip, pchOnionCat, GARLIC_CAT_BYTES);
        for (unsigned int i=0; i < GARLIC_ADDRESS; i++)
        {
            ip[i + GARLIC_CAT_BYTES] = vchAddr[i];
        }
        return true;
    }
    return false;
}

CNetAddr::CNetAddr()
{
    InitIP();
}

CNetAddr::CNetAddr(const struct in_addr& ipv4Addr)
{
    InitIP();
    memcpy(ip,    pchIPv4, 12);
    memcpy(ip+12, &ipv4Addr, 4);
}

#ifdef USE_IPV6
CNetAddr::CNetAddr(const struct in6_addr& ipv6Addr)
{
    InitIP();
    memcpy(ip, &ipv6Addr, 16);
}
#endif

void CNetAddr::Set(const char* pszIp, bool fAllowLookup)
{
    vector<CNetAddr> vIP;
    if (LookupHost(pszIp, vIP, 1, fAllowLookup))
    {
        *this = vIP[0];
    }
}

CNetAddr::CNetAddr(const char* pszIp, bool fAllowLookup)
{
    Set(pszIp, fAllowLookup);
}

CNetAddr::CNetAddr(const string& strIp, bool fAllowLookup)
{
    Set(strIp.c_str(), fAllowLookup);
}

unsigned int CNetAddr::GetByte(int n) const
{
    return ip[15-n];
}

bool CNetAddr::IsIPv4() const
{
    return (memcmp(ip, pchIPv4, sizeof(pchIPv4)) == 0);
}

bool CNetAddr::IsIPv6() const
{
    return (!IsIPv4() && !IsTor() && !IsI2P());
}

bool CNetAddr::IsRFC1918() const
{
    return IsIPv4() && (
        GetByte(3) == 10 ||
        (GetByte(3) == 192 && GetByte(2) == 168) ||
        (GetByte(3) == 172 && (GetByte(2) >= 16 && GetByte(2) <= 31)));
}

bool CNetAddr::IsRFC3927() const
{
    return IsIPv4() && (GetByte(3) == 169 && GetByte(2) == 254);
}

bool CNetAddr::IsRFC3849() const
{
    return GetByte(15) == 0x20 && GetByte(14) == 0x01 && GetByte(13) == 0x0D &&
           GetByte(12) == 0xB8;
}

bool CNetAddr::IsRFC3964() const
{
    return (GetByte(15) == 0x20 && GetByte(14) == 0x02);
}

bool CNetAddr::IsRFC6052() const
{
    static const unsigned char pchRFC6052[] = { 0, 0x64, 0xFF, 0x9B, 0, 0,
                                                0, 0,    0,    0,    0, 0 };
    return (memcmp(ip, pchRFC6052, sizeof(pchRFC6052)) == 0);
}

bool CNetAddr::IsRFC4380() const
{
    return (GetByte(15) == 0x20 && GetByte(14) == 0x01 && GetByte(13) == 0 &&
            GetByte(12) == 0);
}

bool CNetAddr::IsRFC4862() const
{
    static const unsigned char pchRFC4862[] = { 0xFE, 0x80, 0, 0, 0, 0, 0, 0 };
    return (memcmp(ip, pchRFC4862, sizeof(pchRFC4862)) == 0);
}

bool CNetAddr::IsRFC4193() const
{
    return ((GetByte(15) & 0xFE) == 0xFC);
}

bool CNetAddr::IsRFC6145() const
{
    static const unsigned char pchRFC6145[] = { 0, 0, 0,    0,    0, 0,
                                                0, 0, 0xFF, 0xFF, 0, 0 };
    return (memcmp(ip, pchRFC6145, sizeof(pchRFC6145)) == 0);
}

bool CNetAddr::IsRFC4843() const
{
    return (GetByte(15) == 0x20 && GetByte(14) == 0x01 &&
            GetByte(13) == 0x00 && (GetByte(12) & 0xF0) == 0x10);
}

bool CNetAddr::IsTor() const
{
    return (memcmp(ip, pchOnionCat, ONION_CAT_BYTES) == 0);
}

bool CNetAddr::IsTorV3() const
{
    if (sizeof(ip) < ONION_V3_BYTES)
    {
        return false;
    }
    return IPIsTorV3(ip);
}

bool CNetAddr::IsTorV3Placeholder() const
{
    if (memcmp(ip, pchOnionCat, ONION_CAT_BYTES) != 0)
    {
        return false;
    }
    for (size_t i = ONION_CAT_BYTES; i < ONION_CAT_BYTES + 7; ++i)
    {
        if (ip[i] != 0)
        {
            return false;
        }
    }
    return ip[15] == 3;
}

bool CNetAddr::IsI2P() const
{
    return (memcmp(ip, pchGarlicCat, sizeof(pchGarlicCat)) == 0);
}

bool CNetAddr::IsMarkedIP64() const
{
   return memcmp(ip + IP64_MARKER_START,
                 PCH_IP64_MARKER,
                 IP64_MARKER_SIZE) == 0;
}


bool CNetAddr::IsLocal() const
{
    // IPv4 loopback
    if (IsIPv4() && (GetByte(3) == 127 || GetByte(3) == 0))
    {
        return true;
    }

   // IPv6 loopback (::1/128)
    static const unsigned char pchLocal[16] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                                0, 0, 0, 0, 0, 0, 0, 1 };
    if (memcmp(ip, pchLocal, 16) == 0)
    {
        return true;
    }

   return false;
}

bool CNetAddr::IsMulticast() const
{
    return (IsIPv4() && (GetByte(3) & 0xF0) == 0xE0) || (GetByte(15) == 0xFF);
}

bool CNetAddr::IsValid(int nVersionCheck) const
{
    if (fDebugNet)
    {
        printf("Checking address: %s\n%s\n",
               this->ToString().c_str(),
               this->GetHex(16, "    ").c_str());
    }

    // valid Tor addresses require 64 byte ip
    if (IsTor())
    {
        if (nVersionCheck < CADDR_IP64_VERSION)
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    // Since it's not Tor, make sure it's zero-ed out between
    // byte 16 and 45, exclusively
    for (int i = 16; i < IP64_MARKER_START; ++i)
    {
        if (ip[i] != 0)
        {
            return false;
        }
    }

    // Cleanup 3-byte shifted addresses caused by garbage in size field
    // of addr messages from versions before 0.2.9 checksum.
    // Two consecutive addr messages look like this:
    // header20 vectorlen3 addr26 addr26 addr26 header20 vectorlen3 addr26
    // addr26 addr26... so if the first length field is garbled, it reads the
    // second batch of addr misaligned by 3 bytes.
    if (memcmp(ip, pchIPv4 + 3, sizeof(pchIPv4) - 3) == 0)
    {
        return false;
    }

    // unspecified IPv6 address (::/128)
    unsigned char ipNone[16] = {};
    if (memcmp(ip, ipNone, 16) == 0)
    {
        return false;
    }

    // documentation IPv6 address
    if (IsRFC3849())
    {
        return false;
    }

    if (IsIPv4())
    {
        // INADDR_NONE
        uint32_t ipNone = INADDR_NONE;
        if (memcmp(ip + 12, &ipNone, 4) == 0)
        {
            return false;
        }

        // 0
        ipNone = 0;
        if (memcmp(ip + 12, &ipNone, 4) == 0)
        {

            return false;
        }
    }

    return true;
}

bool CNetAddr::IsRoutable() const
{
    return IsValid() && !(IsRFC1918() || IsRFC3927() || IsRFC4862() ||
                          (IsRFC4193() && !IsTorV3() && !IsI2P()) ||
                          IsRFC4843() || IsLocal());
}

enum Network CNetAddr::GetNetwork() const
{
    if (!IsRoutable())
    {
        // catches Tor <v3
        return NET_UNROUTABLE;
    }

    if (IsIPv4())
    {
        return NET_IPV4;
    }

    if (IsTorV3())
    {
        return NET_TOR;
    }

    if (IsI2P())
    {
        return NET_I2P;
    }

    return NET_IPV6;
}

string CNetAddr::ToStringIP() const
{
    if (IsTorV3())
    {
        return EncodeBase32(&ip[ONION_CAT_BYTES], ONION_ADDRESS) +
               STR_ONION_SUFFIX;
    }
    if (IsI2P())
    {
        return EncodeBase32(&ip[GARLIC_CAT_BYTES], GARLIC_ADDRESS) +
               STR_GARLIC_SUFFIX;
    }
    CService serv(*this, 0);
#ifdef USE_IPV6
    struct sockaddr_storage sockaddr;
#else
    struct sockaddr sockaddr;
#endif
    socklen_t socklen = sizeof(sockaddr);
    if (serv.GetSockAddr((struct sockaddr*) &sockaddr, &socklen))
    {
        char name[1025] = "";
        if (!getnameinfo((const struct sockaddr*) &sockaddr,
                         socklen,
                         name,
                         sizeof(name),
                         NULL,
                         0,
                         NI_NUMERICHOST))
        {
            return string(name);
        }
    }
    if (IsIPv4())
    {
        return strprintf("%u.%u.%u.%u",
                         GetByte(3),
                         GetByte(2),
                         GetByte(1),
                         GetByte(0));
    }
    else
    {
        return strprintf("%x:%x:%x:%x:%x:%x:%x:%x",
                         GetByte(15) << 8 | GetByte(14),
                         GetByte(13) << 8 | GetByte(12),
                         GetByte(11) << 8 | GetByte(10),
                         GetByte(9)  << 8 | GetByte(8),
                         GetByte(7)  << 8 | GetByte(6),
                         GetByte(5)  << 8 | GetByte(4),
                         GetByte(3)  << 8 | GetByte(2),
                         GetByte(1)  << 8 | GetByte(0));
    }
}

string CNetAddr::ToString() const
{
    return ToStringIP();
}

string CNetAddr::GetHex(size_t nChunk, const string& strIndent) const
{
    string strHex = HexStr(ip, ip + 64, true);
    if ((nChunk > 0) || (strIndent != ""))
    {
        return ChunkHex(strHex, nChunk, strIndent);
    }
    return strHex;
}


bool operator==(const CNetAddr& a, const CNetAddr& b)
{
    return (memcmp(a.ip, b.ip, 16) == 0);
}

bool operator!=(const CNetAddr& a, const CNetAddr& b)
{
    return (memcmp(a.ip, b.ip, 16) != 0);
}

bool operator<(const CNetAddr& a, const CNetAddr& b)
{
    return (memcmp(a.ip, b.ip, 16) < 0);
}

bool CNetAddr::GetInAddr(struct in_addr* pipv4Addr) const
{
    if (!IsIPv4())
    {
        return false;
    }
    memcpy(pipv4Addr, ip + 12, 4);
    return true;
}

#ifdef USE_IPV6
bool CNetAddr::GetIn6Addr(struct in6_addr* pipv6Addr) const
{
    memcpy(pipv6Addr, ip, 16);
    return true;
}
#endif

// get canonical identifier of an address' group
// no two connections will be attempted to addresses with the same group
vector<unsigned char> CNetAddr::GetGroup() const
{
    vector<unsigned char> vchRet;
    int nClass = NET_IPV6;
    int nStartByte = 0;
    int nBits = 16;

    // all local addresses belong to the same group
    if (IsLocal())
    {
        nClass = 255;
        nBits = 0;
    }

    // all unroutable addresses belong to the same group
    if (!IsRoutable())
    {
        nClass = NET_UNROUTABLE;
        nBits = 0;
    }
    // for IPv4 addresses, '1' + the 16 higher-order bits of the IP
    // includes mapped IPv4, SIIT translated IPv4, and the well-known prefix
    else if (IsIPv4() || IsRFC6145() || IsRFC6052())
    {
        nClass = NET_IPV4;
        nStartByte = 12;
    }
    // for 6to4 tunnelled addresses, use the encapsulated IPv4 address
    else if (IsRFC3964())
    {
        nClass = NET_IPV4;
        nStartByte = 2;
    }
    // for Teredo-tunnelled IPv6 addresses, use the encapsulated IPv4 address
    else if (IsRFC4380())
    {
        vchRet.push_back(NET_IPV4);
        vchRet.push_back(GetByte(3) ^ 0xFF);
        vchRet.push_back(GetByte(2) ^ 0xFF);
        return vchRet;
    }
    else if (IsTorV3())
    {
        nClass = NET_TOR;
        nStartByte = ONION_CAT_BYTES;
        nBits = 4;
    }
    else if (IsI2P())
    {
        nClass = NET_I2P;
        nStartByte = GARLIC_CAT_BYTES;
        nBits = 4;
    }
    // for he.net, use /36 groups
    else if (GetByte(15) == 0x20 && GetByte(14) == 0x11 &&
             GetByte(13) == 0x04 && GetByte(12) == 0x70)
    {
        nBits = 36;
    }
    // for the rest of the IPv6 network, use /32 groups
    else
    {
        nBits = 32;
    }

    vchRet.push_back(nClass);
    while (nBits >= 8)
    {
        vchRet.push_back(GetByte(15 - nStartByte));
        nStartByte++;
        nBits -= 8;
    }
    if (nBits > 0)
    {
        vchRet.push_back(GetByte(15 - nStartByte) | ((1 << nBits) - 1));
    }

    return vchRet;
}

uint64_t CNetAddr::GetHash() const
{
    uint256 hash = Hash(&ip[0], &ip[16]);
    uint64_t nRet;
    memcpy(&nRet, &hash, sizeof(nRet));
    return nRet;
}

void CNetAddr::print() const
{
    printf("CNetAddr(%s)\n", ToString().c_str());
}

// private extensions to enum Network, only returned by GetExtNetwork,
// and only used in GetReachabilityFrom
static const int NET_UNKNOWN = NET_MAX + 0;
static const int NET_TEREDO = NET_MAX + 1;
int static GetExtNetwork(const CNetAddr* addr)
{
    if (addr == NULL)
    {
        return NET_UNKNOWN;
    }
    if (addr->IsRFC4380())
    {
        return NET_TEREDO;
    }
    return addr->GetNetwork();
}

/** Calculates a metric for how reachable (*this) is from a given partner */
int CNetAddr::GetReachabilityFrom(const CNetAddr* paddrPartner) const
{
    enum Reachability
    {
        REACH_UNREACHABLE,
        REACH_DEFAULT,
        REACH_TEREDO,
        REACH_IPV6_WEAK,
        REACH_IPV4,
        REACH_IPV6_STRONG,
        REACH_PRIVATE
    };

    if (!IsRoutable())
    {
        return REACH_UNREACHABLE;
    }

    int ourNet = GetExtNetwork(this);
    int theirNet = GetExtNetwork(paddrPartner);

    if (fDebugNet)
    {
        printf("ourNet: %s\n", GetNetworkType((Network)ourNet));
        printf("    us: %s\n%s\n",
               this->ToString().c_str(),
               this->GetHex(16, "    ").c_str());
        printf("theirNet: %s\n", GetNetworkType((Network)theirNet));
        printf("    them: %s\n%s\n",
               paddrPartner->ToString().c_str(),
               paddrPartner->GetHex(16, "    ").c_str());
    }

    bool fTunnel = IsRFC3964() || IsRFC6052() || IsRFC6145();

    switch (theirNet)
    {
        case NET_IPV4:
            switch (ourNet)
            {
                default:
                    return REACH_DEFAULT;
                case NET_IPV4:
                    return REACH_IPV4;
            }
        case NET_IPV6:
            switch (ourNet)
            {
                default:
                    return REACH_DEFAULT;
                case NET_TEREDO:
                    return REACH_TEREDO;
                case NET_IPV4:
                    return REACH_IPV4;
                case NET_IPV6:
                    // only prefer giving our IPv6 address
                    //    if it isn't tunnelled
                    return fTunnel ? REACH_IPV6_WEAK
                                   : REACH_IPV6_STRONG;
            }
        case NET_TOR:
            switch (ourNet)
            {
                default:
                    return REACH_DEFAULT;
                case NET_IPV4:
                    // Tor users can connect to IPv4 as well
                    return REACH_IPV4;
                case NET_TOR:
                    return REACH_PRIVATE;
            }
        case NET_I2P:
            switch (ourNet)
            {
                default:
                    return REACH_DEFAULT;
                case NET_I2P:
                    return REACH_PRIVATE;
            }
        case NET_TEREDO:
            switch (ourNet)
            {
                default:
                    return REACH_DEFAULT;
                case NET_TEREDO:
                    return REACH_TEREDO;
                case NET_IPV6:
                    return REACH_IPV6_WEAK;
                case NET_IPV4:
                    return REACH_IPV4;
            }
        case NET_UNKNOWN:
        case NET_UNROUTABLE:
        default:
            switch (ourNet)
            {
                default:
                    return REACH_DEFAULT;
                case NET_TEREDO:
                    return REACH_TEREDO;
                case NET_IPV6:
                    return REACH_IPV6_WEAK;
                case NET_IPV4:
                    return REACH_IPV4;
                case NET_I2P:
                    // assume connections from unroutable addresses are I2P
                    return REACH_PRIVATE;
                case NET_TOR:
                    // either from Tor/I2P or don't care about our address
                    return REACH_PRIVATE;
            }
    }
}

void CService::Init()
{
    port = 0;
    InitIP();
}

CService::CService()
{
    Init();
}

CService::CService(const CNetAddr& cip, unsigned short portIn) :
    CNetAddr(cip), port(portIn)
{
}

CService::CService(const struct in_addr& ipv4Addr, unsigned short portIn) :
    CNetAddr(ipv4Addr), port(portIn)
{
}

#ifdef USE_IPV6
CService::CService(const struct in6_addr& ipv6Addr, unsigned short portIn) :
    CNetAddr(ipv6Addr), port(portIn)
{
}
#endif

CService::CService(const struct sockaddr_in& addr) :
    CNetAddr(addr.sin_addr), port(ntohs(addr.sin_port))
{
    assert(addr.sin_family == AF_INET);
}

#ifdef USE_IPV6
CService::CService(const struct sockaddr_in6 &addr) :
    CNetAddr(addr.sin6_addr), port(ntohs(addr.sin6_port))
{
   assert(addr.sin6_family == AF_INET6);
}
#endif

bool CService::SetSockAddr(const struct sockaddr* paddr)
{
    switch (paddr->sa_family)
    {
        case AF_INET:
            *this = CService(*(const struct sockaddr_in*) paddr);
            return true;
#ifdef USE_IPV6
        case AF_INET6:
            *this = CService(*(const struct sockaddr_in6*) paddr);
            return true;
#endif
        default:
            return false;
    }
}

CService::CService(const char* pszIpPort, bool fAllowLookup)
{
    Init();
    CService ip;
    if (Lookup(pszIpPort, ip, 0, fAllowLookup))
    {
        *this = ip;
    }
}

CService::CService(const char* pszIpPort, int portDefault, bool fAllowLookup)
{
    Init();
    CService ip;
    if (Lookup(pszIpPort, ip, portDefault, fAllowLookup))
    {
        *this = ip;
    }
}

CService::CService(const string& strIpPort, bool fAllowLookup)
{
    Init();
    CService ip;
    if (Lookup(strIpPort.c_str(), ip, 0, fAllowLookup))
    {
        *this = ip;
    }
}

CService::CService(const string& strIpPort, int portDefault, bool fAllowLookup)
{
    Init();
    CService ip;
    if (Lookup(strIpPort.c_str(), ip, portDefault, fAllowLookup))
    {
        *this = ip;
    }
}

unsigned short CService::GetPort() const
{
    return port;
}

bool operator==(const CService& a, const CService& b)
{
    return (CNetAddr) a == (CNetAddr) b && a.port == b.port;
}

bool operator!=(const CService& a, const CService& b)
{
    return (CNetAddr) a != (CNetAddr) b || a.port != b.port;
}

bool operator<(const CService& a, const CService& b)
{
    return (CNetAddr) a < (CNetAddr) b ||
           ((CNetAddr) a == (CNetAddr) b && a.port < b.port);
}

bool CService::GetSockAddr(struct sockaddr* paddr, socklen_t* addrlen) const
{
    if (IsIPv4())
    {
        if (*addrlen < (socklen_t) sizeof(struct sockaddr_in))
        {
            return false;
        }
        *addrlen = sizeof(struct sockaddr_in);
        struct sockaddr_in* paddrin = (struct sockaddr_in*) paddr;
        memset(paddrin, 0, *addrlen);
        if (!GetInAddr(&paddrin->sin_addr))
        {
            return false;
        }
        paddrin->sin_family = AF_INET;
        paddrin->sin_port = htons(port);
        return true;
    }
#ifdef USE_IPV6
    if (IsIPv6())
    {
        if (*addrlen < (socklen_t) sizeof(struct sockaddr_in6))
        {
            return false;
        }
        *addrlen = sizeof(struct sockaddr_in6);
        struct sockaddr_in6* paddrin6 = (struct sockaddr_in6*) paddr;
        memset(paddrin6, 0, *addrlen);
        if (!GetIn6Addr(&paddrin6->sin6_addr))
        {
            return false;
        }
        paddrin6->sin6_family = AF_INET6;
        paddrin6->sin6_port = htons(port);
        return true;
    }
#endif
    return false;
}

vector<unsigned char> CService::GetKey() const
{
     vector<unsigned char> vKey;
     vKey.resize(18);
     memcpy(&vKey[0], ip, 16);
     vKey[16] = port / 0x100;
     vKey[17] = port & 0x0FF;
     return vKey;
}

string CService::ToStringPort() const
{
    return strprintf("%u", port);
}

string CService::ToStringIPPort() const
{
    if (IsIPv4() || IsTorV3() || IsI2P())
    {
        return ToStringIP() + ":" + ToStringPort();
    }
    else
    {
        return "[" + ToStringIP() + "]:" + ToStringPort();
    }
}

string CService::ToString() const
{
    return ToStringIPPort();
}

void CService::print() const
{
    printf("CService(%s)\n", ToString().c_str());
}

void CService::SetPort(unsigned short portIn)
{
    port = portIn;
}
