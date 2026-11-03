// calcx.c - the heavier solvers: every root of an equation in an interval
// (sampling, then bracketing and Newton steps), two equations in x and y (2-D
// Newton from a grid of starts), definite integrals (adaptive Gauss-Kronrod)
// and derivatives (Ridders). On the watch these are built into the "math"
// overlay (loaded onto the heap when used), not the app itself; on a PC they're
// linked directly. Plain C.
#include "../c/calc.h"
#include "../c/mmath.h"
#include <stddef.h>
#include <stdlib.h>

// ---------------------------------------------------------------- solver

typedef struct {
  const TokBuf *b;
  CalcVars v;
  double scale;   // largest |lhs|+|rhs| seen: sets the residual tolerance
  int evals, max_evals;
  // Two equations, one of them linear in the other variable (y = sin x):
  // that one gives it as a function of t (= x, or y if 'swap'), and f is the
  // other equation there. NULL: plain f(x) = 0 from b.
  const TokBuf *lin;
  bool swap;
} Sol;

static bool eval2(const TokBuf *e, CalcVars *v, double x, double y, double *f, double *sc) {
  double l, r;
  v->x = x; v->y = y;
  if (calc_eval_eq(e, v, &l, &r) != CALC_OK) return false;
  *f = l - r;
  if (sc) *sc = m_fabs(l) + m_fabs(r);
  return m_isfinite(*f);
}
// the other variable from the linear equation: u with A + B u = 0
static bool other(Sol *s, double t, double *u) {
  double f0, f1_;
  if (!eval2(s->lin, &s->v, s->swap ? 0 : t, s->swap ? t : 0, &f0, NULL) ||
      !eval2(s->lin, &s->v, s->swap ? 1 : t, s->swap ? t : 1, &f1_, NULL)) return false;
  double B = f1_ - f0;
  if (B == 0) return false;
  *u = -f0 / B;
  return m_isfinite(*u);
}

static bool f_at(Sol *s, double x, double *f) {
  double l, r;
  if (s->evals++ > s->max_evals) return false;
  if (s->lin) {
    double u, sc;
    if (!other(s, x, &u) || !eval2(s->b, &s->v, s->swap ? u : x, s->swap ? x : u, f, &sc)) return false;
    if (sc > s->scale) s->scale = sc;
    return true;
  }
  s->v.x = x;
  if (calc_eval_eq(s->b, &s->v, &l, &r) != CALC_OK) return false;
  double sc = m_fabs(l) + m_fabs(r);
  if (sc > s->scale) s->scale = sc;
  *f = l - r;
  return m_isfinite(*f);
}

static bool opp(double a, double b) { return (a < 0 && b > 0) || (a > 0 && b < 0); }

// Illinois method (regula falsi that halves the stale end's value) on a
// sign-change bracket [a, b]: superlinear, always bracketed, and small.
static bool brent(Sol *s, double a, double b, double fa, double fb, double *root) {
  int side = 0, same = 0;
  double c = a;
  for (int it = 0; it < 300; it++) {
    c = (a * fb - b * fa) / (fb - fa);
    double lo = a < b ? a : b, hi = a < b ? b : a, fc;
    // halve outright when the same end keeps moving (x^3 = 1E-60 from 0:
    // the secant creeps up from 1E-59 and would never get there)
    if (!(c > lo && c < hi) || same >= 2) {
      c = 0.5 * (a + b); same = 0;
      // a bracket over many decades on one side of 0 (0 .. 1/6 round
      // x^2 = 1E-50): halve it in the logarithm, or it takes hundreds of steps
      if (lo >= 0 || hi <= 0) {
        double p = lo >= 0 ? lo : -hi, q = lo >= 0 ? hi : -lo;   // 0 <= p < q
        if (p < 1e-3 * q) {
          double g = m_sqrt((p > 1e-300 ? p : 1e-300) * q);
          if (g > p && g < q) c = lo >= 0 ? g : -g;
        }
      }
    }
    if (c <= lo || c >= hi) break;                     // bracket can't shrink further
    if (!f_at(s, c, &fc)) return false;
    if (fc == 0) break;
    int was = side;
    if (opp(fc, fa)) { b = c; fb = fc; if (side == -1) fa *= 0.5; side = -1; }
    else             { a = c; fa = fc; if (side == 1) fb *= 0.5; side = 1; }
    same = side == was ? same + 1 : 0;
    if (m_fabs(b - a) <= 4e-16 * m_fabs(c)) break;
  }
  *root = c;
  return true;
}

// Damped Newton with a numeric derivative. Returns true only if it converged
// (exact zero, step tiny relative to x, or step tiny on the guess's scale).
static bool newton(Sol *s, double x, double f, double gscale, double *root) {
  double dx = 1e300;
  for (int it = 0; it < 200; it++) {
    if (f == 0) { *root = x; return true; }
    double ax = m_fabs(x);
    double h0 = 1e-7 * (ax > 1e-7 ? ax : 1e-7);
    // the step must stay below the distance to the root, or a double root
    // (sin(x)^2 at pi) stalls at about 1e-9 off
    if (h0 > 0.1 * dx && dx > 1e-15 * ax) h0 = 0.1 * dx;
    double xn = x, fn = f;
    bool ok = false;
    // Forward difference first; if that step can't improve (a kink, like
    // abs(x-3) near 3), try the backward difference.
    for (int side = 0; side < 2 && !ok; side++) {
      double h = side ? -h0 : h0, f1;
      if (!f_at(s, x + h, &f1)) continue;
      double d = (f1 - f) / h;
      if (d == 0 || !m_isfinite(d)) continue;
      double step = f / d, lam = 1.0;
      for (int bt = 0; bt < 30; bt++) {
        xn = x - lam * step;
        if (f_at(s, xn, &fn)) {
          if (opp(f, fn)) return brent(s, x, xn, f, fn, root);
          if (m_fabs(fn) < m_fabs(f)) { ok = true; break; }
        }
        lam *= 0.5;
      }
    }
    if (!ok) { *root = x; return true; }   // can't improve: let accept() judge
    dx = m_fabs(xn - x);
    x = xn; f = fn;
    if (dx <= 4e-16 * m_fabs(x)) break;
  }
  *root = x;
  return f == 0 || dx <= 4e-16 * m_fabs(x) || dx <= 1e-12 * gscale;
}

// Residual test. The tolerance is set by the size of the equation's two
// sides at the root and at the guess, so a pole (1/x) is not taken as a root.
static bool accept(Sol *s, double x, double gsides) {
  double f;
  (void)gsides;
  s->scale = 0;
  if (!f_at(s, x, &f)) return false;
  double sc = s->scale, xs = s->scale;
  // the size f has just beside x too: where the sides cancel to ~0 at the
  // root (sin x - x/2) their own size says nothing. "Just beside" is
  // relative to x, so x^4 = 1E-60 is judged at its own tiny scale, not by
  // the size f has across the whole search (that made 1E-22 a "root").
  double dd = 1e-3 * m_fabs(x), fl, fr;
  if (dd > 0) {
    if (f_at(s, x + dd, &fr) && m_fabs(fr) > sc) sc = m_fabs(fr);
    if (f_at(s, x - dd, &fl) && m_fabs(fl) > sc) sc = m_fabs(fl);
  }
  s->scale = xs;
  if (sc < 1e-300) sc = 1e-300;
  double tol = 1e-10 * sc;
  if (m_fabs(f) > tol) return false;
  // A flat run of zeros (exp(x) underflowing far out) is not a root; a flat
  // touching root ((x-0.3)^4) still rises well above its own size nearby.
  double tz = 4e-16 * s->scale;                        // (rounding level: x + 1E7 = 1E7 + 0.5 does move)
  double d = 1e-3 * (m_fabs(x) > 1 ? m_fabs(x) : 1), fa, fb;
  bool za = f_at(s, x + d, &fa) && m_fabs(fa) <= tz;
  bool zb = f_at(s, x - d, &fb) && m_fabs(fb) <= tz;
  return !(za && zb);
}

// Snap 1.9999999999999998 to 2 and 1e-20 to 0 when that fits at least as well.
static double polish(Sol *s, double x) {
  double fx;
  if (!f_at(s, x, &fx)) return x;
  fx = m_fabs(fx);
  double cands[3] = { 0.0, calc_round_sig(x, 12), calc_round_sig(x, 10) };
  for (int i = 0; i < 3; i++) {
    double c = cands[i], fc;
    if (c == x) continue;
    if (i == 0 && m_fabs(x) > 1e-9) continue;
    if (f_at(s, c, &fc) && m_fabs(fc) <= fx) { x = c; fx = m_fabs(fc); }
  }
  return x;
}

// ---------------------------------------------------------------- all roots

#define ROOT_SAMPLES 120
#define ROOT_BUDGET  8000    // total evaluations, keeps the watch responsive

// Two roots closer than tol are one, unless f changes sign between them
// ((x-100)(x-100.0000001), x^2 = 1E-16): then both stay.
static bool apart(Sol *s, double a, double b) {
  double lo = a < b ? a : b, w = m_fabs(a - b), fm, fo;
  if (w <= 1e-12 * (m_fabs(a) > m_fabs(b) ? m_fabs(a) : m_fabs(b)) ||   // the same root, found twice
      w == 0 || !f_at(s, lo + w / 2, &fm) || !f_at(s, lo - w, &fo)) return false;
  return opp(fm, fo);
}
static void add_root(Sol *s, double *out, int *n, int max, double r, double tol) {
  for (int i = 0; i < *n; i++) if (m_fabs(out[i] - r) <= tol && !apart(s, out[i], r)) return;
  // full: keep the lowest ones, so the list has no gaps (sin x = 0 on
  // 0..1000: 0, 3.14, 6.28 ..., not whichever were found first)
  if (*n >= max) { if (max <= 0 || r >= out[max - 1]) return; (*n)--; }
  int i = *n;
  while (i > 0 && out[i - 1] > r) { out[i] = out[i - 1]; i--; }   // keep sorted
  out[i] = r;
  (*n)++;
}

