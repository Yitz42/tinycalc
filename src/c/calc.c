// calc.c - see calc.h
#include "calc.h"
#include "mmath.h"
#include "cas.h"
#include <stddef.h>
#include <stdlib.h>

// ---------------------------------------------------------------- strings

static void s_cpy(char *d, int len, const char *s) {
  int i = 0;
  if (len <= 0) return;
  for (; s[i] && i < len - 1; i++) d[i] = s[i];
  d[i] = 0;
}
static void s_cat(char *d, int len, const char *s) {
  int i = 0;
  while (i < len && d[i]) i++;
  if (i >= len) return;
  s_cpy(d + i, len - i, s);
}
static int s_len(const char *s) { int i = 0; while (s[i]) i++; return i; }

// ---------------------------------------------------------------- tokens

static bool is_digit(int k) { return k >= '0' && k <= '9'; }
static bool is_func(int k) { return (k >= T_SIN && k <= T_ABS) || (k >= T_SINH && k <= T_LCM) || (k >= T_LOGB && k <= T_PROD) || (k >= T_AND && k <= T_ARG) || (k >= T_RANINT && k <= T_NINV); }
static bool is_mem(int k) { return k >= T_MA && k <= T_MF; }
bool calc_is_func(int k) { return is_func(k); }
static bool is_postfix(int k) { return k >= T_SQ && k <= T_FACT; }

const char *calc_tok_text(uint8_t tok) {
  static char one[2];
  switch (tok) {
    // ASCII only: the watch's system fonts have no glyphs for x-sign, divide, superscripts
    case '*': return "*";
    case '/': return "/";
    case T_EE: return "E";
    case T_PI: return "pi";
    case T_E: return "e";
    case T_X: return "x";
    case T_ANS: return "Ans";
    case T_SQ: return "^2";
    case T_CUBE: return "^3";
    case T_RECIP: return "^-1";
    case T_PCT: return "%";
    case T_FACT: return "!";
    case T_SIN: return "sin(";
    case T_COS: return "cos(";
    case T_TAN: return "tan(";
    case T_ASIN: return "asin(";
    case T_ACOS: return "acos(";
    case T_ATAN: return "atan(";
    case T_LN: return "ln(";
    case T_LOG: return "log(";
    case T_EXP: return "e^(";
    case T_TENX: return "10^(";
    case T_SQRT: return "sqrt(";
    case T_CBRT: return "cbrt(";
    case T_ABS: return "abs(";
    case T_Y: return "y";
    case T_SEP: return " ; ";
    case T_NL: return " ; ";
    case T_COMMA: return ", ";
    case T_I: return "i";
    case T_CUR: return "|";
    case T_SINH: return "sinh(";
    case T_COSH: return "cosh(";
    case T_TANH: return "tanh(";
    case T_ASINH: return "asinh(";
    case T_ACOSH: return "acosh(";
    case T_ATANH: return "atanh(";
    case T_ROUND: return "round(";
    case T_FLOOR: return "floor(";
    case T_GCD: return "gcd(";
    case T_LCM: return "lcm(";
    case T_NCR: return " nCr ";
    case T_NPR: return " nPr ";
    case T_MOD: return " mod ";
    case T_LOGB: return "logb(";
    case T_ROOT: return "root(";
    case T_SUM: return "sum(";
    case T_PROD: return "prod(";
    case T_AND: return "and(";
    case T_OR: return "or(";
    case T_XOR: return "xor(";
    case T_SHL: return "shl(";
    case T_SHR: return "shr(";
    case T_NOT: return "not(";
    case T_CONJ: return "conj(";
    case T_RE: return "re(";
    case T_IM: return "im(";
    case T_ARG: return "arg(";
    case T_Z: return "z";
    case T_RAN: return "Ran#";
    case T_RANINT: return "RanInt(";
    case T_NPDF: return "normpdf(";
    case T_NCDF: return "normcdf(";
    case T_NINV: return "invnorm(";
    default:
      if (is_mem(tok)) { one[0] = (char)('A' + tok - T_MA); one[1] = 0; return one; }
      one[0] = (char)tok; one[1] = 0;
      return one;
  }
}

static bool ends_alpha(uint8_t t) { return t == T_RAN || t == T_PI || t == T_E || t == T_X || t == T_Y || t == T_Z || t == T_ANS || is_mem(t) || t == T_I; }

void calc_render(const TokBuf *b, char *buf, int len) { calc_render_cur(b, CALC_CUR_END, buf, len); }

void calc_render_cur(const TokBuf *b, int cur, char *buf, int len) {
  buf[0] = 0;
  for (int i = 0; i <= b->n; i++) {
    if (i == cur) s_cat(buf, len, "|");
    if (i == b->n) break;
    const char *txt = calc_tok_text(b->t[i]);
    char c0 = txt[0];
    bool alnum = (c0 >= 'a' && c0 <= 'z') || (c0 >= 'A' && c0 <= 'Z') || (c0 >= '0' && c0 <= '9');
    // "pi" then "cbrt(" reads as "picbrt(": show the implied product.
    if (i > 0 && ends_alpha(b->t[i - 1]) && alnum) s_cat(buf, len, "*");
    s_cat(buf, len, txt);
  }
}

// x as a variable: not counting the x a sum or product runs over (its first argument).
bool calc_has_x(const TokBuf *b) {
  for (int i = 0; i < b->n; i++) {
    if (b->t[i] == T_X) return true;
    if (b->t[i] == T_SUM || b->t[i] == T_PROD)          // skip to the comma after the body
      for (int depth = 0; i + 1 < b->n; i++) {
        int k = b->t[i + 1];
        if (k == '(' || is_func(k)) depth++;
        else if (k == ')') { if (depth-- == 0) break; }
        else if (k == ',' && depth == 0) break;
      }
  }
  return false;
}
bool calc_has_y(const TokBuf *b) {
  for (int i = 0; i < b->n; i++) if (b->t[i] == T_Y) return true;
  return false;
}
bool calc_has_z(const TokBuf *b) {
  for (int i = 0; i < b->n; i++) if (b->t[i] == T_Z) return true;
  return false;
}
bool calc_has_eq(const TokBuf *b) {
  for (int i = 0; i < b->n; i++) if (b->t[i] == '=') return true;
  return false;
}

const char *calc_err_text(CalcErr e) {
  switch (e) {
    case CALC_ERR_SYNTAX: return "Syntax error";
    case CALC_ERR_MATH: return "Math error";
    case CALC_ERR_EQ: return "No = allowed here";
    case CALC_ERR_NOX: return "No x to solve";
    case CALC_ERR_NOROOT: return "No root found";
    case CALC_ERR_NOSOL: return "No solution found";
    case CALC_ERR_SYM: return "No formula found";
    case CALC_ERR_LIN3: return "3 unknowns: linear only";
    case CALC_ERR_INT: return "Integral won't settle";
    default: return "";
  }
}

// ---------------------------------------------------------------- parser

#define MAX_DEPTH 14

typedef struct {
  const uint8_t *t;
  int n, i, depth;
  const CalcVars *v;
  bool err;
} P;

static int peek(P *p) { return p->i < p->n ? p->t[p->i] : -1; }

static bool starts_primary(int k) {
  return is_digit(k) || k == '.' || k == T_EE || k == '(' || is_func(k) ||
         k == T_PI || k == T_E || k == T_X || k == T_Y || k == T_Z || k == T_ANS || is_mem(k) || k == T_I || k == T_RAN;
}

static double p_expr(P *p);
static double p_unary(P *p);

static double fail(P *p) { p->err = true; return 0.0; }

