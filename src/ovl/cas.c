// cas.c - symbolic math for the calculator (see src/c/cas.h): token string ->
// expression tree -> simplify / differentiate / integrate / factor / expand ->
// token string. Built into the "cas" overlay on the watch (it's loaded onto
// the heap only while it's used); linked directly in the PC tests.
//
// Trees live in one pool per call (malloc'd, freed at the end). Sums and
// products are binary nodes; simplify() flattens them, collects like terms
// and factors, and rebuilds them in a fixed order, so equal things compare
// equal. Subtraction is a + (-1)b and division is a b^-1.
#include "../c/cas.h"
#include "../c/mmath.h"
#include <stddef.h>
#include <stdlib.h>

#define MAXN  1500          // nodes per call
#define MAXV  320           // distinct numbers per call
#define MAXL  24            // terms in a sum / factors in a product
#define MAXDEG 12           // polynomials (factor, expand, rational integrals)

enum { O_NUM, O_X, O_Y, O_PI, O_E, O_ADD, O_MUL, O_POW, O_FN };

typedef struct { uint8_t op, f; int16_t a, b; } N;   // O_NUM: a = index into v[]

typedef struct {
  N n[MAXN];
  double v[MAXV];
  int nn, nv, depth;
  bool fail, deg;
  double ans, mem[CALC_MEMS];
  const uint8_t *t;          // parser input
  int nt, i;
  TokBuf *out;               // emitter output
} Ctx;

static Ctx *K;

// ------------------------------------------------------------------ nodes

static int node(int op, int f, int a, int b) {
  if (K->nn >= MAXN) { K->fail = true; return 0; }
  N *p = &K->n[K->nn];
  p->op = (uint8_t)op; p->f = (uint8_t)f; p->a = (int16_t)a; p->b = (int16_t)b;
  return K->nn++;
}
static int num(double x) {
  if (x == 0) x = 0.0;                                   // no -0
  for (int i = 0; i < K->nv; i++)
    if (K->v[i] == x) return node(O_NUM, 0, i, -1);
  if (K->nv >= MAXV || !m_isfinite(x)) { K->fail = true; return 0; }
  K->v[K->nv] = x;
  return node(O_NUM, 0, K->nv++, -1);
}
#define OP(k) (K->n[k].op)
#define A(k) (K->n[k].a)
#define B(k) (K->n[k].b)
#define F(k) (K->n[k].f)
static bool isnum(int k) { return OP(k) == O_NUM; }
static double NV(int k) { return K->v[A(k)]; }
static bool isv(int k, double x) { return isnum(k) && NV(k) == x; }

static int add(int a, int b) { return node(O_ADD, 0, a, b); }
static int mul(int a, int b) { return node(O_MUL, 0, a, b); }
static int pw(int a, int b) { return node(O_POW, 0, a, b); }
static int fn(int f, int a) { return node(O_FN, f, a, -1); }
static int neg(int a) { return mul(num(-1), a); }
static int inv(int a) { return pw(a, num(-1)); }
static int sym(int op) { return node(op, 0, -1, -1); }

static bool has_x(int k) {
  switch (OP(k)) {
    case O_X: return true;
    case O_ADD: case O_MUL: case O_POW: return has_x(A(k)) || has_x(B(k));
    case O_FN: return has_x(A(k)) || (B(k) >= 0 && has_x(B(k)));
    default: return false;
  }
}

static bool eq(int a, int b) {
  if (a == b) return true;
  if (OP(a) != OP(b) || F(a) != F(b)) return false;
  switch (OP(a)) {
    case O_NUM: return NV(a) == NV(b);
    case O_ADD: case O_MUL: case O_POW: return eq(A(a), A(b)) && eq(B(a), B(b));
    case O_FN: return eq(A(a), A(b)) && (B(a) < 0 ? B(b) < 0 : (B(b) >= 0 && eq(B(a), B(b))));
    default: return true;
  }
}

static bool is_int(double x) { return m_isfinite(x) && x == m_floor(x) && m_fabs(x) < 1e15; }

// x as p/q with q <= qmax (continued fractions). False if no close fraction.
static bool rat(double x, double qmax, double *pp, double *pq) {
  double s = x < 0 ? -1 : 1, y = m_fabs(x);
  double h0 = 0, h1 = 1, k0 = 1, k1 = 0, r = y;
  for (int it = 0; it < 40; it++) {
    double a = m_floor(r);
    double h2 = a * h1 + h0, k2 = a * k1 + k0;
    if (k2 > qmax || h2 > 1e12) break;
    h0 = h1; h1 = h2; k0 = k1; k1 = k2;
    if (m_fabs(y - h1 / k1) <= 1e-12 * (y > 1 ? y : 1)) { *pp = s * h1; *pq = k1; return true; }
    double f = r - a;
    if (f < 1e-15) break;
    r = 1 / f;
  }
  return false;
}

// ------------------------------------------------------------------ parse

static int pk(void) { return K->i < K->nt ? K->t[K->i] : -1; }
static bool digit(int k) { return k >= '0' && k <= '9'; }
static bool starts_primary(int k) {
  return digit(k) || k == '.' || k == T_EE || k == '(' || calc_is_func(k) ||
         k == T_PI || k == T_E || k == T_X || k == T_Y || k == T_ANS || (k >= T_MA && k <= T_MF);
}
static int p_expr(void);
static int p_unary(void);
static int bad(void) { K->fail = true; return 0; }

static int p_number(void) {
  double m = 0;
  int dexp = 0, k;
  bool dot = false, any = false;
  while ((k = pk()) >= 0 && (digit(k) || k == '.')) {
    K->i++;
    if (k == '.') { if (dot) return bad(); dot = true; continue; }
    any = true;
    m = m * 10 + (k - '0');
    if (dot) dexp--;
  }
  if (pk() == T_EE) {
    K->i++;
    if (!any) { m = 1; any = true; }
    bool ng = false;
    if (pk() == '-') { ng = true; K->i++; } else if (pk() == '+') K->i++;
    int e = 0;
    bool ed = false;
    while ((k = pk()) >= 0 && digit(k)) { K->i++; ed = true; if (e < 10000) e = e * 10 + (k - '0'); }
    if (!ed) return bad();
    dexp += ng ? -e : e;
  }
  if (!any) return bad();
  return num(dexp < 0 ? m / m_pow10i(-dexp) : m * m_pow10i(dexp));
}

static int p_close(int v) {
  if (pk() == ')') K->i++;
  else if (pk() != -1 && pk() != '=') bad();
  return v;
}

