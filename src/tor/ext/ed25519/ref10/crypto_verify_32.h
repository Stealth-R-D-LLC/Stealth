/* Added for Tor. */
#include "lib/di_ops.h"
#define crypto_verify_32(a,b) \
  (! tor_memeq((a), (b), 32))
