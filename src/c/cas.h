// cas.h - symbolic math on the calculator's token strings: derivative and
// antiderivative as formulas, factor, expand. Results are token strings the
// typesetter and the parser both understand. On the watch this runs in the
// "cas" overlay (src/ovl/cas.c); PC tests link it directly.
#pragma once
#include "calc.h"

// d/dx of f (an expression in x; y is a constant). False if f isn't
// differentiable symbolically or the result doesn't fit.
bool cas_derivative(const TokBuf *f, const CalcVars *v, TokBuf *out);
// An antiderivative of f (no "+ C"). False if none is found.
bool cas_integral(const TokBuf *f, const CalcVars *v, TokBuf *out);
// f factored over the rationals (a polynomial in x; "p = q" gives "... = 0").
bool cas_factor(const TokBuf *f, const CalcVars *v, TokBuf *out);
// f with products and powers multiplied out and like terms collected.
bool cas_expand(const TokBuf *f, const CalcVars *v, TokBuf *out);
