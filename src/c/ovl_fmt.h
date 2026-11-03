// ovl_fmt.h - the "fmt" overlay (src/ovl/mathfmt.c, the typesetter). Unlike
// the others it's loaded when the app starts and kept for as long as it runs.
#pragma once
#include "mathfmt.h"

enum { OF_BUILD = 1, OF_WRAP_INT, OF_WRAP_DER, OF_MEASURE, OF_DRAW };

typedef struct {
  MathDoc *d;
  const uint8_t *t;
  int n;
  const char *s1, *s2;          // limits / point
  const MathGfx *g;
  int lvl, x, y;
  int w, asc, desc;             // OF_MEASURE results
} OfArgs;

bool fmt_overlay_load(void);    // at start; false if it can't be loaded
