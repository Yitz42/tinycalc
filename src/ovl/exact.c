// exact.c - see exact.h
#include "../c/exact.h"
#include "../c/mmath.h"
#include "../c/config.h"

#if EXACT_FORMS

// 64-bit integers; coefficients are kept below a million so b^2 - 4ac and
// the other products fit.
typedef int64_t ll;
#define COEF_MAX 1000000

static ll labs_(ll a) { return a < 0 ? -a : a; }
static ll gcd_(ll a, ll b) {
  a = labs_(a); b = labs_(b);
  while (b) { ll t = a % b; a = b; b = t; }
  return a;
}

// v as p/q (q <= qmax) to within tol: continued fractions.
static bool rat(double v, double tol, ll qmax, ll *p, ll *q) {
  if (!m_isfinite(v) || m_fabs(v) > 1e6) return false;
  ll h0 = 0, h1 = 1, k0 = 1, k1 = 0;
  double x = v;
  for (int i = 0; i < 40; i++) {
    double a = m_floor(x);
    if (m_fabs(a) > 2e6) break;
    ll ai = (ll)a, h2 = ai * h1 + h0, k2 = ai * k1 + k0;
    if (k2 > qmax) break;
    h0 = h1; h1 = h2; k0 = k1; k1 = k2;
    if (m_fabs(v - (double)h1 / (double)k1) <= tol) { *p = h1; *q = k1; return true; }
    double fr = x - a;
    if (fr < 1e-13) break;
    x = 1.0 / fr;
  }
  return false;
}

// ---------------------------------------------------------------- tokens

static void tk(TokBuf *b, int t) { if (b->n < CALC_MAX_TOK) b->t[b->n++] = (uint8_t)t; }
static void tnum(TokBuf *b, ll v) {
  char d[20];
  int n = 0;
  if (v < 0) { tk(b, '-'); v = -v; }
  do { d[n++] = (char)('0' + v % 10); v /= 10; } while (v);
  while (n) tk(b, d[--n]);
}

// (p + s*k*sqrt(m)*[i]) / q, q > 0, already reduced. m == 1: no root sign.
static void tsurd(TokBuf *b, ll p, int s, ll k, ll m, bool imag, ll q) {
  bool hp = p != 0, hs = k != 0, frac = q != 1;
  bool paren = frac && hp && hs;
  if (paren) tk(b, '(');
  if (hp) tnum(b, p);
  if (hs) {
    if (s < 0) tk(b, '-'); else if (hp) tk(b, '+');
    if (k != 1 || (m == 1 && !imag)) tnum(b, k);
    if (m != 1) { tk(b, T_SQRT); tnum(b, m); tk(b, ')'); }
    if (imag) tk(b, T_I);
  }
  if (!hp && !hs) tk(b, '0');
  if (paren) tk(b, ')');
  if (frac) { tk(b, '/'); tnum(b, q); }
}

// D = k^2 * m with m squarefree (D up to about 1e10).
static void squarefree(ll D, ll *k, ll *m) {
  ll kk = 1;
  for (ll i = 2; i * i <= D && i <= 30000; i++)
    while (D % (i * i) == 0) { D /= i * i; kk *= i; }
  *k = kk; *m = D;
}

// ---------------------------------------------------------------- roots

static void add(Exact *e, const TokBuf *t, double x, bool real) {
  for (int i = 0; i < e->n; i++)                       // repeated root
    if (e->real[i] == real && m_fabs(e->x[i] - x) <= 1e-12 * (1 + m_fabs(x)) && real) return;
  if (e->n >= EXACT_MAX) return;
  e->sol[e->n] = *t;
  e->x[e->n] = x; e->y[e->n] = 0; e->real[e->n] = real;
  e->n++;
}

static void add_rat(Exact *e, ll p, ll q) {
  ll g = gcd_(p, q);
  if (g) { p /= g; q /= g; }
  if (q < 0) { p = -p; q = -q; }
  TokBuf t; t.n = 0;
  tk(&t, T_X); tk(&t, '=');
  tsurd(&t, p, 1, 0, 1, false, q);
  add(e, &t, (double)p / (double)q, true);
}

static void add_quad(Exact *e, ll a, ll b, ll c) {   // a > 0
  ll D = b * b - 4 * a * c;
  if (D == 0) { add_rat(e, -b, 2 * a); return; }
  bool imag = D < 0;
  ll k, m;
  squarefree(imag ? -D : D, &k, &m);
  if (m == 1 && !imag) { add_rat(e, -b - k, 2 * a); add_rat(e, -b + k, 2 * a); return; }
  ll q = 2 * a, g = gcd_(gcd_(b, k), q);
  ll P = -b / g, K = k / g, Q = q / g;
  for (int s = -1; s <= 1; s += 2) {
    TokBuf t; t.n = 0;
    tk(&t, T_X); tk(&t, '=');
    tsurd(&t, P, s, K, m, imag, Q);
    double re = (double)P / (double)Q, sq = (double)K * m_sqrt((double)m) / (double)Q;
    add(e, &t, imag ? re : re + s * sq, !imag);
  }
}

