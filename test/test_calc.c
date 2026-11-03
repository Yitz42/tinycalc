// PC tests for mmath + calc. Build: see test/run.sh
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/c/calc.h"
#include "../src/c/mmath.h"

static int fails = 0, checks = 0;

static void check_rel(const char *name, double got, double want, double tol) {
  checks++;
  double err = fabs(got - want) / (fabs(want) > 1e-300 ? fabs(want) : 1.0);
  if (isnan(want) ? !isnan(got) : !(err <= tol)) {
    fails++;
    printf("FAIL %s: got %.17g want %.17g (rel %.3g)\n", name, got, want, err);
  }
}

// Build a token buffer from a compact string. Multi-char tokens in {braces}.
static TokBuf toks(const char *s) {
  static const struct { const char *n; int t; } map[] = {
    {"E", T_EE}, {"pi", T_PI}, {"e", T_E}, {"x", T_X}, {"y", T_Y}, {"Ans", T_ANS},
    {"sq", T_SQ}, {"cube", T_CUBE}, {"recip", T_RECIP}, {"%", T_PCT}, {"!", T_FACT},
    {"sin", T_SIN}, {"cos", T_COS}, {"tan", T_TAN}, {"asin", T_ASIN},
    {"acos", T_ACOS}, {"atan", T_ATAN}, {"ln", T_LN}, {"log", T_LOG},
    {"exp", T_EXP}, {"tenx", T_TENX}, {"sqrt", T_SQRT}, {"cbrt", T_CBRT}, {"abs", T_ABS},
    {"nCr", T_NCR}, {"nPr", T_NPR}, {"mod", T_MOD}, {"floor", T_FLOOR}, {"sinh", T_SINH},
    {"gcd", T_GCD}, {"round", T_ROUND}, {"logb", T_LOGB}, {"root", T_ROOT}, {"sum", T_SUM}, {"prod", T_PROD},
    {"A", T_MA}, {"B", T_MB}, {"i", T_I}, {"conj", T_CONJ}, {"re", T_RE}, {"im", T_IM}, {"arg", T_ARG},
    {"z", T_Z}, {"Ran", T_RAN}, {"RanInt", T_RANINT}, {"npdf", T_NPDF}, {"ncdf", T_NCDF}, {"ninv", T_NINV},
  };
  TokBuf b; b.n = 0;
  for (const char *p = s; *p; p++) {
    if (*p == ' ') continue;
    if (*p == '{') {
      const char *q = strchr(p, '}');
      int len = (int)(q - p - 1), found = 0;
      for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
        if ((int)strlen(map[i].n) == len && !strncmp(map[i].n, p + 1, len)) {
          b.t[b.n++] = (uint8_t)map[i].t; found = 1;
        }
      if (!found) { printf("bad token %.*s\n", len, p + 1); exit(2); }
      p = q;
    } else {
      b.t[b.n++] = (uint8_t)*p;
    }
  }
  return b;
}

static void ev(const char *s, double want, int deg) {
  TokBuf b = toks(s);
  CalcVars v = { 2.0, 10.0, deg };
  double out = -12345;
  CalcErr e = calc_eval(&b, &v, &out);
  char name[160]; snprintf(name, sizeof name, "eval %s", s);
  checks++;
  if (e != CALC_OK) { fails++; printf("FAIL %s: error %d\n", name, e); return; }
  checks--;
  check_rel(name, out, want, 1e-12);
}

static void ev_err(const char *s, CalcErr want) {
  TokBuf b = toks(s);
  CalcVars v = { 2.0, 10.0, 1 };
  double out;
  CalcErr e = calc_eval(&b, &v, &out);
  checks++;
  if (e != want) { fails++; printf("FAIL err %s: got %d want %d\n", s, e, want); }
}

static void fmt(double v, const char *want) {
  char buf[40];
  calc_format(v, 10, buf, sizeof buf);
  checks++;
  if (strcmp(buf, want)) { fails++; printf("FAIL fmt %.17g: got %s want %s\n", v, buf, want); }
}

// The nearest root to x0, the way the calculator's own = on an equation picks it.
static CalcErr calc_solve(const TokBuf *b, const CalcVars *v, double x0, double *root) {
  if (!calc_has_x(b)) return CALC_ERR_NOX;
  double rs[12];
  for (double half = 10; half <= 10000; half *= 10) {
    int n = calc_roots(b, v, x0 - half, x0 + half, rs, 12), best = 0;
    if (!n) continue;
    for (int i = 1; i < n; i++) if (fabs(rs[i] - x0) < fabs(rs[best] - x0)) best = i;
    *root = rs[best];
    return CALC_OK;
  }
  return CALC_ERR_NOROOT;
}

static void solve(const char *s, double guess, double want, int deg) {
  TokBuf b = toks(s);
  CalcVars v = { 0, 0, deg };
  double r = 0;
  CalcErr e = calc_solve(&b, &v, guess, &r);
  char name[160]; snprintf(name, sizeof name, "solve %s from %g", s, guess);
  checks++;
  if (e != CALC_OK) { fails++; printf("FAIL %s: error %d\n", name, e); return; }
  checks--;
  check_rel(name, r, want, 1e-9);
}
static void solve_fail(const char *s, double guess) {
  TokBuf b = toks(s);
  CalcVars v = { 0, 0, 0 };
  double r = 0;
  CalcErr e = calc_solve(&b, &v, guess, &r);
  checks++;
  if (e == CALC_OK) { fails++; printf("FAIL solve %s should fail, got %.17g\n", s, r); }
}

static double rnd(double lo, double hi) { return lo + (hi - lo) * (rand() / (double)RAND_MAX); }

static void math_sweep(void) {
  double worst[12] = {0};
  const char *names[12] = {"sqrt","exp","log","sin","cos","tan","atan","asin","acos","pow","cbrt","log10"};
  srand(1);
  for (int i = 0; i < 200000; i++) {
    double x = exp(rnd(-40, 40));
    double a = rnd(-50, 50), u = rnd(-1, 1), t = rnd(-700, 700);
    double got[12] = { m_sqrt(x), m_exp(t), m_log(x), m_sin(a), m_cos(a), m_tan(a),
                       m_atan(a * x), m_asin(u), m_acos(u), m_pow(x, rnd(-5, 5)), m_cbrt(-x), m_log10(x) };
    double y = 0;
    srand(i + 7);  // re-derive pow exponent deterministically
    (void)y;
    double want[12] = { sqrt(x), exp(t), log(x), sin(a), cos(a), tan(a),
                        atan(a * x), asin(u), acos(u), NAN, cbrt(-x), log10(x) };
    for (int k = 0; k < 12; k++) {
      if (k == 9) continue;
      double err;
      if (k >= 3 && k <= 5) {
        // absolute error scaled by magnitude near zeros of sin/cos
        err = fabs(got[k] - want[k]) / (fabs(want[k]) > 1e-3 ? fabs(want[k]) : 1e-3);
      } else {
        err = fabs(got[k] - want[k]) / (fabs(want[k]) > 1e-300 ? fabs(want[k]) : 1.0);
      }
      if (err > worst[k]) worst[k] = err;
    }
  }
  // pow separately
  srand(3);
  for (int i = 0; i < 100000; i++) {
    double x = exp(rnd(-20, 20)), y = rnd(-10, 10);
    double err = fabs(m_pow(x, y) - pow(x, y)) / pow(x, y);
    if (err > worst[9]) worst[9] = err;
  }
  for (int k = 0; k < 12; k++) {
    double lim = (k == 9) ? 1e-13 : 1e-14;
    checks++;
    printf("  %-6s worst rel err %.2e%s\n", names[k], worst[k], worst[k] > lim ? "  <-- FAIL" : "");
    if (worst[k] > lim) fails++;
  }
}

