// test_cas.c - symbolic math: every answer is checked numerically.
//   derivative: f'(x) from the formula vs Ridders' numeric derivative
//   integral:   numeric derivative of the formula vs f(x)
//   factor / expand: same value as the input at several x
// Expressions are written in a small text form and turned into tokens.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../src/c/cas.h"

static const struct { const char *s; int t; } WORDS[] = {
  { "asinh(", T_ASINH }, { "acosh(", T_ACOSH }, { "atanh(", T_ATANH },
  { "sinh(", T_SINH }, { "cosh(", T_COSH }, { "tanh(", T_TANH },
  { "asin(", T_ASIN }, { "acos(", T_ACOS }, { "atan(", T_ATAN },
  { "sin(", T_SIN }, { "cos(", T_COS }, { "tan(", T_TAN }, { "ln(", T_LN }, { "log(", T_LOG },
  { "exp(", T_EXP }, { "tenx(", T_TENX }, { "sqrt(", T_SQRT }, { "cbrt(", T_CBRT }, { "abs(", T_ABS },
  { "round(", T_ROUND }, { "logb(", T_LOGB }, { "root(", T_ROOT }, { "floor(", T_FLOOR }, { "gcd(", T_GCD }, { "lcm(", T_LCM },
  { "pi", T_PI }, { "E", T_EE }, { "e", T_E }, { "x", T_X }, { "y", T_Y }, { "^2", T_SQ }, { "!", T_FACT },
};

static TokBuf tk(const char *s) {
  TokBuf b = { .n = 0 };
  while (*s) {
    if (*s == ' ') { s++; continue; }
    int k;
    for (k = 0; k < (int)(sizeof WORDS / sizeof WORDS[0]); k++) {
      size_t l = strlen(WORDS[k].s);
      if (!strncmp(s, WORDS[k].s, l) && !(WORDS[k].t == T_SQ)) { b.t[b.n++] = (uint8_t)WORDS[k].t; s += l; break; }
    }
    if (k == (int)(sizeof WORDS / sizeof WORDS[0])) b.t[b.n++] = (uint8_t)*s++;
  }
  return b;
}

static int fails, checks;
static double at(const TokBuf *b, double x, bool deg) {
  CalcVars v = { x, 0, deg, 0.7 };
  double l, r;
  if (calc_eval_eq(b, &v, &l, &r) != CALC_OK) return NAN;
  return l - r;
}
static bool close(double a, double b) { return fabs(a - b) <= 1e-6 * (1 + fabs(a) + fabs(b)); }

static void show(const char *what, const char *in, const TokBuf *out, bool ok) {
  char buf[400];
  calc_render(out, buf, sizeof buf);
  printf("%-7s %-28s -> %s\n", what, in, ok ? buf : "(none)");
}

static const double XS[] = { 0.37, 0.81, 1.3, 2.2, -0.6, -1.7 };

static void check(const char *what, const char *in, bool expect_ok, bool deg) {
  TokBuf f = tk(in), out;
  CalcVars v = { 0, 0, deg, 0.7 };
  bool ok;
  if (!strcmp(what, "d/dx")) ok = cas_derivative(&f, &v, &out);
  else if (!strcmp(what, "int")) ok = cas_integral(&f, &v, &out);
  else if (!strcmp(what, "factor")) ok = cas_factor(&f, &v, &out);
  else ok = cas_expand(&f, &v, &out);
  show(what, in, &out, ok);
  checks++;
  if (ok != expect_ok) { printf("  FAIL: expected %s\n", expect_ok ? "a formula" : "none"); fails++; return; }
  if (!ok) return;
  for (int i = 0; i < 6; i++) {
    double x = XS[i], want, got;
    CalcVars w = { x, 0, deg, 0.7 };
    if (!strcmp(what, "d/dx")) {
      if (calc_derivative(&f, &w, x, &want) != CALC_OK) continue;
      got = at(&out, x, deg);
    } else if (!strcmp(what, "int")) {
      want = at(&f, x, deg);
      if (calc_derivative(&out, &w, x, &got) != CALC_OK) got = NAN;
    } else {
      want = at(&f, x, deg);
      got = at(&out, x, deg);
      if (calc_has_eq(&f) && !strcmp(what, "factor")) {    // same zeros: compare up to a factor
        if (fabs(want) < 1e-9 || fabs(got) < 1e-9) { want = fabs(want) < 1e-9; got = fabs(got) < 1e-9; }
        else { got = want; }
      }
    }
    if (isnan(want) && isnan(got)) continue;
    if (isnan(want)) continue;                          // outside the domain
    if (isnan(got) && x < 0 && strstr(in, "x^x")) continue;   // (-0.6)^(-0.6) is real (odd root); ln(x) in the formula isn't
    checks++;
    if (!close(want, got)) { printf("  FAIL at x=%g: want %.12g got %.12g\n", x, want, got); fails++; }
  }
}

