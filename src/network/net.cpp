// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2014-2018 Stealth R&D LLC
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "net.h"
#include "irc.h"
#include "db.h"
#include "init.h"
#include "addrman.h"
#include "ui_interface.h"
#include "onionseed.h"
#include "dnsseed.h"

#ifdef WIN32
#include <string.h>
#endif

#ifdef USE_UPNP
#include <miniupnpc/miniwget.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#endif

extern unsigned short onion_port;

extern "C" {
    int tor_main(int argc, char *argv[]);
}



using namespace std;
using namespace boost;


void ThreadMessageHandler2(void* parg);
void ThreadSocketHandler2(void* parg);
void ThreadOpenConnections2(void* parg);
void ThreadOpenAddedConnections2(void* parg);
#ifdef USE_UPNP
void ThreadMapPort2(void* parg);
#endif
void ThreadDNSAddressSeed2(void* parg);
bool OpenNetworkConnection(const CAddress& addrConnect,
                           CSemaphoreGrant* grantOutbound = NULL,
                           const char* strDest = NULL,
                           bool fOneShot = false);


struct LocalServiceInfo {
    int nScore;
    int nPort;
};

//
// Global state variables
//
bool fClient = false;
bool fDiscover = true;
bool fUseUPnP = false;

uint64_t nLocalServices = (fClient ? 0 : NODE_NETWORK);
static CCriticalSection cs_mapLocalHost;
static map<CNetAddr, LocalServiceInfo> mapLocalHost;
static bool vfReachable[NET_MAX] = {};
static bool vfLimited[NET_MAX] = {};
static CNode* pnodeLocalHost = NULL;
CAddress addrSeenByPeer(CSERVICE_NULL, nLocalServices);
uint64_t nLocalHostNonce = 0;
boost::array<int, THREAD_MAX> vnThreadsRunning;
static std::vector<SOCKET> vhListenSocket;
CAddrMan addrman;

vector<CNode*> vNodes;
CCriticalSection cs_vNodes;
map<CInv, CDataStream> mapRelay;
deque<pair<int64_t, CInv> > vRelayExpiration;
CCriticalSection cs_mapRelay;
map<CInv, int64_t> mapAlreadyAskedFor;

static deque<string> vOneShots;
CCriticalSection cs_vOneShots;

set<CNetAddr> setservAddNodeAddresses;
CCriticalSection cs_setservAddNodeAddresses;

static CSemaphore *semOutbound = NULL;

void AddOneShot(string strDest)
{
    LOCK(cs_vOneShots);
    vOneShots.push_back(strDest);
}

unsigned short GetListenPort()
{
    return (unsigned short)(GetArg("-port", GetDefaultPort()));
}

unsigned short GetTorPort()
{
    return onion_port;
}

void CNode::PushGetBlocks(CBlockIndex* pindexBegin, uint256 hashEnd)
{
    // Filter out duplicate requests less than 30 seconds since last
    if (pindexBegin == pindexLastGetBlocksBegin &&
        hashEnd == hashLastGetBlocksEnd)
    {
        if (nLastGetBlocks < (GetTime() - 30))
        {
            nLastGetBlocks = GetTime();
        }
        else
        {
            return;
        }
    }
    pindexLastGetBlocksBegin = pindexBegin;
    hashLastGetBlocksEnd = hashEnd;
    if (fDebugNet)
    {
        printf("PushGetBlocks(): %s:\n    %s to \n    %s\n",
               addrName.c_str(),
               pindexBegin->phashBlock->ToString().c_str(),
               hashEnd.ToString().c_str());
    }
    CBlockLocator locator(pindexBegin);
    PushMessage("getblocks", locator, hashEnd);
}

// find 'best' local address for a particular peer
bool GetLocal(CService& addr, const CNetAddr* paddrPeer)
{
    if (fNoListen)
    {
        if (fDebugNet)
        {
            printf("GetLocal(): not listening: null addr\n");
        }
        return false;
    }

    int nBestScore = -1;
    int nBestReachability = -1;
    {
        LOCK(cs_mapLocalHost);
        for (map<CNetAddr, LocalServiceInfo>::iterator it =
                 mapLocalHost.begin();
             it != mapLocalHost.end();
             it++)
        {
            int nScore = (*it).second.nScore;
            int nReachability = (*it).first.GetReachabilityFrom(paddrPeer);
            if (nReachability > nBestReachability ||
                (nReachability == nBestReachability && nScore > nBestScore))
            {
                addr = CService((*it).first, (*it).second.nPort);
                nBestReachability = nReachability;
                nBestScore = nScore;
            }
        }
    }
    return nBestScore >= 0;
}

// get best local address for a particular peer as a CAddress
CAddress GetLocalAddress(const CNetAddr *paddrPeer)
{
    CAddress ret;
    ret.SetIPv4Null();
    CService addr;
    if (GetLocal(addr, paddrPeer))
    {
        ret = CAddress(addr);
        ret.nServices = nLocalServices;
        ret.nTime = GetAdjustedTime();
    }
    return ret;
}

bool RecvLine(SOCKET hSocket, string& strLine)
{
    strLine = "";
    while (true)
    {
        char c;
        int nBytes = recv(hSocket, &c, 1, 0);
        if (nBytes > 0)
        {
            if (c == '\n')
            {
                continue;
            }
            if (c == '\r')
            {
                return true;
            }
            strLine += c;
            if (strLine.size() >= 9000)
            {
                return true;
            }
        }
        else if (nBytes <= 0)
        {
            if (fShutdown)
            {
                return false;
            }
            if (nBytes < 0)
            {
                int nErr = WSAGetLastError();
                if (nErr == WSAEMSGSIZE)
                {
                    continue;
                }
                if (nErr == WSAEWOULDBLOCK || nErr == WSAEINTR ||
                    nErr == WSAEINPROGRESS)
                {
                    MilliSleep(10);
                    continue;
                }
            }
            if (!strLine.empty())
            {
                return true;
            }
            if (nBytes == 0)
            {
                // socket closed
                printf("socket closed\n");
                return false;
            }
            else
            {
                // socket error
                int nErr = WSAGetLastError();
                printf("recv failed: %d\n", nErr);
                return false;
            }
        }
    }
}

// Is our peer's addrLocal potentially useful as an external IP source?
bool IsPeerAddrLocalGood(CNode* pnode)
{
    return fDiscover && pnode->addr.IsRoutable() &&
           pnode->addrLocal.IsRoutable() &&
           !IsLimited(pnode->addrLocal.GetNetwork());
}


// used when scores of local addresses may have changed
// pushes better local address to peers
void static AdvertizeLocal()
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        if (pnode->fSuccessfullyConnected)
        {
            CAddress addrLocal = GetLocalAddress(&pnode->addr);
            if (addrLocal.IsRoutable() &&
                (CService) addrLocal != (CService) pnode->addrLocal)
            {
                pnode->PushAddress(addrLocal);
                pnode->addrLocal = addrLocal;
            }
        }
    }
}

void SetReachable(enum Network net, bool fFlag)
{
    LOCK(cs_mapLocalHost);
    vfReachable[net] = fFlag;
    if (net == NET_IPV6 && fFlag)
        vfReachable[NET_IPV4] = true;
}

// learn a new local address
bool AddLocal(const CService& addr, int nScore)
{
    if (!addr.IsRoutable())
        return false;

    if (!fDiscover && nScore < LOCAL_MANUAL)
        return false;

    if (IsLimited(addr))
        return false;

    printf("AddLocal(%s,%i)\n", addr.ToString().c_str(), nScore);

    {
        LOCK(cs_mapLocalHost);
        bool fAlready = mapLocalHost.count(addr) > 0;
        LocalServiceInfo &info = mapLocalHost[addr];
        if (!fAlready || nScore >= info.nScore) {
            info.nScore = nScore + (fAlready ? 1 : 0);
            info.nPort = addr.GetPort();
        }
        SetReachable(addr.GetNetwork());
    }

    AdvertizeLocal();

    return true;
}

bool AddLocal(const CNetAddr &addr, int nScore)
{
    return AddLocal(CService(addr, GetListenPort()), nScore);
}

// Make a particular network entirely off-limits (no automatic connects to it)
void SetLimited(enum Network net, bool fLimited)
{
    if (net == NET_UNROUTABLE)
        return;
    LOCK(cs_mapLocalHost);
    vfLimited[net] = fLimited;
}

bool IsLimited(enum Network net)
{
    LOCK(cs_mapLocalHost);
    return vfLimited[net];
}

bool IsLimited(const CNetAddr &addr)
{
    return IsLimited(addr.GetNetwork());
}

/** vote for a local address */
bool SeenLocal(const CService& addr)
{
    {
        LOCK(cs_mapLocalHost);
        if (mapLocalHost.count(addr) == 0)
            return false;
        mapLocalHost[addr].nScore++;
    }

    AdvertizeLocal();

    return true;
}

