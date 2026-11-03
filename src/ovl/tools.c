// tools.c - see src/c/tools.h. In the "math" overlay on the watch, so the
// tables and the statistics only take memory while a list is open.
#include "../c/tools.h"
#include "../c/mmath.h"
#include <stddef.h>

static void cpy(char *d, int n, const char *s) { int i = 0; for (; s[i] && i < n - 1; i++) d[i] = s[i]; d[i] = 0; }
static void cat(char *d, int n, const char *s) { int i = 0; while (d[i]) i++; cpy(d + i, n - i, s); }
static void num(char *d, int n, double v) { calc_format(v, 8, d, n); }

// Keys for the app to press (the key handling isn't in this overlay).
static void key(ToolsIO *io, int k) { if (io->nkeys < (int)sizeof io->keys) io->keys[io->nkeys++] = (uint8_t)k; }

// Type a number as if on the keys (in brackets after something already typed: 2(6.02e23)).
static void type_num(ToolsIO *io, double v) {
  char b[32];
  calc_format(v, 15, b, sizeof b);                    // every digit the constant has
  bool wrap = io->calc->mode == MODE_EDIT && io->calc->expr.n;
  if (wrap) key(io, '(');
  for (int i = 0; b[i]; i++) key(io, b[i] == 'e' ? T_EE : b[i]);
  if (wrap) key(io, ')');
}

// The number to work on: what's typed (if it works out), else the answer shown, else Ans.
static double ans(const Calc *c) {
  double v;
  if (c->mode == MODE_EDIT && c->expr.n && calc_eval(&c->expr, &c->v, &v) == CALC_OK) return v;
  return c->mode == MODE_RESULT && !c->sym ? c->result : c->v.ans;
}

// ---------------------------------------------------------------- statistics

enum { S_ADD, S_ADDXY, S_UNDO, S_CLEAR, S_N, S_MEAN, S_SUM, S_SDEV, S_PDEV, S_MIN, S_MAX, S_MED,
       S_MEANY, S_A, S_B, S_R, S_ROWS };
static const char *const SNAME[S_ROWS] = {
  "+ Ans", "+ (x, y)", "Undo last", "Clear all", "n", "mean", "sum", "s (sample)", "sigma (all)",
  "min", "max", "median", "mean y", "a in y=ax+b", "b in y=ax+b", "r (correlation)" };

static double stat(const StatData *d, int which) {
  int n = d->n;
  if (!n) return m_nan();
  // two passes: the means first, then the sums of squares about them (one
  // pass as sum x^2 - n m^2 loses everything for 1E9+1, 1E9+2, 1E9+3)
  double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0, lo = d->x[0], hi = d->x[0];
  for (int i = 0; i < n; i++) {
    sx += d->x[i];
    if (d->x[i] < lo) lo = d->x[i];
    if (d->x[i] > hi) hi = d->x[i];
    if (d->pairs) sy += d->y[i];
  }
  double m = sx / n, my = sy / n;
  for (int i = 0; i < n; i++) {
    double x = d->x[i] - m, y = d->y[i] - my;
    sxx += x * x;
    if (d->pairs) { syy += y * y; sxy += x * y; }
  }
  double vx = sxx / n;
  switch (which) {
    case S_N: return n;
    case S_MEAN: return m;
    case S_SUM: return sx;
    case S_SDEV: return n > 1 ? m_sqrt(sxx / (n - 1)) : m_nan();
    case S_PDEV: return m_sqrt(vx);
    case S_MIN: return lo;
    case S_MAX: return hi;
    case S_MED: {
      double s[STAT_N];
      for (int i = 0; i < n; i++) {                   // sorted copy
        int j = i;
        while (j > 0 && s[j - 1] > d->x[i]) { s[j] = s[j - 1]; j--; }
        s[j] = d->x[i];
      }
      return n % 2 ? s[n / 2] : (s[n / 2 - 1] + s[n / 2]) / 2;
    }
  }
  if (!d->pairs) return m_nan();
  double a = sxx > 0 ? sxy / sxx : m_nan();
  switch (which) {
    case S_MEANY: return my;
    case S_A: return a;
    case S_B: return my - a * m;
    case S_R: return (sxx > 0 && syy > 0) ? sxy / m_sqrt(sxx * syy) : m_nan();
  }
  return m_nan();
}

