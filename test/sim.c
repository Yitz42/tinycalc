// PC simulator for main.c: fakes the Pebble API, replays touches/buttons
// from argv, and prints the final frame's draw calls for test/render.py.
//   sim "tap 25 85" "swipe 150 120 30 120" "btn select" ... "frame"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "pebble.h"

struct Layer { GRect frame; LayerUpdateProc proc; };
struct Window { WindowHandlers h; ClickConfigProvider ccp; Layer root; };
static Window g_win;
static Layer g_layers[4]; static int g_nlayers;
static TouchServiceHandler g_touch; static void *g_touch_ctx;
static ClickHandler g_single[4], g_long[4];
static AppTimerCallback g_tcb[16]; static void *g_tdata[16];
static int g_timer;  // any pending
static int g_popped;
static unsigned char g_persist[64][256]; static int g_psize[64];

static int c_hex(GColor c) {
  int r = (c.argb >> 4) & 3, g = (c.argb >> 2) & 3, b = c.argb & 3;
  return (r * 85) << 16 | (g * 85) << 8 | (b * 85);
}
static int g_fill, g_stroke, g_text, g_sw = 1;

GFont fonts_get_system_font(const char *k) { return (GFont)k; }
void graphics_context_set_fill_color(GContext *c, GColor col) { g_fill = c_hex(col); }
void graphics_context_set_stroke_color(GContext *c, GColor col) { g_stroke = c_hex(col); }
void graphics_context_set_text_color(GContext *c, GColor col) { g_text = c_hex(col); }
void graphics_context_set_stroke_width(GContext *c, uint8_t w) { g_sw = w; }
void graphics_fill_rect(GContext *c, GRect r, uint16_t rad, GCornerMask m) {
  printf("fill %d %d %d %d %d %06x\n", r.origin.x, r.origin.y, r.size.w, r.size.h, rad, g_fill);
}
void graphics_draw_round_rect(GContext *c, GRect r, uint16_t rad) {
  printf("rrect %d %d %d %d %d %06x %d\n", r.origin.x, r.origin.y, r.size.w, r.size.h, rad, g_stroke, g_sw);
}
void graphics_draw_rect(GContext *c, GRect r) {
  printf("rrect %d %d %d %d 0 %06x 1\n", r.origin.x, r.origin.y, r.size.w, r.size.h, g_stroke);
}
void graphics_draw_line(GContext *c, GPoint a, GPoint b) {
  printf("line %d %d %d %d %06x %d\n", a.x, a.y, b.x, b.y, g_stroke, g_sw);
}
void graphics_fill_circle(GContext *c, GPoint p, uint16_t r) { printf("fcircle %d %d %d %06x\n", p.x, p.y, r, g_fill); }
void graphics_draw_circle(GContext *c, GPoint p, uint16_t r) { printf("circle %d %d %d %06x\n", p.x, p.y, r, g_stroke); }