/** check whether a given address is potentially local */
bool IsLocal(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    return mapLocalHost.count(addr) > 0;
}

/** check whether a given address is in a network we can probably connect to */
bool IsReachable(const CNetAddr& addr)
{
    LOCK(cs_mapLocalHost);
    enum Network net = addr.GetNetwork();
    return vfReachable[net] && !vfLimited[net];
}

bool GetMyExternalIP2(const CService& addrConnect,
                      const char* pszGet,
                      const char* pszKeyword,
                      CNetAddr& ipRet)
{
    SOCKET hSocket;
    if (!ConnectSocket(addrConnect, hSocket))
    {
        return error("GetMyExternalIP() : connection to %s failed",
                     addrConnect.ToString().c_str());
    }

    send(hSocket, pszGet, strlen(pszGet), MSG_NOSIGNAL);

    string strLine;
    while (RecvLine(hSocket, strLine))
    {
        // HTTP response is separated from headers by blank line
        if (strLine.empty())
        {
            while (true)
            {
                if (!RecvLine(hSocket, strLine))
                {
                    closesocket(hSocket);
                    return false;
                }
                if (pszKeyword == NULL)
                {
                    break;
                }
                if (strLine.find(pszKeyword) != string::npos)
                {
                    strLine = strLine.substr(strLine.find(pszKeyword) +
                                             strlen(pszKeyword));
                    break;
                }
            }
            closesocket(hSocket);
            if (strLine.find("<") != string::npos)
            {
                strLine = strLine.substr(0, strLine.find("<"));
            }
            strLine = strLine.substr(strspn(strLine.c_str(), " \t\n\r"));
            while (strLine.size() > 0 && isspace(strLine[strLine.size() - 1]))
            {
                strLine.resize(strLine.size() - 1);
            }
            CService addr(strLine, 0, true);
            printf("GetMyExternalIP() received [%s] %s\n",
                   strLine.c_str(),
                   addr.ToString().c_str());
            if (!addr.IsValid() || !addr.IsRoutable())
            {
                return false;
            }
            ipRet.SetIP(addr);
            return true;
        }
    }
    closesocket(hSocket);
    return error("GetMyExternalIP() : connection closed");
}

// We now get our external IP from the IRC server first and
//    only use this as a backup.
bool GetMyExternalIP(CNetAddr& ipRet)
{
    CService addrConnect;
    const char* pszGet;
    const char* pszKeyword;

    for (int nLookup = 0; nLookup <= 1; nLookup++)
    {
        for (int nHost = 1; nHost <= 2; nHost++)
        {
            // We should be phasing out our use of sites like these.  If we
            // need replacements, we should ask for volunteers to put this
            // simple php file on their web server that prints the client IP:
            //  <?php echo $_SERVER["REMOTE_ADDR"]; ?>
            if (nHost == 1)
            {
                // checkip.dyndns.org
                addrConnect = CService("91.198.22.70", 80);

                if (nLookup == 1)
                {
                    CService addrIP("checkip.dyndns.org", 80, true);
                    if (addrIP.IsValid())
                    {
                        addrConnect = addrIP;
                    }
                }

                pszGet = "GET / HTTP/1.1\r\n"
                         "Host: checkip.dyndns.org\r\n"
                         "User-Agent: Mozilla/4.0 (compatible; MSIE 7.0; "
                         "Windows NT 5.1)\r\n"
                         "Connection: close\r\n"
                         "\r\n";

                pszKeyword = "Address:";
            }
            else if (nHost == 2)
            {
                // www.showmyip.com
                addrConnect = CService("172.67.163.127", 80);

                if (nLookup == 1)
                {
                    CService addrIP("www.showmyip.com", 80, true);
                    if (addrIP.IsValid())
                    {
                        addrConnect = addrIP;
                    }
                }

                pszGet = "GET /simple/ HTTP/1.1\r\n"
                         "Host: www.showmyip.com\r\n"
                         "User-Agent: Mozilla/4.0 (compatible; MSIE 7.0; "
                         "Windows NT 5.1)\r\n"
                         "Connection: close\r\n"
                         "\r\n";

                pszKeyword = NULL;  // Returns just IP address
            }

            if (GetMyExternalIP2(addrConnect, pszGet, pszKeyword, ipRet))
            {
                return true;
            }
        }
    }

    return false;
}

void ThreadGetMyExternalIP(void* parg)
{
    // Make this thread recognisable as the external IP detection thread
    RenameThread("stealth-extip");

    CNetAddr addrLocalHost;
    if (GetMyExternalIP(addrLocalHost))
    {
        printf("GetMyExternalIP() returned %s\n",
               addrLocalHost.ToStringIP().c_str());
        AddLocal(addrLocalHost, LOCAL_HTTP);
    }
}

void AddressCurrentlyConnected(const CService& addr)
{
    addrman.Connected(addr);
}

CNode* FindNode(const CNetAddr& ip)
{
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH (CNode* pnode, vNodes)
        {
            if ((CNetAddr) pnode->addr == ip)
            {
                return (pnode);
            }
        }
    }
    return NULL;
}

CNode* FindNode(std::string addrName)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH (CNode* pnode, vNodes)
    {
        if (pnode->addrName == addrName)
        {
            return (pnode);
        }
    }
    return NULL;
}

CNode* FindNode(const CService& addr)
{
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH (CNode* pnode, vNodes)
        {
            if ((CService) pnode->addr == addr)
            {
                return (pnode);
            }
        }
    }
    return NULL;
}

