// fuzz.c - differential fuzzing: random key sequences through the calculator,
// then the formula typesetter, root finder, exact answers, symbolic math and
// graphs on whatever was typed. Built twice and the outputs compared:
//   PC:  everything linked directly, with AddressSanitizer
//   ARM: under qemu-arm, the real overlay files loaded through ovl.c onto a
//        capped heap, the way the watch runs them
// Lines starting with '#' are build-specific (heap use) and not compared.
//   fuzz <seed> <sequences> [heap cap]
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "calc.h"
#include "cas.h"
#include "exact.h"
#include "mathfmt.h"
#include "ovl_graph.h"

#ifdef EMU
static void tick(void) {}
static void tock(const char *w) { (void)w; }
void emu_write(const char *s, int n);
void emu_heap_init(uint32_t cap);
int emu_heap_check(void);
uint32_t emu_heap_peak(void);
uint32_t emu_heap_used(void);
void *malloc(size_t n);
void free(void *p);
static void out(const char *s) { int n = 0; while (s[n]) n++; emu_write(s, n); }
#else
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static void out(const char *s) { fputs(s, stdout); }
static clock_t t_start;
static TokBuf t_what;
static void tick(void) { t_start = clock(); }
static void tock(const char *what) {
  double ms = (double)(clock() - t_start) * 1000.0 / CLOCKS_PER_SEC;
  if (ms > 15) { fprintf(stderr, "SLOW %.1f ms %s:", ms, what); for (int i = 0; i < t_what.n; i++) fprintf(stderr, " %d", t_what.t[i]); fprintf(stderr, "\n"); }
}
int graph_run(int cmd, GrArgs *a);
bool graph_call(int cmd, void *args) { return graph_run(cmd, args) != 0; }
#endif

static char L[1600];
static int Ln;
static void put(const char *s) { while (*s && Ln < (int)sizeof L - 2) L[Ln++] = *s++; L[Ln] = 0; }
static void puti(long long v) {
  char b[32]; int i = 31; b[i] = 0; bool ng = v < 0; unsigned long long u = ng ? -(unsigned long long)v : (unsigned long long)v;
  do { b[--i] = (char)('0' + u % 10); u /= 10; } while (u);
  if (ng) b[--i] = '-';
  put(b + i);
}
static void putd(double v, int sig) { char b[40]; calc_format(v, sig, b, sizeof b); put(b); }
static void flush(void) { put("\n"); out(L); Ln = 0; L[0] = 0; }
static void toks(const TokBuf *b) { for (int i = 0; i < b->n; i++) { if (i) put(","); puti(b->t[i]); } }

static uint32_t seed;
static uint32_t rnd(void) { seed = seed * 1103515245u + 12345u; return (seed >> 8) & 0xFFFFFF; }

static const int KEYS[] = {
  '0','1','2','3','4','5','6','7','8','9','0','1','2','3','.','+','-','*','/','^','(',')','=','=',
  T_X,T_X,T_X,T_X,T_Y,T_Z,T_PI,T_E,T_ANS,T_SQ,T_SQ,T_CUBE,T_RECIP,T_PCT,T_FACT,T_EE,
  T_SIN,T_COS,T_TAN,T_ASIN,T_ACOS,T_ATAN,T_LN,T_LOG,T_EXP,T_TENX,T_SQRT,T_CBRT,T_ABS,
  T_SINH,T_COSH,T_TANH,T_ASINH,T_ACOSH,T_ATANH,T_ROUND,T_FLOOR,T_GCD,T_LCM,',',T_NCR,T_NPR,T_MOD,
  T_I,T_I,T_I,T_LOGB,T_ROOT,T_SUM,T_PROD,T_MA,T_MB,T_AND,T_XOR,T_NOT,T_CONJ,T_RE,T_IM,T_ARG,
  T_NCDF,T_NINV,T_NPDF,T_RAN,T_RANINT,
  K_POL,K_REC,
  K_EVAL,K_EVAL,K_EVAL,K_DEL,K_DEL,K_INV,K_DEG,K_STOX,K_RCLX,K_INT,K_DER,K_FACTOR,K_EXPAND,K_AC,K_CLRXY,
};

