// ovl_rt.h - what overlay code needs from the C library: malloc and free go
// to the app's heap through the OvlApi; the mem* functions are in ovl_rt.c.
#pragma once
#include "../c/ovl_api.h"
void ovl_rt_init(const OvlApi *api);
