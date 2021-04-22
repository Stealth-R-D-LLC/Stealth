// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.hpp"
#include "FeeworkBuffer.hpp"


void FeeworkBuffer::Initialize()
{
    status = FeeworkBuffer::INIT_OK;
    if (!pbuffer)
    {
        pbuffer = (argon2_buffer*)malloc(sizeof(argon2_buffer));
    }
    else if (pbuffer->memory)
    {
        free(pbuffer->memory);
    }
    if (!pbuffer)
    {
        status |= FeeworkBuffer::INIT_ALLOC_ERROR;
    }
    else
    {
         pbuffer->blocks = chainParams.FEEWORK_MAX_MCOST;
         size_t n = pbuffer->blocks * sizeof(argon2_block);
         pbuffer->memory = (argon2_block*)malloc(n);
         if (!pbuffer->memory)
         {
             status |= FeeworkBuffer::INIT_MEM_ALLOC_ERROR;
         }
         else
         {
             pbuffer->clear = false;
         }
    }
}

FeeworkBuffer::FeeworkBuffer()
    : pbuffer(NULL)
{
    Initialize();
}

FeeworkBuffer::~FeeworkBuffer()
{
    if (pbuffer)
    {
        if (pbuffer->memory)
        {
            free(pbuffer->memory);
        }
        free(pbuffer);
    }
}