// A touching root is only located to about sqrt(precision) by f itself
// (cos(x)=1 near 2pi: f is -d^2/2). It is the turning point of f, where the
// slope changes sign, and the slope can be found much more closely.
static bool slope(Sol *s, double x, double h, double *g) {
  double a, b;
  if (!f_at(s, x + h, &a) || !f_at(s, x - h, &b)) return false;
  *g = a - b;
  return true;
}
static double vertex(Sol *s, double x, double gs) {
  double w = 1e-4 * gs, h = 1e-5 * gs, lo = x - w, hi = x + w, gl, gh, gm;
  if (!slope(s, lo, h, &gl) || !slope(s, hi, h, &gh) || !opp(gl, gh)) return x;
  for (int it = 0; it < 45; it++) {
    double m = 0.5 * (lo + hi);
    if (m <= lo || m >= hi || !slope(s, m, h, &gm)) break;
    if (gm == 0) { lo = hi = m; break; }               // (still judged below: sin is flat at its peak)
    if (opp(gm, gl)) { hi = m; } else { lo = m; gl = gm; }
  }
  double m = 0.5 * (lo + hi), fm, fx;
  // keep it only if it fits at least about as well
  if (f_at(s, m, &fm) && f_at(s, x, &fx) && m_fabs(fm) <= 4 * m_fabs(fx) + 1e-300) return m;
  return x;
}

// r is a simple root between two samples of the same sign, so there is
// another one on the side where f's sign differs from that sample's.
static void partner(Sol *s, double r, double xl, double fl0, double xr, double fr0,
                    double med, double *out, int *n, int max) {
  double gs = m_fabs(r) > 1 ? m_fabs(r) : 1, d = 1e-9 * gs, fl, fh, y;
  if (!(f_at(s, r - d, &fl) && f_at(s, r + d, &fh) && opp(fl, fh))) return;
  bool right = opp(fh, fr0);
  double a = right ? r + d : xl, b = right ? xr : r - d;
  double fa = right ? fh : fl0, fb = right ? fr0 : fl;
  if (brent(s, a, b, fa, fb, &y) && accept(s, y, med * 1e-3))
    add_root(s, out, n, max, polish(s, y), 1e-12 * gs);
}

// Between a and b f stops being defined (on the side 'bad_b': b). Narrow
// down to the edge; a root right there (sqrt(2 - x^2) at sqrt 2) counts.
static void edge(Sol *s, double a, double b, bool bad_b, double med, double *out, int *n, int max, double tol) {
  double good = bad_b ? a : b, bad = bad_b ? b : a, f, fg = 0;
  bool adj = false;                                   // narrowed down to neighbouring doubles
  for (int it = 0; it < 60; it++) {
    double m = 0.5 * (good + bad);
    if (m == good || m == bad) { adj = true; break; }
    if (f_at(s, m, &f)) { good = m; fg = f; } else bad = m;
  }
  double fo;
  if (!f_at(s, bad_b ? a : b, &fo)) return;
  s->scale = 0;
  if (!f_at(s, good, &fg)) return;
  // the edge is a root if f heads to 0 there: a square root at its edge gets
  // down to about 1e-8 of its size a sample away (sqrt(x) + 1E-6 doesn't)
  // or f changes sign between the edge and the first good sample (ln x =
  // -2 at 0.135: the edge at 0 is no root, but the crossing is right beside it)
  double y;
  if (opp(fg, fo) && brent(s, bad_b ? a : good, bad_b ? good : b, bad_b ? fo : fg, bad_b ? fg : fo, &y) &&
      accept(s, y, med * 1e-3))
    add_root(s, out, n, max, polish(s, y), tol);
  double fi;   // (not f underflowing to 0 all along: e^(1/(x-1)) next to 1)
  if (adj && m_fabs(fg) <= 1e-7 * m_fabs(fo) &&
      !(fg == 0 && f_at(s, good + (bad_b ? -1 : 1) * 1e-6 * (b - a), &fi) && fi == 0)) {       // (not x ln x near 0: 1.4E-19 is no edge root)
    double r = calc_round_sig(good, 12);               // 0.1 rather than 0.10000000000000002
    double fr;
    if (!(f_at(s, r, &fr) && m_fabs(fr) <= m_fabs(fg) * 10 + 1e-300)) r = good;
    add_root(s, out, n, max, r, tol);
  }
}

static int roots_core(Sol *sp, double lo, double hi, double *out, int max);
int calc_roots(const TokBuf *b, const CalcVars *v, double lo, double hi, double *out, int max) {
  if (!calc_has_x(b) || !(hi > lo)) return 0;
  double l, r;
  if (calc_eval_eq(b, v, &l, &r) == CALC_ERR_SYNTAX) return 0;
  Sol s = { b, *v, 0.0, 0, ROOT_BUDGET, NULL, false };
  return roots_core(&s, lo, hi, out, max);
}

// f is exactly 0 over a stretch starting between lo (f = flo, not 0) and hi
// (f = 0) after a jump (floor(x + 0.3) = 3 on 2.7 .. 3.7): the start of the
// stretch is a root, found by halving. A real jump keeps its size right up to
// the edge; f only decaying until it rounds to 0 (e^-x = 0 near 745) fades
// out there: no root. The root is a value that is a solution as shown, to 10
// digits (floor(x^2) = 0 starts just after -1: -0.9999999999, not -1).
static void stretch(Sol *s, double lo_, double hi_, double flo, double *out, int *n, int max, double tol) {
  double fm_;
  for (int it = 0; it < 80; it++) {
    double m = 0.5 * (lo_ + hi_);
    if (m <= lo_ || m >= hi_) break;
    if (f_at(s, m, &fm_) && fm_ == 0) hi_ = m; else lo_ = m;
  }
  double fl_;
  if (!(f_at(s, lo_, &fl_) && m_fabs(fl_) >= 1e-3 * m_fabs(flo) && m_fabs(fl_) > 1e-250)) return;
  double r = calc_round_sig(hi_, 10), fr_;
  if (!(f_at(s, r, &fr_) && fr_ == 0)) {
    double u = m_pow10i((int)m_floor(m_log10(m_fabs(hi_) > 1e-300 ? m_fabs(hi_) : 1e-300)) - 9);
    r = calc_round_sig(hi_ + u, 10);
    if (!(r > hi_ && f_at(s, r, &fr_) && fr_ == 0)) return;
  }
  add_root(s, out, n, max, r, tol);
}

