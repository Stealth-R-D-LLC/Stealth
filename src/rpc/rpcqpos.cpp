// Copyright (c) 2019 Stealth R&D LLC
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "wallet.h"
#include "bitcoinrpc.h"
#include "txdb-leveldb.h"

extern QPRegistry *pregistryMain;
extern CWallet* pwalletMain;

using namespace json_spirit;
using namespace std;

uint32_t PcmFromValue(const Value &value)
{
    double dPercent = value.get_real();
    if ((dPercent <= 0.0) || (dPercent > 100.0))
    {
        throw JSONRPCError(RPC_QPOS_INVALID_PAYOUT, "Invalid payout");
    }
    return static_cast<uint32_t>(roundint(dPercent * 1000));
}

Value exitreplay(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0 || !fTestNet)
    {
        throw runtime_error(
            "exitreplay\n"
            "Manually exits registry replay (testnet only).");
    }
    pregistryMain->ExitReplayMode();
    return true;
}

Value getstakerprice(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
    {
        throw runtime_error(
            "getstakerprice\n"
            "Returns the current staker price.");
    }
    uint32_t N = static_cast<uint32_t>(
                    pregistryMain->GetNumberQualified());
    return ValueFromAmount(GetStakerPrice(N, pindexBest->nMoneySupply));
}

Value getstakerid(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
    {
        throw runtime_error(
            "getstakerid <alias>\n"
            "Returns the id of the staker registered with <alias>.");
    }
    string sAlias = params[0].get_str();
    unsigned int nID;
    if (!pregistryMain->GetIDForAlias(sAlias, nID))
    {
        throw JSONRPCError(RPC_QPOS_STAKER_NONEXISTENT,
                            "Staker doesn't exist");
    }
    return static_cast<int64_t>(nID);
}

void ExtractQPoSRPCEssentials(const Array &params,
                              string &txidRet,
                              unsigned int &nOutRet,
                              string &sAliasRet,
                              valtype &vchPubKeyRet,
                              unsigned int &nIDRet,
                              bool fStakerShouldExist)
{

    if (pwalletMain->IsLocked())
    {
      throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED,
          "Error: Please enter the wallet passphrase "
            "with walletpassphrase first.");
    }

    // params: txid, vout, alias
    if (params.size() < 3)
    {
        // should never happen
        throw runtime_error("ExtractQPoSRPCEssentials(): not enough params");
    }

    txidRet = params[0].get_str();
    if (txidRet.size() != 64)
    {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "txid is not 64 char");
    }

    int nOutParam = params[1].get_int();
    if (nOutParam < 0)
    {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                           "vout must be positive");
    }

    nOutRet = static_cast<unsigned int>(nOutParam);

    string sAliasIn = params[2].get_str();
    bool fAllInt = true;
    for (unsigned int i = 0; i < sAliasIn.size(); ++i)
    {
        char c = sAliasIn[i];
        if ((c < '0') || (c > '9'))
        {
            fAllInt = false;
            break;
        }
    }

    // TODO: Refactor from qPoS.cpp
    nIDRet = 0;
    if (fAllInt)
    {
        int k = 1;
        for (int i = sAliasIn.size() - 1; i >= 0; --i)
        {
            nIDRet += k * (sAliasIn[i] - '0');
            // prevents overflow -- there will never be this many NFTs
            if (nIDRet > 100000)
            {
                break;
            }
            k *= 10;
        }
    }

    if (nIDRet)
    {
        if (!fStakerShouldExist)
        {
            if (!pregistryMain->NftIsAvailable(nIDRet, sAliasRet))
            {
                throw JSONRPCError(RPC_QPOS_NFT_NOT_AVAILABLE,
                                   "NFT is not available (by ID)");
            }
        }
    }
    else
    {
        string sLC = ToLowercaseSafe(sAliasIn);
        if (mapNftLookup.count(sLC))
        {
            if (fStakerShouldExist)
            {
               nIDRet = mapNftLookup[sLC];
            }
            else
            {
                if (!pregistryMain->NftIsAvailable(sLC, nIDRet))
                {
                    throw JSONRPCError(RPC_QPOS_NFT_NOT_AVAILABLE,
                                       "NFT is not available (by key)");
                }
            }
            sAliasRet = mapNfts[nIDRet].strNickname;
            if (ToLowercaseSafe(sAliasRet) != sLC)
            {
                throw runtime_error("ExtractQPoSRPCEssentials(): "
                                       "TSNH NFT alias mismatch");
            }
        }
        else
        {
            sAliasRet = sAliasIn;
        }
    }

    if (fStakerShouldExist)
    {
        if (!pregistryMain->GetIDForAlias(sAliasRet, nIDRet))
        {
            throw JSONRPCError(RPC_QPOS_STAKER_NONEXISTENT,
                               "Staker does not exist");
        }
    }
    else
    {
        string sLC;  // lower case normalized alias
        if (!pregistryMain->AliasIsAvailable(sAliasRet, sLC))
        {
            throw JSONRPCError(RPC_QPOS_ALIAS_NOT_AVAILABLE,
                               "Alias is not available");
        }
    }

    if (params.size() > 3)
    {
        vchPubKeyRet = ParseHex(params[3].get_str());
        if (vchPubKeyRet.size() != 33)
        {
            throw JSONRPCError(RPC_QPOS_INVALID_QPOS_PUBKEY,
                               "Key is not a compressed pubkey");
        }
    }
    else
    {
        vchPubKeyRet.clear();
    }
}

