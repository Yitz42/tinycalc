// calc.h - calculator core: tokens, algebraic parser, f(x)=0 solver, number
// formatting and the key-press state machine. Plain C, no pebble.h, so it
// can be tested on a PC (see test/).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define CALC_MAX_TOK 96

// Tokens. Digits and simple operators use their ASCII codes.
//   '0'..'9' '.' '+' '-' '*' '/' '^' '(' ')' '='
enum {
  T_EE = 128,   // x10^ exponent entry
  T_PI, T_E, T_X, T_ANS,
  T_SQ, T_CUBE, T_RECIP, T_PCT, T_FACT,           // postfix
  T_SIN, T_COS, T_TAN, T_ASIN, T_ACOS, T_ATAN,     // functions (include "(")
  T_LN, T_LOG, T_EXP, T_TENX, T_SQRT, T_CBRT, T_ABS,
  T_Y,          // second variable
  T_SEP,        // separates two equations (display only)
  T_NL,         // next line, stacked (display only)
  T_COMMA,      // ", " between items on a line (display only)
  T_I,          // the imaginary unit i
  T_CUR,        // the editing cursor (display only)
  T_SINH, T_COSH, T_TANH, T_ASINH, T_ACOSH, T_ATANH,   // functions (include "(")
  T_ROUND, T_FLOOR,
  T_GCD, T_LCM,                 // two arguments: gcd(a, b) (',' separates them)
  T_NCR, T_NPR, T_MOD,          // infix, like *: 5 nCr 2, 7 mod 3
  T_LOGB, T_ROOT,               // two arguments: logb(base, x), root(n, x)
  T_SUM, T_PROD,                // sum(f(x), a, b): f at x = a, a+1, ..., b added (multiplied)
  T_MA, T_MB, T_MC, T_MD, T_ME, T_MF,   // memories A..F
  T_AND, T_OR, T_XOR, T_SHL, T_SHR,     // two arguments, whole numbers: and(a, b), shl(a, n)
  T_NOT,                                // not(a) = -a - 1
  T_CONJ, T_RE, T_IM, T_ARG,            // complex numbers: conj(z), re(z), im(z), arg(z)
  T_Z,                                  // third variable (three equations)
  T_RAN,                                // Ran#: a random number, 0 <= r < 1 (a value, like pi)
  T_RANINT,                             // two arguments: RanInt(a, b), a random whole number a..b
  T_NPDF, T_NCDF, T_NINV,               // standard normal: density, P(Z < x), the x with P(Z < x) = p
  T_LAST
};

// Commands (keys that aren't tokens).
enum {
  K_EVAL = 200, K_AC, K_DEL, K_INV, K_DEG, K_SOLVE, K_STOX, K_RCLX,
  K_INT,        // integral of the expression in x: asks "from", "to"
  K_DER,        // derivative of the expression in x: asks "at x"
  K_FACTOR,     // factor the expression (a polynomial in x)
  K_EXPAND,     // expand and simplify the expression
  K_CLRXY,      // set x and y back to 0
  K_POL,        // (x, y) -> r, angle: from "a, b" typed, else from x and y; stores r in x, angle in y
  K_REC,        // (r, angle) -> x, y: likewise
};
#define CALC_MEMS 6
extern uint32_t calc_rng;   // Ran#'s state (the app seeds it from the clock)
// K_STOX / K_RCLX ("STO" / "RCL") ask which variable: the next key, x or y, picks it.

typedef enum {
  CALC_OK = 0,
  CALC_ERR_SYNTAX,
  CALC_ERR_MATH,
  CALC_ERR_EQ,      // '=' used outside SOLVE
  CALC_ERR_NOX,     // SOLVE with no x
  CALC_ERR_NOROOT,
  CALC_ERR_NOSOL,   // system with no solution found
  CALC_ERR_SYM,     // no formula found (symbolic integral, factor, ...)
  CALC_ERR_LIN3,    // three equations that aren't all linear
  CALC_ERR_INT,     // an integral that doesn't settle (sin x over a huge range)
  CALC_ERR_GRAPHZ,  // graphing something with z in it (a graph has x and y only)
} CalcErr;

typedef enum { MODE_EDIT = 0, MODE_RESULT, MODE_ERROR,
               MODE_EQ2,  /* typing the second equation */
               MODE_LIM   /* typing a limit (integral) or a point (derivative) */ } CalcMode;

enum { OP_NONE = 0, OP_INT, OP_DER };
// A result that's a formula (in expr; the input is in eq1).
enum { SYM_NONE = 0, SYM_DER, SYM_INT, SYM_FACTOR, SYM_EXPAND };

typedef struct {
  uint8_t t[CALC_MAX_TOK];
  uint8_t n;
} TokBuf;

typedef struct {
  double x, ans;
  bool deg;
  double y;
  double m[CALC_MEMS];        // memories A..F
  double ans_im;              // Ans can be complex: its imaginary part
  double z;                   // third variable
  uint32_t seed;              // Ran#: set for each answer
} CalcVars;

