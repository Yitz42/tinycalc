// ovl_math.c - the "math" overlay: exact answers (exact.c), the root finder,
// two- and three-equation systems (calcx.c; integrals and derivatives are
// in the "calcu" overlay), with the parser (calc.c) and
// math functions (mmath.c) they use. Built by tools/build_overlay.py into
// resources/ovl/math.ovl; the app calls it through src/c/ovl_calls.c.
#include "../c/ovl_api.h"
#include "../c/ovl_math.h"
#include "../c/config.h"
#include "../c/sheet.h"
#include "ovl_rt.h"

int ovl_main(const OvlApi *api, int cmd, void *args) {
  if (!api || api->version != OVL_API_VERSION) return 0;
  ovl_rt_init(api);
  switch (cmd) {
    case OM_SOLVE2: {
      OmSolve2 *a = args;
      a->ret = calc_solve2(a->e1, a->e2, a->v, a->x0, a->x1, a->y0, a->y1, a->xs, a->ys, a->max);
      return 1;
    }
    case OM_ROOTS: {
      OmRoots *a = args;
      a->ret = calc_roots(a->b, a->v, a->lo, a->hi, a->out, a->max);
      return 1;
    }
    case OM_SHEET_FORMULA: { OmSheet *a = args; sh_formula(a->io); return 1; }
    case OM_SHEET_SOLVER: { OmSheet *a = args; sh_solver(a->io, a->n); return 1; }
    case OM_SHEET_FIND: { OmSheet *a = args; a->ret = sh_find(a->io); return 1; }
    case OM_SOLVE3: { OmSolve3 *a = args; a->ret = calc_solve3(a->e[0], a->e[1], a->e[2], a->v, a->xyz); return 1; }
    case OM_SHEET_EXACT: { OmSheet *a = args; a->ret = sh_exact(a->io, a->p); return 1; }
#if EXACT_FORMS
    case OM_EXACT: { OmExact *a = args; a->ret = exact_solve(a->e1, a->v, a->x); return 1; }
    case OM_EXACT2: { OmExact *a = args; a->ret = exact_solve2(a->e1, a->e2, a->v, a->x); return 1; }
    case OM_EXACT_VALUE: { OmExact *a = args; a->ret = exact_value(a->value, a->out); return 1; }
#endif
  }
  return 0;
}