Value purchasestaker(const Array &params, bool fHelp)
{
    if (fHelp || (params.size()  < 4)  ||
                 (params.size()  > 9)  ||
                 (params.size() == 6)  ||
                 (params.size() == 7)  ||
                 (params.size() == 8))
    {
        throw runtime_error(
            "purchasestaker <txid> <vout> <alias> <owner> [amount] "
               "[manager delegate controller payout]\n"
            "    <txid> is the transaction ID of the input\n"
            "    <vout> is the prevout index of the input\n"
            "    <alias> is the case sensitive staker alias\n"
            "            or the ID of an available NFT\n"
            "    <owner> is the owner compressed pubkey\n"
            "    [amount] is is the amount to pay\n"
            "        If the amount is not specified it will be calculated\n"
            "          automatically.\n"
            "        The amount is real and rounded to the nearest 0.000001\n"
            "    [manager], [delegate], and [controller] are compressed pubkeys\n"
            "        If manager, delegate, and controller are not specified then\n"
            "          they are taken from owner.\n"
            "    [payout] is in percentage, and is rounded to the nearest\n"
            "          thousandths of a percent\n"
            "        Either just the owner key or all 4 keys plus the payout\n"
            "          must be specified." +
            HelpRequiringPassphrase());
    }

    string txid, sAlias;
    unsigned int nOut;
    valtype vchOwnerKey, vchManagerKey, vchDelegateKey, vchControllerKey;

    unsigned int nNftID;
    ExtractQPoSRPCEssentials(params, txid, nOut, sAlias, vchOwnerKey,
                             nNftID, false);

    uint32_t N = static_cast<uint32_t>(
                    pregistryMain->GetNumberQualified());
    int64_t nPrice = GetStakerPrice(N, pindexBest->nMoneySupply, true);
    int64_t nAmount;
    if (params.size() > 4)
    {
         nAmount = AmountFromValue(params[4]);
         if (nAmount < nPrice)
         {
             throw JSONRPCError(RPC_QPOS_STAKER_PRICE_TOO_LOW,
                                "Staker price is too low");
         }
         if (nAmount > (2 * nPrice))
         {
             throw JSONRPCError(RPC_QPOS_STAKER_PRICE_TOO_HIGH,
                                "Staker price is too high");
         }
         nPrice = nAmount;
    }

    uint32_t nPcm;
    if (params.size() > 8)
    {
        vchManagerKey = ParseHex(params[5].get_str());
        if (vchManagerKey.size() != 33)
        {
            throw JSONRPCError(RPC_QPOS_INVALID_QPOS_PUBKEY,
                               "Delegate key is not a compressed pubkey");
        }
        vchDelegateKey = ParseHex(params[6].get_str());
        if (vchDelegateKey.size() != 33)
        {
            throw JSONRPCError(RPC_QPOS_INVALID_QPOS_PUBKEY,
                               "Delegate key is not a compressed pubkey");
        }
        vchControllerKey = ParseHex(params[7].get_str());
        if (vchControllerKey.size() != 33)
        {
            throw JSONRPCError(RPC_QPOS_INVALID_QPOS_PUBKEY,
                               "Controller key is not a compressed pubkey");
        }
        nPcm = PcmFromValue(params[8]);
    }
    else
    {
        vchManagerKey = vchOwnerKey;
        vchDelegateKey = vchOwnerKey;
        vchControllerKey = vchOwnerKey;
        nPcm = 0;
    }

    CWalletTx wtx;

    // Purchase
    string strError = pwalletMain->PurchaseStaker(txid, nOut,
                                                  sAlias,
                                                  vchOwnerKey,
                                                  vchManagerKey,
                                                  vchDelegateKey,
                                                  vchControllerKey,
                                                  nPrice, nPcm, wtx);

    if (strError != "")
    {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    return wtx.GetHash().GetHex();
}

Value SetStakerKey(const Array& params,
                   opcodetype opSetKey,
                   uint32_t nPcm)
{
    string txid, sAlias;
    unsigned int nOut;
    valtype vchPubKey;
    unsigned int nID;

    ExtractQPoSRPCEssentials(params, txid, nOut, sAlias, vchPubKey,
                             nID, true);

    CWalletTx wtx;

    // SetKey
    string strError = pwalletMain->SetStakerKey(txid, nOut,
                                                nID, vchPubKey,
                                                opSetKey, nPcm, wtx);

    if (strError != "")
    {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    return wtx.GetHash().GetHex();
}

Value setstakerowner(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 5)
    {
        throw runtime_error(
            "setstakerowner <txid> <vout> <alias> <owner> iunderstand\n"
            "IMPORTANT: This transfers OWNERSHIP of the staker!\n"
            "    <txid> is the transaction ID of the input\n"
            "    <vout> is the prevout index of the input\n"
            "    <alias> is a non-case sensitive staker alias\n"
            "    <owner> is the owner compressed pubkey\n"
            "    \"iunderstand\" is a literal that must be included" +
            HelpRequiringPassphrase());
    }

    if (params[4].get_str() != "iunderstand")
    {
        throw JSONRPCError(RPC_QPOS_TRANSFER_UNACKNOWLEDGED,
                            "Transfer of ownership unacknowledged");
    }

    return SetStakerKey(params, OP_SETOWNER, 0);
}

Value setstakermanager(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
    {
        throw runtime_error(
            "setstakermanager <txid> <vout> <alias> <manager>\n"
            "    <txid> is the transaction ID of the input\n"
            "    <vout> is the prevout index of the input\n"
            "    <alias> is a non-case sensitive staker alias\n"
            "    <manager> is the controller compressed pubkey" +
            HelpRequiringPassphrase());
    }

    return SetStakerKey(params, OP_SETMANAGER, 0);
}

Value setstakerdelegate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 5)
    {
        throw runtime_error(
            "setstakerdelegate <txid> <vout> <alias> <delegate> <payout>\n"
            "    <txid> is the transaction ID of the input\n"
            "    <vout> is the prevout index of the input\n"
            "    <alias> is a non-case sensitive staker alias\n"
            "    <delegate> is the delegate compressed pubkey\n"
            "    <payout> is the fraction of block rewards to pay to\n"
            "        the delegate in millipercent" +
            HelpRequiringPassphrase());
    }

    uint32_t nPcm = PcmFromValue(params[4]);

    return SetStakerKey(params, OP_SETDELEGATE, nPcm);
}

