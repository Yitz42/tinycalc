// main.c - TinySci: touch keypad UI for the Pebble Time 2 (emery, 200x228).
// All math and key logic lives in calc.c (plain C, tested on a PC).
#include <pebble.h>
#include "calc.h"
#include "mathfmt.h"
#include "mmath.h"
#include "config.h"
#include "exact.h"
#include "ovl_graph.h"
#include "ovl_fmt.h"
#include "sheet.h"
#include "tools.h"

#define SCR_W   200
#define PANEL_H 62
#define KEY_Y   64
#define ROW_H   41
#define COL_W   50
#define N_PAGES 5
// Pages 0 and 1 (numbers, functions) swipe left/right. Below them: the solver
// keys (page 2, swipe up from 123) and more functions (page 3, swipe up from
// f(x)); those swipe away back to the page above them.
#define N_KEYPADS 2


#define PERSIST_VER_KEY  1
#define PERSIST_PAGE_KEY 3
#define PERSIST_CALC_KEY 10   // Calc is split over keys 10, 11, ... (256 bytes each)
#define PERSIST_VERSION  9
#define PERSIST_SET_KEY  4    // settings
#define PERSIST_HIST_KEY 30   // history count, then entries in keys 31, 32, ...
#define PERSIST_STAT_KEY 50   // statistics data, keys 50, 51, ...

// Label glyph markers drawn with lines instead of font glyphs.
#define G_PI   "\x01"
#define G_SQRT "\x02"
#define G_CBRT "\x03"
#define G_INT  "\x04"
#define G_DIV  "\x05"
#define G_MUL  "\x06"
#define G_SQ   "\x07"
#define G_CUBE "\x08"
#define G_PLUS "\x09"
#define G_MINUS "\x0a"
#define G_EQ   "\x0b"
#define G_NROOT "\x0c"
#define G_SUM  "\x0d"
#define G_PROD "\x0e"

// Keys handled here rather than in calc.c
#define K_HIST 250    // answer history
#define K_GRAPH 252   // graph what's typed
// The TOOLS keypad (swipe up from MORE)
#define K_SD    230   // answer: exact form <-> decimals
#define K_FMT   231   // number format: NORM, SCI, ENG, FIX
#define K_MEM   233   // the lists (tools.c): memories A..F
#define K_STAT  234   //   statistics
#define K_UNITS 235   //   unit conversions
#define K_CONST 236   //   physical constants
#define K_BASE  237   //   hex / binary / octal and bit operations

enum { ST_NUM = 0, ST_OP, ST_EQ, ST_FN, ST_TOG, ST_SOL, ST_SOLVE };

typedef struct {
  uint8_t code, inv_code, style, span;   // codes fit a byte: tokens < 160, commands 200+
  const char *label, *inv_label;
} KeyDef;

#define K(c, l, s)                { (c), 0, (s), 1, (l), NULL }
#define KI(c, l, ic, il, s)       { (c), (ic), (s), 1, (l), (il) }
#define KS(c, l, s, n)            { (c), 0, (s), (n), (l), NULL }
#define KNONE                     { 0, 0, 0, 0, NULL, NULL }

static const KeyDef PAGES[N_PAGES][16] = {
  { // 0: numbers
    K('7', "7", ST_NUM), K('8', "8", ST_NUM), K('9', "9", ST_NUM), K('/', G_DIV, ST_OP),
    K('4', "4", ST_NUM), K('5', "5", ST_NUM), K('6', "6", ST_NUM), K('*', G_MUL, ST_OP),
    K('1', "1", ST_NUM), K('2', "2", ST_NUM), K('3', "3", ST_NUM), K('-', G_MINUS, ST_OP),
    K('0', "0", ST_NUM), K('.', ".", ST_NUM), K(K_EVAL, G_EQ, ST_EQ), K('+', G_PLUS, ST_OP),
  },
  { // 1: functions
    K(K_INV, "INV", ST_TOG), K(K_DEG, "DEG", ST_TOG), K('(', "(", ST_FN), K(')', ")", ST_FN),
    KI(T_SIN, "sin", T_ASIN, "asin", ST_FN), KI(T_COS, "cos", T_ACOS, "acos", ST_FN),
    KI(T_TAN, "tan", T_ATAN, "atan", ST_FN), K('^', "x^y", ST_FN),
    KI(T_LN, "ln", T_EXP, "e^x", ST_FN), KI(T_LOG, "log", T_TENX, "10^x", ST_FN),
    KI(T_SQRT, G_SQRT, T_CBRT, G_CBRT, ST_FN), KI(T_SQ, G_SQ, T_CUBE, G_CUBE, ST_FN),
    K(T_PI, G_PI, ST_FN), K(T_E, "e", ST_FN), K(T_EE, "EE", ST_FN), K(T_ANS, "Ans", ST_FN),
  },
  { // 2: solver (the = sign comes from the main = key; x^2, sqrt, ( ) are on f(x))
    K(T_X, "x", ST_SOL), K(T_Y, "y", ST_SOL), K(T_Z, "z", ST_SOL), K(K_CLRXY, "Clr xyz", ST_SOL),
    KS(K_INT, G_INT, ST_SOL, 2), KNONE, KS(K_DER, "d/dx", ST_SOL, 2), KNONE,
    K(K_FACTOR, "Factor", ST_SOL), K(K_EXPAND, "Expand", ST_SOL), KS(K_GRAPH, "Graph", ST_SOL, 2), KNONE,
    K(K_STOX, "STO", ST_SOL), K(K_RCLX, "RCL", ST_SOL), KS(K_HIST, "History", ST_SOL, 2), KNONE,
  },
  { // 3: more functions (swipe up from f(x))
    K(K_INV, "INV", ST_TOG), KI(T_SINH, "sinh", T_ASINH, "asinh", ST_FN),
    KI(T_COSH, "cosh", T_ACOSH, "acosh", ST_FN), KI(T_TANH, "tanh", T_ATANH, "atanh", ST_FN),
    K(T_NCR, "nCr", ST_FN), K(T_NPR, "nPr", ST_FN), K(T_MOD, "mod", ST_FN), K(',', ",", ST_FN),
    K(T_GCD, "gcd", ST_FN), K(T_LCM, "lcm", ST_FN), K(T_ROUND, "round", ST_FN), K(T_FLOOR, "floor", ST_FN),
    K(T_FACT, "n!", ST_FN), K(T_ABS, "|x|", ST_FN), K(T_PCT, "%", ST_FN), K(T_RECIP, "1/x", ST_FN),
  },
  { // 4: tools (swipe up from MORE)
    K(K_SD, "S<>D", ST_SOL), K(K_FMT, "MODE", ST_SOL), K(T_LOGB, "logb", ST_FN), K(T_ROOT, G_NROOT, ST_FN),
    K(T_SUM, G_SUM, ST_FN), K(T_PROD, G_PROD, ST_FN), K(',', ",", ST_FN), K(T_I, "i", ST_FN),
    K(K_POL, "Pol", ST_SOL), K(K_REC, "Rec", ST_SOL), K(K_MEM, "MEM", ST_SOL), K(K_STAT, "Stat", ST_SOL),
    K(K_UNITS, "Units", ST_SOL), K(K_CONST, "Const", ST_SOL), K(K_BASE, "Base", ST_SOL), K(T_X, "x", ST_FN),
  },
};

static Window *s_window;
static Layer *s_layer;
static Calc s_calc;
static int s_page;
static uint8_t s_alt;                // the answer shown another way (S<>D, DMS, Base): ALT_*
enum { ALT_NONE, ALT_EXACT, ALT_TEXT };
static TokBuf *s_alt_tok;            // ALT_EXACT: the exact form (heap)
static char s_alt_txt[48];           // ALT_TEXT
static int s_pressed = -1;       // key slot or HIT_* being touched
static bool s_touch;             // touch available
static int s_cursor;             // button-only mode: slot on current page
#define LINE_LEN 200
static char *s_small, *s_big;   // the two display lines as text (heap)

static GFont s_f14, s_f18, s_f18b, s_f24b, s_f28b;

// ---- settings (set on the phone, in the Pebble app; see src/pkjs/index.js)
typedef struct {
  uint8_t btn[6];    // action (A_*) for Up, Up held, Down, Down held, Select, Select held
  uint8_t graph;     // solver sheet graphs one-variable equations
  uint8_t buzz;      // 0 off, 1 light, 2 strong
  uint8_t light;     // 0 = watch default, 1 = light up on each press, 2 = on while the app is open
  uint8_t fmt;       // number format FMT_* (also the MODE key)
  uint8_t digits;    // digits for SCI / ENG / FIX
} Settings;
// What a button does on the calculator screen (the sheets and lists keep their own buttons).
enum { A_NEXT_PAGE, A_PREV_PAGE, A_EQ, A_AC, A_DEL, A_FORMULA, A_HISTORY, A_SOLVER_KEYS,
       A_KEY_PREV, A_KEY_NEXT, A_KEY_PRESS, A_ANS, A_NOTHING, A_COUNT };
enum { B_UP, B_UP_HOLD, B_DOWN, B_DOWN_HOLD, B_SELECT, B_SELECT_HOLD };
static Settings s_set = { { A_NEXT_PAGE, A_HISTORY, A_AC, A_FORMULA, A_EQ, A_AC }, 0, 1, 0, FMT_NORM, 4 };
static void save_settings(void);
enum { MENU_NONE, MENU_HIST, MENU_TOOLS };
static int s_menu;                  // a full-screen list open over the calculator
// A key highlight is shown when a button moves it (always with touch off).
static bool nav(void) {
  if (!s_touch) return true;
  for (int i = 0; i < 6; i++)
    if (s_set.btn[i] == A_KEY_PREV || s_set.btn[i] == A_KEY_NEXT || s_set.btn[i] == A_KEY_PRESS) return true;
  return false;
}

// ---- answer history: newest first, saved across launches
#define HIST_N 8
enum { H_CALC, H_ROOT, H_INT, H_DER, H_SYS };
#define H_APPROX 0x80                 // in kind: the answer was approximate (~)
#define HK(h) ((h)->kind & 0x7f)
typedef struct {
  TokBuf t;          // the expression (f(x) for an integral / derivative; "eq1 ; eq2" for a system)
  uint8_t kind;
  double r, a, b;    // the answer; limits / point (a = y for a system)
} Hist;
static Hist *s_hist;              // HIST_N entries, on the heap (app space is limited to 64 KB)
static int s_nhist;

// ------------------------------------------------------------------ colors

static void key_colors(const KeyDef *k, bool pressed, GColor *bg, GColor *fg) {
  if (pressed) { *bg = GColorWhite; *fg = GColorBlack; return; }
  switch (k->style) {
    case ST_NUM:   *bg = GColorDarkGray;        *fg = GColorWhite; break;
    case ST_OP:    *bg = GColorCobaltBlue;      *fg = GColorWhite; break;
    case ST_EQ:    *bg = GColorChromeYellow;    *fg = GColorBlack; break;
    case ST_FN:    *bg = GColorImperialPurple;  *fg = GColorWhite; break;
    case ST_SOL:   *bg = GColorJaegerGreen;     *fg = GColorWhite; break;
    case ST_SOLVE: *bg = GColorChromeYellow;    *fg = GColorBlack; break;
    default:       *bg = GColorLightGray;       *fg = GColorBlack; break;
  }
  if (k->code == K_INV && s_calc.inv) { *bg = GColorShockingPink; *fg = GColorBlack; }
  if (s_calc.pend && (k->code == T_X || k->code == T_Y || k->code == T_Z || k->code == K_MEM)) { *bg = GColorChromeYellow; *fg = GColorBlack; }  // STO / RCL: pick one
}

// ------------------------------------------------------------------ geometry

static GRect slot_rect(int slot, int span) {
  int r = slot / 4, c = slot % 4;
  return GRect(c * COL_W + 2, KEY_Y + r * ROW_H + 2, span * COL_W - 4, ROW_H - 4);
}

static int slot_owner(int page, int slot) {  // slot covered by a spanning key -> its first slot
  int row = slot / 4;
  for (int s = slot; s >= row * 4; s--) {
    const KeyDef *k = &PAGES[page][s];
    if (k->span > 0) return (s + k->span > slot) ? s : -1;
  }
  return -1;
}


static bool in_rect(GRect r, int x, int y) {
  return x >= r.origin.x && x < r.origin.x + r.size.w &&
         y >= r.origin.y && y < r.origin.y + r.size.h;
}

static int hit_test(int x, int y) {
  if (y < KEY_Y || x < 0 || x >= SCR_W) return -1;
  int r = (y - KEY_Y) / ROW_H, c = x / COL_W;
  if (r > 3 || c > 3) return -1;
  return slot_owner(s_page, r * 4 + c);
}

// ------------------------------------------------------------------ drawing

static int text_w(const char *t, GFont f) {
  return graphics_text_layout_get_content_size(t, f, GRect(0, 0, 1000, 60),
      GTextOverflowModeFill, GTextAlignmentLeft).w;
}

static int font_top(GFont f) {  // distance from a box's top to the glyph centre line
  if (f == s_f28b) return 19;
  if (f == s_f24b) return 16;
  if (f == s_f14) return 9;
  return 12;
}

