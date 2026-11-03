// mmath.c - see mmath.h. Accuracy target: a few ulp, checked against libm in test/.
#include "mmath.h"

typedef union { double d; uint64_t u; } DBits;

double m_nan(void) { DBits b; b.u = 0x7ff8000000000000ULL; return b.d; }
double m_inf(void) { DBits b; b.u = 0x7ff0000000000000ULL; return b.d; }
bool m_isnan(double x) { return x != x; }
bool m_isfinite(double x) {
  DBits b; b.d = x;
  return ((b.u >> 52) & 0x7ff) != 0x7ff;
}
double m_fabs(double x) { return x < 0 ? -x : x; }

double m_floor(double x) {
  if (!m_isfinite(x) || m_fabs(x) >= 4503599627370496.0) return x;  // 2^52
  double t = (double)(int64_t)x;
  return (t > x) ? t - 1.0 : t;
}
double m_round(double x) { return x < 0 ? -m_floor(-x + 0.5) : m_floor(x + 0.5); }

double m_frexp(double x, int *e) {
  DBits b; b.d = x;
  int ex = (int)((b.u >> 52) & 0x7ff);
  if (x == 0 || ex == 0x7ff) { *e = 0; return x; }
  int adj = 0;
  if (ex == 0) {  // subnormal
    b.d = x * 18014398509481984.0;  // 2^54
    ex = (int)((b.u >> 52) & 0x7ff);
    adj = -54;
  }
  *e = ex - 1022 + adj;
  b.u = (b.u & ~(0x7ffULL << 52)) | (1022ULL << 52);
  return b.d;
}

static double pow2i(int e) {  // 2^e for -1022 <= e <= 1023
  DBits b; b.u = (uint64_t)(e + 1023) << 52; return b.d;
}

double m_ldexp(double x, int e) {
  while (e > 1023) { x *= pow2i(1023); e -= 1023; if (!m_isfinite(x)) return x; }
  while (e < -1022) { x *= pow2i(-1022); e += 1022; if (x == 0) return x; }
  return x * pow2i(e);
}

static const double P10[23] = {
  1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
  1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22 };

double m_pow10i(int n) {
  if (n >= 0) {
    if (n > 308) return m_inf();
    double r = 1.0;
    while (n > 22) { r *= 1e22; n -= 22; }
    return r * P10[n];
  }
  n = -n;
  if (n > 340) return 0.0;
  if (n <= 22) return 1.0 / P10[n];
  double r = 1.0;
  while (n > 22) { r /= 1e22; n -= 22; }
  return r / P10[n];
}

double m_sqrt(double x) {
  if (m_isnan(x) || x < 0) return m_nan();
  if (x == 0 || !m_isfinite(x)) return x;
  int e;
  double m = m_frexp(x, &e);           // x = m * 2^e, m in [0.5, 1)
  if (e & 1) { m *= 2.0; e -= 1; }     // m in [0.5, 2), e even
  double y = 0.41731 + 0.59016 * m;    // linear first guess
  for (int i = 0; i < 6; i++) y = 0.5 * (y + m / y);
  return m_ldexp(y, e / 2);
}

double m_exp(double x) {
  if (m_isnan(x)) return x;
  if (x > 709.782712893384) return m_inf();
  if (x < -745.1332191019412) return 0.0;
  static const double LN2_HI = 6.93147180369123816490e-01;
  static const double LN2_LO = 1.90821492927058770002e-10;
  static const double INV_LN2 = 1.44269504088896338700e+00;
  int k = (int)m_round(x * INV_LN2);
  double r = (x - k * LN2_HI) - k * LN2_LO;  // |r| <= ~0.347
  // Taylor series, Horner form, 1 + r + r^2/2! + ... + r^17/17!
  double s = 1.0;
  for (int n = 17; n >= 1; n--) s = 1.0 + s * r / n;
  return m_ldexp(s, k);
}

double m_log(double x) {
  if (m_isnan(x) || x < 0) return m_nan();
  if (x == 0) return -m_inf();
  if (!m_isfinite(x)) return x;
  static const double LN2_HI = 6.93147180369123816490e-01;
  static const double LN2_LO = 1.90821492927058770002e-10;
  int k;
  double m = m_frexp(x, &k);  // [0.5, 1)
  if (m < 0.70710678118654752440) { m *= 2.0; k--; }
  double s = (m - 1.0) / (m + 1.0), s2 = s * s;
  // log(m) = 2*(s + s^3/3 + s^5/5 + ...)
  double t = 0.0;
  for (int n = 31; n >= 3; n -= 2) t = (t + 1.0 / n) * s2;
  double lm = 2.0 * s + 2.0 * s * t;
  return k * LN2_HI + (lm + k * LN2_LO);
}

double m_log10(double x) {
  double r = m_log(x) / M_LN10_D;
  // Exact powers of ten give exact integers.
  double ri = m_round(r);
  if (m_isfinite(r) && m_fabs(r - ri) < 1e-12 && ri >= -300 && ri <= 300 &&
      m_pow10i((int)ri) == x) return ri;
  return r;
}