// Sum of a[k] p^k q^(n-k): zero when p/q is a root.
// (in doubles: exact here, since every term stays below 2^53)
static bool is_root(const ll *a, int n, ll p, ll q) {
  ll s = 0, pp = 1;
  for (int k = 0; k <= n; k++) {
    ll qq = 1, t;
    for (int i = k; i < n; i++) qq *= q;                // q, p <= 1000: no overflow here
    if (__builtin_mul_overflow(a[k], pp, &t) || __builtin_mul_overflow(t, qq, &t) ||
        __builtin_add_overflow(s, t, &s)) return false;
    pp *= p;
  }
  return s == 0;
}

// a / (q x - p), in place; degree drops by one.
static void deflate(ll *a, int n, ll p, ll q) {
  ll d[5];
  d[n - 1] = a[n] / q;
  for (int k = n - 1; k >= 1; k--) d[k - 1] = (a[k] + p * d[k]) / q;
  for (int k = 0; k < n; k++) a[k] = d[k];
  a[n] = 0;
}

static bool find_rat_root(const ll *a, int n, ll *rp, ll *rq) {
  ll c0 = labs_(a[0]), cn = labs_(a[n]);
  if (c0 > 1000 || cn > 1000) return false;
  for (ll p = 1; p <= c0; p++) {
    if (c0 % p) continue;
    for (ll q = 1; q <= cn; q++) {
      if (cn % q || gcd_(p, q) != 1) continue;
      if (is_root(a, n, p, q)) { *rp = p; *rq = q; return true; }
      if (is_root(a, n, -p, q)) { *rp = -p; *rq = q; return true; }
    }
  }
  return false;
}

// x = s * sqrt(p/q)  (or s * i sqrt(-p/q)), reduced
static void add_sqrt_rat(Exact *e, ll p, ll q) {
  ll g = gcd_(p, q);
  p /= g; q /= g;
  if (q < 0) { p = -p; q = -q; }
  bool imag = p < 0;
  ll k, m;
  squarefree((imag ? -p : p) * q, &k, &m);
  ll g2 = gcd_(k, q), K = k / g2, Q = q / g2;
  for (int s = -1; s <= 1; s += 2) {
    TokBuf t; t.n = 0;
    tk(&t, T_X); tk(&t, '=');
    tsurd(&t, 0, s, K, m, imag, Q);
    add(e, &t, imag ? 0 : s * (double)K * m_sqrt((double)m) / (double)Q, !imag);
  }
}

// What is left when there are no more rational roots:
//   a x^3 + c = 0        -> x = k cbrt(m) / q
//   a x^4 + b x^2 + c = 0 with rational x^2 -> x = +-sqrt(x^2)
static void special(Exact *e, const ll *a, int n) {
  if (n == 3 && a[1] == 0 && a[2] == 0 && a[0] != 0) {
    ll P = -a[0], Q = a[3], g = gcd_(P, Q);
    P /= g; Q /= g;
    if (Q < 0) { P = -P; Q = -Q; }
    ll N, k = 1;
    if (__builtin_mul_overflow(labs_(P), Q * Q, &N)) return;
    for (ll i = 2; i * i * i <= N && i <= 20000; i++)
      while (N % (i * i * i) == 0) { N /= i * i * i; k *= i; }
    ll g2 = gcd_(k, Q), K = k / g2, D = Q / g2;
    TokBuf t; t.n = 0;
    tk(&t, T_X); tk(&t, '=');
    if (P < 0) tk(&t, '-');
    if (K != 1) tnum(&t, K);
    tk(&t, T_CBRT); tnum(&t, N); tk(&t, ')');
    if (D != 1) { tk(&t, '/'); tnum(&t, D); }
    add(e, &t, (P < 0 ? -1 : 1) * (double)K * m_cbrt((double)N) / (double)D, true);
    return;
  }
  if (n == 4 && a[1] == 0 && a[3] == 0) {
    ll D = a[2] * a[2] - 4 * a[4] * a[0], k, m;
    if (D < 0) return;
    squarefree(D, &k, &m);
    if (m != 1) return;                                // x^2 is a surd: nested roots, decimals only
    add_sqrt_rat(e, -a[2] - k, 2 * a[4]);
    if (k) add_sqrt_rat(e, -a[2] + k, 2 * a[4]);
  }
}