static int p_primary(void) {
  int k = pk();
  if (k < 0) return bad();
  if (digit(k) || k == '.' || k == T_EE) return p_number();
  K->i++;
  if (++K->depth > 40) return bad();
  int r;
  switch (k) {
    case '(': r = p_close(p_expr()); break;
    case T_PI: r = sym(O_PI); break;
    case T_E: r = sym(O_E); break;
    case T_X: r = sym(O_X); break;
    case T_Y: r = sym(O_Y); break;
    case T_ANS: r = num(K->ans); break;
    case T_LOGB: case T_ROOT: {                      // logb(b, x) = ln x / ln b; root(n, x) = x^(1/n)
      int a = p_expr();
      if (pk() != ',') return bad();
      K->i++;
      int b = p_close(p_expr());
      r = k == T_LOGB ? mul(fn(T_LN, b), inv(fn(T_LN, a))) : pw(b, inv(a));
      break;
    }
    case T_SUM: case T_PROD: return bad();           // no formulas for these
    case T_GCD: case T_LCM: {
      int a = p_expr();
      if (pk() != ',') return bad();
      K->i++;
      int b = p_expr();
      r = p_close(node(O_FN, k, a, b));
      break;
    }
    case T_EXP: r = pw(sym(O_E), p_close(p_expr())); break;
    case T_TENX: r = pw(num(10), p_close(p_expr())); break;
    default:
      if (k >= T_MA && k <= T_MF) { r = num(K->mem[k - T_MA]); break; }
      if (!calc_is_func(k)) return bad();
      r = fn(k, p_close(p_expr()));
      break;
  }
  K->depth--;
  return r;
}

static int p_postfix(void) {
  int v = p_primary();
  for (;;) {
    int k = pk();
    if (k == T_SQ) v = pw(v, num(2));
    else if (k == T_CUBE) v = pw(v, num(3));
    else if (k == T_RECIP) v = inv(v);
    else if (k == T_PCT) v = mul(v, num(0.01));
    else if (k == T_FACT) v = fn(T_FACT, v);
    else return v;
    K->i++;
  }
}

static int p_power(void) {
  int b = p_postfix();
  if (pk() != '^') return b;
  K->i++;
  return pw(b, p_unary());
}

static int p_unary(void) {
  int k = pk();
  if (k == '-' || k == '+') {
    K->i++;
    if (++K->depth > 40) return bad();
    int v = p_unary();
    K->depth--;
    return k == '-' ? neg(v) : v;
  }
  return p_power();
}

static int p_comb(void) {                          // nCr, nPr bind tighter than * and / (as in calc.c)
  int v = p_unary(), k;
  while (!K->fail && ((k = pk()) == T_NCR || k == T_NPR)) { K->i++; v = node(O_FN, k, v, p_unary()); }
  return v;
}

static int p_term(void) {
  int v = p_comb();
  for (;;) {
    int k = pk();
    if (K->fail) return v;
    if (k == '*') { K->i++; v = mul(v, p_comb()); }
    else if (k == '/') { K->i++; v = mul(v, inv(p_comb())); }
    else if (k == T_MOD) { K->i++; v = node(O_FN, k, v, p_comb()); }
    else if (k >= 0 && starts_primary(k)) v = mul(v, p_power());
    else return v;
  }
}

static int p_expr(void) {
  int v = p_term();
  for (;;) {
    int k = pk();
    if (K->fail || (k != '+' && k != '-')) return v;
    K->i++;
    int r = p_term();
    v = add(v, k == '-' ? neg(r) : r);
  }
}

// ------------------------------------------------------------------ simplify

static int simp(int k);

typedef struct { int k[MAXL]; int n; } List;

static void push(List *l, int k) { if (l->n >= MAXL) { K->fail = true; return; } l->k[l->n++] = k; }
static void flat(List *l, int k, int op) {       // a canonical sum/product into its parts
  if (OP(k) == op) { flat(l, A(k), op); flat(l, B(k), op); } else push(l, k);
}
static void gather(List *l, int k, int op) {     // simplify each part, then flatten
  if (OP(k) == op) { gather(l, A(k), op); gather(l, B(k), op); return; }
  flat(l, simp(k), op);
}

static int chain(const List *l, int op, int start) {
  int r = l->k[start];
  for (int i = start + 1; i < l->n; i++) r = node(op, 0, r, l->k[i]);
  return r;
}

// A term as coefficient * rest (rest -1 when the term is just a number).
static double split(int t, int *rest) {
  if (isnum(t)) { *rest = -1; return NV(t); }
  if (OP(t) == O_MUL && isnum(A(t))) { *rest = B(t); return NV(A(t)); }
  *rest = t;
  return 1;
}

static int rank(int base) {
  switch (OP(base)) {
    case O_NUM: return 0;
    case O_PI: return 1;
    case O_X: return 2;
    case O_Y: return 3;
    case O_E: return 4;
    case O_FN: return 5;
    case O_ADD: return 7;
    default: return 6;
  }
}

static int simp_pow(int a, int b);

static int simp_mul(int k) {
  List l = { .n = 0 };
  gather(&l, k, O_MUL);
  double c = 1;
  int base[MAXL], ex[MAXL], n = 0;
  for (int i = 0; i < l.n; i++) {
    int f = l.k[i];
    if (isnum(f)) { c *= NV(f); continue; }
    int bs = f, e = num(1);
    if (OP(f) == O_POW) { bs = A(f); e = B(f); }
    int j;
    for (j = 0; j < n; j++) if (eq(base[j], bs)) break;
    if (j < n) ex[j] = simp(add(ex[j], e));
    else { base[n] = bs; ex[n] = e; n++; }
  }
  if (c == 0 || K->fail) return num(0);
  List out = { .n = 0 };
  for (int r = 0; r <= 7; r++)                      // put the factors in a fixed order
    for (int j = 0; j < n; j++) {
      if (rank(base[j]) != r) continue;
      int f = simp_pow(base[j], ex[j]);
      if (isnum(f)) c *= NV(f);
      else if (OP(f) == O_MUL) {                      // e.g. (2x)^2 came apart
        List m = { .n = 0 };
        flat(&m, f, O_MUL);
        for (int q = 0; q < m.n; q++) { if (isnum(m.k[q])) c *= NV(m.k[q]); else push(&out, m.k[q]); }
      } else push(&out, f);
    }
  if (c == 0) return num(0);
  if (!out.n) return num(c);
  int r = chain(&out, O_MUL, 0);
  return c == 1 ? r : mul(num(c), r);
}

static int degree(int rest) {            // x^k monomials: k; anything else -1
  if (rest < 0) return 0;
  if (OP(rest) == O_X) return 1;
  if (OP(rest) == O_POW && OP(A(rest)) == O_X && isnum(B(rest)) && is_int(NV(B(rest))) && NV(B(rest)) > 0)
    return (int)NV(B(rest));
  return -1;
}

