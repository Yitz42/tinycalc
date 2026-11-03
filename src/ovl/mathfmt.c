// mathfmt.c - see mathfmt.h
#include "../c/mathfmt.h"
#include <stddef.h>

#define NONE      0xFF
#define MAX_DEPTH 24            // deeper trees fall back to one line of text

static int clampl(int l) { return l < 0 ? 0 : (l >= MATH_LEVELS ? MATH_LEVELS - 1 : l); }
static int imax(int a, int b) { return a > b ? a : b; }

// ------------------------------------------------------------------ build


static uint8_t mk(MathDoc *d, uint8_t type, uint8_t a, uint8_t b, const char *text) {
  if (d->count >= MATH_MAX_NODES) return 0;   // node 0 is a placeholder box
  uint8_t k = (uint8_t)d->count++;
  MNode *n = &d->n[k];
  n->type = type; n->a = a; n->b = b; n->text = text;
  n->start = n->len = 0; n->lvl = 0;
  int da = a != NONE ? d->n[a].depth : 0, db = b != NONE ? d->n[b].depth : 0;
  n->depth = (uint8_t)(1 + imax(da, db) > 250 ? 250 : 1 + imax(da, db));
  return k;
}
static uint8_t leaf(MathDoc *d, uint8_t type, const char *t) { return mk(d, type, NONE, NONE, t); }
static uint8_t box(MathDoc *d) { return leaf(d, MN_BOX, NULL); }

static int pk(MathDoc *d) { return d->i < d->ntok ? d->tok[d->i] : -1; }
static bool is_digit(int k) { return k >= '0' && k <= '9'; }
static bool is_func(int k) { return calc_is_func(k); }
static bool starts_primary(int k) {
  return is_digit(k) || k == '.' || k == T_EE || k == '(' || is_func(k) ||
         k == T_PI || k == T_E || k == T_X || k == T_Y || k == T_Z || k == T_ANS || k == T_I || k == T_CUR || k == T_RAN || (k >= T_MA && k <= T_MF);
}
static bool is_sep(int k) { return k == T_SEP || k == T_NL || k == T_COMMA; }
// Parentheses that only group (fraction parts, exponents) are implied by layout.
static uint8_t strip(MathDoc *d, uint8_t k) {
  return (k != NONE && d->n[k].type == MN_PAREN) ? d->n[k].a : k;
}

static uint8_t b_expr(MathDoc *d, int depth);
static uint8_t b_unary(MathDoc *d, int depth);

static uint8_t b_digits(MathDoc *d) {
  int s = d->i;
  while (pk(d) >= 0 && (is_digit(pk(d)) || pk(d) == '.')) d->i++;
  if (d->i == s) return NONE;
  uint8_t k = leaf(d, MN_NUM, NULL);
  if (k) { d->n[k].start = (uint8_t)s; d->n[k].len = (uint8_t)(d->i - s); }
  return k;
}

static uint8_t b_number(MathDoc *d) {
  uint8_t m = b_digits(d);
  if (pk(d) != T_EE) return m;
  d->i++;
  bool neg = false;
  if (pk(d) == '-') { neg = true; d->i++; }
  else if (pk(d) == '+') d->i++;
  uint8_t e = b_digits(d);
  if (e == NONE) e = box(d);
  if (neg) e = mk(d, MN_PREFIX, e, NONE, "-");
  uint8_t p = mk(d, MN_POW, leaf(d, MN_TEXT, "10"), e, NULL);
  return m == NONE ? p : mk(d, MN_BIN, m, p, "*");
}

// Body of "(" or "f(": expression, then an optional ")".
static bool s_closed;                            // did the last group end with a typed ")"?
static uint8_t b_group(MathDoc *d, int depth) {
  uint8_t in = b_expr(d, depth + 1);
  while (pk(d) == ',') {                         // gcd(a, b)
    d->i++;
    in = mk(d, MN_BIN, in, b_expr(d, depth + 1), ",");
  }
  s_closed = pk(d) == ')';
  if (s_closed) d->i++;
  return in;
}
// The arguments of f(a, b, c), which the group builds as ((a, b), c).
static int args(MathDoc *d, uint8_t k, uint8_t *out, int max) {
  const MNode *n = &d->n[k];
  if (n->type == MN_BIN && n->text && n->text[0] == ',') {
    int i = args(d, n->a, out, max);
    if (i < max) out[i++] = n->b;
    return i;
  }
  out[0] = k;
  return 1;
}

// Brackets: one not typed yet (closed for you on =) is drawn dim, as a hint.
static uint8_t paren(MathDoc *d, uint8_t a, bool closed) {
  uint8_t k = mk(d, MN_PAREN, a, NONE, NULL);
  d->n[k].len = closed ? 0 : 1;
  return k;
}

