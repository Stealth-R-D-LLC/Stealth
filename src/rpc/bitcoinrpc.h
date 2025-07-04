// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _BITCOINRPC_H_
#define _BITCOINRPC_H_ 1

#include <string>
#include <list>
#include <map>

#include <boost/asio.hpp>
#include <boost/version.hpp>
#include <boost/asio/version.hpp>

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include "util.h"
#include "checkpoints.h"


class CBlockIndex;


// Before 1.84.0
#define BOOST_ASIO_HAS_DEPRECATED_ADDRESS_V6_METHODS (BOOST_ASIO_VERSION < 102803)
#define BOOST_ASIO_HAS_DEPRECATED_ADDRESS_V4_METHODS (BOOST_ASIO_VERSION < 102803)
#define BOOST_ASIO_HAS_MAX_CONNECTIONS (BOOST_ASIO_VERSION < 102803)
#define BOOST_ASIO_HAS_RESOLVER_QUERY (BOOST_ASIO_VERSION < 102803)
#define BOOST_ASIO_HAS_IO_SERVICE (BOOST_ASIO_VERSION < 102803)


namespace boost_asio_compat {
    
    inline bool is_v4_compatible(const boost::asio::ip::address_v6& addr) {
#if BOOST_ASIO_HAS_DEPRECATED_ADDRESS_V6_METHODS
        return addr.is_v4_compatible();
#else
        return addr.is_v4_mapped();
#endif
    }
    
    inline boost::asio::ip::address_v4 to_v4(const boost::asio::ip::address_v6& addr) {
#if BOOST_ASIO_HAS_DEPRECATED_ADDRESS_V6_METHODS
        return addr.to_v4();
#else
        if (addr.is_v4_mapped()) {
            auto bytes = addr.to_bytes();
            return boost::asio::ip::address_v4(
                boost::asio::ip::address_v4::bytes_type{{
                    bytes[12], bytes[13], bytes[14], bytes[15]
                }}
            );
        }
        throw boost::system::system_error(
            boost::asio::error::address_family_not_supported,
            "Address is not IPv4-compatible"
        );
#endif
    }
    
    inline uint32_t to_ulong(const boost::asio::ip::address_v4& addr) {
#if BOOST_ASIO_HAS_DEPRECATED_ADDRESS_V4_METHODS
        return addr.to_ulong();
#else
        auto bytes = addr.to_bytes();
        return (static_cast<uint32_t>(bytes[0]) << 24) |
               (static_cast<uint32_t>(bytes[1]) << 16) |
               (static_cast<uint32_t>(bytes[2]) << 8) |
               static_cast<uint32_t>(bytes[3]);
#endif
    }
}


// HTTP status codes
enum HTTPStatusCode
{
    HTTP_OK                    = 200,
    HTTP_BAD_REQUEST           = 400,
    HTTP_UNAUTHORIZED          = 401,
    HTTP_FORBIDDEN             = 403,
    HTTP_NOT_FOUND             = 404,
    HTTP_INTERNAL_SERVER_ERROR = 500,
};

// Bitcoin RPC error codes
enum RPCErrorCode
{
    // Standard JSON-RPC 2.0 errors
    RPC_INVALID_REQUEST  = -32600,
    RPC_METHOD_NOT_FOUND = -32601,
    RPC_INVALID_PARAMS   = -32602,
    RPC_INTERNAL_ERROR   = -32603,
    RPC_PARSE_ERROR      = -32700,

    // General application defined errors
    RPC_MISC_ERROR                  = -1,  // std::exception thrown in command handling
    RPC_FORBIDDEN_BY_SAFE_MODE      = -2,  // Server is in safe mode, and command is not allowed in safe mode
    RPC_TYPE_ERROR                  = -3,  // Unexpected type was passed as parameter
    RPC_INVALID_ADDRESS_OR_KEY      = -5,  // Invalid address or key
    RPC_OUT_OF_MEMORY               = -7,  // Ran out of memory during operation
    RPC_INVALID_PARAMETER           = -8,  // Invalid, missing or duplicate parameter
    RPC_DATABASE_ERROR              = -20, // Database error
    RPC_DESERIALIZATION_ERROR       = -22, // Error parsing or validating structure in raw format

    // P2P client errors
    RPC_CLIENT_NOT_CONNECTED        = -9,  // Bitcoin is not connected
    RPC_CLIENT_IN_INITIAL_DOWNLOAD  = -10, // Still downloading initial blocks

