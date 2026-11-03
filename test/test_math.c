// Tests for mathfmt: tree shape for typical input, half-typed input, and a
// random-token fuzz (no crash, bounded nodes, layout sane).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/c/mathfmt.h"

static int fails, checks;
static MathDoc doc;

static void ser(const MathDoc *d, uint8_t k, char *o) {
  const MNode *n = &d->n[k];
  char a[600] = "", b[600] = "";
  if (n->a != 0xFF) ser(d, n->a, a);
  if (n->b != 0xFF) ser(d, n->b, b);
  switch (n->type) {
    case MN_BOX: strcpy(o, "#"); break;
    case MN_TEXT: strcpy(o, n->text); break;
    case MN_NUM: memcpy(o, d->tok + n->start, n->len); o[n->len] = 0; break;
    case MN_PI: strcpy(o, "pi"); break;
    case MN_BIN: sprintf(o, "[%s %s %s]", a, n->text, b); break;
    case MN_PREFIX: sprintf(o, "(%s%s)", n->text, a); break;
    case MN_CAT: sprintf(o, "%s%s", a, b); break;
    case MN_IMPL: sprintf(o, "%s.%s", a, b); break;
    case MN_FRAC: sprintf(o, "frac{%s}{%s}", a, b); break;
    case MN_POW: sprintf(o, "{%s}^{%s}", a, b); break;
    case MN_SQRT: sprintf(o, "sqrt{%s}", a); break;
    case MN_CBRT: sprintf(o, "cbrt{%s}", a); break;
    case MN_ABS: sprintf(o, "|%s|", a); break;
    case MN_PAREN: sprintf(o, "(%s)", a); break;
    case MN_SYS: sprintf(o, "sys{%s;%s}", a, b); break;
    case MN_INT: sprintf(o, "int[%s]{%s}", b, a); break;
    case MN_LIMS: sprintf(o, "%s..%s", a, b); break;
    case MN_AT: sprintf(o, "%s|_%s", a, b); break;
  }
}