static int font_size(GFont f) {
  const char *k = (const char *)f;
  int n = atoi(strstr(k, "GOTHIC_") + 7);
  return n;
}
// Advance widths of the preview font (test/fontw.h), so previews line up.
#include "fontw.h"
static int text_width(const char *t, GFont f) {
  int sz = font_size(f), w = 0;
  bool bold = strstr((const char *)f, "BOLD") != NULL;
  const int *tab = bold ? (sz == 28 ? FW_B28 : sz == 24 ? FW_B24 : sz == 18 ? FW_B18 : FW_B14)
                        : (sz == 28 ? FW_R28 : sz == 24 ? FW_R24 : sz == 18 ? FW_R18 : FW_R14);
  const unsigned char *p = (const unsigned char *)t;
  while (*p) {
    int cp = *p++;
    if (cp >= 0xC0) { cp = ((cp & 0x1F) << 6) | (*p & 0x3F); p++; }
    int cw = sz / 2;
    for (unsigned i = 0; i < sizeof FW_CPS / sizeof FW_CPS[0]; i++) if (FW_CPS[i] == cp) cw = tab[i];
    w += cw;
  }
  return w;
}
static int g_nonascii;   // text the watch fonts can't show (they're ASCII-only)
void graphics_draw_text(GContext *c, const char *t, const GFont f, const GRect r,
                        const GTextOverflowMode o, const GTextAlignment a, GTextAttributes *x) {
  for (const unsigned char *p = (const unsigned char *)t; *p; p++)
    if (*p >= 0x80) { fprintf(stderr, "NON-ASCII TEXT: %s\n", t); g_nonascii = 1; break; }
  printf("text %d %d %d %d %d %d %06x %s %s\n", r.origin.x, r.origin.y, r.size.w, r.size.h,
         font_size(f), (int)a, g_text, strstr((const char *)f, "BOLD") ? "B" : "R", t);
}
GSize graphics_text_layout_get_content_size(const char *t, const GFont f, const GRect b,
                                            const GTextOverflowMode o, const GTextAlignment a) {
  GSize s = { (int16_t)text_width(t, f), (int16_t)font_size(f) };
  return s;
}
GRect grect_inset(GRect r, GEdgeInsets i) {
  return GRect(r.origin.x + i.left, r.origin.y + i.top, r.size.w - i.left - i.right, r.size.h - i.top - i.bottom);
}
Layer *layer_create(GRect f) { Layer *l = &g_layers[g_nlayers++]; l->frame = f; return l; }
void layer_destroy(Layer *l) {}
void layer_set_update_proc(Layer *l, LayerUpdateProc p) { l->proc = p; }
void layer_add_child(Layer *p, Layer *c) {}
void layer_mark_dirty(Layer *l) {}
GRect layer_get_bounds(const Layer *l) { return GRect(0, 0, l->frame.size.w, l->frame.size.h); }
Window *window_create(void) { g_win.root.frame = GRect(0, 0, 200, 228); return &g_win; }
void window_destroy(Window *w) {}
void window_set_background_color(Window *w, GColor c) {}
void window_set_click_config_provider(Window *w, ClickConfigProvider p) { w->ccp = p; }
void window_set_window_handlers(Window *w, WindowHandlers h) { w->h = h; }
Layer *window_get_root_layer(const Window *w) { return (Layer *)&w->root; }
void window_stack_push(Window *w, bool a) { if (w->ccp) w->ccp(NULL); w->h.load(w); w->h.appear(w); }
Window *window_stack_pop(bool a) { g_popped = 1; return &g_win; }
void window_single_click_subscribe(ButtonId b, ClickHandler h) { g_single[b] = h; }
void window_long_click_subscribe(ButtonId b, uint16_t d, ClickHandler h, ClickHandler u) { g_long[b] = h; }
AppTimer *app_timer_register(uint32_t ms, AppTimerCallback cb, void *d) {
  for (int i = 0; i < 16; i++) if (!g_tcb[i]) { g_tcb[i] = cb; g_tdata[i] = d; g_timer = 1; return (AppTimer *)(intptr_t)(i + 1); }
  return NULL;
}
void app_event_loop(void) {}
void app_timer_cancel(AppTimer *t) { int i = (int)(intptr_t)t - 1; if (i >= 0 && i < 16) g_tcb[i] = NULL; }
bool persist_exists(const uint32_t k) { return g_psize[k] > 0; }
int persist_get_size(const uint32_t k) { return g_psize[k]; }
int32_t persist_read_int(const uint32_t k) { int32_t v; memcpy(&v, g_persist[k], 4); return v; }
status_t persist_write_int(const uint32_t k, const int32_t v) { memcpy(g_persist[k], &v, 4); g_psize[k] = 4; return 4; }
int persist_read_data(const uint32_t k, void *b, const size_t n) { memcpy(b, g_persist[k], n); return (int)n; }
int persist_write_data(const uint32_t k, const void *d, const size_t n) { memcpy(g_persist[k], d, n); g_psize[k] = (int)n; return (int)n; }
static int g_touch_on = 1;
void touch_service_subscribe(TouchServiceHandler h, void *c) { g_touch = h; g_touch_ctx = c; }
void touch_service_unsubscribe(void) { g_touch = NULL; }
bool touch_service_is_enabled(void) { return g_touch_on; }
void app_touch_navigation_enable(bool e) {}
void vibes_enqueue_custom_pattern(VibePattern p) { printf("VIBE %u\n", (unsigned)p.durations[0]); }
void light_enable(bool e) { printf("LIGHT %d\n", e); }
void light_enable_interaction(void) { printf("LIGHTPOKE\n"); }

// AppMessage: "cfg Graph 1" delivers one setting as if sent from the phone.
static AppMessageInboxReceived g_inbox;
// Like the build's message_keys.auto.c: variables, numbered in package.json order.
uint32_t MESSAGE_KEY_BtnUp = 10000, MESSAGE_KEY_BtnUpHold = 10001, MESSAGE_KEY_BtnDown = 10002,
         MESSAGE_KEY_BtnDownHold = 10003, MESSAGE_KEY_BtnSelect = 10004, MESSAGE_KEY_BtnSelectHold = 10005,
         MESSAGE_KEY_Graph = 10006, MESSAGE_KEY_Buzz = 10007, MESSAGE_KEY_Backlight = 10008,
         MESSAGE_KEY_ClearHistory = 10009, MESSAGE_KEY_Format = 10010, MESSAGE_KEY_Digits = 10011;
static Tuple g_tuple;
struct DictionaryIterator { int dummy; };
AppMessageInboxReceived app_message_register_inbox_received(AppMessageInboxReceived cb) { g_inbox = cb; return cb; }
int app_message_open(const uint32_t a, const uint32_t b) { return 0; }
Tuple *dict_find(const DictionaryIterator *it, const uint32_t key) { return g_tuple.key == key ? &g_tuple : NULL; }

