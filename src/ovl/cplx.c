// cplx.c - complex evaluation (see calc_ceval in calc.h). The same grammar as
// the real parser in calc.c, on a + bi values. Functions of a real argument
// use the real versions (calc_apply) and fall back to the complex ones when
// those have no real answer (sqrt(-4), ln(-1), asin(2)). Its own overlay on
// the watch ("cplx"), loaded for = on something complex.
#include "../c/calc.h"
#include "../c/mmath.h"

typedef struct { double re, im; } Cx;

static Cx cx(double re, double im) { Cx z = { re, im }; return z; }
static Cx cnan(void) { return cx(m_nan(), 0); }
static Cx cadd(Cx a, Cx b) { return cx(a.re + b.re, a.im + b.im); }
static Cx csub(Cx a, Cx b) { return cx(a.re - b.re, a.im - b.im); }
static Cx cmul(Cx a, Cx b) { return cx(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re); }
static Cx cdiv(Cx a, Cx b) {
  if (b.re == 0 && b.im == 0) return cnan();             // (b.re^2 can underflow for a tiny b)
  if (m_fabs(b.re) >= m_fabs(b.im)) {                 // scaled, so big parts don't overflow
    double r = b.im / b.re, t = b.re + b.im * r;
    return cx((a.re + a.im * r) / t, (a.im - a.re * r) / t);
  }
  double r = b.re / b.im, t = b.re * r + b.im;
  return cx((a.re * r + a.im) / t, (a.im * r - a.re) / t);
}
static double cabs_(Cx z) {
  double a = m_fabs(z.re), b = m_fabs(z.im);
  if (a < b) { double t = a; a = b; b = t; }
  if (a == 0) return 0;
  double r = b / a;
  return a * m_sqrt(1 + r * r);
}
static double carg_(Cx z) {                          // -pi < arg <= pi
  if (z.re > 0) return m_atan(z.im / z.re);
  if (z.re < 0) return m_atan(z.im / z.re) + (z.im < 0 ? -M_PI_D : M_PI_D);
  return z.im > 0 ? M_PI_D / 2 : z.im < 0 ? -M_PI_D / 2 : 0;
}
static Cx cexp_(Cx z) {
  double e = m_exp(z.re);
  if (z.im == 0) return cx(e, 0);
  return cx(e * m_cos(z.im), e * m_sin(z.im));
}
static Cx clog_(Cx z) {
  if (z.re == 0 && z.im == 0) return cnan();
  return cx(m_log(cabs_(z)), carg_(z));
}
static Cx csqrt_(Cx z) {                             // principal root, without cancellation
  if (z.re == 0 && z.im == 0) return cx(0, 0);
  double r = cabs_(z), t = m_sqrt((r + m_fabs(z.re)) / 2);
  if (z.re >= 0) return cx(t, z.im / (2 * t));
  return cx(m_fabs(z.im) / (2 * t), z.im < 0 ? -t : t);
}
static Cx cpow_(Cx a, Cx b) {
  if (b.im == 0 && b.re == m_floor(b.re) && m_fabs(b.re) <= 64) {   // whole powers by multiplying
    int n = (int)m_fabs(b.re);
    Cx r = cx(1, 0), p = a;
    while (n) { if (n & 1) r = cmul(r, p); p = cmul(p, p); n >>= 1; }
    return b.re < 0 ? cdiv(cx(1, 0), r) : r;
  }
  if (a.re == 0 && a.im == 0) return b.re > 0 ? cx(0, 0) : cnan();
  if (b.im == 0 && b.re == m_floor(b.re) && m_fabs(b.re) < 1e15 && (a.re == 0 || a.im == 0)) {
    // c i^k to a whole power: c^n times i^(kn), exact in direction (i^10001 = i)
    double c = a.re != 0 ? a.re : a.im, n = b.re, m = m_pow(m_fabs(c), n);
    int k = a.re != 0 ? (c < 0 ? 2 : 0) : (c < 0 ? 3 : 1);
    int q = (int)(m_fmod(m_fmod(n, 4) * k, 4) + 4) % 4;
    return q == 0 ? cx(m, 0) : q == 1 ? cx(0, m) : q == 2 ? cx(-m, 0) : cx(0, -m);
  }
  if (b.im == 0 && b.re == 0.5) return csqrt_(a);
  return cexp_(cmul(b, clog_(a)));
}
static Cx ci(void) { return cx(0, 1); }
static Cx cre(double v) { return cx(v, 0); }
// sin, cos, sinh, cosh of a + bi
static Cx csin_(Cx z) { return cx(m_sin(z.re) * calc_apply(T_COSH, z.im, 0, false), m_cos(z.re) * calc_apply(T_SINH, z.im, 0, false)); }
static Cx ccos_(Cx z) { return cx(m_cos(z.re) * calc_apply(T_COSH, z.im, 0, false), -m_sin(z.re) * calc_apply(T_SINH, z.im, 0, false)); }
static Cx csinh_(Cx z) { Cx s = csin_(cx(z.im, -z.re)); return cx(-s.im, s.re); }   // sinh z = -i sin(iz)
static Cx ccosh_(Cx z) { return ccos_(cx(-z.im, z.re)); }                           // cosh z = cos(iz)