static int stat_rows(const StatData *d) { return d->pairs && d->n ? S_ROWS : S_MED + 1; }   // (and "Probability >")

// ---------------------------------------------------------------- probability

static const uint8_t PTOK[] = { T_RAN, T_RANINT, T_NCDF, T_NINV, T_NPDF };
static const char *const PNAME[] = { "Ran# random 0 to 1", "RanInt(a, b) whole", "normcdf(x) P(Z<x)",
                                     "invnorm(p) inverse", "normpdf(x) density", "Prime factors" };
#define P_ROWS 6

// v as a product of primes: "2^3 * 3 * 5". Divisors are tried up to 10^6,
// so every number below 10^12 is complete; beyond, what's left is marked.
static void u64s(uint64_t u, char *b) {                // exact decimal digits
  char t[24]; int k = 0;
  do { t[k++] = (char)('0' + u % 10); u /= 10; } while (u);
  int o = 0;
  while (k) b[o++] = t[--k];
  b[o] = 0;
}
static bool factors(double v, char *out, int n) {
  if (v != m_floor(v)) v = calc_round_sig(v, 12);      // 0.1*3*10 shows as 3: it is 3 here too
  if (v == 1 || v == -1) { cpy(out, n, v < 0 ? "-1 (no prime factors)" : "1 (no prime factors)"); return true; }
  if (v == 0) { cpy(out, n, "0 (no prime factors)"); return true; }
  if (v == m_floor(v) && !(m_fabs(v) < 9e15)) { cpy(out, n, "too big (up to 9e15)"); return true; }
  if (!(v == m_floor(v))) { cpy(out, n, "needs a whole number"); return false; }
  // built in full first; " * " between factors, "*" if that is too long,
  // and "..." at the end if even that doesn't fit (never silently cut)
  uint64_t P[16]; int E[16], np = 0;
  uint64_t u = (uint64_t)m_fabs(v);
  for (uint64_t d = 2; d <= 1000000 && d * d <= u; d += d < 3 ? 1 : (d % 6 == 1 ? 4 : 2)) {   // 2, 3, 5, 7, 11, 13 ...
    int e = 0;
    while (u % d == 0) { u /= d; e++; }
    if (e && np < 16) { P[np] = d; E[np++] = e; }
  }
  bool unsure = u > 1000000000000ull;                  // no factor below 10^6: maybe not prime
  if (u > 1 && np < 16) { P[np] = u; E[np++] = 1; }
  char full[128], b[24];
  for (int pass = 0; pass < 2; pass++) {
    full[0] = 0;
    if (v < 0) cat(full, sizeof full, "-");
    for (int i = 0; i < np; i++) {
      if (i) cat(full, sizeof full, pass ? "*" : " * ");
      u64s(P[i], b); cat(full, sizeof full, b);
      if (E[i] > 1) { cat(full, sizeof full, "^"); u64s((uint64_t)E[i], b); cat(full, sizeof full, b); }
    }
    if (unsure) cat(full, sizeof full, " (?)");
    int len = 0; while (full[len]) len++;
    if (len <= n - 1) break;
  }
  int len = 0; while (full[len]) len++;
  if (len > n - 1) { full[n - 4] = 0; cat(full, sizeof full, "..."); }
  cpy(out, n, full);
  return true;
}

// ---------------------------------------------------------------- units

// Grouped: first a list of the groups, then a group's conversions (both ways).
enum { U_ANGLE, U_LENGTH, U_AREA, U_VOLUME, U_MASS, U_TEMP, U_TIME, U_SPEED, U_FUEL, U_ENERGY, U_PRESSURE,
       U_FORCE, U_TORQUE, U_FLOW, U_DATA, N_UCAT };