static void draw_centered(GContext *ctx, const char *t, GFont f, GRect r) {
  int cy = r.origin.y + r.size.h / 2;
  graphics_draw_text(ctx, t, f, GRect(r.origin.x, cy - font_top(f), r.size.w, 40),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_glyph(GContext *ctx, const char *g, GRect r, GColor fg) {
  int cx = r.origin.x + r.size.w / 2, cy = r.origin.y + r.size.h / 2;
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, 3);
  // The system fonts are ASCII-only on the watch, so these are drawn.
  if (g[0] == G_DIV[0]) {
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_line(ctx, GPoint(cx - 7, cy), GPoint(cx + 7, cy));
    graphics_context_set_fill_color(ctx, fg);
    graphics_fill_circle(ctx, GPoint(cx, cy - 6), 2);
    graphics_fill_circle(ctx, GPoint(cx, cy + 6), 2);
    return;
  }
  if (g[0] == G_MUL[0]) {
    graphics_draw_line(ctx, GPoint(cx - 7, cy - 7), GPoint(cx + 7, cy + 7));
    graphics_draw_line(ctx, GPoint(cx - 7, cy + 7), GPoint(cx + 7, cy - 7));
    return;
  }
  if (g[0] == G_PLUS[0] || g[0] == G_MINUS[0]) {
    graphics_draw_line(ctx, GPoint(cx - 9, cy), GPoint(cx + 9, cy));
    if (g[0] == G_PLUS[0]) graphics_draw_line(ctx, GPoint(cx, cy - 9), GPoint(cx, cy + 9));
    return;
  }
  if (g[0] == G_EQ[0]) {
    graphics_draw_line(ctx, GPoint(cx - 9, cy - 4), GPoint(cx + 9, cy - 4));
    graphics_draw_line(ctx, GPoint(cx - 9, cy + 4), GPoint(cx + 9, cy + 4));
    return;
  }
  if (g[0] == G_SQ[0] || g[0] == G_CUBE[0]) {   // x with a raised 2 or 3
    graphics_context_set_text_color(ctx, fg);
    draw_centered(ctx, "x", s_f24b, GRect(cx - 14, cy - 10, 20, 20));
    draw_centered(ctx, g[0] == G_SQ[0] ? "2" : "3", s_f14, GRect(cx + 1, cy - 14, 12, 14));
    return;
  }
  if (g[0] == G_INT[0]) {                // integral sign, then "dx"
    int x0 = cx - 13;
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, GPoint(x0 + 7, cy - 10), GPoint(x0 + 5, cy - 11));
    graphics_draw_line(ctx, GPoint(x0 + 5, cy - 11), GPoint(x0 + 4, cy - 9));
    graphics_draw_line(ctx, GPoint(x0 + 4, cy - 9), GPoint(x0 + 2, cy + 9));
    graphics_draw_line(ctx, GPoint(x0 + 2, cy + 9), GPoint(x0, cy + 11));
    graphics_draw_line(ctx, GPoint(x0, cy + 11), GPoint(x0 - 2, cy + 9));
    graphics_context_set_text_color(ctx, fg);
    draw_centered(ctx, "dx", s_f18b, GRect(x0 + 8, cy - 8, 20, 16));
    return;
  }
  if (g[0] == G_PI[0]) {
    graphics_draw_line(ctx, GPoint(cx - 9, cy - 6), GPoint(cx + 9, cy - 6));
    graphics_draw_line(ctx, GPoint(cx - 4, cy - 5), GPoint(cx - 6, cy + 8));
    graphics_draw_line(ctx, GPoint(cx + 4, cy - 5), GPoint(cx + 4, cy + 6));
    graphics_draw_line(ctx, GPoint(cx + 4, cy + 7), GPoint(cx + 7, cy + 8));
    return;
  }
  if (g[0] == G_SUM[0] || g[0] == G_PROD[0]) {   // sigma / capital pi
    graphics_context_set_stroke_width(ctx, 2);
    int l = cx - 7, rr = cx + 7, t = cy - 9, b = cy + 9;
    graphics_draw_line(ctx, GPoint(l, t), GPoint(rr, t));
    if (g[0] == G_SUM[0]) {
      graphics_draw_line(ctx, GPoint(l, t), GPoint(cx, cy));
      graphics_draw_line(ctx, GPoint(cx, cy), GPoint(l, b));
      graphics_draw_line(ctx, GPoint(l, b), GPoint(rr, b));
    } else {
      graphics_draw_line(ctx, GPoint(l + 3, t), GPoint(l + 3, b));
      graphics_draw_line(ctx, GPoint(rr - 3, t), GPoint(rr - 3, b));
    }
    return;
  }
  // radical sign, optional small 3 (or n), then "x" under the bar
  graphics_context_set_stroke_width(ctx, 2);
  int x0 = cx - 13;
  graphics_draw_line(ctx, GPoint(x0 - 3, cy + 1), GPoint(x0, cy - 1));
  graphics_draw_line(ctx, GPoint(x0, cy - 1), GPoint(x0 + 4, cy + 8));
  graphics_draw_line(ctx, GPoint(x0 + 4, cy + 8), GPoint(x0 + 9, cy - 9));
  graphics_draw_line(ctx, GPoint(x0 + 9, cy - 9), GPoint(cx + 13, cy - 9));
  graphics_context_set_text_color(ctx, fg);
  draw_centered(ctx, "x", s_f18b, GRect(x0 + 9, cy - 6, cx + 13 - (x0 + 9), 14));
  if (g[0] == G_CBRT[0] || g[0] == G_NROOT[0])
    graphics_draw_text(ctx, g[0] == G_CBRT[0] ? "3" : "n", s_f14, GRect(x0 - 6, cy - 16, 10, 16),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_key_at(GContext *ctx, int page, int slot) {
  const KeyDef *k = &PAGES[page][slot];
  bool live = page == s_page;
  if (k->span == 0) return;
  GRect r = slot_rect(slot, k->span);
  bool pressed = live && ((s_pressed == slot) || (nav() && s_cursor == slot));
  GColor bg, fg;
  key_colors(k, live && s_pressed == slot, &bg, &fg);
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, r, 7, GCornersAll);
  if (pressed && nav() && s_pressed != slot) {   // button-mode cursor
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_round_rect(ctx, grect_inset(r, GEdgeInsets(1)), 7);
  }
  const char *label = (s_calc.inv && k->inv_label) ? k->inv_label : k->label;
  if (k->code == K_DEG) label = s_calc.v.deg ? "DEG" : "RAD";
  if ((unsigned char)label[0] < 16) { draw_glyph(ctx, label, r, fg); return; }
  graphics_context_set_text_color(ctx, fg);
  GFont f;
  if (k->style == ST_NUM || k->style == ST_OP || k->style == ST_EQ) f = s_f28b;
  else f = s_f24b;
  if (text_w(label, f) > r.size.w - 6) f = s_f18b;
  if (text_w(label, f) > r.size.w - 4) f = s_f14;
  draw_centered(ctx, label, f, r);
}

// Draw text right-aligned in r, shrinking the font, then keeping the tail.
// Draw text right-aligned in r, shrinking the font. If it still doesn't fit,
// cut it down with "..." on the cut side(s), keeping the cursor "|" in view
// (or the end of the text when there's no cursor). Text is ASCII.
static void draw_fit(GContext *ctx, const char *t, GRect r, GFont *fonts, int nf) {
  GFont f = fonts[nf - 1];
  for (int i = 0; i < nf; i++)
    if (text_w(t, fonts[i]) <= r.size.w) { f = fonts[i]; break; }
  if (text_w(t, f) <= r.size.w) {
    graphics_draw_text(ctx, t, f, r, GTextOverflowModeFill, GTextAlignmentRight, NULL);
    return;
  }
  static char buf[210];
  const char *bar = strchr(t, '|');
  int len = (int)strlen(t), keep = bar ? (int)(bar - t) : len, a = 0, b = len;
  for (;;) {
    int n = 0;
    if (a > 0) { memcpy(buf, "...", 3); n = 3; }
    memcpy(buf + n, t + a, b - a); n += b - a;
    if (b < len) { memcpy(buf + n, "...", 3); n += 3; }
    buf[n] = 0;
    if (text_w(buf, f) <= r.size.w || b - a <= 1) break;
    // trim the side that's further from the cursor
    if (keep - a > b - keep) a++; else b--;
  }
  graphics_draw_text(ctx, buf, f, r, GTextOverflowModeFill, GTextAlignmentRight, NULL);
}


// ---- the display: the expression typeset as math while you type, answer below

typedef struct { GContext *g; int ox, oy; GColor ink; } MathCtx;

// Cursor positions for dragging it in two directions: the display is laid
// out once per possible position with drawing switched off (s_rec), and
// where the cursor bar lands is noted.
static bool s_rec, s_rec_next, s_rec_got;
static int s_rec_x, s_rec_y;

// Graphs (graph sheet and the solver's panel) are drawn by the graph overlay
// through these.
static GraphState *s_gs;             // heap
static void gx_fill(void *c, int x, int y, int w, int h, uint8_t col) {
  graphics_context_set_fill_color(c, (GColor){ .argb = col });
  graphics_fill_rect(c, GRect(x, y, w, h), 0, GCornerNone);
}
static void gx_line(void *c, int x0, int y0, int x1, int y1, uint8_t col, int w) {
  graphics_context_set_stroke_color(c, (GColor){ .argb = col });
  graphics_context_set_stroke_width(c, (uint8_t)w);
  graphics_draw_line(c, GPoint(x0, y0), GPoint(x1, y1));
  graphics_context_set_stroke_width(c, 1);
}
static void gx_circle(void *c, int x, int y, int r, uint8_t col, bool filled) {
  if (filled) {
    graphics_context_set_fill_color(c, (GColor){ .argb = col });
    graphics_fill_circle(c, GPoint(x, y), (uint16_t)r);
  } else {
    graphics_context_set_stroke_color(c, (GColor){ .argb = col });
    graphics_context_set_stroke_width(c, 2);
    graphics_draw_circle(c, GPoint(x, y), (uint16_t)r);
    graphics_context_set_stroke_width(c, 1);
  }
}
static void gx_text(void *c, const char *t, int x, int y, int w, bool big, uint8_t col, int al) {
  graphics_context_set_text_color(c, (GColor){ .argb = col });
  graphics_draw_text(c, t, big ? s_f18b : s_f14, GRect(x, y, w, 20), GTextOverflowModeFill,
                     al == 0 ? GTextAlignmentLeft : al == 1 ? GTextAlignmentCenter : GTextAlignmentRight, NULL);
}
static GfxApi s_gfx = { NULL, gx_fill, gx_line, gx_circle, gx_text };
static MathGfx s_mg;
static MathDoc *s_doc2;              // typesets the display lines and solver rows (heap)
static uint8_t s_ptok[2 * CALC_MAX_TOK + 2];

enum { WRAP_NONE, WRAP_INT, WRAP_DER };

// Typeset tokens into r: the largest size that fits, right-aligned. Too wide
// even at the smallest size: the end stays in view and the start runs off the
// left. suffix (" =") goes after it as text. Returns false if it can't fit and
// the caller should fall back to plain text.
static bool math_fit(GContext *ctx, const uint8_t *t, int n, int wrap, const char *lo, const char *hi,
                     GRect r, int lvl0, const char *suffix, GColor ink, bool must_fit) {
  math_build_raw(s_doc2, t, n);
  if (wrap == WRAP_INT) math_wrap_integral(s_doc2, lo, hi);
  if (wrap == WRAP_DER) math_wrap_derivative(s_doc2, lo);
  int sw = suffix ? text_w(suffix, lvl0 <= 1 ? s_f24b : s_f18) + 2 : 0;
  int w, as, de, lvl;
  for (lvl = lvl0; lvl < MATH_LEVELS; lvl++) {
    math_measure(s_doc2, &s_mg, lvl, &w, &as, &de);
    if (w + sw <= r.size.w && as + de <= r.size.h) break;
  }
  if (lvl == MATH_LEVELS) {
    if (must_fit) return false;
    lvl = MATH_LEVELS - 1;
    math_measure(s_doc2, &s_mg, lvl, &w, &as, &de);
    if (as + de > r.size.h && wrap == WRAP_NONE && n < CALC_MAX_TOK) {
      // too tall even small (a fraction in a power): one line of text, so it
      // doesn't run into the line below; too long, its start makes way
      static char fl[120];
      TokBuf b;
      b.n = (uint8_t)n;
      for (int i = 0; i < n; i++) b.t[i] = t[i];
      calc_render(&b, fl, (int)sizeof fl - 2);
      if (suffix) { int k = (int)strlen(fl); fl[k++] = ' '; for (int i = 0; suffix[i] && i < 2; i++) fl[k++] = suffix[i]; fl[k] = 0; }
      GFont f = r.size.h >= 22 ? s_f18b : s_f14;
      const char *p = fl;
      while (*p && text_w(p, f) > r.size.w) p++;
      graphics_context_set_text_color(ctx, ink);
      graphics_draw_text(ctx, p, f, GRect(r.origin.x, r.origin.y + (r.size.h - (f == s_f14 ? 18 : 22)) / 2 - 2, r.size.w, 24),
                         GTextOverflowModeFill, GTextAlignmentRight, NULL);
      return true;
    }
  }
  int x = r.origin.x + r.size.w - sw - w;
  int base = as + de <= r.size.h ? r.origin.y + (r.size.h - as - de) / 2 + as : r.origin.y + as;   // too tall: keep the top
  MathCtx mc = { ctx, 0, 0, ink };
  s_mg.ctx = &mc;
  graphics_context_set_text_color(ctx, ink);
  graphics_context_set_stroke_color(ctx, ink);
  math_draw(s_doc2, &s_mg, x, base);
  graphics_context_set_stroke_width(ctx, 1);
  if (suffix) {
    GFont f = lvl0 <= 1 ? s_f24b : s_f18;
    graphics_draw_text(ctx, suffix, f, GRect(x + w + 2, base - (lvl0 <= 1 ? 24 : 18), sw + 4, 30),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
  return true;
}

static int tok_append(int n, const TokBuf *b, int cur) {   // copies b, with the cursor at cur
  for (int i = 0; i <= b->n && n < (int)sizeof s_ptok - 1; i++) {
    if (i == cur) s_ptok[n++] = T_CUR;
    if (i < b->n) s_ptok[n++] = b->t[i];
  }
  return n;
}

// The line being typed: it shrinks only to a readable size (18), then scrolls
// sideways instead, keeping the cursor (or the end) in view. A drag sideways
// on the display scrolls it too (s_xfollow off until the next key).
#define EDIT_MIN_LVL 2
static int s_xscroll, s_xmax;          // px shown left of the right-aligned spot; how far it can go
static bool s_xfollow = true;          // scroll to the cursor
static int clampi(int v, int lo, int hi);

typedef struct { int lvl, x, base, w; bool tall; } EditLay;

static EditLay edit_layout(GRect r, bool cursor) {
  EditLay L = { EDIT_MIN_LVL, 0, 0, 0, false };
  int w = 0, as = 0, de = 0, lvl;
  for (lvl = 0; lvl <= EDIT_MIN_LVL; lvl++) {       // largest that fits both ways
    math_measure(s_doc2, &s_mg, lvl, &w, &as, &de);
    if (w <= r.size.w && as + de <= r.size.h) break;
  }
  if (lvl > EDIT_MIN_LVL) {                         // too wide even at 18: scroll at 18
    for (lvl = EDIT_MIN_LVL; lvl < MATH_LEVELS - 1; lvl++) {   // (smaller only if it's too tall)
      math_measure(s_doc2, &s_mg, lvl, &w, &as, &de);
      if (as + de <= r.size.h) break;
    }
    math_measure(s_doc2, &s_mg, lvl, &w, &as, &de);
  }
  L.lvl = lvl; L.w = w; L.tall = as + de > r.size.h;
  L.base = as + de <= r.size.h ? r.origin.y + (r.size.h - as - de) / 2 + as : r.origin.y + as;
  s_xmax = w > r.size.w ? w - r.size.w : 0;
  if (s_xfollow) {
    if (!cursor) s_xscroll = 0;                     // typing at the end: the end shows
    else if (s_xmax) {                              // where the cursor falls, then just enough scroll
      MathCtx mc = { NULL, 0, 0, GColorBlack };
      s_mg.ctx = &mc;
      s_rec = true; s_rec_next = s_rec_got = false;
      math_draw(s_doc2, &s_mg, 0, 0);
      s_rec = false;
      if (s_rec_got) {
        int cx = r.size.w - w + s_xscroll + s_rec_x;  // the cursor, from the box's left
        if (cx < 6) s_xscroll += 6 - cx;
        if (cx > r.size.w - 6) s_xscroll -= cx - (r.size.w - 6);
      }
    }
  }
  s_xscroll = clampi(s_xscroll, 0, s_xmax);
  L.x = r.origin.x + r.size.w - w + s_xscroll;
  return L;
}

static void edit_draw(GContext *ctx, int n, GRect r, GColor ink, bool cursor) {
  math_build_raw(s_doc2, s_ptok, n);
  EditLay L = edit_layout(r, cursor);
  MathCtx mc = { ctx, 0, 0, ink };
  s_mg.ctx = &mc;
  graphics_context_set_text_color(ctx, ink);
  graphics_context_set_stroke_color(ctx, ink);
  math_draw(s_doc2, &s_mg, L.x, L.base);
  graphics_context_set_stroke_width(ctx, 1);
  if (L.tall) {                                     // too tall even small: the top shows, the rest is cut off
    graphics_context_set_fill_color(ctx, GColorPastelYellow);
    int y0 = r.origin.y + r.size.h;
    graphics_fill_rect(ctx, GRect(3, y0, SCR_W - 6, 55 - y0), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(0, PANEL_H, SCR_W, KEY_Y - PANEL_H), 0, GCornerNone);
  }
  if (s_xmax) {                                     // cover what runs past the box; arrows where there's more
    int y0 = r.origin.y < 9 ? 9 : r.origin.y, y1 = r.origin.y + r.size.h > 55 ? 55 : r.origin.y + r.size.h;
    graphics_context_set_fill_color(ctx, GColorBlack);          // outside the panel
    graphics_fill_rect(ctx, GRect(0, r.origin.y, 2, r.size.h), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(SCR_W - 2, r.origin.y, 2, r.size.h), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorPastelYellow);   // the panel's margins (clear of its round corners)
    graphics_fill_rect(ctx, GRect(2, y0, r.origin.x - 2, y1 - y0), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(r.origin.x + r.size.w, y0, SCR_W - 2 - r.origin.x - r.size.w, y1 - y0), 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    int my = r.origin.y + r.size.h / 2, ex = r.origin.x + r.size.w;
    for (int i = 0; i < 4; i++) {                   // small arrowheads: more that way
      if (s_xscroll < s_xmax) graphics_draw_line(ctx, GPoint(3 + i, my - i), GPoint(3 + i, my + i));
      if (s_xscroll > 0) graphics_draw_line(ctx, GPoint(ex + 4 - i, my - i), GPoint(ex + 4 - i, my + i));
    }
  }
}

#define MAX_CARETS 64                 // longer expressions drag left/right only
static int16_t *s_cx, *s_cy;          // screen position of the cursor at each token position (heap)
static int s_ncar = -1;               // positions known (expr.n + 1), or -1
static bool s_want_carets;

static void carets(GContext *ctx, GRect r) {
  s_want_carets = false;
  s_ncar = -1;
  int n = s_calc.expr.n;
  if (n + 1 > MAX_CARETS || !s_cx) return;
  for (int p = 0; p <= n; p++) {
    int k = tok_append(0, &s_calc.expr, p);
    math_build_raw(s_doc2, s_ptok, k);
    s_xfollow = false;                         // (laying out only)
    EditLay L = edit_layout(r, false);
    s_xfollow = true;
    if (s_xmax) return;                        // it scrolls: the cursor slides along instead
    MathCtx mc = { ctx, 0, 0, GColorBlack };
    s_mg.ctx = &mc;
    s_rec = true; s_rec_next = s_rec_got = false;
    math_draw(s_doc2, &s_mg, L.x, L.base);
    s_rec = false;
    if (!s_rec_got) return;
    s_cx[p] = (int16_t)s_rec_x; s_cy[p] = (int16_t)s_rec_y;
  }
  s_ncar = n + 1;
}

static void draw_panel(GContext *ctx) {
  GRect panel = GRect(2, 2, SCR_W - 4, PANEL_H - 2);
  graphics_context_set_fill_color(ctx, GColorPastelYellow);
  graphics_fill_rect(ctx, panel, 7, GCornersAll);

  // small line (top) and big line (bottom); their split depends on what's shown
  GRect SR = GRect(8, 3, SCR_W - 16, 18), BR = GRect(8, 21, SCR_W - 16, 36);
  const GRect NUM = GRect(8, 27, SCR_W - 16, 34);    // an answer in big digits
  if (s_calc.mode == MODE_RESULT || s_calc.mode == MODE_ERROR) SR = GRect(8, 2, SCR_W - 16, 27);
  if (s_calc.mode == MODE_EQ2) { SR = GRect(8, 2, SCR_W - 16, 22); BR = GRect(8, 24, SCR_W - 16, 33); }
  if (s_calc.mode == MODE_LIM) { SR = GRect(8, 2, SCR_W - 16, 32); BR = GRect(8, 33, SCR_W - 16, 25); }
  if (s_calc.mode == MODE_EDIT && !nav()) BR = GRect(8, 4, SCR_W - 16, 53);   // the whole panel, cursor or not
  GColor dim = GColorDarkGray, ink = GColorBlack;
  if (!s_calc.v.deg || calc_fmt != FMT_NORM) {      // RAD, and the number format if not normal
    static char ind[12];
    static const char *F[4] = { "", "SCI", "ENG", "FIX" };
    int o = 0;
    if (!s_calc.v.deg) { ind[o++] = 'R'; ind[o++] = 'A'; ind[o++] = 'D'; ind[o++] = ' '; }
    for (const char *f = F[calc_fmt]; *f; f++) ind[o++] = *f;
    if (calc_fmt != FMT_NORM) ind[o++] = (char)('0' + calc_fmt_digits);
    ind[o] = 0;
    graphics_context_set_text_color(ctx, dim);
    graphics_draw_text(ctx, ind, s_f14, GRect(8, 3, 70, 16), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
  calc_display(&s_calc, s_small, LINE_LEN, s_big, LINE_LEN);
  GFont bf[] = { s_f28b, s_f24b, s_f18b };
  GFont sf[] = { s_f18, s_f14 };
  int cur = s_calc.cur;
  static char lo[20], hi[20], at[28];
  bool op = s_calc.op && (s_calc.mode == MODE_LIM || s_calc.mode == MODE_RESULT || s_calc.mode == MODE_ERROR);
  if (op) {                                          // the integral / derivative, typeset, small
    bool done = s_calc.mode != MODE_LIM;
    if (s_calc.op == OP_INT) {
      if (done || s_calc.step == 1) calc_format(s_calc.lim_a, 6, lo, sizeof lo); else snprintf(lo, sizeof lo, "a");
      if (done) calc_format(s_calc.lim_b, 6, hi, sizeof hi); else snprintf(hi, sizeof hi, "b");
    } else {
      char num[20];
      calc_format(done ? s_calc.lim_a : s_calc.v.x, 6, num, sizeof num);
      snprintf(at, sizeof at, "x=%s", num);
    }
    int n = tok_append(0, &s_calc.eq1, CALC_CUR_END);
    math_fit(ctx, s_ptok, n, s_calc.op == OP_INT ? WRAP_INT : WRAP_DER,
             s_calc.op == OP_INT ? lo : (done ? at : NULL), hi,
             SR, 2, s_calc.mode == MODE_RESULT ? (s_calc.approx ? "~" : "=") : NULL, dim, false);
  }

  switch (s_calc.mode) {
    case MODE_EDIT: {
      if (cur == CALC_CUR_END && nav() && s_calc.expr.n == 0) {
        graphics_context_set_text_color(ctx, dim);
        draw_fit(ctx, "Up/Down + Select", SR, sf, 2);
      }
      if (s_want_carets && cur != CALC_CUR_END) carets(ctx, BR);
      if (s_calc.expr.n == 0 && cur == CALC_CUR_END) {
        graphics_context_set_text_color(ctx, ink);
        draw_fit(ctx, "0", NUM, bf, 3);
      } else {
        int n = tok_append(0, &s_calc.expr, cur);
        edit_draw(ctx, n, BR, ink, cur != CALC_CUR_END);
      }
      break;
    }
    case MODE_EQ2: {                                 // the equation before, small; this one, big
      int n = tok_append(0, s_calc.eq2.n ? &s_calc.eq2 : &s_calc.eq1, CALC_CUR_END);
      math_fit(ctx, s_ptok, n, WRAP_NONE, NULL, NULL, SR, 2, NULL, dim, false);
      if (s_want_carets && cur != CALC_CUR_END) carets(ctx, BR);
      if (s_calc.expr.n || cur != CALC_CUR_END) {
        n = tok_append(0, &s_calc.expr, cur);
        edit_draw(ctx, n, BR, ink, cur != CALC_CUR_END);
      } else {
        graphics_context_set_text_color(ctx, dim);
        draw_fit(ctx, s_calc.eq2.n ? "3rd equation?" : "2nd equation?", NUM, bf, 3);
      }
      break;
    }
    case MODE_LIM: {                                 // "from =" then what's typed
      static const char *P[3] = { "from =", "to =", "at x =" };
      graphics_context_set_text_color(ctx, dim);
      graphics_draw_text(ctx, P[s_calc.step], s_f18b, GRect(8, 35, 70, 22), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      int pw = text_w(P[s_calc.step], s_f18b) + 6;
      GRect r = GRect(8 + pw, BR.origin.y, BR.size.w - pw, BR.size.h);
      if (s_calc.expr.n || cur != CALC_CUR_END) {
        int n = tok_append(0, &s_calc.expr, cur);
        math_fit(ctx, s_ptok, n, WRAP_NONE, NULL, NULL, r, 0, NULL, ink, false);
      } else if (s_calc.step == 1) {
        graphics_context_set_text_color(ctx, dim);
        draw_fit(ctx, "?", GRect(r.origin.x, NUM.origin.y, r.size.w, NUM.size.h), bf, 3);
      } else {                                     // blank: = now gives the formula
        graphics_context_set_text_color(ctx, GColorDarkGray);
        graphics_draw_text(ctx, "= for formula", s_f14, GRect(r.origin.x, r.origin.y + 6, r.size.w, 16),
                           GTextOverflowModeFill, GTextAlignmentRight, NULL);
      }
      break;
    }
    case MODE_RESULT:
    case MODE_ERROR: {
      if (s_calc.mode == MODE_RESULT && s_calc.sym) {   // a formula: what it came from, small; the formula, big
        static char none[1] = "";
        int n = tok_append(0, &s_calc.eq1, CALC_CUR_END);
        int w = s_calc.sym == SYM_DER ? WRAP_DER : s_calc.sym == SYM_INT ? WRAP_INT : WRAP_NONE;
        math_fit(ctx, s_ptok, n, w, s_calc.sym == SYM_DER ? NULL : none, none, SR, 2, "=", dim, false);
        n = tok_append(0, &s_calc.expr, CALC_CUR_END);
        // shrinks to fit; if it's still too wide its start runs off the left
        // (pull down for the formula view to see all of it)
        math_fit(ctx, s_ptok, n, WRAP_NONE, NULL, NULL, GRect(8, 27, SCR_W - 16, 31), 0,
                 s_calc.sym == SYM_INT ? "+C" : NULL, ink, false);
        break;
      }
      bool plain = !s_calc.op && !s_calc.sys && !s_calc.stored && !s_calc.conv && !s_calc.label[0];
      if (plain) {                                   // the expression, typeset, small
        int n = tok_append(0, &s_calc.expr, CALC_CUR_END);
        bool eq = s_calc.mode == MODE_RESULT && !s_calc.solved;
        math_fit(ctx, s_ptok, n, WRAP_NONE, NULL, NULL, SR, 2, eq ? "=" : NULL, dim, false);
      } else if (!s_calc.op) {                       // "x = ..." / "Stored to x"
        graphics_context_set_text_color(ctx, dim);
        draw_fit(ctx, s_small, SR, sf, 2);
      }
      graphics_context_set_text_color(ctx, s_calc.mode == MODE_ERROR ? GColorDarkCandyAppleRed : ink);
      if (s_calc.mode == MODE_RESULT && s_alt == ALT_EXACT && s_alt_tok)
        math_fit(ctx, s_alt_tok->t, s_alt_tok->n, WRAP_NONE, NULL, NULL, NUM, 0, NULL, ink, false);
      else draw_fit(ctx, s_calc.mode == MODE_RESULT && s_alt == ALT_TEXT ? s_alt_txt : s_big, NUM, bf, 3);
      break;
    }
  }

  if (s_calc.pend) {                                 // STO / RCL waiting for x or y
    graphics_context_set_fill_color(ctx, GColorPastelYellow);
    graphics_fill_rect(ctx, GRect(8, 3, SCR_W - 16, 18), 0, GCornerNone);
    graphics_context_set_text_color(ctx, GColorDarkCandyAppleRed);
    graphics_draw_text(ctx, s_calc.pend == K_STOX ? "STO to: x, y or MEM" : "RCL: x, y or MEM", s_f14,
                       GRect(8, 3, SCR_W - 16, 16), GTextOverflowModeFill, GTextAlignmentRight, NULL);
  }

  // grab bar: hints that the panel pulls down into the formula sheet
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_rect(ctx, GRect(SCR_W / 2 - 12, 58, 24, 2), 1, GCornersAll);
}

#define SHEET_CLOSED (-228)
#define SOL_CLOSED   228
static int s_sheet_y = SHEET_CLOSED;      // formula sheet top edge; 0 = fully open
static int s_sol_y = SOL_CLOSED;          // solver sheet top edge; 0 = fully open
#define SOLVER_PAGE 2
#define MORE_PAGE 3
#define TOOLS_PAGE 4
static void draw_sheet(GContext *ctx);
static void draw_solver(GContext *ctx);
static void draw_menu(GContext *ctx);
static void draw_graph_sheet(GContext *ctx);
static bool s_graph_open;

static void update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  if (s_graph_open) { draw_graph_sheet(ctx); return; }
  if (s_menu) { draw_menu(ctx); return; }
  if (s_sheet_y < 0 && s_sol_y > 0) {      // a fully open sheet hides the calculator
    draw_panel(ctx);
    for (int i = 0; i < 16; i++) draw_key_at(ctx, s_page, i);
    if (s_page < N_KEYPADS || s_page == MORE_PAGE) {
      // grab bar at the bottom edge: pull up for the solver keys (from MORE: the tools)
      graphics_context_set_fill_color(ctx, GColorJaegerGreen);
      graphics_fill_rect(ctx, GRect(SCR_W / 2 - 12, 226, 24, 2), 0, GCornerNone);
    }
  }
  draw_sheet(ctx);
  draw_solver(ctx);
}

static void redraw(void) { layer_mark_dirty(s_layer); }

// ------------------------------------------------------------------ actions

static void set_page(int p) {
  s_page = (p + N_PAGES) % N_PAGES;
  if (s_cursor >= 0 && slot_owner(s_page, s_cursor) != s_cursor) s_cursor = 0;
  redraw();
}

static bool solver_can_open(void);
static bool system_can_open(void);
static void sol_open(void);
static void sol_open_as(bool sys);

// ------------------------------------------------------------------ history

// Record the answer now showing, if it's new.
static void hist_note(void) {
  const Calc *c = &s_calc;
  if (c->mode != MODE_RESULT || c->stored || c->sym || c->sys3) return;
  Hist h;
  memset(&h, 0, sizeof h);
  h.r = c->result;
  if (c->op) {
    h.kind = c->op == OP_INT ? H_INT : H_DER;
    h.t = c->eq1; h.a = c->lim_a; h.b = c->lim_b;
  } else if (c->sys) {
    h.kind = H_SYS;
    h.a = c->v.y;
    int n = 0;
    for (int i = 0; i < c->eq1.n && n < CALC_MAX_TOK; i++) h.t.t[n++] = c->eq1.t[i];
    if (n < CALC_MAX_TOK) h.t.t[n++] = T_SEP;
    for (int i = 0; i < c->expr.n && n < CALC_MAX_TOK; i++) h.t.t[n++] = c->expr.t[i];
    h.t.n = (uint8_t)n;
  } else {
    h.kind = c->solved ? H_ROOT : H_CALC;
    h.t = c->expr;
    if (h.kind == H_CALC) h.a = c->result_im;        // a complex answer
  }
  if (h.t.n == 0) return;
  if (c->approx && (h.kind == H_INT || h.kind == H_DER)) h.kind |= H_APPROX;
  const Hist *top = &s_hist[0];
  if (s_nhist && top->kind == h.kind && top->r == h.r && top->a == h.a && top->b == h.b &&
      top->t.n == h.t.n && !memcmp(top->t.t, h.t.t, h.t.n)) return;   // same as the last one
  memmove(&s_hist[1], &s_hist[0], (HIST_N - 1) * sizeof(Hist));
  s_hist[0] = h;
  if (s_nhist < HIST_N) s_nhist++;
}

// Bring a history entry back as the current answer (it's also Ans).
static void hist_use(int i) {
  if (i < 0 || i >= s_nhist) return;
  const Hist *h = &s_hist[i];
  Calc *c = &s_calc;
  c->inv = false;
  c->cur = CALC_CUR_END;
  c->expr.n = 0; c->eq1.n = 0;
  c->op = OP_NONE; c->sys = false; c->approx = false;
  c->conv = CONV_NONE; c->label[0] = 0;
  switch (HK(h)) {
    case H_INT: case H_DER:
      c->eq1 = h->t; c->op = HK(h) == H_INT ? OP_INT : OP_DER;
      c->approx = (h->kind & H_APPROX) != 0;
      c->lim_a = h->a; c->lim_b = h->b;
      c->result = c->v.ans = h->r;
      c->mode = MODE_RESULT; c->solved = c->stored = false;
      break;
    case H_SYS: {
      int k = 0;
      while (k < h->t.n && h->t.t[k] != T_SEP) k++;
      for (int j = 0; j < k; j++) c->eq1.t[j] = h->t.t[j];
      c->eq1.n = (uint8_t)k;
      for (int j = k + 1; j < h->t.n; j++) c->expr.t[c->expr.n++] = h->t.t[j];
      calc_take_sol2(c, h->r, h->a);
      break;
    }
    case H_ROOT:
      c->expr = h->t;
      calc_take_root(c, h->r);
      break;
    default:
      c->expr = h->t;
      c->result = c->v.ans = h->r;
      c->result_im = c->v.ans_im = h->a;
      c->mode = MODE_RESULT; c->solved = c->stored = false;
      break;
  }
}

static void buzz(uint32_t ms);
static void press_code(int code) {
  s_xfollow = true;                        // a key: the cursor (or end) comes back into view
  s_alt = ALT_NONE;
  // SOLVE (or = on a solved equation) shows the roots on the solver sheet.
  bool solve_key = code == K_SOLVE || code == K_EVAL;
  bool sys_state = s_calc.mode == MODE_EQ2 ||
                   ((s_calc.mode == MODE_RESULT || s_calc.mode == MODE_ERROR) && s_calc.sys);
  // Second equation ending in a bare "=": take the sign out and solve it as "= 0".
  TokBuf *e = &s_calc.expr;
  bool stripped = false;
  if (code == K_EVAL && s_calc.mode == MODE_EQ2 && e->n > 1 && e->t[e->n - 1] == '=') {
    stripped = true;
    for (int i = 0; i < e->n - 1; i++) if (e->t[i] == '=') stripped = false;
    if (stripped) { e->n--; s_calc.cur = CALC_CUR_END; }
  }
  bool second_ready = s_calc.mode != MODE_EQ2 || calc_eq_ready(e) || stripped;
  if (solve_key && sys_state && second_ready) {
    if (system_can_open()) {
      s_calc.inv = false;
      sol_open_as(true);
      redraw();
      return;
    }
  }
  bool resolve = code == K_EVAL && s_calc.mode == MODE_RESULT && s_calc.solved && !s_calc.stored;
  // "=" on an equation (there's no separate SOLVE key) solves it
  if (code == K_EVAL && (s_calc.mode == MODE_EDIT || s_calc.mode == MODE_ERROR) &&
      calc_eq_ready(&s_calc.expr)) resolve = true;
  if ((code == K_SOLVE || resolve) && !sys_state && solver_can_open()) {
    s_calc.inv = false;
    sol_open();
    redraw();
    return;
  }
  calc_key(&s_calc, code);
  if (calc_refused) { calc_refused = false; buzz(60); }
  hist_note();
  redraw();
}

static void buzz(uint32_t ms) {
  if (!ms || !s_set.buzz) return;
  static uint32_t d[1];
  d[0] = ms;
  vibes_enqueue_custom_pattern((VibePattern){ .durations = d, .num_segments = 1 });
}

static void menu_open(int which);
static void graph_open(void);
static void tools_key(int code);

static void press_slot(int slot) {
  if (slot >= 0) buzz(s_set.buzz == 2 ? 40 : KEY_BUZZ_MS);   // a key registered
  if (slot < 0) return;
  if (s_set.light == 1) light_enable_interaction();
  const KeyDef *k = &PAGES[s_page][slot];
  int code = (s_calc.inv && k->inv_code) ? k->inv_code : k->code;
  if (code == K_HIST) { menu_open(MENU_HIST); return; }
  if (code == K_GRAPH) { graph_open(); return; }
  if (code >= K_SD && code <= K_BASE) { tools_key(code); return; }
  press_code(code);
}


// ------------------------------------------------------------------ formula sheet
//
// Pull down anywhere (or hold Down) and a sheet slides over the calculator
// showing the expression typeset as math. Tap, push it up, or press
// Back/Select to put it away. Big formulas can be dragged around.

#define FA_X 6
#define FA_Y 24
#define RES_Y 176

static const int MATH_BASE[MATH_LEVELS] = { 28, 24, 18, 14 };   // box top -> baseline

static int s_sheet_target = SHEET_CLOSED;
static bool s_sheet_open;                 // sheet owns the input
static AppTimer *s_anim;
static MathDoc *s_doc_mem;           // on the heap (3.5 KB), allocated at start
#define s_doc (*s_doc_mem)
static Exact *s_exact;               // exact forms (heap)
static TokBuf s_doc_tok;
static uint8_t s_sys_tok[255];   // "eq1 ; eq2", or a formula plus its exact form, for the typesetter

static int s_pan_x, s_pan_y;
// The sheets' logic is in sheet.c (the math overlay on the watch); its state is here.
static SolState S = { .gx0 = -10, .gx1 = 10, .gy0 = -10, .gy1 = 10 };
#define s_fw S.fw
#define s_fasc S.fasc
#define s_fdesc S.fdesc
static SheetIO s_io;
static char s_sheet_res[80];
static bool s_sheet_res_err, s_sheet_res_dim;

static GFont math_font(int l) {
  return l == 0 ? s_f28b : l == 1 ? s_f24b : l == 2 ? s_f18b : s_f14;
}
static int m_text_w(void *c, const char *t, int l) { return text_w(t, math_font(l)); }
static void m_text(void *c, int x, int y, const char *t, int l) {
  MathCtx *m = c;
  if (s_rec) return;
  graphics_draw_text(m->g, t, math_font(l), GRect(x + m->ox, y + m->oy - MATH_BASE[l], 400, MATH_BASE[l] + 12),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}
static void m_line(void *c, int x0, int y0, int x1, int y1, int w) {
  MathCtx *m = c;
  if (s_rec) {
    if (s_rec_next) { s_rec_x = x0 + m->ox; s_rec_y = (y0 + y1) / 2 + m->oy; s_rec_got = true; s_rec_next = false; }
    return;
  }
  graphics_context_set_stroke_width(m->g, (uint8_t)w);
  graphics_draw_line(m->g, GPoint(x0 + m->ox, y0 + m->oy), GPoint(x1 + m->ox, y1 + m->oy));
}
static void m_box(void *c, int x, int y, int w, int h) {
  MathCtx *m = c;
  if (s_rec) return;
  graphics_context_set_stroke_color(m->g, GColorLightGray);
  graphics_context_set_stroke_width(m->g, 1);
  graphics_draw_rect(m->g, GRect(x + m->ox, y + m->oy, w, h));
  graphics_context_set_stroke_color(m->g, m->ink);
}
// Colour for what's drawn next: the line's own ink, dim for a bracket not typed
// yet, or the cursor's blue.
static void m_ink(void *c, int which) {
  MathCtx *m = c;
  if (s_rec) { if (which == MATH_INK_CURSOR) s_rec_next = true; return; }
  GColor col = which == MATH_INK_DIM ? GColorLightGray : which == MATH_INK_CURSOR ? GColorBlue : m->ink;
  graphics_context_set_stroke_color(m->g, col);
  graphics_context_set_text_color(m->g, col);
}
static MathGfx s_mg = {
  m_text_w, m_text, m_line, m_box, m_ink, NULL,
  { 18, 15, 11, 9 },   // cap heights of Gothic 28/24/18/14 bold
  { 6, 5, 4, 3 },
};

static int pan_max_x(void) { return s_fw > FA_W ? s_fw - FA_W : 0; }
static int pan_max_y(void) { int h = s_fasc + s_fdesc; return h > FA_H ? h - FA_H : 0; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Typeset what the sheet logic asked for (done here, after the math overlay
// has returned, so it and the typesetter never need memory at once).
static bool sheet_build(void) {
  if (s_io.bn < 0) return false;
  math_build_raw(s_doc_mem, s_io.bt, s_io.bn);
  if (s_io.wrap == 1) math_wrap_integral(s_doc_mem, s_io.lo, s_io.hi);
  else if (s_io.wrap) math_wrap_derivative(s_doc_mem, s_io.wrap == 3 ? s_io.at : NULL);
  s_io.bn = -1;
  return true;
}

// Snapshot the expression, typeset it, and work out the result line.
static void sheet_prepare(void) {
  s_io.bn = -1;
  sh_formula(&s_io);
  if (s_io.bn < 0) { s_io.bt = s_calc.expr.t; s_io.bn = s_calc.expr.n; s_io.wrap = 0; }   // math didn't load: plain
  sheet_build();
  int lvl;
  for (lvl = 0; lvl < MATH_LEVELS - 1; lvl++) {    // largest size that fits
    math_measure(s_doc_mem, &s_mg, lvl, &S.fw, &S.fasc, &S.fdesc);
    if (S.fw <= FA_W && S.fasc + S.fdesc <= FA_H) break;
  }
  if (lvl == MATH_LEVELS - 1)                      // too big: pan at a readable size
    math_measure(s_doc_mem, &s_mg, 2, &S.fw, &S.fasc, &S.fdesc);
  s_pan_x = s_pan_y = 0;

  char num[32];
  s_sheet_res[0] = 0;
  s_sheet_res_err = s_sheet_res_dim = false;
  switch (s_calc.mode) {
    case MODE_LIM:                                    // "from = ?" etc.
      s_sheet_res_dim = true;
      calc_display(&s_calc, s_small, LINE_LEN, s_big, LINE_LEN);
      snprintf(s_sheet_res, sizeof s_sheet_res, "%.70s", s_big);
      break;
    case MODE_RESULT:
      calc_format_cplx(s_calc.result, s_calc.result_im, num, sizeof num);
      if (s_calc.sym) {
        static const char *W[] = { "", "derivative", "integral, + C", "factored", "expanded" };
        s_sheet_res_dim = true;
        snprintf(s_sheet_res, sizeof s_sheet_res, "%s", W[s_calc.sym]);
      } else if (s_calc.op) {
        snprintf(s_sheet_res, sizeof s_sheet_res, "%s %s", s_calc.approx ? "~" : "=", num);
      } else if (s_calc.sys) {
        char ny[24], nz[24];
        calc_format(s_calc.v.y, s_calc.sys3 ? 7 : 10, ny, sizeof ny);
        calc_format(s_calc.v.z, 7, nz, sizeof nz);
        if (s_calc.sys3) calc_format(s_calc.result, 7, num, sizeof num);
        snprintf(s_sheet_res, sizeof s_sheet_res, s_calc.sys3 ? "x=%s, y=%s, z=%s" : "x=%s, y=%s", num, ny, nz);
      } else {
        snprintf(s_sheet_res, sizeof s_sheet_res, "%s%s",
                 (s_calc.solved || s_calc.stored) ? (s_calc.var == 1 ? "y = " : "x = ") : "= ", num);
      }
      break;
    case MODE_EQ2:
      s_sheet_res_dim = true;
      snprintf(s_sheet_res, sizeof s_sheet_res, s_calc.expr.n ? "SOLVE solves both" : "2nd equation?");
      break;
    case MODE_ERROR:
      snprintf(s_sheet_res, sizeof s_sheet_res, "%s", calc_err_text((CalcErr)s_calc.err));
      s_sheet_res_err = true;
      break;
    default: {                                        // live preview while editing
      double v;
      s_sheet_res_dim = true;
      if (s_calc.expr.n && calc_has_eq(&s_calc.expr))
        snprintf(s_sheet_res, sizeof s_sheet_res, calc_has_x(&s_calc.expr) ? "SOLVE finds x" : "");
      else if (s_calc.expr.n && calc_eval(&s_calc.expr, &s_calc.v, &v) == CALC_OK) {
        calc_format(v, 10, num, sizeof num);
        snprintf(s_sheet_res, sizeof s_sheet_res, "= %s", num);
      }
      break;
    }
  }
}

static void draw_sheet(GContext *ctx) {
  if (s_sheet_y <= SHEET_CLOSED) return;
  int oy = s_sheet_y;
  GColor bg = GColorPastelYellow;
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, GRect(0, oy, SCR_W, 228), 0, GCornerNone);

  // formula, centred when it fits, otherwise panned
  int h = s_fasc + s_fdesc;
  int fx = s_fw <= FA_W ? FA_X + (FA_W - s_fw) / 2 : FA_X - s_pan_x;
  int top = h <= FA_H ? FA_Y + (FA_H - h) / 2 : FA_Y - s_pan_y;
  MathCtx mc = { ctx, 0, oy, GColorBlack };
  s_mg.ctx = &mc;
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  math_draw(&s_doc, &s_mg, fx, top + s_fasc);
  graphics_context_set_stroke_width(ctx, 1);

  // header and footer are painted over anything that pans out of the area
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, GRect(0, oy, SCR_W, FA_Y - 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(0, oy + FA_Y + FA_H + 2, SCR_W, 228 - FA_Y - FA_H - 2), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, (pan_max_x() || pan_max_y()) ? "FORMULA - drag to see all" : "FORMULA",
                     s_f14, GRect(8, oy + 3, 150, 16), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, s_calc.v.deg ? "DEG" : "RAD", s_f14, GRect(158, oy + 3, 34, 16),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);

  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(8, oy + RES_Y - 1), GPoint(SCR_W - 9, oy + RES_Y - 1));
  graphics_context_set_text_color(ctx, s_sheet_res_err ? GColorDarkCandyAppleRed :
                                       s_sheet_res_dim ? GColorDarkGray : GColorBlack);
  GFont rf[] = { s_f28b, s_f24b, s_f18b };
  draw_fit(ctx, s_sheet_res, GRect(8, oy + RES_Y - 2, SCR_W - 16, 34), rf, 3);

  // handle and bottom edge
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(SCR_W / 2 - 18, oy + 217, 36, 4), 2, GCornersAll);
  if (oy < 0) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(0, oy + 226, SCR_W, 2), 0, GCornerNone);
  }
}

static int s_sol_target = SOL_CLOSED;

static int ease(int cur, int target) {
  int diff = target - cur;
  int step = diff / 3;
  if (step > -12 && step < 12) step = diff > 0 ? (diff < 12 ? diff : 12) : (diff > -12 ? diff : -12);
  return cur + step;
}

static void anim_cb(void *data) {
  s_anim = NULL;
  s_sheet_y = ease(s_sheet_y, s_sheet_target);
  s_sol_y = ease(s_sol_y, s_sol_target);
  if (s_sheet_y != s_sheet_target || s_sol_y != s_sol_target)
    s_anim = app_timer_register(16, anim_cb, NULL);
  redraw();
}

static void anim_start(void) {
  if (!s_anim) s_anim = app_timer_register(16, anim_cb, NULL);
}

static void sheet_animate(int target) {
  s_sheet_target = target;
  anim_start();
}

static void sheet_open(void) {
  if (s_sheet_y <= SHEET_CLOSED) sheet_prepare();
  s_sheet_open = true;
  s_pressed = -1;
  sheet_animate(0);
}

static void sheet_close(void) {
  s_sheet_open = false;
  sheet_animate(SHEET_CLOSED);
}

static void sheet_pan(int d) {   // buttons: scroll whichever way it overflows
  if (pan_max_y()) s_pan_y = clampi(s_pan_y + d * 40, 0, pan_max_y());
  else s_pan_x = clampi(s_pan_x + d * 60, 0, pan_max_x());
  redraw();
}

// ------------------------------------------------------------------ solver sheet
//
// SOLVE slides this sheet up with every root of the equation, no guess
// needed. Tap one (or Up/Down + Select) to use it: it goes into x and Ans.
// - / + (or hold Up/Down) narrow or widen where it looks. With y in the
// equation, it solves the two equations together and lists (x, y) pairs.
// With SOLVER_GRAPH (config.h) one-variable equations are graphed as well.

#define ROW_H2 24

static bool s_sol_open;
#define s_sys S.sys
#define s_gx0 S.gx0
#define s_gx1 S.gx1
#define s_gy0 S.gy0
#define s_gy1 S.gy1
#define s_roots S.roots
#define s_root_y S.root_y
#define s_has_exact S.has_exact
#define s_rx S.rx
#define s_rcplx S.rcplx
#define s_nroots S.nroots
#define s_sel S.sel
#define s_list_top S.list_top
#define s_expand S.expand
#define s_sol_has_x S.has_x
#define s_sol_has_eq S.has_eq
#define s_sw S.sw
#define s_sasc S.sasc
#define s_sdesc S.sdesc
#define s_slvl S.slvl
static bool s_finding, s_roots_stale;
static AppTimer *s_find_timer;
static char s_range_txt[48];

#if SOLVER_GRAPH
#define G_X 4
#define G_Y 22             // with the graph the equation isn't shown: graph, then answers
#define G_W 192
#define G_H 110
#define s_fit S.fit
static bool graph_on(void) { return s_set.graph && !s_sys; }
#else
static bool graph_on(void) { return false; }
#endif

// Layout: with the graph, the graph and 3 answer rows (no equation); without,
// the equation, a control row and 4-5 rows.
// Without the graph the sheet scrolls: the equation gets the height it needs,
// then the -/+ row, then every answer (drag up/down, or Up/Down to step).
static int s_eh = 46, s_sscroll;     // equation band height; scroll (px)
static int eq_h(void) { return s_eh; }
static int list_y(void) { return graph_on() ? 135 : 21 + s_eh + 28; }   // in sheet coordinates
static int sscroll(void) { return graph_on() ? 0 : s_sscroll; }
static bool exact_rows(void) { return s_has_exact && s_exact->n; }
static int n_rows(void) { return graph_on() ? 3 : MAX_ROOTS; }
static int sol_max_scroll(void);
// Rows of exact roots are as tall as the tallest one needs at the smallest
// size ((1+sqrt 5)/2 is a fraction over a root): nothing runs out of its box.
static int row_h(void) {
  if (!exact_rows()) return ROW_H2;
  int h = 30;
  for (int i = 0; i < s_nroots; i++) {
    if (s_rx[i] < 0) continue;
    int w, as, de;
    math_build(s_doc2, &s_exact->sol[s_rx[i]]);
    math_measure(s_doc2, &s_mg, MATH_LEVELS - 1, &w, &as, &de);
    if (as + de + 6 > h) h = as + de + 6;
  }
  return h > 46 ? 46 : h;
}

static GRect zoom_rect(bool in) {
#if SOLVER_GRAPH
  // + at the top (next to Up), - at the bottom (next to Down): holding them zooms
  if (graph_on()) return GRect(G_X + G_W - 21, in ? G_Y + 3 : G_Y + G_H - 20, 18, 17);
#endif
  return GRect(in ? 164 : 8, 21 + s_eh + 4 - s_sscroll, 28, 20);
}

#if SOLVER_GRAPH
// The solver's graph panel: drawn by the graph overlay from s_gs.
static void sol_view(void) {
  GraphState *g = s_gs;
  g->x = G_X; g->y = G_Y; g->w = G_W; g->h = G_H;
  g->kind = s_sol_has_eq ? GK_SIDES : GK_FX;
  g->labels = false;
  g->trace = false;
  g->x0 = s_gx0; g->x1 = s_gx1; g->y0 = s_gy0; g->y1 = s_gy1;
}
static void sol_sample(void) {
  sol_view();
  GrArgs a = { .g = s_gs, .f = &s_doc_tok, .v = &s_calc.v };
  graph_call(GR_SAMPLE, &a);
}
static void sol_autoscale(void) {
  sol_view();
  GrArgs a = { .g = s_gs, .f = &s_doc_tok, .v = &s_calc.v };
  if (graph_call(GR_FIT, &a)) { s_gy0 = s_gs->y0; s_gy1 = s_gs->y1; }
}
static int gpx(double x) {
  double v = G_X + (x - s_gx0) / (s_gx1 - s_gx0) * G_W;
  return v < -30000 ? -30000 : v > 30000 ? 30000 : (int)v;
}
#endif

static void sol_range_text(void) {
  char a[16], b[16];
  if (s_sys) {
    calc_format((s_gx1 - s_gx0) / 2, 4, a, sizeof a);
    snprintf(s_range_txt, sizeof s_range_txt, "x, y within +/-%s", a);
    return;
  }
  calc_format(s_gx0, 4, a, sizeof a);
  calc_format(s_gx1, 4, b, sizeof b);
  snprintf(s_range_txt, sizeof s_range_txt, "x %s .. %s", a, b);
}

static void sol_find_cb(void *data);

static void sol_schedule_find(void) {
  if (s_find_timer) app_timer_cancel(s_find_timer);
  s_find_timer = NULL;
  if (!s_sol_has_x) return;
  s_finding = true;
  s_roots_stale = true;
  s_find_timer = app_timer_register(40, sol_find_cb, NULL);
}

// Search box: x in c +- half (systems: y in cy +- half too).
static void sol_set_range(double c, double cy, double half, bool rescale) {
  if (half < 1e-9) half = 1e-9;
  if (half > 1e9) half = 1e9;
  s_gx0 = c - half; s_gx1 = c + half;
  if (s_sys) { s_gy0 = cy - half; s_gy1 = cy + half; }
  sol_range_text();
#if SOLVER_GRAPH
  if (!s_sol_has_x || s_sys) return;
  sol_sample();
  if (rescale) sol_autoscale();
#else
  (void)rescale;
#endif
}

static void sol_io(void) {
  s_io.graph = graph_on();
  s_io.rows = n_rows();
}

// The solver's equation at the largest size that fits across.
static void sol_fit(void) {
  for (s_slvl = 1; s_slvl < MATH_LEVELS; s_slvl++) {
    math_measure(s_doc_mem, &s_mg, s_slvl, &s_sw, &s_sasc, &s_sdesc);
    if (s_sw <= SOL_EQ_W && (s_slvl >= 2 || s_sasc + s_sdesc <= 60)) break;
  }
  if (s_slvl == MATH_LEVELS) {
    s_slvl = MATH_LEVELS - 1;
    math_measure(s_doc_mem, &s_mg, s_slvl, &s_sw, &s_sasc, &s_sdesc);
  }
}

static void sol_find_cb(void *data) {
  s_find_timer = NULL;
  sol_io();
  s_io.bn = -1;
  int more = sh_find(&s_io);
  if (sheet_build()) sol_fit();             // the exact form was dropped: the plain equation
  if (more) {         // nothing on first look: widen the search a few times
    s_expand--;
    sol_set_range((s_gx0 + s_gx1) / 2, (s_gy0 + s_gy1) / 2, (s_gx1 - s_gx0) * 5, true);
    s_find_timer = app_timer_register(20, sol_find_cb, NULL);
    redraw();
    return;
  }
  if (!graph_on()) { s_list_top = 0; s_sscroll = clampi(s_sscroll, 0, sol_max_scroll()); }
  s_finding = false;
  s_roots_stale = false;
  redraw();
}

static double near0(double v) { return (v > -1e6 && v < 1e6) ? ((v > -1e-6 && v < 1e-6) ? 0 : v) : 0; }

static void sol_prepare(bool sys) {
  s_io.bn = -1;
  sh_solver(&s_io, sys);
  if (s_io.bn < 0) { s_io.bt = s_calc.expr.t; s_io.bn = s_calc.expr.n; s_io.wrap = 0; }   // math didn't load
  sheet_build();
  sol_fit();
  s_eh = s_sasc + s_sdesc + 10;                               // as tall as it needs (it scrolls)
  if (s_eh < 46) s_eh = 46;
  s_sscroll = 0;
  s_sheet_y = SHEET_CLOSED;   // the doc is shared with the formula sheet
  s_nroots = 0;
  s_sel = 0;
  s_expand = 3;
#if SOLVER_GRAPH
  s_fit = true;
#endif
  sol_set_range(near0(s_calc.v.x), near0(s_calc.v.y), 10, true);
  sol_schedule_find();
}

static void sol_zoom(double f) {
  s_expand = 0;
#if SOLVER_GRAPH
  s_fit = true;
#endif
  sol_set_range((s_gx0 + s_gx1) / 2, (s_gy0 + s_gy1) / 2, (s_gx1 - s_gx0) / 2 * f, true);
  sol_schedule_find();
  redraw();
}

static bool solver_can_open(void) {
  double v;
  return s_calc.expr.n && calc_has_x(&s_calc.expr) && !calc_has_y(&s_calc.expr) &&
         !calc_has_z(&s_calc.expr) &&            // z is an unknown too: a system, not the solver sheet
         calc_eval(&s_calc.expr, &s_calc.v, &v) != CALC_ERR_SYNTAX;
}

static bool system_can_open(void) {           // two equations in x and y (three with z: calc.c solves them)
  double v;
  if (s_calc.eq2.n || calc_has_z(&s_calc.eq1) || calc_has_z(&s_calc.expr)) return false;
  return s_calc.eq1.n && s_calc.expr.n &&
         calc_eval(&s_calc.eq1, &s_calc.v, &v) != CALC_ERR_SYNTAX &&
         calc_eval(&s_calc.expr, &s_calc.v, &v) != CALC_ERR_SYNTAX;
}

static void sol_open_as(bool sys) {
  if (s_sol_y >= SOL_CLOSED || sys != s_sys) sol_prepare(sys);
  s_sol_open = true;
  s_pressed = -1;
  s_sol_target = 0;
  anim_start();
}
static void sol_open(void) { sol_open_as(false); }

static void sol_close(void) {
  s_sol_open = false;
  s_sol_target = SOL_CLOSED;
  anim_start();
}

static int sol_max_scroll(void) {
  if (graph_on()) return 0;
  int bottom = list_y() + (s_nroots ? s_nroots : 2) * row_h() + 4;
  return bottom > 228 ? bottom - 228 : 0;
}

static void sol_select(int d) {
  if (s_nroots == 0) return;
  int rows = n_rows();
  s_sel = clampi(s_sel + d, 0, s_nroots - 1);
  if (s_sel < s_list_top) s_list_top = s_sel;
  if (s_sel > s_list_top + rows - 1) s_list_top = s_sel - rows + 1;
  if (!graph_on()) {                       // scroll so the highlighted answer is in view
    int top = list_y() + s_sel * row_h(), bot = top + row_h();
    if (top - s_sscroll < 21) s_sscroll = top - 21;
    if (bot - s_sscroll > 226) s_sscroll = bot - 226;
    if (s_sel == 0) s_sscroll = 0;         // back at the first: show the equation again
    s_sscroll = clampi(s_sscroll, 0, sol_max_scroll());
  }
  redraw();
}

static void sol_take(int i) {
  if (i < 0 || i >= s_nroots || s_rcplx[i]) return;
  if (s_sys) calc_take_sol2(&s_calc, s_roots[i], s_root_y[i]);
  else calc_take_root(&s_calc, s_roots[i]);
  hist_note();
  sol_close();
  redraw();
}

#if SOLVER_GRAPH
static void sol_text(GContext *ctx, const char *t, GFont f, GRect r, GTextAlignment a, GColor col);
static void draw_graph(GContext *ctx, int oy) {
  sol_view();
  GraphState *g = s_gs;
  g->y = G_Y + oy;
  g->rx = s_roots; g->ry = s_sol_has_eq ? s_root_y : NULL;
  g->nroots = s_nroots; g->sel = s_sel; g->roots_stale = s_roots_stale;
  s_gfx.ctx = ctx;
  GrArgs a = { .g = g, .f = &s_doc_tok, .v = &s_calc.v, .gfx = &s_gfx };
  if (!graph_call(GR_DRAW, &a)) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(G_X, G_Y + oy, G_W, G_H), 0, GCornerNone);
  }
  sol_text(ctx, "hold Up/Down to zoom", s_f14, GRect(G_X + 5, oy + G_Y + G_H - 18, 140, 16),
           GTextAlignmentLeft, GColorDarkGray);
}
#endif

static void sol_text(GContext *ctx, const char *t, GFont f, GRect r, GTextAlignment a, GColor col) {
  graphics_context_set_text_color(ctx, col);
  graphics_draw_text(ctx, t, f, r, GTextOverflowModeFill, a, NULL);
}

static void draw_zoom(GContext *ctx, bool in, int oy) {
  GRect r = zoom_rect(in);
  r.origin.y += oy;
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, r, 4, GCornersAll);
  graphics_context_set_text_color(ctx, GColorWhite);
  draw_centered(ctx, in ? "+" : "-", s_f18b, r);
}

static void draw_solver_body(GContext *ctx, int oy, int sy, bool g) {
  // equation (or the two equations with a brace); the graph replaces it
  if (s_sol_has_x && !g) {
    MathCtx mc = { ctx, 0, sy, GColorWhite };
    s_mg.ctx = &mc;
    math_measure(&s_doc, &s_mg, s_slvl, &s_sw, &s_sasc, &s_sdesc);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    int h = s_sasc + s_sdesc, eh = eq_h();
    int fx = s_sw <= SCR_W - 8 ? (SCR_W - s_sw) / 2 : 4;
    math_draw(&s_doc, &s_mg, fx, 21 + (eh - h) / 2 + s_sasc);
    graphics_context_set_fill_color(ctx, GColorOxfordBlue);   // keep anything tall out of the next band
    graphics_fill_rect(ctx, GRect(0, sy + 21 + eh, SCR_W, list_y() - 21 - eh), 0, GCornerNone);
  }

#if SOLVER_GRAPH
  if (g) draw_graph(ctx, oy);
#endif
  if (s_sol_has_x) {
    draw_zoom(ctx, false, oy);
    draw_zoom(ctx, true, oy);
    if (!g) sol_text(ctx, s_range_txt, s_f14, GRect(38, sy + 21 + s_eh + 6, 124, 16), GTextAlignmentCenter, GColorLightGray);
  }

  // answers
  int ly = list_y(), rows = n_rows();
  GRect row0 = GRect(4, ly + sy, 192, ROW_H2 - 2);
  if (!s_sol_has_x) {
    sol_text(ctx, "Type an equation in x,", s_f18b, GRect(0, ly + sy, SCR_W, 22), GTextAlignmentCenter, GColorLightGray);
    sol_text(ctx, "like x^2=2, then SOLVE.", s_f18, GRect(0, ly + 22 + sy, SCR_W, 22), GTextAlignmentCenter, GColorLightGray);
  } else if (s_finding) {
    sol_text(ctx, s_sys ? "Solving..." : "Finding roots...", s_f18b, row0, GTextAlignmentCenter, GColorLightGray);
  } else if (s_nroots == 0 && S.always == 3) {     // y = x twice: every point of it
    sol_text(ctx, "The same curve twice", s_f18b, row0, GTextAlignmentCenter, GColorLightGray);
    sol_text(ctx, "every point on it solves both", s_f14, GRect(4, ly + 24 + sy, 192, 18), GTextAlignmentCenter, GColorDarkGray);
  } else if (s_nroots == 0 && S.always) {          // 2x = 2x: every x is a solution
    sol_text(ctx, "True for every x", s_f18b, row0, GTextAlignmentCenter, GColorLightGray);
    if (S.always == 2) sol_text(ctx, "where it's defined", s_f14, GRect(4, ly + 24 + sy, 192, 18), GTextAlignmentCenter, GColorDarkGray);
  } else if (s_nroots == 0) {
    sol_text(ctx, s_sys ? "No solutions found" : "No roots found", s_f18b, row0, GTextAlignmentCenter, GColorLightGray);
    sol_text(ctx, g ? "drag the graph or zoom out" : "tap - to search wider", s_f14,
             GRect(4, ly + 24 + sy, 192, 18), GTextAlignmentCenter, GColorDarkGray);
  } else {
    int rh = row_h();
    for (int k = 0; k < rows; k++) {
      int i = s_list_top + k;
      if (i >= s_nroots) break;
      GRect r = GRect(4, ly + k * rh + sy, 192, rh - 2);
      if (r.origin.y + rh < oy + 21 || r.origin.y > 228) continue;   // scrolled out of view
      bool sel = i == s_sel;
      graphics_context_set_fill_color(ctx, sel ? GColorJaegerGreen : s_rcplx[i] ? GColorImperialPurple : GColorDukeBlue);
      graphics_fill_rect(ctx, r, 5, GCornersAll);
      char num[24], num2[24], line[56];
      if (s_rx[i] >= 0) {
        // exact form, typeset, with the decimal (or "complex") on the right
        if (s_rcplx[i]) snprintf(num, sizeof num, "complex");
        else if (s_sys) num[0] = 0;
        else calc_format(s_roots[i], 7, num, sizeof num);
        int nw = num[0] ? text_w(num, s_f14) + 6 : 0;
        int w = 0, as = 0, de = 0, lvl = MATH_LEVELS;
        math_build(s_doc2, &s_exact->sol[s_rx[i]]);
        // the largest size that fits the row; if none does, without the
        // decimal; if it still doesn't, the row shows the decimal instead
        for (int pass = 0; pass < 2 && lvl == MATH_LEVELS; pass++) {
          if (pass == 1) { if (!nw) break; nw = 0; num[0] = 0; }   // (a complex row stays purple)
          for (lvl = 2; lvl < MATH_LEVELS; lvl++) {
            math_measure(s_doc2, &s_mg, lvl, &w, &as, &de);
            if (w <= r.size.w - 8 - nw && as + de <= r.size.h) break;
          }
        }
        if (lvl == MATH_LEVELS && !s_rcplx[i]) goto plain;
        if (lvl == MATH_LEVELS) { lvl = MATH_LEVELS - 1; math_measure(s_doc2, &s_mg, lvl, &w, &as, &de); }   // no decimal to show
        MathCtx mc = { ctx, 0, 0, GColorWhite };
        s_mg.ctx = &mc;
        graphics_context_set_text_color(ctx, GColorWhite);
        graphics_context_set_stroke_color(ctx, GColorWhite);
        math_draw(s_doc2, &s_mg, r.origin.x + 5, r.origin.y + (r.size.h - as - de) / 2 + as);
        graphics_context_set_stroke_width(ctx, 1);
        if (num[0])
          sol_text(ctx, num, s_f14, GRect(r.origin.x + r.size.w - nw - 3, r.origin.y + r.size.h / 2 - 9, nw, 16),
                   GTextAlignmentRight, sel ? GColorWhite : GColorLightGray);
        continue;
      }
    plain:
      if (s_sys) {
        calc_format(s_roots[i], 7, num, sizeof num);
        calc_format(s_root_y[i], 7, num2, sizeof num2);
        snprintf(line, sizeof line, "x=%s  y=%s", num, num2);
      } else {
        calc_format(s_roots[i], 10, num, sizeof num);
        snprintf(line, sizeof line, "x = %s", num);
      }
      graphics_context_set_text_color(ctx, GColorWhite);
      GFont f = text_w(line, s_f18b) <= r.size.w - 34 ? s_f18b : s_f14;
      graphics_draw_text(ctx, line, f, GRect(r.origin.x + 6, r.origin.y + r.size.h / 2 - font_top(f), r.size.w - 34, 24),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      char idx[24];
      snprintf(idx, sizeof idx, "%d/%d", i + 1, s_nroots);
      sol_text(ctx, idx, s_f14, GRect(r.origin.x + r.size.w - 34, r.origin.y + 2, 30, 16),
               GTextAlignmentRight, sel ? GColorWhite : GColorLightGray);
    }
  }
}

static void draw_solver(GContext *ctx) {
  if (s_sol_y >= SOL_CLOSED) return;
  int oy = s_sol_y;
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, GRect(0, oy, SCR_W, 228), 0, GCornerNone);
  if (oy > 0) {
    graphics_context_set_fill_color(ctx, GColorJaegerGreen);
    graphics_fill_rect(ctx, GRect(0, oy, SCR_W, 2), 0, GCornerNone);
  }
  bool g = graph_on();
  int sy = oy - sscroll();                 // scrolled content
  draw_solver_body(ctx, oy, sy, g);

  // header (drawn last so scrolled content passes under it): handle, title,
  // and the search range (graph) or answer count
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, GRect(0, oy, SCR_W, 21), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_rect(ctx, GRect(SCR_W / 2 - 14, oy + 4, 28, 3), 1, GCornersAll);
  sol_text(ctx, s_sys ? "SYSTEM" : "SOLVER", s_f14, GRect(8, oy + 4, 60, 16), GTextAlignmentLeft, GColorMalachite);
  if (sol_max_scroll()) {                  // scroll bar
    int m = sol_max_scroll(), h = 205 * 205 / (205 + m), y = 22 + (205 - h) * s_sscroll / m;
    graphics_context_set_fill_color(ctx, GColorLightGray);
    graphics_fill_rect(ctx, GRect(SCR_W - 3, oy + y, 2, h), 0, GCornerNone);
  }
  if (g) {
    sol_text(ctx, s_range_txt, s_f14, GRect(96, oy + 4, 96, 16), GTextAlignmentRight, GColorLightGray);
  } else if (s_sol_has_x && !s_finding) {
    char cnt[24];
    int nreal = 0;
    for (int i = 0; i < s_nroots; i++) nreal += !s_rcplx[i];
    if (nreal == 0 && s_nroots) snprintf(cnt, sizeof cnt, "%d complex", s_nroots);
    else snprintf(cnt, sizeof cnt, "%d %s", nreal,
                  s_sys ? (nreal == 1 ? "solution" : "solutions") : (nreal == 1 ? "root" : "roots"));
    sol_text(ctx, cnt, s_f14, GRect(96, oy + 4, 96, 16), GTextAlignmentRight, GColorLightGray);
  }

}

// ------------------------------------------------------------------ history list
//
// A full-screen list over the calculator, opened instantly: the last answers.
// Tap one (or Up/Down + Select) to bring it back. Back closes it.

#define MENU_Y   24
#define HROW_H   50
#define HROWS    4

static int s_msel, s_mtop;

static void apply_light(void) { light_enable(s_set.light == 2); }

// The TOOLS lists (rows from tools.c): memories, statistics, units, constants, bases.
#define TROW_H 32
#define TROWS  6
static int s_tlist;                   // TL_*
static ToolsIO s_tio;
static StatData *s_stat;              // heap

static int menu_count(void) { return s_menu == MENU_TOOLS ? tl_count(&s_tio, s_tlist) : s_nhist; }
static int menu_rows(void) { return s_menu == MENU_TOOLS ? TROWS : HROWS; }
static int row_h_menu(void) { return s_menu == MENU_TOOLS ? TROW_H : HROW_H; }

static bool s_mkeys;                  // Up/Down have been used in the list: show the highlight
static void menu_open(int which) {
  s_menu = which;
  s_msel = 0; s_mtop = 0;
  s_mkeys = false;
  s_pressed = -1;
  redraw();
}
static void tools_open(int list) {
  s_tio.calc = &s_calc; s_tio.st = s_stat; s_tio.cat = -1;
  s_tlist = list;
  menu_open(MENU_TOOLS);
}
static void menu_close(void) { s_menu = MENU_NONE; redraw(); }
// Back (or a sideways swipe): from a group of units or constants up to the groups, else close.
static void menu_back(void) {
  if (s_menu == MENU_TOOLS && s_tio.cat >= 0) {
    s_tio.cat = -1; s_msel = s_mtop = 0;
    redraw();
    return;
  }
  menu_close();
}

static void menu_select(int d) {
  int n = menu_count();
  if (!n) return;
  if (!nav() && !s_mkeys) { s_mkeys = true; s_msel = s_mtop; redraw(); return; }   // first press: show where it is
  s_msel = clampi(s_msel + d, 0, n - 1);
  if (s_msel < s_mtop) s_mtop = s_msel;
  if (s_msel > s_mtop + menu_rows() - 1) s_mtop = s_msel - menu_rows() + 1;
  redraw();
}
static void save_stats(void);
static void menu_press(int i) {
  if (i < 0 || i >= menu_count()) return;
  buzz(s_set.buzz == 2 ? 40 : KEY_BUZZ_MS);
  if (s_menu != MENU_TOOLS) { hist_use(i); menu_close(); return; }
  s_alt = ALT_NONE;
  int a = tl_pick(&s_tio, s_tlist, i);
  for (int k = 0; k < s_tio.nkeys; k++) calc_key(&s_calc, s_tio.keys[k]);
  s_xfollow = true;
  if (a == TA_ALT) { snprintf(s_alt_txt, sizeof s_alt_txt, "%s", s_tio.alt); s_alt = ALT_TEXT; }
  if (a == TA_KEEP) { save_stats(); redraw(); return; }          // statistics: data added or cleared
  if (a == TA_NONE) { buzz(60); return; }
  if (a == TA_SUB) { s_msel = s_mtop = 0; redraw(); return; }   // a group of units or constants
  hist_note();
  menu_close();
}

static void draw_tools(GContext *ctx) {
  static const char *T[TL_COUNT] = { "MEMORY", "STATISTICS", "UNITS", "CONSTANTS", "BASES", "COMPLEX" };
  static const char *H[TL_COUNT] = { "tap: use / STO / RCL", "tap to add or use", "converts Ans", "tap to insert",
                                     "Ans, or insert", "tap to insert" };
  bool group = s_tio.cat >= 0;
  sol_text(ctx, group ? s_tio.title : T[s_tlist], s_f14, GRect(8, 4, 120, 16), GTextAlignmentLeft, GColorMalachite);
  if (!group) sol_text(ctx, H[s_tlist], s_f14, GRect(70, 4, 122, 16), GTextAlignmentRight, GColorLightGray);
  int n = menu_count();
  for (int k = 0; k < TROWS; k++) {
    int i = s_mtop + k;
    if (i >= n) break;
    GRect r = GRect(4, MENU_Y + k * TROW_H, SCR_W - 8, TROW_H - 3);
    bool sel = i == s_pressed || ((nav() || s_mkeys) && i == s_msel);
    graphics_context_set_fill_color(ctx, sel ? GColorJaegerGreen : GColorDukeBlue);
    graphics_fill_rect(ctx, r, 6, GCornersAll);
    tl_row(&s_tio, s_tlist, i);
    GRect t = GRect(r.origin.x + 6, r.origin.y + 3, r.size.w - 12, 22);
    // label left, value right; a long pair gets the smaller font for the label
    int vw = s_tio.value[0] ? text_w(s_tio.value, s_f14) + 8 : 0;
    GFont lf = text_w(s_tio.label, s_f18b) + vw <= t.size.w ? s_f18b : s_f14;
    int lw = text_w(s_tio.label, lf);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s_tio.label, lf, GRect(t.origin.x, t.origin.y + (lf == s_f14 ? 3 : 0), t.size.w - vw, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    if (vw && lw + vw <= t.size.w) {
      graphics_context_set_text_color(ctx, GColorLightGray);
      graphics_draw_text(ctx, s_tio.value, s_f14, GRect(t.origin.x + t.size.w - vw + 8, t.origin.y + 3, vw - 8, 18),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    }
  }
  if (n > TROWS) {                                 // scroll bar
    int h = 190 * TROWS / n, y = MENU_Y + (190 - h) * s_mtop / (n - TROWS);
    graphics_context_set_fill_color(ctx, GColorLightGray);
    graphics_fill_rect(ctx, GRect(SCR_W - 3, y, 2, h), 0, GCornerNone);
  }
}

// The answer line of a history entry.
static void hist_answer(const Hist *h, char *buf, int len) {
  char a[24], b[24];
  if (HK(h) == H_SYS) {
    calc_format(h->r, 7, a, sizeof a);
    calc_format(h->a, 7, b, sizeof b);
    snprintf(buf, len, "x=%s, y=%s", a, b);
  } else {
    calc_format_cplx(h->r, HK(h) == H_CALC ? h->a : 0, a, sizeof a);
    snprintf(buf, len, "%s %s", HK(h) == H_ROOT ? "x =" : (h->kind & H_APPROX) ? "~" : "=", a);
  }
}

// Keys on the TOOLS page that aren't typed: S<>D shows the answer in exact
// form (press again for the decimals), MODE steps the number format, the
// rest open lists.
static int s_alt_key;
static void tools_key(int code) {
  switch (code) {
    case K_MEM: tools_open(TL_MEM); return;
    case K_STAT: tools_open(TL_STAT); return;
    case K_UNITS: tools_open(TL_UNITS); return;
    case K_CONST: tools_open(TL_CONST); return;
    case K_BASE: tools_open(TL_BASE); return;
    case K_FMT:
      s_set.fmt = (uint8_t)((s_set.fmt + 1) % 4);
      calc_fmt = s_set.fmt;
      save_settings();
      calc_display(&s_calc, s_small, LINE_LEN, s_big, LINE_LEN);
      redraw();
      return;
  }
  if (s_alt && s_alt_key == code) { s_alt = ALT_NONE; redraw(); return; }   // back to the decimals
  s_alt = ALT_NONE;
  if (s_calc.mode != MODE_RESULT || s_calc.sym) { buzz(60); return; }
  if (!s_alt_tok || !sh_exact(&s_io, s_alt_tok)) { buzz(60); return; }    // no exact form
  s_alt = ALT_EXACT;
  s_alt_key = code;
  redraw();
}

static void draw_menu(GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, GRect(0, 0, SCR_W, 228), 0, GCornerNone);
  if (s_menu == MENU_TOOLS) { draw_tools(ctx); return; }
  sol_text(ctx, "HISTORY", s_f14, GRect(8, 4, 100, 16), GTextAlignmentLeft, GColorMalachite);
  sol_text(ctx, "tap to reuse", s_f14, GRect(96, 4, 96, 16), GTextAlignmentRight, GColorLightGray);
  if (!s_nhist) {
    sol_text(ctx, "No answers yet", s_f18b, GRect(0, 90, SCR_W, 24), GTextAlignmentCenter, GColorLightGray);
    sol_text(ctx, "Answers show up here", s_f14, GRect(0, 114, SCR_W, 18), GTextAlignmentCenter, GColorDarkGray);
    return;
  }
  for (int k = 0; k < HROWS; k++) {
    int i = s_mtop + k;
    if (i >= s_nhist) break;
    GRect r = GRect(4, MENU_Y + k * HROW_H, SCR_W - 8, HROW_H - 3);
    bool sel = i == s_pressed || ((nav() || s_mkeys) && i == s_msel);
    graphics_context_set_fill_color(ctx, sel ? GColorJaegerGreen : GColorDukeBlue);
    graphics_fill_rect(ctx, r, 6, GCornersAll);
    const Hist *h = &s_hist[i];
    static char lo[20], hi[20], at[28], ans[56];
    int wrap = WRAP_NONE;
    if (HK(h) == H_INT) {
      calc_format(h->a, 5, lo, sizeof lo); calc_format(h->b, 5, hi, sizeof hi);
      wrap = WRAP_INT;
    } else if (HK(h) == H_DER) {
      calc_format(h->a, 5, hi, sizeof hi);
      snprintf(at, sizeof at, "x=%s", hi);
      wrap = WRAP_DER;
    }
    math_fit(ctx, h->t.t, h->t.n, wrap, HK(h) == H_INT ? lo : at, hi,
             GRect(r.origin.x + 6, r.origin.y + 1, r.size.w - 12, 25), 2, NULL, GColorLightGray, false);
    hist_answer(h, ans, sizeof ans);
    graphics_context_set_text_color(ctx, GColorWhite);
    GFont f = text_w(ans, s_f18b) <= r.size.w - 12 ? s_f18b : s_f14;
    graphics_draw_text(ctx, ans, f, GRect(r.origin.x + 6, r.origin.y + 22, r.size.w - 12, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }
  if (s_nhist > HROWS) {                           // scroll bar
    int h = 200 * HROWS / s_nhist, y = MENU_Y + (200 - h) * s_mtop / (s_nhist - HROWS);
    graphics_context_set_fill_color(ctx, GColorLightGray);
    graphics_fill_rect(ctx, GRect(SCR_W - 3, y, 2, h), 0, GCornerNone);
  }
}

static int menu_hit(int y) {
  if (y < MENU_Y) return -1;
  int k = (y - MENU_Y) / row_h_menu();
  if (k >= menu_rows()) return -1;
  int i = s_mtop + k;
  return i < menu_count() ? i : -1;
}

// ------------------------------------------------------------------ graph sheet
//
// Graph: y = f(x) (or f(x) alone), both sides of an equation in x, or the
// curve of an equation in x and y. Drag to pan, hold Up/Down (or tap + / -)
// to zoom, tap to mark a point (Up/Down move it), Select refits, Back closes.

#define GS_Y 22                       // plot top (header above)
static TokBuf s_gtok;                 // what's sampled
static TokBuf *s_gsrc;                // what was typed, for the header (heap)

static GRect gs_zoom_rect(bool in) { return GRect(SCR_W - 24, in ? GS_Y + 4 : 228 - 24, 20, 20); }

static void gs_args(GrArgs *a) { *a = (GrArgs){ s_gs, &s_gtok, &s_calc.v, &s_gfx, 0, 0, 0, 0, NULL, NULL }; }

static void gs_sample(void) { GrArgs a; gs_args(&a); graph_call(GR_SAMPLE, &a); }

static void gs_fit(bool reset) { GrArgs a; gs_args(&a); a.n = reset; graph_call(GR_VIEW, &a); }

static void graph_open(void) {
  const TokBuf *src = &s_calc.expr;
  if ((s_calc.mode == MODE_RESULT || s_calc.mode == MODE_ERROR || s_calc.mode == MODE_LIM) && s_calc.op)
    src = &s_calc.eq1;                           // the function of a d/dx or integral
  if (s_calc.mode == MODE_EQ2 && !s_calc.expr.n) src = &s_calc.eq1;   // waiting for a 2nd equation: the 1st
  if (!src->n || !s_gs || !s_gsrc) return;
  if (calc_has_z(src)) {                        // a graph has x and y only: z is an unknown it can't show
    s_calc.mode = MODE_ERROR; s_calc.err = CALC_ERR_GRAPHZ;
    buzz(60); redraw();
    return;
  }
  *s_gsrc = *src;
  GrArgs a; gs_args(&a);
  a.src = src; a.out = &s_gtok;
  if (!graph_call(GR_SETUP, &a)) { buzz(60); return; }   // what to graph; a syntax error buzzes
  GraphState *g = s_gs;
  g->x = 0; g->y = GS_Y; g->w = SCR_W; g->h = 228 - GS_Y;
  g->labels = true;
  g->trace = false;
  g->nroots = 0;
  g->tcurve = 0;
  s_sol_y = SOL_CLOSED;                          // the solver sheet shares the graph state
  s_graph_open = true;
  s_pressed = -1;
  gs_fit(true);
  redraw();
}

static void graph_close(void) {
  GrArgs a; gs_args(&a);
  graph_call(GR_FREE, &a);
  s_graph_open = false;
  redraw();
}

static void gs_zoom(double f) { GrArgs a; gs_args(&a); a.d = f; if (graph_call(GR_ZOOM, &a)) redraw(); }
static void gs_trace_at(int sx, int sy) { GrArgs a; gs_args(&a); a.sx = sx; a.sy = sy; graph_call(GR_TRACE_AT, &a); redraw(); }
static void gs_step(int d) { GrArgs a; gs_args(&a); a.n = d; graph_call(GR_STEP, &a); redraw(); }

static void draw_graph_sheet(GContext *ctx) {
  s_gfx.ctx = ctx;
  GrArgs a; gs_args(&a);
  if (!graph_call(GR_DRAW, &a)) {
    gx_fill(ctx, 0, GS_Y, SCR_W, 228 - GS_Y, 0xC0);
    sol_text(ctx, "Graph didn't load", s_f18b, GRect(0, 100, SCR_W, 24), GTextAlignmentCenter, GColorLightGray);
  }
  // header: title and what's graphed
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, GRect(0, 0, SCR_W, GS_Y), 0, GCornerNone);
  sol_text(ctx, "GRAPH", s_f14, GRect(6, 3, 50, 16), GTextAlignmentLeft, GColorMalachite);
  if (!math_fit(ctx, s_gsrc->t, s_gsrc->n, WRAP_NONE, NULL, NULL, GRect(52, 1, SCR_W - 58, GS_Y - 2), 2, NULL,
                GColorWhite, true)) {                 // too tall for the header (a fraction): one line of text
    char t[96];
    calc_render(s_gsrc, t, sizeof t);
    GFont f[2] = { s_f14, s_f14 };
    graphics_context_set_text_color(ctx, GColorWhite);
    draw_fit(ctx, t, GRect(52, 1, SCR_W - 58, GS_Y - 2), f, 2);
  }
  for (int k = 0; k < 2; k++) {                  // + (top, by Up) and - (bottom, by Down)
    GRect r = gs_zoom_rect(k == 0);
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, r, 4, GCornersAll);
    graphics_context_set_text_color(ctx, GColorWhite);
    draw_centered(ctx, k == 0 ? "+" : "-", s_f18b, r);
  }
  if (!s_gs->trace) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(2, GS_Y + 2, 136, 17), 0, GCornerNone);
    sol_text(ctx, "hold Up/Down to zoom", s_f14, GRect(5, GS_Y + 1, 140, 16), GTextAlignmentLeft, GColorDarkGray);
  }
}

