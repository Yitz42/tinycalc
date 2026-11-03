// ovl_cas.c - the "cas" overlay: symbolic derivative, antiderivative, factor
// and expand (cas.c), with the parser bits and math functions they use.
#include "../c/ovl_api.h"
#include "../c/ovl_cas.h"
#include "../c/cas.h"
#include "ovl_rt.h"

int ovl_main(const OvlApi *api, int cmd, void *args) {
  if (!api || api->version != OVL_API_VERSION) return 0;
  ovl_rt_init(api);
  OcArgs *a = args;
  switch (cmd) {
    case OC_DER: a->ret = cas_derivative(a->f, a->v, a->out); return 1;
    case OC_INT: a->ret = cas_integral(a->f, a->v, a->out); return 1;
    case OC_FACTOR: a->ret = cas_factor(a->f, a->v, a->out); return 1;
    case OC_EXPAND: a->ret = cas_expand(a->f, a->v, a->out); return 1;
  }
  return 0;
}
