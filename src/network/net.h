// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_NET_H
#define BITCOIN_NET_H

#include <deque>
#include <boost/array.hpp>
#include <boost/foreach.hpp>
#include <openssl/rand.h>

#ifndef WIN32
#include <arpa/inet.h>
#endif

#include "mruset.h"
#include "netbase.h"
#include "protocol.h"
#include "addrman.h"
#include "chainparams.hpp"

class CRequestTracker;
class CNode;
class CBlockIndex;

extern const CAddress CADDR_NULL;

extern int nBestHeight;

extern int GetTargetSpacing(const int nHeight);


inline unsigned int ReceiveBufferSize() { return 1000*GetArg("-maxreceivebuffer",
                                                             chainParams.DEFAULT_MAXRECEIVEBUFFER); }
inline unsigned int SendBufferSize() { return 1000*GetArg("-maxsendbuffer",
                                                          chainParams.DEFAULT_MAXSENDBUFFER); }

void AddOneShot(std::string strDest);
bool RecvLine(SOCKET hSocket, std::string& strLine);
bool GetMyExternalIP(CNetAddr& ipRet);
void AddressCurrentlyConnected(const CService& addr);
CNode* FindNode(const CNetAddr& ip);
CNode* FindNode(const CService& ip);
CNode* ConnectNode(CAddress addrConnect, const char *strDest = NULL, int64_t nTimeout=0);
void MapPort(bool fUseUPnP);
unsigned short GetListenPort();
bool BindListenPort(const CService &bindAddr, std::string& strError=REF(std::string()));
void StartNode(void* parg);
void StartTor(void* parg);
bool StopNode();

enum
{
    LOCAL_NONE,   // unknown
    LOCAL_IF,     // address a local interface listens on
    LOCAL_BIND,   // address explicit bound to
    LOCAL_UPNP,   // address reported by UPnP
    LOCAL_IRC,    // address reported by IRC (deprecated)
    LOCAL_HTTP,   // address reported by whatismyip.com and similar
    LOCAL_MANUAL, // address explicitly specified (-externalip=)

    LOCAL_MAX
};

bool IsPeerAddrLocalGood(CNode *pnode);
void SetLimited(enum Network net, bool fLimited = true);
bool IsLimited(enum Network net);
bool IsLimited(const CNetAddr& addr);
bool AddLocal(const CService& addr, int nScore = LOCAL_NONE);
bool AddLocal(const CNetAddr& addr, int nScore = LOCAL_NONE);
bool SeenLocal(const CService& addr);
bool IsLocal(const CService& addr);
bool GetLocal(CService &addr, const CNetAddr *paddrPeer = NULL);
bool IsReachable(const CNetAddr &addr);
void SetReachable(enum Network net, bool fFlag = true);
CAddress GetLocalAddress(const CNetAddr *paddrPeer = NULL);


enum
{
    MSG_TX = 1,
    MSG_BLOCK,
};

class CRequestTracker
{
public:
    void (*fn)(void*, CDataStream&);
    void* param1;

    explicit CRequestTracker(void (*fnIn)(void*, CDataStream&)=NULL, void* param1In=NULL)
    {
        fn = fnIn;
        param1 = param1In;
    }

    bool IsNull()
    {
        return fn == NULL;
    }
};


/** Thread types */
enum threadId
{
    THREAD_SOCKETHANDLER,
    THREAD_OPENCONNECTIONS,
    THREAD_MESSAGEHANDLER,
#ifdef WITH_MINER
    THREAD_MINER,
#endif
    THREAD_RPCLISTENER,
    THREAD_UPNP,
    THREAD_DNSSEED,
    THREAD_ADDEDCONNECTIONS,
    THREAD_DUMPADDRESS,
    THREAD_REGISTRYSYNC,
    THREAD_RPCHANDLER,
    THREAD_STAKEMINTER,
    THREAD_QPOSMINTER,

    THREAD_MAX
};

extern bool fClient;
extern bool fDiscover;
extern bool fUseUPnP;
extern uint64_t nLocalServices;
extern uint64_t nLocalHostNonce;
extern CAddress addrSeenByPeer;
extern boost::array<int, THREAD_MAX> vnThreadsRunning;
extern CAddrMan addrman;

extern std::vector<CNode*> vNodes;
extern CCriticalSection cs_vNodes;
extern std::map<CInv, CDataStream> mapRelay;
extern std::deque<std::pair<int64_t, CInv> > vRelayExpiration;
extern CCriticalSection cs_mapRelay;
extern std::map<CInv, int64_t> mapAlreadyAskedFor;




class CNodeStats
{
public:
    uint64_t nServices;
    int64_t nLastSend;
    int64_t nLastRecv;
    int64_t nTimeConnected;
    std::string addrName;
    int nVersion;
    std::string cleanSubVer;
    bool fInbound;
    int64_t nReleaseTime;
    int nStartingHeight;
    int nMisbehavior;
};