// The complex version of function f (angles in radians here).
static Cx cfunc(int f, Cx z) {
  Cx one = cre(1), i = ci();
  switch (f) {
    case T_SQRT: return csqrt_(z);
    case T_CBRT: return cpow_(z, cre(1.0 / 3));
    case T_EXP: return cexp_(z);
    case T_TENX: return cexp_(cmul(z, cre(M_LN10_D)));
    case T_LN: return clog_(z);
    case T_LOG: { Cx l = clog_(z); return cx(l.re / M_LN10_D, l.im / M_LN10_D); }
    case T_SIN: return csin_(z);
    case T_COS: return ccos_(z);
    case T_TAN: return cdiv(csin_(z), ccos_(z));
    case T_SINH: return csinh_(z);
    case T_COSH: return ccosh_(z);
    case T_TANH: return cdiv(csinh_(z), ccosh_(z));
    case T_ASIN: {                                   // -i ln(iz + sqrt(1 - z^2))
      Cx l = clog_(cadd(cmul(i, z), csqrt_(csub(one, cmul(z, z)))));
      return cx(l.im, -l.re);
    }
    case T_ACOS: { Cx a = cfunc(T_ASIN, z); return cx(M_PI_D / 2 - a.re, -a.im); }
    case T_ATAN: {                                   // i/2 (ln(1 - iz) - ln(1 + iz)), C99's branch cuts
      Cx iz = cmul(i, z), l = csub(clog_(csub(one, iz)), clog_(cadd(one, iz)));
      return cx(-l.im / 2, l.re / 2);
    }
    case T_ASINH: return clog_(cadd(z, csqrt_(cadd(cmul(z, z), one))));
    case T_ACOSH: return clog_(cadd(z, cmul(csqrt_(cadd(z, one)), csqrt_(csub(z, one)))));
    case T_ATANH: { Cx l = clog_(cdiv(cadd(one, z), csub(one, z))); return cx(l.re / 2, l.im / 2); }
    case T_ABS: return cre(cabs_(z));
    case T_CONJ: return cx(z.re, -z.im);
    case T_RE: return cre(z.re);
    case T_IM: return cre(z.im);
    case T_ARG: return cre(carg_(z));
  }
  return cnan();
}

static bool trig_in(int f) { return f >= T_SIN && f <= T_TAN; }
static bool trig_out(int f) { return (f >= T_ASIN && f <= T_ATAN) || f == T_ARG; }

static double ncdf(double x);
static double ninv(double p);
static Cx apply(int f, Cx z, bool deg) {
  if (f == T_NPDF || f == T_NCDF || f == T_NINV) {
    if (z.im != 0) return cnan();
    return cre(f == T_NPDF ? m_exp(-0.5 * z.re * z.re) * 0.3989422804014327 : f == T_NCDF ? ncdf(z.re) : ninv(z.re));
  }
  if (z.im == 0 && f != T_CONJ && f != T_RE && f != T_IM) {
    double r = calc_apply(f, z.re, 0, deg);           // the real answer when there is one
    if (m_isfinite(r) || f == T_TAN) return cre(r);   // (tan 90: a pole, not a complex answer)
  }
  if (trig_in(f) && deg) z = cmul(z, cre(M_PI_D / 180));
  Cx r = cfunc(f, z);
  if (trig_out(f) && deg) r = cmul(r, cre(180 / M_PI_D));
  return r;
}

