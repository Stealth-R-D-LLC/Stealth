//
// Alert system
//

#include <boost/foreach.hpp>
#include <map>

#include "alert.h"
#include "key.h"
#include "net.h"
#include "sync.h"
#include "ui_interface.h"

using namespace std;

map<uint256, CAlert> mapAlerts;
CCriticalSection cs_mapAlerts;

// stealth: sync-checkpoint master key (520 bits, 130 hex)
// this is the public key for a secp256k1 private key
// e.g.: openssl ecparam -name secp256k1 -genkey -noout -out private-key.pem
//       openssl ec -in private-key.pem -pubout -text -out ecpubkey.txt
// hex is the "pub:" section in ecpubkey.txt with colons removed
static const char* pszMainKey = "04bf2dc2aac67e7d58365a3aa68946"
                                "e40026cd5c97a2606f41991b9614a0"
                                "818c905c28b344e3bbbb2cb072ba18"
                                "0a8974bba93d8bdc53bca40418bd87"
                                "9a733bb064";

// TestNet alerts pubKey
static const char* pszTestKey = "04f7588644485641221aa2e15e6dd11e1c75a1ced52e719b425e520e7f84e31bb0fc0a361fefa1fd423659fd8b204104dd046c2b3d60e42b0769ddd9fb0469ba91";

// TestNet alerts private key
// "308201130201010420b665cff1884e53da26376fd1b433812c9a5a8a4d5221533b15b9629789bb7e42a081a53081a2020101302c06072a8648ce3d0101022100fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f300604010004010704410479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8022100fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141020101a1440342000471dc165db490094d35cde15b1f5d755fa6ad6f2b5ed0f340e3f17f57389c3c2af113a8cbcc885bde73305a553b5640c83021128008ddf882e856336269080496"

void CUnsignedAlert::SetNull()
{
    nVersion = 1;
    nRelayUntil = 0;
    nExpiration = 0;
    nID = 0;
    nCancel = 0;
    setCancel.clear();
    nMinVer = 0;
    nMaxVer = 0;
    setSubVer.clear();
    nPriority = 0;

    strComment.clear();
    strStatusBar.clear();
    strReserved.clear();
}

std::string CUnsignedAlert::ToString() const
{
    std::string strSetCancel;
    BOOST_FOREACH(int n, setCancel)
        strSetCancel += strprintf("%d ", n);
    std::string strSetSubVer;
    BOOST_FOREACH(std::string str, setSubVer)
        strSetSubVer += "\"" + str + "\" ";
    return strprintf(
        "CAlert(\n"
        "    nVersion     = %d\n"
        "    nRelayUntil  = %" PRId64 "\n"
        "    nExpiration  = %" PRId64 "\n"
        "    nID          = %d\n"
        "    nCancel      = %d\n"
        "    setCancel    = %s\n"
        "    nMinVer      = %d\n"
        "    nMaxVer      = %d\n"
        "    setSubVer    = %s\n"
        "    nPriority    = %d\n"
        "    strComment   = \"%s\"\n"
        "    strStatusBar = \"%s\"\n"
        ")\n",
        nVersion,
        nRelayUntil,
        nExpiration,
        nID,
        nCancel,
        strSetCancel.c_str(),
        nMinVer,
        nMaxVer,
        strSetSubVer.c_str(),
        nPriority,
        strComment.c_str(),
        strStatusBar.c_str());
}

void CUnsignedAlert::print() const
{
    printf("%s", ToString().c_str());
}

void CAlert::SetNull()
{
    CUnsignedAlert::SetNull();
    vchMsg.clear();
    vchSig.clear();
}

bool CAlert::IsNull() const
{
    return (nExpiration == 0);
}

uint256 CAlert::GetHash() const
{
    return Hash(this->vchMsg.begin(), this->vchMsg.end());
}

bool CAlert::IsInEffect() const
{
    return (GetAdjustedTime() < nExpiration);
}

bool CAlert::Cancels(const CAlert& alert) const
{
    if (!IsInEffect()) {
        return false; // this was a no-op before 31403
    }

    // alerts with 0 id will cancel all alerts except 1 id alerts
    if (nID == 0) {
         return (alert.nID != 1);
    }

    // only alerts with ID 1 will cancel alerts with ID 0
    if (alert.nID == 0) {
         return (nID == 1);
    }
    
    return (alert.nID <= nCancel || setCancel.count(alert.nID));
}