typedef struct {
  TokBuf expr;
  CalcVars v;
  double result;
  uint8_t mode;     // CalcMode
  uint8_t err;      // CalcErr
  bool inv;         // shift for the function page
  bool solved;      // last result came from SOLVE
  bool stored;      // last result was stored to x
  bool sys;         // last result solved a system (eq1 and expr)
  TokBuf eq1;       // first equation of a system, or f(x) for an integral / derivative
  TokBuf eq2;       // second equation of three (x, y, z); then expr is the third
  uint8_t op;       // OP_INT / OP_DER while asking for limits, and on its result
  uint8_t step;     // MODE_LIM: 0 = "from", 1 = "to", 2 = "at x"
  bool approx;      // integral didn't reach full accuracy
  double lim_a, lim_b;
  uint8_t cur;      // cursor in expr (token index), or CALC_CUR_END when typing at the end
  uint8_t sym;      // SYM_*: the result is a formula (expr), from eq1
  uint8_t pend;     // STO / RCL pressed, waiting for x or y: K_STOX / K_RCLX, else 0
  uint8_t var;      // what STO / RCL used: 0 x, 1 y, 2 both (cleared), 3 + i memory i
  double result_im; // the answer is complex: its imaginary part (else 0)
  char label[16];   // a result from the lists: what it is ("mean =", "in > cm =")
  bool sys3;        // the result solved three equations: x, y, z
  uint8_t conv;     // the result is a conversion: CONV_POL (r, angle) or CONV_REC (x, y) in x and y
} Calc;
enum { CONV_NONE = 0, CONV_POL, CONV_REC };

// How answers are shown: NORM (10 digits, plain or e-notation as fits),
// SCI / ENG (that many significant digits, e-notation; ENG: exponent a
// multiple of 3) or FIX (that many decimals).
enum { FMT_NORM = 0, FMT_SCI, FMT_ENG, FMT_FIX };
extern uint8_t calc_fmt, calc_fmt_digits;
void calc_format_answer(double v, char *buf, int len);   // in the current mode
// a + bi in the current number format ("3+4i", "-2i", "1.5")
void calc_format_cplx(double re, double im, char *buf, int len);
// Complex evaluation (src/ovl/cplx.c, its own overlay on the watch): for
// anything with i, and for what has no real answer (sqrt(-4) = 2i).
CalcErr calc_ceval(const TokBuf *b, const CalcVars *v, double *re, double *im);
// A number in the tokens at *i (digits, point, E exponent); advances *i.
bool calc_read_number(const uint8_t *t, int n, int *i, double *out);
// Show v as the answer (Ans too), with label on the small line.
void calc_take(Calc *c, double v, const char *label);

#define CALC_CUR_END 0xFF

// Parsing / evaluation
CalcErr calc_eval(const TokBuf *b, const CalcVars *v, double *out);
bool    calc_has_x(const TokBuf *b);
bool    calc_hidden_cursor(const TokBuf *b, int pos);   // a spot the typeset view doesn't show
bool    calc_has_eq(const TokBuf *b);
// An equation with something on both sides (ready to solve).
bool    calc_eq_ready(const TokBuf *b);
// Function token f applied to a (and b for the two-argument ones and the
// infix operators). Shared with the symbolic code (cas.c).
double  calc_apply(int f, double a, double b, bool deg);
bool    calc_is_func(int k);
// Both sides of the equation (rhs = 0 without '='); used by calcx.c too.
CalcErr calc_eval_eq(const TokBuf *b, const CalcVars *v, double *lhs, double *rhs);
double  calc_round_sig(double x, int sig);
// Both sides of "lhs = rhs" (rhs = 0 without '=') at x; false where undefined.
bool    calc_sides(const TokBuf *b, const CalcVars *v, double x, double *lhs, double *rhs);
// Every root found in [lo, hi], sorted; returns how many (at most max).
int     calc_roots(const TokBuf *b, const CalcVars *v, double lo, double hi, double *out, int max);
// Use r as the answer: stores it in x and Ans and shows "x = r".
void    calc_take_root(Calc *c, double r);
bool    calc_has_y(const TokBuf *b);
bool    calc_has_z(const TokBuf *b);
extern bool calc_refused;   // the last key was refused (an "=" inside brackets): the app buzzes
// Three linear equations in x, y, z (calcx.c): the one solution, CALC_ERR_NOSOL
// (none, or not just one) or CALC_ERR_LIN3 (not all linear).
CalcErr calc_solve3(const TokBuf *e1, const TokBuf *e2, const TokBuf *e3, const CalcVars *v, double xyz[3]);
// Solutions (x, y) of the system e1, e2 inside the box [x0,x1] x [y0,y1],
// sorted by x; returns how many (at most max).
int     calc_solve2(const TokBuf *e1, const TokBuf *e2, const CalcVars *v,
                    double x0, double x1, double y0, double y1, double *xs, double *ys, int max);
// Use a system solution: x and y stored, Ans = x.
void    calc_take_sol2(Calc *c, double x, double y);
// Integral of f(x) from a to b (adaptive Gauss-Kronrod). *approx set when the
// error estimate stayed above the target.
CalcErr calc_integrate(const TokBuf *f, const CalcVars *v, double a, double b, double *out, bool *approx);
// f'(x0) (Ridders' extrapolated central differences).
CalcErr calc_derivative(const TokBuf *f, const CalcVars *v, double x0, double *out);
// the same, also saying whether it is approximate (rounded to fewer digits: ~)
CalcErr calc_derivative2(const TokBuf *f, const CalcVars *v, double x0, double *out, bool *approx);

// Formatting
void calc_format(double v, int sig, char *buf, int len);
const char *calc_tok_text(uint8_t tok);
void calc_render(const TokBuf *b, char *buf, int len);
// Same, with a "|" at token position cur (CALC_CUR_END: none).
void calc_render_cur(const TokBuf *b, int cur, char *buf, int len);
// Put the cursor at token position pos in the expression being edited
// (clamped). Only in the editing modes; returns false otherwise.
bool calc_set_cursor(Calc *c, int pos);
const char *calc_err_text(CalcErr e);

// State machine
void calc_init(Calc *c);
void calc_key(Calc *c, int key);
// Text for the two display lines. small/big are written as UTF-8.
void calc_display(const Calc *c, char *small, int slen, char *big, int blen);