CNode* ConnectNode(CAddress addrConnect, const char* pszDest, int64_t nTimeout)
{

    if (fDebugNet)
    {
        printf("ConnectNode(): pszDest: %s\n", pszDest ? pszDest : "NULL");
    }

    if (pszDest == NULL)
    {
        if (IsLocal(addrConnect))
        {
            return NULL;
        }

        // Look for an existing connection
        CNode* pnode = FindNode((CService) addrConnect);
        if (fDebugNet)
        {
            printf("ConnectNode(): node exists\n");
        }
        if (pnode)
        {
            if (nTimeout != 0)
            {
                pnode->AddRef(nTimeout);
            }
            else
            {
                pnode->AddRef();
            }
            return pnode;
        }
    }

    /// debug print
    printf("trying connection %s lastseen=%.1fhrs\n",
           pszDest ? pszDest : addrConnect.ToString().c_str(),
           pszDest ? 0
                   : (double)(GetAdjustedTime() - addrConnect.nTime) / 3600.0);

    // Connect
    SOCKET hSocket;
    if (pszDest ? ConnectSocketByName(addrConnect,
                                      hSocket,
                                      pszDest,
                                      GetDefaultPort())
                : ConnectSocket(addrConnect, hSocket))
    {
        addrman.Attempt(addrConnect);

        /// debug print
        printf("connected %s\n",
               pszDest ? pszDest : addrConnect.ToString().c_str());

        // Set to non-blocking
#ifdef WIN32
        u_long nOne = 1;
        if (ioctlsocket(hSocket, FIONBIO, &nOne) == SOCKET_ERROR)
        {
            printf(("ConnectSocket() : ioctlsocket non-blocking setting "
                    "failed, error %d\n"),
                   WSAGetLastError());
        }
#else
        if (fcntl(hSocket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
        {
            printf(("ConnectSocket() : fcntl non-blocking setting failed, "
                    "error %d\n"),
                   errno);
        }
#endif

        // Add node
        CNode* pnode = new CNode(hSocket,
                                 addrConnect,
                                 pszDest ? pszDest : "",
                                 false);
        if (nTimeout != 0)
        {
            pnode->AddRef(nTimeout);
        }
        else
        {
            pnode->AddRef();
        }

        {
            LOCK(cs_vNodes);
            vNodes.push_back(pnode);
        }

        pnode->nTimeConnected = GetTime();

        return pnode;
    }
    else
    {
        return NULL;
    }
}

void CNode::CloseSocketDisconnect()
{
    fDisconnect = true;
    if (hSocket != INVALID_SOCKET)
    {
        printf("disconnecting node %s\n", addrName.c_str());
        closesocket(hSocket);
        hSocket = INVALID_SOCKET;
        vRecv.clear();
    }
}

void CNode::Cleanup()
{
}


void CNode::PushVersion()
{
    /// when NTP implemented, change to just nTime = GetAdjustedTime()
    int64_t nTime = (fInbound ? GetAdjustedTime() : GetTime());
    uint64_t verification_token = 0;
    if (!fInbound)
    {
        if (!addrman.GetReconnectToken(addr, verification_token))
        {
            RAND_bytes((unsigned char*) &verification_token,
                       sizeof(verification_token));
            addrman.SetVerificationToken(addr, verification_token);
        }
    }
    // limit version ping-pong to 2 cycles
    if (nTimesVersionSent > 1)
    {
        return;
    }

    if (fDebugNet)
    {
        if (addr.IsRoutable())
        {
            printf("PushVersion(): Address %s is routable (good)\n",
                   addr.ToString().c_str());
        }
        else
        {
            printf("PushVersion(): Address %s not routable (bad)\n",
                   addr.ToString().c_str());
        }

        if (IsProxy(addr))
        {
            printf("PushVersion(): Address %s is proxy (bad)\n",
                   addr.ToString().c_str());
        }
        else
        {
            printf("PushVersion(): Address %s not proxy (good)\n",
                   addr.ToString().c_str());
        }
    }

    CAddress addrYou = addr;
    if (!addrYou.IsRoutable() || IsProxy(addrYou))
    {
         addrYou.SetIPv4Null();
    }

    CAddress addrMe = GetLocalAddress(&addr);
    if ((nVersion < CADDR_IP64_VERSION) && addrMe.IsTorV3())
    {
        if (fDebugNet)
        {
            printf(("PushVersion(): partner version %d with Tor v3 address\n"
                    "   me : %s\n   you: %s\n"),
                   nVersion,
                   addrMe.ToString().c_str(),
                   addrYou.ToString().c_str());
        }
        // partner is too old to work on tor
        addrMe.SetIPv4Null();
    }
    RAND_bytes((unsigned char*) &nLocalHostNonce, sizeof(nLocalHostNonce));
    printf("send version message: version %d, blocks=%d, us=%s, them=%s, "
           "peer=%s, verification=%" PRId64 "\n",
           PROTOCOL_VERSION,
           nBestHeight,
           addrMe.ToString().c_str(),
           addrYou.ToString().c_str(),
           addr.ToString().c_str(),
           verification_token);
    PushMessage("version",
                PROTOCOL_VERSION,
                nLocalServices,
                nTime,
                addrYou,
                addrMe,
                verification_token,
                nLocalHostNonce,
                FormatSubVersion(CLIENT_NAME,
                                 CLIENT_VERSION,
                                 std::vector<string>()),
                nBestHeight);
    nTimesVersionSent += 1;
}


std::map<CNetAddr, int64_t> CNode::setBanned;
CCriticalSection CNode::cs_setBanned;

void CNode::ClearBanned()
{
    setBanned.clear();
}

bool CNode::IsBanned(CNetAddr ip)
{
    bool fResult = false;
    {
        LOCK(cs_setBanned);
        std::map<CNetAddr, int64_t>::iterator i = setBanned.find(ip);
        if (i != setBanned.end())
        {
            int64_t t = (*i).second;
            if (GetTime() < t)
                fResult = true;
        }
    }
    return fResult;
}

extern CMedianFilter<int> cPeerBlockCounts;

bool CNode::Misbehaving(int howmuch)
{
    if (addr.IsLocal())
    {
        printf("Warning: Local node %s misbehaving (delta: %d)!\n", addrName.c_str(), howmuch);
        return false;
    }

    nMisbehavior += howmuch;
    if (nMisbehavior >= GetArg("-banscore", chainParams.DEFAULT_BANSCORE))
    {
        int64_t banTime = GetTime()+GetArg("-bantime",
                                           chainParams.DEFAULT_BANTIME);
        printf("Misbehaving: %s (%d -> %d) DISCONNECTING\n", addr.ToString().c_str(), nMisbehavior-howmuch, nMisbehavior);
        {
            LOCK(cs_setBanned);
            if (setBanned[addr] < banTime)
                setBanned[addr] = banTime;
        }
        CloseSocketDisconnect();

        cPeerBlockCounts.removeLast(nStartingHeight); // remove this node's reported number of blocks

        return true;
    } else
        printf("Misbehaving: %s (%d -> %d)\n", addr.ToString().c_str(), nMisbehavior-howmuch, nMisbehavior);
    return false;
}

int CNode::GetMisbehavior() const
{
    return nMisbehavior;
}

#undef X
#define X(name) stats.name = name
void CNode::copyStats(CNodeStats &stats)
{
    X(nServices);
    X(nLastSend);
    X(nLastRecv);
    X(nTimeConnected);
    X(addrName);
    X(nVersion);
    X(cleanSubVer);
    X(fInbound);
    X(nReleaseTime);
    X(nStartingHeight);
    X(nMisbehavior);
}
#undef X


void ThreadSocketHandler(void* parg)
{
    // Make this thread recognisable as the networking thread
    RenameThread("stealth-net");

    try
    {
        vnThreadsRunning[THREAD_SOCKETHANDLER]++;
        ThreadSocketHandler2(parg);
        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
        PrintException(&e, "ThreadSocketHandler()");
    } catch (...) {
        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
        throw; // support pthread_cancel()
    }
    printf("ThreadSocketHandler exited\n");
}

void ThreadSocketHandler2(void* parg)
{

    // Remodeling entails the forced disconnecting of
    // nodes. It's better to rate limit this process.
    static int REMODELSLEEP = GetArg("-remodelsleep",
                                     chainParams.DEFAULT_REMODELSLEEP);

    static int MAXCONNECTIONS = GetArg("-maxconnections",
                                       chainParams.DEFAULT_MAXCONNECTIONS);

    static int MINCONNREMODEL = GetArg("-minconnremodel",
                                       min(MAXCONNECTIONS,
                                           chainParams.DEFAULT_MINCONNREMODEL));

    printf("ThreadSocketHandler started\n");
    list<CNode*> vNodesDisconnected;
    unsigned int nPrevNodeCount = 0;

    while (true)
    {
        int64_t nTimeLastDisconnect = GetTime();
        //
        // Disconnect nodes
        //
        {
            LOCK(cs_vNodes);
            // Disconnect unused nodes
            vector<CNode*> const vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
            {
                if (pnode->fDisconnect ||
                    (pnode->GetRefCount() <= 0 && pnode->vRecv.empty() && pnode->vSend.empty()))
                {
                    // remove from vNodes
                    vNodes.erase(remove(vNodes.begin(), vNodes.end(), pnode), vNodes.end());

                    // release outbound grant (if any)
                    pnode->grantOutbound.Release();

                    // close socket and cleanup
                    pnode->CloseSocketDisconnect();
                    pnode->Cleanup();

                    // hold in disconnected pool until all refs are released
                    pnode->nReleaseTime = max(pnode->nReleaseTime, GetTime() + 15 * 60);
                    if (pnode->fNetworkNode || pnode->fInbound)
                        pnode->Release();
                    vNodesDisconnected.push_back(pnode);
                }
            }

            // Delete disconnected nodes
            list<CNode*> vNodesDisconnectedCopy = vNodesDisconnected;
            BOOST_FOREACH(CNode* pnode, vNodesDisconnectedCopy)
            {
                // wait until threads are done using it
                if (pnode->GetRefCount() <= 0)
                {
                    bool fDelete = false;
                    {
                        TRY_LOCK(pnode->cs_vSend, lockSend);
                        if (lockSend)
                        {
                            TRY_LOCK(pnode->cs_vRecv, lockRecv);
                            if (lockRecv)
                            {
                                TRY_LOCK(pnode->cs_mapRequests, lockReq);
                                if (lockReq)
                                {
                                    TRY_LOCK(pnode->cs_inventory, lockInv);
                                    if (lockInv)
                                        fDelete = true;
                                }
                            }
                        }
                    }
                    if (fDelete)
                    {
                        vNodesDisconnected.remove(pnode);
                        delete pnode;
                    }
                }
            }
            // count secured connections
            int unsecured = 0;
            int secured = 0;
            vector<CNode*> vNodesUnsecure;
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
            {
                if (GetTime() - pnode->nTimeConnected < 60) {
                    continue;
                }
                if (
                    !pnode->fInbound || pnode->fVerified
                ) {
                    secured++;
                } else {
                    unsecured++;
                    vNodesUnsecure.push_back(pnode);
                }
            }
            // don't remove unsecure nodes if no secure nodes
            // don't purposefully make peer count smaller than MINCONNREMODEL
            // rate limit disconnects to once per REMODELSLEEP seconds
            if ((secured > 0) &&
                (secured + unsecured > MINCONNREMODEL) &&
                (2 * secured < 3 * unsecured) &&
                (GetTime() - (nTimeLastDisconnect + REMODELSLEEP)) > 0)
            {
                CRandGen rg;
                shuffle(vNodesUnsecure.begin(), vNodesUnsecure.end(), rg);

                CNode *pUnsecure = (CNode *)*vNodesUnsecure.begin();
                string sAddrName = pUnsecure->addrName;
                string sIP;
                for (unsigned int i = 0; i < sAddrName.size(); ++i)
                {
                    char c = sAddrName[i];
                    if (c == ':')
                    {
                        break;
                    }
                    sIP.push_back(c);
                }
                if (IsInitialBlockDownload() ||
                    !pregistryMain->IsCertifiedNode(sIP))
                {
                    printf("removing unsecured connection %s (%s)\n",
                           sAddrName.c_str(),
                           pUnsecure->addr.ToString().c_str());
                    pUnsecure->fDisconnect = true;
                }
            }
        }
        if (vNodes.size() != nPrevNodeCount)
        {
            nPrevNodeCount = vNodes.size();
            uiInterface.NotifyNumConnectionsChanged(vNodes.size());
        }


        //
        // Find which sockets have data to receive
        //
        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 50000; // frequency to poll pnode->vSend

        fd_set fdsetRecv;
        fd_set fdsetSend;
        fd_set fdsetError;
        FD_ZERO(&fdsetRecv);
        FD_ZERO(&fdsetSend);
        FD_ZERO(&fdsetError);
        SOCKET hSocketMax = 0;
        bool have_fds = false;

        BOOST_FOREACH(SOCKET hListenSocket, vhListenSocket) {
            FD_SET(hListenSocket, &fdsetRecv);
            hSocketMax = max(hSocketMax, hListenSocket);
            have_fds = true;
        }
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                if (pnode->hSocket == INVALID_SOCKET)
                    continue;
                FD_SET(pnode->hSocket, &fdsetRecv);
                FD_SET(pnode->hSocket, &fdsetError);
                hSocketMax = max(hSocketMax, pnode->hSocket);
                have_fds = true;
                {
                    TRY_LOCK(pnode->cs_vSend, lockSend);
                    if (lockSend && !pnode->vSend.empty())
                        FD_SET(pnode->hSocket, &fdsetSend);
                }
            }
        }

        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
        int nSelect = select(have_fds ? hSocketMax + 1 : 0,
                             &fdsetRecv, &fdsetSend, &fdsetError, &timeout);
        vnThreadsRunning[THREAD_SOCKETHANDLER]++;
        if (fShutdown)
            return;
        if (nSelect == SOCKET_ERROR)
        {
            if (have_fds)
            {
                int nErr = WSAGetLastError();
                printf("socket select error %d\n", nErr);
                for (unsigned int i = 0; i <= hSocketMax; i++)
                    FD_SET(i, &fdsetRecv);
            }
            FD_ZERO(&fdsetSend);
            FD_ZERO(&fdsetError);
            MilliSleep(timeout.tv_usec/1000);
        }


        //
        // Accept new connections
        //
        BOOST_FOREACH(SOCKET hListenSocket, vhListenSocket)
        if (hListenSocket != INVALID_SOCKET && FD_ISSET(hListenSocket, &fdsetRecv))
        {
#ifdef USE_IPV6
            struct sockaddr_storage sockaddr;
#else
            struct sockaddr sockaddr;
#endif
            socklen_t len = sizeof(sockaddr);
            SOCKET hSocket = accept(hListenSocket, (struct sockaddr*)&sockaddr, &len);
            CAddress addr;
            int nInbound = 0;

            if (hSocket != INVALID_SOCKET)
            {
                if (!addr.SetSockAddr((const struct sockaddr*)&sockaddr))
                    printf("Warning: Unknown socket family\n");
            }

            {
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                    if (pnode->fInbound)
                        nInbound++;
            }

            if (hSocket == INVALID_SOCKET)
            {
                int nErr = WSAGetLastError();
                if (nErr != WSAEWOULDBLOCK)
                    printf("socket error accept failed: %d\n", nErr);
            }
            else if (nInbound >= (GetArg("-maxconnections",
                                         chainParams.DEFAULT_MAXCONNECTIONS) -
                                  chainParams.MAX_OUTBOUND_CONNECTIONS))
            {
                {
                    LOCK(cs_setservAddNodeAddresses);
                    printf("max connections reached\n");
                    if (!setservAddNodeAddresses.count(addr))
                        closesocket(hSocket);
                }
            }
            else
            {
                printf("accepted connection %s\n", addr.ToString().c_str());
                CNode* pnode = new CNode(hSocket, addr, "", true);
                pnode->AddRef();
                {
                    LOCK(cs_vNodes);
                    vNodes.push_back(pnode);
                }
            }
        }


        //
        // Service each socket
        //
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->AddRef();
        }
        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            if (fShutdown)
                return;

            //
            // Receive
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetRecv) || FD_ISSET(pnode->hSocket, &fdsetError))
            {
                TRY_LOCK(pnode->cs_vRecv, lockRecv);
                if (lockRecv)
                {
                    CDataStream& vRecv = pnode->vRecv;
                    unsigned int nPos = vRecv.size();

                    if (nPos > ReceiveBufferSize()) {
                        if (!pnode->fDisconnect)
                            printf("socket recv flood control disconnect (%" PRIszu " bytes)\n", vRecv.size());
                        pnode->CloseSocketDisconnect();
                    }
                    else {
                        // typical socket buffer is 8K-64K
                        char pchBuf[0x10000];
                        int nBytes = recv(pnode->hSocket, pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
                        if (nBytes > 0)
                        {
                            vRecv.resize(nPos + nBytes);
                            memcpy(&vRecv[nPos], pchBuf, nBytes);
                            pnode->nLastRecv = GetTime();
                        }
                        else if (nBytes == 0)
                        {
                            // socket closed gracefully
                            if (!pnode->fDisconnect)
                                printf("socket closed\n");
                            pnode->CloseSocketDisconnect();
                        }
                        else if (nBytes < 0)
                        {
                            // error
                            int nErr = WSAGetLastError();
                            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                            {
                                if (!pnode->fDisconnect)
                                    printf("socket recv error %d\n", nErr);
                                pnode->CloseSocketDisconnect();
                            }
                        }
                    }
                }
            }

            //
            // Send
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetSend))
            {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                {
                    CDataStream& vSend = pnode->vSend;
                    if (!vSend.empty())
                    {
                        int nBytes = send(pnode->hSocket, &vSend[0], vSend.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
                        if (nBytes > 0)
                        {
                            vSend.erase(vSend.begin(), vSend.begin() + nBytes);
                            pnode->nLastSend = GetTime();
                        }
                        else if (nBytes < 0)
                        {
                            // error
                            int nErr = WSAGetLastError();
                            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                            {
                                printf("socket send error %d\n", nErr);
                                pnode->CloseSocketDisconnect();
                            }
                        }
                    }
                }
            }

            //
            // Inactivity checking
            //
            if (pnode->vSend.empty())
                pnode->nLastSendEmpty = GetTime();
            if (GetTime() - pnode->nTimeConnected > 60)
            {
                if (pnode->nLastRecv == 0 || pnode->nLastSend == 0)
                {
                    printf("socket no message in first 60 seconds, %d %d\n", pnode->nLastRecv != 0, pnode->nLastSend != 0);
                    pnode->fDisconnect = true;
                }
                else if (GetTime() - pnode->nLastSend > 90*60 && GetTime() - pnode->nLastSendEmpty > 90*60)
                {
                    printf("socket not sending\n");
                    pnode->fDisconnect = true;
                }
                else if (GetTime() - pnode->nLastRecv > 90*60)
                {
                    printf("socket inactivity timeout\n");
                    pnode->fDisconnect = true;
                }
            }
        }
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }

        MilliSleep(10);
    }
}