double m_cbrt(double x) {
  if (x == 0 || !m_isfinite(x)) return x;
  double a = m_fabs(x);
  double y = m_exp(m_log(a) / 3.0);
  y = y - (y * y * y - a) / (3.0 * y * y);  // one Newton polish
  double yr = m_round(y);
  if (yr * yr * yr == a) y = yr;            // exact cubes stay exact
  return x < 0 ? -y : y;
}

// x! for whole x by multiplying; otherwise gamma(x + 1) (Lanczos, g = 7).
double m_fact(double x) {
  if (m_isnan(x)) return x;
  if (x > 170.6) return m_inf();
  if (x == m_floor(x)) {
    if (x < 0) return m_nan();          // poles at the negative integers
    double r = 1.0;
    for (int i = 2; i <= (int)x; i++) r *= i;
    return r;
  }
  double z = x + 1;                     // gamma(z)
  if (z < 0.5) {                        // reflection: gamma(z) gamma(1-z) = pi / sin(pi z)
    double s = m_sin(M_PI_D * (z - 2 * m_floor(z / 2)));
    return M_PI_D / (s * m_fact(-z));   // (-z)! = gamma(1 - z)
  }
  static const double C[9] = { 0.99999999999980993, 676.5203681218851, -1259.1392167224028,
    771.32342877765313, -176.61502916214059, 12.507343278686905, -0.13857109526572012,
    9.9843695780195716e-6, 1.5056327351493116e-7 };
  z -= 1;
  double a = C[0], t = z + 7.5;
  for (int i = 1; i < 9; i++) a += C[i] / (z + i);
  // sqrt(2 pi) t^(z + 0.5) e^-t a, split so t^(z+0.5) cannot overflow early
  double h = m_pow(t, 0.5 * (z + 0.5));
  return 2.5066282746310002 * h * (h * m_exp(-t)) * a;
}

double m_expm1(double x) {              // e^x - 1, accurate near 0
  double u = m_exp(x);
  if (u == 1.0) return x;
  double um = u - 1.0;
  if (um == -1.0) return -1.0;
  if (!m_isfinite(u)) return u;
  return um * x / m_log(u);
}

double m_log1p(double x) {              // ln(1 + x), accurate near 0
  double u = 1.0 + x;
  if (u == 1.0) return x;
  return m_log(u) * x / (u - 1.0);
}

// x - y*floor(x/y) worked out exactly by long division in binary
double m_fmod(double x, double y) {
  if (!m_isfinite(x) || y == 0 || m_isnan(y)) return m_nan();
  if (!m_isfinite(y)) return x;
  double ay = m_fabs(y), r = m_fabs(x);
  if (r >= ay) {
    int ex, ey;
    m_frexp(r, &ex); m_frexp(ay, &ey);
    double d = m_ldexp(ay, ex - ey);
    for (int k = ex - ey; k >= 0; k--, d *= 0.5) if (r >= d) r -= d;
  }
  if (x < 0 && r != 0) r = ay - r;      // floor semantics: the sign of y
  return y < 0 && r != 0 ? r - ay : r;
}

double m_pow(double x, double y) {
  if (m_isnan(x) || m_isnan(y)) return m_nan();
  if (y == 0) return 1.0;
  if (x == 0) return y > 0 ? 0.0 : m_nan();
  bool yint = (y == m_floor(y));
  if (yint && m_fabs(y) <= 1048576.0) {
    long n = (long)m_fabs(y);
    double b = x, r = 1.0;
    long n0 = n;
    while (n) { if (n & 1) r *= b; b *= b; n >>= 1; }
    // each squaring doubles the error: fine for small powers, or exact for
    // whole numbers that stay below 2^53; else exp(y ln x) is closer
    int ex;
    if (n0 <= 32 || (x == m_floor(x) && m_fabs(r) < 9007199254740992.0) || m_fabs(m_frexp(x, &ex)) == 0.5)   // (0.5^1000: exact)
      return y < 0 ? 1.0 / r : r;
  }
  if (x < 0) {
    if (!yint) {                        // odd roots of negatives: (-8)^(1/3) = -2
      for (int q = 3; q <= 99; q += 2) {   // smallest odd q with y = p/q
        double p = m_round(y * q);
        if (m_fabs(y * q - p) > 1e-12 * q) continue;
        double r = m_exp(y * m_log(-x));
        return (p / 2 == m_floor(p / 2)) ? r : -r;
      }
      return m_nan();
    }
    // huge integer exponent of a negative base: sign from parity
    double r = m_exp(y * m_log(-x));
    double half = y * 0.5;
    return (half == m_floor(half)) ? r : -r;
  }
  // A base just off 1 to a huge power: the base's binary rounding gets
  // multiplied by the power, so (1 + 1E-12)^1E12 would be 2.7185. Use the
  // short decimal it stands for (the calculator works in decimals).
  double u = x - 1;                     // (exact)
  if (m_fabs(u) < 1e-3 && m_fabs(y) > 1e4) {
    double s = m_round(u * 1e15) / 1e15;
    if (m_fabs(s - u) <= 1.12e-16) u = s;
    return m_exp(y * m_log1p(u));
  }
  return m_exp(y * m_log(x));
}