// ---- counting graphics: the number of calls and a checksum of the coordinates
static uint32_t gcount, gsum;
static void gadd(int a, int b, int c, int d) { gcount++; gsum = gsum * 31u + (uint32_t)(a * 7 + b * 13 + c * 17 + d * 19); }
static void g_fill(void *c, int x, int y, int w, int h, uint8_t col) { (void)c; gadd(x, y, w, h + col); }
static void g_line(void *c, int x0, int y0, int x1, int y1, uint8_t col, int w) { (void)c; gadd(x0, y0, x1, y1 + col + w); }
static void g_circle(void *c, int x, int y, int r, uint8_t col, bool f) { (void)c; gadd(x, y, r, col + f); }
static void g_text(void *c, const char *t, int x, int y, int w, bool big, uint8_t col, int al) {
  (void)c; int n = 0; while (t[n]) n++; gadd(x, y, w + n, col + big + al);
}
static GfxApi GFX = { NULL, g_fill, g_line, g_circle, g_text };

static int m_tw(void *c, const char *t, int l) { (void)c; int n = 0; while (t[n]) n++; return n * (8 - l); }
static void m_tx(void *c, int x, int y, const char *t, int l) { (void)c; int n = 0; while (t[n]) n++; gadd(x, y, n, l); }
static void m_ln(void *c, int x0, int y0, int x1, int y1, int w) { (void)c; gadd(x0, y0, x1, y1 + w); }
static void m_bx(void *c, int x, int y, int w, int h) { (void)c; gadd(x, y, w, h); }
static void m_ink(void *c, int w) { (void)c; gadd(w, 0, 0, 0); }
static MathGfx MG = { m_tw, m_tx, m_ln, m_bx, m_ink, NULL, { 18, 15, 11, 9 }, { 6, 5, 4, 3 } };

static MathDoc *doc;
static Exact *ex;
static GraphState *gs;

static void typeset(const char *what, const TokBuf *b) {
  math_build(doc, b);
  put(what);
  for (int l = 0; l < 4; l++) {
    int w, a, d;
    math_measure(doc, &MG, l, &w, &a, &d);
    put(" "); puti(w); put("/"); puti(a); put("/"); puti(d);
  }
  gcount = gsum = 0;
  math_draw(doc, &MG, 3, 40);
  put(" draw "); puti(gcount); put(":"); puti((long long)gsum);
  flush();
}

static void graph(const TokBuf *b, const CalcVars *v) {
  GraphState *g = gs;
  g->x = 0; g->y = 22; g->w = 200; g->h = 206; g->labels = true; g->trace = true; g->tx = 1.5; g->tcurve = 0;
  g->x0 = -10; g->x1 = 10; g->y0 = -10; g->y1 = 10; g->nroots = 0;
  g->kind = calc_has_y(b) ? GK_IMPLICIT : calc_has_eq(b) ? GK_SIDES : GK_FX;
  GrArgs a = { .g = g, .f = b, .v = v, .gfx = &GFX };
  gcount = gsum = 0;
  bool ok = graph_call(GR_SAMPLE, &a) && graph_call(GR_FIT, &a) && graph_call(GR_SAMPLE, &a) &&
            graph_call(GR_TRACE, &a) && graph_call(GR_DRAW, &a);
  put("graph "); puti(ok); put(" k"); puti(g->kind); put(" y "); putd(g->y0, 6); put(".."); putd(g->y1, 6);
  put(" ty "); putd(g->ty, 6); put(" draw "); puti(gcount); put(":"); puti((long long)gsum);
  graph_call(GR_FREE, &a);
  flush();
}