#ifdef USE_UPNP
void ThreadMapPort(void* parg)
{
    // Make this thread recognisable as the UPnP thread
    RenameThread("stealth-upnp");

    printf("ThreadMapPort started\n");

    try
    {
        vnThreadsRunning[THREAD_UPNP]++;
        ThreadMapPort2(parg);
        vnThreadsRunning[THREAD_UPNP]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_UPNP]--;
        PrintException(&e, "ThreadMapPort()");
    } catch (...) {
        vnThreadsRunning[THREAD_UPNP]--;
        PrintException(NULL, "ThreadMapPort()");
    }
    printf("ThreadMapPort exited\n");
}

void ThreadMapPort2(void* parg)
{
    printf("ThreadMapPort2 started\n");

    std::string port = strprintf("%u", GetListenPort());
    const char * multicastif = 0;
    const char * minissdpdpath = 0;
    struct UPNPDev * devlist = 0;
    char lanaddr[64];

#ifndef UPNPDISCOVER_SUCCESS
    /* miniupnpc 1.5 */
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0);
#else
    /* miniupnpc 1.6 */
    int error = 0;
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, &error);
#endif

    struct UPNPUrls urls;
    struct IGDdatas data;
    int r;

    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
    if (r == 1)
    {
        if (fDiscover) {
            char externalIPAddress[40];
            r = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalIPAddress);
            if(r != UPNPCOMMAND_SUCCESS)
                printf("UPnP: GetExternalIPAddress() returned %d\n", r);
            else
            {
                if(externalIPAddress[0])
                {
                    printf("UPnP: ExternalIPAddress = %s\n", externalIPAddress);
                    AddLocal(CNetAddr(externalIPAddress), LOCAL_UPNP);
                }
                else
                    printf("UPnP: GetExternalIPAddress failed.\n");
            }
        }

        string strDesc = "Stealth " + FormatFullVersion();
