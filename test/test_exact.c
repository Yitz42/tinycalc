// Tests for exact.c: simplification, exact roots, linear systems, values.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/c/exact.h"

static int fails, checks;

static TokBuf toks(const char *s) {
  static const struct { const char *n; int t; } map[] = {
    {"E", T_EE}, {"pi", T_PI}, {"e", T_E}, {"x", T_X}, {"y", T_Y}, {"Ans", T_ANS},
    {"sq", T_SQ}, {"cube", T_CUBE}, {"recip", T_RECIP}, {"sin", T_SIN}, {"sqrt", T_SQRT}, {"ln", T_LN}, {"floor", T_FLOOR},
  };
  TokBuf b; b.n = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '{') {
      const char *q = strchr(p, '}');
      int len = (int)(q - p - 1);
      for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
        if ((int)strlen(map[i].n) == len && !strncmp(map[i].n, p + 1, len)) b.t[b.n++] = (uint8_t)map[i].t;
      p = q;
    } else b.t[b.n++] = (uint8_t)*p;
  }
  return b;
}
static void txt(const TokBuf *b, char *o) { calc_render(b, o, 300); }

static void solve(const char *eq, const char *simp, const char *sols) {
  TokBuf f = toks(eq);
  CalcVars v = { 0, 0, 0, 0 };
  Exact e;
  bool ok = exact_solve(&f, &v, &e);
  char s[300] = "", all[600] = "", one[300];
  if (ok) txt(&e.simp, s);
  for (int i = 0; i < e.n; i++) { txt(&e.sol[i], one); strcat(all, i ? " | " : ""); strcat(all, one); }
  checks++;
  if (!ok ? (simp != NULL) : (simp == NULL || strcmp(s, simp) || strcmp(all, sols))) {
    fails++;
    printf("FAIL %s\n   got  ok=%d [%s] [%s]\n   want [%s] [%s]\n", eq, ok, s, all, simp ? simp : "(none)", sols ? sols : "");
  }
  // values must satisfy the equation
  for (int i = 0; i < e.n; i++) if (e.real[i]) {
    double l, r; checks++;
    if (!calc_sides(&f, &v, e.x[i], &l, &r) || __builtin_fabs(l - r) > 1e-9 * (1 + __builtin_fabs(l))) {
      fails++; printf("FAIL value %s: x=%.17g gives %g vs %g\n", eq, e.x[i], l, r);
    }
  }
}

static void sys(const char *a, const char *b, const char *want) {
  TokBuf e1 = toks(a), e2 = toks(b);
  CalcVars v = { 0, 0, 0, 0 };
  Exact e; char o[300] = "";
  bool ok = exact_solve2(&e1, &e2, &v, &e);
  if (ok) txt(&e.sol[0], o);
  checks++;
  if (want ? (!ok || strcmp(o, want)) : ok) { fails++; printf("FAIL sys %s ; %s: got [%s] want [%s]\n", a, b, o, want ? want : "(none)"); }
}

static void val(double v, const char *want) {
  TokBuf o; char s[300] = "";
  bool ok = exact_value(v, &o);
  if (ok) txt(&o, s);
  checks++;
  if (want ? (!ok || strcmp(s, want)) : ok) { fails++; printf("FAIL value %.17g: got [%s] want [%s]\n", v, s, want ? want : "(none)"); }
}