static int simp_add(int k) {
  List l = { .n = 0 };
  gather(&l, k, O_ADD);
  double co[MAXL];
  int rest[MAXL], n = 0;
  for (int i = 0; i < l.n; i++) {
    int r;
    double c = split(l.k[i], &r);
    if (r >= 0 && OP(r) == O_ADD) {                  // c (a + b): spread c over the sum's terms
      List m = { .n = 0 };
      flat(&m, r, O_ADD);
      for (int q = 0; q < m.n; q++) {
        double cq = split(m.k[q], &r);
        push(&l, cq == 1 && c == 1 ? (r < 0 ? num(1) : r) : (r < 0 ? num(c * cq) : mul(num(c * cq), r)));
      }
      continue;
    }
    int j;
    for (j = 0; j < n; j++)
      if ((rest[j] < 0 && r < 0) || (rest[j] >= 0 && r >= 0 && eq(rest[j], r))) break;
    if (j < n) co[j] += c;
    else { co[n] = c; rest[n] = r; n++; }
  }
  List out = { .n = 0 };
  // x^k terms by falling degree, then the rest in order, then the constant
  for (int d = MAXDEG * 4; d >= 1; d--)
    for (int j = 0; j < n; j++)
      if (co[j] != 0 && rest[j] >= 0 && degree(rest[j]) == d)
        push(&out, co[j] == 1 ? rest[j] : mul(num(co[j]), rest[j]));
  for (int j = 0; j < n; j++)
    if (co[j] != 0 && rest[j] >= 0 && degree(rest[j]) < 1)
      push(&out, co[j] == 1 ? rest[j] : mul(num(co[j]), rest[j]));
  for (int j = 0; j < n; j++)
    if (co[j] != 0 && rest[j] < 0) push(&out, num(co[j]));
  if (!out.n) return num(0);
  return chain(&out, O_ADD, 0);
}

static bool nice(double r) {             // worth showing a function's value as a number
  double p, q;
  return m_isfinite(r) && rat(r, 4, &p, &q);
}

static int simp_pow(int a, int b) {
  if (isnum(b)) {
    double e = NV(b);
    if (e == 0) return num(1);
    if (e == 1) return a;
    if (isnum(a)) {
      double x = NV(a);
      if (x == 1) return num(1);
      if (x == 0) return e > 0 ? num(0) : pw(a, b);
      if (is_int(e) && m_fabs(e) <= 64) {
        double r = m_pow(x, e);
        if (m_isfinite(r) && r != 0) return num(r);
      }
      double r = m_pow(x, e);
      if (m_isfinite(r) && is_int(m_round(r)) && m_fabs(r - m_round(r)) <= 1e-9 * (m_fabs(r) > 1 ? m_fabs(r) : 1))
        return num(m_round(r));
      return pw(a, b);
    }
    if (is_int(e)) {
      if (OP(a) == O_POW) return simp_pow(A(a), simp(mul(B(a), b)));
      if (OP(a) == O_MUL) {                           // (2x)^3 = 8x^3
        List l = { .n = 0 };
        flat(&l, a, O_MUL);
        int r = num(1);
        for (int i = 0; i < l.n; i++) r = mul(r, pw(l.k[i], b));
        return simp(r);
      }
      if (OP(a) == O_FN && F(a) == T_SQRT && ((long)e) % 2 == 0) return simp_pow(A(a), num(e / 2));
      if (OP(a) == O_FN && F(a) == T_CBRT && ((long)e) % 3 == 0) return simp_pow(A(a), num(e / 3));
    }
  }
  if (OP(a) == O_E && OP(b) == O_FN && F(b) == T_LN) return A(b);     // e^ln(u) = u
  return pw(a, b);
}

static int simp_fn(int k) {
  int f = F(k), a = simp(A(k)), b = B(k) >= 0 ? simp(B(k)) : -1;
  if (isnum(a) && (b < 0 || isnum(b))) {
    double r = f == T_FACT ? m_fact(NV(a)) : calc_apply(f, NV(a), b >= 0 ? NV(b) : 0, K->deg);
    bool exact = f == T_ABS || f == T_ROUND || f == T_FLOOR || f == T_GCD || f == T_LCM ||
                 f == T_NCR || f == T_NPR || f == T_MOD || f == T_FACT;
    if (m_isfinite(r) && (exact || nice(r))) return num(r);
  }
  if (f == T_LN && OP(a) == O_E) return num(1);
  if (f == T_LN && OP(a) == O_POW && OP(A(a)) == O_E) return B(a);          // ln(e^u) = u
  if (f == T_SQRT && isnum(a) && NV(a) > 0) return simp_pow(a, num(0.5));   // sqrt(8) = 2 sqrt(2) later
  return node(O_FN, f, a, b);
}

static int simp(int k) {
  if (K->fail) return 0;
  if (++K->depth > 120) { K->fail = true; return 0; }
  int r;
  switch (OP(k)) {
    case O_ADD: r = simp_add(k); break;
    case O_MUL: r = simp_mul(k); break;
    case O_POW: r = simp_pow(simp(A(k)), simp(B(k))); break;
    case O_FN: r = simp_fn(k); break;
    default: r = k; break;
  }
  K->depth--;
  return r;
}

// ------------------------------------------------------------------ expand

static int distrib(int a, int b) {
  if (OP(a) == O_ADD) return add(distrib(A(a), b), distrib(B(a), b));
  if (OP(b) == O_ADD) return add(distrib(a, A(b)), distrib(a, B(b)));
  return mul(a, b);
}

static int expand(int k) {
  if (K->fail) return 0;
  if (++K->depth > 120) { K->fail = true; return 0; }
  int r;
  switch (OP(k)) {
    case O_ADD: r = simp(add(expand(A(k)), expand(B(k)))); break;
    case O_MUL: r = simp(distrib(expand(A(k)), expand(B(k)))); break;
    case O_POW: {
      int base = expand(A(k)), e = B(k);
      if (OP(base) == O_ADD && isnum(e) && is_int(NV(e)) && NV(e) >= 2 && NV(e) <= 20) {   // the pool limits the rest
        r = base;
        for (int i = 1; i < (int)NV(e); i++) r = simp(distrib(r, base));
      } else {
        r = simp(pw(base, e));
      }
      break;
    }
    case O_FN: r = simp(node(O_FN, F(k), expand(A(k)), B(k) >= 0 ? expand(B(k)) : -1)); break;
    default: r = k; break;
  }
  K->depth--;
  return r;
}

// ------------------------------------------------------------------ derivative

static int deg_k(void) { return mul(sym(O_PI), num(1.0 / 180)); }       // trig in degrees: d/dx sin x = cos x * pi/180
static int deg_ki(void) { return mul(num(180), inv(sym(O_PI))); }

