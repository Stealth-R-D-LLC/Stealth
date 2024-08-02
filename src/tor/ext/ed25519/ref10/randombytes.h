/* Added for Tor. */
#include "lib/crypto_rand.h"
#define randombytes(b, n) \
  (crypto_strongest_rand((b), (n)), 0)