// ------------------------------------------------------------------ settings from the phone
//
// Settings live on the phone: the gear next to TinySci in the Pebble app
// opens a page (src/pkjs/index.js) that sends them here. They're saved on the
// watch, so they apply without the phone.

static void save_settings(void) { persist_write_data(PERSIST_SET_KEY, &s_set, sizeof(Settings)); }

// The message keys from package.json. The build defines them (message_keys.auto.c);
// they're declared here too because its generated header can be left stale.
extern uint32_t MESSAGE_KEY_BtnUp, MESSAGE_KEY_BtnUpHold, MESSAGE_KEY_BtnDown, MESSAGE_KEY_BtnDownHold,
                MESSAGE_KEY_BtnSelect, MESSAGE_KEY_BtnSelectHold, MESSAGE_KEY_Graph, MESSAGE_KEY_Buzz,
                MESSAGE_KEY_Backlight, MESSAGE_KEY_ClearHistory, MESSAGE_KEY_Format, MESSAGE_KEY_Digits;

static void inbox(DictionaryIterator *it, void *ctx) {
  Tuple *t;
  const uint32_t K[6] = { MESSAGE_KEY_BtnUp, MESSAGE_KEY_BtnUpHold, MESSAGE_KEY_BtnDown,
                          MESSAGE_KEY_BtnDownHold, MESSAGE_KEY_BtnSelect, MESSAGE_KEY_BtnSelectHold };
  for (int i = 0; i < 6; i++)
    if ((t = dict_find(it, K[i])) && t->value->int32 >= 0 && t->value->int32 < A_COUNT)
      s_set.btn[i] = (uint8_t)t->value->int32;
  if ((t = dict_find(it, MESSAGE_KEY_Graph))) s_set.graph = (SOLVER_GRAPH && t->value->int32) ? 1 : 0;
  if ((t = dict_find(it, MESSAGE_KEY_Buzz))) s_set.buzz = (uint8_t)(t->value->int32 > 2 ? 2 : t->value->int32 < 0 ? 0 : t->value->int32);
  if ((t = dict_find(it, MESSAGE_KEY_Backlight))) s_set.light = (uint8_t)(t->value->int32 > 2 ? 2 : t->value->int32 < 0 ? 0 : t->value->int32);
  if ((t = dict_find(it, MESSAGE_KEY_ClearHistory)) && t->value->int32) s_nhist = 0;
  if ((t = dict_find(it, MESSAGE_KEY_Format)) && t->value->int32 >= 0 && t->value->int32 <= FMT_FIX) s_set.fmt = (uint8_t)t->value->int32;
  if ((t = dict_find(it, MESSAGE_KEY_Digits)) && t->value->int32 >= 0 && t->value->int32 <= 9) s_set.digits = (uint8_t)t->value->int32;
  calc_fmt = s_set.fmt; calc_fmt_digits = s_set.digits;
  calc_display(&s_calc, s_small, LINE_LEN, s_big, LINE_LEN);
  save_settings();
  apply_light();
  s_cursor = nav() ? (s_cursor < 0 ? 0 : s_cursor) : -1;
  s_sol_y = s_sol_open ? s_sol_y : SOL_CLOSED;
  redraw();
}

