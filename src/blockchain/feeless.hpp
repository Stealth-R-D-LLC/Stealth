// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _STEALTHFEELESS_H_
#define _STEALTHFEELESS_H_ 1

#include "script.h"
#include "serialize.h"
#include "argon2.h"

#include "json/json_spirit_utils.h"

extern argon2_buffer* pbfrFeeworkMiner;
extern argon2_buffer* pbfrFeeworkValidator;

extern bool fDebugFeeless;

enum FeeworkInitStatus
{
    FEELESS_INIT_OK                         = 0,
    FEELESS_INIT_MINER_ALLOC_ERROR          = 1<<0,
    FEELESS_INIT_MINER_MEM_ALLOC_ERROR      = 1<<1,
    FEELESS_INIT_VALIDATOR_ALLOC_ERROR      = 1<<2,
    FEELESS_INIT_VALIDATOR_MEM_ALLOC_ERROR  = 1<<3
};

int InitializeFeeless(bool fInitMiner, bool fInitValidator);

void ShutdownFeeless();

uint64_t GetFeeworkHash(const uint32_t mcost,
                        const void* data,
                        const size_t datalen,
                        const void* work,
                        argon2_buffer* buffer);

class Feework
{
public:
    enum status
    {
        UNCHECKED=-1,  // feework has yet to be checked
        // valid statuses
        OK,
        NONE,          // tx has no feework and is not needed
        // invalid statuses
        EMPTY,         // vout is empty
        COINBASE,      // vout is coinbase
        COINSTAKE,     // vout is coinstake
        INSOLUBLE,     // an output script can't be solved
        MISPLACED,     // a feework output is not last in vout
        BADVERSION,    // tx version does not support feeleess
        MISSING,       // vout has no feework output, but is expected
        BLOCKUNKNOWN,  // the block referenced by the feework is unknown
        BLOCKTOODEEP,  // the block referenced by the feework is too deep
        LOWMCOST,      //  mcost is too low to get in block
        HIGHMCOST,     //  mcost exceeds limits imposed by protocol
        NOHEIGHT,      // feework object is incomplete, missing height
        NOLIMIT,       //                               missing limit
        NOMCOST,       //                               missing mcost
        NOHASH,        //                               missing hash
        NOWORK,        //                               missing work
        INSUFFICIENT   // feework is insufficient to be included in block
    };

    int height;
    const uint256* pblockhash;
    uint64_t bytes;
    uint32_t mcost;
    uint64_t limit;
    uint64_t work;
    uint64_t hash;
    int status;

    Feework();

    void SetNull();

    bool IsUnchecked() const;
    bool IsChecked() const;
    bool IsOK() const;
    bool HasNone() const;
    bool IsMissing() const;
    bool IsValid() const;
    bool IsBadVersion() const;
    bool IsInsufficient() const;

    void ExtractFeework(const valtype &vch);

    void GetFeeworkHash(const CDataStream& ss, argon2_buffer* buffer);

    bool Check(const uint32_t mcostIn);

    const char* GetStatus() const;

    const int64_t GetDiff() const;

    CScript GetScript() const;

    std::string ToString(std::string strLPad = "") const;

    void AsJSON(json_spirit::Object& objRet) const;
};


#endif  /* _STEALTHFEELESS_H_ */