// same token mini-language as test_calc.c
static TokBuf toks(const char *s) {
  static const struct { const char *n; int t; } map[] = {
    {"E", T_EE}, {"pi", T_PI}, {"e", T_E}, {"x", T_X}, {"y", T_Y}, {"sep", T_SEP}, {"Ans", T_ANS},
    {"sq", T_SQ}, {"cube", T_CUBE}, {"recip", T_RECIP}, {"%", T_PCT}, {"!", T_FACT},
    {"sin", T_SIN}, {"cos", T_COS}, {"tan", T_TAN}, {"asin", T_ASIN},
    {"acos", T_ACOS}, {"atan", T_ATAN}, {"ln", T_LN}, {"log", T_LOG},
    {"exp", T_EXP}, {"tenx", T_TENX}, {"sqrt", T_SQRT}, {"cbrt", T_CBRT}, {"abs", T_ABS},
  };
  TokBuf b; b.n = 0;
  for (const char *p = s; *p; p++) {
    if (*p == ' ') continue;
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

static int tw(void *c, const char *t, int l) { static const int W[] = {14, 12, 9, 7}; return (int)strlen(t) * W[l]; }
static void tx(void *c, int x, int y, const char *t, int l) {}
static void ln(void *c, int a, int b, int d, int e, int w) {}
static void bx(void *c, int x, int y, int w, int h) {}
static const MathGfx G = { tw, tx, ln, bx, NULL, NULL, {19, 16, 12, 10}, {6, 5, 4, 3} };

static void shape(const char *in, const char *want) {
  TokBuf b = toks(in);
  math_build(&doc, &b);
  char o[2000];
  ser(&doc, doc.root, o);
  checks++;
  if (strcmp(o, want)) { fails++; printf("FAIL %s\n   got  %s\n   want %s\n", in, o, want); }
}

int main(void) {
  shape("1+2", "[1 + 2]");
  shape("(1+2)/(3+4)", "frac{[1 + 2]}{[3 + 4]}");
  shape("1/2/3", "frac{frac{1}{2}}{3}");
  shape("1/2*3", "[frac{1}{2} * 3]");
  shape("2^({x}+1)", "{2}^{[x + 1]}");
  shape("2^3^2", "{2}^{{3}^{2}}");
  shape("-{x}{sq}", "(-{x}^{2})");
  shape("{sqrt}2{sq}", "sqrt{{2}^{2}}");
  shape("{sqrt}(1+{x})", "sqrt{[1 + x]}");
  shape("{sin}(2)", "sin(2)");
  shape("2{pi}", "2.pi");
  shape("{pi}2", "pi.2");
  shape("{x}{sq}=2", "[{x}^{2} = 2]");
  shape("{asin}0.5", "{sin}^{-1}(0.5)");
  shape("{exp}(2{x})", "{e}^{2.x}");
  shape("{tenx}3", "{10}^{3}");
  shape("1.5{E}-3", "[1.5 * {10}^{(-3)}]");
  shape("{E}6", "{10}^{6}");
  shape("{abs}{x}-3", "|[x - 3]|");
  shape("{cbrt}27", "cbrt{27}");
  shape("5{!}", "5!");
  shape("4{recip}", "{4}^{-1}");
  shape("1+", "[1 + #]");
  shape("{sin}(", "sin(#)");
  shape("{sin}", "sin(#)");
  shape("2*", "[2 * #]");
  shape("1/", "frac{1}{#}");
  shape("2)+1", "2.(+1)");   // stray ")" skipped
  shape("", "0");
  shape("(1/2)^2", "{(frac{1}{2})}^{2}");
  shape("{x}+{y}=3{sep}{x}-{y}=1", "sys{[[x + y] = 3];[[x - y] = 1]}");
  shape("{x}{y}=1{sep}", "sys{[x.y = 1];#}");

  {
    TokBuf b = toks("{x}{sq}");
    math_build(&doc, &b); math_wrap_integral(&doc, "0", "1");
    char o[600]; ser(&doc, doc.root, o); checks++;
    if (strcmp(o, "int[0..1]{{x}^{2}}")) { fails++; printf("FAIL int wrap: %s\n", o); }
    math_build(&doc, &b); math_wrap_derivative(&doc, "x=2");
    ser(&doc, doc.root, o); checks++;
    if (strcmp(o, "frac{d}{dx}({x}^{2})|_x=2")) { fails++; printf("FAIL d wrap: %s\n", o); }
    int w, a, de; math_measure(&doc, &G, 0, &w, &a, &de); math_draw(&doc, &G, 0, 0);
  }

  // fuzz
  srand(42);
  const int pool[] = {T_Y,T_SEP,'0','1','5','.','+','-','*','/','^','(',')','=',T_EE,T_PI,T_E,T_X,T_ANS,
                      T_SQ,T_CUBE,T_RECIP,T_PCT,T_FACT,T_SIN,T_ASIN,T_LN,T_EXP,T_TENX,T_SQRT,T_CBRT,T_ABS};
  int maxnodes = 0, maxw = 0;
  for (int it = 0; it < 200000; it++) {
    TokBuf b; b.n = (uint8_t)(rand() % (CALC_MAX_TOK + 1));
    int bias = rand() % 3;
    for (int i = 0; i < b.n; i++) {
      int k = pool[rand() % (int)(sizeof pool / sizeof pool[0])];
      if (bias == 1 && rand() % 2) k = '(';
      if (bias == 2 && rand() % 2) k = T_SQRT;
      b.t[i] = (uint8_t)k;
    }
    math_build(&doc, &b);
    if (it % 3 == 1) math_wrap_integral(&doc, "0", "1");
    if (it % 3 == 2) math_wrap_derivative(&doc, "x=1");
    int w, a, d;
    math_measure(&doc, &G, 0, &w, &a, &d);
    math_draw(&doc, &G, 0, 0);
    if (doc.count > maxnodes) maxnodes = doc.count;
    if (w > maxw) maxw = w;
    checks++;
    if (w <= 0 || a <= 0 || d < 0 || doc.count > MATH_MAX_NODES) { fails++; printf("FAIL fuzz %d\n", it); break; }
  }
  printf("fuzz: max nodes %d, max width %d\n", maxnodes, maxw);
  printf("%d checks, %d failures\n", checks, fails);
  return fails != 0;
}