static double p_number(P *p) {
  uint64_t mant = 0;
  int dexp = 0, k;
  bool dot = false, any = false;
  while ((k = peek(p)) >= 0 && (is_digit(k) || k == '.')) {
    p->i++;
    if (k == '.') {
      if (dot) return fail(p);
      dot = true;
      continue;
    }
    any = true;
    if (mant < 100000000000000000ULL) {
      mant = mant * 10 + (uint64_t)(k - '0');
      if (dot) dexp--;
    } else if (!dot) {
      dexp++;
    }
  }
  if (peek(p) == T_EE) {
    p->i++;
    if (!any) { mant = 1; any = true; }
    bool neg = false;
    if (peek(p) == '-') { neg = true; p->i++; }
    else if (peek(p) == '+') { p->i++; }
    int e = 0;
    bool ed = false;
    while ((k = peek(p)) >= 0 && is_digit(k)) {
      p->i++;
      ed = true;
      if (e < 10000) e = e * 10 + (k - '0');
    }
    if (!ed) return fail(p);
    dexp += neg ? -e : e;
  }
  if (!any) return fail(p);
  double m = (double)mant;
  if (mant == 0) return 0.0;
  if (dexp < 0 && dexp >= -22) return m / m_pow10i(-dexp);
  if (dexp < -300) return (m * m_pow10i(-300)) * m_pow10i(dexp + 300);
  if (dexp > 300) return (m * m_pow10i(300)) * m_pow10i(dexp - 300);
  return m * m_pow10i(dexp);
}

static double deg_trig(int f, double a, bool deg) {
  if (deg) {
    double r = m_fmod(a, 360.0);                       // exact, however big a is
    if (r < 0) r += 360.0;
    if (a == m_floor(a) && r == m_floor(r) && (int)r % 15 == 0) {   // the angles with exact values (-1E-15 + 360 rounds to 360: not one)
      int k = (int)r;
      if (f == T_TAN) {
        if (k % 180 == 90) return m_nan();
        if (k % 180 == 0) return 0;
        if (k % 90 == 45) return (k % 180 == 45) ? 1 : -1;
      } else {
        int s = f == T_SIN ? k : (k + 90) % 360;      // cos t = sin(t + 90)
        int sg = s >= 180 ? -1 : 1, t = s % 180;
        if (t > 90) t = 180 - t;
        if (t == 0) return 0;
        if (t == 90) return sg;
        if (t == 30) return sg * 0.5;
      }
    }
    // in degrees, from the nearest multiple of 90 (exact), so angles close
    // to one keep their digits (tan 89.99999, sin 179.9999999); a d that is
    // a short decimal off by the input's own rounding is that decimal
    double q = m_round(r / 90), d = r - 90 * q, dd = m_round(d * 1e10) / 1e10;
    if (m_fabs(a) < 45) { q = 0; d = a; }               // small angles as typed (sin 1E-13 is not 0; r += 360 would round -1E-13)
    else if (m_fabs(d - dd) <= 4e-16 * (m_fabs(r) > m_fabs(a) ? m_fabs(r) : m_fabs(a))) d = dd;   // (by the input's own rounding)
    double t = d * (M_PI_D / 180.0), sn = m_sin(t), cs = m_cos(t);
    int qi = (int)q & 3;
    if (f == T_TAN) return (qi & 1) ? (sn == 0 ? m_nan() : -cs / sn) : sn / cs;
    if (f == T_COS) qi = (qi + 1) & 3;                 // cos r = sin(r + 90)
    return qi == 0 ? sn : qi == 1 ? cs : qi == 2 ? -sn : -cs;
  }
  double r = f == T_SIN ? m_sin(a) : f == T_COS ? m_cos(a) : m_tan(a);
  // sin(pi) is 1.2e-16 only because pi is stored rounded: below the argument's
  // own rounding, call it 0 (but sin(1E15+1) = 0.0319 is real)
  if (m_fabs(r) < 4e-16 * m_fabs(a) && m_fabs(r) < 1e-11) r = 0;
  // and tan right at pi/2 (as close as doubles get) is the pole, not 1.6e16
  if (f == T_TAN) {                                    // (11 pi/2 too: cos there is 0 by the same rule as sin pi)
    double c = m_cos(a);
    if (m_fabs(c) < 4e-16 * m_fabs(a) && m_fabs(c) < 1e-11) r = m_nan();
  }
  return r;
}

// Ran#'s seed: a new one for each answer (the complex evaluator, which
// works Ran# and the probability functions out, takes it from CalcVars)
uint32_t calc_rng = 2463534242u;
static uint32_t next_seed(void) {
  calc_rng ^= calc_rng << 13; calc_rng ^= calc_rng >> 17; calc_rng ^= calc_rng << 5;
  return calc_rng;
}

static bool is_int(double a) { return m_isfinite(a) && a == m_floor(a); }
// What the display shows: 7.9999999999999991 is 8 for floor, mod, n!, nCr...
static double snap(double a) {
  if (is_int(a)) return a;                             // 1E15+1 stays itself
  double s = calc_round_sig(a, 12);
  return m_fabs(s - a) <= 1e-12 * m_fabs(a) ? s : a;
}
static double gcd_d(double a, double b) {
  a = m_fabs(a); b = m_fabs(b);
  while (b > 0.5) { double t = a - b * m_floor(a / b); a = b; b = t; }
  return a;
}
static double perm(double n, double r, bool comb) {   // nPr, nCr
  n = snap(n); r = snap(r);
  if (!is_int(n) || !is_int(r) || r < 0 || n < 0) return m_nan();
  if (r > n) return 0;                                 // no way to choose more than there are
  if (comb && r > n - r) r = n - r;
  double v = 1;
  for (double i = 1; i <= r; i++) {
    if (comb && v > 1e300 / (n - r + i)) v = v / i * (n - r + i);   // near the top: divide first (1029 nCr 514)
    else { v *= (n - r + i); if (comb) v /= i; }
    if (!m_isfinite(v)) return m_inf();
  }
  return comb ? m_round(v) : v;
}

static double apply_func(int f, double a, bool deg) {
  double r;
  switch (f) {
    case T_SINH: case T_COSH: case T_TANH: {
      double b = m_fabs(a);
      if (f == T_TANH) {
        if (b > 20) r = 1;
        else { double e = m_expm1(2 * b); r = e / (e + 2); }
      } else if (b > 709) {                    // e^b overflows before half of it does
        double h = m_exp(b - 0.6931471805599453);
        return (f == T_SINH && a < 0) ? -h : h;
      } else {
        double e = m_expm1(b), u = e + 1;      // sinh = (e + e/u) / 2 stays exact near 0
        if (f == T_COSH) return (u + 1 / u) / 2;
        r = (e + e / u) / 2;
      }
      return a < 0 ? -r : r;
    }
    case T_ASINH: case T_ACOSH: {
      double b = m_fabs(a);
      if (f == T_ACOSH && a < 1) return m_nan();
      if (b > 1e8) r = m_log(b) + 0.6931471805599453;   // a^2 would overflow
      else if (f == T_ACOSH) return m_log(a + m_sqrt((a - 1) * (a + 1)));
      else r = m_log1p(b + b * b / (1 + m_sqrt(b * b + 1)));
      return a < 0 ? -r : r;
    }
    case T_ATANH: return (a <= -1 || a >= 1) ? m_nan() : 0.5 * m_log1p(2 * a / (1 - a));
    case T_ROUND: return m_round(snap(a));
    case T_CONJ: case T_RE: return a;                 // of a real number
    case T_IM: return 0;
    case T_ARG: r = a < 0 ? M_PI_D : 0; break;        // in the angle unit (below)
    case T_NOT: a = snap(a); return (is_int(a) && m_fabs(a) < 9e15) ? -a - 1 : m_nan();
    case T_FLOOR: return m_floor(snap(a));
    case T_SIN: case T_COS: case T_TAN: return deg_trig(f, a, deg);
    case T_ASIN: r = m_asin(a); break;
    case T_ACOS: r = m_acos(a); break;
    case T_ATAN: r = m_atan(a); break;
    case T_LN: return a <= 0 ? m_nan() : m_log(a);
    case T_LOG: return a <= 0 ? m_nan() : m_log10(a);
    case T_EXP: return m_exp(a);
    case T_TENX:
      if (a == m_floor(a) && m_fabs(a) <= 308) return m_pow10i((int)a);
      return m_pow(10.0, a);
    case T_SQRT: return m_sqrt(a);
    case T_CBRT: return m_cbrt(a);
    case T_ABS: return m_fabs(a);
    default: return m_nan();
  }
  if (deg) {                                           // asin 0.5 is 30, not 29.999999999999996
    r *= 180.0 / M_PI_D;
    double k = m_round(r);
    if (k != 0 && m_fabs(r - k) <= 1e-13 * (m_fabs(r) > 1 ? m_fabs(r) : 1)) r = k;   // (not to 0: asin 1E-15 is 5.7E-14 deg)
  }
  return r;
}