// ------------------------------------------------------------------ touch

#if defined(PBL_TOUCH)
enum { DRAG_TAP, DRAG_PAGE, DRAG_PULL, DRAG_PUSH, DRAG_PAN,
       DRAG_PULLUP, DRAG_PUSHDOWN, DRAG_GRAPH, DRAG_CURSOR, DRAG_MENU, DRAG_SSCROLL, DRAG_GSHEET, DRAG_XSCROLL, DRAG_NONE };
static double s_g0x0, s_g0x1, s_g0y0, s_g0y1;
static int s_tx0, s_ty0, s_down, s_drag = DRAG_NONE, s_pan_x0, s_pan_y0, s_mtop0, s_xs0;
static int iabs(int v) { return v < 0 ? -v : v; }

// Hold still on the display for a moment: a cursor appears, slide to move it.
#define HOLD_MS 350
#define CURSOR_STEP 12          // px of sliding per token
static AppTimer *s_hold_timer;
static int s_cur0;

static void tools_open(int list);
static void hold_i_cb(void *data) {
  s_hold_timer = NULL;
  if (s_drag != DRAG_TAP) return;
  s_drag = DRAG_NONE;                    // letting go doesn't type i
  s_pressed = -1;
  buzz(30);
  tools_open(TL_CPLX);
}