static void t_what_set(const TokBuf *b) {
#ifndef EMU
  t_what = *b;
#else
  (void)b;
#endif
}
static void calcs(const TokBuf *b, const CalcVars *v) {
  if (!b->n) return;
  typeset("fmt", b);
  {   // complex evaluation (its own overlay on the watch)
    double re, im;
    int e = calc_has_eq(b) ? -1 : calc_ceval(b, v, &re, &im);
    put("cplx "); puti(e);
    if (e == CALC_OK) { put(" "); putd(re, 10); put(" "); putd(im, 10); }
    flush();
  }
  if (calc_has_x(b) && !calc_has_y(b)) {
    double r[12];
    t_what_set(b); tick();
    int n = calc_roots(b, v, -10, 10, r, 12);
    tock("roots");
    put("roots "); puti(n);
    for (int i = 0; i < n; i++) { put(" "); putd(r[i], 9); }
    flush();
    bool ok = exact_solve(b, v, ex);
    put("exact "); puti(ok);
    if (ok) { put(" n"); puti(ex->n); put(" simp "); toks(&ex->simp); for (int i = 0; i < ex->n; i++) { put(" | "); toks(&ex->sol[i]); } }
    flush();
  }
  TokBuf o;
  t_what_set(b);
  tick(); put("d/dx "); if (cas_derivative(b, v, &o)) toks(&o); else put("-"); flush(); tock("d/dx");
  tick(); put("int "); if (cas_integral(b, v, &o)) toks(&o); else put("-"); flush(); tock("int");
  tick(); put("factor "); if (cas_factor(b, v, &o)) toks(&o); else put("-"); flush(); tock("factor");
  tick();
  put("expand "); if (cas_expand(b, v, &o)) { toks(&o); flush(); typeset("fmt-expand", &o); } else { put("-"); flush(); }
  tock("expand");
  tick(); graph(b, v); tock("graph");
}

// ---- valid expressions from a small grammar (so the solvers get real work)
static TokBuf G;
static void g(int t) { if (G.n < CALC_MAX_TOK - 4) G.t[G.n++] = (uint8_t)t; }
static void gnum(void) {
  int v = rnd() % 4 == 0 ? (int)(rnd() % 100) : (int)(1 + rnd() % 9);
  if (v >= 10) g('0' + v / 10);
  g('0' + v % 10);
  if (rnd() % 6 == 0) { g('.'); g('0' + rnd() % 10); }
}
static void gexpr(int depth, bool y);
static void gatom(int depth, bool y) {
  int r = rnd() % (depth > 3 ? 4 : 12);
  static const int F[] = { T_SIN, T_COS, T_TAN, T_LN, T_EXP, T_SQRT, T_ATAN, T_SINH, T_COSH, T_ABS, T_CBRT, T_LOG, T_TANH };
  switch (r) {
    case 0: case 1: g(T_X); break;
    case 2: gnum(); break;
    case 3: if (y) g(T_Y); else { gnum(); g(T_X); } break;
    case 4: case 5: g('('); gexpr(depth + 1, y); g(')'); break;
    case 6: case 7: g(F[rnd() % 13]); gexpr(depth + 1, y); g(')'); break;
    case 8: g(T_X); g(rnd() % 2 ? T_SQ : T_CUBE); break;
    case 9: g('('); gexpr(depth + 1, y); g(')'); g('^'); g('0' + 2 + rnd() % 3); break;
    case 10: g(rnd() % 2 ? T_PI : T_I); break;
    default: gnum(); g(T_X); g(T_SQ); break;
  }
}
static void gterm(int depth, bool y) {
  gatom(depth, y);
  int n = rnd() % 3;
  for (int i = 0; i < n; i++) { int op = rnd() % 3; if (op == 0) g('*'); else if (op == 1) g('/'); gatom(depth + 1, y); }
}
static void gexpr(int depth, bool y) {
  if (rnd() % 7 == 0) g('-');
  gterm(depth, y);
  int n = rnd() % 3;
  for (int i = 0; i < n; i++) { g(rnd() % 2 ? '+' : '-'); gterm(depth + 1, y); }
}

