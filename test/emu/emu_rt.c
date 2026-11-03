// emu_rt.c - the ARM emulator side: Linux syscalls for output, a capped heap
// (first fit, with guard words to catch overruns), and the overlay blobs as
// resources.
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "pebble.h"

static int sys(int n, int a, int b, int c) {
  register int r0 __asm__("r0") = a; register int r1 __asm__("r1") = b;
  register int r2 __asm__("r2") = c; register int r7 __asm__("r7") = n;
  __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
  return r0;
}
void emu_write(const char *s, int n) { sys(4, 1, (int)s, n); }
void emu_exit(int code) { sys(1, code, 0, 0); for (;;) {} }
void emu_log(const char *fmt, ...) { (void)fmt; }

void *memset(void *d, int c, size_t n) { uint8_t *a = d; while (n--) *a++ = (uint8_t)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { uint8_t *a = d; const uint8_t *b = s; while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n) {
  uint8_t *a = d; const uint8_t *b = s;
  if (a < b) while (n--) *a++ = *b++; else { a += n; b += n; while (n--) *--a = *--b; }
  return d;
}
int memcmp(const void *x, const void *y, size_t n) {
  const uint8_t *a = x, *b = y;
  for (; n; n--, a++, b++) if (*a != *b) return *a - *b;
  return 0;
}

// ---- heap: blocks of [size][magic] payload [magic], first fit, coalescing
#define HEAP_MAX 262144
static uint8_t heap[HEAP_MAX] __attribute__((aligned(4096)));
static uint32_t cap = 60000, used, peak;
#define MAGIC1 0xA110CA7Eu
#define MAGIC2 0xDEADBEEFu
typedef struct Blk { uint32_t size; uint32_t magic; uint32_t free; uint32_t pad; } Blk;   // 16 bytes: keeps payload 8-aligned
static Blk *first;
void emu_heap_init(uint32_t c) {
  cap = c; used = peak = 0;
  first = (Blk *)heap;
  first->size = c - sizeof(Blk); first->magic = MAGIC1; first->free = 1;
  sys(125, (int)heap, HEAP_MAX, 7);                       // the watch can run code from the heap
}
uint32_t emu_heap_peak(void) { return peak; }
uint32_t emu_heap_used(void) { return used; }
static Blk *next(Blk *b) { return (Blk *)((uint8_t *)(b + 1) + b->size + 8); }
static int in_heap(Blk *b) { return (uint8_t *)b < heap + cap - sizeof(Blk); }
void emu_fail(const char *m) { int n = 0; while (m[n]) n++; emu_write("HEAP ERROR: ", 12); emu_write(m, n); emu_write("\n", 1); emu_exit(9); }
int emu_heap_check(void) {
  for (Blk *b = first; in_heap(b); b = next(b)) {
    if (b->magic != MAGIC1) emu_fail("block header overwritten");
    uint32_t t; memcpy(&t, (uint8_t *)(b + 1) + b->size, 4);
    if (!b->free && t != MAGIC2) emu_fail("write past the end of a block");
  }
  return 1;
}
void *malloc(size_t n) {
  n = (n + 7) & ~7u;
  for (Blk *b = first; in_heap(b); b = next(b)) {
    if (b->magic != MAGIC1) emu_fail("malloc: corrupt heap");
    if (!b->free || b->size < n) continue;
    if (b->size >= n + sizeof(Blk) + 8 + 16) {             // split
      Blk *r = (Blk *)((uint8_t *)(b + 1) + n + 8);
      r->size = b->size - n - sizeof(Blk) - 8; r->magic = MAGIC1; r->free = 1;
      b->size = n;
    }
    b->free = 0;
    uint32_t t = MAGIC2; memcpy((uint8_t *)(b + 1) + b->size, &t, 4);
    used += b->size; if (used > peak) peak = used;
    return b + 1;
  }
  return 0;
}
void free(void *p) {
  if (!p) return;
  Blk *b = (Blk *)p - 1;
  if (b->magic != MAGIC1) emu_fail("free: not a heap block");
  if (b->free) emu_fail("double free");
  uint32_t t; memcpy(&t, (uint8_t *)(b + 1) + b->size, 4);
  if (t != MAGIC2) emu_fail("free: write past the end of a block");
  b->free = 1; used -= b->size;
  for (Blk *c = first; in_heap(c); c = next(c))           // merge free neighbours
    while (c->free && in_heap(next(c)) && next(c)->free) c->size += next(c)->size + sizeof(Blk) + 8;
}

// ---- resources: the overlay files
extern const uint8_t _binary_math_ovl_start[], _binary_math_ovl_end[], _binary_cas_ovl_start[], _binary_cas_ovl_end[],
  _binary_graph_ovl_start[], _binary_graph_ovl_end[], _binary_fmt_ovl_start[], _binary_fmt_ovl_end[],
  _binary_tools_ovl_start[], _binary_tools_ovl_end[], _binary_cplx_ovl_start[], _binary_cplx_ovl_end[],
  _binary_calcu_ovl_start[], _binary_calcu_ovl_end[];
typedef struct { const uint8_t *a, *b; } Res;
static Res R[8];
ResHandle resource_get_handle(uint32_t id) {
  R[1] = (Res){ _binary_math_ovl_start, _binary_math_ovl_end }; R[2] = (Res){ _binary_cas_ovl_start, _binary_cas_ovl_end };
  R[3] = (Res){ _binary_graph_ovl_start, _binary_graph_ovl_end }; R[4] = (Res){ _binary_fmt_ovl_start, _binary_fmt_ovl_end };
  R[5] = (Res){ _binary_tools_ovl_start, _binary_tools_ovl_end };
  R[6] = (Res){ _binary_cplx_ovl_start, _binary_cplx_ovl_end };
  R[7] = (Res){ _binary_calcu_ovl_start, _binary_calcu_ovl_end };
  return id >= 1 && id <= 7 ? &R[id] : 0;
}
size_t resource_size(ResHandle h) { const Res *r = h; return r->b - r->a; }
size_t resource_load_byte_range(ResHandle h, uint32_t s, uint8_t *buf, size_t n) {
  const Res *r = h;
  if (s + n > (size_t)(r->b - r->a)) return 0;
  memcpy(buf, r->a + s, n);
  return n;
}

int main(int argc, char **argv);
void _start_c(uint32_t *sp) { int argc = (int)sp[0]; char **argv = (char **)(sp + 1); emu_exit(main(argc, argv)); }
__attribute__((naked)) void _start(void) { __asm__ volatile("mov r0, sp\n\tbl _start_c"); }
