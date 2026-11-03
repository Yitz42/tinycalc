// ovl_api.h - overlays: feature code kept in the app's resources (which don't
// count against the 64 KB app limit) and loaded onto the heap when used.
//
// A blob is: OvlHeader, the code/data image (linked at address 0), then a list
// of image offsets holding absolute addresses, which the loader shifts by the
// load address. Overlays call back into the app only through OvlApi.
#pragma once
#include <stddef.h>
#include <stdint.h>

#define OVL_MAGIC 0x314C564F   // "OVL1"
#define OVL_API_VERSION 2

typedef struct {
  uint32_t magic;
  uint32_t image_size;   // bytes of code + data (+ zeroed bss) to load
  uint32_t entry;        // offset of the entry function in the image
  uint32_t nrel;         // number of uint32 offsets after the image
} OvlHeader;

// What the app gives an overlay.
typedef struct {
  uint32_t version;      // OVL_API_VERSION
  void *(*malloc)(size_t n);
  void (*free)(void *p);
  void (*log)(const char *msg);
} OvlApi;

// Overlay entry: run command cmd with its argument block; returns 0 if the
// command is unknown or the API doesn't match.
typedef int (*OvlEntry)(const OvlApi *api, int cmd, void *args);
