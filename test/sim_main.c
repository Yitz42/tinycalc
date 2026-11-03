#define main pebble_main
#include "../src/c/main.c"
void sim_init(void) { init(); }
void sim_key(int k) { press_code(k); }
int printf(const char *fmt, ...);
void sim_state(void) {   // the expression (with cursor) and the two display lines as text
  char e[300], sm[200], bg[200];
  calc_render_cur(&s_calc.expr, s_calc.cur, e, sizeof e);
  calc_display(&s_calc, sm, sizeof sm, bg, sizeof bg);
  printf("STATE [%s] [%s] [%s]\n", e, sm, bg);
}
// The graph overlay's code, linked directly (no overlays on the PC).
#include "../src/c/ovl_graph.h"
int graph_run(int cmd, GrArgs *a);
bool graph_call(int cmd, void *args) { return graph_run(cmd, args) != 0; }