static int der(int k) {
  if (K->fail) return 0;
  if (!has_x(k)) return num(0);
  if (++K->depth > 120) { K->fail = true; return 0; }
  int r = 0;
  switch (OP(k)) {
    case O_X: r = num(1); break;
    case O_ADD: r = add(der(A(k)), der(B(k))); break;
    case O_MUL: r = add(mul(der(A(k)), B(k)), mul(A(k), der(B(k)))); break;
    case O_POW: {
      int a = A(k), b = B(k);
      if (!has_x(b)) r = mul(mul(b, pw(a, add(b, num(-1)))), der(a));
      else if (OP(a) == O_E) r = mul(k, der(b));
      else if (isv(a, 0) || isv(a, 1)) r = num(0);   // 0^x, 1^x: flat (where defined)
      else if (!has_x(a)) r = mul(mul(k, fn(T_LN, a)), der(b));
      else r = mul(k, add(mul(der(b), fn(T_LN, a)), mul(mul(b, der(a)), inv(a))));
      break;
    }
    case O_FN: {
      int u = A(k), g;
      bool d = K->deg;
      switch (F(k)) {
        case T_SIN: g = fn(T_COS, u); if (d) g = mul(g, deg_k()); break;
        case T_COS: g = neg(fn(T_SIN, u)); if (d) g = mul(g, deg_k()); break;
        case T_TAN: g = pw(fn(T_COS, u), num(-2)); if (d) g = mul(g, deg_k()); break;
        case T_ASIN: g = pw(add(num(1), neg(pw(u, num(2)))), num(-0.5)); if (d) g = mul(g, deg_ki()); break;
        case T_ACOS: g = neg(pw(add(num(1), neg(pw(u, num(2)))), num(-0.5))); if (d) g = mul(g, deg_ki()); break;
        case T_ATAN: g = inv(add(num(1), pw(u, num(2)))); if (d) g = mul(g, deg_ki()); break;
        case T_LN: g = inv(u); break;
        case T_LOG: g = inv(mul(u, fn(T_LN, num(10)))); break;
        case T_SQRT: g = inv(mul(num(2), fn(T_SQRT, u))); break;
        case T_CBRT: g = inv(mul(num(3), pw(fn(T_CBRT, u), num(2)))); break;
        case T_ABS: g = mul(u, inv(fn(T_ABS, u))); break;
        case T_SINH: g = fn(T_COSH, u); break;
        case T_COSH: g = fn(T_SINH, u); break;
        case T_TANH: g = pw(fn(T_COSH, u), num(-2)); break;
        case T_ASINH: g = inv(fn(T_SQRT, add(pw(u, num(2)), num(1)))); break;
        case T_ACOSH: g = inv(fn(T_SQRT, add(pw(u, num(2)), num(-1)))); break;
        case T_ATANH: g = inv(add(num(1), neg(pw(u, num(2))))); break;
        default: K->fail = true; g = 0; break;          // round, floor, n!, gcd... of x
      }
      r = mul(g, der(u));
      break;
    }
    default: K->fail = true; break;
  }
  K->depth--;
  return r;
}

// ------------------------------------------------------------------ integral

static int integ(int k);

static double lin(int k) {               // d/dx of k if it's a non-zero constant (k linear in x), else 0
  if (!has_x(k)) return 0;
  int d = simp(der(k));
  return (!K->fail && isnum(d)) ? NV(d) : 0;
}

// Antiderivative of the outer function of F, as a function of its inner u,
// divided by a (u' = a, or 1 for a substitution). -1 if not in the table.
static int outer(int Fk, int u, double a) {
  int r = -1, kk = K->deg ? deg_ki() : num(1);       // trig of degrees integrates with a 180/pi
  int s = num(1 / a);
  if (OP(Fk) == O_POW && A(Fk) == u) {
    int e = B(Fk);
    if (isv(e, -1)) return mul(s, fn(T_LN, fn(T_ABS, u)));
    return mul(mul(s, inv(add(e, num(1)))), pw(u, add(e, num(1))));
  }
  if (OP(Fk) == O_POW && B(Fk) == u) {
    if (OP(A(Fk)) == O_E) return mul(s, Fk);
    return mul(mul(s, inv(fn(T_LN, A(Fk)))), Fk);
  }
  if (OP(Fk) != O_FN || A(Fk) != u || B(Fk) >= 0) return -1;
  switch (F(Fk)) {
    case T_SIN: r = mul(kk, neg(fn(T_COS, u))); break;
    case T_COS: r = mul(kk, fn(T_SIN, u)); break;
    case T_TAN: r = mul(kk, neg(fn(T_LN, fn(T_ABS, fn(T_COS, u))))); break;
    case T_SQRT: r = mul(num(2.0 / 3), pw(u, num(1.5))); break;
    case T_CBRT: r = mul(num(0.75), pw(u, num(4.0 / 3))); break;
    case T_LN: r = add(mul(u, fn(T_LN, u)), neg(u)); break;
    case T_LOG: r = mul(inv(fn(T_LN, num(10))), add(mul(u, fn(T_LN, u)), neg(u))); break;
    case T_SINH: r = fn(T_COSH, u); break;
    case T_COSH: r = fn(T_SINH, u); break;
    case T_TANH: r = fn(T_LN, fn(T_COSH, u)); break;
    case T_ABS: r = mul(num(0.5), mul(u, Fk)); break;                          // u|u|/2
    // inverse trig by parts; in degrees they return degrees, so the second
    // part carries 180/pi
    case T_ATAN: r = add(mul(u, Fk), neg(mul(kk, mul(num(0.5), fn(T_LN, add(pw(u, num(2)), num(1))))))); break;
    case T_ASIN: r = add(mul(u, Fk), mul(kk, pw(add(num(1), neg(pw(u, num(2)))), num(0.5)))); break;
    case T_ACOS: r = add(mul(u, Fk), neg(mul(kk, pw(add(num(1), neg(pw(u, num(2)))), num(0.5))))); break;
    case T_ASINH: r = add(mul(u, Fk), neg(pw(add(pw(u, num(2)), num(1)), num(0.5)))); break;
    case T_ATANH: r = add(mul(u, Fk), mul(num(0.5), fn(T_LN, add(num(1), neg(pw(u, num(2))))))); break;
    default: return -1;
  }
  return mul(s, r);
}

static int mkpoly(const double *c, int d) {          // c[0] + c[1] x + ... + c[d] x^d
  int r = num(c[0]);
  for (int i = 1; i <= d; i++) if (c[i] != 0) r = add(r, mul(num(c[i]), i == 1 ? sym(O_X) : pw(sym(O_X), num(i))));
  return r;
}

static int poly(int k, double *c);

// 1 / (c2 x^2 + c1 x + c0): atan after completing the square, a log of the
// two linear factors, or -1/(c2 (x - r)) for a double root.
static int quad_inv(const double *c) {
  double c2 = c[2], c1 = c[1], c0 = c[0], D = c1 * c1 - 4 * c2 * c0, h = c1 / (2 * c2);
  int X = sym(O_X);
  if (D < 0) {
    double k = c0 / c2 - h * h, sk = m_sqrt(k);
    return mul(mul(num(1 / (c2 * sk)), fn(T_ATAN, mul(num(1 / sk), add(X, num(h))))), K->deg ? deg_k() : num(1));
  }
  if (D == 0) return neg(mul(num(1 / c2), inv(add(X, num(h)))));
  double sd = m_sqrt(D), r1 = (-c1 - sd) / (2 * c2), r2 = (-c1 + sd) / (2 * c2);
  if (c2 * (r1 - r2) < 0) { double t = r1; r1 = r2; r2 = t; }   // a positive factor in front
  return mul(num(1 / (c2 * (r1 - r2))), fn(T_LN, fn(T_ABS, mul(add(X, num(-r1)), inv(add(X, num(-r2)))))));
}

