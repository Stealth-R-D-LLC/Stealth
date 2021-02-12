// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "feeless.hpp"
#include "chainparams.hpp"

#include <boost/multiprecision/cpp_int.hpp>

typedef boost::multiprecision::int128_t int128_t;

using namespace std;
using namespace json_spirit;

argon2_buffer* pbfrFeeworkMiner = NULL;
argon2_buffer* pbfrFeeworkValidator = NULL;

bool fDebugFeeless = false;

int InitializeFeeless(bool fInitMiner, bool fInitValidator)
{
    int fStatus = FEELESS_INIT_OK;
    if (fInitMiner)
    {
       if (pbfrFeeworkMiner == NULL)
       {
           pbfrFeeworkMiner = (argon2_buffer*)malloc(sizeof(argon2_buffer));
       }
       else if (pbfrFeeworkMiner->memory != NULL)
       {
           free(pbfrFeeworkMiner->memory);
       }
       if (pbfrFeeworkMiner == NULL)
       {
           fStatus |= FEELESS_INIT_MINER_ALLOC_ERROR;
       }
       else
       {
             pbfrFeeworkMiner->blocks = chainParams.FEEWORK_MAX_MCOST;
             size_t n = pbfrFeeworkMiner->blocks * sizeof(argon2_block);
             pbfrFeeworkMiner->memory = (argon2_block*)malloc(n);
             if (!pbfrFeeworkMiner->memory)
             {
                 fStatus |= FEELESS_INIT_MINER_MEM_ALLOC_ERROR;
             }
             else
             {
                 pbfrFeeworkMiner->clear = false;
             }
       }
    }
    if (fInitValidator)
    {
       if (pbfrFeeworkValidator == NULL)
       {
           pbfrFeeworkValidator = (argon2_buffer*)malloc(sizeof(argon2_buffer));
       }
       else if (pbfrFeeworkValidator->memory != NULL)
       {
           free(pbfrFeeworkValidator->memory);
       }
       if (pbfrFeeworkValidator == NULL)
       {
           fStatus |= FEELESS_INIT_VALIDATOR_ALLOC_ERROR;
       }
       else
       {
             pbfrFeeworkValidator->blocks = chainParams.FEEWORK_MAX_MCOST;
             size_t n = pbfrFeeworkValidator->blocks * sizeof(argon2_block);
             pbfrFeeworkValidator->memory = (argon2_block*)malloc(n);
             if (!pbfrFeeworkValidator->memory)
             {
                 fStatus |= FEELESS_INIT_VALIDATOR_MEM_ALLOC_ERROR;
             }
             else
             {
                 pbfrFeeworkValidator->clear = false;
             }
       }
    }
    return fStatus;
}

void ShutdownFeeless()
{
    if (pbfrFeeworkMiner)
    {
        if (pbfrFeeworkMiner->memory)
        {
            free(pbfrFeeworkMiner->memory);
        }
        free(pbfrFeeworkMiner);
    }
    if (pbfrFeeworkValidator)
    {
        if (pbfrFeeworkValidator->memory)
        {
            free(pbfrFeeworkValidator->memory);
        }
        free(pbfrFeeworkValidator);
    }
}

uint64_t GetFeeworkHash(const uint32_t mcost,
                        const void* data,
                        const size_t datalen,
                        const void* work,
                        argon2_buffer* buffer,
                        int &nResultRet)
{
    static const size_t WORKLEN = chainParams.FEELESS_WORKLEN;
    static const size_t HASHLEN = chainParams.FEELESS_HASHLEN;
    static const int HASHSIZE = chainParams.FEELESS_HASHLEN;

    static const uint32_t TCOST = chainParams.FEELESS_TCOST;
    static const uint32_t PARALLELISM = chainParams.FEELESS_PARALLELISM;

    // important: doesn't initialize value of vchnum
    vchnum vchHash(HASHSIZE);
    unsigned char* pchHash = vchHash.BeginData();

    nResultRet = argon2d_hash_raw(TCOST, mcost, PARALLELISM,
                                  data, datalen,
                                  work, WORKLEN,
                                  pchHash, HASHLEN,
                                  buffer);

    return vchHash.GetValue();
}

Feework::Feework()
    : height(-1),
      pblockhash(NULL),
      bytes(0),
      mcost(0),
      limit(0),
      work(0),
      hash(0),
      status(Feework::UNCHECKED) {}

