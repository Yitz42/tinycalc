// sheet.c - see src/c/sheet.h. Moved here from main.c to keep the app under
// its 64 KB; on the watch it's part of the "math" overlay.
#include "../c/sheet.h"
#include "../c/mmath.h"
#include "../c/config.h"
#include <stddef.h>

static int join(SheetIO *io) {                 // eq1 ; expr (three equations: one under the other)
  const Calc *c = io->calc;
  int n = 0;
  for (int i = 0; i < c->eq1.n; i++) io->sys_tok[n++] = c->eq1.t[i];
  if (c->eq2.n && n + c->eq2.n + c->expr.n + 2 <= io->sys_cap) {
    io->sys_tok[n++] = T_NL;
    for (int i = 0; i < c->eq2.n; i++) io->sys_tok[n++] = c->eq2.t[i];
    io->sys_tok[n++] = T_NL;
    for (int i = 0; i < c->expr.n; i++) io->sys_tok[n++] = c->expr.t[i];
    return n;
  }
  io->sys_tok[n++] = T_SEP;
  for (int i = 0; i < c->expr.n; i++) io->sys_tok[n++] = c->expr.t[i];
  return n;
}

static void str(char *d, const char *s) { while ((*d++ = *s++)) {} }
static void req(SheetIO *io, const uint8_t *t, int n) { io->bt = t; io->bn = n; io->wrap = 0; }

void sh_formula(SheetIO *io) {
  Calc *c = io->calc;
  uint8_t *st = io->sys_tok;
  *io->doc_tok = c->expr;
  if (c->mode == MODE_RESULT && c->solved && c->stored) {   // RCL x
    io->doc_tok->n = 1;
    io->doc_tok->t[0] = c->var ? T_Y : T_X;
  }
  bool sys = c->mode == MODE_EQ2 || ((c->mode == MODE_RESULT || c->mode == MODE_ERROR) && c->sys);
  bool op = c->op && (c->mode == MODE_LIM || c->mode == MODE_RESULT || c->mode == MODE_ERROR);
  if (c->mode == MODE_RESULT && c->sym) {          // a formula answer: the formula, big
    *io->doc_tok = c->expr;                        // a copy: the sheet can outlive the expression
    req(io, io->doc_tok->t, io->doc_tok->n);
  } else if (op) {                                 // integral / derivative of f(x)
    for (int i = 0; i < c->eq1.n; i++) st[i] = c->eq1.t[i];   // a copy, as above
    req(io, st, c->eq1.n);
    bool done = c->mode != MODE_LIM;
    if (c->op == OP_INT) {
      if (done || c->step == 1) calc_format(c->lim_a, 6, io->lo, sizeof io->lo); else str(io->lo, "a");
      if (done) calc_format(c->lim_b, 6, io->hi, sizeof io->hi); else str(io->hi, "b");
      io->wrap = 1;
    } else {
      str(io->at, "x=");
      calc_format(done ? c->lim_a : c->v.x, 6, io->at + 2, sizeof io->at - 2);
      io->wrap = done ? 3 : 2;
    }
  } else if (sys) {
    int n = join(io);                              // both equations, braced
    const Exact *e = io->exact;
    if (EXACT_FORMS && c->mode == MODE_RESULT && !c->sys3 && exact_solve2(&c->eq1, &c->expr, &c->v, io->exact) &&
        m_fabs(e->x[0] - c->result) < 1e-7 * (1 + m_fabs(c->result)) &&       // and it's the answer shown
        m_fabs(e->y[0] - c->v.y) < 1e-7 * (1 + m_fabs(c->v.y)) &&             // (|x - 9| + y = 3 isn't linear)
        n + 1 + e->sol[0].n <= io->sys_cap) {
      st[n++] = T_NL;                              // ...and the exact solution under them
      for (int i = 0; i < io->exact->sol[0].n; i++) st[n++] = io->exact->sol[0].t[i];
    }
    req(io, st, n);
  } else {
    // With an answer, add its exact form: the root under the equation, or
    // "= 2sqrt(3)" after a plain calculation.
    const TokBuf *ex = NULL;
    uint8_t joint = T_NL;
    TokBuf val;
    const TokBuf *dt = io->doc_tok;
    if (EXACT_FORMS && c->mode == MODE_RESULT && !c->stored) {
      if (c->solved && exact_solve(dt, &c->v, io->exact)) {
        for (int j = 0; j < io->exact->n; j++) {
          double d = io->exact->x[j] - c->result, tol = 1e-7 * (1 + m_fabs(c->result));
          if (io->exact->real[j] && d < tol && -d < tol) ex = &io->exact->sol[j];
        }
      } else if (!c->solved && !calc_has_eq(dt) && exact_value(c->result, &val)) {
        ex = &val;
        joint = '=';
      }
    }
    if (ex) {
      int n = 0;
      for (int i = 0; i < dt->n; i++) st[n++] = dt->t[i];
      st[n++] = joint;
      for (int i = 0; i < ex->n; i++) st[n++] = ex->t[i];
      req(io, st, n);
    } else {
      req(io, dt->t, dt->n);
    }
  }
}

