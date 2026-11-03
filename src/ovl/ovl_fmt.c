// ovl_fmt.c - the "fmt" overlay: the formula typesetter (mathfmt.c). It
// draws only through the MathGfx callbacks the app passes in.
#include "../c/ovl_api.h"
#include "../c/ovl_fmt.h"
#include "ovl_rt.h"

int ovl_main(const OvlApi *api, int cmd, void *args) {
  if (!api || api->version != OVL_API_VERSION) return 0;
  ovl_rt_init(api);
  OfArgs *a = args;
  switch (cmd) {
    case OF_BUILD: math_build_raw(a->d, a->t, a->n); return 1;
    case OF_WRAP_INT: math_wrap_integral(a->d, a->s1, a->s2); return 1;
    case OF_WRAP_DER: math_wrap_derivative(a->d, a->s1); return 1;
    case OF_MEASURE: math_measure(a->d, a->g, a->lvl, &a->w, &a->asc, &a->desc); return 1;
    case OF_DRAW: math_draw(a->d, a->g, a->x, a->y); return 1;
  }
  return 0;
}