#ifndef UPNPDISCOVER_SUCCESS
        /* miniupnpc 1.5 */
        r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                            port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0);
#else
        /* miniupnpc 1.6 */
        r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                            port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0, "0");
#endif

        if(r!=UPNPCOMMAND_SUCCESS)
            printf("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n",
                port.c_str(), port.c_str(), lanaddr, r, strupnperror(r));
        else
            printf("UPnP Port Mapping successful.\n");
        int i = 1;
        while (true) {
            if (fShutdown || !fUseUPnP)
            {
                r = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "TCP", 0);
                printf("UPNP_DeletePortMapping() returned : %d\n", r);
                freeUPNPDevlist(devlist); devlist = 0;
                FreeUPNPUrls(&urls);
                return;
            }
            if (i % 600 == 0) // Refresh every 20 minutes
            {
#ifndef UPNPDISCOVER_SUCCESS
                /* miniupnpc 1.5 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0);
#else
                /* miniupnpc 1.6 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0, "0");
#endif

                if(r!=UPNPCOMMAND_SUCCESS)
                    printf("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n",
                        port.c_str(), port.c_str(), lanaddr, r, strupnperror(r));
                else
                    printf("UPnP Port Mapping successful.\n");;
            }
            MilliSleep(2000);
            i++;
        }
    } else {
        printf("No valid UPnP IGDs found\n");
        freeUPNPDevlist(devlist); devlist = 0;
        if (r != 0)
            FreeUPNPUrls(&urls);
        while (true) {
            if (fShutdown || !fUseUPnP)
                return;
            MilliSleep(2000);
        }
    }
    printf("ThreadMapPort2 exited\n");
}

void MapPort(bool fUseUPnP)
{
    printf("MapPort()...\n");
    if (fUseUPnP && vnThreadsRunning[THREAD_UPNP] < 1)
    {
        if (!NewThread(ThreadMapPort, NULL))
            printf("Error: ThreadMapPort(ThreadMapPort) failed\n");
    }
}
#else
void MapPort()
{
    // Intentionally left blank.
}
#endif


void ThreadOnionSeed(void* parg)
{

    // Make this thread recognisable as the tor thread
    RenameThread("stealth-onionsd");

    printf("ThreadOnionSeed started\n");

    static const char *(*strOnionSeed)[1] = fTestNet ? strTestNetOnionSeed : strMainNetOnionSeed;

    int found = 0;

    printf("Loading addresses from .onion seeds\n");

    for (unsigned int seed_idx = 0; strOnionSeed[seed_idx][0] != NULL; seed_idx++) {
        CNetAddr parsed;
        if (
            !parsed.SetSpecial(
                strOnionSeed[seed_idx][0]
            )
        ) {
            throw runtime_error("ThreadOnionSeed() : invalid .onion seed");
        }
        int nOneDay = 24*3600;
        CAddress addr = CAddress(CService(parsed, GetDefaultPort()));
        addr.nTime = GetTime() - 3*nOneDay - GetRand(4*nOneDay); // use a random age between 3 and 7 days old
        found++;
        addrman.Add(addr, parsed);
    }

    printf("%d addresses found from .onion seeds\n", found);
}


void ThreadDNSAddressSeed(void* parg)
{
    // Make this thread recognisable as the DNS seeding thread
    RenameThread("stealth-dnsseed");

    try
    {
        vnThreadsRunning[THREAD_DNSSEED]++;
        ThreadDNSAddressSeed2(parg);
        vnThreadsRunning[THREAD_DNSSEED]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_DNSSEED]--;
        PrintException(&e, "ThreadDNSAddressSeed()");
    } catch (...) {
        vnThreadsRunning[THREAD_DNSSEED]--;
        throw; // support pthread_cancel()
    }
    printf("ThreadDNSAddressSeed exited\n");
}

void ThreadDNSAddressSeed2(void* parg)
{
    printf("ThreadDNSAddressSeed started\n");

    static const char *(*strDNSSeed)[2] = fTestNet ? strTestNetDNSSeed : strMainNetDNSSeed;

    int found = 0;

    printf("Loading addresses from DNS seeds (could take a while)\n");

    for (unsigned int seed_idx = 0; strDNSSeed[seed_idx][0] != NULL; seed_idx++) {
        if (HaveNameProxy()) {
            AddOneShot(strDNSSeed[seed_idx][1]);
        } else {
            vector<CNetAddr> vaddr;
            vector<CAddress> vAdd;
            if (LookupHost(strDNSSeed[seed_idx][1], vaddr))
            {
                BOOST_FOREACH(CNetAddr& ip, vaddr)
                {
                    int nOneDay = 24*3600;
                    CAddress addr = CAddress(CService(ip, GetDefaultPort()));
                    // use a random age between 3 and 7 days old
                    addr.nTime = GetTime() - 3*nOneDay - GetRand(4*nOneDay);
                    vAdd.push_back(addr);
                    printf("ThreadDNSAddressSeed2: added \"%s\"\n",
                           addr.ToStringIPPort().c_str());
                    found++;
                }
            }
            addrman.Add(vAdd, CNetAddr(strDNSSeed[seed_idx][0], true));
        }
    }

    printf("%d addresses found from DNS seeds\n", found);
}

void DumpAddresses()
{
    int64_t nStart = GetTimeMillis();

    CAddrDB adb;
    adb.Write(addrman);

    printf("Flushed %d addresses to peers.dat  %" PRId64 "ms\n",
           addrman.size(), GetTimeMillis() - nStart);
}

void ThreadDumpAddress2(void* parg)
{
    vnThreadsRunning[THREAD_DUMPADDRESS]++;
    while (!fShutdown)
    {
        DumpAddresses();
        vnThreadsRunning[THREAD_DUMPADDRESS]--;
        MilliSleep(100000);
        vnThreadsRunning[THREAD_DUMPADDRESS]++;
    }
    vnThreadsRunning[THREAD_DUMPADDRESS]--;
}

void ThreadDumpAddress(void* parg)
{
    // Make this thread recognisable as the address dumping thread
    RenameThread("stealth-adrdump");

    try
    {
        ThreadDumpAddress2(parg);
    }
    catch (std::exception& e) {
        PrintException(&e, "ThreadDumpAddress()");
    }
    printf("ThreadDumpAddress exited\n");
}

void ThreadOpenConnections(void* parg)
{
    // Make this thread recognisable as the connection opening thread
    RenameThread("stealth-opencon");

    try
    {
        vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
        ThreadOpenConnections2(parg);
        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        PrintException(&e, "ThreadOpenConnections()");
    } catch (...) {
        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        PrintException(NULL, "ThreadOpenConnections()");
    }
    printf("ThreadOpenConnections exited\n");
}

void static ProcessOneShot()
{
    string strDest;
    {
        LOCK(cs_vOneShots);
        if (vOneShots.empty())
            return;
        strDest = vOneShots.front();
        vOneShots.pop_front();
    }
    CAddress addr;
    CSemaphoreGrant grant(*semOutbound, true);
    if (grant) {
        if (!OpenNetworkConnection(addr, &grant, strDest.c_str(), true))
            AddOneShot(strDest);
    }
}

// ppcoin: stake minter thread
void static ThreadStakeMinter(void* parg)
{
    printf("ThreadStakeMinter started\n");
    CWallet* pwallet = (CWallet*)parg;
    try
    {
        vnThreadsRunning[THREAD_STAKEMINTER]++;
        StealthMinter(pwallet, PROOFTYPE_POS);
        vnThreadsRunning[THREAD_STAKEMINTER]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_STAKEMINTER]--;
        PrintException(&e, "ThreadStakeMinter()");
    } catch (...) {
        vnThreadsRunning[THREAD_STAKEMINTER]--;
        PrintException(NULL, "ThreadStakeMinter()");
    }
    printf("ThreadStakeMinter exiting, %d threads remaining\n",
                                  vnThreadsRunning[THREAD_STAKEMINTER]);
}

bool OpenTrustedConnections(const string strSetting)
{
    bool fResult = false;
    // Connect to specific addresses
    if (mapArgs.count(strSetting) && mapMultiArgs[strSetting].size() > 0)
    {
        fResult = true;
        for (int64_t nLoop = 0;; nLoop++)
        {
            ProcessOneShot();
            BOOST_FOREACH (string strAddr, mapMultiArgs[strSetting])
            {
                if (fDebugNet)
                {
                    printf("Opening trusted connection %s from \"%s\"\n",
                           strAddr.c_str(),
                           strSetting.c_str());
                }
                CAddress addr;
                OpenNetworkConnection(addr, NULL, strAddr.c_str());
                for (int i = 0; i < 10 && i < nLoop; i++)
                {
                    MilliSleep(500);
                    if (fShutdown)
                    {
                        return fResult;
                    }
                }
            }
            MilliSleep(500);
        }
    }
    return fResult;
}

void ThreadOpenConnections2(void* parg)
{
    printf("ThreadOpenConnections started\n");

    // The difference between "-connect" and "-addnode" is
    //    that only the "-connect" trusted peers can connect,
    //    and then only outbound. In other words, the client will not
    //    listen if "-connect" is set for one or more peers.
    OpenTrustedConnections("-connect");

    while (true)
    {
        ProcessOneShot();

        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        MilliSleep(2000);
        vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
        if (fShutdown)
        {
            return;
        }

        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        CSemaphoreGrant grant(*semOutbound);
        vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
        if (fShutdown)
        {
            return;
        }

        //
        // Choose an address to connect to based on most recently seen
        //
        CAddress addrConnect;

        // Only connect out to one peer per network group (/16 for IPv4).
        // Do this here so we don't have to critsect vNodes inside mapAddresses
        // critsect.
        int nOutbound = 0;
        set<vector<unsigned char> > setConnected;
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH (CNode* pnode, vNodes)
            {
                if (!pnode->fInbound)
                {
                    setConnected.insert(pnode->addr.GetGroup());
                    nOutbound++;
                }
            }
        }

        int64_t nANow = GetAdjustedTime();

        // a flag so we don't check validity of addresses that
        //    don't need checking
        bool fCheck = true;
        int nTries = 0;
        while (true)
        {
            // 10 tries per second should be fast enough
            MilliSleep(100);

            if (addrman.size() == 0)
            {
                fCheck = false;
                break;
            }

            // use an nUnkBias between 10 (no outgoing connections) and 90 (8
            // outgoing connections)

            CAddress addr = addrman.Select(10 + min(nOutbound, 8) * 10);

            // if we selected an invalid address, restart
            if (!addr.IsValid() || setConnected.count(addr.GetGroup()) ||
                IsLocal(addr))
            {
                fCheck = false;
                break;
            }

            // If we didn't find an appropriate destination after trying 100
            // addresses fetched from addrman, stop this loop, and let the
            // outer loop run again (which sleeps, adds seed nodes,
            // recalculates already-connected network ranges, ...) before
            // trying new addrman addresses.
            nTries++;
            if (nTries > 100)
            {
                break;
            }

            if (IsLimited(addr))
            {
                continue;
            }

            // only consider very recently tried nodes after 30 failed attempts
            if (nANow - addr.nLastTry < 600 && nTries < 30)
            {
                continue;
            }

            // do not allow non-default ports, unless after 50 invalid
            // addresses selected already
            if (addr.GetPort() != GetDefaultPort() && nTries < 50)
            {
                continue;
            }

            addrConnect = addr;
            break;
        }

        if (fCheck && addrConnect.IsValid())
        {
            OpenNetworkConnection(addrConnect, &grant);
        }
    }
}

void ThreadOpenAddedConnections(void* parg)
{
    // Make this thread recognisable as the connection opening thread
    RenameThread("stealth-openadd");

    printf("ThreadOpenAddedConnections started\n");

    try
    {
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]++;
        ThreadOpenAddedConnections2(parg);
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
        PrintException(&e, "ThreadOpenAddedConnections()");
    } catch (...) {
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
        PrintException(NULL, "ThreadOpenAddedConnections()");
    }
    printf("ThreadOpenAddedConnections exited\n");
}

void ThreadOpenAddedConnections2(void* parg)
{
    printf("ThreadOpenAddedConnections2 started\n");

    if (mapMultiArgs["-addnode"].size() == 0)
    {
        printf("ThreadOpenAddedConnections: found no nodes to add\n");
        return;
    }

    if (HaveNameProxy()) {
        while(!fShutdown) {
            BOOST_FOREACH(string& strAddNode, mapMultiArgs["-addnode"]) {
                CAddress addr;
                CSemaphoreGrant grant(*semOutbound);
                OpenNetworkConnection(addr, &grant, strAddNode.c_str());
                MilliSleep(500);
            }
            vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
            MilliSleep(120000); // Retry every 2 minutes
            vnThreadsRunning[THREAD_ADDEDCONNECTIONS]++;
        }
        return;
    }

    vector<vector<CService> > vservAddressesToAdd(0);
    BOOST_FOREACH(string& strAddNode, mapMultiArgs["-addnode"])
    {
        printf("ThreadOpenAddedConnections: found addnode \"%s\"\n", strAddNode.c_str());
        vector<CService> vservNode(0);
        if (Lookup(strAddNode.c_str(), vservNode, GetDefaultPort(), fNameLookup, 0))
        {
            if (fDebugNet)
            {
                printf("Lookup Result:\n  %s\n  %s\n%s\n",
                       strAddNode.c_str(),
                       vservNode[0].ToString().c_str(),
                       vservNode[0].GetHex(16, "    ").c_str());
            }
            vservAddressesToAdd.push_back(vservNode);
            {
                LOCK(cs_setservAddNodeAddresses);
                BOOST_FOREACH(CService& serv, vservNode)
                    setservAddNodeAddresses.insert(serv);
            }
        }
    }
    while (true)
    {
        vector<vector<CService> > vservConnectAddresses = vservAddressesToAdd;
        vector<string> vCertIPs(0);
        pregistryMain->GetCertifiedNodes(vCertIPs);
        printf("ThreadOpenAddedConnections: %zu certified nodes\n",
               vCertIPs.size());
        BOOST_FOREACH(string strCertIP, vCertIPs)
        {
            vector<CService> vservCertNode(0);
            if (Lookup(strCertIP.c_str(), vservCertNode, GetDefaultPort(), fNameLookup, 0))
            {
                vservConnectAddresses.push_back(vservCertNode);
            }
        }
        // Attempt to connect to each IP for each addnode entry until at least one is successful per addnode entry
        // (keeping in mind that addnode entries can have many IPs if fNameLookup)
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
                for (vector<vector<CService> >::iterator it = vservConnectAddresses.begin(); it != vservConnectAddresses.end(); it++)
                    BOOST_FOREACH(CService& addrNode, *(it))
                        if (pnode->addr == addrNode)
                        {
                            it = vservConnectAddresses.erase(it);
                            it--;
                            break;
                        }
        }
        BOOST_FOREACH(vector<CService>& vserv, vservConnectAddresses)
        {
            CSemaphoreGrant grant(*semOutbound);
            CAddress addrToConnect(*(vserv.begin()));

            if (find(vservAddressesToAdd.begin(),
                     vservAddressesToAdd.end(),
                     vserv) != vservAddressesToAdd.end())
            {
                printf("ThreadOpenAddedConnections: added node\n  \"%s\"\n",
                       addrToConnect.ToStringIPPort().c_str());
            }
            else
            {
                printf("ThreadOpenAddedConnections: certified node:\n  \"%s\"\n",
                       addrToConnect.ToStringIPPort().c_str());
            }
            if (fDebugNet)
            {
                printf("Node:\n%s", addrToConnect.GetHex(16, "    ").c_str());
            }
            OpenNetworkConnection(addrToConnect, &grant);
            MilliSleep(500);
            if (fShutdown)
            {
                return;
            }
        }
        if (fShutdown)
            return;
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
        MilliSleep(20000); // Retry every 20 seconds
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]++;
        if (fShutdown)
            return;
    }
    printf("ThreadOpenAddedConnections2 exited\n");
}

// if successful, this moves the passed grant to the constructed node
bool OpenNetworkConnection(const CAddress& addrConnect,
                           CSemaphoreGrant* grantOutbound,
                           const char* strDest,
                           bool fOneShot)
{
    //
    // Initiate outbound network connection
    //
    if (fShutdown)
    {
        return false;
    }
    if (!strDest)
    {
        if (IsLocal(addrConnect) || FindNode((CNetAddr) addrConnect) ||
            CNode::IsBanned(addrConnect) ||
            FindNode(addrConnect.ToStringIPPort().c_str()))
        {
            return false;
        }
    }
    if (fDebugNet)
    {
        printf("opening destination:\n  %s\n  address: %s\n",
               strDest,
               addrConnect.ToString().c_str());
    }
    if (strDest && FindNode(strDest))
    {
        return false;
    }
    vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
    CNode* pnode = ConnectNode(addrConnect, strDest);
    vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
    if (fShutdown)
    {
        return false;
    }
    if (!pnode)
    {
        return false;
    }
    if (grantOutbound)
    {
        grantOutbound->MoveTo(pnode->grantOutbound);
    }
    pnode->fNetworkNode = true;
    if (fOneShot)
    {
        pnode->fOneShot = true;
    }
    return true;
}


void ThreadMessageHandler(void* parg)
{
    // Make this thread recognisable as the message handling thread
    RenameThread("stealth-msghand");

    printf("ThreadMessageHandler started\n");

    try
    {
        vnThreadsRunning[THREAD_MESSAGEHANDLER]++;
        ThreadMessageHandler2(parg);
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
        PrintException(&e, "ThreadMessageHandler()");
    } catch (...) {
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
        PrintException(NULL, "ThreadMessageHandler()");
    }
    printf("ThreadMessageHandler exited\n");
}

void ThreadMessageHandler2(void* parg)
{
    printf("ThreadMessageHandler2 started\n");
    CWallet* pwallet = (CWallet*)parg;

    // allow conf to disable qPoS minting
    bool fQPoSActive = GetBoolArg("-qposminting", true);

    // qPoS (TODO: and PoS)
    // Each block production thread has its own key and counter
    CReserveKey reservekey(pwallet);
    // TODO: add extra nonce if ever putting PoS in this thread
    // unsigned int nExtraNonce = 0;

    SetThreadPriority(THREAD_PRIORITY_ABOVE_NORMAL);
    while (!fShutdown)
    {
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->AddRef();
        }

        // Poll the connected nodes for messages
        CNode* pnodeTrickle = NULL;
        if (!vNodesCopy.empty())
            pnodeTrickle = vNodesCopy[GetRand(vNodesCopy.size())];
        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            // Receive messages
            {
                TRY_LOCK(pnode->cs_vRecv, lockRecv);
                if (lockRecv)
                {
                    ProcessMessages(pnode);
                }
            }
            if (fShutdown)
                return;

            // Send messages
            {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                    SendMessages(pnode, pnode == pnodeTrickle);
            }
            if (fShutdown)
                return;
        }

        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }

        // avoid multithreading issues by combining
        // message handling and stake minting
        SetThreadPriority(THREAD_PRIORITY_LOWEST);

        CBlockIndex* pindexPrev = pindexBest;
        int nHeight = pindexPrev->nHeight + 1;

        // rollbacks mean qPoS can keep producing even with 0 connections
        if ((GetFork(nHeight) >= XST_FORKQPOS) &&
            ((nMaxHeight <= 0) || (nBestHeight < nMaxHeight)) &&
            fQPoSActive &&
            (fTestNet ||
              (GetBoolArg("-stake", true) &&
               GetBoolArg("-staking", true))) &&
            !pwallet->IsLocked())
        {
           /*******************************************************************
            ** qPoS
            *******************************************************************/
            if ((GetFork(nHeight - 1) < XST_FORKQPOS) &&
                (pregistryMain->GetRound() == 0))
            {
                pregistryMain->UpdateOnNewTime(GetAdjustedTime(),
                                               pindexPrev,
                                               QPRegistry::NO_SNAPS,
                                               fDebugQPoS);
            }

            AUTO_PTR<CBlock> pblock(new CBlock());
            if (!pblock.get())
            {
                 return;
            }

            BlockCreationResult nResult = CreateNewBlock(pwallet,
                                                         PROOFTYPE_QPOS,
                                                         pblock);

            bool fFinalizeBlock = true;
            if (nResult == BLOCKCREATION_QPOS_IN_REPLAY)
            {
                fFinalizeBlock = false;
            }
            if ((nResult == BLOCKCREATION_NOT_CURRENTSTAKER) ||
                (nResult == BLOCKCREATION_QPOS_BLOCK_EXISTS))
            {
                if (fDebugBlockCreation)
                {
                    printf("block creation abandoned with \"%s\"\n",
                           DescribeBlockCreationResult(nResult));
                }
                fFinalizeBlock = false;
            }

            if (nResult == BLOCKCREATION_INSTANTIATION_FAIL ||
                nResult == BLOCKCREATION_REGISTRY_FAIL)
            {
                if (fDebugBlockCreation)
                {
                    printf("block creation catastrophic fail with \"%s\"\n",
                           DescribeBlockCreationResult(nResult));
                }
                fFinalizeBlock = false;
                fQPoSActive = false;
            }

            if (fFinalizeBlock)
            {
                pblock->hashMerkleRoot = pblock->BuildMerkleTree();
                if (pblock->SignBlock(*pwallet, pregistryMain))
                {
                    printf("QPoS block found %s\n",
                           pblock->GetHash().ToString().c_str());
                    pblock->print();
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    CheckWork(pblock.get(), *pwallet, reservekey, pindexPrev);
                    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
                }
            }
            pblock.reset();
        }

        // Wait and allow messages to bunch up.
        // Reduce vnThreadsRunning so StopNode has permission to exit while
        // we're sleeping, but we must always check fShutdown after doing this.
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
        MilliSleep(100);
        if (fRequestShutdown)
        {
            StartShutdown();
        }
        vnThreadsRunning[THREAD_MESSAGEHANDLER]++;
        if (fShutdown)
        {
            return;
        }
        if (GetFork(nBestHeight) >= XST_FORKQPOS)
        {
            unsigned int nStakersThis = pregistryMain->GetCurrentQueueSize();
            unsigned int nStakersLast = pregistryMain->GetPreviousQueueSize();
            unsigned int nPcmRatio = 10000 * nStakersThis / nStakersLast;
            if (vNodes.empty() ||
                (pindexBest->nTime < (GetTime() - QP_REGISTRY_MAX_FALL_BEHIND)) ||
                (nStakersThis < 3) ||
                (nPcmRatio < 6667))
            {
                pregistryMain->EnterReplayMode();
            }
            else
            {
                pregistryMain->CheckSynced();
            }
        }
    }
    printf("ThreadMessageHandler2 exited\n");
}