void sh_solver(SheetIO *io, bool sys) {
  Calc *c = io->calc;
  SolState *S = io->s;
  TokBuf *dt = io->doc_tok;
  S->sys = sys;
  if (sys) {
    S->has_x = true;
    S->has_eq = true;
    S->has_exact = EXACT_FORMS && exact_solve2(&c->eq1, &c->expr, &c->v, io->exact);
    req(io, io->sys_tok, join(io));
  } else {
    *dt = c->expr;
    S->has_x = calc_has_x(dt);
    S->has_eq = calc_has_eq(dt);
    S->has_exact = EXACT_FORMS && exact_solve(dt, &c->v, io->exact);
    bool same = S->has_exact && io->exact->simp.n == dt->n;
    for (int i = 0; same && i < dt->n; i++) same = io->exact->simp.t[i] == dt->t[i];
    if (S->has_exact && io->exact->simp.n && !same) {   // the equation, then its simplified form
      int n = 0;
      for (int i = 0; i < dt->n; i++) io->sys_tok[n++] = dt->t[i];
      io->sys_tok[n++] = T_NL;
      for (int i = 0; i < io->exact->simp.n && n < io->sys_cap; i++) io->sys_tok[n++] = io->exact->simp.t[i];
      req(io, io->sys_tok, n);
    } else {
      req(io, dt->t, dt->n);
    }
  }
  for (int i = 0; i < MAX_ROOTS; i++) { S->rx[i] = -1; S->rcplx[i] = false; }
}