void Feework::SetNull()
{
    height = -1;
    pblockhash = NULL;
    bytes = 0;
    mcost = 0;
    limit = 0;
    work = 0;
    hash = 0;
    status = Feework::UNCHECKED;
}

bool Feework::IsUnchecked() const
{
    return (status == Feework::UNCHECKED);
}

bool Feework::IsChecked() const
{
    return (status != Feework::UNCHECKED);
}

bool Feework::IsOK() const
{
    return (status == Feework::OK);
}

bool Feework::HasNone() const
{
    return (status == Feework::NONE);
}

bool Feework::IsMissing() const
{
    return (status == Feework::MISSING);
}

bool Feework::IsValid() const
{
    return ((status == Feework::OK) || (status == Feework::NONE));
}

bool Feework::IsBadVersion() const
{
    return (status == Feework::BADVERSION);
}

bool Feework::IsInsufficient() const
{
    return (status == Feework::INSUFFICIENT);
}

void Feework::ExtractFeework(const valtype &vch)
{
    valtype::const_iterator first = vch.begin();
    valtype::const_iterator last = first;
    if (!IncrementN(vch, last, 4))
    {
        return;
    }
    height = GETINT(first, last);
    first = last;
    if (!IncrementN(vch, last, 4))
    {
        return;
    }
    mcost = GETUINT32(first, last);
    first = last;
    if (!IncrementN(vch, last, 8))
    {
        return;
    }
    work = GETUINT64(first, last);
}

int Feework::GetFeeworkHash(const CDataStream& ss, argon2_buffer* buffer)
{
    static const uint32_t MAX_MCOST = chainParams.FEEWORK_MAX_MCOST;

    const char* pchData = ss.begindata();
    const size_t datalen = ss.sizeall();

    vchnum vchWork(work);
    const unsigned char* pchWork = vchWork.BeginData();

    // This test circumvents a DoS attack that spams feeless
    // transactions that specify very high memory costs.
    // The tradeoff is that bigger transactions can not be
    // feeless in fuller blocks.
    int result;
    if (mcost > MAX_MCOST)
    {
        result = ARGON2_MEMORY_TOO_MUCH;
        hash = chainParams.TX_FEEWORK_ABSOLUTE_LIMIT;
    }
    else
    {
        hash = ::GetFeeworkHash(mcost, pchData, datalen, pchWork, buffer, result);
    }
    return result;
}

bool Feework::Check(const uint32_t mcostIn)
{
    static const uint32_t MAX_MCOST = chainParams.FEEWORK_MAX_MCOST;
    if (status == Feework::UNCHECKED)
    {
        if (mcost < mcostIn)
        {
           status = Feework::LOWMCOST;
        }
        else if (mcost > MAX_MCOST)
        {
           status = Feework::HIGHMCOST;
        }
        else if (height < 0)
        {
           status = Feework::NOHEIGHT;
        }
        else if (work == 0)
        {
           status = Feework::NOWORK;
        }
        else if (hash == 0)
        {
            status = Feework::NOHASH;
        }
        else if (limit == 0)
        {
           status = Feework::NOLIMIT;
        }
        else if (mcost == 0)
        {
           status = Feework::NOMCOST;
        }
        else if (limit < hash)
        {
           status = Feework::INSUFFICIENT;
        }
        else
        {
           status = Feework::OK;
        }
    }
    return (status == Feework::OK);
}

const char* Feework::GetStatus() const
{
    switch (status)
    {
    case Feework::UNCHECKED: return "unchecked";
    case Feework::OK: return "ok";
    case Feework::NONE: return "tx_has_no_feework";
    case Feework::EMPTY: return "tx_has_empty_vout";
    case Feework::COINBASE: return "tx_is_coinbase";
    case Feework::COINSTAKE: return "tx_is_coinstake";
    case Feework::INSOLUBLE: return "tx_hash_insoluble_script";
    case Feework::MISPLACED: return "tx_has_misplaced_feework";
    case Feework::BADVERSION: return "tx_has_bad_version";
    case Feework::MISSING: return "tx_missing_feework";
    case Feework::BLOCKUNKNOWN: return "unknown_block";
    case Feework::BLOCKTOODEEP: return "block_too_deep";
    case Feework::LOWMCOST: return "low_memory_cost";
    case Feework::HIGHMCOST: return "high_memory_cost";
    case Feework::NOHEIGHT: return "no_height";
    case Feework::NOWORK: return "no_work";
    case Feework::NOLIMIT: return "no_limit";
    case Feework::NOMCOST: return "no_memory_cost";
    case Feework::NOHASH: return "no_hash";
    case Feework::INSUFFICIENT: return "insufficient_work";
    }
    return NULL;
}