static int roots_core(Sol *sp, double lo, double hi, double *out, int max) {

  // on the heap while searching: not on the small stack, not in the app's static space
  uint8_t *mem = malloc((ROOT_SAMPLES + 1) * (sizeof(double) + 2 * sizeof(float) + sizeof(bool)));
  if (!mem) return 0;
  double *fs = (double *)mem;
  float *sc = (float *)(fs + ROOT_SAMPLES + 1), *tmp = sc + ROOT_SAMPLES + 1;
  bool *ok = (bool *)(tmp + ROOT_SAMPLES + 1);
#define XS(i) (lo + (hi - lo) * (i) / ROOT_SAMPLES)
  Sol s = *sp;
  for (int i = 0; i <= ROOT_SAMPLES; i++) {
    s.scale = 0;
    ok[i] = f_at(&s, XS(i), &fs[i]);
    sc[i] = ok[i] ? (float)s.scale : 0.0f;
  }
  // Typical size of the two sides: median, so poles and zeros don't skew it.
  double med = 0;
  {
    int m = 0;
    for (int i = 0; i <= ROOT_SAMPLES; i++) if (ok[i]) {
      float t = sc[i];
      int j = m++;
      while (j > 0 && tmp[j - 1] > t) { tmp[j] = tmp[j - 1]; j--; }
      tmp[j] = t;
    }
    if (m) med = tmp[m / 2];
  }
  double fmed = med;   // scale for "near zero" when looking for touching roots
  double tol = 1e-9 * (hi - lo);
  int n = 0;
  for (int i = 0; i <= ROOT_SAMPLES && s.evals < ROOT_BUDGET; i++) {
    if (!ok[i]) {
      // the edge of where f is defined, between this sample and the next:
      // f can reach 0 right there (sqrt(x - 0.1) at 0.1)
      if (i < ROOT_SAMPLES && ok[i + 1]) edge(&s, XS(i), XS(i + 1), false, med, out, &n, max, tol);
      continue;
    }
    if (i < ROOT_SAMPLES && !ok[i + 1]) edge(&s, XS(i), XS(i + 1), true, med, out, &n, max, tol);
    double x;
    bool zero = fs[i] == 0;
    if (zero && i > 0 && ok[i - 1] && fs[i - 1] != 0 && m_fabs(fs[i - 1]) > 1e-10 * med &&
        !accept(&s, XS(i), 0))
      stretch(&s, XS(i - 1), XS(i), fs[i - 1], out, &n, max, tol);   // f 0 over a stretch after a jump
    if (zero && accept(&s, XS(i), 0)) {
      add_root(&s, out, &n, max, XS(i), tol);
      if (i > 0 && i < ROOT_SAMPLES && ok[i - 1] && ok[i + 1] && !opp(fs[i - 1], fs[i + 1]))
        partner(&s, XS(i), XS(i - 1), fs[i - 1], XS(i + 1), fs[i + 1], med, out, &n, max);   // its twin close by
    }
    // sign change to the next sample
    if (!zero && i < ROOT_SAMPLES && ok[i + 1] && opp(fs[i], fs[i + 1])) {
      if (brent(&s, XS(i), XS(i + 1), fs[i], fs[i + 1], &x)) {
        double fx_;
        if (accept(&s, x, med * 1e-3)) add_root(&s, out, &n, max, polish(&s, x), tol);
        // a zero stretch narrower than the samples, between two jumps
        // (floor(x^2) = 50): brent lands in it
        else if (f_at(&s, x, &fx_) && fx_ == 0 && m_fabs(fs[i]) > 1e-10 * med)
          stretch(&s, XS(i), x, fs[i], out, &n, max, tol);
      }
    }
    // a pole between two samples of the same sign, with a root right beside
    // it (1/(x - 1.2) = 1000, tan x = 1000): f steps against its trend on
    // both sides. Bisect to the jump, then bracket against each branch's end.
    // (also in the first or last gap, with the trend from the one side there)
    bool hl = i > 0 && ok[i - 1], hr = i + 2 <= ROOT_SAMPLES && ok[i + 2];
    if (!zero && i + 1 <= ROOT_SAMPLES && ok[i + 1] && (hl || hr) && fs[i + 1] != 0 &&
        !opp(fs[i], fs[i + 1]) && s.evals < ROOT_BUDGET) {
      double t0 = hl ? fs[i] - fs[i - 1] : 0, t1 = fs[i + 1] - fs[i], t2 = hr ? fs[i + 2] - fs[i + 1] : 0;
      double tr = hl ? t0 : t2;                         // the trend of f either side
      if (tr != 0 && (!hl || !hr || (t2 != 0 && !opp(t0, t2))) && opp(tr, t1)) {
        double a = XS(i), b = XS(i + 1), fa = fs[i], fb = fs[i + 1], fm;
        double big = m_fabs(fs[i]) > m_fabs(fs[i + 1]) ? m_fabs(fs[i]) : m_fabs(fs[i + 1]);
        bool pole = false;
        for (int it = 0; it < 64; it++) {
          double m = 0.5 * (a + b);
          if (m == a || m == b) break;
          if (!f_at(&s, m, &fm)) {                     // right on the pole: step just off it
            pole = true;
            m = 0.5 * (a + b) - 1e-9 * (b - a);
            if (m <= a || !f_at(&s, m, &fm)) break;
          }
          if (m_fabs(fm) > 4 * big) pole = true;
          if (it == 12 && !pole) break;               // not growing: a wiggle, not a pole
          if (!opp(fm - fs[i], tr) && fm != fs[i]) { a = m; fa = fm; } else { b = m; fb = fm; }
        }
        if (pole)
        {
          if (opp(fa, fs[i]) && brent(&s, XS(i), a, fs[i], fa, &x) && accept(&s, x, med * 1e-3))
            add_root(&s, out, &n, max, polish(&s, x), tol);
          if (opp(fb, fs[i + 1]) && brent(&s, b, XS(i + 1), fb, fs[i + 1], &x) && accept(&s, x, med * 1e-3))
            add_root(&s, out, &n, max, polish(&s, x), tol);
        }
      }
    }
    // A sample at 0, or a dip of |f| (with or without a sign change beside
    // it): look closer, 16 steps across the two intervals, for roots the
    // samples step over: two close together (sin(x^2) far out), one next to
    // the zero (x^2(e^x - 1.1) at 0 and 0.095), or one touching (x^2 = 0,
    // (x - 0.1)^2 beside a root at 0.2).
    if (!(i > 0 && i < ROOT_SAMPLES && ok[i - 1] && ok[i + 1])) continue;
    if (!zero && !(m_fabs(fs[i]) <= m_fabs(fs[i - 1]) && m_fabs(fs[i]) <= m_fabs(fs[i + 1]))) continue;
    double sx[17], sf[17];
    bool sok[17];
    for (int k = 0; k <= 16; k++) {
      sx[k] = XS(i - 1) + (XS(i + 1) - XS(i - 1)) * k / 16;
      sok[k] = k == 0 ? true : k == 16 ? true : (s.evals < ROOT_BUDGET && f_at(&s, sx[k], &sf[k]));
      if (k == 0) sf[k] = fs[i - 1];
      if (k == 16) sf[k] = fs[i + 1];
    }
    for (int k = 0; k < 16; k++)
      if (sok[k] && sok[k + 1] && sf[k] != 0 && sf[k + 1] != 0 && opp(sf[k], sf[k + 1]) &&
          brent(&s, sx[k], sx[k + 1], sf[k], sf[k + 1], &x) && accept(&s, x, med * 1e-3))
        add_root(&s, out, &n, max, polish(&s, x), tol);
    for (int k = 0; k <= 16; k++)                       // a closer look that lands right on a root (9x^3-12x^2+4x at 2/3)
      if (sok[k] && sf[k] == 0 && k > 0 && k < 16 && sok[k - 1] && sok[k + 1] && sf[k - 1] != 0 && sf[k + 1] != 0 &&
          accept(&s, sx[k], med * 1e-3))                // (an isolated 0, not e^x underflowing)
        add_root(&s, out, &n, max, sx[k], tol);
    // touching roots: where |f| comes down to a low point without changing sign
    for (int k = 1; k < 16; k++) {
      if (!(sok[k - 1] && sok[k] && sok[k + 1]) || sf[k] == 0) continue;
      if (!(m_fabs(sf[k]) <= m_fabs(sf[k - 1]) && m_fabs(sf[k]) <= m_fabs(sf[k + 1]))) continue;
      if (opp(sf[k - 1], sf[k]) || opp(sf[k], sf[k + 1])) continue;
      if (!(m_fabs(sf[k]) < 0.5 * (fmed > 1e-300 ? fmed : 1e-300))) continue;
      double gs = m_fabs(sx[k]) > 1 ? m_fabs(sx[k]) : 1;
      double loc = sc[i - 1] > sc[i + 1] ? sc[i - 1] : sc[i + 1], fr;
      x = sx[k];
      newton(&s, sx[k], sf[k], gs, &x);   // a double root converges slowly: judged below
      if (x >= sx[k - 1] && x <= sx[k + 1] && accept(&s, x, med * 1e-3) && f_at(&s, x, &fr)) {
        // A touching root brings |f| right down; x^2 = -1e-12 only gets to 1e-12.
        double d = 1e-9 * gs, fl, fh;
        bool cross = f_at(&s, x - d, &fl) && f_at(&s, x + d, &fh) && opp(fl, fh);   // a real crossing at x
        if (fr != 0 && m_fabs(fr) > 1e-14 * loc) {
          // two roots closer than Newton can see? f changes sign beside x
          if (!(f_at(&s, x - d, &fl) && f_at(&s, x + d, &fh) && (opp(fl, sf[k]) || opp(fh, sf[k])))) continue;
        }
        // the turning point only for a touching root; Newton's own root
        // stands when it is one (sin x = 0.99999999: two roots, not the peak)
        // a touching root is a low point of |f|; f only shrinking toward a pole
        // (e^(1/(x-1.3)) = 0 near 1.3) is not one
        if (fr != 0 && !cross) {
          double w = 1e-6 * gs, ql, qh;
          if (!(f_at(&s, x - w, &ql) && f_at(&s, x + w, &qh) && m_fabs(ql) >= m_fabs(fr) && m_fabs(qh) >= m_fabs(fr))) continue;
        }
        double r = polish(&s, (fr == 0 || cross) ? x : vertex(&s, x, gs));
        add_root(&s, out, &n, max, r, tol);
        partner(&s, r, sx[k - 1], sf[k - 1], sx[k + 1], sf[k + 1], med, out, &n, max);
      }
    }
  }
  free(mem);
  return n;
#undef XS
}

// ---------------------------------------------------------------- systems

typedef struct {
  const TokBuf *e[2];
  CalcVars v;
  int calls, max_calls;
} Sys;

// f[k] = lhs - rhs of equation k at (x, y); sc[k] = |lhs| + |rhs|.
static bool F2(Sys *s, double x, double y, double f[2], double sc[2]) {
  if (s->calls++ > s->max_calls) return false;
  s->v.x = x; s->v.y = y;
  for (int k = 0; k < 2; k++) {
    double l, r;
    if (calc_eval_eq(s->e[k], &s->v, &l, &r) != CALC_OK) return false;
    f[k] = l - r;
    sc[k] = m_fabs(l) + m_fabs(r);
    if (!m_isfinite(f[k])) return false;
  }
  return true;
}

static double nrm(const double f[2]) { return f[0] * f[0] + f[1] * f[1]; }

