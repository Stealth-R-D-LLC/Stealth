// Credit: Pavol Rusnak (https://github.com/prusnak)
// From https://github.com/trezor/trezor-crypto

#ifndef __MEMZERO_H__
#define __MEMZERO_H__

#include <stddef.h>

void memzero(void* const pnt, const size_t len);

#endif