Value setstakercontroller(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
    {
        throw runtime_error(
            "setstakercontroller <txid> <vout> <alias> <controller>\n"
            "    <txid> is the transaction ID of the input\n"
            "    <vout> is the prevout index of the input\n"
            "    <alias> is a non-case sensitive staker alias\n"
            "    <controller> is the controller compressed pubkey" +
            HelpRequiringPassphrase());
    }

    return SetStakerKey(params, OP_SETCONTROLLER, 0);
}

Value SetStakerState(const Array& params, bool fEnable)
{
    string txid, sAlias;
    unsigned int nOut;
    valtype vchPubKey;
    unsigned int nID;

    ExtractQPoSRPCEssentials(params, txid, nOut, sAlias, vchPubKey,
                             nID, true);

    CWalletTx wtx;

    // SetKey
    string strError = pwalletMain->SetStakerState(txid, nOut,
                                                  nID, vchPubKey,
                                                  fEnable, wtx);

    if (strError != "")
    {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    return wtx.GetHash().GetHex();
}

Value enablestaker(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
    {
        throw runtime_error(
            "enablestaker <txid> <vout> <alias>\n"
            "    <txid> is the transaction ID of the input\n"
            "    <vout> is the prevout index of the input\n"
            "    <alias> is a non-case sensitive staker alias" +
            HelpRequiringPassphrase());
    }

    return SetStakerState(params, true);
}

Value disablestaker(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
    {
        throw runtime_error(
            "disablestaker <txid> <vout> <alias>\n"
            "    <txid> is the transaction ID of the input\n"
            "    <vout> is the prevout index of the input\n"
            "    <alias> is a non-case sensitive staker alias" +
            HelpRequiringPassphrase());
    }

    return SetStakerState(params, false);
}

Value claimqposbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
    {
        throw runtime_error(
            "claimqposbalance <txid> <vout> <amount>\n"
            "    <txid> is the transaction ID of the input\n"
            "    <vout> is the prevout index of the input\n"
            "    <amount> is the amount to claim\n"
            "      Amount is real and rounded to the nearest 0.000001.\n"
            "      Claim plus change is returned to claimant hashed pubkey" +
            HelpRequiringPassphrase());
    }

    if (pwalletMain->IsLocked())
    {
      throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED,
          "Error: Please enter the wallet passphrase "
            "with walletpassphrase first.");
    }

    string txid = params[0].get_str();
    if (txid.size() != 64)
    {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "txid is not 64 char");
    }

    int nOutParam = params[1].get_int();
    if (nOutParam < 0)
    {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                           "vout must be positive");
    }

    unsigned int nOut = static_cast<unsigned int>(nOutParam);

    int64_t nAmount = AmountFromValue(params[2]);

    CWalletTx wtx;

    // SetKey
    string strError = pwalletMain->ClaimQPoSBalance(txid, nOut, nAmount, wtx);

    if (strError != "")
    {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    return wtx.GetHash().GetHex();
}

Value setstakermeta(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 5)
    {
        throw runtime_error(
            "setstakermeta <txid> <vout> <alias> <key> <value>\n"
            "    <txid> is the transaction ID of the input\n"
            "    <vout> is the prevout index of the input\n"
            "    <alias> is a non-case sensitive staker alias\n"
            "    <key> is the metadata key\n"
            "    <value> is the metatdata value" +
            HelpRequiringPassphrase());
    }

    string sKey = params[3].get_str();
    if (CheckMetaKey(sKey) == QPKEY_NONE)
    {
        throw JSONRPCError(RPC_QPOS_META_KEY_NOT_VALID,
                           "Meta key is not valid");
    }

    string sValue = params[4].get_str();
    if (!CheckMetaValue(sValue))
    {
            throw JSONRPCError(RPC_QPOS_META_VALUE_NOT_VALID,
                           "Meta value is not valid");
    }
                   
    string txid, sAlias;
    unsigned int nOut;
    valtype vchPubKey;
    unsigned int nID;

    Array a;
    for (int i = 0; i < 3; ++i)
    {
        a.push_back(params[i]);
    }

    ExtractQPoSRPCEssentials(a, txid, nOut, sAlias, vchPubKey, nID, true);

    CWalletTx wtx;

    // SetMeta
    string strError = pwalletMain->SetStakerMeta(txid, nOut,
                                                 nID, vchPubKey,
                                                 sKey, sValue, wtx);

    if (strError != "")
    {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    return wtx.GetHash().GetHex();
}

