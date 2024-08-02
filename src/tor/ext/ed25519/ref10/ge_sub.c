#include "ext/ed25519/ref10/ge.h"

/*
r = p - q
*/

void ge_sub(ge_p1p1 *r,const ge_p3 *p,const ge_cached *q)
{
  fe t0;
#include "ext/ed25519/ref10/ge_sub.h"
}