static uint8_t b_primary(MathDoc *d, int depth) {
  int k = pk(d);
  if (k < 0 || !starts_primary(k)) return box(d);
  if (is_digit(k) || k == '.' || k == T_EE) return b_number(d);
  d->i++;
  switch (k) {
    case T_PI: return leaf(d, MN_PI, NULL);
    case T_E: return leaf(d, MN_TEXT, "e");
    case T_X: return leaf(d, MN_TEXT, "x");
    case T_Y: return leaf(d, MN_TEXT, "y");
    case T_Z: return leaf(d, MN_TEXT, "z");
    case T_RAN: return leaf(d, MN_TEXT, "Ran#");
    case T_I: return leaf(d, MN_TEXT, "i");
    case T_CUR: return leaf(d, MN_CURSOR, NULL);
    case T_ANS: return leaf(d, MN_TEXT, "Ans");
    default: break;
  }
  if (k >= T_MA && k <= T_MF) {
    static const char *M[6] = { "A", "B", "C", "D", "E", "F" };
    return leaf(d, MN_TEXT, M[k - T_MA]);
  }
  if (depth > MAX_DEPTH) return leaf(d, MN_TEXT, calc_tok_text((uint8_t)k));  // flattened by caller
  uint8_t a = b_group(d, depth);
  bool closed = s_closed;
  if (k == '(') return paren(d, a, closed);
  if (k == T_LOGB || k == T_ROOT || k == T_SUM || k == T_PROD) {
    uint8_t g[3];                                    // the arguments (boxes for ones not typed yet)
    int n = args(d, a, g, 3);
    for (int i = n; i < 3; i++) g[i] = box(d);
    if (k == T_LOGB)
      return mk(d, MN_CAT, mk(d, MN_SUB, leaf(d, MN_TEXT, "log"), strip(d, g[0]), NULL), paren(d, g[1], closed), NULL);
    if (k == T_ROOT) return mk(d, MN_NROOT, strip(d, g[1]), strip(d, g[0]), NULL);
    uint8_t body = strip(d, g[0]);
    const MNode *bn = &d->n[body];
    if (bn->type == MN_BIN && (bn->text[0] == '+' || bn->text[0] == '-')) body = paren(d, body, true);
    uint8_t lo = mk(d, MN_BIN, leaf(d, MN_TEXT, "x"), strip(d, g[1]), "=");
    return mk(d, MN_BIGOP, body, mk(d, MN_LIMS, lo, strip(d, g[2]), NULL), k == T_SUM ? "S" : "P");
  }
  a = strip(d, a);   // sqrt((1+x)) shows one set of brackets
  switch (k) {
    case T_SQRT: return mk(d, MN_SQRT, a, NONE, NULL);
    case T_CBRT: return mk(d, MN_CBRT, a, NONE, NULL);
    case T_ABS: return mk(d, MN_ABS, a, NONE, NULL);
    case T_EXP: return mk(d, MN_POW, leaf(d, MN_TEXT, "e"), strip(d, a), NULL);
    case T_TENX: return mk(d, MN_POW, leaf(d, MN_TEXT, "10"), strip(d, a), NULL);
    default: break;
  }
  const char *name;
  bool inv = false;
  switch (k) {
    case T_SIN: name = "sin"; break;
    case T_COS: name = "cos"; break;
    case T_TAN: name = "tan"; break;
    case T_ASIN: name = "sin"; inv = true; break;
    case T_ACOS: name = "cos"; inv = true; break;
    case T_ATAN: name = "tan"; inv = true; break;
    case T_LN: name = "ln"; break;
    case T_SINH: name = "sinh"; break;
    case T_COSH: name = "cosh"; break;
    case T_TANH: name = "tanh"; break;
    case T_ASINH: name = "sinh"; inv = true; break;
    case T_ACOSH: name = "cosh"; inv = true; break;
    case T_ATANH: name = "tanh"; inv = true; break;
    case T_ROUND: name = "round"; break;
    case T_FLOOR: name = "floor"; break;
    case T_GCD: name = "gcd"; break;
    case T_LCM: name = "lcm"; break;
    case T_AND: name = "and"; break;
    case T_OR: name = "or"; break;
    case T_XOR: name = "xor"; break;
    case T_SHL: name = "shl"; break;
    case T_SHR: name = "shr"; break;
    case T_NOT: name = "not"; break;
    case T_CONJ: name = "conj"; break;
    case T_RE: name = "Re"; break;
    case T_IM: name = "Im"; break;
    case T_ARG: name = "arg"; break;
    case T_RANINT: name = "RanInt"; break;
    case T_NPDF: name = "normpdf"; break;
    case T_NCDF: name = "normcdf"; break;
    case T_NINV: name = "invnorm"; break;
    default: name = "log"; break;
  }
  uint8_t f = leaf(d, MN_TEXT, name);
  if (inv) f = mk(d, MN_POW, f, leaf(d, MN_TEXT, "-1"), NULL);
  return mk(d, MN_CAT, f, paren(d, a, closed), NULL);
}

