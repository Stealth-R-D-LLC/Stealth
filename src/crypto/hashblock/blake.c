/* $Id: blake.c 252 2011-06-07 17:55:14Z tp $ */
/*
 * BLAKE implementation.
 *
 * ==========================(LICENSE BEGIN)============================
 *
 * Copyright (c) 2007-2010  Projet RNRT SAPHIR
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * ===========================(LICENSE END)=============================
 *
 * @author   Thomas Pornin <thomas.pornin@cryptolog.com>
 */

/*
 * BLAKE implementation.
 *
 * Copyright (c) 2007-2010 Projet RNRT SAPHIR
 * Author: Thomas Pornin <thomas.pornin@cryptolog.com>
 */

#include <stddef.h>
#include <string.h>
#include <limits.h>

#include "sph_blake.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialization vectors for BLAKE-224 and BLAKE-256 */
static const sph_u32 IV224[8] = {
    SPH_C32(0xC1059ED8), SPH_C32(0x367CD507),
    SPH_C32(0x3070DD17), SPH_C32(0xF70E5939),
    SPH_C32(0xFFC00B31), SPH_C32(0x68581511),
    SPH_C32(0x64F98FA7), SPH_C32(0xBEFA4FA4)
};

static const sph_u32 IV256[8] = {
    SPH_C32(0x6A09E667), SPH_C32(0xBB67AE85),
    SPH_C32(0x3C6EF372), SPH_C32(0xA54FF53A),
    SPH_C32(0x510E527F), SPH_C32(0x9B05688C),
    SPH_C32(0x1F83D9AB), SPH_C32(0x5BE0CD19)
};

/* Round constants for BLAKE-32 and BLAKE-64 */
static const sph_u32 C32[16] = {
    SPH_C32(0x243F6A88), SPH_C32(0x85A308D3),
    SPH_C32(0x13198A2E), SPH_C32(0x03707344),
    SPH_C32(0xA4093822), SPH_C32(0x299F31D0),
    SPH_C32(0x082EFA98), SPH_C32(0xEC4E6C89),
    SPH_C32(0x452821E6), SPH_C32(0x38D01377),
    SPH_C32(0xBE5466CF), SPH_C32(0x34E90C6C),
    SPH_C32(0xC0AC29B7), SPH_C32(0xC97C50DD),
    SPH_C32(0x3F84D5B5), SPH_C32(0xB5470917)
};

static const sph_u64 C64[16] = {
    SPH_C64(0x243F6A8885A308D3), SPH_C64(0x13198A2E03707344),
    SPH_C64(0xA4093822299F31D0), SPH_C64(0x082EFA98EC4E6C89),
    SPH_C64(0x452821E638D01377), SPH_C64(0xBE5466CF34E90C6C),
    SPH_C64(0xC0AC29B7C97C50DD), SPH_C64(0x3F84D5B5B5470917),
    SPH_C64(0x9216D5D98979FB1B), SPH_C64(0xD1310BA698DFB5AC),
    SPH_C64(0x2FFD72DBD01ADFB7), SPH_C64(0xB8E1AFED6A267E96),
    SPH_C64(0xBA7C9045F12C7F99), SPH_C64(0x24A19947B3916CF7),
    SPH_C64(0x0801F2E2858EFC16), SPH_C64(0x636920D871574E69)
};

/* Constants for BLAKE compression function */
#define ROT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

/* BLAKE round function for BLAKE-32 */
#define G32(v, a, b, c, d, m, s)                \
    do {                                         \
        (v)[a] = SPH_T32((v)[a] + (v)[b] + (m)); \
        (v)[d] = ROT((v)[d] ^ (v)[a], 16);      \
        (v)[c] = SPH_T32((v)[c] + (v)[d]);      \
        (v)[b] = ROT((v)[b] ^ (v)[c], 12);      \
        (v)[a] = SPH_T32((v)[a