static void hold_cb(void *data) {
  s_hold_timer = NULL;
  if (s_drag != DRAG_TAP) return;
  if (s_calc.mode == MODE_RESULT || s_calc.mode == MODE_ERROR) calc_key(&s_calc, K_DEL);   // edit it again
  int n = s_calc.expr.n;
  s_cur0 = s_calc.cur == CALC_CUR_END ? n : s_calc.cur;
  if (!calc_set_cursor(&s_calc, s_cur0)) return;
  s_drag = DRAG_CURSOR;
  s_xfollow = true;
  s_pressed = -1;
  s_want_carets = true;                  // worked out on the next draw
  s_ncar = -1;
  buzz(30);
  redraw();
}

static void hold_cancel(void) {
  if (s_hold_timer) app_timer_cancel(s_hold_timer);
  s_hold_timer = NULL;
}

static void touch_handler(const TouchEvent *e, void *ctx) {
  int x = e->x, y = e->y, dx = x - s_tx0, dy = y - s_ty0;
  switch (e->type) {
    case TouchEvent_Touchdown:
      s_tx0 = x; s_ty0 = y;
      s_drag = DRAG_TAP;
      if (s_set.light == 1) light_enable_interaction();
      if (s_graph_open) {
        s_g0x0 = s_gs->x0; s_g0x1 = s_gs->x1; s_g0y0 = s_gs->y0; s_g0y1 = s_gs->y1;
        break;
      }
      if (s_menu) {
        s_pressed = menu_hit(y);
        s_mtop0 = s_mtop;
        redraw();
      } else if (s_sol_open) {
        s_g0x0 = s_gx0; s_g0x1 = s_gx1; s_g0y0 = s_gy0; s_g0y1 = s_gy1;
      } else if (s_sheet_open) {
        s_pan_x0 = s_pan_x; s_pan_y0 = s_pan_y;
      } else {
        s_down = hit_test(x, y);
        s_pressed = s_down;
        hold_cancel();
        if (y < PANEL_H && s_down < 0) s_hold_timer = app_timer_register(HOLD_MS, hold_cb, NULL);
        else if (s_down >= 0 && PAGES[s_page][s_down].code == T_I)      // hold i: conj, re, im, arg
          s_hold_timer = app_timer_register(HOLD_MS, hold_i_cb, NULL);
        redraw();
      }
      break;

    case TouchEvent_PositionUpdate:
      if (s_drag == DRAG_NONE) break;          // finger is up: ignore stray updates
      if (s_graph_open) {                      // pan the graph
        if (s_drag == DRAG_TAP && (iabs(dx) > 8 || iabs(dy) > 8)) s_drag = DRAG_GSHEET;
        if (s_drag != DRAG_GSHEET) break;
        GraphState *g = s_gs;
        double sx = (s_g0x1 - s_g0x0) / g->w, sy = (s_g0y1 - s_g0y0) / (g->h - 1);
        g->x0 = s_g0x0 - dx * sx; g->x1 = s_g0x1 - dx * sx;
        g->y0 = s_g0y0 + dy * sy; g->y1 = s_g0y1 + dy * sy;
        if (g->kind != GK_IMPLICIT) { gs_sample(); GrArgs a; gs_args(&a); if (g->trace) graph_call(GR_TRACE, &a); }   // a grid is resampled when you let go
        redraw();
        break;
      }
      if (s_drag == DRAG_CURSOR) {
        int n = s_calc.expr.n, pos;
        if (s_ncar == n + 1 && s_cur0 <= n) {
          // the cursor follows the finger in both directions: up/down moves
          // between the top and bottom of a fraction, into and out of powers
          int tx = s_cx[s_cur0] + dx, ty = s_cy[s_cur0] + dy, best = 0x7fffffff;
          pos = s_cur0;
          for (int p = 0; p <= n; p++) {
            if (calc_hidden_cursor(&s_calc.expr, p)) continue;
            int ex = s_cx[p] - tx, ey = s_cy[p] - ty, d = ex * ex + 2 * ey * ey;
            if (d < best) { best = d; pos = p; }
          }
        } else {
          pos = s_cur0 + dx / CURSOR_STEP;
        }
        pos = pos < 0 ? 0 : pos > n ? n : pos;
        if (calc_hidden_cursor(&s_calc.expr, pos)) pos += dx < 0 ? -1 : 1;   // step over it
        if (pos != s_calc.cur) { calc_set_cursor(&s_calc, pos); redraw(); }
        break;
      }
      if (s_menu) {                            // scroll the list; a sideways swipe closes it
        if (s_drag == DRAG_TAP && (iabs(dx) > 14 || iabs(dy) > 14)) {
          s_drag = iabs(dy) > iabs(dx) ? DRAG_MENU : DRAG_PAGE;
          s_pressed = -1;
          redraw();
        }
        if (s_drag == DRAG_MENU) {
          int maxtop = menu_count() - menu_rows();
          int t = clampi(s_mtop0 - dy / (row_h_menu() / 2), 0, maxtop > 0 ? maxtop : 0);
          if (t != s_mtop) { s_mtop = t; redraw(); }
        }
        break;
      }
      if (s_drag == DRAG_TAP && (iabs(dx) > 8 || iabs(dy) > 8)) hold_cancel();   // not holding still
      if (s_drag == DRAG_TAP && (iabs(dx) > 14 || iabs(dy) > 14)) {
        bool vertical = iabs(dy) > iabs(dx);
        if (s_sol_open) {
#if SOLVER_GRAPH
          if (graph_on() && s_ty0 >= G_Y && s_ty0 < G_Y + G_H && s_sol_has_x) s_drag = DRAG_GRAPH;
          else
#endif
          if (vertical && !graph_on() && sol_max_scroll() && (dy < 0 || s_sscroll > 0)) {
            s_drag = DRAG_SSCROLL;          // scroll the sheet (down at the top closes it)
            s_pan_y0 = s_sscroll;
          } else if (vertical && dy > 0) s_drag = DRAG_PUSHDOWN;
          else s_drag = DRAG_PAN;   // nothing to do
        } else if (!s_sheet_open) {
          s_pressed = -1;
          if (!vertical && s_ty0 < PANEL_H && s_xmax > 0 &&
              (s_calc.mode == MODE_EDIT || s_calc.mode == MODE_EQ2)) {   // a long line: slide it along
            s_drag = DRAG_XSCROLL;
            s_xs0 = s_xscroll;
          } else if (vertical && dy > 0 && s_ty0 < 57 && s_page < N_KEYPADS && s_sol_y >= SOL_CLOSED) {   // top quarter only
            s_drag = DRAG_PULL;
            if (s_sheet_y <= SHEET_CLOSED) sheet_prepare();
            s_sheet_target = SHEET_CLOSED;   // finger is in charge now
          } else if (vertical && dy < 0 && (s_page < N_KEYPADS || s_page == MORE_PAGE) && s_sheet_y <= SHEET_CLOSED) {
            s_drag = DRAG_PULLUP;
          } else if (vertical && dy > 0 && s_page >= N_KEYPADS) {
            s_drag = DRAG_PAGE;     // swipe down on the solver keys: back to the keypad
          } else if (vertical) {
            s_drag = DRAG_PAN;      // a vertical drag lower down does nothing
          } else {
            s_drag = DRAG_PAGE;
          }
          redraw();
        } else {
          // Push up closes unless the formula needs vertical dragging
          // (then only the bottom handle area closes it).
          bool from_handle = s_ty0 >= RES_Y;
          if (vertical && dy < 0 && (!pan_max_y() || from_handle)) s_drag = DRAG_PUSH;
          else s_drag = DRAG_PAN;
        }
      }
      if (s_drag == DRAG_XSCROLL) {
        s_xfollow = false;
        s_xscroll = clampi(s_xs0 + dx, 0, s_xmax);
        redraw();
      } else if (s_drag == DRAG_SSCROLL) {
        s_sscroll = clampi(s_pan_y0 - dy, 0, sol_max_scroll());
        redraw();
      } else if (s_drag == DRAG_PUSHDOWN) {
        s_sol_y = clampi(dy * 3 / 2, 0, SOL_CLOSED);
        redraw();
#if SOLVER_GRAPH
      } else if (s_drag == DRAG_GRAPH) {
        double sx = (s_g0x1 - s_g0x0) / G_W, sy = (s_g0y1 - s_g0y0) / (G_H - 1);
        s_gx0 = s_g0x0 - dx * sx; s_gx1 = s_g0x1 - dx * sx;
        s_gy0 = s_g0y0 + dy * sy; s_gy1 = s_g0y1 + dy * sy;
        s_roots_stale = true;
        sol_range_text();
        sol_sample();
        redraw();
#endif
      } else if (s_drag == DRAG_PULL) {
        s_sheet_y = clampi(SHEET_CLOSED + dy * 3 / 2, SHEET_CLOSED, 0);
        redraw();
      } else if (s_drag == DRAG_PUSH) {
        s_sheet_y = clampi(dy * 3 / 2, SHEET_CLOSED, 0);
        redraw();
      } else if (s_drag == DRAG_PAN) {
        s_pan_x = clampi(s_pan_x0 - dx, 0, pan_max_x());
        s_pan_y = clampi(s_pan_y0 - dy, 0, pan_max_y());
        redraw();
      }
      break;

    case TouchEvent_Liftoff: {
      if (s_drag == DRAG_NONE) break;
      int drag = s_drag;
      s_drag = DRAG_NONE;                        // until the next touchdown
      if (s_graph_open) {
        GRect zi = gs_zoom_rect(true), zo = gs_zoom_rect(false);
        zi = grect_inset(zi, GEdgeInsets(-5)); zo = grect_inset(zo, GEdgeInsets(-5));
        if (drag == DRAG_TAP) {
          if (in_rect(zi, x, y)) gs_zoom(0.5);
          else if (in_rect(zo, x, y)) gs_zoom(2.0);
          else if (y >= GS_Y) gs_trace_at(x, y);
        } else if (drag == DRAG_GSHEET && s_gs->kind == GK_IMPLICIT) {
          gs_sample();
          redraw();
        }
        break;
      }
      if (s_menu) {
        int was = s_pressed;
        s_pressed = -1;
        if (drag == DRAG_TAP && was >= 0 && menu_hit(y) == was) menu_press(was);
        else if (drag == DRAG_PAGE && iabs(dx) >= 36) menu_back();
        redraw();
        break;
      }
      s_pressed = -1;
      hold_cancel();
      switch (drag) {
        case DRAG_CURSOR:                        // let go at the end: back to normal typing
          if (s_calc.cur != CALC_CUR_END && s_calc.cur >= s_calc.expr.n) s_calc.cur = CALC_CUR_END;
          break;
        case DRAG_TAP:
          if (s_sol_open) {
            GRect zo = zoom_rect(false), zi = zoom_rect(true);
            zo.origin.x -= 4; zo.size.w += 8; zo.origin.y -= 4; zo.size.h += 8;   // easier to hit
            zi.origin.x -= 4; zi.size.w += 8; zi.origin.y -= 4; zi.size.h += 8;
            if (s_sol_has_x && in_rect(zo, x, y)) { sol_zoom(2.0); return; }
            if (s_sol_has_x && in_rect(zi, x, y)) { sol_zoom(0.5); return; }
            int ly = list_y();
            if (y >= 21 && y + sscroll() >= ly && !s_finding && !s_roots_stale) {   // an answer row
              int k = (y + sscroll() - ly) / row_h(), i = s_list_top + k;
              if (i < s_nroots && k < n_rows()) { sol_take(i); return; }
            }
#if SOLVER_GRAPH
            if (graph_on() && y >= G_Y && y < G_Y + G_H && !s_roots_stale) {   // a marker
              for (int i = 0; i < s_nroots; i++)
                if (iabs(gpx(s_roots[i]) - x) <= 10) { s_sel = i; sol_select(0); return; }
            }
#endif
          } else if (s_sheet_open) sheet_close();
          else if (s_down < 0 && y < PANEL_H && s_calc.cur != CALC_CUR_END) s_calc.cur = CALC_CUR_END;   // tap the display: cursor off
          else if (hit_test(x, y) == s_down) { press_slot(s_down); return; }
          break;
        case DRAG_PAGE:
          if (s_page >= N_KEYPADS && dy >= 36 && dy > iabs(dx)) {
            set_page(s_page == MORE_PAGE ? 1 : s_page == TOOLS_PAGE ? MORE_PAGE : 0);
            return;
          }
          if (iabs(dx) >= 36 && iabs(dx) * 5 > iabs(dy) * 6) {
            if (s_page >= N_KEYPADS) set_page(0);                            // back to the 123 keypad
            else set_page((s_page + (dx < 0 ? 1 : N_KEYPADS - 1)) % N_KEYPADS);
            return;
          }
          break;
        case DRAG_PULL:
          if (s_sheet_y > SHEET_CLOSED + 50) sheet_open(); else sheet_close();
          break;
        case DRAG_PULLUP:                     // switch straight to the solver keys
          if (dy <= -36) { set_page(s_page == 1 ? MORE_PAGE : s_page == MORE_PAGE ? TOOLS_PAGE : SOLVER_PAGE); return; }
          break;
        case DRAG_PUSHDOWN:
          if (s_sol_y > 50) sol_close(); else { s_sol_target = 0; anim_start(); }
          break;
#if SOLVER_GRAPH
        case DRAG_GRAPH:
          s_expand = 0;
          s_fit = false;
          sol_schedule_find();
          break;
#endif
        case DRAG_PUSH:
          if (s_sheet_y < -50) sheet_close(); else sheet_animate(0);
          break;
        default:
          break;
      }
      redraw();
      break;
    }

    default:
      break;
  }
}
#endif