Value getstakerinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
    {
        throw runtime_error(
            "getstakerinfo <alias>\n"
            "<alias> is a non-case sensitive staker alias\n"
            "Returns exhaustive information about the staker");
    }

    string sAlias = params[0].get_str();

    unsigned int nID;
    if (!pregistryMain->GetIDForAlias(sAlias, nID))
    {
        throw JSONRPCError(RPC_QPOS_STAKER_NONEXISTENT,
                            "Staker doesn't exist");
    }

    Object obj;
    pregistryMain->GetStakerAsJSON(nID, obj, true);

    return obj;
}

Value getstakerauthorities(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
    {
        throw runtime_error(
            "getstakerauthorities <alias>\n"
            "<alias> is a non-case sensitive staker alias\n"
            "Returns the staker authorities as pubkeys and addresses");
    }

    string sAlias = params[0].get_str();

    unsigned int nID;
    if (!pregistryMain->GetIDForAlias(sAlias, nID))
    {
        throw JSONRPCError(RPC_QPOS_STAKER_NONEXISTENT,
                            "Staker doesn't exist");
    }

    CPubKey keyOwner, keyManager, keyDelegate, keyController;
    if (!pregistryMain->GetOwnerKey(nID, keyOwner))
    {
        throw runtime_error("TSNH: can't get owner key");
    }
    if (!pregistryMain->GetManagerKey(nID, keyManager))
    {
        throw runtime_error("TSNH: can't get manager key");
    }
    if (!pregistryMain->GetDelegateKey(nID, keyDelegate))
    {
        throw runtime_error("TSNH: can't get delegate key");
    }
    if (!pregistryMain->GetControllerKey(nID, keyController))
    {
        throw runtime_error("TSNH: can't get controller key");
    }

    CBitcoinAddress addrOwner(keyOwner.GetID());
    CBitcoinAddress addrManager(keyManager.GetID());
    CBitcoinAddress addrDelegate(keyDelegate.GetID());
    CBitcoinAddress addrController(keyController.GetID());

    Object objOwner;
    objOwner.push_back(Pair("address", addrOwner.ToString()));
    objOwner.push_back(Pair("pubkey", HexStr(keyOwner.Raw())));
    Object objManager;
    objManager.push_back(Pair("address", addrManager.ToString()));
    objManager.push_back(Pair("pubkey", HexStr(keyManager.Raw())));
    Object objDelegate;
    objDelegate.push_back(Pair("address", addrDelegate.ToString()));
    objDelegate.push_back(Pair("pubkey", HexStr(keyDelegate.Raw())));
    Object objController;
    objController.push_back(Pair("address", addrController.ToString()));
    objController.push_back(Pair("pubkey", HexStr(keyController.Raw())));

    Object obj;
    obj.push_back(Pair("owner", objOwner));
    obj.push_back(Pair("manager", objManager));
    obj.push_back(Pair("delegate", objDelegate));
    obj.push_back(Pair("controller", objController));

    return obj;
}


Value getqposinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
    {
        throw runtime_error(
            "getqposinfo [height]\n"
            "Returns exhaustive qPoS information\n"
            "Optional [height] will get info as of that height (expensive)");
    }

    int nHeight = -1;
    if (params.size() > 0)
    {
        nHeight = params[0].get_int();
        if ((nHeight < 0) || (nHeight > pindexBest->nHeight))
        {
            throw runtime_error("Height out of range.");
        }
    }

    Object obj;
    if (nHeight == -1)
    {
        pregistryMain->AsJSON(obj);
    }
    else
    {
        bool fFound = false;
        CBlockIndex *pindex = pindexBest;
        while (pindex->pprev)
        {
            if (pindex->nHeight == nHeight)
            {
                fFound = true;
                break;
            }
            pindex = pindex->pprev;
        }
        if (!fFound)
        {
            throw runtime_error("TSNH Block not found at this height.");
        }
        AUTO_PTR<QPRegistry> pregistryTemp(new QPRegistry());
        CTxDB txdb;
        CBlockIndex* pindexCurrent;
        if (!RewindRegistry(txdb, pindex, pregistryTemp.get(), pindexCurrent))
        {
            throw runtime_error("TSRH Error calculating info.");
        }
        pregistryTemp->AsJSON(obj);
    }

    return obj;
}


