// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_NETBASE_H
#define BITCOIN_NETBASE_H

#include <string>
#include <vector>

#include "serialize.h"
#include "compat.h"


#ifdef WIN32
// In MSVC, this is defined as a macro, undefine it to prevent a compile and link error
#undef SetPort
#endif

#define TOR_V3_PUBKEY_SIZE 32
#define IP64_MARKER_SIZE 20

enum Network
{
    NET_UNROUTABLE,
    NET_IPV4,
    NET_IPV6,
    NET_TOR,
    NET_I2P,

    NET_MAX
};

enum _Networks
{
    NO_NETWORK          =      0,
    UNROUTABLE_NETWORK  = 1 << 0,
    IPV4_NETWORK        = 1 << 1,
    IPV6_NETWORK        = 1 << 2,
    IP_NETWORK          = IPV4_NETWORK | IPV6_NETWORK,
    TOR_NETWORK         = 1 << 3,
    I2P_NETWORK         = 1 << 4
};

class CNetAddr;
class CService;

extern const char PSZ_NULL_IP[];
extern const CService CSERVICE_NULL;

extern const unsigned char PCH_IP64_MARKER[IP64_MARKER_SIZE];

extern bool fNetworks;

extern int nConnectTimeout;
extern bool fNameLookup;

// This should be 64 or greater. Change with extreme care.
#define CNETADDR_IP_SIZE 64
#define IP64_MARKER_START (CNETADDR_IP_SIZE - IP64_MARKER_SIZE)

/** IP address (IPv6, or IPv4 using mapped IPv6 range (::FFFF:0:0/96)) */
class CNetAddr
{
    protected:
        unsigned char ip[CNETADDR_IP_SIZE]; // in network byte order

    public:
        CNetAddr();
        CNetAddr(const struct in_addr& ipv4Addr);
        void Set(const char *pszIp, bool fAllowLookup = false);
        const unsigned char* Get() const { return ip; };
        explicit CNetAddr(const char *pszIp, bool fAllowLookup = false);
        explicit CNetAddr(const std::string &strIp, bool fAllowLookup = false);
        void InitIP();
        void SetIP(const CNetAddr& ip);
        void SetIPv4Null();
        void SetTorV3Placeholder();
        void MarkIP64();
        bool SetSpecial(const std::string &strName); // for Tor and I2P addresses
        bool IsIPv4() const;    // IPv4 mapped address (::FFFF:0:0/96, 0.0.0.0/0)
        bool IsIPv6() const;    // IPv6 address (not mapped IPv4, not Tor/I2P)
        bool IsRFC1918() const; // IPv4 private networks (10.0.0.0/8, 192.168.0.0/16, 172.16.0.0/12)
        bool IsRFC3849() const; // IPv6 documentation address (2001:0DB8::/32)
        bool IsRFC3927() const; // IPv4 autoconfig (169.254.0.0/16)
        bool IsRFC3964() const; // IPv6 6to4 tunnelling (2002::/16)
        bool IsRFC4193() const; // IPv6 unique local (FC00::/15)
        bool IsRFC4380() const; // IPv6 Teredo tunnelling (2001::/32)
        bool IsRFC4843() const; // IPv6 ORCHID (2001:10::/28)
        bool IsRFC4862() const; // IPv6 autoconfig (FE80::/64)
        bool IsRFC6052() const; // IPv6 well-known prefix (64:FF9B::/96)
        bool IsRFC6145() const; // IPv6 IPv4-translated address (::FFFF:0:0:0/96)
        bool IsTor() const;
        bool IsTorV3() const;
        bool IsTorV3Placeholder() const;
        bool IsI2P() const;
        bool IsMarkedIP64() const;
        bool IsLocal() const;
        bool IsRoutable() const;
        bool IsValid(int nVersionCheck =
                              std::numeric_limits<int>::max()) const;
        bool IsMulticast() const;
        enum Network GetNetwork() const;
        std::string ToString() const;
        std::string ToStringIP() const;
        std::string GetHex(size_t nChunk = 0,
                           const std::string& strIndent = "") const;
        unsigned int GetByte(int n) const;
        uint64_t GetHash() const;
        bool GetInAddr(struct in_addr* pipv4Addr) const;
        std::vector<unsigned char> GetGroup() const;
        int GetReachabilityFrom(const CNetAddr *paddrPartner = NULL) const;
        void print() const;

#ifdef USE_IPV6
        CNetAddr(const struct in6_addr& pipv6Addr);
        bool GetIn6Addr(struct in6_addr* pipv6Addr) const;
#endif

        friend bool operator==(const CNetAddr& a, const CNetAddr& b);
        friend bool operator!=(const CNetAddr& a, const CNetAddr& b);
        friend bool operator<(const CNetAddr& a, const CNetAddr& b);