// 1 / sqrt(c2 x^2 + c1 x + c0), with v = x + c1/(2 c2) and m = c0 - c2 h^2
static int quad_isqrt(const double *c) {
  double c2 = c[2], h = c[1] / (2 * c2), m = c[0] - c2 * h * h;
  int v = add(sym(O_X), num(h));
  if (c2 < 0 && m > 0)
    return mul(mul(num(1 / m_sqrt(-c2)), fn(T_ASIN, mul(num(m_sqrt(-c2 / m)), v))), K->deg ? deg_k() : num(1));
  if (c2 > 0 && m > 0) return mul(num(1 / m_sqrt(c2)), fn(T_ASINH, mul(num(m_sqrt(c2 / m)), v)));
  if (c2 > 0 && m < 0)          // ln|v + sqrt(v^2 + m/c2)| / sqrt(c2)
    return mul(num(1 / m_sqrt(c2)), fn(T_LN, fn(T_ABS, add(v, pw(add(pw(v, num(2)), num(m / c2)), num(0.5))))));
  return -1;
}

// trig squared of a linear argument: power reduction, and 1/cos^2 = tan'
static int trig_pow(int b, double e) {
  if (OP(b) != O_FN || B(b) >= 0) return -1;
  int u = A(b);
  double a = lin(u);
  if (a == 0) return -1;
  int kk = K->deg ? deg_ki() : num(1), X = sym(O_X), u2 = mul(num(2), u);
  int f = F(b);
  if (e == 2 && f == T_SIN) return add(mul(num(0.5), X), neg(mul(kk, mul(num(1 / (4 * a)), fn(T_SIN, u2)))));
  if (e == 2 && f == T_COS) return add(mul(num(0.5), X), mul(kk, mul(num(1 / (4 * a)), fn(T_SIN, u2))));
  if (e == 2 && f == T_TAN) return add(mul(kk, mul(num(1 / a), fn(T_TAN, u))), neg(X));
  if (e == -2 && f == T_COS) return mul(kk, mul(num(1 / a), fn(T_TAN, u)));
  if (e == -2 && f == T_SIN) return neg(mul(kk, mul(num(1 / a), inv(fn(T_TAN, u)))));
  if (e == 2 && f == T_SINH) return add(mul(num(1 / (4 * a)), fn(T_SINH, u2)), neg(mul(num(0.5), X)));
  if (e == 2 && f == T_COSH) return add(mul(num(1 / (4 * a)), fn(T_SINH, u2)), mul(num(0.5), X));
  if (e == -2 && f == T_COSH) return mul(num(1 / a), fn(T_TANH, u));
  return -1;
}

// A quotient of polynomials whose bottom has degree 1 or 2: divide, then
// logs and atan (partial fractions). f: the factors that hold x.
static int integ_rat(const int *f, int n) {
  int top = num(1), bot = num(1), nb = 0;
  for (int i = 0; i < n; i++) {
    double c[MAXDEG + 1];
    if (OP(f[i]) == O_POW && isnum(B(f[i])) && is_int(NV(B(f[i]))) && NV(B(f[i])) < 0) {
      int e = (int)-NV(B(f[i]));
      if (poly(A(f[i]), c) < 1) return -1;
      for (int j = 0; j < e; j++) bot = mul(bot, A(f[i]));
      nb++;
    } else if (poly(f[i], c) >= 0) top = mul(top, f[i]);
    else return -1;
    if (K->fail) return -1;
  }
  if (!nb) return -1;
  double N[MAXDEG + 1], D[MAXDEG + 1];
  int dn = poly(top, N), dd = poly(bot, D);
  if (dn < 0 || dd < 1 || dd > 2 || K->fail) return -1;
  double Q[MAXDEG + 1] = { 0 };
  for (int k = dn - dd; k >= 0; k--) {               // long division: N = Q D + R
    double q = N[k + dd] / D[dd];
    Q[k] = q;
    for (int j = 0; j <= dd; j++) N[k + j] -= q * D[j];
    N[k + dd] = 0;
  }
  int r = num(0);
  if (dn >= dd) r = integ(simp(mkpoly(Q, dn - dd)));
  if (r < 0) return -1;
  int Db = mkpoly(D, dd), lnD = fn(T_LN, fn(T_ABS, Db));
  if (dd == 1) return add(r, mul(num(N[0] / D[1]), lnD));
  double a = N[1] / (2 * D[2]), b = N[0] - a * D[1];   // R = a D' + b
  if (a != 0) r = add(r, mul(num(a), lnD));
  if (b != 0) { int q = quad_inv(D); if (q < 0) return -1; r = add(r, mul(num(b), q)); }
  return r;
}

// Polynomial coefficients of k (after expanding): c[0..deg]. -1 if k isn't
// a polynomial in x with numeric coefficients.
static int poly(int k, double *c) {
  for (int i = 0; i <= MAXDEG; i++) c[i] = 0;
  int e = expand(k);
  if (K->fail) return -1;
  List l = { .n = 0 };
  flat(&l, e, O_ADD);
  int top = 0;
  for (int i = 0; i < l.n; i++) {
    int rest;
    double co = split(l.k[i], &rest);
    int d = degree(rest);
    if (d < 0 || d > MAXDEG) return -1;
    c[d] += co;
    if (d > top && c[d] != 0) top = d;
  }
  return top;
}

static int integ_factor(int Fk) {
  if (OP(Fk) == O_X) return mul(num(0.5), pw(Fk, num(2)));
  if (OP(Fk) == O_POW) {
    int b = A(Fk), e = B(Fk);
    if (!has_x(e)) {
      double a = lin(b);
      if (a != 0) return outer(Fk, b, a);
      if (!isnum(e)) return -1;
      double c[MAXDEG + 1], ev = NV(e);
      int t = trig_pow(b, ev);
      if (t >= 0) return t;
      int d = poly(b, c);
      if (K->fail || d < 1) return -1;
      if (is_int(ev) && ev >= 2 && ev <= MAXDEG) { int x = expand(Fk); return K->fail ? -1 : integ(x); }
      if (d == 2 && ev == -1) return quad_inv(c);
      if (d == 2 && ev == -0.5) return quad_isqrt(c);
      if (is_int(ev) && ev < 0) return integ_rat(&Fk, 1);
      return -1;
    }
    if (!has_x(b)) { double a = lin(e); return a != 0 ? outer(Fk, e, a) : -1; }
    return -1;
  }
  if (OP(Fk) == O_FN && B(Fk) < 0) {
    double a = lin(A(Fk));
    return a != 0 ? outer(Fk, A(Fk), a) : -1;
  }
  return -1;
}

