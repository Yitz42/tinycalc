// ovl_tools.c - the "tools" overlay: the TOOLS keypad's lists (tools.c), with
// the number formatting and parser bits they use. Loaded while a list is open.
#include "../c/ovl_api.h"
#include "../c/ovl_tools.h"
#include "ovl_rt.h"

int ovl_main(const OvlApi *api, int cmd, void *args) {
  if (!api || api->version != OVL_API_VERSION) return 0;
  ovl_rt_init(api);
  OtArgs *a = args;
  switch (cmd) {
    case OT_COUNT: a->ret = tl_count(a->io, a->list); return 1;
    case OT_ROW: tl_row(a->io, a->list, a->i); return 1;
    case OT_PICK: a->ret = tl_pick(a->io, a->list, a->i); return 1;
  }
  return 0;
}
