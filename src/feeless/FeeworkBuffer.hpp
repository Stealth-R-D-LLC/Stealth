// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _FEEWORKBUFFER_H_
#define _FEEWORKBUFFER_H_ 1

#include "argon2.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/lockable_adapter.hpp>


// lockable wrapper for argon2_buffer
class FeeworkBuffer
    : public boost::basic_lockable_adapter<boost::mutex>
{
public:
    enum Status
    {
        INIT_OK                   = 0,
        INIT_ALLOC_ERROR          = 1<<0,
        INIT_MEM_ALLOC_ERROR      = 1<<1
    };

    argon2_buffer* pbuffer;
    int status;

    void Initialize();

    FeeworkBuffer();
    ~FeeworkBuffer();
};


#endif  /* _FEEWORKBUFFER_H_ */