/** Information about a peer */
class CNode
{
public:
    // socket
    uint64_t nServices;
    SOCKET hSocket;
    CDataStream vSend;
    CDataStream vRecv;
    CCriticalSection cs_vSend;
    CCriticalSection cs_vRecv;
    int64_t nLastSend;
    int64_t nLastRecv;
    int64_t nLastSendEmpty;
    int64_t nTimeConnected;
    int nHeaderStart;
    unsigned int nMessageStart;
    CAddress addr;
    std::string addrName;
    CService addrLocal;
    // protocol version
    int nVersion;
    // strSubVer is whatever byte array we read from the wire. However, this
    // field is intended to be printed out, displayed to humans in
    // various forms and so on. So we sanitize it and store the sanitized
    // version in cleanSubVer. The original should be used when dealing with
    // the network or wire types and the cleaned string used when displayed or
    // logged.
    std::string strSubVer, cleanSubVer;
    bool fOneShot;
    bool fClient;
    bool fInbound;
    bool fVerified;
    bool fNetworkNode;
    bool fSuccessfullyConnected;
    bool fDisconnect;
    int nOrphans;
    CSemaphoreGrant grantOutbound;
private:
    int nTimesVersionSent;
protected:
    int nRefCount;

    // Denial-of-service detection/prevention
    // Key is IP address, value is banned-until-time
    static std::map<CNetAddr, int64_t> setBanned;
    static CCriticalSection cs_setBanned;
    int nMisbehavior;

public:
    int64_t nReleaseTime;
    std::map<uint256, CRequestTracker> mapRequests;
    CCriticalSection cs_mapRequests;
    uint256 hashContinue;
    CBlockIndex* pindexLastGetBlocksBegin;
    uint256 hashLastGetBlocksEnd;
    int64_t nLastGetBlocks;
    int nStartingHeight;

    // flood relay
    std::vector<CAddress> vAddrToSend;
    std::set<CAddress> setAddrKnown;
    bool fGetAddr;
    std::set<uint256> setKnown;
    uint256 hashCheckpointKnown; // ppcoin: known sent sync-checkpoint

    // inventory based relay
    mruset<CInv> setInventoryKnown;
    std::vector<CInv> vInventoryToSend;
    CCriticalSection cs_inventory;
    std::multimap<int64_t, CInv> mapAskFor;

    CNode(SOCKET hSocketIn, CAddress addrIn, std::string addrNameIn = "", bool fInboundIn=false) : vSend(SER_NETWORK,  INIT_PROTO_VERSION), vRecv(SER_NETWORK, INIT_PROTO_VERSION)
    {
        nServices = 0;
        hSocket = hSocketIn;
        nLastSend = 0;
        nLastRecv = 0;
        nLastSendEmpty = GetTime();
        nTimeConnected = GetTime();
        nHeaderStart = -1;
        nMessageStart = -1;
        addr = addrIn;
        addrName = addrNameIn == "" ? addr.ToStringIPPort() : addrNameIn;
        nVersion = 0;
        strSubVer = "";
        fOneShot = false;
        fClient = false; // set by version message
        fInbound = fInboundIn;
        fVerified = false;
        fNetworkNode = false;
        fSuccessfullyConnected = false;
        fDisconnect = false;
        nOrphans = 0;
        nTimesVersionSent = 0;
        nRefCount = 0;
        nReleaseTime = 0;
        hashContinue = 0;
        pindexLastGetBlocksBegin = 0;
        hashLastGetBlocksEnd = 0;
        nLastGetBlocks = 0;
        nStartingHeight = -1;
        fGetAddr = false;
        nMisbehavior = 0;
        hashCheckpointKnown = 0;
        setInventoryKnown.max_size(SendBufferSize() / 1000);

        printf("CNode(): %s-bound pfrom-addr %s\n",
               fInbound ? "in" : "out",
               addrName.c_str());

        PushVersion();
    }

    ~CNode()
    {
        if (hSocket != INVALID_SOCKET)
        {
            closesocket(hSocket);
            hSocket = INVALID_SOCKET;
        }
    }

private:
    CNode(const CNode&);
    void operator=(const CNode&);
public:


    int GetRefCount()
    {
        return std::max(nRefCount, 0) + (GetTime() < nReleaseTime ? 1 : 0);
    }

    CNode* AddRef(int64_t nTimeout = 0)
    {
        if (nTimeout != 0)
            nReleaseTime = std::max(nReleaseTime, GetTime() + nTimeout);
        else
            nRefCount++;
        return this;
    }

    void Release()
    {
        nRefCount--;
    }



