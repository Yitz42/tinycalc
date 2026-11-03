// ovl_math.h - the commands of the "math" overlay (src/ovl/ovl_math.c):
// exact answers, two-equation systems, integrals and derivatives.
#pragma once
#include <stdbool.h>
#include "calc.h"
#include "exact.h"

enum { OM_SOLVE2 = 1, OM_INTEGRATE, OM_DERIVATIVE, OM_EXACT, OM_EXACT2, OM_EXACT_VALUE, OM_ROOTS,
       OM_SHEET_FORMULA, OM_SHEET_SOLVER, OM_SHEET_FIND,   // sheet.c: io = SheetIO *, n = sys
       OM_SHEET_EXACT,                                     //   p = the TokBuf for the exact form
       OM_SOLVE3 };

typedef struct { const TokBuf *e[3]; const CalcVars *v; double *xyz; int ret; } OmSolve3;

typedef struct { void *io; int n, ret; void *p; int i; } OmSheet;

typedef struct {
  const TokBuf *e1, *e2;
  const CalcVars *v;
  double x0, x1, y0, y1;
  double *xs, *ys;
  int max, ret;
} OmSolve2;

typedef struct {
  const TokBuf *b;
  const CalcVars *v;
  double lo, hi;
  double *out;
  int max, ret;
} OmRoots;

typedef struct {
  const TokBuf *f;
  const CalcVars *v;
  double a, b;                // integral: from a to b; derivative: at a
  double *out;
  bool *approx;
  int ret;                    // CalcErr
} OmCalculus;

typedef struct {
  const TokBuf *e1, *e2;      // e2: second equation (OM_EXACT2)
  const CalcVars *v;
  Exact *x;
  double value;               // OM_EXACT_VALUE
  TokBuf *out;                // OM_EXACT_VALUE
  int ret;                    // bool
} OmExact;