static const char *const UCAT[N_UCAT] = { "Angle", "Length", "Area", "Volume", "Mass", "Temperature", "Time",
                                          "Speed", "Fuel economy", "Energy and power", "Pressure", "Force",
                                          "Torque", "Flow", "Data" };
// b = a * f + off; or, with rc set, b = f / a (fuel economy: mpg <-> L/100 km)
typedef struct { uint8_t cat; const char *a, *b; double f, off; uint8_t rc; } Unit;
#define U(c, a, b, f) { c, a, b, f, 0, 0 }
static const Unit UNITS[] = {
  U(U_ANGLE, "deg", "rad", M_PI_D / 180), U(U_ANGLE, "deg", "grad", 10.0 / 9), U(U_ANGLE, "turn", "deg", 360),
  U(U_ANGLE, "arcmin", "deg", 1.0 / 60), U(U_ANGLE, "arcsec", "deg", 1.0 / 3600),
  U(U_LENGTH, "in", "cm", 2.54), U(U_LENGTH, "in", "mm", 25.4), U(U_LENGTH, "ft", "m", 0.3048),
  U(U_LENGTH, "yd", "m", 0.9144), U(U_LENGTH, "mi", "km", 1.609344), U(U_LENGTH, "nmi", "km", 1.852),
  U(U_LENGTH, "mil", "mm", 0.0254), U(U_LENGTH, "ly", "km", 9.4607304725808e12),
  U(U_AREA, "acre", "ha", 0.40468564224), U(U_AREA, "sq ft", "sq m", 0.09290304), U(U_AREA, "sq in", "sq cm", 6.4516),
  U(U_AREA, "sq yd", "sq m", 0.83612736), U(U_AREA, "sq mi", "sq km", 2.589988110336),
  U(U_VOLUME, "gal", "L", 3.785411784), U(U_VOLUME, "qt", "L", 0.946352946), U(U_VOLUME, "pt", "mL", 473.176473),
  U(U_VOLUME, "cup", "mL", 236.5882365), U(U_VOLUME, "fl oz", "mL", 29.5735295625),
  U(U_VOLUME, "tbsp", "mL", 14.78676478125), U(U_VOLUME, "tsp", "mL", 4.92892159375),
  U(U_VOLUME, "UK gal", "L", 4.54609), U(U_VOLUME, "UK pt", "mL", 568.26125),
  U(U_VOLUME, "cu ft", "L", 28.316846592), U(U_VOLUME, "cu ft", "cu m", 0.028316846592),
  U(U_VOLUME, "cu in", "cu cm", 16.387064), U(U_VOLUME, "barrel", "L", 158.987294928),
  U(U_MASS, "lb", "kg", 0.45359237), U(U_MASS, "oz", "g", 28.349523125), U(U_MASS, "st", "kg", 6.35029318),
  U(U_MASS, "US ton", "t", 0.90718474), U(U_MASS, "UK ton", "t", 1.0160469088), U(U_MASS, "troy oz", "g", 31.1034768),
  U(U_MASS, "ct", "g", 0.2), U(U_MASS, "grain", "mg", 64.79891),
  { U_TEMP, "F", "C", 5.0 / 9, -160.0 / 9, 0 }, { U_TEMP, "C", "K", 1, 273.15, 0 },
  { U_TEMP, "F", "K", 5.0 / 9, 459.67 * 5 / 9, 0 }, U(U_TEMP, "R", "K", 5.0 / 9),
  U(U_TIME, "min", "s", 60), U(U_TIME, "h", "min", 60), U(U_TIME, "h", "s", 3600), U(U_TIME, "day", "h", 24),
  U(U_TIME, "week", "day", 7), U(U_TIME, "year", "day", 365.25),
  U(U_SPEED, "mph", "km/h", 1.609344), U(U_SPEED, "knot", "km/h", 1.852), U(U_SPEED, "m/s", "km/h", 3.6),
  U(U_SPEED, "ft/s", "m/s", 0.3048), U(U_SPEED, "Mach", "m/s", 340.29),
  U(U_FUEL, "mpg", "km/L", 0.425143707), { U_FUEL, "mpg", "L/100km", 235.214583, 0, 1 },
  { U_FUEL, "UK mpg", "L/100km", 282.480936, 0, 1 },
  U(U_ENERGY, "kcal", "kJ", 4.184), U(U_ENERGY, "cal", "J", 4.184), U(U_ENERGY, "BTU", "kJ", 1.05505585262),
  U(U_ENERGY, "kWh", "MJ", 3.6), U(U_ENERGY, "eV", "J", 1.602176634e-19), U(U_ENERGY, "ft lbf", "J", 1.3558179483314004),
  U(U_ENERGY, "therm", "MJ", 105.505585262), U(U_ENERGY, "hp", "kW", 0.745699872), U(U_ENERGY, "PS", "kW", 0.73549875),
  U(U_ENERGY, "BTU/h", "W", 0.29307107017),
  U(U_PRESSURE, "psi", "kPa", 6.894757293), U(U_PRESSURE, "atm", "kPa", 101.325), U(U_PRESSURE, "bar", "kPa", 100),
  U(U_PRESSURE, "mmHg", "kPa", 0.133322387415), U(U_PRESSURE, "inHg", "kPa", 3.38638864),
  U(U_PRESSURE, "kgf/cm2", "kPa", 98.0665),
  U(U_FORCE, "lbf", "N", 4.4482216152605), U(U_FORCE, "kgf", "N", 9.80665), U(U_FORCE, "dyn", "N", 1e-5),
  U(U_TORQUE, "lb ft", "N m", 1.3558179483314004), U(U_TORQUE, "lb in", "N m", 0.1129848290276167),
  U(U_TORQUE, "kgf m", "N m", 9.80665),
  U(U_FLOW, "gal/min", "L/min", 3.785411784), U(U_FLOW, "cfm", "cu m/h", 1.6990107955), U(U_FLOW, "L/s", "L/min", 60),
  U(U_DATA, "byte", "bit", 8), U(U_DATA, "kB", "byte", 1000), U(U_DATA, "MB", "kB", 1000), U(U_DATA, "GB", "MB", 1000),
  U(U_DATA, "TB", "GB", 1000), U(U_DATA, "KiB", "byte", 1024), U(U_DATA, "MiB", "KiB", 1024),
  U(U_DATA, "GiB", "MiB", 1024), U(U_DATA, "GiB", "GB", 1.073741824),
};
#define N_UNITS ((int)(sizeof UNITS / sizeof UNITS[0]))