Value getblockschedule(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
    {
        throw runtime_error(
            "getblockschedule <blocks>\n"
            "Returns details of StealthNodes in the block schedule.\n"
            "<blocks> is the max number of blocks to report before and\n"
            "   after the current block.");
    }

    if (GetFork(nBestHeight) < XST_FORKQPOS)
    {
        throw runtime_error("qPoS hasn't started yet");
    }

    int nBlocks = params[0].get_int();
    if (nBlocks < 0)
    {
        throw runtime_error("blocks must be positive");
    }

    int64_t nTime = GetAdjustedTime();
    unsigned int nCounterNext = pregistryMain->GetNextIDCounter();
    int nHeight = pregistryMain->GetBlockHeight();
    unsigned int nCurrentSlot = pregistryMain->GetCurrentSlot();

    vector<QPSlotInfo> vPrevious;
    pregistryMain->GetPreviousSlotsInfo(nTime, 0, vPrevious);
    vector<QPSlotInfo> vCurrent;
    pregistryMain->GetCurrentSlotsInfo(nTime, 0, vCurrent);

    int nSizePrevious = static_cast<int>(vPrevious.size());
    if (nSizePrevious < 1)
    {
        throw runtime_error("no previous queue (too early?)");
    }
    int nSizeCurrent = static_cast<int>(vCurrent.size());
    if (nSizeCurrent < 1)
    {
        throw runtime_error("TSNH: no current queue (unexpected)");
    }
    int nSizeAll = static_cast<int>(nSizePrevious + nSizeCurrent);

    vector<QPSlotInfo> vAll;
    vAll.reserve(nSizeAll);
    vAll.insert(vAll.end(), vPrevious.begin(), vPrevious.end());
    vAll.insert(vAll.end(), vCurrent.begin(), vCurrent.end());

    vector<string> vStatus;
    vStatus.reserve(nSizeAll);
    vector<QPSlotInfo>::const_iterator it;
    int nIndex = -1;
    int nIndexNow = -1;
    for (it = vAll.begin(); it < vAll.end(); ++it)
    {
        nIndex += 1;
        bool fQueueIsOld = (nIndex < nSizePrevious);
        string strStatus = it->GetRelativeStatusType(fQueueIsOld);
        if (nIndexNow == -1)
        {
            if (strStatus == "future")
            {
                nIndexNow = nIndex - 1;
            }
            else if (strStatus == "current")
            {
                nIndexNow = nIndex;
            }
        }
        vStatus.push_back(strStatus);
    }

    if (nIndexNow == -1)
    {
        if (nCurrentSlot == vCurrent.back().nSlot)
        {
            // end of current queue, with produced block
            // TODO: make temp registry and append to all
            nIndexNow = nSizeAll;
        }
        else
        {
            // this should never happen: no current slot
            throw runtime_error("TSNH: no current slot");
        }
    }

    int nStart = max(0, nIndexNow - nBlocks);
    int nStop = min(nSizeAll - 1, nIndexNow + nBlocks);

    int nLookBack = nIndexNow - nStart;
    int nMissingBefore = nBlocks - nLookBack;
    int nLookForward = nStop - nIndexNow;
    int nMissingAfter = nBlocks - nLookForward;

    int nOffset = -nLookBack;
    Array aryStakers;
    for (int i = nStart; i <= nStop; ++i)
    {
        const QPSlotInfo& info = vAll[i];
        unsigned int nID = info.nStakerID;
        const QPStaker* pstaker = pregistryMain->GetStaker(nID);
        if (!pstaker)
        {
            // this should never happen: no such staker
            throw runtime_error("TSNH: no such staker");
        }

        Object objStkr;
        unsigned int nSeniority =  nCounterNext - nID;
        pstaker->AsJSON(nID, nSeniority, objStkr);
        if (i < nSizePrevious)
        {
            objStkr.push_back(Pair("queue", "previous"));
        }
        else
        {
            objStkr.push_back(Pair("queue", "current"));
        }
        objStkr.push_back(Pair("slot", (int64_t)info.nSlot));
        string strStatus = info.GetStatusType();
        objStkr.push_back(Pair("status", strStatus));
        int64_t nSchedule = (int64_t)nOffset *  QP_TARGET_SPACING;
        objStkr.push_back(Pair("relative_schedule", nSchedule));
        objStkr.push_back(Pair("relative_status", vStatus[i]));
        aryStakers.push_back(objStkr);
        nOffset += 1;
    }

    Object result;
    result.push_back(Pair("call_time", (int64_t)nTime));
    result.push_back(Pair("latest_block_height", (boost::int64_t)nHeight));
    result.push_back(Pair("missing_before", (int64_t)nMissingBefore));
    result.push_back(Pair("missing_after", (int64_t)nMissingAfter));
    result.push_back(Pair("schedule", aryStakers));

    return result;
}

Value getstakersbyid(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
    {
        throw runtime_error(
            "getstakersbyid [disqualified]\n"
            "Returns details of StealthNodes by ascending id.\n"
            "Optional [disqualified=false] includes disqualified stakers if true");
    }

    bool fDisqualified = false;
    if (params.size() > 0)
    {
        fDisqualified = params[0].get_bool();
    }

    const unsigned int nCounterNext = pregistryMain->GetNextIDCounter();

    vector<const QPStaker*> vpStakers;
    pregistryMain->GetStakers(vpStakers);

    Array aryStakers;
    unsigned int nID = 0;
    vector<const QPStaker*>::const_iterator vit;
    for (vit = vpStakers.begin(); vit != vpStakers.end(); ++vit)
    {
        nID += 1;
        const QPStaker* pstaker = (const QPStaker *)(*vit);
        if (pstaker->IsQualified() || fDisqualified)
        {
           Object objStkr;
           unsigned int nSeniority = nCounterNext - nID;
           pstaker->AsJSON(nID, nSeniority, objStkr);
           aryStakers.push_back(objStkr);
        }
    }

    return aryStakers;
}

Value getstakersbyweight(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
    {
        throw runtime_error(
            "getstakersbyweight\n"
            "Returns details of StealthNodes in descending weight.");
    }

    QPMapPStakers mapStakers;
    pregistryMain->GetStakers(mapStakers);
    const unsigned int nCounterNext = pregistryMain->GetNextIDCounter();
    vector<pair<unsigned int, unsigned int> > vWeights;
    QPRegistryPIterator mit;
    for (mit = mapStakers.begin(); mit != mapStakers.end(); ++mit)
    {
        if (mit->second->IsQualified())
        {
            unsigned int nID = mit->first;
            const QPStaker* pstaker = mit->second;
            unsigned int nSeniority = nCounterNext - nID;
            unsigned int nWeight = pstaker->GetWeight(nSeniority);
            vWeights.push_back(make_pair(nWeight, nID));
        }
    }

    // rank goes in reverse order of weight
    sort(vWeights.begin(), vWeights.end());
    reverse(vWeights.begin(), vWeights.end());

    Array aryStakers;
    vector<pair<unsigned int, unsigned int> >::const_iterator vit;
    for (vit = vWeights.begin(); vit != vWeights.end(); ++vit)
    {
        unsigned int nID = vit->second;
        const QPStaker* pstaker = mapStakers[nID];
        Object objStkr;
        unsigned int nSeniority =  nCounterNext - nID;
        pstaker->AsJSON(nID, nSeniority, objStkr);
        aryStakers.push_back(objStkr);
    }

    return aryStakers;
}



