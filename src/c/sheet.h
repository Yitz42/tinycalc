// sheet.h - the logic behind the formula sheet and the solver sheet: what to
// typeset, how big, and the solver's search for answers. It lives in the
// "math" overlay on the watch (src/ovl/sheet.c), so it only takes heap while a
// sheet is in use; main.c keeps the drawing and the touch handling. The PC
// simulator links sheet.c directly.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "calc.h"
#include "exact.h"
#include "mathfmt.h"

#define MAX_ROOTS 20
#define FA_W 188          // formula sheet: area for the formula
#define FA_H 148
#define SOL_EQ_W 192      // solver sheet: width for the equation

// Everything the solver sheet shows (main.c refers to these as s_sys, s_roots, ...).
typedef struct {
  bool sys;                                 // solving two equations
  bool has_exact;                           // the equation is one we simplify / solve exactly
  bool has_x, has_eq;                       // what the expression holds
  bool fit;                                 // re-fit the graph's y range around the roots once found
  double gx0, gx1, gy0, gy1;                // search box (and graph view)
  double roots[MAX_ROOTS], root_y[MAX_ROOTS];   // x (and y for systems; the curve's y with the graph)
  int8_t rx[MAX_ROOTS];                     // the exact form a row shows, or -1
  bool rcplx[MAX_ROOTS];                    // a complex root: shown, can't be used
  int nroots, sel, list_top, expand;
  int sw, sasc, sdesc, slvl;                // the solver's equation: size and typeset level
  int fw, fasc, fdesc;                      // the formula sheet's formula: size
} SolState;

typedef struct {
  SolState *s;
  Calc *calc;
  MathDoc *doc;
  Exact *exact;
  TokBuf *doc_tok;                          // what the doc was built from (a copy)
  uint8_t *sys_tok;                         // room for "eq1 ; eq2" or a formula plus its exact form
  int sys_cap;
  char lo[20], hi[20], at[28];              // integral limits / "x=a" (the doc points at them)
  // What to typeset, for the app to do after the call (on the watch the
  // typesetter and the math overlay don't both fit in memory): bn < 0 is
  // nothing new; wrap 1 an integral from lo to hi, 2 d/dx, 3 d/dx at "at".
  const uint8_t *bt;
  int bn;
  uint8_t wrap;
  // solver layout
  bool graph;                               // the solver shows its graph
  int rows;                                 // answer rows visible
  double aspect;                            // graph: height / width in pixels
} SheetIO;

void sh_formula(SheetIO *io);               // typeset the formula sheet and size it
void sh_solver(SheetIO *io, bool sys);      // typeset the solver's equation(s), exact answers
int sh_find(SheetIO *io);                   // search; 1: nothing found, widen the box and call again
bool sh_exact(SheetIO *io, TokBuf *out);    // the answer's exact form (for S<>D), if it has one
