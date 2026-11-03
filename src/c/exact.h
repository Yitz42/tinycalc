// exact.h - exact answers: spots equations that are really polynomials (up to
// degree 4, rational coefficients) or linear systems, simplifies them, and
// gives roots in exact form, e.g. x=(1+sqrt(5))/2. Also recognises a plain
// number as p/q, a surd or a multiple of pi. Results are token strings for the
// typesetter. Plain C, tested on a PC.
#pragma once
#include "calc.h"

#define EXACT_MAX 4

typedef struct {
  TokBuf simp;               // simplified equation, e.g. x^2-x-1=0 (n = 0: none)
  uint8_t n;                 // exact solutions found
  TokBuf sol[EXACT_MAX];     // "x=(1+sqrt(5))/2", or "x=7/3, y=2/3"
  double x[EXACT_MAX], y[EXACT_MAX];
  bool real[EXACT_MAX];      // false: complex root (x is its real part)
} Exact;

// (With EXACT_FORMS 0 in config.h these aren't built; the app doesn't call them.)
// One equation in x. False if it isn't a polynomial we can solve exactly.
bool exact_solve(const TokBuf *f, const CalcVars *v, Exact *e);
// Two equations in x and y: only when both are linear with one solution.
bool exact_solve2(const TokBuf *e1, const TokBuf *e2, const CalcVars *v, Exact *e);
// v as p/q, k*sqrt(m)/q or p*pi/q. False if none fits (or v is an integer).
bool exact_value(double v, TokBuf *out);