// ---------------------------------------------------------------- probability
// The standard normal (density, P(Z < x), its inverse). Only here, in the
// complex evaluator's overlay: the real one hands these over (to save room
// in the app).
// erfc(x) for x >= 0: a series for small x (all terms positive: no
// cancellation), a continued fraction beyond.
static double erfc_pos(double x) {
  if (x < 1.5) {
    double t = x, s = x, x2 = x * x;                  // erf = 2/sqrt(pi) e^-x^2 sum 2^n x^(2n+1) / (1.3...(2n+1))
    for (int n = 1; n < 200 && t > 1e-17 * s; n++) { t *= 2 * x2 / (2 * n + 1); s += t; }
    return 1 - 1.1283791670955126 * m_exp(-x2) * s;
  }
  double f = x, C = x, D = 0;                         // Lentz: x + (1/2)/(x + 1/(x + (3/2)/(x + ...)))
  for (int n = 1; n < 300; n++) {
    double an = n * 0.5;
    D = x + an * D; D = 1 / D;
    C = x + an / C;
    double dl = C * D;
    f *= dl;
    if (m_fabs(dl - 1) < 1e-16) break;
  }
  return m_exp(-x * x) / (f * 1.7724538509055160);
}
static double ncdf(double x) {                         // P(Z < x)
  if (m_isnan(x)) return x;
  double e = erfc_pos(m_fabs(x) * 0.7071067811865476) * 0.5;
  return x < 0 ? e : 1 - e;
}
static double ninv(double p) {                         // Acklam's start, then Halley steps
  if (!(p > 0 && p < 1)) return m_nan();
  static const double A[6] = { -3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
                               1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00 };
  static const double B[5] = { -5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
                               6.680131188771972e+01, -1.328068155288572e+01 };
  static const double C[6] = { -7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
                               -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00 };
  static const double D[4] = { 7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00, 3.754408661907416e+00 };
  double q = p < 0.5 ? p : 1 - p, x;
  if (q < 0.02425) {
    double t = m_sqrt(-2 * m_log(q));
    x = (((((C[0] * t + C[1]) * t + C[2]) * t + C[3]) * t + C[4]) * t + C[5]) / ((((D[0] * t + D[1]) * t + D[2]) * t + D[3]) * t + 1);
    if (p > 0.5) x = -x;
  } else {
    double u = p - 0.5, r = u * u;
    x = (((((A[0] * r + A[1]) * r + A[2]) * r + A[3]) * r + A[4]) * r + A[5]) * u /
        (((((B[0] * r + B[1]) * r + B[2]) * r + B[3]) * r + B[4]) * r + 1);
  }
  for (int it = 0; it < 3; it++) {                     // Halley on P(Z < x) = p, from the nearer tail
    double t = erfc_pos(m_fabs(x) * 0.7071067811865476) * 0.5;   // P(Z > |x|)
    double e = x < 0 ? t - p : (1 - p) - t;             // P(Z < x) - p
    double d = m_exp(-0.5 * x * x) * 0.3989422804014327;
    if (d == 0) break;
    double u = e / d;
    x -= u / (1 + x * u / 2);
  }
  return x;
}

// ---------------------------------------------------------------- parser

typedef struct {
  const uint8_t *t;
  int n, i, depth;
  const CalcVars *v;
  bool err;
  uint32_t rng;                 // Ran#: from v->seed, a new number each time it's used
} P;
static double ran(P *p) {       // xorshift32: 0 <= r < 1
  p->rng ^= p->rng << 13; p->rng ^= p->rng >> 17; p->rng ^= p->rng << 5;
  return p->rng / 4294967296.0;
}

