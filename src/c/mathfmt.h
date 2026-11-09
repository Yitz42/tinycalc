// mathfmt.h - typeset an expression as 2-D math (fractions, exponents,
// radicals...). Plain C: drawing goes through callbacks, so the same code
// runs on the watch and in the PC simulator.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "calc.h"

#define MATH_MAX_NODES 220
#define MATH_LEVELS    4      // font sizes, 0 = largest

enum {
  MN_BOX = 0,   // placeholder for a missing operand
  MN_TEXT,      // literal text (name, symbol)
  MN_NUM,       // digits from the token buffer
  MN_PI,        // pi drawn with lines
  MN_BIN,       // a op b, spaced
  MN_PREFIX,    // op a  (unary minus)
  MN_CAT,       // a b   (function name + args, postfix)
  MN_IMPL,      // a b   implicit product (dot before a leading number)
  MN_FRAC,      // a over b
  MN_POW,       // a raised b
  MN_SQRT,
  MN_CBRT,
  MN_ABS,
  MN_PAREN,
  MN_SYS,       // a above b with a left brace (system of equations)
  MN_INT,       // integral sign, limits b = MN_LIMS(lo, hi), integrand a, then "dx"
  MN_LIMS,      // the two limits of an integral (laid out by MN_INT)
  MN_AT,        // a, then a bar with subscript b (evaluated at)
  MN_STACK,     // a above b, centred (separate lines)
  MN_CURSOR,    // the editing cursor: a thin bar
  MN_SUB,       // a with b lowered (log base)
  MN_NROOT,     // root of a with the index b
  MN_BIGOP,     // sum / product sign (text "S" / "P"), limits b = MN_LIMS(lo, hi), then a
};

typedef struct {
  uint8_t type, lvl, a, b;
  uint8_t start, len;        // MN_NUM: range in the token buffer
  uint8_t depth;             // tree depth below this node
  const char *text;          // MN_TEXT text / MN_BIN, MN_PREFIX operator
  int16_t w, asc, desc;      // layout result
} MNode;

typedef struct {
  MNode n[MATH_MAX_NODES];
  int count;
  uint8_t root;
  const uint8_t *tok;
  int ntok, i;
  char flat[CALC_MAX_TOK * 6 + 4];   // one-line fallback for formulas too deep to typeset
} MathDoc;

enum { MATH_INK_NORMAL, MATH_INK_DIM, MATH_INK_CURSOR };

typedef struct {
  int (*text_w)(void *ctx, const char *t, int lvl);
  void (*text)(void *ctx, int x, int baseline, const char *t, int lvl);
  void (*line)(void *ctx, int x0, int y0, int x1, int y1, int width);
  void (*box)(void *ctx, int x, int y, int w, int h);   // placeholder outline
  void (*ink)(void *ctx, int which);                  // MATH_INK_*: colour for what's drawn next (may be NULL)
  void *ctx;
  int asc[MATH_LEVELS], desc[MATH_LEVELS];             // cap height / descent
} MathGfx;

#ifndef SIM
// The typesetter's scratch doc on the watch: allocated with the fmt overlay
// and freed with it, so it makes way along with it for a big overlay.
#define MATH_SCRATCH ((MathDoc *)8)
#endif

// Builds the tree. b must stay alive while the doc is used.
void math_build(MathDoc *d, const TokBuf *b);
// Same from a raw token array; T_SEP splits a system into two lines.
void math_build_raw(MathDoc *d, const uint8_t *t, int n);
// Wrap the built expression as an integral from lo to hi ("" for none), or
// as its derivative at a point ("x=2"; NULL for just d/dx). The strings must
// stay alive while drawn.
void math_wrap_integral(MathDoc *d, const char *lo, const char *hi);
void math_wrap_derivative(MathDoc *d, const char *at);
// Lays out from base level lvl; returns the root's size.
void math_measure(MathDoc *d, const MathGfx *g, int lvl, int *w, int *asc, int *desc);
// Draws the last measured layout with its baseline-left corner at (x, baseline).
void math_draw(const MathDoc *d, const MathGfx *g, int x, int baseline);
