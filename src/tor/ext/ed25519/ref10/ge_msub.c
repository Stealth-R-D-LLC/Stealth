#include "ext/ed25519/ref10/ge.h"

/*
r = p - q
*/

void ge_msub(ge_p1p1 *r,const ge_p3 *p,const ge_precomp *q)
{
  fe t0;
#include "ext/ed25519/ref10/ge_msub.h"
}
