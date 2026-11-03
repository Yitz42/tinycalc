// ovl_cas.h - the commands of the "cas" overlay (src/ovl/ovl_cas.c).
#pragma once
#include "calc.h"

enum { OC_DER = 1, OC_INT, OC_FACTOR, OC_EXPAND };

typedef struct {
  const TokBuf *f;
  const CalcVars *v;
  TokBuf *out;
  int ret;          // bool
} OcArgs;