double calc_apply(int f, double a, double b, bool deg) {
  switch (f) {
    case T_GCD: case T_LCM: {
      a = snap(a); b = snap(b);
      if (!is_int(a) || !is_int(b)) return m_nan();
      double g = gcd_d(a, b);
      if (f == T_GCD) return g;
      return g == 0 ? 0 : m_fabs(a / g * b);
    }
    case T_LOGB:                                      // logb(base, x)
      return (a <= 0 || a == 1 || b <= 0) ? m_nan() : m_log(b) / m_log(a);
    case T_ROOT: {                                    // root(n, x): x^(1/n), real for odd n
      if (a == 0) return m_nan();
      double n = snap(a);
      if (b < 0 && is_int(n) && m_fmod(n, 2) != 0) return -m_pow(-b, 1 / n);
      double r = m_pow(b, 1 / n), rr = m_round(r);
      if (is_int(n) && m_fabs(n) <= 64 && m_pow(rr, n) == b) r = rr;   // exact roots stay exact
      return r;
    }
    case T_AND: case T_OR: case T_XOR: case T_SHL: case T_SHR: {   // on whole numbers (two's complement)
      a = snap(a); b = snap(b);
      if (!is_int(a) || !is_int(b) || m_fabs(a) > 9e15 || m_fabs(b) > 9e15) return m_nan();
      int64_t x = (int64_t)a, y = (int64_t)b;
      if (f == T_SHL || f == T_SHR) {
        if (y < 0 || y > 52) return m_nan();
        double r = f == T_SHL ? a * (double)((int64_t)1 << y) : (double)(x >> y);
        return m_fabs(r) > 9e15 ? m_nan() : r;
      }
      return (double)(f == T_AND ? (x & y) : f == T_OR ? (x | y) : (x ^ y));
    }
    case T_NCR: return perm(a, b, true);
    case T_NPR: return perm(a, b, false);
    case T_MOD: {
      a = snap(a); b = snap(b);
      double m = m_fmod(a, b);
      // 0.7 mod 0.1: the remainder of the stored values is 0.09999999999999995
      double t = (is_int(a) && is_int(b)) ? 0 : 1e-12 * m_fabs(b) + 1e-15 * m_fabs(a);   // whole numbers: exact
      if (m_fabs(m) <= t || m_fabs(m_fabs(m) - m_fabs(b)) <= t) return 0;
      return m;
    }
    case '*': return a * b;
    case '/': return b == 0 ? m_nan() : a / b;
    default: return apply_func(f, a, deg);
  }
}

// Parse a parenthesised body: expr, then ')' or end of input.
static double p_group(P *p) {
  if (++p->depth > MAX_DEPTH) return fail(p);
  double v = p_expr(p);
  int k = peek(p);
  if (k == ')') p->i++;
  else if (k != -1 && k != '=') fail(p);   // auto-close at end or at '='

  p->depth--;
  return v;
}

// sum(f, a, b) / prod(f, a, b): f evaluated at x = a, a + 1, ..., b. The
// body is parsed once to find its end, then again for each x.
#define SUM_MAX 10000
static double p_sum(P *p, bool prod) {
  if (++p->depth > MAX_DEPTH) return fail(p);
  int body = p->i;
  p_expr(p);
  if (p->err || peek(p) != ',') return fail(p);
  p->i++;
  double a = p_expr(p);
  if (p->err || peek(p) != ',') return fail(p);
  p->i++;
  double b = p_expr(p);
  if (peek(p) == ')') p->i++;
  else if (peek(p) != -1 && peek(p) != '=') return fail(p);
  if (p->err) return 0;
  int end = p->i;
  p->depth--;
  a = snap(a); b = snap(b);
  if (!is_int(a) || !is_int(b) || b - a > SUM_MAX) return m_nan();
  CalcVars w = *p->v;
  P q = *p;
  q.v = &w;
  double r = prod ? 1 : 0;
  for (double k = a; k <= b; k++) {
    w.x = k;
    q.i = body;
    q.depth = p->depth + 1;
    double f = p_expr(&q);
    if (q.err) return fail(p);
    r = prod ? r * f : r + f;
  }
  p->i = end;
  return r;
}

bool calc_read_number(const uint8_t *t, int n, int *i, double *out) {
  P p = { t, n, *i, 0, NULL, false };
  *out = p_number(&p);
  *i = p.i;
  return !p.err;
}

static double p_primary(P *p) {
  int k = peek(p);
  if (k < 0) return fail(p);
  if (is_digit(k) || k == '.' || k == T_EE) return p_number(p);
  p->i++;
  switch (k) {
    case '(': return p_group(p);
    case T_PI: return M_PI_D;
    case T_E: return M_E_D;
    case T_X: return p->v->x;
    case T_Y: return p->v->y;
    case T_Z: return p->v->z;
    case T_RAN: return m_nan();                        // (worked out by the complex evaluator)
    case T_ANS: return p->v->ans;
    default: break;
  }
  if (is_mem(k)) return p->v->m[k - T_MA];
  if (k == T_I) return m_nan();                      // no real value: the complex evaluator takes over
  if (k == T_SUM || k == T_PROD) return p_sum(p, k == T_PROD);
  if (k == T_GCD || k == T_LCM || k == T_LOGB || k == T_ROOT || k == T_RANINT || (k >= T_AND && k <= T_SHR)) {   // two arguments
    if (++p->depth > MAX_DEPTH) return fail(p);
    double a = p_expr(p);
    if (peek(p) != ',') return fail(p);
    p->i++;
    double b = p_expr(p);
    if (peek(p) == ')') p->i++;
    else if (peek(p) != -1 && peek(p) != '=') fail(p);
    p->depth--;
    return calc_apply(k, a, b, false);
  }
  if (is_func(k)) return apply_func(k, p_group(p), p->v->deg);
  return fail(p);
}

static double p_postfix(P *p) {
  double v = p_primary(p);
  int k;
  while ((k = peek(p)) >= 0 && is_postfix(k)) {
    p->i++;
    switch (k) {
      case T_SQ: v = v * v; break;
      case T_CUBE: v = v * v * v; break;
      case T_RECIP: v = 1.0 / v; break;
      case T_PCT: v = v / 100.0; break;
      case T_FACT: v = m_fact(snap(v)); break;
    }
  }
  return v;
}

static double p_power(P *p) {
  double b = p_postfix(p);
  if (peek(p) == '^') {
    p->i++;
    double e = p_unary(p);
    if (b == 0 && e < 0) return m_nan();
    return m_pow(b, e);
  }
  return b;
}

static double p_unary(P *p) {
  int k = peek(p);
  if (k == '-' || k == '+') {
    p->i++;
    if (++p->depth > MAX_DEPTH) return fail(p);
    double v = p_unary(p);
    p->depth--;
    return k == '-' ? -v : v;
  }
  return p_power(p);
}

// nCr and nPr bind tighter than * and /: 2*5 nCr 2 is 2*(5 nCr 2)
static double p_comb(P *p) {
  double v = p_unary(p);
  int k;
  while (!p->err && ((k = peek(p)) == T_NCR || k == T_NPR)) {
    p->i++;
    v = calc_apply(k, v, p_unary(p), false);
  }
  return v;
}

static double p_term(P *p) {
  double v = p_comb(p);
  for (;;) {
    int k = peek(p);
    if (k == '*' || k == '/' || k == T_MOD) {
      p->i++;
      double r = p_comb(p);
      v = calc_apply(k, v, r, false);
    } else if (k >= 0 && starts_primary(k)) {
      v *= p_power(p);   // implicit multiplication: 2pi, 3(4), 2x
    } else {
      return v;
    }
    if (p->err) return v;
  }
}

static double p_expr(P *p) {
  double v = p_term(p);
  for (;;) {
    int k = peek(p);
    if (k != '+' && k != '-') return v;
    p->i++;
    double r = p_term(p);
    v = (k == '+') ? v + r : v - r;
    if (p->err) return v;
  }
}

