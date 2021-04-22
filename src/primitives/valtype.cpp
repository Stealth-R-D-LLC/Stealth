// Copyright (c) 2021 The Stealth Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "valtype.hpp"

bool IncrementN(const valtype &v,
                valtype::const_iterator &i,
                unsigned int n)
{
   valtype::const_iterator v_end = v.end();
   for (unsigned int j = 0; j < n; ++j)
   {
       if (i == v_end)
       {
           return false;
       }
       ++i;
   }
   return true;
}
