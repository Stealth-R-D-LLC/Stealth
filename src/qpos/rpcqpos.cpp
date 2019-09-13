// Copyright (c) 2019 Stealth R&D LLC
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "wallet.h"
#include "bitcoinrpc.h"

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
    return ValueFromAmount(GetStakerPrice(pregistryMain, pindexBest));
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

    sAliasRet = params[2].get_str();
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
        nIDRet = 0;
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
                 (params.size()  > 8)  ||
                 (params.size() == 6)  ||
                 (params.size() == 7))
    {
        throw runtime_error(
            "purchasestaker <txid> <vout> <alias> <owner> [amount] "
            "[delegate controller payout]\n"
            "    <txid> is the transaction ID of the input\n"
            "    <vout> is the prevout index of the input\n"
            "    <alias> is the case sensitive staker alias\n"
            "    <owner> is the owner compressed pubkey\n"
            "    [amount] is is the amount to pay\n"
            "        If the amount is not specified it will be calculated\n"
            "          automatically.\n"
            "        The amount is real and rounded to the nearest 0.000001\n"
            "    [delegate] and [controller] are compressed pubkeys\n"
            "        If delegate and controller are not specified then they\n"
            "          are taken from owner.\n"
            "    [payout] is in percentage, and is rounded to the nearest\n"
            "          thousandths of a percent\n"
            "        Either just the owner key or all 3 keys plus the payout\n"
            "          must be specified." +
            HelpRequiringPassphrase());
    }

    string txid, sAlias;
    unsigned int nOut;
    valtype vchOwnerKey, vchDelegateKey, vchControllerKey;

    unsigned int nIDUnused;
    ExtractQPoSRPCEssentials(params, txid, nOut, sAlias, vchOwnerKey,
                             nIDUnused, false);

    int64_t nPrice = GetStakerPrice(pregistryMain, pindexBest, true);
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
    if (params.size() > 7)
    {
        vchDelegateKey = ParseHex(params[5].get_str());
        if (vchDelegateKey.size() != 33)
        {
            throw JSONRPCError(RPC_QPOS_INVALID_QPOS_PUBKEY,
                               "Delegate key is not a compressed pubkey");
        }
        vchControllerKey = ParseHex(params[6].get_str());
        if (vchControllerKey.size() != 33)
        {
            throw JSONRPCError(RPC_QPOS_INVALID_QPOS_PUBKEY,
                               "Controller key is not a compressed pubkey");
        }
        nPcm = PcmFromValue(params[7]);
    }
    else
    {
        vchDelegateKey = vchOwnerKey;
        vchControllerKey = vchOwnerKey;
        nPcm = 0;
    }

    CWalletTx wtx;

    // Purchase
    string strError = pwalletMain->PurchaseStaker(txid, nOut,
                                                  sAlias,
                                                  vchOwnerKey,
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
            "Returns exhaustive information about the qPoS registry");
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

Value getqposinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
    {
        throw runtime_error(
            "getqposinfo\n"
            "Returns exhaustive qPoS information");
    }

    Object obj;
    pregistryMain->AsJSON(obj);

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