    void AddAddressKnown(const CAddress& addr)
    {
        setAddrKnown.insert(addr);
    }

    void PushAddress(const CAddress& addr)
    {
        // Known checking here is only to save space from duplicates.
        // SendMessages will filter it again for knowns that were added
        // after addresses were pushed.
        if (addr.IsValid(nVersion) && !setAddrKnown.count(addr))
        {
            vAddrToSend.push_back(addr);
        }
    }


    void AddInventoryKnown(const CInv& inv)
    {
        {
            LOCK(cs_inventory);
            setInventoryKnown.insert(inv);
        }
    }

    bool PushInventory(const CInv& inv)
    {
        static const size_t nMaxInvSize = (size_t)(2 * chainParams.GETBLOCKS_LIMIT);
        {
            LOCK(cs_inventory);
            if (setInventoryKnown.count(inv))
            {
                if (fDebugNet)
                {
                    printf("inventory known\n");
                }
                return false;
            }
            if (vInventoryToSend.size() >= nMaxInvSize)
            {
                if (fDebugNet)
                {
                    printf("inventory exceeds %lu\n",
                           (unsigned long)nMaxInvSize);
                }
                return false;
            }
            vInventoryToSend.push_back(inv);
            if (fDebugNet && ((vInventoryToSend.size() % 1000) == 0))
            {
                printf("Inventory size is: %lu\n",
                       (unsigned long)vInventoryToSend.size());
            }
        }
        return true;
    }

    void AskFor(const CInv& inv)
    {
        static int64_t nLastTime;
        // We're using mapAskFor as a priority queue,
        // the key is the earliest time the request can be sent
        int64_t nRequestTime = mapAlreadyAskedFor[inv];
        // Make sure not to reuse time indexes to keep things in the same order
        // Wait at least 1 second
        int64_t nNow = (GetTime() - 1) * 1000000;
        ++nLastTime;
        nNow = std::max(nNow, nLastTime);
        nLastTime = nNow;

        if (fDebugNet)
        {
            printf("AskFor(): %s %s @%" PRId64 " (%s)\n   request %" PRId64 " (%s)\n",
                   addrName.c_str(),
                   inv.ToString().c_str(),
                   nNow,
                   DateTimeStrFormat("%H:%M:%S",
                                     nNow/1000000).c_str(),
                   nRequestTime,
                   DateTimeStrFormat("%H:%M:%S",
                                     nRequestTime/1000000).c_str());
        }

        // Each retry is half of target spacing
        int nSpacing = GetTargetSpacing(nBestHeight);
        nRequestTime = std::max(nRequestTime + (nSpacing * 1000000)/2, nNow);
        mapAskFor.insert(std::make_pair(nRequestTime, inv));
    }



    void BeginMessage(const char* pszCommand)
    {
        ENTER_CRITICAL_SECTION(cs_vSend);
        if (nHeaderStart != -1)
            AbortMessage();
        nHeaderStart = vSend.size();
        vSend << CMessageHeader(pszCommand, 0);
        nMessageStart = vSend.size();
        if (fDebugNet)
        {
            printf("sending: %s\n", pszCommand);
        }
    }

    void AbortMessage()
    {
        if (nHeaderStart < 0)
            return;
        vSend.resize(nHeaderStart);
        nHeaderStart = -1;
        nMessageStart = -1;
        LEAVE_CRITICAL_SECTION(cs_vSend);

        if (fDebugNet)
            printf("(aborted)\n");
    }

    void EndMessage()
    {
        if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
        {
            printf("dropmessages DROPPING SEND MESSAGE\n");
            AbortMessage();
            return;
        }

        if (nHeaderStart < 0)
            return;

        // Set the size
        unsigned int nSize = vSend.size() - nMessageStart;
        memcpy((char*)&vSend[nHeaderStart] + CMessageHeader::MESSAGE_SIZE_OFFSET, &nSize, sizeof(nSize));

        // Set the checksum
        uint256 hash = Hash(vSend.begin() + nMessageStart, vSend.end());
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        assert(nMessageStart - nHeaderStart >= CMessageHeader::CHECKSUM_OFFSET + sizeof(nChecksum));
        memcpy((char*)&vSend[nHeaderStart] + CMessageHeader::CHECKSUM_OFFSET, &nChecksum, sizeof(nChecksum));

        if (fDebugNet) {
            printf("Message size: %d bytes\n", nSize);
            printf("    (vSend.hex() @ version %d):\n%s\n",
                   vSend.nVersion,
                   vSend.hex(16, "    ").c_str());
        }


        nHeaderStart = -1;
        nMessageStart = -1;
        LEAVE_CRITICAL_SECTION(cs_vSend);
    }

    void EndMessageAbortIfEmpty()
    {
        if (nHeaderStart < 0)
            return;
        int nSize = vSend.size() - nMessageStart;
        if (nSize > 0)
            EndMessage();
        else
            AbortMessage();
    }