Value getstakersummary(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
    {
        throw runtime_error(
            "getstakersummary\n"
            "Returns a summary of the state and activity of StealthNodes.");
    }

    const CBlockIndex* pindex = pindexBest;

    int64_t nTime = GetAdjustedTime();

    // latest staker
    unsigned int nIDLatest = pindex->nStakerID;
    if (nIDLatest == 0)
    {
        throw runtime_error("Latest block has no staker.");
    }
    const QPStaker* pstakerLatest = pregistryMain->GetStaker(nIDLatest);
    if (!pstakerLatest)
    {
        // this should never happen: the latest staker isn't in registry
        throw runtime_error("TSNH: Latest staker is not in registry.");
    }

    // next staker
    unsigned int nIDNext = pregistryMain->GetCurrentID();
    if (nIDNext == 0)
    {
        throw runtime_error("Next slot has no staker.");
    }
    unsigned int nSlotNext = pregistryMain->GetCurrentSlot();
    const QPStaker* pstakerNext = pregistryMain->GetStaker(nIDNext);
    if (!pstakerNext)
    {
        // this should never happen: the latest staker isn't in registry
        throw runtime_error("TSNH: Next staker is not in registry.");
    }

    // the queue
    vector<QPSlotInfo> vSlotsInfo;
    pregistryMain->GetCurrentSlotsInfo(nTime, 0, vSlotsInfo);
    int64_t nMissedQueue = 0;
    int64_t nProducedQueue = 0;
    Array aryRemaining;
    for (unsigned int i = 0; i < vSlotsInfo.size(); ++i)
    {
        const QPSlotInfo& slot = vSlotsInfo[i];
        if (slot.status == QPSLOT_MISSED)
        {
            nMissedQueue += 1;
        }
        else if (slot.status == QPSLOT_HIT)
        {
            nProducedQueue += 1;
        }
        if (i >= nSlotNext)
        {
            string strAlias;
            if (!pregistryMain->GetAliasForID(slot.nStakerID, strAlias))
            {
                // this should never happen: can't get the alias
                throw runtime_error("TSNH: Can't get queued staker's alias.");
            }
            aryRemaining.push_back(strAlias);
        }
    }
    int64_t nRemainingQueue = static_cast<int64_t>(aryRemaining.size());

    // enabled
    int64_t nMissedRecently = 0;
    int64_t nProducedRecently = 0;
    int64_t nPriceNewest = -1;
    unsigned int nIDNewest = pregistryMain->GetCurrentIDCounter();
    QPMapPStakers mapStakers;
    pregistryMain->GetStakers(mapStakers);
    QPRegistryPIterator it;
    for (it = mapStakers.begin(); it != mapStakers.end(); ++it)
    {
        if (it->second->IsEnabled())
        {
            if (it->second->DidMissMostRecentBlock())
            {
                nMissedRecently += 1;
            }
            else
            {
                nProducedRecently += 1;
            }
        }
        if (it->first == nIDNewest)
        {
            nPriceNewest = it->second->GetPrice();
        }
    }
    uint32_t N = static_cast<uint32_t>(
                    pregistryMain->GetNumberQualified());
    int64_t nPriceNext = GetStakerPrice(N, pindex->nMoneySupply);

    Object obj;
    // time
    obj.push_back(Pair("call_time", nTime));
    // states of stakers
    obj.push_back(Pair("enabled_stakers",
                       (boost::int64_t)pregistryMain->GetNumberEnabled()));
    obj.push_back(Pair("disabled_stakers",
                       (boost::int64_t)pregistryMain->GetNumberDisabled()));
    obj.push_back(Pair("terminated_stakers",
                       (boost::int64_t)pregistryMain->GetNumberDisqualified()));
    obj.push_back(Pair("productive_stakers",
                       (boost::int64_t)pregistryMain->GetNumberProductive()));
    obj.push_back(Pair("missed_recently", nMissedRecently));
    obj.push_back(Pair("produced_recently", nProducedRecently));
    // earned
    obj.push_back(Pair("total_xst_earned",
                       ValueFromAmount(pregistryMain->GetTotalEarned())));
    // latest staker
    obj.push_back(Pair("latest_staker_id", (boost::int64_t)nIDLatest));
    obj.push_back(Pair("latest_staker_alias", pstakerLatest->GetAlias()));
    // latest block
    obj.push_back(Pair("latest_block_height",
                       (boost::int64_t)pindex->nHeight));
    obj.push_back(Pair("latest_block_tx_volume",
                       (boost::int64_t)pindex->nTxVolume));
    obj.push_back(Pair("latest_block_xst_volume",
                       ValueFromAmount(pindex->nXSTVolume)));
    // pico power
    obj.push_back(Pair("pico_power",
                       pregistryMain->GetPicoPower()));
    obj.push_back(Pair("prev_pico_power",
                       pregistryMain->GetPicoPowerPrev()));
    obj.push_back(Pair("current_pico_power",
                       pregistryMain->GetPicoPowerCurrent()));
    // next staker
    obj.push_back(Pair("next_staker_id", (boost::int64_t)nIDNext));
    obj.push_back(Pair("next_staker_alias", pstakerNext->GetAlias()));
    // queue
    obj.push_back(Pair("missed_queue", nMissedQueue));
    obj.push_back(Pair("produced_queue", nProducedQueue));
    obj.push_back(Pair("remaining_queue", nRemainingQueue));
    obj.push_back(Pair("remaining_queue_aliases", aryRemaining));
    // prices
    if (nPriceNewest != -1)
    {
        obj.push_back(Pair("newest_staker_price",
                           ValueFromAmount(nPriceNewest)));
    }
    obj.push_back(Pair("next_staker_price",
                       ValueFromAmount(nPriceNext)));

    return obj;
}