int pebble_main(void);
static void touch(TouchEventType t, int x, int y) {
  TouchEvent e = { t, (int16_t)x, (int16_t)y, false };
  if (g_touch) g_touch(&e, g_touch_ctx);
}
static void run_timer(void) {   // fire every pending timer once
  AppTimerCallback cb[16]; void *d[16];
  for (int i = 0; i < 16; i++) { cb[i] = g_tcb[i]; d[i] = g_tdata[i]; g_tcb[i] = NULL; }
  for (int i = 0; i < 16; i++) if (cb[i]) cb[i](d[i]);
  g_timer = 0;
  for (int i = 0; i < 16; i++) if (g_tcb[i]) g_timer = 1;
}
static void frame(void) {
  printf("FRAME\n");
  g_layers[0].proc(&g_layers[0], NULL);
  printf("END\n");
}

// Key centre on the keypad: row r, col c
int main(int argc, char **argv) {
  if (getenv("NOTOUCH")) g_touch_on = 0;
  // run init only: pebble_main runs init, loop (no-op), deinit
  extern void sim_init(void);
  sim_init();
  for (int i = 1; i < argc; i++) {
    int a, b, c, d;
    char w[32];
    if (sscanf(argv[i], "tap %d %d", &a, &b) == 2) {
      touch(TouchEvent_Touchdown, a, b); touch(TouchEvent_Liftoff, a, b);
    } else if (sscanf(argv[i], "key %d %d", &a, &b) == 2) {   // row col on keypad
      int x = b * 50 + 25, y = 64 + a * 41 + 20;
      touch(TouchEvent_Touchdown, x, y); touch(TouchEvent_Liftoff, x, y);
    } else if (sscanf(argv[i], "down %d %d", &a, &b) == 2) {
      touch(TouchEvent_Touchdown, a, b);
    } else if (sscanf(argv[i], "swipe %d %d %d %d", &a, &b, &c, &d) == 4) {
      touch(TouchEvent_Touchdown, a, b);
      for (int s = 1; s <= 4; s++) touch(TouchEvent_PositionUpdate, a + (c - a) * s / 4, b + (d - b) * s / 4);
      touch(TouchEvent_Liftoff, c, d);
    } else if (sscanf(argv[i], "btn %31s", w) == 1) {
      int id = !strcmp(w, "up") ? BUTTON_ID_UP : !strcmp(w, "down") ? BUTTON_ID_DOWN :
               !strcmp(w, "select") ? BUTTON_ID_SELECT : BUTTON_ID_BACK;
      g_single[id](NULL, NULL);
    } else if (sscanf(argv[i], "long %31s", w) == 1) {
      int id = !strcmp(w, "select") ? BUTTON_ID_SELECT : !strcmp(w, "down") ? BUTTON_ID_DOWN :
               !strcmp(w, "up") ? BUTTON_ID_UP : BUTTON_ID_BACK;
      g_long[id](NULL, NULL);
    } else if (sscanf(argv[i], "k %d", &a) == 1) {          // press a key code directly
      extern void sim_key(int); sim_key(a);
    } else if (sscanf(argv[i], "pull %d", &a) == 1) {       // drag down a px, keep finger down
      touch(TouchEvent_Touchdown, 100, 30);
      for (int s = 1; s <= 6; s++) touch(TouchEvent_PositionUpdate, 100, 30 + a * s / 6);
    } else if (sscanf(argv[i], "lift %d %d", &a, &b) == 2) {
      touch(TouchEvent_Liftoff, a, b);
    } else if (sscanf(argv[i], "move %d %d", &a, &b) == 2) {
      touch(TouchEvent_PositionUpdate, a, b);
    } else if (sscanf(argv[i], "cfg %31s %d", w, &a) == 2) {
      static const char *names[] = { "BtnUp", "BtnUpHold", "BtnDown", "BtnDownHold", "BtnSelect",
                                     "BtnSelectHold", "Graph", "Buzz", "Backlight", "ClearHistory",
                                     "Format", "Digits" };
      for (int k = 0; k < 12; k++) if (!strcmp(w, names[k])) g_tuple.key = 10000 + k;
      g_tuple.value[0].int32 = a;
      struct DictionaryIterator it;
      if (g_inbox) g_inbox(&it, NULL);
    } else if (!strcmp(argv[i], "state")) {
      extern void sim_state(void); sim_state();
    } else if (!strcmp(argv[i], "anim")) {
      for (int n = 0; n < 400 && g_timer; n++) run_timer();
    } else if (!strcmp(argv[i], "timer")) {
      run_timer();
    } else if (!strcmp(argv[i], "frame")) {
      frame();
    }
  }
  printf("POPPED %d\n", g_popped);
  return g_nonascii ? 3 : 0;
}