    // Wallet errors
    RPC_WALLET_ERROR                = -4,  // Unspecified problem with wallet (key not found etc.)
    RPC_WALLET_INSUFFICIENT_FUNDS   = -6,  // Not enough funds in wallet or account
    RPC_WALLET_INVALID_ACCOUNT_NAME = -11, // Invalid account name
    RPC_WALLET_KEYPOOL_RAN_OUT      = -12, // Keypool ran out, call keypoolrefill first
    RPC_WALLET_UNLOCK_NEEDED        = -13, // Enter the wallet passphrase with walletpassphrase first
    RPC_WALLET_PASSPHRASE_INCORRECT = -14, // The wallet passphrase entered was incorrect
    RPC_WALLET_WRONG_ENC_STATE      = -15, // Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
    RPC_WALLET_ENCRYPTION_FAILED    = -16, // Failed to encrypt the wallet
    RPC_WALLET_ALREADY_UNLOCKED     = -17, // Wallet is already unlocked

    // qPoS errors
    RPC_QPOS_ALIAS_NOT_AVAILABLE       = -101,
    RPC_QPOS_INVALID_QPOS_PUBKEY       = -102,
    RPC_QPOS_INVALID_PAYOUT            = -103,
    RPC_QPOS_STAKER_NONEXISTENT        = -104,
    RPC_QPOS_STAKER_PRICE_TOO_LOW      = -105,
    RPC_QPOS_STAKER_PRICE_TOO_HIGH     = -106,
    RPC_QPOS_TRANSFER_UNACKNOWLEDGED   = -107,
    RPC_QPOS_KEY_NOT_IN_LEDGER         = -108,
    RPC_QPOS_META_KEY_NOT_VALID        = -109,
    RPC_QPOS_META_VALUE_NOT_VALID      = -110,
    RPC_QPOS_NFT_NOT_AVAILABLE         = -111

};

json_spirit::Object JSONRPCError(int code, const std::string& message);

void ThreadRPCServer(void* parg);
int CommandLineRPC(int argc, char *argv[]);

/** Convert parameter values for RPC call from strings to command-specific JSON objects. */
json_spirit::Array RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams);

/*
  Type-check arguments; throws JSONRPCError if wrong type given. Does not check that
  the right number of arguments are passed, just that any passed are the correct type.
  Use like:  RPCTypeCheck(params, boost::assign::list_of(str_type)(int_type)(obj_type));
*/
void RPCTypeCheck(const json_spirit::Array& params,
                  const std::list<json_spirit::Value_type>& typesExpected, bool fAllowNull=false);
/*
  Check for expected keys/value types in an Object.
  Use like: RPCTypeCheck(object, boost::assign::map_list_of("name", str_type)("value", int_type));
*/
void RPCTypeCheck(const json_spirit::Object& o,
                  const std::map<std::string, json_spirit::Value_type>& typesExpected, bool fAllowNull=false);


void ScriptPubKeyToJSON(const CScript& scriptPubKey, json_spirit::Object& out);

typedef json_spirit::Value(*rpcfn_type)(const json_spirit::Array& params, bool fHelp);


//
// Pagination
//
typedef struct pagination_t
{
    int page;
    int per_page;
    bool forward;
    int start;
    int finish;
    int max;
    int last_page;
} pagination_t;

void GetPagination(const json_spirit::Array& params,
                   const unsigned int nLeadingParams,
                   const int nTotal,
                   pagination_t& pgRet);


class CRPCCommand
{
public:
    std::string name;
    rpcfn_type actor;
    bool okSafeMode;
    bool unlocked;
};

/**
 * Bitcoin RPC command dispatcher.
 */
class CRPCTable
{
private:
    std::map<std::string, const CRPCCommand*> mapCommands;
public:
    CRPCTable();
    const CRPCCommand* operator[](std::string name) const;
    std::string help(std::string name) const;

    /**
     * Execute a method.
     * @param method   Method to execute
     * @param params   Array of arguments (JSON objects)
     * @returns Result of the call.
     * @throws an exception (json_spirit::Value) when an error happens.
     */
    json_spirit::Value execute(const std::string &method, const json_spirit::Array &params) const;
};

extern const CRPCTable tableRPC;