// Pair the numeric answers with their exact forms; add exact ones the search
// missed (outside the range) and complex ones (shown, not usable).
static void merge_exact(SheetIO *io) {
  SolState *S = io->s;
  const Exact *e = io->exact;
  for (int i = 0; i < MAX_ROOTS; i++) { S->rx[i] = -1; S->rcplx[i] = false; }
  if (!S->has_exact) return;
  // Does the exact answer agree with what the search found? A coefficient
  // too small to tell from rounding can make it wrong: (x-1)^2 = 1E-14 is
  // not (x-1)^2 = 0 (two roots, not one), x^2 = 1E-20 not x^2 = 0. If not,
  // no exact forms at all (nor the "simplified" equation).
  bool bad = false;
  int hit[EXACT_MAX] = { 0 };
  for (int i = 0; i < S->nroots && !bad; i++) {
    bool near = false;
    for (int j = 0; j < e->n; j++) {
      if (!e->real[j]) continue;
      double dx = e->x[j] - S->roots[i], dy = S->sys ? e->y[j] - S->root_y[i] : 0;
      double tol = 1e-7 * (1 + m_fabs(S->roots[i]));
      if (m_fabs(dx) + m_fabs(dy) < 1e-2 * (1 + m_fabs(S->roots[i]))) near = true;   // (a double root is found less exactly)
      if (!(dx < tol && dx > -tol && dy < tol && dy > -tol)) continue;
      if (e->x[j] == 0 && m_fabs(S->roots[i]) > 1e-12) { bad = true; break; }   // 1e-10 is not x = 0
      if (++hit[j] > 1) bad = true;                     // two roots found where it says one
      break;
    }
    // A root the exact answer doesn't have: |x - 8| = 1 agrees with x - 7 = 0
    // only up to the kink. Fine only if the simplified equation holds there
    // (an irrational root it couldn't write exactly).
    if (!near && !bad) {
      double x = S->roots[i], l, r, l2, r2;
      if (S->sys || !calc_sides(&e->simp, &io->calc->v, x, &l, &r) ||
          !calc_sides(&e->simp, &io->calc->v, x + 1e-6 * (1 + m_fabs(x)), &l2, &r2) ||
          m_fabs(l - r) > 0.01 * m_fabs((l2 - r2) - (l - r))) bad = true;
    }
  }
  for (int j = 0; j < e->n && !bad; j++) {               // a real root the search didn't find must hold up
    if (hit[j] || !e->real[j]) continue;
    double l, r;
    if (S->sys) {                                        // both equations at (x, y) (floor(10x) + y = 3 isn't linear)
      CalcVars w = io->calc->v;
      w.x = e->x[j]; w.y = e->y[j];
      const TokBuf *E2[2] = { &io->calc->eq1, &io->calc->expr };
      for (int q = 0; q < 2 && !bad; q++)
        if (calc_eval_eq(E2[q], &w, &l, &r) != CALC_OK ||
            m_fabs(l - r) > 1e-9 * (m_fabs(l) + m_fabs(r)) + 1e-12 * (1 + m_fabs(w.x) + m_fabs(w.y))) bad = true;
      continue;
    }
    if (!calc_sides(io->doc_tok, &io->calc->v, e->x[j], &l, &r) ||
        m_fabs(l - r) > 1e-9 * (m_fabs(l) + m_fabs(r))) bad = true;
  }
  if (bad) {
    S->has_exact = false;
    if (!S->sys) req(io, io->doc_tok->t, io->doc_tok->n);
    return;
  }
  bool used[EXACT_MAX] = { false };
  for (int i = 0; i < S->nroots; i++)
    for (int j = 0; j < e->n; j++) {
      double dx = e->x[j] - S->roots[i], dy = S->sys ? e->y[j] - S->root_y[i] : 0;
      double tol = 1e-7 * (1 + m_fabs(S->roots[i]));
      if (e->x[j] == 0 && m_fabs(S->roots[i]) > 1e-12) continue;   // 1e-8 (x^2 = 1E-16) is not x = 0
      if (!used[j] && e->real[j] && dx < tol && dx > -tol && dy < tol && dy > -tol) {
        S->rx[i] = (int8_t)j; used[j] = true; break;
      }
    }
  for (int j = 0; j < e->n && S->nroots < MAX_ROOTS; j++) {
    if (used[j]) continue;
    if (e->real[j] && !S->sys) {                        // a real root the search didn't find: check it
      double l, r;
      if (!calc_sides(io->doc_tok, &io->calc->v, e->x[j], &l, &r) ||
          m_fabs(l - r) > 1e-9 * (m_fabs(l) + m_fabs(r))) continue;
    }
    int i = S->nroots++;
    if (e->real[j] && !S->sys)                          // keep real roots in order
      while (i > 0 && !S->rcplx[i - 1] && S->roots[i - 1] > e->x[j]) {
        S->roots[i] = S->roots[i - 1]; S->root_y[i] = S->root_y[i - 1];
        S->rx[i] = S->rx[i - 1]; S->rcplx[i] = S->rcplx[i - 1]; i--;
      }
    S->roots[i] = e->x[j]; S->root_y[i] = e->y[j];
    S->rx[i] = (int8_t)j; S->rcplx[i] = !e->real[j];
  }
}