// Damped 2-D Newton with a numeric Jacobian. True if it converged.
static bool newton2(Sys *s, double *px, double *py) {
  double x = *px, y = *py, f[2], sc[2];
  if (!F2(s, x, y, f, sc)) return false;
  for (int it = 0; it < 60; it++) {
    if (f[0] == 0 && f[1] == 0) break;
    double ax = m_fabs(x), ay = m_fabs(y);
    double hx = 1e-7 * (ax > 1e-7 ? ax : 1e-7), hy = 1e-7 * (ay > 1e-7 ? ay : 1e-7);
    double fx[2], fy[2], t[2];
    if (!F2(s, x + hx, y, fx, t)) { hx = -hx; if (!F2(s, x + hx, y, fx, t)) return false; }
    if (!F2(s, x, y + hy, fy, t)) { hy = -hy; if (!F2(s, x, y + hy, fy, t)) return false; }
    double a = (fx[0] - f[0]) / hx, b = (fy[0] - f[0]) / hy;
    double c = (fx[1] - f[1]) / hx, d = (fy[1] - f[1]) / hy;
    double det = a * d - b * c;
    if (det == 0 || !m_isfinite(det)) return false;
    double dx = (d * f[0] - b * f[1]) / det, dy = (a * f[1] - c * f[0]) / det;
    double lam = 1, n0 = nrm(f), xn = x, yn = y, fn[2], scn[2];
    bool ok = false;
    for (int bt = 0; bt < 25; bt++) {
      xn = x - lam * dx; yn = y - lam * dy;
      if (F2(s, xn, yn, fn, scn) && nrm(fn) < n0) { ok = true; break; }
      lam *= 0.5;
    }
    if (!ok) break;                       // can't improve: let the residual test decide
    double step = m_fabs(xn - x) + m_fabs(yn - y);
    x = xn; y = yn; f[0] = fn[0]; f[1] = fn[1];
    if (step <= 4e-16 * (m_fabs(x) + m_fabs(y))) break;
  }
  *px = x; *py = y;
  return true;
}

// Residual test, each equation against the size of its own two sides.
static bool accept2(Sys *s, double x, double y, const double gsc[2], double *res) {
  double f[2], sc[2];
  if (!F2(s, x, y, f, sc)) return false;
  *res = nrm(f);
  for (int k = 0; k < 2; k++) {
    double m = sc[k] > gsc[k] ? sc[k] : gsc[k];
    if (m < 1e-300) m = 1e-300;
    if (m_fabs(f[k]) > 1e-9 * m) return false;
  }
  return true;
}

static bool lin2(const TokBuf *e1, const TokBuf *e2, const CalcVars *v, double *x, double *y);

// Is e linear in y (swap: in x), with the coefficient of y never 0 where
// checked? Then y comes straight from it: y = sin x, x + y^0... (y = 0.5).
static bool lin_in(const TokBuf *e, const CalcVars *v, bool swap, double t0, double t1, bool fixed) {
  CalcVars w = *v;
  int good = 0;
  double B0 = 0;
  for (int k = 0; k < 7; k++) {
    double t = t0 + (t1 - t0) * (k + 0.37) / 7, f0, f1_, f2, s0, s1, s2;
    // long steps in y (2^20), so y still shows beside a big constant
    // (x^2 + y = 1E10 + 4)
    const double Y = 1048576;
    if (!eval2(e, &w, swap ? 0 : t, swap ? t : 0, &f0, &s0) || !eval2(e, &w, swap ? Y : t, swap ? t : Y, &f1_, &s1) ||
        !eval2(e, &w, swap ? -2.5 * Y : t, swap ? t : -2.5 * Y, &f2, &s2)) continue;
    double B = f1_ - f0, sc = s0 + s1 + s2 + 1e-300;
    if (m_fabs(B) <= 1e-9 * sc) return false;          // no y here (x = 2), or not linear
    if (m_fabs(f2 - (f0 - 2.5 * B)) > 1e-9 * sc) return false;
    if (fixed && good && m_fabs(B - B0) > 1e-9 * (m_fabs(B) + m_fabs(B0))) return false;   // y's factor changes with x
    B0 = B;
    good++;
  }
  return good >= 3;
}

// Two equations, one linear in a variable: put it into the other and find
// every root of that, in one variable, so none are missed (y = sin x with
// y = 0: all seven, x^2 + y^2 = 1 with y = 1: the touching (0, 1)).
// -1: not this case.
static int elim2(const TokBuf *e1, const TokBuf *e2, const CalcVars *v,
                 double x0, double x1, double y0, double y1, double *xs, double *ys, int max) {
  const TokBuf *E[2] = { e1, e2 };
  // an equation where y (or x) has a fixed factor first: y = x, not x y = 1E-30
  for (int pass = 0; pass < 4; pass++) {
    int sw = pass & 1;
    bool fixed = pass < 2;
    for (int k = 0; k < 2; k++) {
      if (!lin_in(E[k], v, sw, sw ? y0 : x0, sw ? y1 : x1, fixed)) continue;
      Sol s = { E[1 - k], *v, 0.0, 0, ROOT_BUDGET, E[k], sw != 0 };
      double t[12];
      int m = roots_core(&s, sw ? y0 : x0, sw ? y1 : x1, t, 12), n = 0;
      for (int i = 0; i < m && n < max; i++) {
        double u, fa, fb;
        s.scale = 0;
        if (!other(&s, t[i], &u)) continue;
        double x = sw ? u : t[i], y = sw ? t[i] : u;
        // (the other variable may be outside the search box: y = 100 with
        // x^2 + y = 104 still has its answers x = -2 and 2)
        // the other equation holds there (the linear one does by
        // construction), against the size f has close by: at the point
        // itself both sides can be ~0 (sin x + sin y = 0), and then any
        // rounding would fail a test against them
        CalcVars w = *v;
        const TokBuf *eo = E[1 - k];
        double sa, fo, so;
        if (!eval2(eo, &w, x, y, &fa, &sa)) continue;
        double dx = 1e-2 * (x1 - x0), dy = 1e-2 * (y1 - y0);
        const double P[4][2] = { { dx, 0 }, { -dx, 0 }, { 0, dy }, { 0, -dy } };
        for (int q = 0; q < 4; q++)
          if (eval2(eo, &w, x + P[q][0], y + P[q][1], &fo, &so)) {
            if (so > sa) sa = so;
            if (m_fabs(fo) > sa) sa = m_fabs(fo);
          }
        (void)fb;
        if (m_fabs(fa) > 1e-8 * (sa + 1e-300) + 1e-300) continue;
        double rx = calc_round_sig(x, 12), ry = calc_round_sig(y, 12);   // 0.5 rather than 0.49999999999999994
        if (m_fabs(rx - x) <= 1e-12 * m_fabs(x)) x = rx;
        if (m_fabs(ry - y) <= 1e-12 * m_fabs(y)) y = ry;
        if (x == 0) x = 0.0;                           // (no -0; tiny answers stay: x y = 1E-30, y = x)
        if (y == 0) y = 0.0;
        int j = n++;
        while (j > 0 && (xs[j - 1] > x || (xs[j - 1] == x && ys[j - 1] > y))) { xs[j] = xs[j - 1]; ys[j] = ys[j - 1]; j--; }
        xs[j] = x; ys[j] = y;
      }
      return n;
    }
  }
  return -1;
}

int calc_solve2(const TokBuf *e1, const TokBuf *e2, const CalcVars *v,
                double x0, double x1, double y0, double y1, double *xs, double *ys, int max) {
  double l, r;
  if (calc_eval_eq(e1, v, &l, &r) == CALC_ERR_SYNTAX || calc_eval_eq(e2, v, &l, &r) == CALC_ERR_SYNTAX) return 0;
  if (max > 0 && lin2(e1, e2, v, &xs[0], &ys[0])) return 1;
  int ne = elim2(e1, e2, v, x0, x1, y0, y1, xs, ys, max);
  if (ne >= 0) return ne;
  Sys s = { { e1, e2 }, *v, 0, 6000 };
  double tx = 1e-7 * (x1 - x0), ty = 1e-7 * (y1 - y0);
  int n = 0;
  // Seeds: the stored (x, y) if it's in view, then a 6 x 4 grid over the view.
  for (int k = -1; k < 24 && s.calls < s.max_calls; k++) {
    double x, y;
    if (k < 0) {
      x = v->x; y = v->y;
      if (!(x >= x0 && x <= x1 && y >= y0 && y <= y1)) continue;
    } else {
      x = x0 + (x1 - x0) * (k % 6 + 0.5) / 6;
      y = y0 + (y1 - y0) * (k / 6 + 0.5) / 4;
    }
    double f[2], gsc[2], res;
    if (!F2(&s, x, y, f, gsc)) continue;
    if (!newton2(&s, &x, &y) || !accept2(&s, x, y, gsc, &res)) continue;
    if (x < x0 - tx || x > x1 + tx || y < y0 - ty || y > y1 + ty) continue;
    if (x == 0) x = 0.0;
    if (y == 0) y = 0.0;
    bool dup = false;
    for (int i = 0; i < n; i++)
      if (m_fabs(xs[i] - x) <= tx * 10 && m_fabs(ys[i] - y) <= ty * 10) { dup = true; break; }
    if (dup || n >= max) continue;
    int i = n++;
    while (i > 0 && (xs[i - 1] > x || (xs[i - 1] == x && ys[i - 1] > y))) {
      xs[i] = xs[i - 1]; ys[i] = ys[i - 1]; i--;
    }
    xs[i] = x; ys[i] = y;
  }
  return n;
}

// ---------------------------------------------------------------- calculus

static bool f1(const TokBuf *f, CalcVars *w, double x, double *out) {
  double l, r;
  w->x = x;
  if (calc_eval_eq(f, w, &l, &r) != CALC_OK) return false;
  *out = l;
  return m_isfinite(l);
}

// 15-point Kronrod rule with its embedded 7-point Gauss rule.
static const double XGK[8] = {
  0.991455371120812639206854697526329, 0.949107912342758524526189684047851,
  0.864864423359769072789712788640926, 0.741531185599394439863864773280788,
  0.586087235467691130294144845693013, 0.405845151377397166906606412076961,
  0.207784955007898467600689403773245, 0.0 };