// Evaluates "lhs" or "lhs = rhs". rhs is 0 when there is no '='.
CalcErr calc_eval_eq(const TokBuf *b, const CalcVars *v, double *lhs, double *rhs) {
  P p = { b->t, b->n, 0, 0, v, false };
  if (b->n == 0) return CALC_ERR_SYNTAX;
  *rhs = 0.0;
  *lhs = p_expr(&p);
  if (!p.err && peek(&p) == '=') {
    p.i++;
    *rhs = p_expr(&p);
  }
  if (p.err || p.i != p.n) return CALC_ERR_SYNTAX;
  if (!m_isfinite(*lhs) || !m_isfinite(*rhs)) return CALC_ERR_MATH;
  return CALC_OK;
}

CalcErr calc_eval(const TokBuf *b, const CalcVars *v, double *out) {
  if (calc_has_eq(b)) {
    // still report syntax errors first
    double l, r;
    CalcErr e = calc_eval_eq(b, v, &l, &r);
    return e == CALC_ERR_SYNTAX ? e : CALC_ERR_EQ;
  }
  double l, r;
  CalcErr e = calc_eval_eq(b, v, &l, &r);
  if (e != CALC_OK) return e;
  if (l == 0) l = 0.0;  // no "-0"
  *out = l;
  return CALC_OK;
}

double calc_round_sig(double x, int sig) {
  if (x == 0 || !m_isfinite(x)) return x;
  int e = (int)m_floor(m_log10(m_fabs(x)));
  int k = sig - 1 - e;
  if (k > 300 || k < -300) return x;
  return k >= 0 ? m_round(x * m_pow10i(k)) / m_pow10i(k)
                : m_round(x / m_pow10i(-k)) * m_pow10i(-k);
}

// ---------------------------------------------------------------- all roots

bool calc_sides(const TokBuf *b, const CalcVars *v, double x, double *lhs, double *rhs) {
  CalcVars w = *v;
  w.x = x;
  return calc_eval_eq(b, &w, lhs, rhs) == CALC_OK;
}

void calc_format_cplx(double re, double im, char *buf, int len) {
  if (im == 0) { calc_format_answer(re, buf, len); return; }
  char a[32], b[32];
  int o = 0;
  bool neg = im < 0;
  // both parts on one line: 7 digits each in the normal format, so it fits
  bool both = re != 0 && calc_fmt == FMT_NORM;
  if (both) calc_format(neg ? -im : im, 7, b, sizeof b); else calc_format_answer(neg ? -im : im, b, sizeof b);
  if (re != 0) {
    if (both) calc_format(re, 7, a, sizeof a); else calc_format_answer(re, a, sizeof a);
    s_cpy(buf, len, a);
    o = s_len(buf);
    s_cat(buf, len, neg ? "-" : "+");
  } else {
    s_cpy(buf, len, neg ? "-" : "");
  }
  if (!(b[0] == '1' && b[1] == 0)) s_cat(buf, len, b);   // i, not 1i
  s_cat(buf, len, "i");
  (void)o;
}

void calc_take(Calc *c, double v, const char *label) {
  if (v == 0) v = 0.0;
  c->result = v; c->v.ans = v;
  c->mode = m_isfinite(v) ? MODE_RESULT : MODE_ERROR;
  c->err = CALC_ERR_MATH;
  c->solved = c->stored = c->sys = c->sys3 = false;
  c->op = OP_NONE; c->sym = SYM_NONE; c->conv = CONV_NONE;
  s_cpy(c->label, sizeof c->label, label);
}

void calc_take_root(Calc *c, double r) {
  if (r == 0) r = 0.0;
  c->op = OP_NONE;
  c->v.x = r;
  c->v.ans = r;
  c->result = r;
  c->mode = MODE_RESULT;
  c->solved = true;
  c->stored = false;
}

void calc_take_sol2(Calc *c, double x, double y) {
  c->op = OP_NONE;
  c->v.x = x;
  c->v.y = y;
  c->v.ans = x;
  c->result = x;
  c->mode = MODE_RESULT;
  c->solved = true;
  c->stored = false;
  c->sys = true;
  c->sys3 = false;
}

// ---------------------------------------------------------------- format

// The first sig significant digits of a > 0 into dg (as characters); returns
// the decimal exponent of the first one.
static int sig_digits(double a, int sig, char *dg) {
  int e = (int)m_floor(m_log10(a));
  uint64_t lim = 1;
  for (int i = 0; i < sig; i++) lim *= 10;
  uint64_t m = 0;
  for (int tries = 0; tries < 3; tries++) {
    int k = sig - 1 - e;
    double sc;
    if (k > 300) sc = (a * m_pow10i(300)) * m_pow10i(k - 300);
    else if (k >= 0) sc = a * m_pow10i(k);
    else sc = a / m_pow10i(-k);
    m = (uint64_t)m_round(sc);
    if (m >= lim) { e++; continue; }
    if (m < lim / 10) { e--; continue; }
    break;
  }
  if (m >= lim) { m /= 10; e++; }
  for (int i = sig - 1; i >= 0; i--) { dg[i] = (char)('0' + m % 10); m /= 10; }
  dg[sig] = 0;
  return e;
}

static int put_exp(char *out, int o, int e) {
  out[o++] = 'e';
  if (e < 0) { out[o++] = '-'; e = -e; }
  char eb[6]; int en = 0;
  do { eb[en++] = (char)('0' + e % 10); e /= 10; } while (e);
  while (en) out[o++] = eb[--en];
  return o;
}

static bool format_special(double v, char *buf, int len) {
  if (m_isnan(v)) { s_cpy(buf, len, "Error"); return true; }
  if (!m_isfinite(v)) { s_cpy(buf, len, "Overflow"); return true; }
  if (v == 0) { s_cpy(buf, len, "0"); return true; }
  return false;
}

void calc_format(double v, int sig, char *buf, int len) {
  if (format_special(v, buf, len)) return;
  if (sig < 1) sig = 1;
  if (sig > 15) sig = 15;
  bool neg = v < 0;
  char dg[20];
  int e = sig_digits(neg ? -v : v, sig, dg);
  int nd = sig;
  while (nd > 1 && dg[nd - 1] == '0') nd--;  // significant digits actually needed

  char out[40];
  int o = 0;
  if (neg) out[o++] = '-';
  // plain when it fits: 0.00001 rather than 1e-5 (up to 10 digits after the point)
  if ((e >= -4 || (e >= -9 && nd - e - 1 <= 10)) && e < sig) {
    if (e >= 0) {
      for (int i = 0; i <= e; i++) out[o++] = (i < nd) ? dg[i] : '0';
      if (nd > e + 1) {
        out[o++] = '.';
        for (int i = e + 1; i < nd; i++) out[o++] = dg[i];
      }
    } else {
      out[o++] = '0'; out[o++] = '.';
      for (int i = 0; i < -e - 1; i++) out[o++] = '0';
      for (int i = 0; i < nd; i++) out[o++] = dg[i];
    }
  } else {
    out[o++] = dg[0];
    if (nd > 1) {
      out[o++] = '.';
      for (int i = 1; i < nd; i++) out[o++] = dg[i];
    }
    o = put_exp(out, o, e);
  }
  out[o] = 0;
  s_cpy(buf, len, out);
}

uint8_t calc_fmt = FMT_NORM, calc_fmt_digits = 4;

// sig digits of a, rounded half up from its 15-digit decimal form (what NORM
// shows): FIX 2 of 1.005 is 1.01 like on paper, though the double is 1.00499...
static int dec_digits(double a, int sig, char *dg) {
  char d15[20];
  int e = sig_digits(a, 15, d15);
  if (sig >= 15) { for (int i = 0; i <= 15; i++) dg[i] = i < sig ? (i < 15 ? d15[i] : '0') : 0; dg[sig] = 0; return e; }
  bool up = d15[sig] >= '5';
  for (int i = 0; i < sig; i++) dg[i] = d15[i];
  dg[sig] = 0;
  for (int i = sig - 1; up && i >= 0; i--) {
    if (dg[i] == '9') dg[i] = '0';
    else { dg[i]++; up = false; }
  }
  if (up) { dg[0] = '1'; for (int i = 1; i < sig; i++) dg[i] = '0'; e++; }   // 9.99 -> 10.0
  return e;
}