static int product(const int *f, int n, int skip) {
  int r = num(1);
  for (int i = 0; i < n; i++) if (i != skip) r = mul(r, f[i]);
  return r;
}

static int integ_prod(const int *f, int n) {
  if (n == 1) return integ_factor(f[0]);
  // substitution: one factor is F(g) and the rest is a constant times g'
  for (int i = 0; i < n; i++) {
    int Fk = f[i], g[2], ng = 0;
    if (OP(Fk) == O_FN && B(Fk) < 0) g[ng++] = A(Fk);
    if (OP(Fk) == O_POW && !has_x(B(Fk))) g[ng++] = A(Fk);
    if (OP(Fk) == O_POW && !has_x(A(Fk))) g[ng++] = B(Fk);
    for (int j = 0; j < ng; j++) {
      if (OP(g[j]) == O_X) continue;
      int ratio = simp(mul(product(f, n, i), inv(simp(der(g[j])))));
      if (K->fail) return -1;
      if (has_x(ratio)) continue;
      int G = outer(Fk, g[j], 1);
      if (G >= 0) return mul(ratio, G);
    }
    // F itself as the inner function: F * F' -> F^2 / 2 (e.g. ln(x)/x)
    int ratio = simp(mul(product(f, n, i), inv(simp(der(Fk)))));
    if (K->fail) return -1;
    if (!has_x(ratio)) return mul(mul(ratio, num(0.5)), pw(Fk, num(2)));
  }
  // by parts: x^n times sin/cos/sinh/cosh/e^ of a linear argument, or x^n ln(x)
  if (n == 2) {
    for (int i = 0; i < 2; i++) {
      int P = f[i], T = f[1 - i];
      double pn = OP(P) == O_X ? 1 : (OP(P) == O_POW && OP(A(P)) == O_X && isnum(B(P))) ? NV(B(P)) : 0;
      if (pn == 0) continue;
      if (OP(T) == O_FN && F(T) == T_LN && OP(A(T)) == O_X && pn != -1) {
        int x1 = pw(sym(O_X), num(pn + 1));
        return add(mul(mul(num(1 / (pn + 1)), x1), T), neg(mul(num(1 / ((pn + 1) * (pn + 1))), x1)));
      }
      bool ok = (OP(T) == O_FN && B(T) < 0 && (F(T) == T_SIN || F(T) == T_COS || F(T) == T_SINH || F(T) == T_COSH) &&
                 lin(A(T)) != 0) ||
                (OP(T) == O_POW && !has_x(A(T)) && lin(B(T)) != 0);
      if (!ok || !is_int(pn) || pn < 1 || pn > MAXDEG) continue;
      int sum = num(0), dp = P, a = T;
      for (int k = 0; k <= (int)pn; k++) {
        a = integ(simp(a));
        if (a < 0) return -1;
        sum = add(sum, mul(num(k % 2 ? -1 : 1), mul(dp, a)));
        dp = simp(der(dp));
      }
      return sum;
    }
  }
  return -1;
}

static int integ(int k) {
  if (K->fail) return -1;
  if (++K->depth > 60) { K->fail = true; return -1; }
  int r = -1;
  if (!has_x(k)) r = mul(k, sym(O_X));
  else if (OP(k) == O_ADD) {
    int a = integ(A(k)), b = a >= 0 ? integ(B(k)) : -1;
    r = (a >= 0 && b >= 0) ? add(a, b) : -1;
  } else if (OP(k) == O_MUL) {
    List l = { .n = 0 };
    flat(&l, k, O_MUL);
    int c = num(1), xf[MAXL], n = 0;
    bool sum = false;
    for (int i = 0; i < l.n; i++) {
      if (has_x(l.k[i])) { xf[n++] = l.k[i]; sum |= OP(l.k[i]) == O_ADD; }
      else c = mul(c, l.k[i]);
    }
    int q = integ_rat(xf, n);                        // a quotient of polynomials
    if (q >= 0) r = mul(c, q);
    else if (K->fail) r = -1;
    else if (sum) {                                  // multiply out, then term by term
      int e = expand(k);
      r = (OP(e) == O_ADD) ? integ(e) : -1;
    } else {
      int p = integ_prod(xf, n);
      r = p >= 0 ? mul(c, p) : -1;
    }
  } else {
    r = integ_factor(k);
  }
  K->depth--;
  return K->fail ? -1 : r;
}

// ------------------------------------------------------------------ factor

typedef long long i64;
static i64 gcd64(i64 a, i64 b) { if (a < 0) a = -a; if (b < 0) b = -b; while (b) { i64 t = a % b; a = b; b = t; } return a; }

// The positive divisors of n, found in pairs up to sqrt(n) (up to 20000, so
// for huge n only the small ones and their partners). At most max of them.
static int divisors(i64 n, i64 *out, int max) {
  if (n < 0) n = -n;
  int k = 0;
  for (i64 i = 1; i * i <= n && i <= 20000 && k < max; i++) {
    if (n % i) continue;
    out[k++] = i;
    if (i != n / i && k < max) out[k++] = n / i;
  }
  return k;
}

