// tools.h - the lists on the TOOLS keypad: memories, statistics, units,
// constants and number bases. Each is a list of rows (a label and a value);
// picking a row acts on the calculator. Lives in the "math" overlay on the
// watch (src/ovl/tools.c); the app only draws the rows.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "calc.h"

enum { TL_MEM = 0, TL_STAT, TL_UNITS, TL_CONST, TL_BASE, TL_CPLX, TL_COUNT };

#define STAT_N 40                     // data points kept

typedef struct {
  uint8_t n;                          // points
  bool pairs;                         // every point has a y
  double x[STAT_N], y[STAT_N];
} StatData;

enum { TA_NONE = 0, TA_CLOSE, TA_ALT, TA_KEEP, TA_SUB };   // what the app does after a pick (TA_SUB: a group opened)

typedef struct {
  Calc *calc;
  StatData *st;
  int cat;                            // units / constants: the group shown, or -1 for the list of groups
  char title[24];                     // its name
  char label[28], value[28];          // tl_row: one row
  char alt[48];                       // TA_ALT: show this instead of the answer
  uint8_t keys[32];                   // keys for the app to press after a pick (typing a constant...)
  int nkeys;
  int ret;
} ToolsIO;

int  tl_count(ToolsIO *io, int list);
void tl_row(ToolsIO *io, int list, int i);          // fills label and value
int  tl_pick(ToolsIO *io, int list, int i);         // TA_*