// ------------------------------------------------------------------ buttons

static void cursor_move(int d) {
  for (int n = 0; n < 32; n++) {
    s_cursor += d;
    if (s_cursor > 15) { s_cursor = 0; s_page = (s_page + 1) % N_PAGES; }
    if (s_cursor < 0) { s_cursor = 15; s_page = (s_page + N_PAGES - 1) % N_PAGES; }
    if (PAGES[s_page][s_cursor].span > 0) break;
  }
  redraw();
}

static void poke(void) { if (s_set.light == 1) light_enable_interaction(); }

// The calculator screen: run the action set for this button on the phone.
static void do_action(int b) {
  int a = s_set.btn[b];
  if (!s_touch) {                          // touch off: Up/Down/Select must reach the keys
    if (b == B_UP) a = A_KEY_PREV;
    if (b == B_DOWN) a = A_KEY_NEXT;
    if (b == B_SELECT) a = A_KEY_PRESS;
  }
  uint32_t bz = s_set.buzz == 2 ? 40 : KEY_BUZZ_MS;
  switch (a) {
    case A_NEXT_PAGE: set_page(s_page + 1); break;
    case A_PREV_PAGE: set_page(s_page - 1); break;
    case A_EQ: buzz(bz); press_code(K_EVAL); break;
    case A_AC: buzz(bz); press_code(K_AC); break;
    case A_DEL: buzz(bz); press_code(K_DEL); break;
    case A_FORMULA: sheet_open(); break;
    case A_HISTORY: menu_open(MENU_HIST); break;
    case A_SOLVER_KEYS: set_page(s_page == SOLVER_PAGE ? 0 : SOLVER_PAGE); break;
    case A_KEY_PREV: if (s_cursor < 0) s_cursor = 0; cursor_move(-1); break;
    case A_KEY_NEXT: if (s_cursor < 0) s_cursor = 0; cursor_move(1); break;
    case A_KEY_PRESS: if (s_cursor >= 0) press_slot(s_cursor); break;
    case A_ANS: buzz(bz); press_code(T_ANS); break;
    default: break;
  }
}