static void solve_int(Exact *e, ll *a, int n) {
  while (n > 2) {                                     // peel off rational roots
    ll p, q;
    if (a[0] == 0) { p = 0; q = 1; }
    else if (!find_rat_root(a, n, &p, &q)) { special(e, a, n); return; }
    add_rat(e, p, q);
    deflate(a, n, p, q);
    n--;
  }
  if (n == 2) add_quad(e, a[2], a[1], a[0]);
  else if (n == 1) add_rat(e, -a[0], a[1]);
}

// ---------------------------------------------------------------- polynomials

static bool fx(const TokBuf *f, const CalcVars *v, double x, double *out) {
  double l, r;
  if (!calc_sides(f, v, x, &l, &r)) return false;
  *out = l - r;
  return m_isfinite(*out);
}

// Doubles that are rationals -> integers with no common factor.
static bool integerize(const double *c, int n, ll *a) {
  double big = 0;
  for (int k = 0; k <= n; k++) if (m_fabs(c[k]) > big) big = m_fabs(c[k]);
  // tight: 0.3333333333 typed is not 1/3, and 1e-10 typed is not 0
  double tol = 1e-13 * (big > 1 ? big : 1);
  ll p[5], q[5], L = 1;
  for (int k = 0; k <= n; k++) {
    // rounding noise. The constant term is f(0) itself, not a difference of
    // samples, so only its own rounding counts: (x+0.3)^2 - 0.09 leaves 1e-17,
    // but x^2 + 1E-15 is no x^2.
    double noise = (k == 0 ? 5e-17 : 1e-14) * (big > 1 ? big : 1);
    if (m_fabs(c[k]) <= noise) { p[k] = 0; q[k] = 1; continue; }
    if (m_fabs(c[k]) <= tol) return false;           // x^2 = 1E-12: tiny but real, no exact form
    if (!rat(c[k], tol, 1000, &p[k], &q[k])) return false;
    if (labs_(p[k]) > COEF_MAX) return false;
    L = L / gcd_(L, q[k]) * q[k];
    if (L > COEF_MAX) return false;
  }
  ll g = 0;
  for (int k = 0; k <= n; k++) {
    a[k] = p[k] * (L / q[k]);
    if (labs_(a[k]) > COEF_MAX) return false;
    g = gcd_(g, a[k]);
  }
  if (g > 1) for (int k = 0; k <= n; k++) a[k] /= g;
  return true;
}

static void tpoly(TokBuf *b, const ll *a, int n) {
  bool first = true;
  for (int k = n; k >= 0; k--) {
    ll c = a[k];
    if (!c) continue;
    if (c < 0) tk(b, '-'); else if (!first) tk(b, '+');
    c = labs_(c);
    if (c != 1 || k == 0) tnum(b, c);
    if (k >= 1) tk(b, T_X);
    if (k == 2) tk(b, T_SQ);
    if (k == 3) tk(b, T_CUBE);
    if (k == 4) { tk(b, '^'); tk(b, '4'); }
    first = false;
  }
  tk(b, '='); tk(b, '0');
}

bool exact_solve(const TokBuf *f, const CalcVars *v, Exact *e) {
  e->n = 0; e->simp.n = 0;
  if (!calc_has_x(f) || calc_has_y(f)) return false;
  double d[5];
  for (int i = 0; i < 5; i++) if (!fx(f, v, i, &d[i])) return false;
  for (int k = 1; k < 5; k++)                          // forward differences at 0
    for (int i = 4; i >= k; i--) d[i] -= d[i - 1];
  double c[5] = { d[0], d[1] - d[2] / 2 + d[3] / 3 - d[4] / 4,
                  d[2] / 2 - d[3] / 2 + 11 * d[4] / 24, d[3] / 6 - d[4] / 4, d[4] / 24 };
  // It's only this polynomial if it also matches away from 0..4, at points
  // that are no simple fractions either (floor(10x) agrees at every tenth)
  // (and out past the search box: |x - 8| = 1 is x - 7 = 0 at all of 0..7)
  static const double T[9] = { -1.5, 2.7, -7.3, 0.7071067811865476, -1.618033988749895, 9.37, -9.83, 23.7, -23.1 };
  for (int t = 0; t < 9; t++) {
    double ft, p = 0, s = 1, pw = 1;
    if (!fx(f, v, T[t], &ft)) return false;
    for (int k = 4; k >= 0; k--) p = p * T[t] + c[k];
    for (int k = 0; k <= 4; k++) { s += m_fabs(c[k]) * pw; pw *= m_fabs(T[t]); }
    if (m_fabs(ft - p) > 1e-9 * s) return false;
  }
  ll a[5];
  if (!integerize(c, 4, a)) return false;
  int n = 4;
  while (n > 0 && a[n] == 0) n--;
  if (n == 0) return false;
  if (a[n] < 0) for (int k = 0; k <= n; k++) a[k] = -a[k];
  tpoly(&e->simp, a, n);
  solve_int(e, a, n);
  // real roots in increasing order, complex ones after
  for (int i = 1; i < e->n; i++)
    for (int j = i; j > 0; j--) {
      bool sw = e->real[j] && (!e->real[j - 1] || e->x[j] < e->x[j - 1]);
      if (!sw) break;
      TokBuf t = e->sol[j]; e->sol[j] = e->sol[j - 1]; e->sol[j - 1] = t;
      double x = e->x[j]; e->x[j] = e->x[j - 1]; e->x[j - 1] = x;
      bool r = e->real[j]; e->real[j] = e->real[j - 1]; e->real[j - 1] = r;
    }
  return true;
}