static const double WGK[8] = {
  0.022935322010529224963732008058970, 0.063092092629978553290700663189204,
  0.104790010322250183839876322541518, 0.140653259715525918745189590510238,
  0.169004726639267902826583426598550, 0.190350578064785409913256402421014,
  0.204432940075298892414161999234649, 0.209482141084727828012999174891714 };
static const double WG[4] = {
  0.129484966168869693270611432679082, 0.279705391489276667901467771423780,
  0.381830050505118944950369775488975, 0.417959183673469387755102040816327 };

// f at a quadrature node; a node that lands on a removable hole (sin(x)/x
// at 0) takes the value just beside it.
static bool fn(const TokBuf *f, CalcVars *w, double x, double h, double *out) {
  return f1(f, w, x, out) || f1(f, w, x + 1e-9 * h, out);
}

// A piece far longer than its distance to the nearest anchor (an end of
// the range, or its middle), like 25 .. 2.5E11 next to 0 in 0 .. 1E99: its
// nodes go in evenly in the logarithm of the distance, or the area near the
// anchor (e^-x, 1/x^2) falls between them.
static double g_anc[3];
static bool logmap(double a, double b, double *c, double *sg, double *u0, double *u1) {
  double best = -1, dp = 0, dq = 0;
  for (int k = 0; k < 3; k++) {
    double t = g_anc[k], d0, d1, s;
    if (t <= a) { d0 = a - t; d1 = b - t; s = 1; }
    else if (t >= b) { d0 = t - b; d1 = t - a; s = -1; }
    else continue;
    if (best < 0 || d0 < best) { best = d0; dp = d0; dq = d1; *c = t; *sg = s; }
  }
  if (!(best > 0) || !(dq > 10 * dp)) return false;
  *u0 = m_log(dp); *u1 = m_log(dq);
  return true;
}
static double split_at(double a, double b) {
  double c, sg, u0, u1;
  if (logmap(a, b, &c, &sg, &u0, &u1)) {
    double m = c + sg * m_exp(0.5 * (u0 + u1));
    if (m > a && m < b) return m;
  }
  return 0.5 * (a + b);
}
static bool gk15(const TokBuf *f, CalcVars *w, double a, double b, double *res, double *err) {
  double C = 0, sg = 1, u0, u1;
  bool lg = logmap(a, b, &C, &sg, &u0, &u1);
  if (lg) { a = u0; b = u1; }
  double c = 0.5 * (a + b), h = 0.5 * (b - a), fc, sk, sg7;
  #define AT(u, out) (lg ? (fn(f, w, C + sg * m_exp(u), h * m_exp(u), out) && ((*(out) *= m_exp(u)), 1)) \
                         : fn(f, w, u, h, out))
  if (!AT(c, &fc)) return false;
  sk = fc * WGK[7];
  sg7 = fc * WG[3];
  for (int j = 0; j < 7; j++) {
    double x = h * XGK[j], fa, fb;
    if (!AT(c - x, &fa) || !AT(c + x, &fb)) return false;
    sk += WGK[j] * (fa + fb);
    if (j & 1) sg7 += WG[j / 2] * (fa + fb);
  }
  #undef AT
  *res = sk * h;
  *err = m_fabs((sk - sg7) * h);
  return true;
}

#define INT_MAX_PIECES 256

// Tanh-sinh (double exponential) rule on [a, b]: copes with a singular end
// (1/sqrt(1-x^2) at 1, x^-0.9 at 0). 0: converged, 1: diverges (the terms
// at the very ends don't die away: 1/x at 0), 2: no answer, 3: close.
static double g_de_err;                             // de(): its error, about
static int de(const TokBuf *f, CalcVars *w, double a, double b, double *out) {
  const double PI2 = M_PI_D / 2;
  double c = 0.5 * (a + b), hw = 0.5 * (b - a), f0, R, I = 0, prev = 0, tail = 0;
  bool cut = false;               // stopped short of an end where f still mattered
  if (!fn(f, w, c, hw, &f0)) return 2;
  R = PI2 * f0;
  double h = 1;
  for (int lev = 0; lev <= 6; lev++) {
    double step = lev ? 2 * h : h, tl = 0, tr = 0, gl = 0, gr = 0;
    bool L = true, Rt = true;                                     // each end until it is used up
    for (double t = h; t <= 6.5 && (L || Rt); t += step) {
      double e = m_exp(t), sh = 0.5 * (e - 1 / e), ch = 0.5 * (e + 1 / e);
      double u = m_exp(PI2 * sh), uu = u * u;
      if (!m_isfinite(uu)) break;
      double d = 2 / (uu + 1), wt = PI2 * ch * d * (2 - d), x, fx;
      for (int side = 0; side < 2; side++) {
        if (!(side ? Rt : L)) continue;
        x = side ? b - hw * d : a + hw * d;
        // as close as doubles go; next to a nonzero end f (1/sqrt(1-x^2) at
        // 1) is mostly rounding noise in the last few doubles: stop there
        double en = side ? b : a;
        if (m_fabs(x - en) <= 2.3e-16 * m_fabs(en) || x == en) {
          if (x != en && (side ? tr : tl) > 1e-10 * m_fabs(R)) cut = true;
          if (side) Rt = false; else L = false; continue;
        }
        if (!f1(f, w, x, &fx)) {
          // a few doubles off a nonzero end that is only near the true point
          // (|x - 1.3|^-0.7: the point found is 1.3000000000000003): as cut
          if (en != 0 && m_fabs(x - en) <= 1e-13 * m_fabs(en)) {
            if ((side ? tr : tl) > 1e-10 * m_fabs(R)) cut = true;
            if (side) Rt = false; else L = false; continue;
          }
          // overflowing right at an end after big terms (1/x^2 at 0): diverges
          return (d < 1e-6 && (side ? tr : tl) > 1e-6 * m_fabs(R) && (side ? gr : gl) > 1e10 * (m_fabs(f0) + 1e-300)) ? 1 : 2;
        }
        if (side) gr = m_fabs(fx); else gl = m_fabs(fx);
        double tm = wt * fx;
        R += tm;
        if (side) tr = m_fabs(tm); else tl = m_fabs(tm);
        if (t > 2 && m_fabs(tm) <= 1e-17 * m_fabs(R)) { if (side) { Rt = false; tr = 0; } else { L = false; tl = 0; } }
      }
    }
    tail = tl > tr ? tl : tr;
    I = h * hw * R;
    if (!m_isfinite(I)) return 1;
    if (lev >= 3 && m_fabs(I - prev) <= 1e-11 * m_fabs(I) + 1e-300) break;
    prev = I;
    h *= 0.5;
  }
  *out = I;
  // what the ends left out, about: the last terms' share
  g_de_err = tail * h * hw + m_fabs(I - prev);
  // big terms still at an end: diverges (1/x at 0), unless doubles just
  // couldn't get closer to a nonzero end ((1-x)^-0.7 at 1: the caller's
  // d|f| test tells a pole from that)
  if (tail > 1e-6 * m_fabs(R)) return cut ? 3 : 1;
  if (m_fabs(I - prev) > 1e-8 * m_fabs(I) + 1e-300) return 2;
  return (cut || tail > 1e-10 * m_fabs(R)) ? 3 : 0;      // 3: the ends could not be reached closely enough
}