static uint8_t b_postfix(MathDoc *d, int depth) {
  uint8_t v = b_primary(d, depth);
  for (;;) {
    int k = pk(d);
    const char *sup = NULL, *after = NULL;
    if (k == T_SQ) sup = "2";
    else if (k == T_CUBE) sup = "3";
    else if (k == T_RECIP) sup = "-1";
    else if (k == T_PCT) after = "%";
    else if (k == T_FACT) after = "!";
    else return v;
    d->i++;
    if (sup) v = mk(d, MN_POW, v, leaf(d, MN_TEXT, sup), NULL);
    else v = mk(d, MN_CAT, v, leaf(d, MN_TEXT, after), NULL);
  }
}

static uint8_t b_power(MathDoc *d, int depth) {
  uint8_t b = b_postfix(d, depth);
  if (pk(d) != '^') return b;
  d->i++;
  uint8_t e = b_unary(d, depth + 1);
  return mk(d, MN_POW, b, strip(d, e), NULL);
}

static uint8_t b_unary(MathDoc *d, int depth) {
  int k = pk(d);
  if ((k == '-' || k == '+') && depth <= MAX_DEPTH) {
    d->i++;
    uint8_t a = b_unary(d, depth + 1);
    return mk(d, MN_PREFIX, a, NONE, k == '-' ? "-" : "+");
  }
  return b_power(d, depth);
}

static uint8_t b_term(MathDoc *d, int depth) {
  uint8_t v = b_unary(d, depth);
  for (;;) {
    int k = pk(d);
    if (k == '*') {
      d->i++;
      v = mk(d, MN_BIN, v, b_unary(d, depth), "*");
    } else if (k == T_NCR || k == T_NPR || k == T_MOD) {
      d->i++;
      v = mk(d, MN_BIN, v, b_unary(d, depth), k == T_NCR ? "C" : k == T_NPR ? "P" : "mod");
    } else if (k == '/') {
      d->i++;
      uint8_t r = b_unary(d, depth);
      v = mk(d, MN_FRAC, strip(d, v), strip(d, r), NULL);
    } else if (k >= 0 && starts_primary(k)) {
      v = mk(d, MN_IMPL, v, b_power(d, depth), NULL);
    } else {
      return v;
    }
  }
}

static uint8_t b_expr(MathDoc *d, int depth) {
  uint8_t v = b_term(d, depth);
  for (;;) {
    int k = pk(d);
    if (k != '+' && k != '-') return v;
    d->i++;
    v = mk(d, MN_BIN, v, b_term(d, depth), k == '+' ? "+" : "-");
  }
}

// One equation: expressions joined by '=', skipping stray ")" and anything
// else that can't start an expression. Stops at T_SEP.
static uint8_t b_eqn(MathDoc *d) {
  if (d->i >= d->ntok || is_sep(pk(d))) return box(d);
  uint8_t root = b_expr(d, 0);
  while (d->i < d->ntok && !is_sep(pk(d))) {
    int k = pk(d), before = d->i;
    if (k == ')') { d->i++; continue; }          // stray ")"
    if (k == '=' || k == ',') {                  // "3, 4" (for Pol / Rec) shows its comma
      d->i++;
      root = mk(d, MN_BIN, root, b_expr(d, 0), k == '=' ? "=" : ",");
      continue;
    }
    uint8_t more = b_expr(d, 0);
    if (d->i == before) d->i++;                  // always make progress
    else root = mk(d, MN_IMPL, root, more, NULL);
  }
  return root;
}

