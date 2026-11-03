// ovl_cplx.c - the "cplx" overlay: complex evaluation (cplx.c), with the
// parser bits and math functions it uses. Loaded for = on something complex.
#include "../c/ovl_api.h"
#include "../c/calc.h"
#include "ovl_rt.h"

typedef struct { const TokBuf *b; const CalcVars *v; double *re, *im; int ret; } OxArgs;

int ovl_main(const OvlApi *api, int cmd, void *args) {
  if (!api || api->version != OVL_API_VERSION) return 0;
  ovl_rt_init(api);
  OxArgs *a = args;
  if (cmd != 1) return 0;
  a->ret = calc_ceval(a->b, a->v, a->re, a->im);
  return 1;
}
