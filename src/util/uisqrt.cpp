// Copyright (c) 2019 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdint.h>

const uint8_t bit_length_table[256] =
        {  255, 1,
           2,2, 3,3,3,3, 4,4,4,4,4,4,4,4, 5,5,5,5,5,5,5,5,5, 6,6,6,6,6,6,6,
           6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
           7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
           7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
           8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
           8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
           8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
           8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8 };

uint32_t bit_length(uint32_t v)
{
    static const uint32_t ones = -1;
    uint32_t r = 0;
    if (v & (ones << 24))
    {
        r += 24;
        v >>= 24;
    } 
    else if (v & (ones << 16))
    {
        r += 16;
        v >>= 16;
    }
    else if (v & (ones <<  8))
    {
        r += 8;
        v >>= 8;
    }
    return r + (uint32_t) bit_length_table[v];
}


uint32_t uisqrt(uint32_t n)
{
    if (n == 0)
    {
        return 0;
    }
    uint32_t b = bit_length(n);
    uint32_t x = 1 << (b >> 1);
    if (b ^ x)
    {
        x <<= 1;
    }
    uint32_t y = (x + (n / x)) >> 1;
    while (y < x)
    {
        x = y;
        y = (x + (n / x)) >> 1;
    }
    return x;
}

uint64_t bit_length(uint64_t v)
{
    static const uint64_t ones = -1;
    uint64_t r = 0;
    if (v & (ones << 56))
    {
        r += 56;
        v >>= 56;
    }
    else if (v & (ones << 48))
    {
        r += 48;
        v >>= 48;
    }
    else if (v & (ones <<  40))
    {
        r += 40;
        v >>= 40;
    }
    else if (v & (ones <<  32))
    {
        r += 32;
        v >>= 32;
    }
    else if (v & (ones << 24))
    {
        r += 24;
        v >>= 24;
    }
    else if (v & (ones << 16))
    {
        r += 16;
        v >>= 16;
    }
    else if (v & (ones <<  8))
    {
        r += 8;
        v >>= 8;
    }
    return r + (uint64_t) bit_length_table[v];
}


uint64_t uisqrt(uint64_t n)
{
    if (n == 0)
    {
        return 0;
    }
    uint64_t b = bit_length(n);
    uint64_t x = 1 << (b >> 1);
    if (b ^ x)
    {
        x <<= 1;
    }
    uint64_t y = (x + (n / x)) >> 1;
    while (y < x)
    {
        x = y;
        y = (x + (n / x)) >> 1;
    }
    return x;
}