    void PushVersion();


    void PushMessage(const char* pszCommand)
    {
        try
        {
            BeginMessage(pszCommand);
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }

    template<typename T1>
    void PushMessage(const char* pszCommand, const T1& a1)
    {
        try
        {
            BeginMessage(pszCommand);
            vSend << a1;
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }

    template<typename T1, typename T2>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2)
    {
        try
        {
            BeginMessage(pszCommand);
            vSend << a1 << a2;
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }

    template<typename T1, typename T2, typename T3>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3)
    {
        try
        {
            BeginMessage(pszCommand);
            vSend << a1 << a2 << a3;
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }

    template<typename T1, typename T2, typename T3, typename T4>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4)
    {
        try
        {
            BeginMessage(pszCommand);
            vSend << a1 << a2 << a3 << a4;
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }

    template<typename T1, typename T2, typename T3, typename T4, typename T5>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5)
    {
        try
        {
            BeginMessage(pszCommand);
            vSend << a1 << a2 << a3 << a4 << a5;
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }

    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6)
    {
        try
        {
            BeginMessage(pszCommand);
            vSend << a1 << a2 << a3 << a4 << a5 << a6;
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }

    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7)
    {
        try
        {
            BeginMessage(pszCommand);
            vSend << a1 << a2 << a3 << a4 << a5 << a6 << a7;
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }

    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8)
    {
        try
        {
            BeginMessage(pszCommand);
            vSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8;
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }

    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8, const T9& a9)
    {
        try
        {
            BeginMessage(pszCommand);
            vSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9;
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }


    void PushRequest(const char* pszCommand,
                     void (*fn)(void*, CDataStream&), void* param1)
    {
        uint256 hashReply;
        RAND_bytes((unsigned char*)&hashReply, sizeof(hashReply));

        {
            LOCK(cs_mapRequests);
            mapRequests[hashReply] = CRequestTracker(fn, param1);
        }

        PushMessage(pszCommand, hashReply);
    }

    template<typename T1>
    void PushRequest(const char* pszCommand, const T1& a1,
                     void (*fn)(void*, CDataStream&), void* param1)
    {
        uint256 hashReply;
        RAND_bytes((unsigned char*)&hashReply, sizeof(hashReply));

        {
            LOCK(cs_mapRequests);
            mapRequests[hashReply] = CRequestTracker(fn, param1);
        }

        PushMessage(pszCommand, hashReply, a1);
    }

    template<typename T1, typename T2>
    void PushRequest(const char* pszCommand, const T1& a1, const T2& a2,
                     void (*fn)(void*, CDataStream&), void* param1)
    {
        uint256 hashReply;
        RAND_bytes((unsigned char*)&hashReply, sizeof(hashReply));

        {
            LOCK(cs_mapRequests);
            mapRequests[hashReply] = CRequestTracker(fn, param1);
        }

        PushMessage(pszCommand, hashReply, a1, a2);
    }



    void PushGetBlocks(CBlockIndex* pindexBegin, uint256 hashEnd);
    bool IsSubscribed(unsigned int nChannel);
    void Subscribe(unsigned int nChannel, unsigned int nHops=0);
    void CancelSubscribe(unsigned int nChannel);
    void CloseSocketDisconnect();
    void Cleanup();


    // Denial-of-service detection/prevention
    // The idea is to detect peers that are behaving
    // badly and disconnect/ban them, but do it in a
    // one-coding-mistake-won't-shatter-the-entire-network
    // way.
    // IMPORTANT:  There should be nothing I can give a
    // node that it will forward on that will make that
    // node's peers drop it. If there is, an attacker
    // can isolate a node and/or try to split the network.
    // Dropping a node for sending stuff that is invalid
    // now but might be valid in a later version is also
    // dangerous, because it can cause a network split
    // between nodes running old code and nodes running
    // new code.
    static void ClearBanned(); // needed for unit testing
    static bool IsBanned(CNetAddr ip);
    bool Misbehaving(int howmuch); // 1 == a little, 100 == a lot
    int GetMisbehavior() const;
    void copyStats(CNodeStats &stats);
};


inline void RelayInventory(const CInv& inv)
{
    // Put on lists to offer to the other nodes
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
            pnode->PushInventory(inv);
    }
}

template<typename T>
void RelayMessage(const CInv& inv, const T& a)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(10000);
    ss << a;
    RelayMessage(inv, ss);
}

template<>
inline void RelayMessage<>(const CInv& inv, const CDataStream& ss)
{
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
class CTransaction;
void RelayTransaction(const CTransaction& tx, const uint256& hash);
void RelayTransaction(const CTransaction& tx, const uint256& hash, const CDataStream& ss);

#endif
