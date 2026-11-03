// ovl_graph.c - the "graph" overlay: graph sampling and drawing (graph.c),
// with the parser (calc.c) and math functions (mmath.c) it uses.
#include "../c/ovl_api.h"
#include "../c/ovl_graph.h"
#include "ovl_rt.h"

int graph_run(int cmd, GrArgs *a);

int ovl_main(const OvlApi *api, int cmd, void *args) {
  if (!api || api->version != OVL_API_VERSION) return 0;
  ovl_rt_init(api);
  return graph_run(cmd, args);
}