int main(int argc, char **argv) {
  long s = 1, n = 50, capv = 60000;
  if (argc > 1) { s = 0; for (const char *p = argv[1]; *p; p++) s = s * 10 + (*p - '0'); }
  if (argc > 2) { n = 0; for (const char *p = argv[2]; *p; p++) n = n * 10 + (*p - '0'); }
  if (argc > 3) { capv = 0; for (const char *p = argv[3]; *p; p++) capv = capv * 10 + (*p - '0'); }
#ifdef EMU
  emu_heap_init((uint32_t)capv);
#endif
  (void)capv;
  seed = (uint32_t)s;
  doc = malloc(sizeof(MathDoc));
  ex = malloc(sizeof(Exact));
  gs = malloc(sizeof(GraphState));
  for (size_t i = 0; i < sizeof(GraphState); i++) ((uint8_t *)gs)[i] = 0;
  gs->ca = malloc(GR_COLS * sizeof(float));
  gs->cb = malloc(GR_COLS * sizeof(float));
#ifdef EMU
  // the rest of what the app keeps (second doc, history, stats, carets, lines)
  // and the system's window and layers, all allocated before the typesetter
  if (argc > 4) { size_t pad = 0; for (const char *p = argv[4]; *p; p++) pad = pad * 10 + (size_t)(*p - '0'); (void)malloc(pad); }
  extern bool fmt_overlay_load(void);
  if (!fmt_overlay_load()) out("# fmt overlay didn't load\n");
#endif
  Calc c;
  for (long q = 0; q < n; q++) {
    calc_init(&c);
    c.v.deg = rnd() % 2;
    int len = 3 + rnd() % 22;
    put("== seq "); puti(q); flush();
    for (int k = 0; k < len; k++) {
      int key = KEYS[rnd() % (sizeof KEYS / sizeof KEYS[0])];
      calc_key(&c, key);
      put("k"); puti(key); put(" m"); puti(c.mode); put(" e"); puti(c.err); put(" s"); puti(c.sym);
      put(" o"); puti(c.op); put(" t"); puti(c.step); put(" ["); toks(&c.expr); put("] ["); toks(&c.eq1);
      put("] "); putd(c.result, 10); put(" i"); putd(c.result_im, 10); put(" x="); putd(c.v.x, 10);
      flush();
      if (c.mode == MODE_RESULT && c.sym) typeset("fmt-sym", &c.expr);
    }
    calcs(&c.expr, &c.v);
    if (c.eq1.n) calcs(&c.eq1, &c.v);
    // and a generated expression or equation
    G.n = 0;
    bool y = rnd() % 5 == 0;
    gexpr(0, y);
    if (rnd() % 3 == 0) { g('='); gexpr(1, y); }
    put("gen "); toks(&G); flush();
    calcs(&G, &c.v);
    // and raw random tokens: anything at all, up to the full length
    G.n = 0;
    int rl = rnd() % 4 == 0 ? CALC_MAX_TOK : 1 + rnd() % 30;
    for (int i = 0; i < rl; i++) {
      int k = KEYS[rnd() % (sizeof KEYS / sizeof KEYS[0])];
      if (k >= 200) k = rnd() % 2 ? T_X : ')';
      G.t[G.n++] = (uint8_t)k;
    }
    if (rnd() % 8 == 0) G.t[rnd() % G.n] = (uint8_t)(T_SEP + rnd() % 4);   // display-only tokens too
    put("raw "); toks(&G); flush();
    { double l, r; put("eval "); puti(calc_eval_eq(&G, &c.v, &l, &r)); flush(); }
    calcs(&G, &c.v);
    if (calc_has_x(&G) && !calc_has_y(&G) && !calc_has_eq(&G)) {   // integral and derivative values
      double r = 0; bool ap = false;
      t_what_set(&G); tick();
      put("integral "); puti(calc_integrate(&G, &c.v, 0.25, 1.75, &r, &ap)); tock("integral"); put(" "); putd(r, 9);
      r = 0; put(" deriv "); puti(calc_derivative(&G, &c.v, 0.6, &r)); put(" "); putd(r, 9); flush();
    }
#ifdef EMU
    emu_heap_check();
    put("# heap used "); puti(emu_heap_used()); put(" peak "); puti(emu_heap_peak()); flush();
#endif
  }
  put("done"); flush();
  return 0;
}