// Factors of a polynomial with integer coefficients a[0..d]: x^m, (q x - p)^mult, and what's left.
static int factor_poly(const double *c, int d) {
  double den = 1;
  i64 a[MAXDEG + 1];
  for (int i = 0; i <= d; i++) {                     // common denominator
    double p, q;
    if (!rat(c[i], 10000, &p, &q)) return -1;
    den = den / gcd64((i64)den, (i64)q) * q;
    if (den > 1e9) return -1;
  }
  for (int i = 0; i <= d; i++) {
    double v = m_round(c[i] * den);
    if (m_fabs(v) > 1e12) return -1;
    a[i] = (i64)v;
  }
  i64 g = 0;
  for (int i = 0; i <= d; i++) g = gcd64(g, a[i]);
  if (g == 0) return num(0);
  if (a[d] < 0) g = -g;
  for (int i = 0; i <= d; i++) a[i] /= g;
  int m = 0;                                         // x^m
  while (m < d && a[m] == 0) m++;
  for (int i = m; i <= d; i++) a[i - m] = a[i];
  d -= m;
  int r = num((double)g / den);
  if (m) r = mul(r, m == 1 ? sym(O_X) : pw(sym(O_X), num(m)));
  // rational roots p/q: p divides a[0], q divides a[d]. Only real divisors are
  // tried (found up to their square roots), with a cap on the work, so a
  // polynomial with huge coefficients can't stall the watch.
  int budget = 6000;
  bool again = true;
  while (again && d >= 1 && budget > 0) {
    again = false;
    i64 qs[64], ps[64];
    int nq = divisors(a[d], qs, 64), np = divisors(a[0], ps, 64);
    for (int iq = 0; iq < nq && !again && budget > 0; iq++)
      for (int ip = 0; ip < np && !again && budget > 0; ip++) {
        i64 q = qs[iq], p = ps[ip];
        if (gcd64(p, q) != 1) continue;
        for (int sgn = 1; sgn >= -1 && !again; sgn -= 2) {
          i64 pp = sgn * p;
          int mult = 0;
          budget--;
          for (;;) {                                 // divide by (q x - pp) while it goes
            if (d < 1) break;
            i64 b[MAXDEG];
            b[d - 1] = a[d] / q;
            if (a[d] % q) break;
            bool ok = true;
            for (int k = d - 1; k >= 1 && ok; k--) {
              i64 t;                                 // a[k] + pp b[k], unless that overflows
              if (__builtin_mul_overflow(pp, b[k], &t) || __builtin_add_overflow(t, a[k], &t) || t % q) ok = false;
              else b[k - 1] = t / q;
            }
            i64 last;
            if (!ok || __builtin_mul_overflow(-pp, b[0], &last) || a[0] != last) break;
            for (int k = 0; k < d; k++) a[k] = b[k];
            d--;
            mult++;
          }
          if (mult) {
            int lf = simp(add(mul(num((double)q), sym(O_X)), num((double)-pp)));
            r = mul(r, mult == 1 ? lf : pw(lf, num(mult)));
            again = true;                            // the coefficients changed: new divisors
          }
        }
      }
  }
  // quadratic factors s x^2 + t x + u of what's left (x^4-5x^2+6, x^4+4)
  while (d >= 4 && budget > 0) {
    i64 ss[32], us[32];
    int ns = divisors(a[d], ss, 32), nu = divisors(a[0], us, 32), found = 0;
    for (int is = 0; is < ns && !found; is++)
      for (int iu = 0; iu < nu * 2 && !found; iu++)
        for (i64 t = -40; t <= 40 && !found && budget > 0; t++) {
          i64 S = ss[is], U = (iu & 1) ? -us[iu / 2] : us[iu / 2], rem[MAXDEG + 1], b[MAXDEG];
          budget--;
          for (int k = 0; k <= d; k++) rem[k] = a[k];
          bool ok = true;
          for (int k = d - 2; k >= 0 && ok; k--) {
            if (rem[k + 2] % S) { ok = false; break; }
            i64 q = rem[k + 2] / S, m1, m2;
            b[k] = q;
            if (__builtin_mul_overflow(q, t, &m1) || __builtin_mul_overflow(q, U, &m2) ||
                __builtin_sub_overflow(rem[k + 1], m1, &rem[k + 1]) || __builtin_sub_overflow(rem[k], m2, &rem[k])) ok = false;
            rem[k + 2] = 0;
          }
          if (!ok || rem[1] || rem[0]) continue;
          for (int k = 0; k <= d - 2; k++) a[k] = b[k];
          d -= 2;
          int qf = simp(add(add(mul(num((double)S), pw(sym(O_X), num(2))), mul(num((double)t), sym(O_X))), num((double)U)));
          r = mul(r, qf);
          found = 1;
        }
    if (!found) break;
  }
  if (d >= 1) {                                      // what's left, as a polynomial
    int rest = num((double)a[0]);
    for (int i = 1; i <= d; i++)
      rest = add(rest, mul(num((double)a[i]), i == 1 ? sym(O_X) : pw(sym(O_X), num(i))));
    rest = simp(rest);
    r = mul(r, rest);
  } else {
    r = mul(r, num((double)a[0]));
  }
  return r;
}

// ------------------------------------------------------------------ emit

static void tok(int t) {
  TokBuf *o = K->out;
  if (o->n >= CALC_MAX_TOK) { K->fail = true; return; }
  o->t[o->n++] = (uint8_t)t;
}
static int last(void) { return K->out->n ? K->out->t[K->out->n - 1] : -1; }

static void emit_expr(int k);
static void emit_factor(int k);

static void emit_int(double v) {          // v >= 0, whole
  char d[20];
  int n = 0;
  do { double q = m_floor(v / 10); d[n++] = (char)('0' + (int)(v - q * 10)); v = q; } while (v > 0 && n < 19);
  while (n) tok(d[--n]);
}

static void emit_decimal(double v) {      // v > 0, not a simple fraction
  char buf[32];
  calc_format(v, 10, buf, sizeof buf);
  for (char *p = buf; *p; p++) {
    if (digit(*p) || *p == '.' || *p == '-') tok(*p);
    else if (*p == 'E') tok(T_EE);
    else { K->fail = true; return; }
  }
}

static void emit_num(double v) {          // v >= 0 (sign handled by the caller)
  double p, q;
  if (is_int(v)) emit_int(v);
  else if (rat(v, 1000, &p, &q)) { emit_int(m_fabs(p)); tok('/'); emit_int(q); }
  else emit_decimal(v);
}

static bool atomic(int k) {
  int o = OP(k);
  return o == O_X || o == O_Y || o == O_PI || o == O_E || o == O_FN || (o == O_NUM && is_int(NV(k)) && NV(k) >= 0);
}

static void emit_paren(int k) { tok('('); emit_expr(k); tok(')'); }

static void emit_factor(int k) {
  if (K->fail) return;
  if ((digit(last()) || last() == '.') && (OP(k) == O_NUM || (OP(k) == O_POW && isnum(A(k)) && !(isnum(B(k)) && NV(B(k)) == 0.5))))
    tok('*');                                        // 2 * 3^x, not 23^x
  switch (OP(k)) {
    case O_X: tok(T_X); return;
    case O_Y: tok(T_Y); return;
    case O_PI: tok(T_PI); return;
    case O_E: tok(T_E); return;
    case O_NUM:
      if (NV(k) < 0) { tok('('); tok('-'); emit_num(-NV(k)); tok(')'); }
      else if (is_int(NV(k))) emit_num(NV(k));
      else { tok('('); emit_num(NV(k)); tok(')'); }
      return;
    case O_POW: {
      int b = A(k), e = B(k);
      if (isnum(e) && NV(e) == 0.5) { tok(T_SQRT); emit_expr(b); tok(')'); return; }
      if (isnum(e) && m_fabs(NV(e) - 1.0 / 3) < 1e-15) { tok(T_CBRT); emit_expr(b); tok(')'); return; }
      if (!isnum(e) && OP(b) == O_E) { tok(T_EXP); emit_expr(e); tok(')'); return; }
      if (!isnum(e) && isv(b, 10)) { tok(T_TENX); emit_expr(e); tok(')'); return; }
      if (atomic(b)) emit_factor(b); else emit_paren(b);
      tok('^');
      if (isnum(e) && is_int(NV(e)) && NV(e) >= 0) emit_num(NV(e));
      else if (isnum(e)) { tok('('); if (NV(e) < 0) tok('-'); emit_num(m_fabs(NV(e))); tok(')'); }
      else emit_paren(e);
      return;
    }
    case O_FN: {
      int f = F(k);
      if (f == T_FACT) { if (atomic(A(k))) emit_factor(A(k)); else emit_paren(A(k)); tok(T_FACT); return; }
      if (f == T_NCR || f == T_NPR || f == T_MOD) {
        tok('('); emit_paren(A(k)); tok(f); emit_paren(B(k)); tok(')');
        return;
      }
      tok(f);
      emit_expr(A(k));
      if (B(k) >= 0) { tok(','); emit_expr(B(k)); }
      tok(')');
      return;
    }
    default:
      emit_paren(k);
      return;
  }
}

