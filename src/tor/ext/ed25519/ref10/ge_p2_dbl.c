#include "ext/ed25519/ref10/ge.h"

/*
r = 2 * p
*/

void ge_p2_dbl(ge_p1p1 *r,const ge_p2 *p)
{
  fe t0;
#include "ext/ed25519/ref10/ge_p2_dbl.h"
}