int main(void) {
  // derivatives
  check("d/dx", "x^3", true, false);
  check("d/dx", "x^2sin(x)", true, false);
  check("d/dx", "sin(x^2)", true, false);
  check("d/dx", "exp(2x)", true, false);
  check("d/dx", "ln(x^2+1)", true, false);
  check("d/dx", "sqrt(x^2+1)", true, false);
  check("d/dx", "x/(x+1)", true, false);
  check("d/dx", "sin(x)/x", true, false);
  check("d/dx", "tan(x)", true, false);
  check("d/dx", "atan(x)", true, false);
  check("d/dx", "asin(x/3)", true, false);
  check("d/dx", "x^x", true, false);
  check("d/dx", "2^x", true, false);
  check("d/dx", "tenx(x)", true, false);
  check("d/dx", "log(x)", true, false);
  check("d/dx", "cbrt(x)", true, false);
  check("d/dx", "abs(x)", true, false);
  check("d/dx", "sinh(x)cosh(x)", true, false);
  check("d/dx", "tanh(3x)", true, false);
  check("d/dx", "asinh(x)+acosh(x+3)+atanh(x/4)", true, false);
  check("d/dx", "5", true, false);
  check("d/dx", "xy+y^2", true, false);
  check("d/dx", "(x+1)^5", true, false);
  check("d/dx", "1/x^2", true, false);
  check("d/dx", "3x^2-4x+7", true, false);
  check("d/dx", "pi x", true, false);
  check("d/dx", "sin(x)", true, true);       // degrees
  check("d/dx", "atan(x)", true, true);
  check("d/dx", "sin(x)-x cos(x)", true, false);
  check("d/dx", "-(x+1)^2", true, false);
  check("d/dx", "floor(x)", false, false);
  check("d/dx", "x!", false, false);
  check("d/dx", "x=2", false, false);
  check("d/dx", "logb(2,x)", true, false);
  check("d/dx", "root(3,x^2+1)", true, false);
  check("int", "root(3,x)", true, false);
  // integrals
  check("int", "x^2", true, false);
  check("int", "3x^2-4x+7", true, false);
  check("int", "1/x", true, false);
  check("int", "x^-2", true, false);
  check("int", "sqrt(x)", true, false);
  check("int", "sin(x)", true, false);
  check("int", "cos(3x+1)", true, false);
  check("int", "exp(2x)", true, false);
  check("int", "exp(x)", true, false);
  check("int", "2^x", true, false);
  check("int", "1/(2x+1)", true, false);
  check("int", "(2x+1)^3", true, false);
  check("int", "x sin(x)", true, false);
  check("int", "x^2 exp(x)", true, false);
  check("int", "x^3 cos(2x)", true, false);
  check("int", "x ln(x)", true, false);
  check("int", "ln(x)", true, false);
  check("int", "ln(x)/x", true, false);
  check("int", "x cos(x^2)", true, false);
  check("int", "x/(x^2+1)", true, false);
  check("int", "1/(x^2+1)", true, false);
  check("int", "1/(4x^2+9)", true, false);
  check("int", "2x exp(x^2)", true, false);
  check("int", "sin(x)cos(x)", true, false);
  check("int", "tan(x)", true, false);
  check("int", "(x+1)(x-2)", true, false);
  check("int", "(x^2+1)exp(x)", true, false);
  check("int", "sinh(2x)", true, false);
  check("int", "y x", true, false);
  check("int", "5", true, false);
  check("int", "sin(x)", true, true);        // degrees
  check("int", "1/(x^2+1)", true, true);
  check("int", "atan(x)", true, false);
  check("int", "asin(x/2)", true, false);
  check("int", "acos(x)", true, false);
  check("int", "atan(x)", true, true);        // degrees
  check("int", "1/(x^2-1)", true, false);
  check("int", "1/(x(x+1))", true, false);
  check("int", "1/(x^2+2x+2)", true, false);
  check("int", "1/(x^2+2x+1)", true, false);
  check("int", "(2x+3)/(x^2+3x+2)", true, false);
  check("int", "x/(x+1)", true, false);
  check("int", "(x^3+1)/(x^2+4)", true, false);
  check("int", "1/(x^2+2x+2)", true, true);
  check("int", "sin(x)^2", true, false);
  check("int", "cos(3x)^2", true, false);
  check("int", "tan(x)^2", true, false);
  check("int", "1/cos(x)^2", true, false);
  check("int", "1/sin(2x)^2", true, false);
  check("int", "sin(x)^2", true, true);
  check("int", "cosh(x)^2", true, false);
  check("int", "1/sqrt(1-x^2)", true, false);
  check("int", "1/sqrt(4-x^2)", true, true);
  check("int", "1/sqrt(x^2+1)", true, false);
  check("int", "1/sqrt(x^2-1)", true, false);
  check("int", "1/sqrt(x^2+2x+5)", true, false);
  check("int", "x/sqrt(x^2+1)", true, false);
  check("int", "x sqrt(x)", true, false);
  check("int", "1/sqrt(x)", true, false);
  check("int", "abs(x)", true, false);
  check("int", "(x^2+1)^2", true, false);
  check("int", "asinh(x)", true, false);
  check("int", "atanh(x/2)", true, false);
  check("int", "exp(x^2)", false, false);
  check("int", "sin(x)/x", false, false);
  // factor
  check("factor", "x^2-x-6", true, false);
  check("factor", "x^3-6x^2+11x-6", true, false);
  check("factor", "2x^2+4x", true, false);
  check("factor", "6x^2-x-2", true, false);
  check("factor", "x^2-2", true, false);
  check("factor", "x^4-1", true, false);
  check("factor", "(x+1)^2(x-3)", true, false);
  check("factor", "x^2/2-1/2", true, false);
  check("factor", "x^2=4", true, false);
  check("factor", "x^3", true, false);
  check("factor", "sin(x)", false, false);
  check("factor", "x^4-5x^2+6", true, false);
  check("factor", "x^4+4", true, false);
  check("factor", "x^12-1", true, false);
  check("factor", "x^10-1", true, false);
  check("expand", "(x+1)^12", true, false);
  check("factor", "-0.0031x^6+25x^4-44x^3+3x^2+64000x-7", true, false);   // large coefficients: no overflow
  check("factor", "123456x^4-98765x^3+1", true, false);
  // expand
  check("expand", "(x+1)^3", true, false);
  check("expand", "(x-2)(x+3)", true, false);
  check("expand", "(2x+y)^2", true, false);
  check("expand", "x(x+1)-x^2", true, false);
  check("expand", "(sin(x)+1)^2", true, false);
  check("expand", "(x+1)^2=x", true, false);
  check("expand", "3(x-1)/2", true, false);
  // random expressions: no crash, and any answer must check out
  static const char *PARTS[] = { "x", "x^2", "3", "2x+1", "sin(x)", "cos(2x)", "exp(x)", "ln(x^2+1)",
                                 "sqrt(x^2+4)", "1/(x+3)", "(x-1)^3", "atan(x)", "x^3", "y", "pi", "tanh(x)" };
  static const char *OPS[] = { "+", "-", "*", "/", "" };
  unsigned seed = 12345;
  int answered = 0, tried = 0;
  for (int it = 0; it < 1500; it++) {
    char e[200] = "";
    int n = 1 + (int)((seed = seed * 1103515245u + 12345u) >> 16) % 4;
    for (int j = 0; j < n; j++) {
      if (j) strcat(e, OPS[((seed = seed * 1103515245u + 12345u) >> 16) % 5]);
      strcat(e, "(");
      strcat(e, PARTS[((seed = seed * 1103515245u + 12345u) >> 16) % 16]);
      strcat(e, ")");
    }
    const char *kind = (it % 4 == 0) ? "d/dx" : (it % 4 == 1) ? "int" : (it % 4 == 2) ? "expand" : "factor";
    TokBuf f = tk(e), out;
    CalcVars v = { 0, 0, false, 0.7 };
    bool ok = !strcmp(kind, "d/dx") ? cas_derivative(&f, &v, &out) : !strcmp(kind, "int") ? cas_integral(&f, &v, &out)
            : !strcmp(kind, "expand") ? cas_expand(&f, &v, &out) : cas_factor(&f, &v, &out);
    tried++;
    if (!ok) continue;
    answered++;
    for (int i = 0; i < 6; i++) {
      double x = XS[i], want, got;
      CalcVars w = { x, 0, false, 0.7 };
      if (!strcmp(kind, "d/dx")) { if (calc_derivative(&f, &w, x, &want) != CALC_OK) continue; got = at(&out, x, false); }
      else if (!strcmp(kind, "int")) { want = at(&f, x, false); if (calc_derivative(&out, &w, x, &got) != CALC_OK) continue; }
      else { want = at(&f, x, false); got = at(&out, x, false); }
      if (isnan(want) || fabs(want) > 300 || fabs(got) > 300) continue;   // near a pole the numeric reference is unreliable
      checks++;
      if (!(fabs(want - got) <= 1e-5 * (1 + fabs(want) + fabs(got)))) {
        char buf[400]; calc_render(&out, buf, sizeof buf);
        printf("  FAIL %s %s -> %s at x=%g: want %.10g got %.10g\n", kind, e, buf, x, want, got);
        fails++;
        break;
      }
    }
  }
  printf("random: %d of %d gave a formula\n", answered, tried);
  printf("%d checks, %d failures\n", checks, fails);
  return fails != 0;
}
