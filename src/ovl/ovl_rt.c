// ovl_rt.c - C library pieces for overlays (they can't call the watch's own).
#include <stddef.h>
#include <stdint.h>
#include "ovl_rt.h"

static const OvlApi *g_api;
void ovl_rt_init(const OvlApi *api) { g_api = api; }

void *malloc(size_t n) { return g_api->malloc(n); }
void free(void *p) { g_api->free(p); }
void *memcpy(void *d, const void *s, size_t n) {
  uint8_t *a = d; const uint8_t *b = s;
  while (n--) *a++ = *b++;
  return d;
}
void *memmove(void *d, const void *s, size_t n) {
  uint8_t *a = d; const uint8_t *b = s;
  if (a < b) while (n--) *a++ = *b++;
  else { a += n; b += n; while (n--) *--a = *--b; }
  return d;
}
void *memset(void *d, int c, size_t n) {
  uint8_t *a = d;
  while (n--) *a++ = (uint8_t)c;
  return d;
}
int memcmp(const void *x, const void *y, size_t n) {
  const uint8_t *a = x, *b = y;
  for (; n; n--, a++, b++) if (*a != *b) return *a - *b;
  return 0;
}