// Rows of a group: (Angle: degrees in d m s first, row -1 here) then each unit forwards and backwards.
static int unit_rows(int cat) {
  int n = cat == U_ANGLE ? 1 : 0;
  for (int k = 0; k < N_UNITS; k++) if (UNITS[k].cat == cat) n += 2;
  return n;
}
// Row i of group cat: the unit and the direction; false for the d m s row.
static bool unit_at(int cat, int i, const Unit **u, bool *back) {
  if (cat == U_ANGLE) { if (i == 0) return false; i--; }
  for (int k = 0; k < N_UNITS; k++)
    if (UNITS[k].cat == cat) { if (i < 2) { *u = &UNITS[k]; *back = i & 1; return true; } i -= 2; }
  return false;
}
static double unit_conv(const Unit *u, bool back, double v, char *label, int n) {
  cpy(label, n, back ? u->b : u->a);
  cat(label, n, " > ");
  cat(label, n, back ? u->a : u->b);
  if (u->rc) return v == 0 ? m_nan() : u->f / v;          // the same both ways
  return back ? (v - u->off) / u->f : v * u->f + u->off;
}

// ---------------------------------------------------------------- constants

enum { C_MATH, C_UNIV, C_EM, C_ATOM, C_CHEM, C_EARTH, C_ASTRO, N_CCAT };
static const char *const CCAT[N_CCAT] = { "Mathematical", "Universal", "Electromagnetic", "Atomic and nuclear",
                                          "Physico-chemical", "Earth", "Astronomy" };
