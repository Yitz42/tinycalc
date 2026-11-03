// ovl_calls.c - on the watch, the heavier code lives in overlays (resources
// loaded onto the heap when used): "math" (root finder, systems, integrals,
// derivatives, exact answers), "cas" (symbolic math) and "graph" (sampling
// and drawing graphs). These are the app-side versions of those functions:
// same names and arguments as in calcx.c / exact.c / cas.c (which the PC
// tests link directly), each passing its arguments to its overlay.
#include <pebble.h>
#include "calc.h"
#include "exact.h"
#include "config.h"
#include "ovl.h"
#include "ovl_math.h"
#include "ovl_cas.h"
#include "cas.h"
#include "ovl_fmt.h"
#include "sheet.h"
#include "ovl_tools.h"

// One slot per overlay. Each is loaded on first use and kept, except the
// symbolic one (big, used now and then), which is freed right after each call;
// if the heap is short, the others are freed and the load is tried again.
// The typesetter (fmt) is loaded at start and kept, but makes way whenever
// another overlay is loaded or a running one needs memory (it is loaded again
// when next needed, so it ends up above the others).
enum { OV_MATH, OV_CAS, OV_GRAPH, OV_FMT, OV_TOOLS, OV_CPLX, OV_CALCU, OV_COUNT };
static Ovl s_ov[OV_COUNT];
static const uint32_t RES[OV_COUNT] = { RESOURCE_ID_OVL_MATH, RESOURCE_ID_OVL_CAS, RESOURCE_ID_OVL_GRAPH,
                                        RESOURCE_ID_OVL_FMT, RESOURCE_ID_OVL_TOOLS, RESOURCE_ID_OVL_CPLX, RESOURCE_ID_OVL_CALCU };
static void log_msg(const char *m) { APP_LOG(APP_LOG_LEVEL_INFO, "%s", m); }
static unsigned s_running;                             // overlays now running (bit per slot): never freed

static void unload_except(int keep) {
  for (int i = 0; i < OV_COUNT; i++)
    if (i != keep && i != OV_FMT && !(s_running & (1u << i)) && s_ov[i].mem) ovl_unload(&s_ov[i]);
}
// Memory for a running overlay (the root finder's samples, an integral's
// pieces): if the heap is short, the idle overlays make way.
static void *ovl_malloc(size_t n) {
  void *p = malloc(n);
  if (!p) { unload_except(-1); p = malloc(n); }
  // still short: the typesetter too, unless it is what's running (it is
  // loaded again when next needed)
  if (!p && !(s_running & (1u << OV_FMT)) && s_ov[OV_FMT].mem) { ovl_unload(&s_ov[OV_FMT]); p = malloc(n); }
  return p;
}
static const OvlApi API = { OVL_API_VERSION, ovl_malloc, free, log_msg };

static bool ensure(int id) {
  if (s_ov[id].mem) return true;
  // The typesetter makes way first (it is loaded again when next needed, above
  // this one): left in the middle it would split the free memory, and the
  // overlay's own working memory (a root search, an algebra tree) might not fit.
  if (id != OV_FMT && !(s_running & (1u << OV_FMT)) && s_ov[OV_FMT].mem) ovl_unload(&s_ov[OV_FMT]);
  if (ovl_load(&s_ov[id], RES[id])) return true;
  unload_except(id);                                   // make room and try again
  if (ovl_load(&s_ov[id], RES[id])) return true;
  APP_LOG(APP_LOG_LEVEL_ERROR, "overlay %d didn't load", id);
  return false;
}

static bool call(int id, int cmd, void *args) {
  if (!ensure(id)) return false;
  unsigned was = s_running;
  s_running |= 1u << id;
  bool ok = s_ov[id].entry(&API, cmd, args) != 0;
  s_running = was;
  return ok;
}
static bool run(int cmd, void *args) { return call(OV_MATH, cmd, args); }

void ovl_unload_all(void) { unload_except(-1); ovl_unload(&s_ov[OV_FMT]); }

int calc_roots(const TokBuf *b, const CalcVars *v, double lo, double hi, double *out, int max) {
  OmRoots a = { b, v, lo, hi, out, max, 0 };
  return run(OM_ROOTS, &a) ? a.ret : 0;
}

// ---- the sheets' logic (sheet.c, in the math overlay). It doesn't typeset:
// it says what to typeset, and the app does it once math has returned, so the
// two never need memory at the same time.
void sh_formula(SheetIO *io) { OmSheet a = { .io = io }; run(OM_SHEET_FORMULA, &a); }
void sh_solver(SheetIO *io, bool sys) { OmSheet a = { .io = io, .n = sys }; run(OM_SHEET_SOLVER, &a); }
int sh_find(SheetIO *io) { OmSheet a = { .io = io }; return run(OM_SHEET_FIND, &a) ? a.ret : 0; }
bool sh_exact(SheetIO *io, TokBuf *out) { OmSheet a = { .io = io, .p = out }; return run(OM_SHEET_EXACT, &a) && a.ret; }

// ---- complex evaluation (cplx overlay)
typedef struct { const TokBuf *b; const CalcVars *v; double *re, *im; int ret; } OxArgs;
CalcErr calc_ceval(const TokBuf *b, const CalcVars *v, double *re, double *im) {
  OxArgs a = { b, v, re, im, CALC_ERR_MATH };
  return call(OV_CPLX, 1, &a) ? (CalcErr)a.ret : CALC_ERR_MATH;
}

