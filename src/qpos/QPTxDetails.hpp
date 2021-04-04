// Copyright (c) 2020 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QPTXDETAILS_H_
#define _QPTXDETAILS_H_ 1

#include "key.h"

// general purpose, can be used for any qpos tx type
class QPTxDetails
{
public:
    int t;               // indicates how to use data fields
    unsigned int id;            // staker ID -- or NFT ID if puchase (0 if N/A)
    std::string alias;          // alias (empty if N/A)
    std::vector<CPubKey> keys;  // length 1, 3 or 4
                                //    latter 2 for TX_PURCHASE4 only
    int32_t pcm;                // nonzero if TX_PURCHASE4 or TX_SETDELEGATE
    int64_t value;        // claim value (nonzero if TX_CLAIM)
    std::string meta_key;       // meta key (empty if N/A)
    std::string meta_value;     // meta value (empty if N/A)
    uint256 hash;        //  block hash
    uint256 txid;        //  tx hash
    int n;               //  index of vout

    void Clear();
    void SetNull();
    QPTxDetails();

    std::string ToString() const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(t);
        READWRITE(id);
        READWRITE(alias);
        READWRITE(keys);
        READWRITE(pcm);
        READWRITE(value);
        READWRITE(meta_key);
        READWRITE(meta_value);
        READWRITE(hash);
        READWRITE(txid);
        READWRITE(n);
    )
};


#endif  // _QPTXDETAILS_H_
