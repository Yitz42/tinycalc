// ovl_graph.h - the "graph" overlay (src/ovl/graph.c): samples and draws
// graphs for the graph sheet and the solver sheet. It draws through GfxApi,
// small wrappers the app passes in around the watch's graphics calls.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "calc.h"

typedef struct {
  void *ctx;                                                      // the GContext
  void (*fill)(void *ctx, int x, int y, int w, int h, uint8_t argb);
  void (*line)(void *ctx, int x0, int y0, int x1, int y1, uint8_t argb, int width);
  void (*circle)(void *ctx, int x, int y, int r, uint8_t argb, bool filled);
  void (*text)(void *ctx, const char *t, int x, int y, int w, bool big, uint8_t argb, int align);  // align 0 left, 1 centre, 2 right
} GfxApi;

// What's graphed.
enum {
  GK_FX = 0,      // y = f(x): the tokens are f (no '=')
  GK_SIDES,       // an equation in x: both sides as two curves (they meet at the roots)
  GK_IMPLICIT,    // an equation in x and y: the curve where both sides are equal
};

#define GR_COLS 200        // most columns a graph can have
#define GR_CELL 5          // implicit curves: grid spacing in pixels

typedef struct {
  int x, y, w, h;                   // plot area on screen
  uint8_t kind;                     // GK_*
  bool labels;                      // grid lines with numbers (graph sheet); the solver panel has none
  double x0, x1, y0, y1;            // the view
  float *ca, *cb;                   // per column: f(x) (or the two sides); the app allocates them
  float *grid;                      // implicit: F(x, y) at grid points (the overlay allocates it)
  int gw, gh;                       // grid points across / down
  double gx0, gy1, gdx, gdy;        // where the grid was sampled (drawn moved/scaled while panning)
  bool trace;                       // a point marked on the curve
  double tx, ty;
  uint8_t tcurve;                   // 0: f / left side, 1: right side
  const double *rx, *ry;            // roots to mark (solver)
  int nroots, sel;
  bool roots_stale;
} GraphState;

// GR_SETUP: what to graph from src (into out, the tokens graphed, and g->kind); 0 on a syntax error.
// GR_VIEW: fit the view (n: reset to +-10 first). GR_ZOOM: by d around the centre or the
// traced point. GR_TRACE_AT: mark the point at screen (sx, sy). GR_STEP: move the point by n
// steps (implicit curves: pan up/down).
enum { GR_SAMPLE = 1, GR_FIT, GR_DRAW, GR_TRACE, GR_FREE, GR_SETUP, GR_VIEW, GR_ZOOM, GR_TRACE_AT, GR_STEP };

typedef struct {
  GraphState *g;
  const TokBuf *f;
  const CalcVars *v;
  const GfxApi *gfx;
  double d;
  int n, sx, sy;
  const TokBuf *src;
  TokBuf *out;
} GrArgs;

// App side (ovl_calls.c on the watch; graph.c directly in the simulator).
bool graph_call(int cmd, void *args);
