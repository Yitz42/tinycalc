// Freestanding stand-in for the ARM compile check (the SDK has the real one).
#pragma once
#include <stddef.h>
void *malloc(size_t size);
void *calloc(size_t n, size_t size);
void free(void *ptr);
