// ovl.c - overlay loader. Copies the image from the resource onto the heap,
// adds the load address to each absolute address listed after it, and
// returns its entry point (Thumb). See ovl_api.h and tools/build_overlay.py.
#include <pebble.h>
#include "ovl.h"

bool ovl_load(Ovl *o, uint32_t res_id) {
  o->mem = NULL;
  o->entry = NULL;
  ResHandle h = resource_get_handle(res_id);
  if (!h) return false;
  size_t total = resource_size(h);
  OvlHeader hd;
  if (total < sizeof hd || resource_load_byte_range(h, 0, (uint8_t *)&hd, sizeof hd) != sizeof hd)
    return false;
  if (hd.magic != OVL_MAGIC || hd.entry >= hd.image_size || (hd.image_size & 3) ||
      sizeof hd + hd.image_size + 4 * hd.nrel != total)
    return false;
  uint8_t *m = malloc(hd.image_size);
  if (!m) return false;
  if (resource_load_byte_range(h, sizeof hd, m, hd.image_size) != hd.image_size) { free(m); return false; }
  uint32_t offs[16];
  for (uint32_t i = 0; i < hd.nrel; i += 16) {
    uint32_t n = hd.nrel - i < 16 ? hd.nrel - i : 16;
    resource_load_byte_range(h, sizeof hd + hd.image_size + 4 * i, (uint8_t *)offs, 4 * n);
    for (uint32_t k = 0; k < n; k++) {
      if (offs[k] > hd.image_size - 4) { free(m); return false; }
      uint32_t v;
      memcpy(&v, m + offs[k], 4);
      v += (uint32_t)(uintptr_t)m;
      memcpy(m + offs[k], &v, 4);
    }
  }
  __asm__ volatile("dsb\n\tisb" ::: "memory");    // make the new code visible before running it
  o->mem = m;
  o->entry = (OvlEntry)((uintptr_t)(m + hd.entry) | 1);   // Thumb code
  return true;
}

void ovl_unload(Ovl *o) {
  free(o->mem);
  o->mem = NULL;
  o->entry = NULL;
}