// ---------------------------------------------------------------- systems

static bool f2(const TokBuf *f, const CalcVars *v, double x, double y, double *out) {
  CalcVars w = *v;
  w.y = y;
  return fx(f, &w, x, out);
}

// f = a x + b y + c exactly? -> integers a, b, c
static bool lin(const TokBuf *f, const CalcVars *v, ll *abc) {
  double c, fa, fb;
  if (!f2(f, v, 0, 0, &c) || !f2(f, v, 1, 0, &fa) || !f2(f, v, 0, 1, &fb)) return false;
  double k[3] = { fa - c, fb - c, c };
  static const double P[5][2] = { { 2.5, -1.3 }, { -3, 4.2 }, { 0.7071067811865476, -0.5772156649015329 },   // (one irrational)
                                  { 9.37, -8.6 }, { -23.7, 17.3 } };                     // (and far out)
  for (int t = 0; t < 5; t++) {
    double ft;
    if (!f2(f, v, P[t][0], P[t][1], &ft)) return false;
    double want = k[0] * P[t][0] + k[1] * P[t][1] + k[2];
    if (m_fabs(ft - want) > 1e-9 * (1 + m_fabs(ft) + m_fabs(k[0] * P[t][0]) + m_fabs(k[1] * P[t][1]) + m_fabs(k[2]))) return false;
  }
  return integerize(k, 2, abc);
}

bool exact_solve2(const TokBuf *e1, const TokBuf *e2, const CalcVars *v, Exact *e) {
  e->n = 0; e->simp.n = 0;
  ll u[3], w[3];
  if (!lin(e1, v, u) || !lin(e2, v, w)) return false;
  ll det = u[0] * w[1] - w[0] * u[1];
  if (det == 0) return false;
  ll xn = u[1] * w[2] - w[1] * u[2], yn = w[0] * u[2] - u[0] * w[2];
  ll gx = gcd_(xn, det), gy = gcd_(yn, det);
  ll xd = det / (gx ? gx : 1), yd = det / (gy ? gy : 1);
  if (gx) xn /= gx;
  if (gy) yn /= gy;
  if (xd < 0) { xn = -xn; xd = -xd; }
  if (yd < 0) { yn = -yn; yd = -yd; }
  TokBuf t; t.n = 0;
  tk(&t, T_X); tk(&t, '='); tsurd(&t, xn, 1, 0, 1, false, xd);
  tk(&t, T_COMMA);
  tk(&t, T_Y); tk(&t, '='); tsurd(&t, yn, 1, 0, 1, false, yd);
  e->sol[0] = t;
  e->x[0] = (double)xn / (double)xd;
  e->y[0] = (double)yn / (double)yd;
  e->real[0] = true;
  e->n = 1;
  return true;
}

// ---------------------------------------------------------------- numbers

bool exact_value(double v, TokBuf *out) {
  out->n = 0;
  if (!m_isfinite(v) || m_fabs(v) > 1e9) return false;
  if (m_fabs(v - m_round(v)) <= 1e-12 * (1 + m_fabs(v))) return false;   // integers need nothing
  ll p, q, k, m;
  // close enough to be the same number, not just the same first 10 digits
  if (rat(v, 5e-14 * (1 + m_fabs(v)), 1000, &p, &q)) {
    tsurd(out, p, 1, 0, 1, false, q);
    return true;
  }
  double s = v * v;                                    // a surd: v^2 rational
  if (rat(s, 1e-13 * (1 + s), 1000, &p, &q) && p > 0 && p <= 1000000 && (double)p * q < 2e9) {
    squarefree(p * q, &k, &m);
    if (m > 1) {
      ll g = gcd_(k, q);
      tsurd(out, 0, v < 0 ? -1 : 1, k / g, m, false, q / g);
      return true;
    }
  }
  if (rat(v / M_PI_D, 5e-14 * (1 + m_fabs(v)), 100, &p, &q) && labs_(p) <= 1000) {   // p pi / q
    if (p < 0) tk(out, '-');
    if (labs_(p) != 1) tnum(out, labs_(p));
    tk(out, T_PI);
    if (q != 1) { tk(out, '/'); tnum(out, q); }
    return true;
  }
  return false;
}

#endif  // EXACT_FORMS