extern int64_t nWalletUnlockTime;
extern int64_t AmountFromValue(const json_spirit::Value& value);
extern json_spirit::Value ValueFromAmount(int64_t amount);
extern double GetDifficulty(const CBlockIndex* blockindex = NULL);

extern double GetPoSKernelPS();

extern std::string HexBits(unsigned int nBits);
extern std::string HelpRequiringPassphrase();
extern void EnsureWalletIsUnlocked();
extern uint256 ParseHashV(const json_spirit::Value& v, std::string strName);
extern uint256 ParseHashO(const json_spirit::Object& o, std::string strKey);
extern std::vector<unsigned char> ParseHexV(const json_spirit::Value& v, std::string strName);
extern std::vector<unsigned char> ParseHexO(const json_spirit::Object& o, std::string strKey);


extern json_spirit::Value getconnectioncount(const json_spirit::Array& params, bool fHelp); // in rpcnet.cpp
extern json_spirit::Value getadjustedtime(const json_spirit::Array& params, bool fHelp); // in rpcnet.cpp
extern json_spirit::Value getpeerinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value dumpprivkey(const json_spirit::Array& params, bool fHelp); // in rpcdump.cpp
extern json_spirit::Value importprivkey(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value importaddress(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value sendalert(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value getgenerate(const json_spirit::Array& params, bool fHelp); // in rpcmining.cpp
extern json_spirit::Value getsubsidy(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setgenerate(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gethashespersec(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getmininginfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getwork(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getworkex(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblocktemplate(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value submitblock(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value getnewaddress(const json_spirit::Array& params, bool fHelp); // in rpcwallet.cpp
extern json_spirit::Value getaccountaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddressesbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendtoaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value signmessage(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value verifymessage(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getreceivedbyaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getreceivedbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value movecmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendfrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendmany(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addmultisigaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listreceivedbyaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listreceivedbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listtransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listaddressgroupings(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listaccounts(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listsinceblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value backupwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value keypoolrefill(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletpassphrase(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletpassphrasechange(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletlock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value encryptwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value validateaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value reservebalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value checkwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value repairwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value resendtx(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createfeework(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value makekeypair(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value validatepubkey(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getnewpubkey(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getnetworkhashps(const json_spirit::Array& params, bool fHelp);
// in rcprawtransaction.cpp
extern json_spirit::Value getrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listunspent(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decoderawtransaction(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value signrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendrawtransaction(const json_spirit::Array& params, bool fHelp);
// in rpcblockchain.cpp
extern json_spirit::Value getbestblockhash(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decryptsend(const json_spirit::Array& params, bool fHelp);
// in rpcblockchain.cpp
extern json_spirit::Value getblockcount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getdifficulty(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value settxfee(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getrawmempool(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockhash(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockhash9(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockbynumber(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getbestblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getnewestblockbeforetime(const json_spirit::Array& params, bool fHelp);
// explorer api
extern json_spirit::Value gettxvolume(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getxstvolume(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockinterval(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockintervalmean(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockintervalrmsd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getpicopowermean(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gethourlymissed(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getchildkey(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddressbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddressinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddressinputs(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddressoutputs(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddresstxspg(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddressinouts(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddressinoutspg(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gethdaccountpg(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gethdaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gethdaddresses(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getrichlistsize(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getrichlist(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getrichlistpg(const json_spirit::Array& params, bool fHelp);
//
extern json_spirit::Value getcheckpoint(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getnewstealthaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststealthaddresses(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value importstealthaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendtostealthaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value clearwallettransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value scanforalltxns(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value scanforstealthtxns(const json_spirit::Array& params, bool fHelp);
// in rpcqpos.cpp
extern json_spirit::Value getstakerprice(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstakerid(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value purchasestaker(const json_spirit::Array &params, bool fHelp);
extern json_spirit::Value setstakerowner(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setstakermanager(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setstakerdelegate(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setstakercontroller(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value enablestaker(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value disablestaker(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value claimqposbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setstakermeta(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstakerinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstakerauthorities(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststakerunspent(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getqposinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getqueuesummary(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockschedule(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstakersbyid(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstakersbyweight(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstakersummary(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstakerpriceinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getcertifiednodes(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getrecentqueue(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getqposbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getcharacterspg(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value exitreplay(const json_spirit::Array& params, bool fHelp);

#endif