CalcErr calc_integrate(const TokBuf *f, const CalcVars *v, double a, double b, double *out, bool *approx) {
  double l, r;
  if (calc_eval_eq(f, v, &l, &r) == CALC_ERR_SYNTAX) return CALC_ERR_SYNTAX;
  if (calc_has_eq(f)) return CALC_ERR_EQ;
  if (!m_isfinite(a) || !m_isfinite(b)) return CALC_ERR_MATH;
  *approx = false;
  if (a == b) { *out = 0; return CALC_OK; }
  if (a > b) {                                        // from 1E99 to 0: minus the integral from 0 to 1E99
    CalcErr e = calc_integrate(f, v, b, a, out, approx);
    if (e == CALC_OK && *out != 0) *out = -*out;
    return e;
  }
  // pieces live on the heap only while integrating (the watch's stack is small)
  double *pa = malloc(4 * INT_MAX_PIECES * sizeof(double));
  if (!pa) return CALC_ERR_MATH;
  double *pb = pa + INT_MAX_PIECES, *pr = pb + INT_MAX_PIECES, *pe = pr + INT_MAX_PIECES;
  CalcVars w = *v;
  int n = 0;
  CalcErr ret = CALC_OK;
  bool stuck = false;
  double fin_asum = 0;                                // the size of the parts, at the end
  // Start from 4 pieces; over a long range (e^-x from 0 to 1E5) the end
  // quarters are cut finer toward each end too, at 1/10, 1/100, ... of the
  // length, or all the area near an end could fall between the nodes.
  double br[48];
  int nb = 0;
  br[nb++] = a;
  // (up to 10 steps each way; on a range like 0..1E99 each step spans
  // several decades, and such pieces are split at their geometric mean)
  int D = 0;
  for (double L = m_fabs(b - a); L > 100 && D < 330; L /= 10) D++;
  int st = (D + 9) / 10, g = st ? (D + st - 1) / st : 0;
  #define GR(k) (m_pow10i(-(k) * st - 1) * 2.5)
  for (int k = g; k >= 1; k--) br[nb++] = a + (b - a) * GR(k);
  br[nb++] = a + (b - a) / 4;
  {   // and toward the middle from both sides: a peak right there (e^-x^2 from -1E4 to 1E4) sits on a break
    double m = a + (b - a) / 2;
    for (int k = 1; k <= g; k++) br[nb++] = m - (b - a) * GR(k);
    br[nb++] = m;
    for (int k = g; k >= 1; k--) br[nb++] = m + (b - a) * GR(k);
  }
  br[nb++] = a + (b - a) * 3 / 4;
  for (int k = 1; k <= g; k++) br[nb++] = b - (b - a) * GR(k);
  br[nb++] = b;
  #undef GR
  g_anc[0] = a; g_anc[1] = a + (b - a) / 2; g_anc[2] = b;
  for (int i = 0; i + 1 < nb; i++) {
    pa[n] = br[i];
    pb[n] = br[i + 1];
    if (!gk15(f, &w, pa[n], pb[n], &pr[n], &pe[n])) { ret = CALC_ERR_MATH; goto done; }
    n++;
  }
  int n0 = n;                                         // (pieces before any split)
  for (;;) {
    double tot = 0, terr = 0, asum = 0;                // asum: the size of the parts (sin x over 0..2pi: tot 0, asum 4)
    int worst = 0;
    for (int i = 0; i < n; i++) {
      tot += pr[i]; terr += pe[i]; asum += m_fabs(pr[i]);
      if (pe[i] > pe[worst]) worst = i;
    }
    fin_asum = asum;
    double tol = 1e-11 * m_fabs(tot);
    if (tol < 1e-13 * asum) tol = 1e-13 * asum;
    if (tol < 1e-300) tol = 1e-300;
    if (terr <= tol && !stuck) {
      // A wave far faster than the pieces can fool both rules alike (sin x
      // over 0..10^6), and then only a few pieces were needed (checked below).
      bool tiny_end = false;          // an end piece shrunk to rounding size: a singular end
      for (int i = 0; i < n; i++)     // GK can't resolve (1/sqrt(1-x^2) at 1)
        if ((pa[i] == a && a != 0 && pb[i] - pa[i] < 1e-12 * m_fabs(a)) ||
            (pb[i] == b && b != 0 && pb[i] - pa[i] < 1e-12 * m_fabs(b))) tiny_end = true;
      if (tiny_end) { stuck = true; continue; }
      if (n > n0 + 8) { *out = m_fabs(tot) < 2e-15 * asum ? 0 : tot; break; }   // (parts that cancel: 0, not 3e-16)
      // only a few pieces were needed: work each out again cut at another
      // point (0.382 of the way); a wave aliasing with the nodes won't agree
      double chk = 0, ce = 0, r_, e_;
      for (int i = 0; i < n; i++) {
        double c = pa[i] + (pb[i] - pa[i]) * 0.3819660112501051;
        if (!gk15(f, &w, pa[i], c, &r_, &e_)) { chk = m_nan(); break; }
        chk += r_; ce += e_;
        if (!gk15(f, &w, c, pb[i], &r_, &e_)) { chk = m_nan(); break; }
        chk += r_; ce += e_;
      }
      if (m_isfinite(chk) && m_fabs(chk - tot) > ce + terr + 1e-8 * m_fabs(tot) + 1e-12 * asum + 1e-300) { ret = CALC_ERR_INT; goto done; }
      *out = m_fabs(tot) < 2e-15 * asum ? 0 : tot; break;
    }
    double ma = pa[worst], mb = pb[worst], mid = split_at(ma, mb);   // (in the logarithm for a piece spanning decades)
    if (stuck || n >= INT_MAX_PIECES || mid == ma || mid == mb) {   // out of room: best estimate
      *out = tot;
      if (m_fabs(tot) < 2e-15 * asum && terr < 1e-10 * asum) { *out = 0; break; }   // parts that cancel: 0 (x over +-1E12)
      *approx = terr > 1e-10 * (m_fabs(tot) > 1e-13 * asum ? m_fabs(tot) : 1e-13 * asum + 1e-300);
      if (!*approx) break;
      // Each piece still far from settled: is f unbounded there (a singular
      // point: 1/sqrt(1-x^2) at 1, x^-0.9 at 0)? Only then does the double
      // exponential rule replace that piece. A bounded f there is a jump or
      // wiggles (floor x, sin(1/x), or sin(x)/x over a long range, whose hole
      // at 0 is no singularity) and its piece stays, the total marked ~.
      double avg = (m_fabs(tot) + terr) / m_fabs(b - a), left = 0, sum = 0, W = m_fabs(b - a) / 16;
      bool rough = false, any = false;
      double sing[6];
      int ns = 0;
      for (int i = 0; i < n && ns < 6; i++) {
        if (pe[i] <= 0.05 * terr) continue;
        double pa_ = pa[i], pb_ = pb[i], pm_ = 0.5 * (pa_ + pb_), h_ = pb_ - pa_, fa_ = 0, fb_ = 0, fm_ = 0;
        bool oa = fn(f, &w, pa_, h_, &fa_), ob = fn(f, &w, pb_, -h_, &fb_), om = fn(f, &w, pm_, h_, &fm_);
        double big = m_fabs(fa_);
        if (m_fabs(fb_) > big) big = m_fabs(fb_);
        if (m_fabs(fm_) > big) big = m_fabs(fm_);
        if (oa && ob && om && big <= 1e4 * avg + 1e-300) continue;        // bounded: no singular point
        double sp = !oa ? pa_ : !ob ? pb_ : m_fabs(fa_) >= m_fabs(fb_) ? (m_fabs(fa_) >= m_fabs(fm_) ? pa_ : pm_)
                                                                        : (m_fabs(fb_) >= m_fabs(fm_) ? pb_ : pm_);
        {   // home in on where |f| peaks (tan x: pi/2 is between the ends, not at one)
          double lo_ = pa_ - h_ > a ? pa_ - h_ : a, hi_ = pb_ + h_ < b ? pb_ + h_ : b, q1, q2;
          for (int it = 0; it < 200 && hi_ - lo_ > 4e-16 * (m_fabs(lo_) + m_fabs(hi_)); it++) {
            double m1 = lo_ + (hi_ - lo_) * 0.382, m2 = hi_ - (hi_ - lo_) * 0.382;
            if (!fn(f, &w, m1, 0, &q1)) { lo_ = hi_ = m1; break; }
            if (!fn(f, &w, m2, 0, &q2)) { lo_ = hi_ = m2; break; }
            if (m_fabs(q1) > m_fabs(q2)) hi_ = m2; else lo_ = m1;
          }
          sp = 0.5 * (lo_ + hi_);
        }
        double s12 = calc_round_sig(sp, 12);                // the point itself is most likely a plain number (1, 0.3)
        if (s12 >= pa_ && s12 <= pb_) sp = s12;
        bool dup = false;
        for (int k = 0; k < ns; k++) if (m_fabs(sing[k] - sp) <= W) dup = true;
        if (!dup) sing[ns++] = sp;
      }
      // Around each singular point, the pieces within W of it (a run, [L, R])
      // go to the double exponential rule: it needs room, since doubles can't
      // get very close to a point away from 0.
      uint8_t used[INT_MAX_PIECES];
      for (int i = 0; i < n; i++) used[i] = 0;
      for (int k = 0; k < ns && ret == CALC_OK; k++) {
        double sp = sing[k], L = sp, R = sp, pr_ = 0, pe_ = 0;
        for (int j = 0; j < n; j++)
          if (!used[j] && ((pa[j] <= sp && pb[j] >= sp) || (pa[j] >= sp - W && pb[j] <= sp + W))) {
            if (pa[j] < L) L = pa[j];
            if (pb[j] > R) R = pb[j];
          }
        for (int j = 0; j < n; j++)                     // (all of [L, R], so nothing counts twice)
          if (!used[j] && pa[j] >= L && pb[j] <= R) { used[j] = 1; pr_ += pr[j]; pe_ += pe[j]; }
        double r1 = 0, r2 = 0;
        {   // a pole that no double lands on (tan x at pi/2): d |f| stays put as d shrinks
          double m4 = 0, m8 = 0, q;
          for (int sd = -1; sd <= 1; sd += 2) {
            double d4 = 1e-4 * W, d8 = 1e-8 * W;
            if (sp + sd * d4 >= a && sp + sd * d4 <= b && f1(f, &w, sp + sd * d4, &q) && d4 * m_fabs(q) > m4) m4 = d4 * m_fabs(q);
            if (sp + sd * d8 >= a && sp + sd * d8 <= b && f1(f, &w, sp + sd * d8, &q) && d8 * m_fabs(q) > m8) m8 = d8 * m_fabs(q);
          }
          if (m4 > 0 && m8 >= 0.5 * m4) { ret = CALC_ERR_MATH; break; }
        }
        double de_e = 0;
        int k1 = sp > L ? de(f, &w, L, sp, &r1) : 0;
        if (sp > L) de_e += g_de_err;
        int k2 = sp < R ? de(f, &w, sp, R, &r2) : 0;
        if (sp < R) de_e += g_de_err;
        if (k1 == 1 || k2 == 1) { ret = CALC_ERR_MATH; break; }           // diverges
        if (k1 == 2 || k2 == 2) { sum += pr_; left += pe_; continue; }    // no better: the pieces stay
        sum += r1 + r2;
        any = true;
        if (k1 == 3 || k2 == 3) { rough = true; left += de_e; }   // (what the ends left out counts too)
      }
      for (int i = 0; i < n; i++) if (!used[i]) { sum += pr[i]; left += pe[i]; }
      if (ret != CALC_OK) break;
      if (m_fabs(sum) < 10 * left + 2e-15 * asum) sum = 0;   // no bigger than its own error: ~0 (1/cbrt(x-1) on 0..2)
      *out = sum;
      double rel = left / (m_fabs(sum) > 1e-3 * asum ? m_fabs(sum) : 1e-3 * asum + 1e-300);
      *approx = rough || rel > 1e-10;
      if (*approx) {
        // only the digits the error estimate vouches for (sin(1/x) on [0, 1]:
        // ~0.504, not ~0.5041174345 with the last six digits made up)
        int dg = rel > 0 ? (int)m_floor(-m_log10(rel)) - 2 : 10;   // (the estimate itself can be off 100x on waves)
        if (dg > 10) dg = 10;
        if (rough && dg > 7) dg = 7;
        if (dg < 2 && sum != 0) { ret = CALC_ERR_INT; break; }
        if (sum != 0) *out = calc_round_sig(sum, dg);
      }
      if (!any && terr > 1e-2 * (m_fabs(tot) > 1e-3 * asum ? m_fabs(tot) : 1e-3 * asum) + 1e-300) ret = CALC_ERR_INT;   // nowhere near: no number
      break;
    }
    // split the worst piece in two
    double r1_, e1_, r2_, e2_;
    if (!gk15(f, &w, ma, mid, &r1_, &e1_) || !gk15(f, &w, mid, mb, &r2_, &e2_)) {
      stuck = true;               // a node hit the singular point itself: as out of room
      continue;
    }
    pb[worst] = mid; pr[worst] = r1_; pe[worst] = e1_;
    pa[n] = mid; pb[n] = mb; pr[n] = r2_; pe[n] = e2_;
    n++;
  }
  if (!m_isfinite(*out)) ret = CALC_ERR_MATH;
  // parts far bigger than the answer (x - 5 over +-1E12: parts of 5E23, answer
  // -1E13): their rounding alone limits it, ~4E-16 of them. Then "~" and only
  // the digits that survive.
  if (ret == CALC_OK && *out != 0) {
    double rel = 4e-16 * fin_asum / m_fabs(*out);
    if (rel > 1e-10) {
      int dg = (int)m_floor(-m_log10(rel));
      if (rel >= 0.5) *out = 0;                         // lost in the parts' rounding
      else *out = calc_round_sig(*out, dg < 1 ? 1 : dg < 10 ? dg : 10);
      *approx = true;
    }
  }
  if (ret == CALC_OK && *out == 0) *out = 0.0;
done:
  free(pa);
  return ret;
}