void math_build_raw(MathDoc *d, const uint8_t *t, int n) {
  d->count = 0;
  d->tok = t;
  d->ntok = n;
  d->i = 0;
  box(d);  // node 0: fallback placeholder
  uint8_t root;
  if (n == 0) {
    root = leaf(d, MN_TEXT, "0");
  } else {
    root = b_eqn(d);
    for (int k; (k = pk(d)) >= 0 && is_sep(k);) {     // brace pair, next line, or ", "
      d->i++;
      uint8_t next = b_eqn(d);
      root = k == T_SEP ? mk(d, MN_SYS, root, next, NULL)
           : k == T_NL ? mk(d, MN_STACK, root, next, NULL)
           : mk(d, MN_BIN, root, next, ",");
    }
  }
  if (d->n[root].depth > MAX_DEPTH + 6 || d->count >= MATH_MAX_NODES) {
    // Too deep or too big to typeset safely: show it on one line instead.
    TokBuf b;
    b.n = (uint8_t)(n > CALC_MAX_TOK ? CALC_MAX_TOK : n);
    for (int i = 0; i < b.n; i++) b.t[i] = t[i];
    calc_render(&b, d->flat, sizeof d->flat);
    d->count = 0;
    box(d);
    root = leaf(d, MN_TEXT, d->flat);
  }
  d->root = root;
}

void math_build(MathDoc *d, const TokBuf *b) { math_build_raw(d, b->t, b->n); }

void math_wrap_integral(MathDoc *d, const char *lo, const char *hi) {
  if (d->count > MATH_MAX_NODES - 6) return;   // no room: show the integrand alone
  uint8_t lims = mk(d, MN_LIMS, leaf(d, MN_TEXT, lo), leaf(d, MN_TEXT, hi), NULL);
  d->root = mk(d, MN_INT, d->root, lims, NULL);
}

void math_wrap_derivative(MathDoc *d, const char *at) {
  if (d->count > MATH_MAX_NODES - 8) return;
  uint8_t ddx = mk(d, MN_FRAC, leaf(d, MN_TEXT, "d"), leaf(d, MN_TEXT, "dx"), NULL);
  uint8_t body = mk(d, MN_CAT, ddx, mk(d, MN_PAREN, d->root, NONE, NULL), NULL);
  d->root = at ? mk(d, MN_AT, body, leaf(d, MN_TEXT, at), NULL) : body;   // no point: d/dx (f)
}

// ------------------------------------------------------------------ layout

// Copies a number's digits into a shared buffer (kept off the stack, since
// measure() and draw() recurse). Use the result before the next call.
static const char *num_text(const MathDoc *d, const MNode *n) {
  static char buf[CALC_MAX_TOK + 1];
  int k = 0;
  for (int i = 0; i < n->len; i++) buf[k++] = (char)d->tok[n->start + i];
  buf[k] = 0;
  return buf;
}

// Operators. The watch fonts are ASCII-only, so the times sign is drawn as
// a small cross; everything else is text.
static int op_w(const MathGfx *g, const char *op, int l) {
  if (op[0] == '*') return g->asc[l] / 2 + 2;
  return g->text_w(g->ctx, op, l);
}
static void op_draw(const MathGfx *g, int x, int y, const char *op, int l) {
  if (op[0] != '*') { g->text(g->ctx, x, y, op, l); return; }
  int s = g->asc[l] / 2, cy = y - g->asc[l] / 2, x0 = x + 1, sw = l <= 1 ? 2 : 1;
  g->line(g->ctx, x0, cy - s / 2, x0 + s - 1, cy + s / 2, sw);
  g->line(g->ctx, x0, cy + s / 2, x0 + s - 1, cy - s / 2, sw);
}

static int gap(int l) { static const int G[MATH_LEVELS] = { 4, 3, 2, 1 }; return G[l]; }
static int thick(int l) { return l <= 1 ? 2 : 1; }

// Rightmost leaf is the cursor? ("2|3" needs no product dot)
static bool ends_cursor(const MathDoc *d, uint8_t k) {
  while (k != NONE) {
    const MNode *n = &d->n[k];
    if (n->type == MN_CURSOR) return true;
    if (n->type == MN_IMPL || n->type == MN_CAT) k = n->b; else return false;
  }
  return false;
}

// Leftmost leaf is a number? (2·3 needs a dot, 2x doesn't)
static bool starts_num(const MathDoc *d, uint8_t k) {
  while (k != NONE) {
    const MNode *n = &d->n[k];
    if (n->type == MN_NUM) return true;
    if (n->type == MN_TEXT) return n->text[0] >= '0' && n->text[0] <= '9';
    if (n->type == MN_BIN || n->type == MN_CAT || n->type == MN_IMPL || n->type == MN_POW) k = n->a;
    else return false;
  }
  return false;
}