#define MAX_DEPTH 14
static int peek(P *p) { return p->i < p->n ? p->t[p->i] : -1; }
static Cx fail(P *p) { p->err = true; return cre(0); }
static bool is_digit(int k) { return k >= '0' && k <= '9'; }
static bool starts_primary(int k) {
  return is_digit(k) || k == '.' || k == T_EE || k == '(' || calc_is_func(k) || k == T_PI || k == T_E ||
         k == T_X || k == T_Y || k == T_Z || k == T_ANS || k == T_I || k == T_RAN || (k >= T_MA && k <= T_MF);
}
static Cx p_expr(P *p);
static Cx p_unary(P *p);

static Cx p_group(P *p) {
  if (++p->depth > MAX_DEPTH) return fail(p);
  Cx v = p_expr(p);
  int k = peek(p);
  if (k == ')') p->i++;
  else if (k != -1) fail(p);
  p->depth--;
  return v;
}

static double real_of(P *p, Cx z) { if (z.im != 0) p->err = true; return z.re; }

static Cx p_sum(P *p, bool prod) {                   // sum(f, a, b) with f complex
  if (++p->depth > MAX_DEPTH) return fail(p);
  int body = p->i;
  p_expr(p);
  if (p->err || peek(p) != ',') return fail(p);
  p->i++;
  double a = calc_round_sig(real_of(p, p_expr(p)), 12);
  if (p->err || peek(p) != ',') return fail(p);
  p->i++;
  double b = calc_round_sig(real_of(p, p_expr(p)), 12);
  if (peek(p) == ')') p->i++;
  else if (peek(p) != -1) return fail(p);
  if (p->err) return cre(0);
  int end = p->i;
  p->depth--;
  if (a != m_floor(a) || b != m_floor(b) || b - a > 10000) return cnan();
  CalcVars w = *p->v;
  P q = *p;
  q.v = &w;
  Cx r = cre(prod ? 1 : 0);
  for (double k = a; k <= b; k++) {
    w.x = k;
    q.i = body;
    q.depth = p->depth + 1;
    Cx f = p_expr(&q);
    if (q.err) return fail(p);
    r = prod ? cmul(r, f) : cadd(r, f);
  }
  p->i = end;
  return r;
}

static Cx p_primary(P *p) {
  int k = peek(p);
  if (k < 0) return fail(p);
  if (is_digit(k) || k == '.' || k == T_EE) {
    double d;
    if (!calc_read_number(p->t, p->n, &p->i, &d)) return fail(p);
    return cre(d);
  }
  p->i++;
  switch (k) {
    case '(': return p_group(p);
    case T_PI: return cre(M_PI_D);
    case T_E: return cre(M_E_D);
    case T_X: return cre(p->v->x);
    case T_Y: return cre(p->v->y);
    case T_Z: return cre(p->v->z);
    case T_RAN: return cre(ran(p));
    case T_ANS: return cx(p->v->ans, p->v->ans_im);
    case T_I: return ci();
  }
  if (k >= T_MA && k <= T_MF) return cre(p->v->m[k - T_MA]);
  if (k == T_SUM || k == T_PROD) return p_sum(p, k == T_PROD);
  if (k == T_GCD || k == T_LCM || k == T_LOGB || k == T_ROOT || k == T_RANINT || (k >= T_AND && k <= T_SHR)) {
    if (++p->depth > MAX_DEPTH) return fail(p);
    Cx a = p_expr(p);
    if (peek(p) != ',') return fail(p);
    p->i++;
    Cx b = p_expr(p);
    if (peek(p) == ')') p->i++;
    else if (peek(p) != -1) fail(p);
    p->depth--;
    if (k == T_LOGB) return cdiv(clog_(b), clog_(a));           // logb(base, z)
    if (k == T_ROOT) {                                           // root(n, z)
      if (a.im == 0 && b.im == 0) { double r = calc_apply(T_ROOT, a.re, b.re, false); if (m_isfinite(r)) return cre(r); }
      return cpow_(b, cdiv(cre(1), a));
    }
    if (a.im != 0 || b.im != 0) return cnan();                   // whole-number functions: real only
    if (k == T_RANINT) {                                         // a..b, each as likely
      double lo = calc_round_sig(a.re, 12), hi = calc_round_sig(b.re, 12);
      if (lo != m_floor(lo) || hi != m_floor(hi) || hi < lo || hi - lo > 4e9) return cnan();
      return cre(lo + m_floor(ran(p) * (hi - lo + 1)));
    }
    return cre(calc_apply(k, a.re, b.re, false));
  }
  if (calc_is_func(k)) return apply(k, p_group(p), p->v->deg);
  return fail(p);
}