int main(void) {
  solve("2{x}+3=7", "x-2=0", "x=2");
  solve("3{x}=2", "3x-2=0", "x=2/3");
  solve("{x}{sq}=2", "x^2-2=0", "x=-sqrt(2) | x=sqrt(2)");
  solve("{x}{sq}-{x}-1=0", "x^2-x-1=0", "x=(1-sqrt(5))/2 | x=(1+sqrt(5))/2");
  solve("({x}+1){sq}-3=2{x}", "x^2-2=0", "x=-sqrt(2) | x=sqrt(2)");
  solve("{x}{sq}-5{x}+6", "x^2-5x+6=0", "x=2 | x=3");
  solve("{x}{sq}+{x}+1=0", "x^2+x+1=0", "x=(-1-sqrt(3)i)/2 | x=(-1+sqrt(3)i)/2");
  solve("{x}{sq}+4=0", "x^2+4=0", "x=-2i | x=2i");
  solve("{x}{sq}=0", "x^2=0", "x=0");
  solve("{x}{cube}-6{x}{sq}+11{x}-6", "x^3-6x^2+11x-6=0", "x=1 | x=2 | x=3");
  solve("{x}{cube}=2{x}", "x^3-2x=0", "x=-sqrt(2) | x=0 | x=sqrt(2)");
  solve("{x}^4=16", "x^4-16=0", "x=-2 | x=2 | x=-2i | x=2i");
  solve("2{x}{sq}+3{x}-2=0", "2x^2+3x-2=0", "x=-2 | x=1/2");
  solve("{x}/2+{x}/3=5", "x-6=0", "x=6");
  solve("4{x}{sq}-8{x}+1=0", "4x^2-8x+1=0", "x=(2-sqrt(3))/2 | x=(2+sqrt(3))/2");
  solve("{x}{cube}-2=0", "x^3-2=0", "x=cbrt(2)");        // pure cube
  solve("2{x}{cube}+54=0", "x^3+27=0", "x=-3 | x=(3-3sqrt(3)i)/2 | x=(3+3sqrt(3)i)/2");
  solve("{x}{cube}=16", "x^3-16=0", "x=2cbrt(2)");
  solve("4{x}{cube}=1", "4x^3-1=0", "x=cbrt(2)/2");
  solve("{x}{cube}-2{x}-5=0", "x^3-2x-5=0", "");         // cubic, no rational root: simplified only
  solve("{x}^4-5{x}{sq}+6=0", "x^4-5x^2+6=0", "x=-sqrt(3) | x=-sqrt(2) | x=sqrt(2) | x=sqrt(3)");
  solve("{x}^4+3{x}{sq}-4=0", "x^4+3x^2-4=0", "x=-1 | x=1 | x=-2i | x=2i");
  solve("1000000{x}{sq}=1", "1000000x^2-1=0", "x=-1/1000 | x=1/1000");
  solve("{x}{sq}=0.3333333333", NULL, NULL);            // not 1/3
  solve("{x}=1.0000000001", NULL, NULL);
  solve("{sin}{x}=0.5", NULL, NULL);                    // not a polynomial
  solve("{x}^5=1", NULL, NULL);                         // degree 5: beyond us
  solve("1/{x}=2", NULL, NULL);                         // undefined at 0
  solve("{x}-{x}=1", NULL, NULL);                       // no x left
  solve("{sqrt}{x}=3", NULL, NULL);
  solve("{sqrt}(({x}-8){sq})=1", NULL, NULL);           // |x - 8| = 1 is x - 7 = 0 only up to the kink
  solve("{sqrt}(({x}-9){sq})=0.5", NULL, NULL);

  sys("{x}+{y}=3", "{x}-{y}=1", "x=2, y=1");
  sys("2{x}+3{y}=7", "{x}-{y}=1", "x=2, y=1");
  sys("{x}+{y}=1", "{x}-2{y}=0", "x=2/3, y=1/3");
  sys("{x}/2+{y}=1", "3{x}-{y}=2", "x=6/7, y=4/7");
  sys("{x}+{y}=1", "{x}+{y}=2", NULL);                  // parallel
  sys("{x}{sq}+{y}{sq}=25", "{y}={x}+1", NULL);         // not linear
  sys("{sqrt}(({x}-9){sq})+{y}=3", "{y}=2{x}-20", NULL); // |x - 9| + y = 3: linear only up to x = 9

  val(0.75, "3/4");
  val(1.0 / 3, "1/3");
  val(-2.5, "-5/2");
  val(3.4641016151377544, "2sqrt(3)");
  val(1.4142135623730951, "sqrt(2)");
  val(0.70710678118654752, "sqrt(2)/2");
  val(-1.7320508075688772, "-sqrt(3)");
  val(2.356194490192345, "3pi/4");
  val(3.141592653589793, "pi");
  val(6, NULL);
  val(2.718281828459045, NULL);

  // fuzz: random polynomials (overflow is caught by -fsanitize=signed-integer-overflow);
  // every real exact root must satisfy the equation
  srand(7);
  int nexact = 0;
  for (int it = 0; it < 20000; it++) {
    TokBuf f; f.n = 0;
    int deg = 1 + rand() % 4;
    for (int k = deg; k >= 0; k--) {
      int range = (it % 3 == 0) ? 40000 : (it % 3 == 1 ? 30 : 4);
      int c = rand() % (2 * range + 1) - range;
      if (k == deg && c == 0) c = 1;
      f.t[f.n++] = c < 0 ? '-' : '+';
      char d[12]; int n = snprintf(d, sizeof d, "%d", c < 0 ? -c : c);
      for (int i = 0; i < n; i++) f.t[f.n++] = (uint8_t)d[i];
      if (it % 5 == 0) { f.t[f.n++] = '/'; f.t[f.n++] = (uint8_t)('1' + rand() % 9); }
      if (k >= 1) { f.t[f.n++] = '*'; f.t[f.n++] = T_X; }
      if (k >= 2) { f.t[f.n++] = '^'; f.t[f.n++] = (uint8_t)('0' + k); }
    }
    CalcVars v = { 0, 0, 0, 0 };
    Exact e;
    if (!exact_solve(&f, &v, &e)) continue;
    nexact++;
    for (int i = 0; i < e.n; i++) if (e.real[i]) {
      double l, r, s = 0;
      calc_sides(&f, &v, e.x[i], &l, &r);
      for (int k = 0; k < 5; k++) s += __builtin_pow(__builtin_fabs(e.x[i]), k) * 40000;
      checks++;
      if (__builtin_fabs(l - r) > 1e-8 * s) { fails++; printf("FAIL fuzz root %.17g\n", e.x[i]); break; }
    }
  }
  printf("fuzz: %d of 20000 random polynomials solved exactly\n", nexact);
  {   // agreeing at every tenth is not being a polynomial: floor(10x) = 7 is not 10x - 7 = 0
    Exact ex; TokBuf b = toks("{floor}(10{x})=7"); CalcVars v = { 0, 0, 0, 0 };
    checks++; if (exact_solve(&b, &v, &ex)) { fails++; printf("FAIL floor(10x) = 7 taken as a polynomial\n"); }
    TokBuf b1 = toks("{floor}(10{x})+{y}=3"), b2 = toks("{x}-{y}=1");
    checks++; if (exact_solve2(&b1, &b2, &v, &ex)) { fails++; printf("FAIL floor(10x) + y = 3 taken as linear\n"); }
  }
  printf("%d checks, %d failures\n", checks, fails);
  return fails != 0;
}