Value getstakerpriceinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
    {
        throw runtime_error(
            "getstakerpriceinfo <stakers>"
            "  returns staker price history and ROI information\n"
            "  <stakers> is the number of stakers for which to calculate prices");
    }

    static const int MAX_STAKERS = 600;

    const CBlockIndex* pindex = pindexBest;

    int nStakersIn = params[0].get_int();
    if (nStakersIn < 0)
    {
        throw runtime_error("stakers must be positive");
    }
    if (nStakersIn > MAX_STAKERS)
    {
        throw runtime_error(
                strprintf("stakers must not exceed %d", MAX_STAKERS));
    }

    unsigned int nStakers = static_cast<unsigned int>(nStakersIn);

    vector<const QPStaker*> vpStakers;
    pregistryMain->GetStakers(vpStakers);

    vector<uint32_t> vBlocks;
    Array aryPrices;
    Array arySupply;
    Array aryFractionalPrices;
    vector<const QPStaker*>::const_iterator vit;
    for (vit = vpStakers.begin(); vit != vpStakers.end(); ++vit)
    {
        const QPStaker* pstaker = (const QPStaker *)(*vit);
        vBlocks.push_back(pstaker->GetNetBlocks());
        int64_t nPrice = pstaker->GetPrice();
        aryPrices.push_back(ValueFromAmount(nPrice));
        CBlockIndex* p = mapBlockIndex[pstaker->GetHashBlockCreated()];
        arySupply.push_back(ValueFromAmount(p->nMoneySupply));
        aryFractionalPrices.push_back((double)nPrice / (double)p->nMoneySupply);
    }

    unsigned int N = vpStakers.size();
    int64_t nPriceNewest = N > 0 ? vpStakers.back()->GetPrice() : -1;

    N += 1;
    int64_t nSupply = pindex->nMoneySupply;
    int64_t nPriceNext = GetStakerPrice(N, nSupply);
    nSupply -= nPriceNext;
    aryPrices.push_back(ValueFromAmount(nPriceNext));
    arySupply.push_back(ValueFromAmount(nSupply));
    aryFractionalPrices.push_back((double)nPriceNext / (double)nSupply);
    for (N += 1 ; N <= nStakers; ++N)
    {
        int64_t nPrice = GetStakerPrice(N, nSupply);
        aryPrices.push_back(ValueFromAmount(nPrice));
        arySupply.push_back(ValueFromAmount(nSupply));
        aryFractionalPrices.push_back((double)nPrice / (double)nSupply);
        nSupply -= nPrice;
    }

    // estimate with compounding weekly (DPY is days per year)
    static const int64_t DPY = 365;
    // days per week
    static const int64_t DPW = 7;
    // blocks per week
    static const uint32_t BPW = 120960;

    nSupply = pindex->nMoneySupply;
    uint32_t nQualified = (uint32_t)pregistryMain->GetNumberQualified();
    uint32_t nHypothetical = nQualified + 1;
    // blocks per month per staker
    uint32_t BPWPS = BPW / nHypothetical;

    uint32_t nBlocksSelf = 0;
    int64_t nWeeksROI = 0;
    int64_t nEarnedTotal = 0;
    Array aryEarned;
    Array aryEarnedTotal;
    while (nEarnedTotal < nPriceNext)
    {
        uint32_t nWeightSelf = 1;
        if (nBlocksSelf > 0)
        {
            nWeightSelf = uisqrt(nBlocksSelf);
        }
        nBlocksSelf += BPWPS;

        uint32_t nWeightAll = nWeightSelf;
        vector<uint32_t>::iterator it;
        for (it = vBlocks.begin(); it != vBlocks.end(); ++it)
        {
            nWeightAll += uisqrt(*it);
            *it += BPWPS;
        }

        int64_t nNewMoney = DPW * (nSupply / (DPY * RECIPROCAL_QPOS_INFLATION));
        int64_t nEarned = (int64_t)nWeightSelf * (nNewMoney / (int64_t)nWeightAll);
        nEarnedTotal += nEarned;
        aryEarned.push_back(ValueFromAmount(nEarned));
        aryEarnedTotal.push_back(ValueFromAmount(nEarnedTotal));
        nWeeksROI += 1;

        nSupply += nNewMoney;
    }

    Object obj;

    // prices
    if (nPriceNewest != -1)
    {
        obj.push_back(Pair("newest_staker_price",
                           ValueFromAmount(nPriceNewest)));
    }
    obj.push_back(Pair("next_staker_price",
                       ValueFromAmount(nPriceNext)));
    obj.push_back(Pair("qualified_stakers", (int64_t)nQualified));
    obj.push_back(Pair("prices", aryPrices));
    obj.push_back(Pair("money_supplies", arySupply));
    obj.push_back(Pair("fractional_prices", aryFractionalPrices));
    obj.push_back(Pair("hypothetical_stakers", (int64_t)nHypothetical));
    obj.push_back(Pair("moneysupply", ValueFromAmount(nSupply)));
    obj.push_back(Pair("inflation_percent",
                       (double)100 / (double)RECIPROCAL_QPOS_INFLATION));
    obj.push_back(Pair("weeks_to_roi", nWeeksROI));
    obj.push_back(Pair("days_to_roi", DPW * nWeeksROI));
    obj.push_back(Pair("years_to_roi", (double)nWeeksROI * (double)DPW / (double)DPY));
    obj.push_back(Pair("weekly_earnings", aryEarned));
    obj.push_back(Pair("weekly_cumulative_earnings", aryEarnedTotal));

    return obj;
}