typedef struct { uint8_t cat; const char *name; double v; } Const;
// SI units; CODATA 2018 and IAU values
static const Const CONSTS[] = {
  { C_MATH, "phi golden ratio", 1.6180339887498949 }, { C_MATH, "gamma Euler", 0.57721566490153286 },
  { C_MATH, "sqrt 2", 1.4142135623730951 }, { C_MATH, "ln 2", 0.69314718055994531 },
  { C_MATH, "zeta(3) Apery", 1.2020569031595943 }, { C_MATH, "Catalan G", 0.91596559417721901 },
  { C_UNIV, "c light, m/s", 299792458 }, { C_UNIV, "G gravitation", 6.67430e-11 },
  { C_UNIV, "h Planck", 6.62607015e-34 }, { C_UNIV, "hbar", 1.054571817e-34 },
  { C_UNIV, "Planck length, m", 1.616255e-35 }, { C_UNIV, "Planck mass, kg", 2.176434e-8 },
  { C_UNIV, "Planck time, s", 5.391247e-44 },
  { C_EM, "e charge, C", 1.602176634e-19 }, { C_EM, "eps0 electric", 8.8541878128e-12 },
  { C_EM, "mu0 magnetic", 1.25663706212e-6 }, { C_EM, "ke Coulomb", 8.9875517923e9 },
  { C_EM, "Z0 vacuum, ohm", 376.730313668 }, { C_EM, "Phi0 flux quant.", 2.067833848e-15 },
  { C_EM, "muB Bohr magneton", 9.2740100783e-24 }, { C_EM, "muN nuclear mag.", 5.0507837461e-27 },
  { C_EM, "KJ Josephson", 483597.8484e9 }, { C_EM, "RK von Klitzing", 25812.80745 },
  { C_ATOM, "me electron, kg", 9.1093837015e-31 }, { C_ATOM, "mp proton, kg", 1.67262192369e-27 },
  { C_ATOM, "mn neutron, kg", 1.67492749804e-27 }, { C_ATOM, "u atomic mass, kg", 1.66053906660e-27 },
  { C_ATOM, "m muon, kg", 1.883531627e-28 }, { C_ATOM, "md deuteron, kg", 3.3435837724e-27 },
  { C_ATOM, "mp/me ratio", 1836.15267343 }, { C_ATOM, "alpha fine str.", 7.2973525693e-3 },
  { C_ATOM, "a0 Bohr radius, m", 5.29177210903e-11 }, { C_ATOM, "re electron r., m", 2.8179403262e-15 },
  { C_ATOM, "Compton wl., m", 2.42631023867e-12 }, { C_ATOM, "Rydberg, 1/m", 10973731.568160 },
  { C_ATOM, "eV in J", 1.602176634e-19 },
  { C_CHEM, "NA Avogadro", 6.02214076e23 }, { C_CHEM, "k Boltzmann", 1.380649e-23 },
  { C_CHEM, "R gas, J/mol K", 8.314462618 }, { C_CHEM, "F Faraday, C/mol", 96485.33212 },
  { C_CHEM, "sigma Stefan-B.", 5.670374419e-8 }, { C_CHEM, "b Wien, m K", 2.897771955e-3 },
  { C_CHEM, "Vm ideal gas, STP", 22.41396954e-3 }, { C_CHEM, "n0 Loschmidt", 2.686780111e25 },
  { C_CHEM, "0 C in K", 273.15 },
  { C_EARTH, "g gravity, m/s2", 9.80665 }, { C_EARTH, "atm, Pa", 101325 },
  { C_EARTH, "Earth mass, kg", 5.9722e24 }, { C_EARTH, "Earth radius, m", 6.3710e6 },
  { C_EARTH, "Moon mass, kg", 7.342e22 }, { C_EARTH, "Moon distance, m", 3.84399e8 },
  { C_EARTH, "sound, air 20C", 343.2 }, { C_EARTH, "day, s", 86400 }, { C_EARTH, "year, s", 31557600 },
  { C_ASTRO, "AU, m", 149597870700 }, { C_ASTRO, "light year, m", 9.4607304725808e15 },
  { C_ASTRO, "parsec, m", 3.0856775814913673e16 }, { C_ASTRO, "Sun mass, kg", 1.98847e30 },
  { C_ASTRO, "Sun radius, m", 6.957e8 }, { C_ASTRO, "Sun power, W", 3.828e26 },
  { C_ASTRO, "Jupiter mass, kg", 1.89813e27 }, { C_ASTRO, "Hubble, km/s/Mpc", 67.4 },
};
#define N_CONSTS ((int)(sizeof CONSTS / sizeof CONSTS[0]))

