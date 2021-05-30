// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qPoS.hpp"

#include "main.h"

#include "boost/assign.hpp"

#include <map>

using namespace std;

// qPoS
map<txnouttype, QPKeyType> mapQPoSKeyTypes = boost::assign::map_list_of
      (TX_SETOWNER, QPKEY_OWNER)
      (TX_SETMANAGER, QPKEY_MANAGER)
      (TX_SETDELEGATE, QPKEY_DELEGATE)
      (TX_SETCONTROLLER, QPKEY_CONTROLLER);

void ExtractPurchase(const valtype &vch, qpos_purchase &prchsRet)
{
    valtype::const_iterator first = vch.begin();
    valtype::const_iterator last = first;
    if (!IncrementN(vch, last, 8))
    {
        return;
    }
    prchsRet.value = GETUINT64(first, last);
    first = last;
    prchsRet.keys.clear();
    while (true)
    {
        if (!IncrementN(vch, last, 33))
        {
            break;
        }
        prchsRet.keys.push_back(CPubKey(valtype(first, last)));
        first = last;
    }
    last = first;
    if ((prchsRet.keys.size() == 3) || (prchsRet.keys.size() == 4))
    {
        if (!IncrementN(vch, last, 4))
        {
            return;
        }
        prchsRet.pcm = GETUINT32(first, last);
    }
    else
    {
        prchsRet.pcm = 0;
    }
    first = last;
    last = vch.end();
    string sTemp(first, last);
    prchsRet.alias.clear();
    bool fAllInt = true;
    for (unsigned int i = 0; i < sTemp.size(); ++i)
    {
        char c = sTemp[i];
        if (c)
        {
            if ((c < '0') || (c > '9'))
            {
                fAllInt = false;
            }
            prchsRet.alias.push_back(c);
        }
        else
        {
            break;
        }
    }
    prchsRet.nft = 0;
    if (fAllInt)
    {
        int k = 1;
        for (int i = prchsRet.alias.size() - 1; i >= 0; --i)
        {
            prchsRet.nft += k * (prchsRet.alias[i] - '0');
            // prevents overflow -- there will never be this many NFTs
            if (prchsRet.nft > 100000)
            {
                break;
            }
            k *= 10;
        }
        // since nft retains full info of original alias, we can reassign here
        if (mapNfts.count(k))
        {
            prchsRet.alias = mapNfts[k].strNickname;
        }
    }
    else
    {
        string sLC = ToLowercaseSafe(prchsRet.alias);
        if (mapNftLookup.count(sLC))
        {
            prchsRet.nft = mapNftLookup[sLC];
            // prchsRet.alias = mapNft[prchsRet.nft].strNickname;
        }
    }
}

void ExtractPurchase(const valtype &vch, QPTxDetails &deetsRet)
{
    deetsRet.Clear();
    qpos_purchase purchase;
    ExtractPurchase(vch, purchase);
    deetsRet.alias = purchase.alias;
    deetsRet.keys = purchase.keys;
    deetsRet.pcm = purchase.pcm;
    deetsRet.value = purchase.value;
    deetsRet.id = purchase.nft;
}

// ensure setkeyRet.keytype is assigned
void ExtractSetKey(const valtype &vch, qpos_setkey &setkeyRet)
{
    valtype::const_iterator first = vch.begin();
    valtype::const_iterator last = first;
    if (!IncrementN(vch, last, 4))
    {
        return;
    }
    setkeyRet.id = GETUINT(first, last);
    first = last;
    if (!IncrementN(vch, last, 33))
    {
        return;
    }
    setkeyRet.key = CPubKey(valtype(first, last));
    if (setkeyRet.keytype == QPKEY_DELEGATE)
    {
        first = last;
        if (!IncrementN(vch, last, 4))
        {
            return;
        }
        setkeyRet.pcm = GETUINT32(first, last);
    }
    else
    {
        setkeyRet.pcm = 0;
    }
}

void ExtractSetKey(const valtype &vch, QPTxDetails &deetsRet)
{
    qpos_setkey setkey;
    txnouttype typetxo(static_cast<txnouttype>(deetsRet.t));
    if (mapQPoSKeyTypes.count(typetxo))
    {
        setkey.keytype = mapQPoSKeyTypes[typetxo];
    }
    ExtractSetKey(vch, setkey);
    deetsRet.Clear();
    deetsRet.t = typetxo;
    deetsRet.id = setkey.id;
    deetsRet.keys.clear();
    deetsRet.keys.push_back(setkey.key);
    deetsRet.pcm = setkey.pcm;
}

void ExtractSetState(const valtype &vch, qpos_setstate &setstateRet)
{
    valtype::const_iterator first = vch.begin();
    valtype::const_iterator last = first;
    if (!IncrementN(vch, last, 4))
    {
        return;
    }
    setstateRet.id = GETUINT(first, last);
}

void ExtractSetState(const valtype &vch, QPTxDetails &deetsRet)
{
    deetsRet.Clear();
    qpos_setstate setstate;
    ExtractSetState(vch, setstate);
    deetsRet.id = setstate.id;
}

void ExtractClaim(const valtype &vch, qpos_claim &claimRet)
{
    valtype::const_iterator first = vch.begin();
    valtype::const_iterator last = first;
    if (!IncrementN(vch, last, 33))
    {
        return;
    }
    claimRet.key = CPubKey(valtype(first, last));
    first = last;
    if (!IncrementN(vch, last, 8))
    {
        return;
    }
    claimRet.value = GETUINT64(first, last);
}

void ExtractClaim(const valtype &vch, QPTxDetails &deetsRet)
{
    deetsRet.Clear();
    qpos_claim claim;
    ExtractClaim(vch, claim);
    deetsRet.keys.clear();
    deetsRet.keys.push_back(claim.key);
    deetsRet.value = claim.value;
}

void ExtractSetMeta(const valtype &vch, qpos_setmeta &setmetaRet)
{
    valtype::const_iterator first = vch.begin();
    valtype::const_iterator last = first;
    if (!IncrementN(vch, last, 4))
    {
        return;
    }
    setmetaRet.id = GETUINT(first, last);

    first = last;
    if (!IncrementN(vch, last, 16))
    {
        return;
    }

    valtype::const_iterator e;
    for (e = first; e != last; ++e)
    {
        if (*e == static_cast<unsigned char>(0))
        {
            break;
        }
    }
    setmetaRet.key = string(first, e);

    first = last;
    if (!IncrementN(vch, last, 40))
    {
        return;
    }

    for (e = first; e != last; ++e)
    {
        if (*e == static_cast<unsigned char>(0))
        {
            break;
        }
    }
    setmetaRet.value = string(first, e);
}

void ExtractSetMeta(const valtype &vch, QPTxDetails &deetsRet)
{
    deetsRet.Clear();
    qpos_setmeta setmeta;
    ExtractSetMeta(vch, setmeta);
    deetsRet.id = setmeta.id;
    deetsRet.meta_key = setmeta.key;
    deetsRet.meta_value = setmeta.value;
}