void calc_format_answer(double v, char *buf, int len) {
  int d = calc_fmt_digits > 9 ? 9 : calc_fmt_digits;
  if (calc_fmt == FMT_NORM || format_special(v, buf, len)) { calc_format(v, 10, buf, len); return; }
  bool neg = v < 0;
  double a = neg ? -v : v;
  char dg[20], out[40];
  int o = 0;
  if (neg) out[o++] = '-';
  if (calc_fmt == FMT_FIX && a * m_pow10i(d) < 1e15) {   // that many decimals (15 digits at most: they're all real)
    uint64_t m = 0;                                    // a * 10^d, rounded in decimal
    if (a > 0) {
      char d15[20];
      int e = sig_digits(a, 15, d15), k = e + 1 + d;   // digits of m
      if (k > 15) { for (int q = 0; q < 15; q++) m = m * 10 + (uint64_t)(d15[q] - '0'); for (; k > 15; k--) m *= 10; }
      else if (k <= 0) m = (k == 0 && d15[0] >= '5');  // 0.006 to 2 places: 0.01
      else {
        char dg[20];
        int e2 = dec_digits(a, k, dg);
        for (int q = 0; q < k; q++) m = m * 10 + (uint64_t)(dg[q] - '0');
        if (e2 > e) m *= 10;                           // 9.995 -> 10.00: a digit more
      }
    }
    char t[24]; int n = 0;
    do { t[n++] = (char)('0' + m % 10); m /= 10; } while (m || n <= d);
    bool zero = true;
    for (int i = 0; i < n; i++) if (t[i] != '0') zero = false;
    if (zero) o = 0;                                    // no "-0.00"
    while (n > d) out[o++] = t[--n];
    if (d) { out[o++] = '.'; while (n) out[o++] = t[--n]; }
    out[o] = 0;
    s_cpy(buf, len, out);
    return;
  }
  int sig = d + 1;                                     // SCI (and FIX when too big): d decimals
  int e = dec_digits(a, sig, dg), ip = 1;
  if (calc_fmt == FMT_ENG) {                           // exponent a multiple of 3: 1 to 3 digits before the point
    for (int pass = 0; pass < 2; pass++) {
      int e3 = e >= 0 ? e / 3 * 3 : -((-e + 2) / 3 * 3);
      ip = e - e3 + 1;
      if (sig < ip) { sig = ip; e = dec_digits(a, sig, dg); continue; }   // rounding can move e: once more
      break;
    }
    e = e >= 0 ? e / 3 * 3 : -((-e + 2) / 3 * 3);
    ip = dec_digits(a, sig, dg) - e + 1;
  }
  for (int i = 0; i < ip; i++) out[o++] = dg[i];
  if (sig > ip) { out[o++] = '.'; for (int i = ip; i < sig; i++) out[o++] = dg[i]; }
  o = put_exp(out, o, e);
  out[o] = 0;
  s_cpy(buf, len, out);
}

// ---------------------------------------------------------------- state machine

void calc_init(Calc *c) {
  c->expr.n = 0;
  c->v.x = 0;
  c->v.ans = 0;
  c->v.deg = true;
  c->result = 0;
  c->mode = MODE_EDIT;
  c->err = CALC_OK;
  c->inv = false;
  c->solved = false;
  c->stored = false;
  c->sys = c->sys3 = false;
  c->eq1.n = 0; c->eq2.n = 0;
  c->v.y = c->v.z = 0;
  c->sys3 = false;
  c->op = OP_NONE;
  c->step = 0;
  c->approx = false;
  c->lim_a = c->lim_b = 0;
  c->cur = CALC_CUR_END;
  c->sym = SYM_NONE;
  c->pend = 0;
  c->var = 0;
  c->conv = CONV_NONE;
  c->label[0] = 0;
  c->result_im = c->v.ans_im = 0;
  for (int i = 0; i < CALC_MEMS; i++) c->v.m[i] = 0;
}

static void push(TokBuf *b, uint8_t t) {
  if (b->n < CALC_MAX_TOK) b->t[b->n++] = t;
}

static bool continues_ans(int k) {
  return k == '+' || k == '-' || k == '*' || k == '/' || k == '^' || is_postfix(k);
}

static void set_error(Calc *c, CalcErr e) {
  c->mode = MODE_ERROR;
  c->err = (uint8_t)e;
}

static void start_solve(Calc *c);

static void do_eval(Calc *c) {
  if (c->expr.n == 0) return;
  if (calc_has_eq(&c->expr)) { start_solve(c); return; }   // "=" on an equation solves it
  double r, im = 0;
  c->v.seed = next_seed();
  CalcErr e = calc_eval(&c->expr, &c->v, &r);
  bool cx = false;                                     // i typed, or a complex Ans used
  for (int i = 0; i < c->expr.n; i++)
    if (c->expr.t[i] == T_I || (c->expr.t[i] == T_ANS && c->v.ans_im != 0)) cx = true;
  if (cx || e == CALC_ERR_MATH) {                      // no real answer: maybe a complex one
    CalcErr e2 = calc_ceval(&c->expr, &c->v, &r, &im);
    if (e2 == CALC_OK || cx) e = e2;
  }
  if (e != CALC_OK) { set_error(c, e); return; }
  c->result = r; c->result_im = im;
  c->v.ans = r; c->v.ans_im = im;
  c->mode = MODE_RESULT;
  c->solved = false;
  c->stored = false;
  c->sys = c->sys3 = false;
  c->op = OP_NONE;
}

static void start_solve(Calc *c) {
  if (c->expr.n == 0) return;
  if (calc_has_y(&c->expr) || calc_has_z(&c->expr)) {   // a system: ask for the second equation
    double l, r;
    if (calc_eval_eq(&c->expr, &c->v, &l, &r) == CALC_ERR_SYNTAX) { set_error(c, CALC_ERR_SYNTAX); return; }
    c->eq1 = c->expr;
    c->expr.n = 0;
    c->eq2.n = 0;
    c->sys = c->sys3 = false;
    c->mode = MODE_EQ2;
    return;
  }
  // One variable: the root nearest the current x. (The watch UI shows them
  // all on the solver sheet instead.)
  if (!calc_has_x(&c->expr)) { set_error(c, CALC_ERR_NOX); return; }
  double l, r, rs[12], x0 = c->v.x;
  if (calc_eval_eq(&c->expr, &c->v, &l, &r) == CALC_ERR_SYNTAX) { set_error(c, CALC_ERR_SYNTAX); return; }
  if (!(x0 > -1e6 && x0 < 1e6)) x0 = 0;
  for (double half = 10; half <= 1e6; half *= 10) {
    int n = calc_roots(&c->expr, &c->v, x0 - half, x0 + half, rs, 12), best = 0;
    if (n == 0) continue;
    for (int i = 1; i < n; i++) {                       // nearest; on a tie (x^2=4) the larger
      double d = m_fabs(rs[i] - x0), db = m_fabs(rs[best] - x0);
      if (d < db || (d == db && rs[i] > rs[best])) best = i;
    }
    calc_take_root(c, rs[best]);
    c->sys = c->sys3 = false;
    return;
  }
  set_error(c, CALC_ERR_NOROOT);
}

// Solve eq1 + expr in a default box and take the solution nearest the
// stored (x, y). The watch UI shows all of them on the solver sheet instead.
static void run_solve2(Calc *c) {
  double l, r;
  if (c->expr.n == 0 || calc_eval_eq(&c->expr, &c->v, &l, &r) == CALC_ERR_SYNTAX) {
    set_error(c, CALC_ERR_SYNTAX);
    return;
  }
  double xs[8], ys[8];
  int n = calc_solve2(&c->eq1, &c->expr, &c->v, -100, 100, -100, 100, xs, ys, 8);
  if (n == 0) { set_error(c, CALC_ERR_NOSOL); c->sys = true; return; }
  int best = 0;
  double bd = 1e300;
  for (int i = 0; i < n; i++) {
    double d = m_fabs(xs[i] - c->v.x) + m_fabs(ys[i] - c->v.y);
    if (d < bd) { bd = d; best = i; }
  }
  calc_take_sol2(c, xs[best], ys[best]);
}

// Three equations: x, y and z, shown together; Ans = x.
static void run_solve3(Calc *c) {
  double l, r, s[3];
  if (c->expr.n == 0 || calc_eval_eq(&c->expr, &c->v, &l, &r) == CALC_ERR_SYNTAX) { set_error(c, CALC_ERR_SYNTAX); return; }
  CalcErr e = calc_solve3(&c->eq1, &c->eq2, &c->expr, &c->v, s);
  c->sys = true;
  c->sys3 = true;
  if (e != CALC_OK) { set_error(c, e); return; }
  calc_take_sol2(c, s[0], s[1]);
  c->v.z = s[2];
  c->sys3 = true;
}