// A product: numerator factors, then "/" and the denominator if there is one.
static void emit_term(double c, int rest) {
  List l = { .n = 0 };
  if (rest >= 0) flat(&l, rest, O_MUL);
  int up[MAXL], dn[MAXL], nu = 0, nd = 0;
  for (int i = 0; i < l.n; i++) {
    int f = l.k[i];
    if (OP(f) == O_POW && isnum(B(f)) && NV(B(f)) < 0) {
      double e = -NV(B(f));
      dn[nd++] = e == 1 ? A(f) : pw(A(f), num(e));
    } else if (isnum(f)) {
      c *= NV(f);
    } else up[nu++] = f;
  }
  double p = c, q = 1;
  if (!is_int(c) && !rat(c, 1000, &p, &q)) { p = c; q = 1; }
  bool frac = nd > 0 || q != 1;
  bool wrap = frac && (nu + (p != 1 || !nu ? 1 : 0)) > 1;
  if (wrap) tok('(');
  if (p != 1 || !nu) { if (is_int(p)) emit_num(p); else emit_decimal(p); }
  for (int i = 0; i < nu; i++) emit_factor(up[i]);
  if (wrap) tok(')');
  if (!frac) return;
  tok('/');
  bool wd = (nd + (q != 1)) > 1;
  if (wd) tok('(');
  if (q != 1) emit_num(q);
  for (int i = 0; i < nd; i++) {
    int f = dn[i];
    if (OP(f) == O_ADD && (nd + (q != 1)) == 1) emit_paren(f); else emit_factor(f);
  }
  if (wd) tok(')');
}

static void emit_expr(int k) {
  if (K->fail) return;
  List l = { .n = 0 };
  flat(&l, k, O_ADD);
  bool polyn = true;                                 // a polynomial keeps its falling powers
  for (int i = 0; i < l.n && polyn; i++) { int r0; split(l.k[i], &r0); polyn = degree(r0) >= 0; }
  for (int i = 1; i < l.n && !polyn; i++) {          // otherwise start with a positive term if there is one
    int r0, ri;
    if (split(l.k[0], &r0) >= 0) break;
    if (split(l.k[i], &ri) > 0) { int t = l.k[i]; for (int j = i; j > 0; j--) l.k[j] = l.k[j - 1]; l.k[0] = t; break; }
  }
  for (int i = 0; i < l.n; i++) {
    int rest;
    double c = split(l.k[i], &rest);
    if (c < 0) { tok('-'); c = -c; }
    else if (i > 0) tok('+');
    emit_term(c, rest);
  }
}

static int radpow(int k) {                            // sqrt(u) -> u^(1/2), cbrt(u) -> u^(1/3)
  if (K->fail) return k;
  switch (OP(k)) {
    case O_ADD: case O_MUL: case O_POW: {
      int a = radpow(A(k)), b = radpow(B(k));
      return (a == A(k) && b == B(k)) ? k : node(OP(k), 0, a, b);
    }
    case O_FN: {
      int a = radpow(A(k));
      if (B(k) < 0 && F(k) == T_SQRT) return pw(a, num(0.5));
      if (B(k) < 0 && F(k) == T_CBRT) return pw(a, num(1.0 / 3));
      return a == A(k) ? k : node(O_FN, F(k), a, B(k));
    }
    default: return k;
  }
}

// ------------------------------------------------------------------ API

enum { DO_DER, DO_INT, DO_FACTOR, DO_EXPAND };

static bool run(int what, const TokBuf *f, const CalcVars *v, TokBuf *out) {
  Ctx *c = malloc(sizeof(Ctx));
  if (!c) return false;
  K = c;
  c->nn = c->nv = c->depth = 0;
  c->fail = false;
  c->deg = v->deg;
  c->ans = v->ans;
  for (int i = 0; i < CALC_MEMS; i++) c->mem[i] = v->m[i];
  c->t = f->t; c->nt = f->n; c->i = 0;
  c->out = out;
  out->n = 0;
  num(0);                                             // node 0: a harmless 0 to fall back on
  int lhs = p_expr(), rhs = -1;
  if (!c->fail && pk() == '=') { c->i++; rhs = p_expr(); }
  bool ok = !c->fail && c->i == c->nt && f->n > 0;
  if (ok && rhs >= 0 && (what == DO_DER || what == DO_INT)) ok = false;   // no '=' in calculus
  if (ok) {
    lhs = simp(lhs);
    if (rhs >= 0) rhs = simp(rhs);
    int r = -1;
    switch (what) {
      case DO_DER: r = simp(der(lhs)); break;
      case DO_INT:
        r = integ(lhs);
        if (r < 0 && !c->fail) {                     // again with sqrt(u) as u^(1/2): x sqrt(x), x/sqrt(x^2+1)
          int l2 = radpow(lhs);
          if (l2 != lhs) { l2 = simp(l2); c->depth = 0; r = integ(l2); }
        }
        if (r >= 0) r = simp(r);
        // 2(x atanh(x/2)/2 + ...) reads better multiplied out
        if (r >= 0 && OP(r) == O_MUL && isnum(A(r)) && OP(B(r)) == O_ADD) r = simp(distrib(A(r), B(r)));
        break;
      case DO_EXPAND:
        lhs = expand(lhs);
        if (rhs >= 0) rhs = expand(rhs);
        r = lhs;
        break;
      case DO_FACTOR: {
        double co[MAXDEG + 1];
        int p = rhs >= 0 ? simp(add(lhs, neg(rhs))) : lhs;
        int d = poly(p, co);
        r = d >= 0 ? factor_poly(co, d) : -1;
        if (r >= 0) r = simp(r);
        if (rhs >= 0) rhs = num(0);
        break;
      }
    }
    ok = r >= 0 && !c->fail;
    if (ok) {
      emit_expr(r);
      if (rhs >= 0 && what != DO_DER && what != DO_INT) { tok('='); emit_expr(rhs); }
      ok = !c->fail && out->n > 0;
    }
  }
  free(c);
  K = NULL;
  if (!ok) out->n = 0;
  return ok;
}

bool cas_derivative(const TokBuf *f, const CalcVars *v, TokBuf *out) { return run(DO_DER, f, v, out); }
bool cas_integral(const TokBuf *f, const CalcVars *v, TokBuf *out) { return run(DO_INT, f, v, out); }
bool cas_factor(const TokBuf *f, const CalcVars *v, TokBuf *out) { return run(DO_FACTOR, f, v, out); }
bool cas_expand(const TokBuf *f, const CalcVars *v, TokBuf *out) { return run(DO_EXPAND, f, v, out); }