static void measure(MathDoc *d, const MathGfx *g, uint8_t k, int l) {
  MNode *n = &d->n[k];
  l = clampl(l);
  n->lvl = (uint8_t)l;
  int A = g->asc[l], D = g->desc[l];
  MNode *a = n->a != NONE ? &d->n[n->a] : NULL;
  MNode *b = n->b != NONE ? &d->n[n->b] : NULL;
  switch (n->type) {
    case MN_TEXT:
      n->w = (int16_t)g->text_w(g->ctx, n->text, l); n->asc = (int16_t)A; n->desc = (int16_t)D;
      break;
    case MN_NUM:
      n->w = (int16_t)g->text_w(g->ctx, num_text(d, n), l);
      n->asc = (int16_t)A; n->desc = (int16_t)D;
      break;
    case MN_PI:
      n->w = (int16_t)(A + 3); n->asc = (int16_t)(A * 3 / 4); n->desc = 0;
      break;
    case MN_BOX:
      n->w = (int16_t)(A * 7 / 10 + 1); n->asc = (int16_t)A; n->desc = 0;
      break;
    case MN_BIN: {
      measure(d, g, n->a, l); measure(d, g, n->b, l);
      int ow = op_w(g, n->text, l);
      n->w = (int16_t)(a->w + 2 * gap(l) + ow + b->w);
      n->asc = (int16_t)imax(imax(a->asc, b->asc), A);
      n->desc = (int16_t)imax(a->desc, b->desc);
      break;
    }
    case MN_PREFIX:
      measure(d, g, n->a, l);
      n->w = (int16_t)(g->text_w(g->ctx, n->text, l) + 1 + a->w);
      n->asc = (int16_t)imax(a->asc, A); n->desc = a->desc;
      break;
    case MN_CAT:
    case MN_IMPL: {
      measure(d, g, n->a, l); measure(d, g, n->b, l);
      int mid = 1;
      if (n->type == MN_IMPL && starts_num(d, n->b) && !ends_cursor(d, n->a))
        mid = 6;                                     // room for a centred dot
      n->w = (int16_t)(a->w + mid + b->w);
      n->asc = (int16_t)imax(a->asc, b->asc); n->desc = (int16_t)imax(a->desc, b->desc);
      break;
    }
    case MN_FRAC: {
      measure(d, g, n->a, l); measure(d, g, n->b, l);
      int axis = A / 2, gp = 2, t = thick(l);
      n->w = (int16_t)(imax(a->w, b->w) + 6);
      n->asc = (int16_t)(axis + gp + a->desc + a->asc);
      n->desc = (int16_t)(t + gp + b->asc + b->desc - axis);
      break;
    }
    case MN_POW: {
      measure(d, g, n->a, l); measure(d, g, n->b, l + 1);
      int shift = imax(a->asc - b->asc / 2, A / 2);
      n->w = (int16_t)(a->w + 1 + b->w);
      n->asc = (int16_t)imax(a->asc, shift + b->asc);
      n->desc = (int16_t)imax(a->desc, b->desc - shift);
      break;
    }
    case MN_SQRT:
    case MN_CBRT: {
      measure(d, g, n->a, l);
      int hook = A * 45 / 100 + 3;
      int lo = 0;
      if (n->type == MN_CBRT) lo = g->text_w(g->ctx, "3", clampl(l + 2)) / 2 + 1;
      n->w = (int16_t)(lo + hook + a->w + 3);
      n->asc = (int16_t)(imax(a->asc, A) + 4); n->desc = (int16_t)(a->desc + 1);
      break;
    }
    case MN_ABS:
      measure(d, g, n->a, l);
      n->w = (int16_t)(a->w + 8);
      n->asc = (int16_t)(imax(a->asc, A) + 2); n->desc = (int16_t)(imax(a->desc, D / 2) + 2);
      break;
    case MN_SYS: {
      measure(d, g, n->a, l); measure(d, g, n->b, l);
      int h = a->asc + a->desc + 4 + b->asc + b->desc;
      n->w = (int16_t)(8 + imax(a->w, b->w));
      n->asc = (int16_t)(h / 2 + A / 2);
      n->desc = (int16_t)(h - n->asc);
      break;
    }
    case MN_INT: {
      measure(d, g, n->a, l);
      MNode *lo = &d->n[b->a], *hi = &d->n[b->b];
      measure(d, g, b->a, l + 1); measure(d, g, b->b, l + 1);
      int H = imax(a->asc + a->desc + 6, A * 8 / 5), sw = l <= 1 ? 8 : 6;
      int as = A / 2 + H / 2, ds = H - as;              // sign centred on the math axis
      n->w = (int16_t)(sw + imax(lo->w, hi->w) + 3 + a->w + 3 + g->text_w(g->ctx, "dx", l));
      n->asc = (int16_t)imax(as + 2, a->asc);
      n->desc = (int16_t)imax(ds + lo->desc, a->desc);
      break;
    }
    case MN_LIMS:
      break;   // laid out by MN_INT / MN_BIGOP
    case MN_SUB: {
      measure(d, g, n->a, l); measure(d, g, n->b, l + 1);
      int drop = A / 3;
      n->w = (int16_t)(a->w + 1 + b->w);
      n->asc = (int16_t)imax(a->asc, b->asc - drop);
      n->desc = (int16_t)imax(a->desc, drop + b->desc);
      break;
    }
    case MN_NROOT: {
      measure(d, g, n->a, l); measure(d, g, n->b, l + 2);
      int hook = A * 45 / 100 + 3, lo = imax(b->w - hook / 2, 0) + 1;
      n->w = (int16_t)(lo + hook + a->w + 3);
      n->asc = (int16_t)(imax(imax(a->asc, A) + 4, imax(a->asc, A) * 2 / 5 + b->asc + b->desc + 2));
      n->desc = (int16_t)(a->desc + 1);
      break;
    }
    case MN_BIGOP: {
      measure(d, g, n->a, l);
      MNode *lo = &d->n[b->a], *hi = &d->n[b->b];
      measure(d, g, b->a, l + 1); measure(d, g, b->b, l + 1);
      int H = imax(a->asc + a->desc + 4, A * 7 / 5), sw = H * 3 / 4;
      int as = A / 2 + H / 2, cw = imax(sw, imax(lo->w, hi->w));
      n->w = (int16_t)(cw + 3 + a->w);
      n->asc = (int16_t)imax(as + 1 + hi->asc + hi->desc, a->asc);
      n->desc = (int16_t)imax(H - as + 1 + lo->asc + lo->desc, a->desc);
      break;
    }
    case MN_CURSOR:
      n->w = 6; n->asc = (int16_t)(A + 3); n->desc = (int16_t)(D + 1);
      break;
    case MN_STACK:
      measure(d, g, n->a, l); measure(d, g, n->b, l);
      n->w = (int16_t)imax(a->w, b->w);
      n->asc = a->asc;
      n->desc = (int16_t)(a->desc + 3 + b->asc + b->desc);
      break;
    case MN_AT:
      measure(d, g, n->a, l); measure(d, g, n->b, l + 1);
      n->w = (int16_t)(a->w + 4 + b->w);
      n->asc = (int16_t)(a->asc + 1);
      n->desc = (int16_t)(a->desc + b->desc);
      break;
    case MN_PAREN: {
      measure(d, g, n->a, l);
      int h = a->asc + a->desc;
      int pw = (h > A + D + 4) ? 7 : 5;
      n->w = (int16_t)(2 * pw + a->w);
      n->asc = (int16_t)(imax(a->asc, A) + 2); n->desc = (int16_t)(imax(a->desc, D) + 1);
      break;
    }
  }
}