// We are using signed int here because we treat the feework diff
// as directly comprable to amount of the money fee.
// So this saves lots of compiler warnings or type casting elsewhere.
// We ignore that the money cost of the feework resources is
// much less than the "equivalent" in fee money.
const int64_t Feework::GetDiff() const
{
    if (!(mcost && limit && work && hash))
    {
        return 0;
    }
    // using 128 bit integers here because precision of the
    // returned value requires a lot of extra bitwidth in the
    // intermediate calculations
    static const int128_t LIMIT = chainParams.TX_FEEWORK_LIMIT;
    static const int128_t MCOST = chainParams.FEELESS_MCOST_MIN;
    // Difficulty is measured in XST, to allow comparison with money fees.
    // Minimum feework is worth the minimum tx fee.
    static const int128_t MINFEE = chainParams.MIN_TX_FEE;
    int128_t diff = ((LIMIT * MINFEE) / hash) * (mcost / MCOST);
    static const int128_t MAXDIFF = (int64_t)std::numeric_limits<int64_t>::max;
    if (diff > MAXDIFF)
    {
        // This should never happen, but if someone mindlessly changes
        // the feework limit or min fee, int64_t may overflow.
        // So we throw a helpful error message for our cloner friends.
        printf("Feework::GetDiff(): TSNH: ERROR: exceeded numeric limits\n");
        throw;
    }
    return (int64_t)diff;
}

CScript Feework::GetScript() const
{
    CScript script;
    valtype vchWork = *vchnum(static_cast<uint64_t>(work)).Get();
    valtype vchMCost = *vchnum(static_cast<uint32_t>(mcost)).Get();
    valtype vchHeight = *vchnum(static_cast<uint32_t>(height)).Get();

    valtype vchFeework(0);

    EXTEND(vchFeework, vchWork);
    EXTEND(vchFeework, vchMCost);
    EXTEND(vchFeework, vchHeight);

    script << vchFeework << OP_FEEWORK;

    return script;
}

string Feework::ToString(string strLPad) const
{
    std::string str;
    str += strprintf("%sFeework: height=%d, bytes=%lu mcost=%s, limit=%s\n"
                        "%s         work=%s, hash=%s, status=%s\n"
                        "%s  block_hash=%s",
                     strLPad.c_str(),
                     height,
                     bytes,
                     CBigNum((uint64_t)mcost).GetHex().c_str(),
                     CBigNum(limit).GetHex().c_str(),
                     CBigNum(work).GetHex().c_str(),
                     strLPad.c_str(),
                     CBigNum(hash).GetHex().c_str(),
                     GetStatus(),
                     strLPad.c_str(),
                     pblockhash ? pblockhash->GetHex().c_str() : "NULL");
     return str;
}

void Feework::AsJSON(Object& objRet) const
{
    objRet.push_back(Pair("height", (boost::int64_t)height));
    if (pblockhash)
    {
        objRet.push_back(Pair("block_hash", pblockhash->GetHex()));
    }
    objRet.push_back(Pair("mcost", (boost::int64_t)mcost));
    valtype vchLimit = *vchnum(static_cast<uint64_t>(limit)).Get();
    string strLimit = HexStr(vchLimit.begin(), vchLimit.end());
    objRet.push_back(Pair("bytes", bytes));
    objRet.push_back(Pair("limit_hex", strLimit));
    objRet.push_back(Pair("limit_denary", (uint64_t)limit));
    valtype vchWork = *vchnum(static_cast<uint64_t>(work)).Get();
    string strWork = HexStr(vchWork.begin(), vchWork.end());
    objRet.push_back(Pair("work_hex", strWork));
    objRet.push_back(Pair("work_denary", work));
    valtype vchHash = *vchnum(static_cast<uint64_t>(hash)).Get();
    string strHash = HexStr(vchHash.begin(), vchHash.end());
    objRet.push_back(Pair("hash_hex", strHash));
    objRet.push_back(Pair("hash_denary", static_cast<uint64_t>(hash)));
    CScript script = GetScript();
    string strScript = HexStr(script.begin(), script.end());
    objRet.push_back(Pair("script_asm", strScript));
    objRet.push_back(Pair("script_hex", script.ToString()));
}