// No roots because it holds everywhere: one equation the same on both sides
// (2x = 2x, x/x = 1), or two equations that are one curve (y = x, 2y = 2x).
static void always(SheetIO *io) {
  SolState *S = io->s;
  const Calc *c = io->calc;
  S->always = 0;
  int def = 0, same = 0;
  double k = 0;
  for (int t = 0; t < 13; t++) {
    CalcVars w = c->v;
    double x = S->gx0 + (S->gx1 - S->gx0) * (0.0371 + 0.0773 * t), l1, r1, l2 = 0, r2 = 0;
    w.y = S->gy0 + (S->gy1 - S->gy0) * (0.9137 - 0.0697 * t);
    if (!calc_sides(S->sys ? &c->eq1 : io->doc_tok, &w, x, &l1, &r1)) continue;
    if (S->sys && !calc_sides(&c->expr, &w, x, &l2, &r2)) continue;
    if (!(m_fabs(l1) < 1e300 && m_fabs(r1) < 1e300 && m_fabs(l2) < 1e300 && m_fabs(r2) < 1e300)) continue;
    def++;
    double g1 = l1 - r1, g2 = l2 - r2;
    if (!S->sys) { if (m_fabs(g1) <= 1e-12 * (m_fabs(l1) + m_fabs(r1)) + 1e-300) same++; continue; }
    if (g1 == 0 && g2 == 0) { same++; continue; }
    if (g1 == 0 || g2 == 0) continue;
    if (k == 0) k = g2 / g1;                      // the second a fixed multiple of the first
    if (m_fabs(g2 - k * g1) <= 1e-12 * (m_fabs(l2) + m_fabs(r2) + m_fabs(k) * (m_fabs(l1) + m_fabs(r1)))) same++;
  }
  if (def < 6 || same != def) return;
  if (S->sys) { S->always = 3; S->nroots = 0; return; }
  double l, r;                                    // (x/x = 1 has a hole at 0)
  bool at0 = calc_sides(io->doc_tok, &c->v, 0, &l, &r) && m_fabs(l) < 1e300 && m_fabs(r) < 1e300;
  S->always = def == 13 && at0 ? 1 : 2;
}

int sh_find(SheetIO *io) {
  Calc *c = io->calc;
  SolState *S = io->s;
  if (S->sys)
    S->nroots = calc_solve2(&c->eq1, &c->expr, &c->v, S->gx0, S->gx1, S->gy0, S->gy1, S->roots, S->root_y, MAX_ROOTS);
  else
    S->nroots = calc_roots(io->doc_tok, &c->v, S->gx0, S->gx1, S->roots, MAX_ROOTS);
  merge_exact(io);                                 // (may find the exact form false)
  S->always = 0;
  if (S->sys || !S->nroots) always(io);           // (2x = 2x; y = x twice)
  // Nothing on first look: widen the search a few times (sin x = 0.5 in degrees).
  if (S->nroots == 0 && S->expand > 0 && !S->always) return 1;
  // highlight the answer nearest the middle of the search box
  double cx = (S->gx0 + S->gx1) / 2, cy = (S->gy0 + S->gy1) / 2, best = 1e300;
  S->sel = 0;
  for (int i = 0; i < S->nroots; i++) {
    if (S->rcplx[i]) continue;
    double d = m_fabs(S->roots[i] - cx);
    if (S->sys) d += m_fabs(S->root_y[i] - cy);
#if SOLVER_GRAPH
    else {                                         // where the root sits on the graph
      double l, r;
      S->root_y[i] = calc_sides(io->doc_tok, &c->v, S->roots[i], &l, &r) ? l : 0;
    }
#endif
    if (d < best) { best = d; S->sel = i; }
  }
  int rows = io->rows;
  S->list_top = S->sel > 1 ? S->sel - 1 : 0;
  if (S->nroots >= rows && S->list_top > S->nroots - rows) S->list_top = S->nroots - rows;
#if SOLVER_GRAPH
  if (S->fit && S->nroots && !S->sys) {
    // Centre the y range on the crossings. Use a 1:1 aspect when that's
    // tighter than the auto range (x^2 = 2 then shows its U, not a flat line).
    double ylo = 1e300, yhi = -1e300;
    for (int i = 0; i < S->nroots; i++) {
      double yv = S->has_eq ? S->root_y[i] : 0;
      if (yv < ylo) ylo = yv;
      if (yv > yhi) yhi = yv;
    }
    double span = (S->gx1 - S->gx0) * io->aspect, autos = S->gy1 - S->gy0;
    if (autos < span) span = autos;
    if (span < (yhi - ylo) * 1.4) span = (yhi - ylo) * 1.4;
    double m = (ylo + yhi) / 2;
    if (span > 0) { S->gy0 = m - span / 2; S->gy1 = m + span / 2; }
  }
#endif
  S->fit = false;
  return 0;
}

