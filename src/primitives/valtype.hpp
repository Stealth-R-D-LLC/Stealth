// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _VALTYPE_H_
#define _VALTYPE_H_ 1

#include <vector>


typedef std::vector<unsigned char> valtype;


// safely increment a valtype iterator
bool IncrementN(const valtype &v,
                valtype::const_iterator &i,
                unsigned int n);


#endif  /* _VALTYPE_H_ */