void math_measure(MathDoc *d, const MathGfx *g, int lvl, int *w, int *asc, int *desc) {
  measure(d, g, d->root, lvl);
  const MNode *r = &d->n[d->root];
  *w = r->w; *asc = r->asc; *desc = r->desc;
}

// ------------------------------------------------------------------ draw

static void draw(const MathDoc *d, const MathGfx *g, uint8_t k, int x, int y) {
  const MNode *n = &d->n[k];
  int l = n->lvl, A = g->asc[l];
  const MNode *a = n->a != NONE ? &d->n[n->a] : NULL;
  const MNode *b = n->b != NONE ? &d->n[n->b] : NULL;
  void *c = g->ctx;
  switch (n->type) {
    case MN_TEXT: g->text(c, x, y, n->text, l); break;
    case MN_NUM: g->text(c, x, y, num_text(d, n), l); break;
    case MN_BOX:
      g->box(c, x + 1, y - n->asc + 1, n->w - 2, n->asc - 1);
      break;
    case MN_PI: {
      // drawn as heavy as the bold digits next to it
      int t = n->asc, w = n->w, sw = l <= 1 ? 3 : 2, h = sw / 2;
      g->line(c, x + 1, y - t + h, x + w - 1, y - t + h, sw);              // bar
      g->line(c, x + w / 3 + 1, y - t + h, x + w / 3 - 2, y - h, sw);      // left leg
      g->line(c, x + 2 * w / 3, y - t + h, x + 2 * w / 3, y - h - 1, sw);  // right leg
      g->line(c, x + 2 * w / 3, y - h - 1, x + 2 * w / 3 + 3, y - h, sw);  // foot
      break;
    }
    case MN_BIN: {
      draw(d, g, n->a, x, y);
      bool comma = n->text[0] == ',';                 // "a, b": no space before the comma
      int ox = x + a->w + (comma ? 0 : gap(l));
      op_draw(g, ox, y, n->text, l);
      draw(d, g, n->b, ox + op_w(g, n->text, l) + (comma ? 2 * gap(l) : gap(l)), y);
      break;
    }
    case MN_PREFIX:
      g->text(c, x, y, n->text, l);
      draw(d, g, n->a, x + g->text_w(c, n->text, l) + 1, y);
      break;
    case MN_CAT:
    case MN_IMPL: {
      draw(d, g, n->a, x, y);
      int ox = x + a->w + 1;
      if (n->type == MN_IMPL && starts_num(d, n->b) && !ends_cursor(d, n->a)) {
        int dy = y - A / 2;                              // centred dot, drawn
        g->line(c, ox + 1, dy, ox + 2, dy, 2);
        ox += 5;
      }
      draw(d, g, n->b, ox, y);
      break;
    }
    case MN_FRAC: {
      int axis = A / 2, gp = 2, t = thick(l);
      int by = y - axis;                       // top row of the bar
      for (int i = 0; i < t; i++) g->line(c, x + 1, by + i, x + n->w - 2, by + i, 1);
      draw(d, g, n->a, x + (n->w - a->w) / 2, by - gp - a->desc);
      draw(d, g, n->b, x + (n->w - b->w) / 2, by + t + gp + b->asc);
      break;
    }
    case MN_POW: {
      int shift = imax(a->asc - b->asc / 2, A / 2);
      draw(d, g, n->a, x, y);
      draw(d, g, n->b, x + a->w + 1, y - shift);
      break;
    }
    case MN_SQRT:
    case MN_CBRT: {
      int hook = A * 45 / 100 + 3, sw = thick(l);
      int lo = 0;
      if (n->type == MN_CBRT) lo = g->text_w(c, "3", clampl(l + 2)) / 2 + 1;
      int top = y - n->asc + 1, bot = y + n->desc - 1;
      int mid = bot - (bot - top) * 2 / 5;
      int x0 = x + lo;
      g->line(c, x0, mid + 1, x0 + 2, mid, sw);                      // small tick
      g->line(c, x0 + 2, mid, x0 + hook / 2, bot, sw);               // down
      g->line(c, x0 + hook / 2, bot, x0 + hook - 1, top, sw);        // up
      g->line(c, x0 + hook - 1, top, x + n->w - 1, top, sw);         // bar
      if (n->type == MN_CBRT) g->text(c, x, mid - 2, "3", clampl(l + 2));
      draw(d, g, n->a, x0 + hook + 1, y);
      break;
    }
    case MN_ABS: {
      int top = y - n->asc + 1, bot = y + n->desc - 1, sw = thick(l);
      g->line(c, x + 1, top, x + 1, bot, sw);
      g->line(c, x + n->w - 2, top, x + n->w - 2, bot, sw);
      draw(d, g, n->a, x + 4, y);
      break;
    }
    case MN_SYS: {
      int top = y - n->asc, bot = y + n->desc - 1, mid = (top + bot) / 2, sw = thick(l);
      // left brace
      g->line(c, x + 5, top, x + 3, top + 2, sw);
      g->line(c, x + 3, top + 2, x + 3, mid - 2, sw);
      g->line(c, x + 3, mid - 2, x, mid, sw);
      g->line(c, x, mid, x + 3, mid + 2, sw);
      g->line(c, x + 3, mid + 2, x + 3, bot - 2, sw);
      g->line(c, x + 3, bot - 2, x + 5, bot, sw);
      int ya = top + a->asc;
      draw(d, g, n->a, x + 8, ya);
      draw(d, g, n->b, x + 8, ya + a->desc + 4 + b->asc);
      break;
    }
    case MN_INT: {
      const MNode *lo = &d->n[b->a], *hi = &d->n[b->b];
      int H = imax(a->asc + a->desc + 6, A * 8 / 5), sw = l <= 1 ? 8 : 6, t = thick(l);
      int as = A / 2 + H / 2, top = y - as, bot = top + H - 1;
      // the long S
      g->line(c, x + sw, top + 2, x + sw - 2, top, t);
      g->line(c, x + sw - 2, top, x + sw - 3, top + 2, t);
      g->line(c, x + sw - 3, top + 2, x + 3, bot - 2, t);
      g->line(c, x + 3, bot - 2, x + 1, bot, t);
      g->line(c, x + 1, bot, x, bot - 2, t);
      draw(d, g, b->b, x + sw + 1, top + hi->asc - 1);           // upper limit
      draw(d, g, b->a, x + sw - 1, bot);                         // lower limit
      int fx = x + sw + imax(lo->w, hi->w) + 3;
      draw(d, g, n->a, fx, y);
      g->text(c, fx + a->w + 3, y, "dx", l);
      break;
    }
    case MN_LIMS:
      break;
    case MN_SUB:
      draw(d, g, n->a, x, y);
      draw(d, g, n->b, x + a->w + 1, y + A / 3);
      break;
    case MN_NROOT: {
      int hook = A * 45 / 100 + 3, sw = thick(l), lo = imax(b->w - hook / 2, 0) + 1;
      int top = y - n->asc + 1, bot = y + n->desc - 1;
      int mid = bot - (bot - top) * 2 / 5;
      int x0 = x + lo;
      g->line(c, x0, mid + 1, x0 + 2, mid, sw);
      g->line(c, x0 + 2, mid, x0 + hook / 2, bot, sw);
      g->line(c, x0 + hook / 2, bot, x0 + hook - 1, top, sw);
      g->line(c, x0 + hook - 1, top, x + n->w - 1, top, sw);
      draw(d, g, n->b, x, mid - 2 - b->desc);           // the index, small, above the tick
      draw(d, g, n->a, x0 + hook + 1, y);
      break;
    }
    case MN_BIGOP: {
      const MNode *lo = &d->n[b->a], *hi = &d->n[b->b];
      int H = imax(a->asc + a->desc + 4, A * 7 / 5), sw = H * 3 / 4, t = thick(l);
      int as = A / 2 + H / 2, cw = imax(sw, imax(lo->w, hi->w));
      int top = y - as, bot = top + H - 1, sx = x + (cw - sw) / 2, ex = sx + sw - 1;
      g->line(c, sx, top, ex, top, t);                  // top bar
      if (n->text[0] == 'S') {                          // sigma
        int mx = sx + sw / 2, my = (top + bot) / 2;
        g->line(c, sx, top, mx, my, t);
        g->line(c, mx, my, sx, bot, t);
        g->line(c, sx, bot, ex, bot, t);
      } else {                                          // pi: two legs
        g->line(c, sx + 2, top, sx + 2, bot, t);
        g->line(c, ex - 2, top, ex - 2, bot, t);
      }
      draw(d, g, b->b, x + (cw - hi->w) / 2, top - 2 - hi->desc);   // upper limit
      draw(d, g, b->a, x + (cw - lo->w) / 2, bot + 2 + lo->asc);    // lower limit
      draw(d, g, n->a, x + cw + 3, y);
      break;
    }
    case MN_CURSOR:                                    // a bold bar in the cursor colour
      if (g->ink) g->ink(c, MATH_INK_CURSOR);
      g->line(c, x + 1, y - n->asc, x + 1, y + n->desc - 1, 3);
      g->line(c, x - 1, y - n->asc, x + 3, y - n->asc, 2);
      g->line(c, x - 1, y + n->desc - 1, x + 3, y + n->desc - 1, 2);
      if (g->ink) g->ink(c, MATH_INK_NORMAL);
      break;
    case MN_STACK:                                     // lines centred under each other
      draw(d, g, n->a, x + (n->w - a->w) / 2, y);
      draw(d, g, n->b, x + (n->w - b->w) / 2, y + a->desc + 3 + b->asc);
      break;
    case MN_AT: {
      draw(d, g, n->a, x, y);
      int bx = x + a->w + 2, sw = thick(l);
      g->line(c, bx, y - a->asc, bx, y + a->desc, sw);
      draw(d, g, n->b, bx + 2, y + a->desc);
      break;
    }
    case MN_PAREN: {
      int h = a->asc + a->desc;
      int pw = (h > A + g->desc[l] + 4) ? 7 : 5, sw = thick(l);
      int top = y - n->asc + 1, bot = y + n->desc - 1;
      int xl = x + 1, xr = x + n->w - 2, cv = pw - 3;  // curve depth
      int r = (bot - top) / 5 + 1;
      g->line(c, xl + cv, top, xl, top + r, sw);
      g->line(c, xl, top + r, xl, bot - r, sw);
      g->line(c, xl, bot - r, xl + cv, bot, sw);
      if (n->len && g->ink) g->ink(c, MATH_INK_DIM);     // not typed yet
      g->line(c, xr - cv, top, xr, top + r, sw);
      g->line(c, xr, top + r, xr, bot - r, sw);
      g->line(c, xr, bot - r, xr - cv, bot, sw);
      if (n->len && g->ink) g->ink(c, MATH_INK_NORMAL);
      draw(d, g, n->a, x + pw, y);
      break;
    }
  }
}

void math_draw(const MathDoc *d, const MathGfx *g, int x, int baseline) {
  draw(d, g, d->root, x, baseline);
}