        IMPLEMENT_SERIALIZE
            (
             unsigned char* pip = const_cast<unsigned char*>(this->ip);
             if (nVersion < CADDR_IP64_VERSION)
             {
                 unsigned char ip_16[16];
                 if (fRead)
                 {
                     READWRITE(FLATDATA(ip_16));
                     for (int i = 0; i < 64; ++i)
                     {
                         if (i < 16)
                         {
                             pip[i] = ip_16[i];
                         }
                         else if (i < IP64_MARKER_START)
                         {
                             pip[i] = 0;
                         }
                         else if (i < (IP64_MARKER_START + IP64_MARKER_SIZE))
                         {
                             pip[i] = PCH_IP64_MARKER[i - IP64_MARKER_START];
                         }
                     }
                 }
                 else
                 {
                     memcpy(ip_16, ip, 16);
                     READWRITE(FLATDATA(ip_16));
                 }
             }
             else
             {
                 READWRITE(FLATDATA(ip));
             }
            )
};

/** A combination of a network address (CNetAddr) and a (TCP) port */
class CService : public CNetAddr
{
    protected:
        unsigned short port; // host order

    public:
        CService();
        CService(const CNetAddr& ip, unsigned short port);
        CService(const struct in_addr& ipv4Addr, unsigned short port);
        CService(const struct sockaddr_in& addr);
        explicit CService(const char *pszIpPort, int portDefault, bool fAllowLookup = false);
        explicit CService(const char *pszIpPort, bool fAllowLookup = false);
        explicit CService(const std::string& strIpPort, int portDefault, bool fAllowLookup = false);
        explicit CService(const std::string& strIpPort, bool fAllowLookup = false);
        void Init();
        void SetPort(unsigned short portIn);
        unsigned short GetPort() const;
        bool GetSockAddr(struct sockaddr* paddr, socklen_t *addrlen) const;
        bool SetSockAddr(const struct sockaddr* paddr);
        friend bool operator==(const CService& a, const CService& b);
        friend bool operator!=(const CService& a, const CService& b);
        friend bool operator<(const CService& a, const CService& b);
        std::vector<unsigned char> GetKey() const;
        std::string ToString() const;
        std::string ToStringPort() const;
        std::string ToStringIPPort() const;
        void print() const;

#ifdef USE_IPV6
        CService(const struct in6_addr& ipv6Addr, unsigned short port);
        CService(const struct sockaddr_in6& addr);
#endif

        IMPLEMENT_SERIALIZE
            (
             CService* pthis = const_cast<CService*>(this);
             if (nVersion < CADDR_IP64_VERSION)
             {
                 unsigned char ip_16[16];
                 if (fRead)
                 {
                     READWRITE(FLATDATA(ip_16));
                     for (int i = 0; i < 64; ++i)
                     {
                         if (i < 16)
                         {
                             pthis->ip[i] = ip_16[i];
                         }
                         else if (i < IP64_MARKER_START)
                         {
                             pthis->ip[i] = 0;
                         }
                         else if (i < (IP64_MARKER_START + IP64_MARKER_SIZE))
                         {
                             pthis->ip[i] = PCH_IP64_MARKER[i - IP64_MARKER_START];
                         }
                     }
                 }
                 else
                 {
                     memcpy(ip_16, ip, 16);
                     READWRITE(FLATDATA(ip_16));
                 }
             }
             else
             {
                 READWRITE(FLATDATA(ip));
             }
             unsigned short portN = htons(port);
             READWRITE(portN);
             if (fRead)
             {
                 pthis->port = ntohs(portN);
             }
            )
};

typedef std::pair<CService, int> proxyType;

int64_t GetAdjustedTime();
void AddTimeData(const CNetAddr& ip, int64_t nTime);

const char* GetNetworkType(Network n);
enum Network ParseNetwork(std::string net);
void SplitHostPort(std::string in, int &portOut, std::string &hostOut);
bool SetProxy(enum Network net, CService addrProxy, int nSocksVersion = 5);
bool GetProxy(enum Network net, proxyType &proxyInfoOut);
bool IsProxy(const CNetAddr &addr);
bool SetNameProxy(CService addrProxy, int nSocksVersion = 5);
bool HaveNameProxy();
bool LookupHost(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions = 0, bool fAllowLookup = true);
bool LookupHostNumeric(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions = 0);
bool Lookup(const char *pszName, CService& addr, int portDefault = 0, bool fAllowLookup = true);
bool Lookup(const char *pszName, std::vector<CService>& vAddr, int portDefault = 0, bool fAllowLookup = true, unsigned int nMaxSolutions = 0);
bool LookupNumeric(const char *pszName, CService& addr, int portDefault = 0);
bool ConnectSocket(const CService &addr, SOCKET& hSocketRet, int nTimeout = nConnectTimeout);
bool ConnectSocketByName(CService &addr, SOCKET& hSocketRet, const char *pszDest, int portDefault = 0, int nTimeout = nConnectTimeout);

std::string MakeTorV3Address(const unsigned char (&pchPubkey)[TOR_V3_PUBKEY_SIZE]);

#endif
