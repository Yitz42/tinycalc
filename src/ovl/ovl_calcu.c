// ovl_calcu.c - the "calcu" overlay: integrals and derivatives (calcx.c),
// with the parser (calc.c) and math functions (mmath.c) they use. Apart from
// the "math" overlay so that either fits in the heap with room to work (an
// integral's pieces, the root finder's samples).
#include "../c/ovl_api.h"
#include "../c/ovl_math.h"
#include "ovl_rt.h"

int ovl_main(const OvlApi *api, int cmd, void *args) {
  if (!api || api->version != OVL_API_VERSION) return 0;
  ovl_rt_init(api);
  OmCalculus *a = args;
  switch (cmd) {
    case OM_INTEGRATE: a->ret = calc_integrate(a->f, a->v, a->a, a->b, a->out, a->approx); return 1;
    case OM_DERIVATIVE: a->ret = calc_derivative2(a->f, a->v, a->a, a->out, a->approx); return 1;
  }
  return 0;
}