Value getrecentqueue(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
    {
        throw runtime_error(
            "getrecentqueue <blocks>\n"
            "<blocks> is the number of blocks to look back\n"
            "Returns a 1,0 array, where 1 is a hit and 0 is a miss.\n"
            "The array is ordered chronologically.");
    }

    int nBlocks = params[0].get_int();

    if (nBlocks < 1)
    {
        throw runtime_error("Number of blocks is less than 1.");
    }

    if (nBlocks > (int)pregistryMain->GetRecentBlocksSize())
    {
        throw runtime_error(
                strprintf("Number of blocks greater than %u.",
                          pregistryMain->GetRecentBlocksSize()));
    }

    int nFinish = pindexBest->nHeight;

    if (nBlocks > (nFinish + 1))
    {
        throw runtime_error(
                strprintf("Number of blocks greater than chain height %u.",
                          nFinish));
    }

    int nStart = (nFinish + 1) - nBlocks;

    Object obj;
    Array ary;
    for (int i = nBlocks - 1; i >= 0; --i)
    {
        int f = pregistryMain->GetRecentBlock((unsigned int)i);
        if ((f < 0) || (f > 1))
        {
            throw runtime_error("TSNH: unexpected bad number of blocks");
        }
        ary.push_back(f);
    }
    obj.push_back(Pair("first", (boost::int64_t)nStart));
    obj.push_back(Pair("last", (boost::int64_t)nFinish));
    obj.push_back(Pair("data", ary));

    return obj;
}

Value getqposbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
    {
        throw runtime_error(
            "getqposbalance <pubkey>\n"
            "Returns qPoS balance owned by <pubkey>");
    }

    valtype vchPubKey = ParseHex(params[0].get_str());
    CPubKey key(vchPubKey);

    int64_t nBalance;
    if (!pregistryMain->GetBalanceForPubKey(key, nBalance))
    {
        throw JSONRPCError(RPC_QPOS_KEY_NOT_IN_LEDGER,
                           "PubKey is not in the qPoS ledger.");
    }

    return ValueFromAmount(nBalance);
}

Value getcharacterspg(const Array &params, bool fHelp)
{
    if (fHelp || (params.size() < 2) || (params.size() > 3))
    {
        throw runtime_error(
                "getcharacterspg <page> <perpage> [ordering]\n"
                "Returns up to <perpage> characters\n"
                "  beginning with 1 + (<perpage> * (<page> - 1>))\n"
                "  For example, <page>=2 and <perpage>=20 means to\n"
                "  return characters 21 - 40 (if possible).\n"
                "    <page> is the page number\n"
                "    <perpage> is the number of transactions per page\n"
                "    [ordering] by character id (default=true -> forward)");
    }

    // leading params = 1 (1st param is <address>, 2nd is <page>)
    static const unsigned int LEADING_PARAMS = 0;

    int nQtyNfts = mapNfts.size();

    if (nQtyNfts == 0)
    {
         throw runtime_error("No characters.");
    }

    pagination_t pg;
    GetPagination(params, LEADING_PARAMS, nQtyNfts, pg);

    vector<Object> vNfts;
    int nStop = min(pg.start + pg.max -1, nQtyNfts);
    for (int i = pg.start; i <= nStop; ++i)
    {
        if (!mapNfts.count(i))
        {
            throw runtime_error(strprintf("TSNH: no such character %d\n", i));
        }
        Object objNft;
        mapNfts[i].AsJSON(objNft);
        objNft.push_back(Pair("character_id", static_cast<int64_t>(i)));
        unsigned int nStakerID = pregistryMain->GetNftOwner(i);
        if (nStakerID)
        {
            string strAlias;
            if (!pregistryMain->GetAliasForID(nStakerID, strAlias))
            {
                throw runtime_error(strprintf("TSNH: no such staker %u\n",
                                              nStakerID));
            }
            objNft.push_back(Pair("owner_alias", strAlias));
            objNft.push_back(Pair("owner_id", static_cast<uint64_t>(nStakerID)));
            vNfts.push_back(objNft);
        }
    }

    if (!pg.forward)
    {
        reverse(vNfts.begin(), vNfts.end());
    }

    if (vNfts.size() == 1)
    {
        return vNfts[0];
    }

    Array result;
    for (unsigned int i = 0; i < vNfts.size(); ++i)
    {
        result.push_back(vNfts[i]);
    }

    return result;
}