static CalcErr deriv(const TokBuf *f, const CalcVars *v, double x0, double sx_, double *out);

static bool g_dapprox;                               // deriv(): rounded to fewer digits
CalcErr calc_derivative(const TokBuf *f, const CalcVars *v, double x0, double *out) {
  return calc_derivative2(f, v, x0, out, NULL);
}
CalcErr calc_derivative2(const TokBuf *f, const CalcVars *v, double x0, double *out, bool *approx) {
  double l, r;
  g_dapprox = false;
  if (approx) *approx = false;
  if (calc_eval_eq(f, v, &l, &r) == CALC_ERR_SYNTAX) return CALC_ERR_SYNTAX;
  if (calc_has_eq(f)) return CALC_ERR_EQ;
  if (!m_isfinite(x0)) return CALC_ERR_MATH;
  CalcErr e = deriv(f, v, x0, 0, out);
  bool ap = g_dapprox;
  // close to 0 the steps are sized for 1; at 1E-9 (x^3: 3E-18) or 1E-100
  // (ln x) they must be sized for x itself
  double ax = m_fabs(x0), o2;
  if (ax > 0 && ax < 1 && (e != CALC_OK || *out == 0) && (g_dapprox = false, deriv(f, v, x0, ax, &o2)) == CALC_OK && o2 != 0) {
    *out = o2;
    if (approx) *approx = g_dapprox;
    return CALC_OK;
  }
  if (approx && e == CALC_OK) *approx = ap;
  return e;
}

static CalcErr deriv(const TokBuf *f, const CalcVars *v, double x0, double sx_, double *out) {
  CalcVars w = *v;
  double fp, fm, f0;
  if (!f1(f, &w, x0, &f0)) return CALC_ERR_MATH;     // must be defined at x0
  // Ridders: central differences with shrinking h, Richardson-extrapolated.
  enum { NT = 10 };
  const double CON = 1.4, CON2 = CON * CON;
  // First find a step where f looks smooth: the slopes to the left and to
  // the right agree, or their gap shrinks in step with h (f'' != 0). A pole
  // close by (1/x at 0.001) forces a smaller h; a kink (abs(x) at 0), a jump
  // (floor at 1) or a vertical tangent (cbrt at 0) never settles: no answer.
  // The first step is at most 0.5: sin x at 47 has waves far shorter than
  // 5% of x. Agreement must hold at two steps in a row (once can be luck).
  double ax = m_fabs(x0), sx = sx_ > 0 ? sx_ : ax > 1 ? ax : 1, h = 0.05 * (sx < 10 ? sx : 10), gap0 = -1;
  int lin = 0, flat = 0, agree = 0;
  double d0 = 0;
  // far from 0 a small step can be lost in rounding (ln x at 1E8: f moves
  // by 1E-8 of itself over 0.5): then start bigger, until f visibly moves
  for (int g = 0; g < 40 && h < 0.05 * sx; g++) {
    if (!f1(f, &w, x0 + h, &fp) || !f1(f, &w, x0 - h, &fm)) break;
    if (m_fabs(fp - fm) >= 1e-6 * (m_fabs(fp) + m_fabs(fm))) break;
    h *= 4;
  }
  if (h > 0.05 * sx) h = 0.05 * sx;
  double sbig = 0, hbig = h;                          // the slope over the first, widest step
  if (f1(f, &w, x0 + h, &fp) && f1(f, &w, x0 - h, &fm)) sbig = m_fabs(fp - fm) / (2 * h);
  int kk = 0;
retry:
  for (int k = kk;; k++) {
    kk = k + 1;
    if (k == 60) return CALC_ERR_MATH;
    h = (x0 + h) - x0;                                 // a step the doubles hold exactly
    if (f1(f, &w, x0 + h, &fp) && f1(f, &w, x0 - h, &fm)) {
      // a jump (floor at 1): f(x0+h) - f(x0-h) stops shrinking with h, even
      // with h far smaller than anything near x0 (|x| at 0.001 is no jump)
      double d = m_fabs(fp - fm);
      if (h < 1e-7 * sx && d0 > 0 && d > d0 / 1.5 && d < d0 * 1.5 && d > 1e-9 * (m_fabs(fp) + m_fabs(fm))) {
        if (++flat >= 3) return CALC_ERR_MATH;
      } else flat = 0;
      d0 = d;
      double sr = (fp - f0) / h, sl = (f0 - fm) / h, gap = m_fabs(sr - sl);
      double noise = 1e-13 * (m_fabs(f0) + m_fabs(fp) + m_fabs(fm)) / h;
      if (gap <= 0.02 * (m_fabs(sr) + m_fabs(sl)) + noise) { if (++agree >= 2) break; }
      else agree = 0;
      if (gap0 > 0 && gap <= 0.3 * gap0 && gap >= 0.01 * gap0) { if (++lin >= 2) break; }   // (f'' = 0 shrinks faster: x^4 at 0)
      else lin = 0;
      gap0 = gap;
    } else {
      gap0 = -1; lin = 0; agree = 0;
    }
    h *= 0.25;
    if (h < 1e-300) return CALC_ERR_MATH;
  }
  {   // a slope that keeps growing as h shrinks is a vertical tangent (cbrt(x) at 0)
    double s1 = (fp - fm) / (2 * h), q, r;
    if (f1(f, &w, x0 + h / 16, &q) && f1(f, &w, x0 - h / 16, &r)) {
      double s2 = (q - r) / (h / 8);
      if (m_fabs(s2) > 3 * m_fabs(s1) + 1e-12 * (m_fabs(f0) + 1) / h) {
        // waves shorter than the step can fake agreement (tan(1000x)): look closer first
        if (h > 1e-6 * sx) { h *= 0.25; gap0 = -1; lin = 0; agree = 0; goto retry; }
        return CALC_ERR_MATH;
      }
    }
  }
  // how much f moves over the step: the scale of f near x0 even where f(x0) = 0
  double fsc = m_fabs(fp - fm) / h * (ax > 1 ? ax : 1), hs = h;
  double prev[NT], cur[NT], err = 1e300, eff = 1e300, ans = (fp - fm) / (2 * h), hsel = h;
  double fbig = m_fabs(f0);
  if (m_fabs(fp) > fbig) fbig = m_fabs(fp);
  if (m_fabs(fm) > fbig) fbig = m_fabs(fm);
  prev[0] = ans;
  for (int i = 1; i < NT; i++) {
    h /= CON;
    h = (x0 + h) - x0;
    if (!f1(f, &w, x0 + h, &fp) || !f1(f, &w, x0 - h, &fm)) break;
    cur[0] = (fp - fm) / (2 * h);
    if (m_fabs(fp) > fbig) fbig = m_fabs(fp);
    if (m_fabs(fm) > fbig) fbig = m_fabs(fm);
    double fac = CON2;
    for (int j = 1; j <= i; j++) {
      cur[j] = (cur[j - 1] * fac - prev[j - 1]) / (fac - 1);
      fac *= CON2;
      double e1 = m_fabs(cur[j] - cur[j - 1]), e2 = m_fabs(cur[j] - prev[j - 1]);
      // (plus f's own rounding at this step, and that of x0 +- h inside f:
      // sin(1000 x) at 4489 moves by eps |x0 f'|)
      double fx_ = m_fabs(x0 * cur[j]) > fbig ? m_fabs(x0 * cur[j]) : fbig;
      double et = e1 > e2 ? e1 : e2, rn = 8e-16 * fx_ / h;
      double ee = et > rn ? et : rn;
      if (ee <= eff) { eff = ee; err = et; ans = cur[j]; hsel = h; }
    }
    if (m_fabs(cur[i] - prev[i - 1]) >= 2 * err) break;   // higher order got worse: stop
    for (int j = 0; j <= i; j++) prev[j] = cur[j];
  }
  if (!m_isfinite(ans)) return CALC_ERR_MATH;
  // The extrapolation didn't settle (x*abs(x) at 0: f'' jumps there and the
  // steps' error isn't the smooth h^2 kind): no trustworthy answer.
  double sc = (m_fabs(f0) + fsc) / (ax > 1 ? ax : 1);
  if (err > 1e-6 * m_fabs(ans) + 1e-12 * sc && err > 1e-300) return CALC_ERR_MATH;
  // The extrapolation can't see f's own rounding (f + 1E10 rounds the same
  // at every step, so the steps "agree"): at least that much error, ~eps |f| / h.
  (void)hs;
  double rfl = 8e-16 * (m_fabs(x0 * ans) > fbig ? m_fabs(x0 * ans) : fbig) / hsel;
  if (err < rfl) err = rfl;
  if (m_fabs(ans) <= 2 * rfl) {                      // under the rounding of f itself
    // a real 0 (x^3+1 at 0: f still moves over the first step), or f too
    // big to see its slope at all (x + 1E15): then no answer, not 0
    // (judged by the odd part, the slope, and the even part, the bend: at a
    // turning point (x^2 - 2x at 1) the slope over the wide step is only
    // rounding, but f still visibly moves)
    { double ev_ = 0, q_, r_;
      if (f1(f, &w, x0 + hbig, &q_) && f1(f, &w, x0 - hbig, &r_)) ev_ = m_fabs(q_ + r_ - 2 * f0) / 2;
      if (sbig != 0 && sbig * hbig <= 64e-16 * fbig && ev_ <= 64e-16 * fbig) return CALC_ERR_MATH;   // f doesn't visibly move
      if (sbig * hbig > 64e-16 * fbig && sbig * hsel <= 64e-16 * fbig) return CALC_ERR_MATH; }       // a real slope lost in rounding
    if (sbig == 0 && fbig > 0) {                       // f didn't move over the widest step: look much wider
      double H = 1048576 * 2.2e-16 * fbig, q, r2;
      if (H > 0.05 * sx && f1(f, &w, x0 + H, &q) && f1(f, &w, x0 - H, &r2) && (m_fabs(q - f0) > 64e-16 * fbig || m_fabs(r2 - f0) > 64e-16 * fbig)) return CALC_ERR_MATH;
    }
    ans = 0;
  }
  // tidy 5.999999999999 -> 6
  double s = calc_round_sig(ans, 11);
  if (m_fabs(s - ans) <= 1e-11 * m_fabs(ans)) ans = s;
  // and no more digits than the extrapolation vouches for (sin(x)/x close
  // to 0: -4.18333e-8, not -4.183331415e-8)
  if (err > 1e-300 && ans != 0) {
    int dg = (int)m_floor(-m_log10(err / m_fabs(ans))) - 1;
    if (dg < 11) ans = calc_round_sig(ans, dg < 1 ? 1 : dg);
    if (dg < 10) g_dapprox = true;                     // shown with ~ (28631020 is not all digits)
  }
  // what is left of a zero slope is rounding: x^2 at 0, sin at 90 deg (but
  // d/dx(1E-20 x) = 1E-20 is real)
  if (m_fabs(ans) <= 4 * err && m_fabs(ans) > 0.1 * sbig && err <= rfl && sbig * hbig > 64e-16 * fbig) return CALC_ERR_MATH;   // f's rounding swamps a real slope (2x + 1E15)
  if (m_fabs(ans) <= 4 * err || m_fabs(ans) < 1e-14 * sc) ans = 0.0;
  *out = ans;
  return CALC_OK;
}