bool CAlert::AppliesTo(int nVersion, std::string strSubVerIn) const
{
    // TODO: rework for client-version-embedded-in-strSubVer ?
    return (IsInEffect() &&
            nMinVer <= nVersion && nVersion <= nMaxVer &&
            (setSubVer.empty() || setSubVer.count(strSubVerIn)));
}

bool CAlert::AppliesToMe() const
{
    return AppliesTo(PROTOCOL_VERSION, FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<std::string>()));
}

bool CAlert::RelayTo(CNode* pnode) const
{
    if (!IsInEffect())
        return false;
    // returns true if wasn't already contained in the set
    if (pnode->setKnown.insert(GetHash()).second)
    {
        if (AppliesTo(pnode->nVersion, pnode->strSubVer) ||
            AppliesToMe() ||
            GetAdjustedTime() < nRelayUntil)
        {
            pnode->PushMessage("alert", *this);
            return true;
        }
    }
    return false;
}

bool CAlert::CheckSignature() const
{
    CKey key;
    if (!key.SetPubKey(ParseHex(fTestNet ? pszTestKey : pszMainKey)))
        return error("CAlert::CheckSignature() : SetPubKey failed");
    if (!key.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
        return error("CAlert::CheckSignature() : verify signature failed");

    // Now unserialize the data
    CDataStream sMsg(vchMsg, SER_NETWORK, PROTOCOL_VERSION);
    sMsg >> *(CUnsignedAlert*)this;
    return true;
}

CAlert CAlert::getAlertByHash(const uint256 &hash)
{
    CAlert retval;
    {
        LOCK(cs_mapAlerts);
        map<uint256, CAlert>::iterator mi = mapAlerts.find(hash);
        if(mi != mapAlerts.end())
            retval = mi->second;
    }
    return retval;
}

bool CAlert::ProcessAlert()
{
    if (!CheckSignature())
        return false;
    if (!IsInEffect())
        return false;

    // alert.nID=max is reserved for if the alert key is
    // compromised. It must have a pre-defined message,
    // must never expire, must apply to all versions,
    // and must cancel all previous
    // alerts or it will be ignored (so an attacker can't
    // send an "everything is OK, don't panic" version that
    // cannot be overridden):
    int maxInt = std::numeric_limits<int>::max();
    if (nID == maxInt)
    {
        if (!(
                nExpiration == maxInt &&
                nCancel == (maxInt-1) &&
                nMinVer == 0 &&
                nMaxVer == maxInt &&
                setSubVer.empty() &&
                nPriority == maxInt &&
                strStatusBar == "URGENT: Alert key compromised, upgrade required"
                ))
            return false;
    }

    {
        LOCK(cs_mapAlerts);
        // Cancel previous alerts
        for (map<uint256, CAlert>::iterator mi = mapAlerts.begin(); mi != mapAlerts.end();)
        {
            const CAlert& alert = (*mi).second;
            if (Cancels(alert))
            {
                printf("cancelling alert %d\n", alert.nID);
                uiInterface.NotifyAlertChanged((*mi).first, CT_DELETED);
                mapAlerts.erase(mi++);
            }
            else if (!alert.IsInEffect())
            {
                printf("expiring alert %d\n", alert.nID);
                uiInterface.NotifyAlertChanged((*mi).first, CT_DELETED);
                mapAlerts.erase(mi++);
            }
            else
                mi++;
        }

        // Check if this alert has been cancelled
        BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
        {
            const CAlert& alert = item.second;
            if (alert.Cancels(*this))
            {
                printf("alert already cancelled by %d\n", alert.nID);
                return false;
            }
        }

        // Add to mapAlerts
        mapAlerts.insert(make_pair(GetHash(), *this));
        // Notify UI if it applies to me and wouldn't cancel itself
        if(AppliesToMe() && (nID > nCancel)) {
            uiInterface.NotifyAlertChanged(GetHash(), CT_NEW);
        }

    printf("accepted alert %d, AppliesToMe()=%d\n", nID, AppliesToMe());
    return true;
  }
}