static const Const *const_at(int cat, int i) {
  for (int k = 0; k < N_CONSTS; k++) if (CONSTS[k].cat == cat && i-- == 0) return &CONSTS[k];
  return NULL;
}
static int const_rows(int cat) { int n = 0; for (int k = 0; k < N_CONSTS; k++) n += CONSTS[k].cat == cat; return n; }

// ---------------------------------------------------------------- bases

static const uint8_t BITOPS[] = { T_AND, T_OR, T_XOR, T_NOT, T_SHL, T_SHR };
static const char *const BITNAME[] = { "and(a, b)", "or(a, b)", "xor(a, b)", "not(a)", "shl(a, n)", "shr(a, n)" };
static const char *const BASENAME[] = { "hex", "bin", "oct" };
static const int BASE[] = { 16, 2, 8 };

// The answer in base b ("0x1F", "0b101", "0o17"); false if it isn't a whole number.
static bool in_base(double v, int b, char *out, int n) {
  double r = v == m_floor(v) ? v : calc_round_sig(v, 12);   // 2^50+1 as it is; 2.9999999999999996 as 3
  if (!(m_fabs(r) < 9e15) || r != m_floor(r)) { cpy(out, n, "not whole"); return false; }
  uint64_t u = (uint64_t)m_fabs(r);
  char t[70];
  int k = 0;
  do { t[k++] = "0123456789ABCDEF"[u % b]; u /= b; } while (u);
  int o = 0;
  if (r < 0) out[o++] = '-';
  out[o++] = '0';
  out[o++] = b == 16 ? 'x' : b == 2 ? 'b' : 'o';
  if (o + k > n - 1) { cpy(out, n, "too long to show"); return true; }    // not cut short
  while (k) out[o++] = t[--k];
  out[o] = 0;
  return true;
}

// ---------------------------------------------------------------- complex

static const uint8_t CFUNC[] = { T_RE, T_IM, T_CONJ, T_ARG, T_ABS };
static const char *const CNAME[] = { "re(z) real part", "im(z) imaginary part", "conj(z) conjugate",
                                     "arg(z) angle", "abs(z) size |z|" };

// The answer as r and angle ("5 at 53.13 deg"), for complex answers
static void polar(const Calc *c, char *out, int n) {
  double re = c->result, im = c->result_im, r = m_sqrt(re * re + im * im), k = c->v.deg ? 180 / M_PI_D : 1;
  double a = re > 0 ? m_atan(im / re) : re < 0 ? m_atan(im / re) + (im < 0 ? -M_PI_D : M_PI_D)
           : im > 0 ? M_PI_D / 2 : im < 0 ? -M_PI_D / 2 : 0;
  char b[20];
  calc_format(r, 7, out, n);
  cat(out, n, " at ");
  calc_format(a * k, 7, b, sizeof b);
  cat(out, n, b);
  cat(out, n, c->v.deg ? " deg" : " rad");
}