bool BindListenPort(const CService &addrBind, string& strError)
{
    strError = "";
    int nOne = 1;

#ifdef WIN32
    // Initialize Windows Sockets
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR)
    {
        strError = strprintf("Error: TCP/IP socket library failed to start (WSAStartup returned error %d)", ret);
        printf("%s\n", strError.c_str());
        return false;
    }
#endif

    // Create socket for listening for incoming connections
#ifdef USE_IPV6
    struct sockaddr_storage sockaddr;
#else
    struct sockaddr sockaddr;
#endif
    socklen_t len = sizeof(sockaddr);
    if (!addrBind.GetSockAddr((struct sockaddr*)&sockaddr, &len))
    {
        strError = strprintf("Error: bind address family for %s not supported", addrBind.ToString().c_str());
        printf("%s\n", strError.c_str());
        return false;
    }

    SOCKET hListenSocket = socket(((struct sockaddr*)&sockaddr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (hListenSocket == INVALID_SOCKET)
    {
        strError = strprintf("Error: Couldn't open socket for incoming connections (socket returned error %d)", WSAGetLastError());
        printf("%s\n", strError.c_str());
        return false;
    }

#ifdef SO_NOSIGPIPE
    // Different way of disabling SIGPIPE on BSD
    setsockopt(hListenSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&nOne, sizeof(int));
#endif

#ifndef WIN32
    // Allow binding if the port is still in TIME_WAIT state after
    // the program was closed and restarted.  Not an issue on windows.
    setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEADDR, (void*)&nOne, sizeof(int));
#endif


#ifdef WIN32
    // Set to non-blocking, incoming connections will also inherit this
    if (ioctlsocket(hListenSocket, FIONBIO, (u_long*)&nOne) == SOCKET_ERROR)
#else
    if (fcntl(hListenSocket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
#endif
    {
        strError = strprintf("Error: Couldn't set properties on socket for incoming connections (error %d)", WSAGetLastError());
        printf("%s\n", strError.c_str());
        return false;
    }

#ifdef USE_IPV6
    // some systems don't have IPV6_V6ONLY but are always v6only; others do have the option
    // and enable it by default or not. Try to enable it, if possible.
    if (addrBind.IsIPv6()) {
#ifdef IPV6_V6ONLY
#ifdef WIN32
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&nOne, sizeof(int));
#else
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&nOne, sizeof(int));
#endif
#endif
#ifdef WIN32
        int nProtLevel = 10 /* PROTECTION_LEVEL_UNRESTRICTED */;
        int nParameterId = 23 /* IPV6_PROTECTION_LEVEl */;
        // this call is allowed to fail
        setsockopt(hListenSocket, IPPROTO_IPV6, nParameterId, (const char*)&nProtLevel, sizeof(int));
#endif
    }
#endif

    if (::bind(hListenSocket, (struct sockaddr*)&sockaddr, len) == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        if (nErr == WSAEADDRINUSE)
            strError = strprintf(_("Unable to bind to %s on this computer. Stealth is probably already running."), addrBind.ToString().c_str());
        else
            strError = strprintf(_("Unable to bind to %s on this computer (bind returned error %d, %s)"), addrBind.ToString().c_str(), nErr, strerror(nErr));
        printf("%s\n", strError.c_str());
        return false;
    }
    printf("Bound to %s\n", addrBind.ToString().c_str());

    // Listen for incoming connections
    if (listen(hListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        strError = strprintf("Error: Listening for incoming connections failed (listen returned error %d)", WSAGetLastError());
        printf("%s\n", strError.c_str());
        return false;
    }

    vhListenSocket.push_back(hListenSocket);

    if (addrBind.IsRoutable() && fDiscover)
        AddLocal(addrBind, LOCAL_BIND);

    return true;
}

void static Discover()
{
   // no network discovery
}

static void run_tor() {
    fprintf(stdout, "TOR thread started.\n");

    std::string logDecl = "notice file " + GetDataDir().string() + "/tor/tor.log";

    char* argv[4];

    argv[0] = (char*)"tor";
    argv[1] = (char*)"--hush";
    argv[2] = (char*)"--Log";
    argv[3] = (char*)logDecl.c_str();

    tor_main(4, argv);
}


void StartTor(void* parg)
{
    // Make this thread recognisable as the tor thread
    RenameThread("stealth-onion");

    try
    {
      run_tor();
    }
    catch (std::exception& e) {
      PrintException(&e, "StartTor()");
    }

    printf("Onion thread exited.");

}

void StartNode(void* parg)
{
    // Make this thread recognisable as the startup thread
    RenameThread("stealth-start");

    if (semOutbound == NULL) {
        // initialize semaphore
        int nMaxOutbound = min(chainParams.MAX_OUTBOUND_CONNECTIONS,
                               (int)GetArg("-maxconnections",
                                           chainParams.DEFAULT_MAXCONNECTIONS));
        semOutbound = new CSemaphore(nMaxOutbound);
    }

    if (pnodeLocalHost == NULL)
        pnodeLocalHost = new CNode(INVALID_SOCKET,
                                   CAddress(CService("127.0.0.1", 0), nLocalServices));

    printf("StartNode(): pnodeLocalHost addr: %s\n",
           pnodeLocalHost->addr.ToString().c_str());

    Discover();

    //
    // Start threads
    //

    // start the onion seeder if necessary
    if (!vfLimited[NET_TOR])
    {
        if (!GetBoolArg("-onionseed", true))
            printf(".onion seeding disabled\n");
        else
            if (!NewThread(ThreadOnionSeed, NULL))
                  printf("Error: could not start .onion seeding\n");
    }

    // start the DNS seeder if necessary
    if (!(vfLimited[NET_IPV4] && vfLimited[NET_IPV6]))
    {
        if (!GetBoolArg("-dnsseed", true))
            printf("DNS seeding disabled\n");
        else
            if (!NewThread(ThreadDNSAddressSeed, NULL))
                  printf("Error: could not start DNS seeding\n");
    }

    // Map ports with UPnP (default)
#ifdef USE_UPNP
    if (fUseUPnP)
        MapPort(fUseUPnP);
#endif
    // Get addresses from IRC and advertise ours
    //if (!NewThread(ThreadIRCSeed, NULL))
    //    printf("Error: NewThread(ThreadIRCSeed) failed\n");

    // Send and receive from sockets, accept connections
    if (!NewThread(ThreadSocketHandler, NULL))
        printf("Error: NewThread(ThreadSocketHandler) failed\n");

    // Initiate outbound connections from -addnode
    if (!NewThread(ThreadOpenAddedConnections, NULL))
        printf("Error: NewThread(ThreadOpenAddedConnections) failed\n");

    // Initiate outbound connections
    if (!NewThread(ThreadOpenConnections, NULL))
        printf("Error: NewThread(ThreadOpenConnections) failed\n");

    // Process messages and mint qPoS blocks
    // TODO: add PoS to this thread
    if (!NewThread(ThreadMessageHandler, pwalletMain))
        printf("Error: NewThread(ThreadMessageHandler) failed\n");

    // Dump network addresses
    if (!NewThread(ThreadDumpAddress, NULL))
        printf("Error; NewThread(ThreadDumpAddress) failed\n");

    // make sure conf allows it (stake=0 in conf will disallow)
    if (GetBoolArg("-stake", true) && GetBoolArg("-staking", true)) {
        printf("Stake minting enabled at startup.\n");
        if (!NewThread(ThreadStakeMinter, pwalletMain))
            printf("Error: NewThread(ThreadStakeMinter) failed\n");
    } else {
        printf("Stake minting disabled at startup (staking=0).\n");
    }

#ifdef WITH_MINER
    // Generate coins in the background
    GenerateXST(GetBoolArg("-gen", false), pwalletMain);
#endif  /* WITH_MINER */
}



bool StopNode()
{
    printf("StopNode()\n");
    fShutdown = true;
    nTransactionsUpdated++;
    int64_t nStart = GetTime();
    if (semOutbound)
        for (int i=0; i < chainParams.MAX_OUTBOUND_CONNECTIONS; i++)
            semOutbound->post();
    do
    {
        int nThreadsRunning = 0;
        for (int n = 0; n < THREAD_MAX; n++)
            nThreadsRunning += vnThreadsRunning[n];
        if (nThreadsRunning == 0)
            break;
        if (GetTime() - nStart > 20)
            break;
        MilliSleep(20);
    } while(true);
    if (vnThreadsRunning[THREAD_SOCKETHANDLER] > 0) printf("ThreadSocketHandler still running\n");
    if (vnThreadsRunning[THREAD_OPENCONNECTIONS] > 0) printf("ThreadOpenConnections still running\n");
    if (vnThreadsRunning[THREAD_MESSAGEHANDLER] > 0) printf("ThreadMessageHandler still running\n");
#ifdef WITH_MINER
    if (vnThreadsRunning[THREAD_MINER] > 0) printf("ThreadStealthMiner still running\n");
#endif
    if (vnThreadsRunning[THREAD_RPCLISTENER] > 0) printf("ThreadRPCListener still running\n");
    if (vnThreadsRunning[THREAD_RPCHANDLER] > 0) printf("ThreadsRPCServer still running\n");
#ifdef USE_UPNP
    if (vnThreadsRunning[THREAD_UPNP] > 0) printf("ThreadMapPort still running\n");
#endif
    if (vnThreadsRunning[THREAD_DNSSEED] > 0) printf("ThreadDNSAddressSeed still running\n");
    if (vnThreadsRunning[THREAD_ADDEDCONNECTIONS] > 0) printf("ThreadOpenAddedConnections still running\n");
    if (vnThreadsRunning[THREAD_DUMPADDRESS] > 0) printf("ThreadDumpAddresses still running\n");
    if (vnThreadsRunning[THREAD_STAKEMINTER] > 0) printf("ThreadStakeMinter still running\n");

    while (vnThreadsRunning[THREAD_MESSAGEHANDLER] > 0 || vnThreadsRunning[THREAD_RPCHANDLER] > 0)
        MilliSleep(20);
    MilliSleep(50);
    DumpAddresses();
    return true;
}

class CNetCleanup
{
public:
    CNetCleanup()
    {
    }
    ~CNetCleanup()
    {
        // Close sockets
        BOOST_FOREACH(CNode* pnode, vNodes)
            if (pnode->hSocket != INVALID_SOCKET)
                closesocket(pnode->hSocket);
        BOOST_FOREACH(SOCKET hListenSocket, vhListenSocket)
            if (hListenSocket != INVALID_SOCKET)
                if (closesocket(hListenSocket) == SOCKET_ERROR)
                    printf("closesocket(hListenSocket) failed with error %d\n", WSAGetLastError());

#ifdef WIN32
        // Shutdown Windows Sockets
        WSACleanup();
#endif
    }
}
instance_of_cnetcleanup;

void RelayTransaction(const CTransaction& tx, const uint256& hash)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(10000);
    ss << tx;
    RelayTransaction(tx, hash, ss);
}

void RelayTransaction(const CTransaction& tx, const uint256& hash, const CDataStream& ss)
{
    CInv inv(MSG_TX, hash);
    {
        LOCK(cs_mapRelay);
        // Expire old relay messages
        while (!vRelayExpiration.empty() && vRelayExpiration.front().first < GetTime())
        {
            mapRelay.erase(vRelayExpiration.front().second);
            vRelayExpiration.pop_front();
        }

        // Save original serialized message so newer versions are preserved
        mapRelay.insert(std::make_pair(inv, ss));
        vRelayExpiration.push_back(std::make_pair(GetTime() + 15 * 60, inv));
    }

    RelayInventory(inv);
}