static void up_click(ClickRecognizerRef r, void *ctx) {
  poke();
  if (s_graph_open) { gs_step(-1); return; }
  if (s_menu) { menu_select(-1); return; }
  if (s_sol_open) { sol_select(-1); return; }
  if (s_sheet_open) { if (pan_max_x() || pan_max_y()) sheet_pan(-1); else sheet_close(); return; }
  do_action(B_UP);
}
static void down_click(ClickRecognizerRef r, void *ctx) {
  poke();
  if (s_graph_open) { gs_step(1); return; }
  if (s_menu) { menu_select(1); return; }
  if (s_sol_open) { sol_select(1); return; }
  if (s_sheet_open) { sheet_pan(1); return; }
  do_action(B_DOWN);
}
static void down_long(ClickRecognizerRef r, void *ctx) {
  if (s_graph_open) { gs_zoom(2.0); return; }
  if (s_sol_open) { if (s_sol_has_x) sol_zoom(2.0); return; }
  if (s_menu) { menu_select(menu_rows()); return; }   // a page down
  if (s_sheet_open) return;
  do_action(B_DOWN_HOLD);
}
static void up_long(ClickRecognizerRef r, void *ctx) {
  if (s_graph_open) { gs_zoom(0.5); return; }
  if (s_sol_open) { if (s_sol_has_x) sol_zoom(0.5); return; }
  if (s_menu) { menu_select(-menu_rows()); return; }  // a page up
  if (s_sheet_open) return;
  do_action(B_UP_HOLD);
}
static void select_click(ClickRecognizerRef r, void *ctx) {
  poke();
  if (s_graph_open) { gs_fit(s_gs->kind == GK_IMPLICIT); redraw(); return; }   // refit the view
  if (s_menu) {                                  // Select picks the highlighted row (first press: shows it)
    if (!nav() && !s_mkeys) { s_mkeys = true; s_msel = s_mtop; redraw(); return; }
    menu_press(s_msel);
    return;
  }
  if (s_sol_open) { if (s_nroots && !s_finding && !s_roots_stale) sol_take(s_sel); else sol_close(); return; }
  if (s_sheet_open) { sheet_close(); return; }
  do_action(B_SELECT);
}
static void select_long(ClickRecognizerRef r, void *ctx) {
  if (s_sheet_open || s_sol_open || s_menu || s_graph_open) return;
  do_action(B_SELECT_HOLD);
}
static void back_click(ClickRecognizerRef r, void *ctx) {
  poke();
  if (s_graph_open) { graph_close(); return; }
  if (s_menu) { menu_back(); return; }
  if (s_sol_open) { sol_close(); return; }
  if (s_sheet_open) { sheet_close(); return; }
  // Back is always DEL (on an answer it brings the expression back to edit);
  // it only exits when there's nothing left. Hold Back to exit any time.
  if (s_calc.mode == MODE_EDIT && s_calc.expr.n == 0) { window_stack_pop(true); return; }
  press_code(K_DEL);
}
static void back_long(ClickRecognizerRef r, void *ctx) { window_stack_pop(true); }

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_long_click_subscribe(BUTTON_ID_UP, 500, up_long, NULL);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_long_click_subscribe(BUTTON_ID_DOWN, 500, down_long, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 600, select_long, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
  window_long_click_subscribe(BUTTON_ID_BACK, 700, back_long, NULL);
}