// ---- trig ----
static const double PIO2_1 = 1.57079632673412561417e+00;
static const double PIO2_2 = 6.07710050630396597660e-11;
static const double PIO2_3 = 2.02226624871116645580e-21;
static const double TWO_OVER_PI = 6.36619772367581382433e-01;

static double ksin(double r) {  // |r| <= pi/4
  double r2 = r * r;
  double term = r, sum = r;
  for (int n = 1; n <= 10; n++) {
    term *= -r2 / (double)((2 * n) * (2 * n + 1));
    sum += term;
  }
  return sum;
}
static double kcos(double r) {
  double r2 = r * r, term = 1.0, sum = 1.0;
  for (int n = 1; n <= 10; n++) {
    term *= -r2 / (double)((2 * n - 1) * (2 * n));
    sum += term;
  }
  return sum;
}

// a * b = *p + *e exactly (Dekker), for the reduction of big arguments
static void mul2(double a, double b, double *p, double *e) {
  const double S = 134217729.0;   // 2^27 + 1
  *p = a * b;
  double t = S * a, ah = t - (t - a), al = a - ah;
  t = S * b;
  double bh = t - (t - b), bl = b - bh;
  *e = ((ah * bh - *p) + ah * bl + al * bh) + al * bl;
}

// Reduce x to r in about [-pi/4, pi/4], returning quadrant in *q. Returns false if too large.
static bool reduce(double x, double *r, int *q) {
  if (m_fabs(x) > 1e16) return false;
  double n = m_round(x * TWO_OVER_PI);
  if (m_fabs(n) < 1048576.0) {          // n * PIO2_1 is exact
    *r = ((x - n * PIO2_1) - n * PIO2_2) - n * PIO2_3;
  } else {                              // pi/2 to 160 bits, products kept exact
    static const double P0 = 1.5707963267948966, P1 = 6.123233995736766e-17, P2 = -1.4973849048591698e-33;
    double p, e, t;
    mul2(n, P0, &p, &e);
    t = x - p;                          // exact: p is within a factor 2 of x
    t -= e;
    mul2(n, P1, &p, &e);
    t -= p;
    *r = t - (e + n * P2);
  }
  // near 1e16, x * 2/pi itself rounds and n can be off by one: put r back
  // within pi/4 (the kernels' range)
  while (*r > 0.7853981633974484) { *r = (*r - 1.5707963267948966) - 6.123233995736766e-17; n += 1; }
  while (*r < -0.7853981633974484) { *r = (*r + 1.5707963267948966) + 6.123233995736766e-17; n -= 1; }
  *q = (int)(n - 4.0 * m_floor(n / 4.0));
  return true;
}

double m_sin(double x) {
  if (!m_isfinite(x)) return m_nan();
  double r; int q;
  if (!reduce(x, &r, &q)) return m_nan();
  switch (q) {
    case 0: return ksin(r);
    case 1: return kcos(r);
    case 2: return -ksin(r);
    default: return -kcos(r);
  }
}
double m_cos(double x) {
  if (!m_isfinite(x)) return m_nan();
  double r; int q;
  if (!reduce(x, &r, &q)) return m_nan();
  switch (q) {
    case 0: return kcos(r);
    case 1: return -ksin(r);
    case 2: return -kcos(r);
    default: return ksin(r);
  }
}
double m_tan(double x) {
  if (!m_isfinite(x)) return m_nan();
  double r; int q;
  if (!reduce(x, &r, &q)) return m_nan();
  double s = ksin(r), c = kcos(r);
  return (q & 1) ? -c / s : s / c;
}

double m_atan(double x) {
  if (m_isnan(x)) return x;
  bool neg = x < 0;
  if (neg) x = -x;
  bool inv = false;
  if (x > 1.0) { x = 1.0 / x; inv = true; }  // 1/inf = 0 works
  bool sh = false;
  static const double SQRT3 = 1.73205080756887729353;
  if (x > 0.26794919243112270) {  // tan(pi/12)
    x = (SQRT3 * x - 1.0) / (SQRT3 + x);
    sh = true;
  }
  double x2 = x * x, term = x, sum = x;
  for (int n = 1; n <= 16; n++) {
    term *= -x2;
    sum += term / (2 * n + 1);
  }
  if (sh) sum += M_PI_D / 6.0;
  if (inv) sum = M_PI_D / 2.0 - sum;
  return neg ? -sum : sum;
}

double m_asin(double x) {
  if (m_isnan(x) || x > 1.0 || x < -1.0) return m_nan();
  if (x == 1.0) return M_PI_D / 2.0;
  if (x == -1.0) return -M_PI_D / 2.0;
  return m_atan(x / m_sqrt((1.0 - x) * (1.0 + x)));
}

double m_acos(double x) {
  if (m_isnan(x) || x > 1.0 || x < -1.0) return m_nan();
  if (x == -1.0) return M_PI_D;
  return 2.0 * m_atan(m_sqrt((1.0 - x) / (1.0 + x)));
}
