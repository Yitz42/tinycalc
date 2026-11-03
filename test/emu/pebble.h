// Stand-in pebble.h for the ARM emulator fuzz build: just what ovl.c and
// ovl_calls.c use. Resources are the overlay blobs linked into the program.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef const void *ResHandle;
ResHandle resource_get_handle(uint32_t id);
size_t resource_size(ResHandle h);
size_t resource_load_byte_range(ResHandle h, uint32_t start, uint8_t *buf, size_t n);
enum { RESOURCE_ID_OVL_MATH = 1, RESOURCE_ID_OVL_CAS, RESOURCE_ID_OVL_GRAPH, RESOURCE_ID_OVL_FMT, RESOURCE_ID_OVL_TOOLS, RESOURCE_ID_OVL_CPLX, RESOURCE_ID_OVL_CALCU };
void *malloc(size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void emu_log(const char *fmt, ...);
#define APP_LOG(level, ...) emu_log(__VA_ARGS__)