// ------------------------------------------------------------------ window

static void win_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, update_proc);
  layer_add_child(root, s_layer);
}
static void win_unload(Window *w) { layer_destroy(s_layer); }

static void win_appear(Window *w) {
#if defined(PBL_TOUCH)
  s_touch = touch_service_is_enabled();
  if (s_touch) touch_service_subscribe(touch_handler, NULL);
#else
  s_touch = false;
#endif
  s_cursor = nav() ? 0 : -1;
  redraw();
}
static void win_disappear(Window *w) {
#if defined(PBL_TOUCH)
  if (s_touch) touch_service_unsubscribe();
#endif
}

#define CHUNK 256

static bool read_chunks(uint32_t key, void *p, int size) {
  for (int off = 0, k = 0; off < size; off += CHUNK, k++) {
    int n = size - off < CHUNK ? size - off : CHUNK;
    if (persist_get_size(key + k) != n) return false;
    persist_read_data(key + k, (uint8_t *)p + off, n);
  }
  return true;
}

static void write_chunks(uint32_t key, const void *p, int size) {
  for (int off = 0, k = 0; off < size; off += CHUNK, k++) {
    int n = size - off < CHUNK ? size - off : CHUNK;
    persist_write_data(key + k, (const uint8_t *)p + off, n);
  }
}

static void load_state(void) {
  calc_init(&s_calc);
  if (persist_exists(PERSIST_VER_KEY) && persist_read_int(PERSIST_VER_KEY) == PERSIST_VERSION) {
    if (!read_chunks(PERSIST_CALC_KEY, &s_calc, sizeof(Calc)) ||
        s_calc.expr.n > CALC_MAX_TOK || s_calc.eq1.n > CALC_MAX_TOK ||
        s_calc.mode > MODE_LIM)
      calc_init(&s_calc);
  }
  s_page = persist_exists(PERSIST_PAGE_KEY) ? persist_read_int(PERSIST_PAGE_KEY) : 0;
  if (s_page < 0 || s_page >= N_PAGES) s_page = 0;
  int ssz = persist_get_size(PERSIST_SET_KEY);
  if (ssz > 0 && ssz <= (int)sizeof(Settings)) {     // older, shorter settings keep the new defaults
    persist_read_data(PERSIST_SET_KEY, &s_set, ssz);
    if (s_set.fmt > FMT_FIX) s_set.fmt = FMT_NORM;
    if (s_set.digits > 9) s_set.digits = 4;
    if (s_set.buzz > 2) s_set.buzz = 1;
    if (s_set.light > 2) s_set.light = 0;
    for (int i = 0; i < 6; i++) if (s_set.btn[i] >= A_COUNT) s_set.btn[i] = A_NOTHING;
    if (!SOLVER_GRAPH) s_set.graph = 0;
  }
  s_nhist = 0;
  if (persist_exists(PERSIST_HIST_KEY)) {
    int n = persist_read_int(PERSIST_HIST_KEY);
    if (n > 0 && n <= HIST_N && read_chunks(PERSIST_HIST_KEY + 1, s_hist, n * (int)sizeof(Hist))) {
      s_nhist = n;
      for (int i = 0; i < n; i++)
        if (s_hist[i].t.n > CALC_MAX_TOK || HK(&s_hist[i]) > H_SYS) s_nhist = 0;
    }
  }
  if (s_stat && persist_exists(PERSIST_STAT_KEY) &&
      (!read_chunks(PERSIST_STAT_KEY, s_stat, sizeof(StatData)) || s_stat->n > STAT_N)) s_stat->n = 0;
  calc_fmt = s_set.fmt; calc_fmt_digits = s_set.digits;
}

static void save_stats(void) {
  if (s_stat) write_chunks(PERSIST_STAT_KEY, s_stat, sizeof(StatData));
}

static void save_state(void) {
  persist_write_int(PERSIST_VER_KEY, PERSIST_VERSION);
  write_chunks(PERSIST_CALC_KEY, &s_calc, sizeof(Calc));
  persist_write_int(PERSIST_PAGE_KEY, s_page);
  save_settings();
  persist_write_int(PERSIST_HIST_KEY, s_nhist);
  if (s_nhist) write_chunks(PERSIST_HIST_KEY + 1, s_hist, s_nhist * (int)sizeof(Hist));
  save_stats();
}


static void init(void) {
  calc_rng ^= (uint32_t)time(NULL) * 2654435761u;   // Ran#: a different run each time
  if (!calc_rng) calc_rng = 1;
  s_f14 = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  s_f18 = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  s_f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_f24b = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  s_f28b = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  // Buffers go on the heap: code + static data must stay under 64 KB.
  s_hist = calloc(HIST_N, sizeof(Hist));
  s_stat = calloc(1, sizeof(StatData));
  s_alt_tok = calloc(1, sizeof(TokBuf));
  s_small = calloc(1, LINE_LEN);
  s_big = calloc(1, LINE_LEN);
  load_state();
  apply_light();

  app_message_register_inbox_received(inbox);
  app_message_open(128, 0);              // settings from the phone; nothing is sent back
  s_doc_mem = malloc(sizeof(MathDoc));
#ifdef SIM
  s_doc2 = malloc(sizeof(MathDoc));
#else
  s_doc2 = MATH_SCRATCH;                 // comes and goes with the typesetter
#endif
  s_exact = malloc(sizeof(Exact));
  s_gs = calloc(1, sizeof(GraphState));
  s_gsrc = calloc(1, sizeof(TokBuf));
  s_cx = malloc(MAX_CARETS * sizeof(int16_t));
  s_cy = malloc(MAX_CARETS * sizeof(int16_t));
  s_gs->ca = malloc(GR_COLS * sizeof(float));
  s_gs->cb = malloc(GR_COLS * sizeof(float));
  s_io = (SheetIO){ .s = &S, .calc = &s_calc, .doc = s_doc_mem, .exact = s_exact, .doc_tok = &s_doc_tok,
                    .sys_tok = s_sys_tok, .sys_cap = (int)sizeof s_sys_tok, .bn = -1 };
#if SOLVER_GRAPH
  s_io.aspect = (double)(G_H - 1) / G_W;
#endif
#if defined(PBL_TOUCH)
  app_touch_navigation_enable(false);   // we read raw touches ourselves
#endif
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = win_load, .unload = win_unload, .appear = win_appear, .disappear = win_disappear,
  });
  window_stack_push(s_window, true);
#ifndef SIM
  // The typesetter, last: above everything the app keeps, so when it makes
  // way for a big overlay the free memory is one piece.
  fmt_overlay_load();
#endif
}

static void deinit(void) {
  save_state();
  light_enable(false);
  free(s_doc_mem);
  free(s_hist); free(s_small); free(s_big); free(s_stat); free(s_alt_tok);
#ifdef SIM
  free(s_doc2);
#endif
  free(s_exact);
  free(s_gs->grid); free(s_gs->ca); free(s_gs->cb); free(s_gs); free(s_gsrc); free(s_cx); free(s_cy);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