// Degrees as degrees, minutes, seconds: 12.5 -> 12d 30m 0s
static void dms(double v, char *out, int n) {
  double a = m_fabs(v), d = m_floor(a), mf = (a - d) * 60, m = m_floor(mf), sec = (mf - m) * 60;
  sec = m_floor(sec * 100 + 0.5) / 100;
  if (sec >= 60) { sec -= 60; m++; }
  if (m >= 60) { m -= 60; d++; }
  char b[20];
  cpy(out, n, v < 0 ? "-" : "");
  calc_format(d, 12, b, sizeof b); cat(out, n, b); cat(out, n, "d ");
  calc_format(m, 2, b, sizeof b); cat(out, n, b); cat(out, n, "m ");
  calc_format(sec, 6, b, sizeof b); cat(out, n, b); cat(out, n, "s");
}

// ---------------------------------------------------------------- the lists

int tl_count(ToolsIO *io, int list) {
  switch (list) {
    case TL_MEM: return CALC_MEMS;
    case TL_STAT: return io->cat < 0 ? stat_rows(io->st) + 1 : P_ROWS;
    case TL_UNITS: return io->cat < 0 ? N_UCAT : unit_rows(io->cat);      // the groups, then one group
    case TL_CPLX: return 6;
    case TL_CONST: return io->cat < 0 ? N_CCAT : const_rows(io->cat);
    case TL_BASE: return 3 + 6;
  }
  return 0;
}

void tl_row(ToolsIO *io, int list, int i) {
  Calc *c = io->calc;
  char *L = io->label, *V = io->value;
  int n = sizeof io->label;
  L[0] = V[0] = 0;
  switch (list) {
    case TL_MEM: {
      char m[2] = { (char)('A' + i), 0 };
      cpy(L, n, m);
      num(V, n, c->v.m[i]);
      break;
    }
    case TL_STAT: {
      const StatData *d = io->st;
      if (io->cat >= 0) { cpy(L, n, PNAME[i]); if (i == P_ROWS - 1) cpy(V, n, "of Ans"); break; }
      if (i == stat_rows(d)) { cpy(L, n, "Probability"); cpy(V, n, ">"); break; }
      cpy(L, n, SNAME[i]);
      if (i == S_ADD) { cpy(V, n, "now "); char b[12]; num(b, sizeof b, d->n); cat(V, n, b); }
      else if (i == S_ADDXY) {
        char b[14];
        num(b, sizeof b, c->v.x); cpy(V, n, b); cat(V, n, ", ");
        num(b, sizeof b, c->v.y); cat(V, n, b);
      } else if (i >= S_N) calc_format(stat(d, i), 10, V, n);   // all the digits: 1e9+1 is not 1e9
      break;
    }
    case TL_UNITS: {
      if (io->cat < 0) { cpy(L, n, UCAT[i]); cpy(V, n, ">"); break; }
      const Unit *u; bool back;
      if (!unit_at(io->cat, i, &u, &back)) { cpy(L, n, "deg > d m s"); if (m_fabs(ans(c)) < 1e9) dms(ans(c), V, n); break; }
      num(V, n, unit_conv(u, back, ans(c), L, n));
      break;
    }
    case TL_CPLX:
      if (i < 5) cpy(L, n, CNAME[i]);
      else { cpy(L, n, "Polar"); if (c->mode == MODE_RESULT) polar(c, V, n); }
      break;
    case TL_CONST: {
      if (io->cat < 0) { cpy(L, n, CCAT[i]); cpy(V, n, ">"); break; }
      const Const *k = const_at(io->cat, i);
      if (k) { cpy(L, n, k->name); calc_format(k->v, 6, V, n); }   // the full value is typed
      break;
    }
    case TL_BASE:
      if (i < 3) { cpy(L, n, "Ans in "); cat(L, n, BASENAME[i]); in_base(ans(c), BASE[i], V, n); }
      else cpy(L, n, BITNAME[i - 3]);
      break;
  }
}