// ---- the TOOLS lists (tools overlay, loaded while a list is open)
int tl_count(ToolsIO *io, int list) { OtArgs a = { io, list, 0, 0 }; return call(OV_TOOLS, OT_COUNT, &a) ? a.ret : 0; }
void tl_row(ToolsIO *io, int list, int i) {
  OtArgs a = { io, list, i, 0 };
  if (!call(OV_TOOLS, OT_ROW, &a)) io->label[0] = io->value[0] = 0;
}
int tl_pick(ToolsIO *io, int list, int i) { OtArgs a = { io, list, i, 0 }; return call(OV_TOOLS, OT_PICK, &a) ? a.ret : TA_NONE; }

// ---- symbolic (cas overlay): the others are freed first, it's freed after
static bool cas_call(int cmd, const TokBuf *f, const CalcVars *v, TokBuf *out) {
  unload_except(OV_CAS);
  OcArgs a = { f, v, out, 0 };
  bool ok = call(OV_CAS, cmd, &a) && a.ret;
  ovl_unload(&s_ov[OV_CAS]);
  return ok;
}
bool cas_derivative(const TokBuf *f, const CalcVars *v, TokBuf *out) { return cas_call(OC_DER, f, v, out); }
bool cas_integral(const TokBuf *f, const CalcVars *v, TokBuf *out) { return cas_call(OC_INT, f, v, out); }
bool cas_factor(const TokBuf *f, const CalcVars *v, TokBuf *out) { return cas_call(OC_FACTOR, f, v, out); }
bool cas_expand(const TokBuf *f, const CalcVars *v, TokBuf *out) { return cas_call(OC_EXPAND, f, v, out); }

// ---- graph overlay
bool graph_call(int cmd, void *args) { return call(OV_GRAPH, cmd, args); }

int calc_solve2(const TokBuf *e1, const TokBuf *e2, const CalcVars *v,
                double x0, double x1, double y0, double y1, double *xs, double *ys, int max) {
  OmSolve2 a = { e1, e2, v, x0, x1, y0, y1, xs, ys, max, 0 };
  return run(OM_SOLVE2, &a) ? a.ret : 0;
}

CalcErr calc_solve3(const TokBuf *e1, const TokBuf *e2, const TokBuf *e3, const CalcVars *v, double xyz[3]) {
  OmSolve3 a = { { e1, e2, e3 }, v, xyz, CALC_ERR_NOSOL };
  return run(OM_SOLVE3, &a) ? (CalcErr)a.ret : CALC_ERR_NOSOL;
}

CalcErr calc_integrate(const TokBuf *f, const CalcVars *v, double lo, double hi, double *out, bool *approx) {
  OmCalculus a = { f, v, lo, hi, out, approx, CALC_ERR_MATH };
  return call(OV_CALCU, OM_INTEGRATE, &a) ? (CalcErr)a.ret : CALC_ERR_MATH;
}

CalcErr calc_derivative2(const TokBuf *f, const CalcVars *v, double x0, double *out, bool *approx) {
  bool ap = false;
  OmCalculus a = { f, v, x0, 0, out, &ap, CALC_ERR_MATH };
  CalcErr e = call(OV_CALCU, OM_DERIVATIVE, &a) ? (CalcErr)a.ret : CALC_ERR_MATH;
  if (approx) *approx = ap;
  return e;
}
CalcErr calc_derivative(const TokBuf *f, const CalcVars *v, double x0, double *out) {
  return calc_derivative2(f, v, x0, out, NULL);
}

#if EXACT_FORMS
bool exact_solve(const TokBuf *f, const CalcVars *v, Exact *e) {
  OmExact a = { f, NULL, v, e, 0, NULL, 0 };
  return run(OM_EXACT, &a) && a.ret;
}
bool exact_solve2(const TokBuf *e1, const TokBuf *e2, const CalcVars *v, Exact *e) {
  OmExact a = { e1, e2, v, e, 0, NULL, 0 };
  return run(OM_EXACT2, &a) && a.ret;
}
bool exact_value(double val, TokBuf *out) {
  OmExact a = { NULL, NULL, NULL, NULL, val, out, 0 };
  return run(OM_EXACT_VALUE, &a) && a.ret;
}
#endif

// ---- the typesetter (fmt overlay, resident)
bool fmt_overlay_load(void) { return ensure(OV_FMT); }
static bool fmt(int cmd, OfArgs *a) { return call(OV_FMT, cmd, a); }   // (loaded again if it made way)

void math_build_raw(MathDoc *d, const uint8_t *t, int n) {
  OfArgs a = { .d = d, .t = t, .n = n };
  if (!fmt(OF_BUILD, &a)) { d->count = 0; d->root = 0; }
}
void math_build(MathDoc *d, const TokBuf *b) { math_build_raw(d, b->t, b->n); }
void math_wrap_integral(MathDoc *d, const char *lo, const char *hi) {
  OfArgs a = { .d = d, .s1 = lo, .s2 = hi };
  fmt(OF_WRAP_INT, &a);
}
void math_wrap_derivative(MathDoc *d, const char *at) {
  OfArgs a = { .d = d, .s1 = at };
  fmt(OF_WRAP_DER, &a);
}
void math_measure(MathDoc *d, const MathGfx *g, int lvl, int *w, int *asc, int *desc) {
  OfArgs a = { .d = d, .g = g, .lvl = lvl, .w = 9999, .asc = 999, .desc = 999 };   // too big if not loaded
  fmt(OF_MEASURE, &a);
  *w = a.w; *asc = a.asc; *desc = a.desc;
}
void math_draw(const MathDoc *d, const MathGfx *g, int x, int baseline) {
  OfArgs a = { .d = (MathDoc *)d, .g = g, .x = x, .y = baseline };
  fmt(OF_DRAW, &a);
}