// Start an integral / derivative of f(x): the current expression, or the
// last one used if we're looking at its result.
static void start_op(Calc *c, int op) {
  TokBuf f = (c->op && (c->mode == MODE_RESULT || c->mode == MODE_ERROR)) ? c->eq1 : c->expr;
  if (c->mode == MODE_RESULT && !c->op && !f.n) return;
  if (f.n == 0) return;
  double v;
  CalcErr e = calc_eval(&f, &c->v, &v);
  if (e == CALC_ERR_SYNTAX || e == CALC_ERR_EQ) { set_error(c, e); return; }
  c->eq1 = f;
  c->expr.n = 0;
  c->sym = SYM_NONE;
  c->op = (uint8_t)op;
  c->sys = c->sys3 = false;
  c->step = op == OP_INT ? 0 : 2;
  c->mode = MODE_LIM;
}

// "=" while typing a limit / point.
// A formula as the answer: expr = out, from eq1.
static void sym_result(Calc *c, int kind, const TokBuf *in, const TokBuf *out) {
  c->eq1 = *in;
  c->expr = *out;
  c->op = OP_NONE;
  c->sys = c->sys3 = false;
  c->sym = (uint8_t)kind;
  c->mode = MODE_RESULT;
  c->solved = c->stored = false;
}

// "=" with nothing typed at "from =" or "at x =": the formula instead of a number.
static void lim_symbolic(Calc *c) {
  TokBuf out;
  bool ok = c->step == 0 ? cas_integral(&c->eq1, &c->v, &out) : cas_derivative(&c->eq1, &c->v, &out);
  if (!ok) { set_error(c, CALC_ERR_SYM); return; }
  TokBuf in = c->eq1;
  sym_result(c, c->step == 0 ? SYM_INT : SYM_DER, &in, &out);
}

// Factor / Expand what's typed (or the formula or expression of an answer).
static void sym_op(Calc *c, int key) {
  if (c->mode == MODE_EQ2) return;
  // at a d/dx or integral prompt, or on its answer: the function itself
  TokBuf in = (c->op && (c->mode == MODE_RESULT || c->mode == MODE_ERROR || c->mode == MODE_LIM)) ? c->eq1 : c->expr;
  if (!in.n) return;
  TokBuf out;
  bool ok = key == K_FACTOR ? cas_factor(&in, &c->v, &out) : cas_expand(&in, &c->v, &out);
  if (!ok) {
    c->expr = in; c->eq1.n = 0;
    c->op = OP_NONE; c->sys = c->sys3 = false; c->sym = SYM_NONE;
    set_error(c, CALC_ERR_SYM);
    return;
  }
  sym_result(c, key == K_FACTOR ? SYM_FACTOR : SYM_EXPAND, &in, &out);
}

static void lim_enter(Calc *c) {
  double v = c->v.x;
  if (c->expr.n) {
    CalcErr e = calc_eval(&c->expr, &c->v, &v);
    if (e != CALC_OK) { set_error(c, e); return; }
  } else if (c->step != 1) {
    lim_symbolic(c);                      // nothing typed: the formula
    return;
  } else {
    return;                               // the upper limit must be typed
  }
  if (c->step == 0) { c->lim_a = v; c->step = 1; c->expr.n = 0; return; }
  double out;
  CalcErr e;
  c->approx = false;
  if (c->step == 1) {
    c->lim_b = v;
    e = calc_integrate(&c->eq1, &c->v, c->lim_a, c->lim_b, &out, &c->approx);
  } else {
    c->lim_a = v;
    e = calc_derivative2(&c->eq1, &c->v, v, &out, &c->approx);
  }
  c->expr.n = 0;
  if (e != CALC_OK) { set_error(c, e); return; }
  c->result = out;
  c->v.ans = out;
  c->mode = MODE_RESULT;
  c->solved = false;
  c->stored = false;
}

// Typing at the cursor. "/" follows the usual order of operations: 1+2
// then / 3 is 1 + 2/3 (only the 2 on top); brackets put more on top.
static void ins(Calc *c, uint8_t t) {
  TokBuf *b = &c->expr;
  // A digit after a lone leading zero replaces it: 0 then 1 is 1 (0.5, 10, 105 stay as typed).
  int at = (c->cur == CALC_CUR_END || c->cur >= b->n) ? b->n : c->cur;
  if (is_digit(t) && at > 0 && b->t[at - 1] == '0' &&
      (at == 1 || !(is_digit(b->t[at - 2]) || b->t[at - 2] == '.')) &&
      (at == b->n || !(is_digit(b->t[at]) || b->t[at] == '.'))) {
    b->t[at - 1] = t;                                  // (a cursor stays just after it)
    return;
  }
  if (c->cur == CALC_CUR_END || c->cur >= b->n) { push(b, t); c->cur = CALC_CUR_END; return; }
  if (b->n >= CALC_MAX_TOK) return;
  if (calc_hidden_cursor(b, c->cur)) c->cur += b->t[c->cur] == '(' ? 1 : -1;   // type inside the brackets
  for (int i = b->n; i > c->cur; i--) b->t[i] = b->t[i - 1];
  b->t[c->cur++] = t;
  b->n++;
}
static void del(Calc *c) {
  TokBuf *b = &c->expr;
  if (c->cur == CALC_CUR_END || c->cur >= b->n) { if (b->n) b->n--; if (c->cur != CALC_CUR_END) c->cur = b->n; return; }
  if (c->cur == 0) return;
  for (int i = c->cur - 1; i < b->n - 1; i++) b->t[i] = b->t[i + 1];
  b->n--;
  c->cur--;
}

// "=" on input with x or y but no "=" yet types the equation sign: there's
// no separate key for it. A second "=" then solves.
static bool wants_eq_sign(const Calc *c) {
  return !calc_has_eq(&c->expr) && (calc_has_x(&c->expr) || calc_has_y(&c->expr));
}

static bool editing(const Calc *c) {
  return c->mode == MODE_EDIT || c->mode == MODE_EQ2 || c->mode == MODE_LIM;
}

// Positions the typeset view hides: just outside the brackets round the top
// or bottom of a fraction, which aren't drawn. (1+2)|/4 looks like the end of
// the top, so typing 5 there would give (1+2)5/4 and the 5 would jump out.
bool calc_hidden_cursor(const TokBuf *b, int p) {
  if (p <= 0 || p >= b->n) return false;
  if (b->t[p - 1] == '/' && b->t[p] == '(') return true;
  if (b->t[p - 1] != ')' || b->t[p] != '/') return false;
  for (int i = p - 2, depth = 0; i >= 0; i--) {         // the bracket's opening: a plain "(", not sin(
    int k = b->t[i];
    if (k == ')') depth++;
    else if (k == '(' || is_func(k)) { if (depth == 0) return k == '('; depth--; }
  }
  return false;
}

bool calc_set_cursor(Calc *c, int pos) {
  if (!editing(c)) return false;
  c->cur = (uint8_t)(pos < 0 ? 0 : pos > c->expr.n ? c->expr.n : pos);
  return true;
}

// The one "=" key. With x or y in the expression and no "=" yet, it types
// the equals sign; "=" with nothing after it takes the sign back out and
// just calculates (so 2x = = gives 2 times the stored x). Returns true if
// it typed the sign.
static bool eq_key(Calc *c) {
  TokBuf *b = &c->expr;
  if (!b->n) return false;
  int eqs = 0;
  for (int i = 0; i < b->n; i++) eqs += b->t[i] == '=';
  if (!eqs) {
    if (!calc_has_x(b) && !calc_has_y(b)) return false;
    ins(c, '=');
    return true;
  }
  if (eqs == 1 && b->t[b->n - 1] == '=') b->n--;     // empty right side
  return false;
}

bool calc_eq_ready(const TokBuf *b) {
  return b->n && calc_has_eq(b) && b->t[b->n - 1] != '=';
}

static void key_inner(Calc *c, int key);