int tl_pick(ToolsIO *io, int list, int i) {
  Calc *c = io->calc;
  io->nkeys = 0;
  switch (list) {
    case TL_MEM: key(io, T_MA + i); return TA_CLOSE;        // STO / RCL waiting: that memory; else typed
    case TL_STAT: {
      StatData *d = io->st;
      if (io->cat >= 0) {
        if (i < P_ROWS - 1) { key(io, PTOK[i]); return TA_CLOSE; }
        if (c->mode == MODE_EDIT && c->expr.n) key(io, K_EVAL);      // worked out first
        else if (c->mode != MODE_RESULT || c->sym) return TA_NONE;
        factors(ans(c), io->alt, sizeof io->alt);
        return TA_ALT;
      }
      if (i == stat_rows(d)) { io->cat = 0; cpy(io->title, sizeof io->title, "PROBABILITY"); return TA_SUB; }
      if (i == S_ADD || i == S_ADDXY) {
        if (d->n >= STAT_N) return TA_NONE;
        bool xy = i == S_ADDXY;
        if (!d->n) d->pairs = xy;
        if (!xy) d->pairs = false;                       // a single value: no pairs any more
        d->x[d->n] = xy ? c->v.x : ans(c);
        d->y[d->n] = xy ? c->v.y : 0;
        d->n++;
        return TA_KEEP;
      }
      if (i == S_UNDO) { if (d->n) d->n--; return TA_KEEP; }
      if (i == S_CLEAR) { d->n = 0; d->pairs = false; return TA_KEEP; }
      char l[16];
      cpy(l, sizeof l, SNAME[i]);
      for (int k = 0; l[k]; k++) if (l[k] == ' ') { l[k] = 0; break; }   // "s (sample)" -> "s"
      cat(l, sizeof l, " =");
      calc_take(c, stat(d, i), l);
      return TA_CLOSE;
    }
    case TL_UNITS: {
      char l[28];
      const Unit *u; bool back;
      if (io->cat < 0) { io->cat = i; cpy(io->title, sizeof io->title, UCAT[i]); return TA_SUB; }
      if (!unit_at(io->cat, i, &u, &back)) {             // d m s, in place of the answer (worked out first)
        if (c->mode == MODE_EDIT && c->expr.n) key(io, K_EVAL);
        else if (c->mode != MODE_RESULT || c->sym) return TA_NONE;
        if (!(m_fabs(ans(c)) < 1e9)) return TA_NONE;
        dms(ans(c), io->alt, sizeof io->alt);
        return TA_ALT;
      }
      double v = unit_conv(u, back, ans(c), l, sizeof l);
      l[15] = 0;
      calc_take(c, v, l);
      return TA_CLOSE;
    }
    case TL_CONST: {
      if (io->cat < 0) { io->cat = i; cpy(io->title, sizeof io->title, CCAT[i]); return TA_SUB; }
      const Const *k = const_at(io->cat, i);
      if (!k) return TA_NONE;
      type_num(io, k->v);
      return TA_CLOSE;
    }
    case TL_CPLX:
      if (i < 5) { key(io, CFUNC[i]); return TA_CLOSE; }
      if (c->mode != MODE_RESULT) return TA_NONE;
      polar(c, io->alt, sizeof io->alt);
      return TA_ALT;
    case TL_BASE:
      if (i < 3) {                                       // shown in place of the answer (worked out first)
        if (c->mode == MODE_EDIT && c->expr.n) key(io, K_EVAL);
        else if (c->mode != MODE_RESULT || c->sym) return TA_NONE;
        return in_base(ans(c), BASE[i], io->alt, sizeof io->alt) ? TA_ALT : TA_NONE;
      }
      key(io, BITOPS[i - 3]);
      return TA_CLOSE;
  }
  return TA_NONE;
}