static void keys(Calc *c, const int *k) { while (*k) calc_key(c, *k++); }

// complex answers: type s (compact token form), =, and compare the big line
static void cplx(const char *s, bool deg, const char *want) {
  Calc c; calc_init(&c);
  c.v.deg = deg;
  TokBuf b = toks(s);
  for (int i = 0; i < b.n; i++) calc_key(&c, b.t[i]);
  calc_key(&c, K_EVAL);
  char sm[128], bg[128];
  calc_display(&c, sm, sizeof sm, bg, sizeof bg);
  checks++;
  if (strcmp(bg, want)) { fails++; printf("FAIL complex %s: got [%s] want [%s]\n", s, bg, want); }
}

static void disp(const Calc *c, const char *ws, const char *wb) {
  char s[128], b[128];
  calc_display(c, s, sizeof s, b, sizeof b);
  checks++;
  if (strcmp(s, ws) || strcmp(b, wb)) {
    fails++;
    printf("FAIL display: got [%s] [%s] want [%s] [%s]\n", s, b, ws, wb);
  }
}

int main(void) {
  printf("math accuracy vs libm:\n");
  math_sweep();

  // parsing & precedence
  ev("1+1", 2, 1);
  ev("2+3*4", 14, 1);
  ev("(2+3)*4", 20, 1);
  ev("2^3^2", 512, 1);
  ev("-2^2", -4, 1);
  ev("2^-2", 0.25, 1);
  ev("2{pi}", 2 * M_PI, 1);
  ev("3(4+1)", 15, 1);
  ev("2{x}{sq}", 8, 1);          // x = 2 -> 2*x^2
  ev("{Ans}/4", 2.5, 1);         // Ans = 10
  ev("(1+2", 3, 1);              // auto-close
  ev("{sqrt}16", 4, 1);
  ev("{sqrt}2{sq}", 2, 1);       // sqrt(2²) auto-closed
  ev("{sin}30", 0.5, 1);
  ev("{sin}180", 0, 1);
  ev("{cos}90", 0, 1);
  ev("{tan}45", 1, 1);
  ev("{asin}1", 90, 1);
  ev("{sin}({pi}/6)", 0.5, 0);
  ev("{ln}{e}", 1, 1);
  ev("{log}1000", 3, 1);
  ev("{tenx}3", 1000, 1);
  ev("{exp}1", M_E, 1);
  ev("5{!}", 120, 1);
  ev("50{%}", 0.5, 1);
  ev("4{recip}", 0.25, 1);
  ev("3{cube}", 27, 1);
  ev("{cbrt}-27", -3, 1);
  ev("{abs}-3.5", 3.5, 1);
  ev("1.5{E}3", 1500, 1);
  ev("1{E}-3", 0.001, 1);
  ev("{E}6", 1e6, 1);
  ev("0.1+0.2", 0.3, 1);
  ev("--3", 3, 1);
  ev("2*-3", -6, 1);
  ev("{logb}2,8)", 3, 0);
  ev("{logb}10,1000)", 3, 0);
  ev("{root}3,-8)", -2, 0);
  ev("{root}4,16)", 2, 0);
  ev("{root}2,2)", 1.4142135623730951, 0);
  ev_err("{root}2,-4)", CALC_ERR_MATH);
  ev("{sum}{x},1,100)", 5050, 0);
  ev("{sum}1/{x}{sq},1,1000)+{x}", 1.6439345666815615 + 2, 0);   // x outside the sum is still x (2)
  ev("{prod}{x},1,5)", 120, 0);
  ev("{sum}{sum}{x},1,3),1,2)", 12, 0);       // the inner sum doesn't depend on the outer x
  ev("2{sum}{x},1,3)", 12, 0);
  ev_err("{sum}{x},1,100000)", CALC_ERR_MATH);
  ev_err("{sum}{x},1)", CALC_ERR_SYNTAX);
  ev_err("{i}", CALC_ERR_MATH);                       // no real value
  ev("{re}5)+{im}5)+{conj}5)", 10, 0);
  ev("{arg}-2)", 180, 1);
  ev("(-8)^(1/3)", -2, 0);
  ev("(-8)^(2/3)", 4, 0);
  ev("(-32)^0.2", -2, 0);
  ev_err("(-8)^(1/2)", CALC_ERR_MATH);
  ev("2*5{nCr}2", 20, 0);
  ev("5{nCr}6", 0, 0);
  ev("1{E}20{mod}7", 2, 0);
  ev("{floor}(0.7+0.1)*10)", 8, 0);
  ev("0.7{mod}0.1", 0, 0);
  ev("-7{mod}3", 2, 0);
  ev("7{mod}-3", -2, 0);
  ev("{gcd}1{E}15+1,1{E}15)", 1, 0);
  ev("0.5{!}", 0.886226925452758, 0);
  ev("(-0.5){!}", 1.772453850905516, 0);
  ev("4.5{!}", 52.34277778455352, 0);
  ev("{sinh}1{E}-10)", 1e-10, 0);
  ev("{sin}{pi})", 0, 0);
  ev("{cos}{pi}/2)", 0, 0);
  ev("{sin}1{E}10)", -0.4875060250875107, 0);
  ev_err("1+", CALC_ERR_SYNTAX);
  ev_err("1..2", CALC_ERR_SYNTAX);
  ev_err("1/0", CALC_ERR_MATH);
  ev_err("{sqrt}-1", CALC_ERR_MATH);
  ev_err("{ln}0", CALC_ERR_MATH);
  ev_err("{tan}90", CALC_ERR_MATH);
  ev_err("2)", CALC_ERR_SYNTAX);
  ev_err("{x}=2", CALC_ERR_EQ);
  ev_err("10^400", CALC_ERR_MATH);
  ev_err("((((((((((((((((((((1))))))))))))))))))))", CALC_ERR_SYNTAX);

  // formatting
  fmt(2, "2"); fmt(-2.5, "-2.5"); fmt(1.0 / 3, "0.3333333333");
  fmt(2.0 / 3, "0.6666666667"); fmt(1234567890, "1234567890");
  fmt(12345678901.0, "1.23456789e10"); fmt(0.0001, "0.0001");
  fmt(0.00001234, "0.00001234"); fmt(0.00001, "0.00001"); fmt(1.23456789e-7, "1.23456789e-7"); fmt(1e100, "1e100"); fmt(6.02214076e23, "6.02214076e23");
  fmt(0.1 + 0.2, "0.3"); fmt(M_PI, "3.141592654"); fmt(-1e-300, "-1e-300");
  fmt(9.9999999999, "10"); fmt(1e-320, "9.999888672e-321");

  // solver
  solve("{x}{sq}=2", 1, sqrt(2), 0);
  solve("{x}{sq}=2", -1, -sqrt(2), 0);
  solve("{x}{cube}-2{x}-5", 2, 2.0945514815423265, 0);
  solve("{cos}{x}={x}", 0, 0.7390851332151607, 0);
  solve("{exp}{x}=10", 0, log(10), 0);
  solve("{x}^{x}=100", 1, 3.5972850235404175, 0);
  solve("{sin}{x}=0.5", 10, 30, 1);
  solve("{x}{sq}=0", 1, 0, 0);
  solve("{x}{cube}=27", 0, 3, 0);
  solve("{ln}{x}=2", 5, exp(2), 0);       // guess in domain
  solve("{ln}{x}=2", -5, exp(2), 0);      // guess outside domain
  solve("{sqrt}{x}=3", 0, 9, 0);
  solve("1000(1+{x})^10=2000", 0, pow(2, 0.1) - 1, 0);
  solve("{x}{sq}=1{E}-12", 1, 1e-6, 0);
  solve("{x}=5", 0, 5, 0);
  solve("{abs}{x})-3", 1, 3, 0);
  solve("{abs}{x}-3", 1, 3, 0);   // abs(x-3): touches zero, no sign change
  solve("{x}{cube}", 0.5, 0, 0);
  solve("{atan}{x}=1", 0, tan(1), 0);
  solve_fail("{x}{sq}+1", 0);
  solve_fail("1/{x}", 1);
  solve_fail("{exp}{x}", 0);
  {
    TokBuf b = toks("2+2"); CalcVars v = {0,0,0}; double r;
    checks++;
    if (calc_solve(&b, &v, 0, &r) != CALC_ERR_NOX) { fails++; printf("FAIL no-x\n"); }
  }

  // key state machine
  {
    Calc c; calc_init(&c);
    int k1[] = {'1','+','1',K_EVAL,0}; keys(&c,k1); disp(&c,"1+1 =","2");
    int k2[] = {'*','3',K_EVAL,0}; keys(&c,k2); disp(&c,"Ans*3 =","6");
    int k3[] = {'5',0}; keys(&c,k3); disp(&c,"","5");
    int k4[] = {K_DEL,K_DEL,0}; keys(&c,k4); disp(&c,"","0");
    int k5[] = {'1',K_STOX,T_X,0}; keys(&c,k5); disp(&c,"Stored to x","1");
    int k6[] = {T_X,T_SQ,'=','2',K_SOLVE,0}; keys(&c,k6); disp(&c,"x^2=2","x = 1.414213562");   // nearest x = 1
    int k7[] = {K_RCLX,T_X,0}; keys(&c,k7); disp(&c,"x =","1.414213562");
    int k8[] = {T_SQ,K_EVAL,0}; keys(&c,k8); disp(&c,"Ans^2 =","2");
    int k9[] = {K_AC,'1','/','0',K_EVAL,0}; keys(&c,k9); disp(&c,"1/0","Math error");
    int k10[] = {K_DEL,K_DEL,'4',K_EVAL,0}; keys(&c,k10); disp(&c,"1/4 =","0.25");
    int k11[] = {K_STOX,T_X,0}; keys(&c,k11); disp(&c,"Stored to x","0.25");
    int k12[] = {K_AC,'2',T_X,K_EVAL,0}; keys(&c,k12); disp(&c,"","2x=");        // = types the equation sign
    int k12b[] = {'1',K_EVAL,0}; keys(&c,k12b); disp(&c,"2x=1","x = 0.5");      // a second = solves
    int k13[] = {K_AC,T_X,'=','3',K_EVAL,0}; keys(&c,k13); disp(&c,"x=3","x = 3");   // = solves equations
    int k16[] = {K_AC,'2',K_SOLVE,0}; keys(&c,k16); disp(&c,"2","No x to solve");
    // STO / RCL ask for x or y; Clr x,y; anything else cancels STO
    int k17[] = {K_AC,'7',K_STOX,0}; keys(&c,k17); disp(&c,"STO to: x, y or MEM","7");
    { Calc m; calc_init(&m);
      int a1[] = {'7',K_STOX,T_MA,0}; keys(&m,a1); disp(&m,"Stored to A","7");
      int a2[] = {T_MA,'*','2',K_EVAL,0}; keys(&m,a2); disp(&m,"A*2 =","14");
      int a3[] = {K_RCLX,T_MA,0}; keys(&m,a3); disp(&m,"A =","7");
      int a4[] = {K_AC,'3',',','4',K_POL,0}; keys(&m,a4); disp(&m,"r = 5","deg = 53.13010235");
      int a5[] = {K_AC,'2',',','6','0',K_REC,0}; keys(&m,a5); disp(&m,"x = 1","y = 1.732050808");
      int a6[] = {K_AC,K_POL,0}; keys(&m,a6); disp(&m,"r = 2","deg = 60");      // from x and y
      int a7[] = {K_AC,'0','1',0}; keys(&m,a7); disp(&m,"","1");               // 0 then 1 is 1
      int a8[] = {K_AC,'0','.','0','1',0}; keys(&m,a8); disp(&m,"","0.01");
      int a9[] = {K_AC,'1','0','0',0}; keys(&m,a9); disp(&m,"","100"); }
    int k18[] = {T_Y,0}; keys(&c,k18); disp(&c,"Stored to y","7");
    int k19[] = {K_AC,T_Y,'+','1',K_EVAL,0}; keys(&c,k19); disp(&c,"","y+1=");
    int k20[] = {K_AC,K_RCLX,T_Y,0}; keys(&c,k20); disp(&c,"y =","7");
    int k21[] = {K_CLRXY,0}; keys(&c,k21); disp(&c,"Cleared x, y, z","0");
    int k22[] = {K_RCLX,T_Y,0}; keys(&c,k22); disp(&c,"y =","0");
    int k23[] = {K_AC,'5',K_STOX,'+','1',K_EVAL,0}; keys(&c,k23); disp(&c,"5+1 =","6");
    // "/" after typing: all of it goes on top
    int k24[] = {K_AC,'1','+','2','/','3',K_EVAL,0}; keys(&c,k24); disp(&c,"1+2/3 =","1.666666667");
    int k25[] = {K_AC,'2','*','(','1','+','3','/','4',K_EVAL,0}; keys(&c,k25); disp(&c,"2*(1+3/4 =","3.5");
    int k26[] = {K_AC,'6','/','3',K_EVAL,0}; keys(&c,k26); disp(&c,"6/3 =","2");
    int k27[] = {K_AC,'1','E','-','2','/','2',K_EVAL,0}; (void)k27;
  }


  // complex numbers
  cplx("(2+3{i})(1-{i})", 0, "5+i");
  cplx("{sqrt}-4)", 1, "2i");
  cplx("{ln}-1)", 1, "3.141592654i");
  cplx("{i}{sq}", 0, "-1");
  cplx("{exp}{i}{pi})", 0, "-1");
  cplx("(1+{i})/(1-{i})", 0, "i");
  cplx("{abs}3+4{i})", 0, "5");
  cplx("{arg}{i})", 1, "90");
  cplx("{conj}2+3{i})", 0, "2-3i");
  cplx("{asin}2)", 1, "90-75.45613i");
  cplx("{sin}{i})", 0, "1.175201194i");
  cplx("(-4)^0.5", 0, "2i");
  cplx("{i}^{i}", 0, "0.2078795764");
  cplx("{sqrt}{i})", 0, "0.7071068+0.7071068i");
  cplx("{logb}2,-8)", 0, "3+4.53236i");
  cplx("{root}2,-9)", 0, "3i");
  cplx("{sum}{x}{i},1,3)", 0, "6i");
  cplx("2+2", 0, "4");                                 // still real
  { Calc c; calc_init(&c);
    int k1[] = {'1','+',T_I,K_EVAL,'*','2',K_EVAL,0}; keys(&c,k1); disp(&c,"Ans*2 =","2+2i");   // complex Ans
    int k2[] = {K_AC,'3','+','4',T_I,K_EVAL,K_POL,0}; keys(&c,k2); disp(&c,"r = 5","deg = 53.13010235");
    int k3[] = {K_AC,'5',K_EVAL,'+','1',K_EVAL,0}; keys(&c,k3); disp(&c,"Ans+1 =","6"); }   // back to real

  // all roots in an interval
  {
    struct { const char *e; double lo, hi; int deg; int n; double r[4]; } R[] = {
      { "{x}{sq}=2", -10, 10, 0, 2, { -1.4142135623730951, 1.4142135623730951 } },
      { "{x}{cube}-{x}", -10, 10, 0, 3, { -1, 0, 1 } },
      { "{x}{sq}", -10, 10, 0, 1, { 0 } },                 // touching root
      { "({x}-3){sq}", -10, 10, 0, 1, { 3 } },
      { "{sin}{x}=0.5", -360, 360, 1, 4, { -330, -210, 30, 150 } },
      { "1/{x}", -10, 10, 0, 0, { 0 } },                    // pole, no root
      { "{tan}{x}", -4, 4, 0, 3, { -3.141592653589793, 0, 3.141592653589793 } },
      { "{x}{sq}+1", -10, 10, 0, 0, { 0 } },
      { "{ln}{x}=1", -10, 10, 0, 1, { 2.718281828459045 } },
      { "{exp}{x}=0", -100, 100, 0, 0, { 0 } },
      { "{cos}{x}={x}", -10, 10, 0, 1, { 0.7390851332151607 } },
      { "({x}{sq}-2){sq}", -10, 10, 0, 2, { -1.4142135623730951, 1.4142135623730951 } },   // double roots
      { "({x}-1)({x}-1.0000001)", -10, 10, 0, 2, { 1, 1.0000001 } },                     // very close
      { "{x}{sq}=-1{E}-12", -10, 10, 0, 0, { 0 } },                                       // a near miss
      { "({x}-0.3)^4", -10, 10, 0, 1, { 0.3 } },
      { "{cos}{x}=1", -1, 7, 0, 2, { 0, 6.283185307179586 } },
    };
    for (unsigned k = 0; k < sizeof R / sizeof R[0]; k++) {
      TokBuf b = toks(R[k].e);
      CalcVars v = { 0, 0, R[k].deg };
      double out[12];
      int n = calc_roots(&b, &v, R[k].lo, R[k].hi, out, 12);
      checks++;
      if (n != R[k].n) {
        fails++;
        printf("FAIL roots %s: got %d roots:", R[k].e, n);
        for (int i = 0; i < n; i++) printf(" %.12g", out[i]);
        printf(" want %d\n", R[k].n);
        continue;
      }
      for (int i = 0; i < n; i++) {
        char nm[80]; snprintf(nm, sizeof nm, "root %s #%d", R[k].e, i);
        if (R[k].r[i] == 0) { checks++; if (fabs(out[i]) > 1e-9) { fails++; printf("FAIL %s: %.17g\n", nm, out[i]); } }
        else check_rel(nm, out[i], R[k].r[i], 1e-9);
      }
    }
  }

  // systems of two equations
  {
    struct { const char *a, *b; int n; double s[4][2]; } S[] = {
      { "{x}+{y}=3", "{x}-{y}=1", 1, { { 2, 1 } } },
      { "{x}{sq}+{y}{sq}=25", "{y}={x}+1", 2, { { -4, -3 }, { 3, 4 } } },
      { "{y}={x}{sq}", "{y}=2{x}+3", 2, { { -1, 1 }, { 3, 9 } } },
      { "{x}{y}=6", "{x}+{y}=5", 2, { { 2, 3 }, { 3, 2 } } },
      { "{x}{sq}+{y}{sq}=1", "{y}=5", 0, { { 0 } } },
      { "{x}+{y}=1", "{x}+{y}=2", 0, { { 0 } } },       // parallel lines
      { "{y}={exp}{x}", "{y}=2", 1, { { 0.6931471805599453, 2 } } },
      { "2{x}+3{y}=7", "{y}=1", 1, { { 2, 1 } } },
    };
    for (unsigned k = 0; k < sizeof S / sizeof S[0]; k++) {
      TokBuf a = toks(S[k].a), b = toks(S[k].b);
      CalcVars v = { 0, 0, 0, 0 };
      double xs[12], ys[12];
      int n = calc_solve2(&a, &b, &v, -10, 10, -10, 10, xs, ys, 12);
      checks++;
      if (n != S[k].n) {
        fails++;
        printf("FAIL system %s ; %s: got %d:", S[k].a, S[k].b, n);
        for (int i = 0; i < n; i++) printf(" (%.10g, %.10g)", xs[i], ys[i]);
        printf("\n");
        continue;
      }
      for (int i = 0; i < n; i++) {
        char nm[120]; snprintf(nm, sizeof nm, "system %s #%d", S[k].a, i);
        check_rel(nm, xs[i], S[k].s[i][0], 1e-9);
        check_rel(nm, ys[i], S[k].s[i][1], 1e-9);
      }
    }
    // key flow: x+y=3, SOLVE, x-y=1, SOLVE -> x = 2, y = 1
    Calc c; calc_init(&c);
    int k1[] = {T_X,'+',T_Y,'=','3',K_SOLVE,0}; keys(&c,k1); disp(&c,"1: x+y=3","2nd equation?");
    int k2[] = {T_X,'-',T_Y,'=','1',0}; keys(&c,k2); disp(&c,"1: x+y=3","2: x-y=1");
    int k3[] = {K_SOLVE,0}; keys(&c,k3); disp(&c,"x = 2","y = 1");
    int k4[] = {T_Y,'*','2',K_EVAL,0}; keys(&c,k4); disp(&c,"","y*2=");
    int k5[] = {K_AC,T_X,T_Y,'=','1',K_SOLVE,K_DEL,0}; keys(&c,k5); disp(&c,"","x*y=1");
    // three equations: x+y+z=6, x-y+z=2, 2x+y-z=1 -> 1, 2, 3
    { Calc t; calc_init(&t);
      int a1[] = {T_X,'+',T_Y,'+',T_Z,'=','6',K_EVAL,0}; keys(&t,a1); disp(&t,"1: x+y+z=6","2nd equation?");
      int a2[] = {T_X,'-',T_Y,'+',T_Z,'=','2',K_EVAL,0}; keys(&t,a2); disp(&t,"2: x-y+z=2","3rd equation?");
      int a3[] = {'2',T_X,'+',T_Y,'-',T_Z,'=','1',K_EVAL,0}; keys(&t,a3); disp(&t,"x = 1, y = 2","z = 3");
      int a4[] = {K_DEL,0}; keys(&t,a4); disp(&t,"2: x-y+z=2","3: 2x+y-z=1");        // back to edit the third
      int a5[] = {K_DEL,K_DEL,K_DEL,K_DEL,K_DEL,K_DEL,K_DEL,K_DEL,K_DEL,0}; keys(&t,a5);
      disp(&t,"1: x+y+z=6","2: x-y+z=2");                                               // ...and the second
      int a6[] = {K_EVAL,'2',T_X,'+','2',T_Y,'+','2',T_Z,'=','1',K_EVAL,0}; keys(&t,a6);
      disp(&t,"2x+2y+2z=1","No solution found");                                          // parallel planes
      int a7[] = {K_AC,T_X,T_Z,'=','1',K_EVAL,T_Y,'=','1',K_EVAL,T_Z,'=','2',K_EVAL,0}; keys(&t,a7);
      disp(&t,"z=2","3 unknowns: linear only");
      int a8[] = {K_AC,'5',K_STOX,T_Z,K_AC,T_Z,'*','2',K_EVAL,0}; keys(&t,a8); disp(&t,"z*2 =","10"); }
  }

  // integrals and derivatives
  {
    struct { const char *f; double a, b; int deg; double want, tol; } I[] = {
      { "{x}{sq}", 0, 1, 0, 1.0 / 3, 1e-12 },
      { "{sin}{x}", 0, M_PI, 0, 2, 1e-12 },
      { "{sin}{x}", 0, 180, 1, 360 / M_PI, 1e-12 },     // degrees: d(x°) = pi/180 dx
      { "1/{x}", 1, M_E, 0, 1, 1e-12 },
      { "{sqrt}{x}", 0, 1, 0, 2.0 / 3, 1e-10 },
      { "1/{sqrt}{x}", 0, 1, 0, 2, 1e-5 },              // integrable singularity
      { "{exp}(-{x}{sq})", -5, 5, 0, 1.7724538509055159, 1e-11 },
      { "{x}{cube}", 2, -1, 0, -3.75, 1e-12 },             // reversed limits
      { "5", 0, 3, 0, 15, 1e-13 },
      { "{abs}{x}", -1, 2, 0, 2.5, 1e-11 },              // kink
      { "{x}^(-0.9)", 0, 1, 0, 10, 1e-9 },               // strong end singularity
      { "{ln}{x}", 0, 1, 0, -1, 1e-9 },
      { "{sin}{x})/{x}", -1, 2, 0, 2.551496047169878, 1e-10 },   // a node on the hole at 0
    };
    for (unsigned k = 0; k < sizeof I / sizeof I[0]; k++) {
      TokBuf b = toks(I[k].f);
      CalcVars v = { 0, 0, I[k].deg, 0 };
      double out; bool approx;
      CalcErr e = calc_integrate(&b, &v, I[k].a, I[k].b, &out, &approx);
      char nm[80]; snprintf(nm, sizeof nm, "integral %s", I[k].f);
      checks++;
      if (e != CALC_OK) { fails++; printf("FAIL %s: err %d\n", nm, e); continue; }
      checks--;
      check_rel(nm, out, I[k].want, I[k].tol);
    }
    struct { const char *f; double x; int deg; double want; } D[] = {
      { "{x}{cube}", 2, 0, 12 },
      { "{sin}{x}", 0, 0, 1 },
      { "{exp}{x}", 1, 0, M_E },
      { "{ln}{x}", 2, 0, 0.5 },
      { "{sqrt}{x}", 1e-6, 0, 500 },            // close to the edge of the domain
      { "{x}^{x}", 2, 0, 4 * (log(2) + 1) },
      { "{sin}{x}", 60, 1, M_PI / 180 * 0.5 },  // degrees
      { "1/{x}", -2, 0, -0.25 },
      { "1/{x}", 0.001, 0, -1e6 },              // a pole close by
      { "{sin}{x}", 1000, 0, 0.5623790762907029 },
      { "{abs}{x}", 2, 0, 1 },
    };
    for (unsigned k = 0; k < sizeof D / sizeof D[0]; k++) {
      TokBuf b = toks(D[k].f);
      CalcVars v = { 0, 0, D[k].deg, 0 };
      double out;
      CalcErr e = calc_derivative(&b, &v, D[k].x, &out);
      char nm[80]; snprintf(nm, sizeof nm, "derivative %s at %g", D[k].f, D[k].x);
      checks++;
      if (e != CALC_OK) { fails++; printf("FAIL %s: err %d\n", nm, e); continue; }
      checks--;
      check_rel(nm, out, D[k].want, 1e-9);
    }
    {
      TokBuf b = toks("{sqrt}{x}"); CalcVars v = {0,0,0,0}; double out; bool ap;
      checks += 3;
      if (calc_derivative(&b, &v, -1, &out) == CALC_OK) { fails++; printf("FAIL d sqrt at -1\n"); }
      TokBuf l = toks("{ln}{x}");
      if (calc_integrate(&l, &v, -1, 1, &out, &ap) == CALC_OK) { fails++; printf("FAIL int ln -1..1\n"); }
      TokBuf q = toks("{x}=1");
      if (calc_integrate(&q, &v, 0, 1, &out, &ap) != CALC_ERR_EQ) { fails++; printf("FAIL int with =\n"); }
      // not differentiable, or diverging: an error, not a number
      static const char *ND[] = { "{abs}{x}", "{cbrt}{x}", "{floor}({x}+1)" };
      for (int k = 0; k < 3; k++) {
        TokBuf t = toks(ND[k]); checks++;
        if (calc_derivative(&t, &v, 0, &out) == CALC_OK) { fails++; printf("FAIL d %s at 0 gave %g\n", ND[k], out); }
      }
      static const char *DV[] = { "1/{x}", "1/{x}{sq}" };
      for (int k = 0; k < 2; k++) {
        TokBuf t = toks(DV[k]); checks++;
        if (calc_integrate(&t, &v, -1, 1, &out, &ap) == CALC_OK) { fails++; printf("FAIL int %s -1..1 gave %g\n", DV[k], out); }
      }
    }
    // key flows
    Calc c; calc_init(&c);
    int k1[] = {T_X,T_SQ,K_INT,0}; keys(&c,k1); disp(&c,"int x^2","from = ");
    int k2[] = {'0',K_EVAL,0}; keys(&c,k2); disp(&c,"int x^2","to = ?");
    int k3[] = {'3',K_EVAL,0}; keys(&c,k3); disp(&c,"int 0 to 3 =","9");
    int k4[] = {'+','1',K_EVAL,0}; keys(&c,k4); disp(&c,"Ans+1 =","10");
    int k5[] = {K_AC,T_X,T_CUBE,K_DER,0}; keys(&c,k5); disp(&c,"d/dx x^3","at x = ");
    int k6[] = {'2',K_EVAL,0}; keys(&c,k6); disp(&c,"d/dx at 2 =","12");
    int k7[] = {K_INT,'1',K_EVAL,'2',K_EVAL,0}; keys(&c,k7); disp(&c,"int 1 to 2 =","3.75");  // reuses x³
    int k8[] = {K_DEL,0}; keys(&c,k8); disp(&c,"","x^3");
    int k9[] = {K_DER,K_DEL,0}; keys(&c,k9); disp(&c,"","x^3");
  }

  // cursor editing
  {
    Calc c; calc_init(&c);
    int k1[] = {'1','+','3',0}; keys(&c,k1);
    checks++; if (!calc_set_cursor(&c, 2)) { fails++; printf("FAIL set cursor\n"); }
    disp(&c,"","1+|3");
    int k2[] = {'2',0}; keys(&c,k2); disp(&c,"","1+2|3");   // typed in order, cursor moves on
    int k3[] = {'*',0}; keys(&c,k3); disp(&c,"","1+2*|3");
    int k4[] = {K_DEL,K_DEL,0}; keys(&c,k4); disp(&c,"","1+|3");
    calc_set_cursor(&c, 0);
    int k5[] = {K_DEL,'(',0}; keys(&c,k5); disp(&c,"","(|1+3");
    int k6[] = {K_EVAL,0}; keys(&c,k6); disp(&c,"(1+3 =","4");
    checks++; if (calc_set_cursor(&c, 0)) { fails++; printf("FAIL cursor in result\n"); }
    int k7[] = {K_DEL,0}; keys(&c,k7); disp(&c,"","(1+3");          // back to editing, cursor at end
    calc_set_cursor(&c, 99); disp(&c,"","(1+3|");                    // clamped
    int k7b[] = {'5',0}; keys(&c,k7b); disp(&c,"","(1+35");          // typing at the end: cursor off
    calc_set_cursor(&c, 1); int k7c[] = {'2','3',0}; keys(&c,k7c); disp(&c,"","(23|1+35");
    int k8[] = {K_AC,0}; keys(&c,k8); disp(&c,"","0");
    calc_set_cursor(&c, 0); disp(&c,"","|");
  }

  // the one "=" key
  {
    Calc c; calc_init(&c);
    int k1[] = {T_X,T_SQ,K_EVAL,0}; keys(&c,k1); disp(&c,"","x^2=");          // types the sign
    int k2[] = {'4',K_EVAL,0}; keys(&c,k2); disp(&c,"x^2=4","x = 2");         // solves; -2 and 2 tie for nearest to x=0: the larger
    int k3[] = {K_AC,'2',T_X,K_EVAL,K_EVAL,0}; keys(&c,k3); disp(&c,"2x =","4");  // 2x = = : uses stored x (2)
    int k4[] = {K_AC,'2','+','3',K_EVAL,0}; keys(&c,k4); disp(&c,"2+3 =","5");  // no x: just calculates
    int k5[] = {K_AC,T_X,'+',T_Y,K_EVAL,'3',K_EVAL,T_X,'-',T_Y,K_EVAL,'1',K_EVAL,0}; keys(&c,k5);
    disp(&c,"x = 2","y = 1");
  }

  // the math review's findings
  {
    cplx("{tan}90", true, "Math error"); cplx("{tan}(-90)", true, "Math error"); cplx("{tan}270", true, "Math error");
    ev("{sin}30)-0.5", 0, 1); ev("{tan}45)-1", 0, 1); ev("{cos}60)-0.5", 0, 1); ev("{asin}0.5)-30", 0, 1);
    ev("{cos}(-120)", -0.5, 1); ev("{tan}135", -1, 1);
    ev("{sin}(1{E}15+30)", -0.766044443118978, 1);       // whole turns removed exactly
    ev("{sin}1000000000000001", 0.0318891292809201, 0);  // not set to 0
    ev("2000000000001{mod}2000000000000", 1, 1);
    ev("(-8)^(1/33)", -1.065041089439963, 1);
    cplx("{atan}(2{i})", false, "1.570796+0.5493061i");  // C99's branch: +pi/2
    // the probability functions live in the complex evaluator
    { const char *f[] = { "{ncdf}1.96", "{ncdf}(-10)", "{ninv}0.975", "{ninv}1{E}-20", "{npdf}0", "2{ncdf}0)+1" };
      const double w[] = { 0.9750021048517795, 7.619853024160593e-24, 1.959963984540054, -9.262340089798405, 0.3989422804014327, 2 };
      for (int k = 0; k < 6; k++) {
        TokBuf b = toks(f[k]); CalcVars v = { 0 }; double re = 0, im = 0;
        checks++;
        if (calc_ceval(&b, &v, &re, &im) != CALC_OK) { fails++; printf("FAIL %s: error\n", f[k]); continue; }
        checks--; check_rel(f[k], re, w[k], 1e-13);
      } }
    { Calc t; calc_init(&t); int lo = 7, hi = 0; double first = -1; bool differ = false;
      for (int k = 0; k < 200; k++) {                    // RanInt(1, 6): 1..6, and = again gives a new one
        int a[] = {K_AC,T_RANINT,'1',',','6',')',K_EVAL,0}; keys(&t,a);
        double o = t.result;
        if (o < lo) lo = (int)o;
        if (o > hi) hi = (int)o;
        if (first < 0) first = o; else if (o != first) differ = true;
        checks++;
        if (t.mode != MODE_RESULT || o != floor(o)) { fails++; printf("FAIL RanInt %g\n", o); break; }
      }
      checks++; if (lo != 1 || hi != 6 || !differ) { fails++; printf("FAIL RanInt range %d..%d\n", lo, hi); }
      int r[] = {K_AC,T_RAN,K_EVAL,0}; keys(&t,r);
      checks++; if (t.mode != MODE_RESULT || t.result < 0 || t.result >= 1) { fails++; printf("FAIL Ran# %g\n", t.result); } }
    struct { const char *f; double a, b; double want; int err; } I2[] = {
      { "{floor}{x}", 0, 10, 45, 0 },                    // jumps: the pieces' total, not tanh-sinh's
      { "{sin}{x}", 0, 1000000, 0, CALC_ERR_INT },       // far too many waves: no number
      { "{x}^(-0.9)", 0, 1, 10, 0 },
    };
    for (unsigned k = 0; k < sizeof I2 / sizeof I2[0]; k++) {
      TokBuf b = toks(I2[k].f);
      CalcVars v = { 0 };
      double out = 0; bool approx;
      CalcErr e = calc_integrate(&b, &v, I2[k].a, I2[k].b, &out, &approx);
      checks++;
      if (e != (CalcErr)I2[k].err || (!e && fabs(out - I2[k].want) > 1e-9 * fabs(I2[k].want))) {
        fails++; printf("FAIL integral %s: err %d, %.15g\n", I2[k].f, e, out);
      }
    }
    { TokBuf b = toks("{x}{abs}{x})"); CalcVars v = { 0 }; double out;   // f'' jumps at 0: no answer
      checks++; if (calc_derivative(&b, &v, 0, &out) == CALC_OK && fabs(out) > 1e-9) { fails++; printf("FAIL d x|x|: %g\n", out); } }
    { TokBuf b = toks("1{E}-20{x}"); CalcVars v = { 0 }; double out = 0;
      checks++; if (calc_derivative(&b, &v, 1, &out) != CALC_OK || fabs(out - 1e-20) > 1e-30) { fails++; printf("FAIL d 1e-20x: %g\n", out); } }
    // FIX rounds like on paper
    calc_fmt = FMT_FIX; calc_fmt_digits = 2;
    { char b[40]; const double v[] = { 1.005, 9.995, 2.675, 0.006 }; const char *w[] = { "1.01", "10.00", "2.68", "0.01" };
      for (int k = 0; k < 4; k++) { calc_format_answer(v[k], b, sizeof b); checks++; if (strcmp(b, w[k])) { fails++; printf("FAIL FIX2 %g: %s\n", v[k], b); } } }
    calc_fmt = FMT_NORM;
    // a linear pair whose answer is far outside the search box; rows of very different sizes
    { Calc t; calc_init(&t);
      int a1[] = {T_X,'+',T_Y,'=','1',T_EE,'6',K_EVAL,T_X,'-','0','.','1','2','3','4','5',T_Y,'=','0',K_EVAL,0}; keys(&t,a1);
      disp(&t,"x = 109884.7301","y = 890115.2699");
      int a2[] = {K_AC,'1',T_EE,'-','7',T_X,'+','1',T_EE,'-','7',T_Y,'+','1',T_EE,'-','7',T_Z,'=','3',T_EE,'-','7',K_EVAL,
                  '1',T_EE,'6',T_X,'-','1',T_EE,'6',T_Y,'=','0',K_EVAL,
                  '1',T_EE,'6',T_X,'+','1',T_EE,'6',T_Y,'-','1',T_EE,'6',T_Z,'=','1',T_EE,'6',K_EVAL,0}; keys(&t,a2);
      disp(&t,"x = 1, y = 1","z = 1");
      Calc u; calc_init(&u);                             // Pol without overflow
      int a4[] = {'1',T_EE,'2','0','0',',','1',T_EE,'2','0','0',K_POL,0}; keys(&u,a4); disp(&u,"r = 1.414213562e200","deg = 45"); }
  }

  // round 2 of the review
  {
    struct { const char *f; double a, b, want, tol; } I3[] = {
      { "{sin}{x})/{x}", 0, 1000, 1.570233121968771, 1e-5 },   // hole at 0 over a long range: no tanh-sinh there
      { "{ln}{x})/({x}-1)", 0, 2, 2.467401100272340, 1e-10 },
      { "1/{sqrt}{abs}({x}-1)", 0, 3, 4.828427124746190, 1e-6 },
    };
    for (unsigned k = 0; k < sizeof I3 / sizeof I3[0]; k++) {
      TokBuf b = toks(I3[k].f); CalcVars v = { 0 }; double out = 0; bool ap;
      checks++;
      if (calc_integrate(&b, &v, I3[k].a, I3[k].b, &out, &ap) != CALC_OK || fabs(out - I3[k].want) > I3[k].tol * fabs(I3[k].want)) {
        fails++; printf("FAIL integral %s: %.15g\n", I3[k].f, out); }
    }
    struct { const char *f; int n; double r[3]; } R2[] = {
      { "{x}{sq}({exp}{x})-1.1)", 2, { 0, 0.0953101798043249 } },     // a root next to a sample at 0
      { "({x}-0.1){sq}({x}-0.2)", 2, { 0.1, 0.2 } },                  // touching beside a crossing
      { "{sqrt}(2-{x}{sq})", 2, { -1.414213562373095, 1.414213562373095 } },   // at the edge of the domain
      { "{sqrt}({x}-0.1)", 1, { 0.1 } },
      { "{x}{sq}+1{E}-15", 0, { 0 } },
    };
    for (unsigned k = 0; k < sizeof R2 / sizeof R2[0]; k++) {
      TokBuf b = toks(R2[k].f); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++;
      bool bad = n != R2[k].n;
      for (int i = 0; i < n && !bad; i++) if (fabs(r[i] - R2[k].r[i]) > 1e-9) bad = true;
      if (bad) { fails++; printf("FAIL roots %s: %d\n", R2[k].f, n); }
    }
    struct { const char *f; double x, want; } D2[] = {
      { "{sin}{x}", 47.121, -0.999995824519769 }, { "{abs}{x}", 0.001, 1 }, { "{floor}{x}", 1.0001, 0 },
    };
    for (unsigned k = 0; k < sizeof D2 / sizeof D2[0]; k++) {
      TokBuf b = toks(D2[k].f); CalcVars v = { 0 }; double out = 0;
      checks++;
      if (calc_derivative(&b, &v, D2[k].x, &out) != CALC_OK || fabs(out - D2[k].want) > 1e-9) { fails++; printf("FAIL d %s: %g\n", D2[k].f, out); }
    }
    { const char *A[] = { "{y}={sin}{x}", "{x}{sq}+{y}{sq}=1", "{y}=({x}-1){sq}" }, *B[] = { "{y}=0", "{y}=1", "{y}=0" };
      const int W[] = { 7, 1, 1 };
      for (int k = 0; k < 3; k++) {
        TokBuf e1 = toks(A[k]), e2 = toks(B[k]); CalcVars v = { 0 }; double xs[12], ys[12];
        int n = calc_solve2(&e1, &e2, &v, -10, 10, -10, 10, xs, ys, 12);
        checks++; if (n != W[k]) { fails++; printf("FAIL system %s; %s: %d\n", A[k], B[k], n); }
      } }
    ev("{tan}89.99999", 5729577.951308174, 1); ev("{sin}179.9999999", 1.7453292519943295e-09, 1);
    ev("170{nPr}170", 7.257415615307994e+306, 1);
    cplx("{i}^10001", false, "i");
    calc_fmt = FMT_FIX; calc_fmt_digits = 9;
    { char b[40]; calc_format_answer(1e7 / 3, b, sizeof b); checks++;
      if (!strcmp(b, "3333333.333333330")) { fails++; printf("FIX9 shows made-up digits: %s\n", b); } }
    calc_fmt = FMT_NORM;
  }

  // round 3 of the math review
  {
    ev("1029{nCr}514", 1.429820686498904e308, 0);
    { TokBuf a = toks("{sin}{x})+{sin}{y})=0"), b = toks("{x}-{y}=1"); CalcVars v = { 0 }; double xs[20], ys[20];
      int n = calc_solve2(&a, &b, &v, -10, 10, -10, 10, xs, ys, 20);
      checks++; if (n != 7) { fails++; printf("FAIL sin x + sin y = 0, x - y = 1: %d solutions\n", n); } }
    { TokBuf a = toks("{x}+{y}=2{E}-10"), b = toks("{x}-{y}=0"); CalcVars v = { 0 }; double xs[4], ys[4];
      int n = calc_solve2(&a, &b, &v, -10, 10, -10, 10, xs, ys, 4);
      checks++; if (n != 1 || xs[0] != 1e-10 || ys[0] != 1e-10) { fails++; printf("FAIL x+y=2E-10: %g %g\n", xs[0], ys[0]); } }
    { TokBuf b = toks("{sin}{x})=0.99999999"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, 0, 3, r, 12);
      checks++; if (n != 2 || fabs(r[0] - 1.570654905) > 1e-8 || fabs(r[1] - 1.570937749) > 1e-8) { fails++; printf("FAIL sin x = 0.99999999: %d\n", n); } }
    struct { const char *f; double a, b, want; } I3[] = {
      { "{exp}(-{x})", 0, 1e5, 1 }, { "{exp}(-{x}^2)", 0, 1e4, 0.886226925452758 }, { "{exp}{x}", -1e5, 0, 1 } };
    for (unsigned k = 0; k < 3; k++) {
      TokBuf b = toks(I3[k].f); CalcVars v = { 0 }; double out = 0; bool ap;
      CalcErr e = calc_integrate(&b, &v, I3[k].a, I3[k].b, &out, &ap);
      checks++; if (e || fabs(out - I3[k].want) > 1e-9) { fails++; printf("FAIL integral %s: %d %.15g\n", I3[k].f, e, out); }
    }
    struct { const char *f; double x, want; } D3[] = { { "1/{x}", 1e15, -1e-30 }, { "{ln}{x}", 1e8, 1e-8 }, { "{sqrt}{x}", 1e15, 1.58113883008419e-8 } };
    for (unsigned k = 0; k < 3; k++) {
      TokBuf b = toks(D3[k].f); CalcVars v = { 0 }; double out = 0;
      CalcErr e = calc_derivative(&b, &v, D3[k].x, &out);
      checks++; if (e || fabs(out - D3[k].want) > 1e-6 * fabs(D3[k].want)) { fails++; printf("FAIL d/dx %s: %d %.15g\n", D3[k].f, e, out); }
    }
  }

  // round 4
  {
    struct { const char *f; double x, want; } D4[] = { { "{x}^3", 0, 0 }, { "{x}^4", 0, 0 }, { "{x}^3+1", 0, 0 }, { "{x}^5", 1e-5, 5e-20 } };
    for (unsigned k = 0; k < 4; k++) {
      TokBuf b = toks(D4[k].f); CalcVars v = { 0 }; double out = 1;
      CalcErr e = calc_derivative(&b, &v, D4[k].x, &out);
      checks++; if (e || fabs(out - D4[k].want) > 1e-7 * fabs(D4[k].want)) { fails++; printf("FAIL d/dx %s at %g: %d %.15g\n", D4[k].f, D4[k].x, e, out); }
    }
    { TokBuf b = toks("{exp}(-{x}^2)"); CalcVars v = { 0 }; double out = 0; bool ap;
      CalcErr e = calc_integrate(&b, &v, -1e4, 1e4, &out, &ap);
      checks++; if (e || fabs(out - 1.772453850905516) > 1e-9) { fails++; printf("FAIL e^-x^2 over +-1E4: %.15g\n", out); } }
    cplx("{tan}({pi}/2)", false, "Math error");      // RAD: the pole, as close as doubles get
  }

  // round 5
  {
    struct { const char *f; double a, b, want; } I5[] = {
      { "{exp}(-{x})", 0, 1e99, 1 }, { "1/(1+{x}^2)", 0, 1e50, 1.570796326794897 }, { "{exp}(-{x}^2)", -1e99, 1e99, 1.772453850905516 },
      { "{sin}{x}", 0, 6.283185307179586, 0 } };
    for (unsigned k = 0; k < 4; k++) {
      TokBuf b = toks(I5[k].f); CalcVars v = { 0 }; double out = 9; bool ap;
      CalcErr e = calc_integrate(&b, &v, I5[k].a, I5[k].b, &out, &ap);
      checks++; if (e || fabs(out - I5[k].want) > 1e-9) { fails++; printf("FAIL integral %s to %g: %d %.15g\n", I5[k].f, I5[k].b, e, out); }
    }
    { TokBuf b = toks("{x}^4=1{E}-60"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++; if (n != 2 || fabs(r[1] - 1e-15) > 1e-24) { fails++; printf("FAIL x^4 = 1E-60: %d %g\n", n, r[0]); } }
    { TokBuf b = toks("{x}+1{E}7=1{E}7+0.5"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++; if (n != 1 || r[0] != 0.5) { fails++; printf("FAIL x + 1E7 = 1E7 + 0.5: %d\n", n); } }
    { TokBuf b = toks("{x}^3+1{E}10"); CalcVars v = { 0 }; double out = 0;
      CalcErr e = calc_derivative(&b, &v, 2, &out);
      checks++; if (e || out != 12) { fails++; printf("FAIL d/dx x^3+1E10 at 2: %.15g\n", out); } }
    cplx("{tan}(11{pi}/2)", false, "Math error");
  }

  // round 6
  {
    { TokBuf b = toks("{exp}(-{x})"); CalcVars v = { 0 }; double out = 0; bool ap;
      CalcErr e = calc_integrate(&b, &v, 1e99, 0, &out, &ap);
      checks++; if (e || fabs(out + 1) > 1e-9) { fails++; printf("FAIL integral from 1E99 to 0: %.15g\n", out); } }
    { TokBuf b = toks("{x}^2=1{E}-50"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++; if (n != 2 || fabs(r[1] - 1e-25) > 1e-34) { fails++; printf("FAIL x^2 = 1E-50: %d\n", n); } }
    ev("{sin}(1{E}-13)", 1.745329251994330e-15, 1);
    { TokBuf b = toks("{x}^3"); CalcVars v = { 0 }; double out = 0;
      CalcErr e = calc_derivative(&b, &v, 1e-9, &out);
      checks++; if (e || fabs(out - 3e-18) > 1e-27) { fails++; printf("FAIL d/dx x^3 at 1E-9: %g\n", out); } }
    { TokBuf b = toks("1{E}15+{x}"); CalcVars v = { 0 }; double out = 0;
      CalcErr e = calc_derivative(&b, &v, 5, &out);
      checks++; if (e == CALC_OK && out == 0) { fails++; printf("FAIL d/dx 1E15+x: 0 shown\n"); } }
  }

  // round 7
  {
    struct { const char *f; double want; } R7[] = { { "{ln}{x})=-2", 0.1353352832366127 }, { "1/{x}=10", 0.1 } };
    for (unsigned k = 0; k < 2; k++) {
      TokBuf b = toks(R7[k].f); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++; if (n != 1 || fabs(r[0] - R7[k].want) > 1e-12) { fails++; printf("FAIL roots %s: %d\n", R7[k].f, n); }
    }
    { TokBuf b = toks("2{x}+1{E}15"); CalcVars v = { 0 }; double out = 0;
      CalcErr e = calc_derivative(&b, &v, 100, &out);
      checks++; if (e == CALC_OK && out == 0) { fails++; printf("FAIL d/dx 2x+1E15: 0 shown\n"); } }
    ev("{sin}(-1{E}-15)", -1.745329251994330e-17, 1);
  }

  // round 8
  {
    { TokBuf b = toks("{x}^2-2{x}"); CalcVars v = { 0 }; double out = 1;
      CalcErr e = calc_derivative(&b, &v, 1, &out);
      checks++; if (e || out != 0) { fails++; printf("FAIL d/dx x^2-2x at 1: %d %g\n", e, out); } }
    { TokBuf b = toks("1/({x}-1.2)=1000"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++; if (n != 1 || fabs(r[0] - 1.201) > 1e-12) { fails++; printf("FAIL 1/(x-1.2)=1000: %d\n", n); } }
    { TokBuf b = toks("{x}{ln}{x})=0"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++; if (n != 1 || r[0] != 1) { fails++; printf("FAIL x ln x = 0: %d %g\n", n, r[0]); } }
    ev("{asin}(1{E}-15)", 5.729577951308232e-14, 1);
  }

  // round 9
  {
    { TokBuf b = toks("{exp}(1/({x}-1.3))=0"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++; if (n != 0) { fails++; printf("FAIL e^(1/(x-1.3)) = 0: %d roots\n", n); } }
    { TokBuf b = toks("1/({x}-9.75)=1000"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++; if (n != 1 || fabs(r[0] - 9.751) > 1e-12) { fails++; printf("FAIL 1/(x-9.75)=1000: %d\n", n); } }
    { TokBuf b = toks("{tan}{x}"); CalcVars v = { 0 }; double out = 0; bool ap;
      CalcErr e = calc_integrate(&b, &v, 0, 2, &out, &ap);
      checks++; if (e != CALC_ERR_MATH) { fails++; printf("FAIL tan x over 0..2 (RAD): %d %g\n", e, out); } }
    { TokBuf b = toks("{tan}(1000{x})"); CalcVars v = { 0 }; double out = 0;
      CalcErr e = calc_derivative(&b, &v, 1, &out);
      checks++; if (e || fabs(out - 3161.853123) > 1e-5) { fails++; printf("FAIL d/dx tan 1000x: %d %g\n", e, out); } }
  }

  // round 10
  {
    struct { const char *f; double a, b, want, tol; } I10[] = {
      { "(1-{x})^(-0.7)", 0, 1, 3.333333333333333, 1e-2 }, { "{x}^5+1", -1000, 1000, 2000, 1 } };
    for (unsigned k = 0; k < 2; k++) {
      TokBuf b = toks(I10[k].f); CalcVars v = { 0 }; double out = 0; bool ap;
      CalcErr e = calc_integrate(&b, &v, I10[k].a, I10[k].b, &out, &ap);
      checks++; if (e || fabs(out - I10[k].want) > I10[k].tol) { fails++; printf("FAIL integral %s: %d %.15g\n", I10[k].f, e, out); }
    }
    { TokBuf b = toks("{floor}({x}+0.3)=3"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++; if (n != 1 || r[0] != 2.7) { fails++; printf("FAIL floor(x+0.3)=3: %d %g\n", n, r[0]); } }
    { TokBuf b = toks("{x}^2{ln}{x})"); CalcVars v = { 0 }; double out = 0; bool ap = false;
      CalcErr e = calc_derivative2(&b, &v, 1e6, &out, &ap);
      checks++; if (e || !ap || fabs(out - 28631021.12) > 10) { fails++; printf("FAIL d/dx x^2 ln x at 1E6: %d %d %.10g\n", e, ap, out); } }
  }

  // round 11
  {
    { TokBuf b = toks("{exp}(-{x})=0"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -1000, 1000, r, 12);
      checks++; if (n != 0) { fails++; printf("FAIL e^-x = 0: %d roots (%g)\n", n, r[0]); } }
    { TokBuf b = toks("{floor}({x}^2)=0"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++; if (n != 1 || !(r[0] > -1 && r[0] < 1)) { fails++; printf("FAIL floor(x^2) = 0: %d %.12g\n", n, r[0]); } }
  }

  // round 12
  {
    { TokBuf b = toks("{floor}({x}^2)=50"); CalcVars v = { 0 }; double r[12];
      int n = calc_roots(&b, &v, -10, 10, r, 12);
      checks++; if (n != 2) { fails++; printf("FAIL floor(x^2) = 50: %d roots\n", n); } }
  }
  // round 13: a base just off 1 to a huge power is the decimal typed
  ev("(1+1/1{E}12)^1{E}12", 2.718281828457686, 0);
  ev("1.0000001^1{E}9", 2.688103701264924e43, 0);
  ev("0.9999999^1{E}8", 4.539990706252413e-05, 0);
  printf("%d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