void calc_key(Calc *c, int key) {
  uint8_t mode = c->mode;
  double res = c->result, ans = c->v.ans, rim = c->result_im, aim = c->v.ans_im;
  if (key != K_INV && key != K_DEG) { c->conv = CONV_NONE; c->label[0] = 0; }
  key_inner(c, key);
  // A new real answer (or a stored / recalled one) has no imaginary part.
  if (c->result_im == rim && (c->result != res || c->mode != mode)) c->result_im = 0;
  if (c->v.ans_im == aim && c->v.ans != ans) c->v.ans_im = 0;
  if (c->mode != MODE_RESULT) c->result_im = 0;
  // The cursor survives only typing and DEL in the same editing mode;
  // anything else puts typing back at the end.
  bool typing = (key < 200 || key == K_DEL || key == K_INV || key == K_DEG);
  c->cur = (typing && c->mode == mode && editing(c)) ? c->cur : CALC_CUR_END;
  // A cursor at the very end is the same as typing normally: switch it off.
  if (c->cur != CALC_CUR_END && c->cur >= c->expr.n) c->cur = CALC_CUR_END;
}

// STO / RCL, then x, y or a memory: which = 0 x, 1 y, 3 + i memory i
static double *slot(Calc *c, int which) {
  return which == 0 ? &c->v.x : which == 1 ? &c->v.y : which == 9 ? &c->v.z : &c->v.m[which - 3];
}
static void store_recall(Calc *c, int what, int which) {
  double val;
  if (what == K_STOX) {
    if (c->mode == MODE_EDIT && c->expr.n) {
      do_eval(c);
      if (c->mode == MODE_ERROR) return;
    }
    val = (c->mode == MODE_RESULT && !c->sym) ? c->result : c->v.ans;
    *slot(c, which) = val;
    c->solved = false;
  } else {
    val = *slot(c, which);
    c->solved = true;                                  // with stored: shows "x =" / "y ="
  }
  c->v.ans = val;
  c->result = val;
  c->mode = MODE_RESULT;
  c->stored = true;
  c->var = (uint8_t)which;
  c->sys = c->sys3 = false; c->op = OP_NONE; c->sym = SYM_NONE;
}

// Polar <-> rectangular. The pair is "a, b" if that's what was typed (or
// the expression of the answer shown), else x and y. The result goes into
// x and y (and Ans = the first), shown as r / angle or x / y.
static void convert(Calc *c, bool to_polar) {
  double a = c->v.x, b = c->v.y;
  if (c->mode == MODE_RESULT && c->result_im != 0) { a = c->result; b = c->result_im; }   // a complex answer
  const TokBuf *e = &c->expr;
  int comma = -1, depth = 0;
  if (c->mode == MODE_EDIT || (c->mode == MODE_RESULT && !c->sym && !c->op && !c->solved && !c->stored))
    for (int i = 0; i < e->n; i++) {
      int k = e->t[i];
      if (k == '(' || is_func(k)) depth++;
      else if (k == ')') depth--;
      else if (k == ',' && depth == 0) { comma = i; break; }
    }
  if (comma >= 0) {
    TokBuf l = { .n = 0 }, r = { .n = 0 };
    for (int i = 0; i < comma; i++) l.t[l.n++] = e->t[i];
    for (int i = comma + 1; i < e->n; i++) r.t[r.n++] = e->t[i];
    CalcErr er = calc_eval(&l, &c->v, &a);
    if (er == CALC_OK) er = calc_eval(&r, &c->v, &b);
    if (er != CALC_OK) { set_error(c, er); return; }
  }
  double p, q, k = c->v.deg ? 180 / M_PI_D : 1;     // angles in the current mode
  if (to_polar) {
    double ma = m_fabs(a), mb = m_fabs(b), hi = ma > mb ? ma : mb, lo = ma > mb ? mb : ma;
    p = hi == 0 ? 0 : hi * m_sqrt(1 + (lo / hi) * (lo / hi));   // no overflow at 1E200
    q = a > 0 ? m_atan(b / a) : a < 0 ? m_atan(b / a) + (b < 0 ? -M_PI_D : M_PI_D)
      : b > 0 ? M_PI_D / 2 : b < 0 ? -M_PI_D / 2 : 0;
    q *= k;
  } else {
    p = a * deg_trig(T_COS, b, c->v.deg);
    q = a * deg_trig(T_SIN, b, c->v.deg);
  }
  if (!m_isfinite(p) || !m_isfinite(q)) { set_error(c, CALC_ERR_MATH); return; }
  c->v.x = p; c->v.y = q; c->v.ans = p; c->result = p;
  c->mode = MODE_RESULT;
  c->solved = false; c->stored = false; c->sys = c->sys3 = false; c->op = OP_NONE; c->sym = SYM_NONE;
  c->conv = to_polar ? CONV_POL : CONV_REC;
}

static void key_inner(Calc *c, int key) {
  if (c->pend) {
    int what = c->pend;
    c->pend = 0;
    if (key == T_X || key == T_Y) { store_recall(c, what, key == T_Y); return; }
    if (is_mem(key)) { store_recall(c, what, 3 + key - T_MA); return; }
    if (key == T_Z) { store_recall(c, what, 9); return; }
    // anything else: STO / RCL is cancelled and the key does what it normally does
  }
  if (key == K_INV) { c->inv = !c->inv; return; }
  if (key == K_DEG) { c->v.deg = !c->v.deg; return; }
  if (key < 200) c->inv = false;

  if (key == K_FACTOR || key == K_EXPAND) { sym_op(c, key); return; }
  if (c->sym && c->mode == MODE_RESULT && key != K_INT && key != K_DER) {
    // A formula answer: DEL or = brings it back to edit, an operator goes on
    // from it, anything else starts afresh.
    c->sym = SYM_NONE;
    c->eq1.n = 0;
    if (key == K_DEL || key == K_EVAL) { c->mode = MODE_EDIT; return; }
    if (key < 200 && continues_ans(key) && c->expr.n + 3 <= CALC_MAX_TOK) {
      TokBuf f = c->expr;
      c->expr.n = 0;
      push(&c->expr, '(');
      for (int i = 0; i < f.n; i++) push(&c->expr, f.t[i]);
      push(&c->expr, ')');
      push(&c->expr, (uint8_t)key);
      c->mode = MODE_EDIT;
      return;
    }
    c->expr.n = 0;
    c->mode = MODE_EDIT;
    if (key < 200) { push(&c->expr, (uint8_t)key); return; }
  }

  if (key == K_AC) {
    c->expr.n = 0; c->eq1.n = 0; c->eq2.n = 0;
    c->sys = c->sys3 = false;
    c->sym = SYM_NONE;
    c->op = OP_NONE;
    c->mode = MODE_EDIT;
    return;
  }
  if (key == K_CLRXY) {
    if (c->mode == MODE_EQ2 || c->mode == MODE_LIM) return;
    c->v.x = c->v.y = c->v.z = 0;
    c->result = 0;
    c->mode = MODE_RESULT;
    c->solved = false;
    c->stored = true;
    c->var = 2;
    c->sys = c->sys3 = false; c->op = OP_NONE; c->sym = SYM_NONE;
    return;
  }
  if (key == K_INT || key == K_DER) {
    if (c->mode == MODE_EQ2 || c->mode == MODE_LIM) return;
    start_op(c, key == K_INT ? OP_INT : OP_DER);
    return;
  }
  if (key == K_POL || key == K_REC) {
    if (c->mode == MODE_EQ2 || c->mode == MODE_LIM) return;
    convert(c, key == K_POL);
    return;
  }
  if (key == K_STOX || key == K_RCLX) {                // asks: x or y?
    if (c->mode == MODE_EQ2 || c->mode == MODE_LIM) return;
    c->pend = (uint8_t)key;
    return;
  }

  switch (c->mode) {
    case MODE_LIM:
      if (key == K_EVAL || key == K_SOLVE) { lim_enter(c); return; }
      if (key == K_DEL) {
        if (c->expr.n) del(c);
        else if (c->step == 1) c->step = 0;                 // back to "from"
        else { c->expr = c->eq1; c->eq1.n = 0; c->op = OP_NONE; c->mode = MODE_EDIT; }
        return;
      }
      if (key < 200 && key != '=') ins(c, (uint8_t)key);
      return;

    case MODE_EQ2:
      if (key == K_EVAL && !c->expr.n) return;              // nothing typed yet: keep asking
      if (key == K_EVAL && wants_eq_sign(c)) { ins(c, '='); return; }
      if (key == K_EVAL && eq_key(c)) return;
      if (key == K_EVAL || key == K_SOLVE) {
        if (c->eq2.n) { run_solve3(c); return; }
        if (calc_has_z(&c->eq1) || calc_has_z(&c->expr)) {   // z too: ask for a third equation
          double l, r;
          if (calc_eval_eq(&c->expr, &c->v, &l, &r) == CALC_ERR_SYNTAX) { set_error(c, CALC_ERR_SYNTAX); return; }
          c->eq2 = c->expr;
          c->expr.n = 0;
          return;
        }
        run_solve2(c);
        return;
      }
      if (key == K_DEL) {
        if (c->expr.n) del(c);
        else if (c->eq2.n) { c->expr = c->eq2; c->eq2.n = 0; }            // back to equation 2
        else { c->expr = c->eq1; c->eq1.n = 0; c->mode = MODE_EDIT; }   // back to equation 1
        return;
      }
      if (key < 200) ins(c, (uint8_t)key);
      return;

    case MODE_RESULT:
    case MODE_ERROR:
      if (c->op && key == K_DEL) {                           // back to editing f(x)
        c->expr = c->eq1; c->eq1.n = 0; c->op = OP_NONE; c->mode = MODE_EDIT;
        return;
      }
      if (c->op && key == K_EVAL) return;                     // result already shown
      if (key == K_DEL) { c->mode = c->sys ? MODE_EQ2 : MODE_EDIT; return; }
      if (c->sys && (key == K_EVAL || key == K_SOLVE)) { if (c->sys3) run_solve3(c); else run_solve2(c); return; }
      if (key == K_EVAL) {
        if (c->mode == MODE_RESULT && c->solved && !c->stored) { start_solve(c); return; }
        do_eval(c);
        return;
      }
      if (key == K_SOLVE) { start_solve(c); return; }
      if (key < 200) {
        bool was_result = c->mode == MODE_RESULT;
        c->expr.n = 0;
        c->eq1.n = 0;
        c->sys = c->sys3 = false;
        c->op = OP_NONE;
        c->mode = MODE_EDIT;
        if (was_result && continues_ans(key)) push(&c->expr, T_ANS);
        push(&c->expr, (uint8_t)key);
      }
      return;

    default:  // MODE_EDIT
      if (key == K_EVAL && wants_eq_sign(c)) { ins(c, '='); return; }
      if (key == K_EVAL) { if (!eq_key(c)) do_eval(c); return; }
      if (key == K_SOLVE) { start_solve(c); return; }
      if (key == K_DEL) { del(c); return; }
      if (key < 200) ins(c, (uint8_t)key);
      return;
  }
}

