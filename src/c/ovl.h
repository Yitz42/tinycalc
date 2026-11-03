// ovl.h - load an overlay (feature code kept in resources) onto the heap.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "ovl_api.h"

typedef struct {
  void *mem;          // the loaded image (heap), NULL when not loaded
  OvlEntry entry;
} Ovl;

// Load the overlay in raw resource res_id. False if it's missing, damaged or
// there isn't enough heap.
bool ovl_load(Ovl *o, uint32_t res_id);
void ovl_unload(Ovl *o);
