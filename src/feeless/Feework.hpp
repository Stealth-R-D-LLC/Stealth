// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "FeeworkBuffer.hpp"

#include "script.h"
#include "serialize.h"

#include "json/json_spirit_utils.h"


#ifndef _FEEWORK_H_
#define _FEEWORK_H_ 1

class Feework
{
private:
    uint64_t GetFeeworkHash(const void* data,
                            const size_t datalen,
                            const void* work,
                            FeeworkBuffer& buffer,
                            int &nResultRet) const;
public:
    enum Status
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


    int GetFeeworkHash(const CDataStream& ss, FeeworkBuffer& buffer);

    bool Check(const uint32_t mcostIn);

    const char* GetStatus() const;

    const int64_t GetDiff() const;

    CScript GetScript() const;

    std::string ToString(std::string strLPad = "") const;

    void AsJSON(json_spirit::Object& objRet) const;
};





#endif  /* _FEEWORKBUFFER_H_ */