// One part of a complex answer, exactly: a whole number, or a fraction / surd / pi multiple.
static bool exact_part(double v, TokBuf *t) {
  t->n = 0;
  if (v == m_floor(v) && m_fabs(v) < 1e12) {
    char b[24];
    calc_format(v, 12, b, sizeof b);
    for (int i = 0; b[i]; i++) t->t[t->n++] = (uint8_t)b[i];
    return true;
  }
  return exact_value(v, t);
}

// The answer's exact form for the S<>D key: the root of a solved equation
// (from its exact solution) or a plain answer as a fraction, surd or pi multiple.
bool sh_exact(SheetIO *io, TokBuf *out) {
  Calc *c = io->calc;
  out->n = 0;
  if (!EXACT_FORMS || c->mode != MODE_RESULT || c->sym || c->op) return false;
  if (c->solved && !c->stored && !c->sys) {
    if (!exact_solve(&c->expr, &c->v, io->exact)) return false;
    for (int j = 0; j < io->exact->n; j++) {
      double d = io->exact->x[j] - c->result, tol = 1e-7 * (1 + m_fabs(c->result));
      if (io->exact->real[j] && d < tol && -d < tol) {
        const TokBuf *s = &io->exact->sol[j];               // "x=..." : drop the "x="
        int k = (s->n > 2 && s->t[1] == '=') ? 2 : 0;
        for (int i = k; i < s->n; i++) out->t[out->n++] = s->t[i];
        return out->n > 0;
      }
    }
    return false;
  }
  if (c->result_im != 0) {                                  // a + bi: both parts exact, or no exact form
    TokBuf a;
    if (c->result != 0) {
      if (!exact_part(c->result, &a)) return false;
      for (int i = 0; i < a.n; i++) out->t[out->n++] = a.t[i];
    }
    double im = c->result_im;
    if (im < 0 || c->result != 0) out->t[out->n++] = im < 0 ? '-' : '+';
    im = m_fabs(im);
    if (im != 1) {
      if (!exact_part(im, &a) || out->n + a.n + 3 > CALC_MAX_TOK) return false;
      bool br = false;                                      // (1+sqrt(3))/2 i needs no brackets; a sum would
      for (int i = 0; i < a.n; i++) if (a.t[i] == '+' || (a.t[i] == '-' && i > 0)) br = true;
      if (br) out->t[out->n++] = '(';
      for (int i = 0; i < a.n; i++) out->t[out->n++] = a.t[i];
      if (br) out->t[out->n++] = ')';
    }
    out->t[out->n++] = T_I;
    return true;
  }
  return exact_value(c->result, out);
}
