////////////////////////////////////////////////////////////////////////////////
//
// typedefs.h
//
// Copyright (c) 2013 Eric Lombrozo
// Copyright (c) 2011-2016 Ciphrex Corp.
//
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.
//

#ifndef __TYPEDEFS_H___
#define __TYPEDEFS_H___

#include "valtype.hpp"

#include <vector>
#include <set>
#include <string>

#define BYTES(x) bytes_t((x).begin(), (x).end())
#define SECURE_BYTES(x) secure_bytes_t((x).begin(), (x).end())

typedef valtype bytes_t;
typedef secure_valtype secure_bytes_t;

typedef std::vector<bytes_t> hashvector_t;
typedef std::set<bytes_t> hashset_t;

// from allocators.h
typedef SecureString secure_string_t;

typedef std::vector<int> ints_t;
typedef std::vector<int, secure_allocator<int> > secure_ints_t;

#endif // __TYPEDEFS_H__