// ---------------------------------------------------------------- three equations

// lhs - rhs of e at (x, y, z)
static bool f3(const TokBuf *e, const CalcVars *v, double x, double y, double z, double *f) {
  CalcVars w = *v;
  double l, r;
  w.x = x; w.y = y; w.z = z;
  if (calc_eval_eq(e, &w, &l, &r) != CALC_OK) return false;
  *f = l - r;
  return m_isfinite(*f);
}

// e = a x + b y + c z + d? Coefficients from a base point and unit steps, then
// checked at two more points.
static CalcErr lin3(const TokBuf *e, const CalcVars *v, double k[4]) {
  static const double P[3][3] = { { 0.37, 0.61, 0.89 }, { -2.3, 1.7, 3.1 }, { 4.1, -0.9, -2.7 } };
  double f0, f;
  // From the origin when f is defined there: the constant is then f itself,
  // not a difference that cancels (x + y = 2E-10 keeps all its digits)
  double B[3] = { 0, 0, 0 };
  if (!f3(e, v, 0, 0, 0, &f0)) {
    B[0] = P[0][0]; B[1] = P[0][1]; B[2] = P[0][2];
    if (!f3(e, v, B[0], B[1], B[2], &f0)) return CALC_ERR_LIN3;
  }
  for (int i = 0; i < 3; i++) {
    double q[3] = { B[0], B[1], B[2] };
    q[i] += 1048576;                                  // a long step: 1E-12 x still shows above rounding
    if (!f3(e, v, q[0], q[1], q[2], &f)) return CALC_ERR_LIN3;
    k[i] = (f - f0) / 1048576;
  }
  k[3] = f0 - k[0] * B[0] - k[1] * B[1] - k[2] * B[2];
  for (int t = 0; t < 3; t++) {
    if (!f3(e, v, P[t][0], P[t][1], P[t][2], &f)) return CALC_ERR_LIN3;
    double want = k[0] * P[t][0] + k[1] * P[t][1] + k[2] * P[t][2] + k[3];
    double sc = 1 + m_fabs(f) + 5 * (m_fabs(k[0]) + m_fabs(k[1]) + m_fabs(k[2])) + m_fabs(k[3]);
    if (m_fabs(f - want) > 1e-9 * sc) return CALC_ERR_LIN3;
  }
  for (int i = 0; i < 4; i++) {                      // 2.0000000000000004 -> 2
    double r = calc_round_sig(k[i], 12);
    if (m_fabs(r - k[i]) <= 1e-13 * m_fabs(k[i])) k[i] = r;
  }
  return CALC_OK;
}

// Solve the n x n system m (rows a x + b y [+ c z] = d) by elimination with
// the largest pivot, each row first scaled to its largest coefficient (so a
// row of 1E-7s counts as much as a row of 1E6s).
static CalcErr solve_lin(double m[3][4], int n, double *out) {
  for (int i = 0; i < n; i++) {
    double rb = 0;
    for (int j = 0; j < n; j++) if (m_fabs(m[i][j]) > rb) rb = m_fabs(m[i][j]);
    if (rb == 0) return CALC_ERR_NOSOL;
    for (int j = 0; j <= n; j++) m[i][j] /= rb;
    m[i][3] = m[i][n];
  }
  for (int c = 0; c < n; c++) {
    int p = c;
    for (int r = c + 1; r < n; r++) if (m_fabs(m[r][c]) > m_fabs(m[p][c])) p = r;
    if (m_fabs(m[p][c]) <= 1e-12) return CALC_ERR_NOSOL;    // none, or not just one
    for (int j = 0; j < 4; j++) { double t = m[c][j]; m[c][j] = m[p][j]; m[p][j] = t; }
    for (int r = 0; r < n; r++) {
      if (r == c) continue;
      double f = m[r][c] / m[c][c];
      for (int j = c; j < 4; j++) m[r][j] -= f * m[c][j];
    }
  }
  double big = 0;
  for (int i = 0; i < n; i++) {
    double s = m[i][3] / m[i][i], r = calc_round_sig(s, 12);
    if (m_fabs(r - s) <= 1e-11 * m_fabs(s)) s = r;
    if (!m_isfinite(s)) return CALC_ERR_NOSOL;
    out[i] = s;
    if (m_fabs(s) > big) big = m_fabs(s);
  }
  for (int i = 0; i < n; i++) if (m_fabs(out[i]) <= 1e-14 * big || out[i] == 0) out[i] = 0.0;
  return CALC_OK;
}

CalcErr calc_solve3(const TokBuf *e1, const TokBuf *e2, const TokBuf *e3, const CalcVars *v, double xyz[3]) {
  const TokBuf *E[3] = { e1, e2, e3 };
  double m[3][4];
  for (int i = 0; i < 3; i++) {
    CalcErr e = lin3(E[i], v, m[i]);
    if (e != CALC_OK) return e;
    m[i][3] = -m[i][3];                               // a x + b y + c z = -d
  }
  return solve_lin(m, 3, xyz);
}

// Two linear equations in x and y: the one answer directly, wherever it is
// (the search below only looks inside the view). False if not that case.
static bool lin2(const TokBuf *e1, const TokBuf *e2, const CalcVars *v, double *x, double *y) {
  double m[3][4], k[4], o[2];
  const TokBuf *E[2] = { e1, e2 };
  for (int i = 0; i < 2; i++) {
    if (lin3(E[i], v, k) != CALC_OK || k[2] != 0) return false;
    m[i][0] = k[0]; m[i][1] = k[1]; m[i][2] = -k[3];
  }
  if (solve_lin(m, 2, o) != CALC_OK) return false;
  *x = o[0]; *y = o[1];
  return true;
}
