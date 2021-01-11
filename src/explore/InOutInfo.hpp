// Copyright (c) 2019 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _EXPLOREINOUTINFO_H_
#define _EXPLOREINOUTINFO_H_ 1

#include "ExploreTx.hpp"
#include "ExploreInput.hpp"
#include "ExploreOutput.hpp"

class InOutInfo
{
public:
    enum InOutType
    {
        no_type = 0,
        input_type = 1,
        output_type = 2
    };

    typedef union inout_t
    {
       ExploreInput input;
       ExploreOutput output;
       inout_t() {}
    } inout_t;

    int height;
    int vtx;

    InOutType type;

    inout_t inout;

    void SetNull();
    InOutInfo();

    void Set(const int fType);
    InOutInfo(const int fType);

    void Set(const int heightIn,
             const int vtxIn,
             const ExploreInput& inputIn);
    void Set(const int heightIn,
             const int vtxIn,
             const ExploreOutput& outputIn);
    InOutInfo(const int heightIn,
              const int vtxIn,
              const ExploreInput& inputIn);
    InOutInfo(const int heightIn,
              const int vtxIn,
              const ExploreOutput& outputIn);

    InOutInfo &operator = (const InOutInfo& other) noexcept;

    bool operator < (const InOutInfo& other) const;
    bool operator > (const InOutInfo& other) const;

    void BecomeInput();
    void BecomeOutput();

    bool IsInput() const;
    bool IsOutput() const;

    const uint256* GetTxID() const;
    int GetN() const;
    int64_t GetAmount() const;
    int64_t GetBalance() const;

    ExploreInput& GetInput();
    ExploreOutput& GetOutput();

    void AsUniqueJSON(json_spirit::Object& objRet) const;

    void AsJSON(const int nBestHeight,
                const std::string* pstrAddress,
                const ExploreTx* ptx,
                json_spirit::Object* pobjCommon,
                json_spirit::Object& objInOut) const;

    void AsJSON(const int nBestHeight,
                const std::string& strAddress,
                const ExploreTx& tx,
                json_spirit::Object& objRet) const;

    void AsJSON(const int nBestHeight, json_spirit::Object& objRet) const;

};


#endif  /* _EXPLOREINOUTINFO_H_ */