static Cx p_postfix(P *p) {
  Cx v = p_primary(p);
  int k;
  while ((k = peek(p)) >= 0 && k >= T_SQ && k <= T_FACT) {
    p->i++;
    switch (k) {
      case T_SQ: v = cmul(v, v); break;
      case T_CUBE: v = cmul(cmul(v, v), v); break;
      case T_RECIP: v = cdiv(cre(1), v); break;
      case T_PCT: v = cx(v.re / 100, v.im / 100); break;
      case T_FACT: v = v.im == 0 ? cre(m_fact(calc_round_sig(v.re, 12))) : cnan(); break;
    }
  }
  return v;
}

static Cx p_power(P *p) {
  Cx b = p_postfix(p);
  if (peek(p) != '^') return b;
  p->i++;
  Cx e = p_unary(p);
  if (b.im == 0 && e.im == 0) {                      // real when it can be: (-8)^(1/3) = -2
    double r = m_pow(b.re, e.re);
    if (m_isfinite(r) && !(b.re == 0 && e.re < 0)) return cre(r);
  }
  return cpow_(b, e);
}

static Cx p_unary(P *p) {
  int k = peek(p);
  if (k == '-' || k == '+') {
    p->i++;
    if (++p->depth > MAX_DEPTH) return fail(p);
    Cx v = p_unary(p);
    p->depth--;
    return k == '-' ? cx(-v.re, -v.im) : v;
  }
  return p_power(p);
}

static Cx p_comb(P *p) {
  Cx v = p_unary(p);
  int k;
  while (!p->err && ((k = peek(p)) == T_NCR || k == T_NPR)) {
    p->i++;
    Cx r = p_unary(p);
    v = (v.im == 0 && r.im == 0) ? cre(calc_apply(k, v.re, r.re, false)) : cnan();
  }
  return v;
}

static Cx p_term(P *p) {
  Cx v = p_comb(p);
  for (;;) {
    int k = peek(p);
    if (k == '*') { p->i++; v = cmul(v, p_comb(p)); }
    else if (k == '/') { p->i++; v = cdiv(v, p_comb(p)); }
    else if (k == T_MOD) {
      p->i++;
      Cx r = p_comb(p);
      v = (v.im == 0 && r.im == 0) ? cre(calc_apply(T_MOD, v.re, r.re, false)) : cnan();
    } else if (k >= 0 && starts_primary(k)) v = cmul(v, p_power(p));   // 2i, 3(1+i)
    else return v;
    if (p->err) return v;
  }
}

static Cx p_expr(P *p) {
  Cx v = p_term(p);
  for (;;) {
    int k = peek(p);
    if (k != '+' && k != '-') return v;
    p->i++;
    Cx r = p_term(p);
    v = k == '+' ? cadd(v, r) : csub(v, r);
    if (p->err) return v;
  }
}

// Parts far below the other one are rounding left-overs: e^(i pi) is -1, not -1 + 1.2e-16i.
static double tidy(double a, double other) { return m_fabs(a) < 1e-14 * m_fabs(other) ? 0.0 : a; }

CalcErr calc_ceval(const TokBuf *b, const CalcVars *v, double *re, double *im) {
  if (!b->n) return CALC_ERR_SYNTAX;
  for (int i = 0; i < b->n; i++) if (b->t[i] == '=') return CALC_ERR_EQ;
  P p = { b->t, b->n, 0, 0, v, false, v->seed ? v->seed : 1 };
  Cx z = p_expr(&p);
  if (p.err || p.i != p.n) return CALC_ERR_SYNTAX;
  if (!m_isfinite(z.re) || !m_isfinite(z.im)) return CALC_ERR_MATH;
  double r = tidy(z.re, z.im), i = tidy(z.im, z.re);
  *re = r == 0 ? 0.0 : r;
  *im = i == 0 ? 0.0 : i;
  return CALC_OK;
}