void calc_display(const Calc *c, char *small, int slen, char *big, int blen) {
  char num[32];
  small[0] = 0;
  big[0] = 0;
  switch (c->mode) {
    case MODE_EDIT:
      if (c->expr.n || c->cur != CALC_CUR_END) calc_render_cur(&c->expr, c->cur, big, blen);
      else s_cpy(big, blen, "0");
      break;
    case MODE_EQ2: {
      bool third = c->eq2.n > 0;
      s_cpy(small, slen, third ? "2: " : "1: ");
      calc_render(third ? &c->eq2 : &c->eq1, small + 3, slen - 3);
      if (c->expr.n) {
        s_cpy(big, blen, third ? "3: " : "2: ");
        calc_render_cur(&c->expr, c->cur, big + 3, blen - 3);
      } else {
        s_cpy(big, blen, third ? "3rd equation?" : "2nd equation?");
      }
      break;
    }
    case MODE_LIM: {
      static const char *P[3] = { "from = ", "to = ", "at x = " };
      s_cpy(small, slen, c->op == OP_INT ? "int " : "d/dx ");
      int k = s_len(small);
      calc_render(&c->eq1, small + k, slen - k);
      s_cpy(big, blen, P[c->step]);
      k = s_len(big);
      if (c->expr.n) calc_render_cur(&c->expr, c->cur, big + k, blen - k);
      else if (c->step == 1) s_cat(big, blen, "?");      // nothing typed at "from" / "at x": = gives the formula
      break;
    }
    case MODE_RESULT:
      calc_format_cplx(c->result, c->result_im, num, sizeof num);
      if (c->sym) {                          // a formula: "d/dx sin(x) =" then the formula
        static const char *W[] = { "", "d/dx ", "int ", "factor ", "expand " };
        s_cpy(small, slen, W[c->sym]);
        int k = s_len(small);
        calc_render(&c->eq1, small + k, slen - k);
        s_cat(small, slen, " =");
        calc_render(&c->expr, big, blen);
        if (c->sym == SYM_INT) s_cat(big, blen, " + C");
      } else if (c->op) {                           // "int 0 to 1 =" / "d/dx at 2 ="
        char a[24], b[24];
        calc_format(c->lim_a, 6, a, sizeof a);
        calc_format(c->lim_b, 6, b, sizeof b);
        if (c->op == OP_INT) {
          s_cpy(small, slen, "int ");
          s_cat(small, slen, a);
          s_cat(small, slen, " to ");
          s_cat(small, slen, b);
          s_cat(small, slen, c->approx ? " ~" : " =");
        } else {
          s_cpy(small, slen, "d/dx at ");
          s_cat(small, slen, a);
          s_cat(small, slen, c->approx ? " ~" : " =");
        }
        s_cpy(big, blen, num);
      } else if (c->label[0]) {                     // from the lists: "mean =" then the value
        s_cpy(small, slen, c->label);
        s_cpy(big, blen, num);
      } else if (c->conv) {                         // r and angle, or x and y, from Pol / Rec
        bool pol = c->conv == CONV_POL;
        calc_format_answer(c->v.x, num, sizeof num);
        s_cpy(small, slen, pol ? "r = " : "x = ");
        s_cat(small, slen, num);
        calc_format_answer(c->v.y, num, sizeof num);
        s_cpy(big, blen, pol ? (c->v.deg ? "deg = " : "rad = ") : "y = ");
        s_cat(big, blen, num);
      } else if (c->sys3) {                         // three: x and y on the small line, z on the big
        calc_format(c->v.x, 7, num, sizeof num);
        s_cpy(small, slen, "x = "); s_cat(small, slen, num);
        calc_format(c->v.y, 7, num, sizeof num);
        s_cat(small, slen, ", y = "); s_cat(small, slen, num);
        calc_format_answer(c->v.z, num, sizeof num);
        s_cpy(big, blen, "z = "); s_cat(big, blen, num);
      } else if (c->sys) {                          // system: x on the small line, y on the big
        s_cpy(small, slen, "x = ");
        s_cat(small, slen, num);
        calc_format_answer(c->v.y, num, sizeof num);
        s_cpy(big, blen, "y = ");
        s_cat(big, blen, num);
      } else if (c->solved && c->stored) {   // recall x / y / a memory
        static char nm[4] = "x =";
        nm[0] = c->var == 9 ? 'z' : c->var >= 3 ? (char)('A' + c->var - 3) : c->var ? 'y' : 'x';
        s_cpy(small, slen, nm);
        s_cpy(big, blen, num);
      } else if (c->stored) {
        static char st[12] = "Stored to x";
        st[10] = c->var == 9 ? 'z' : c->var >= 3 ? (char)('A' + c->var - 3) : c->var ? 'y' : 'x';
        s_cpy(small, slen, c->var == 2 ? "Cleared x, y, z" : st);
        s_cpy(big, blen, num);
      } else if (c->solved) {
        calc_render(&c->expr, small, slen);
        s_cpy(big, blen, "x = ");
        s_cat(big, blen, num);
      } else {
        calc_render(&c->expr, small, slen);
        if (s_len(small) < slen - 3) s_cat(small, slen, " =");
        s_cpy(big, blen, num);
      }
      break;
    case MODE_ERROR:
      calc_render(c->op ? &c->eq1 : &c->expr, small, slen);
      s_cpy(big, blen, calc_err_text((CalcErr)c->err));
      break;
  }
  if (c->pend) s_cpy(small, slen, c->pend == K_STOX ? "STO to: x, y or MEM" : "RCL: x, y or MEM");
}
