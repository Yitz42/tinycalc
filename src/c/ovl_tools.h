// ovl_tools.h - the commands of the "tools" overlay (src/ovl/ovl_tools.c).
#pragma once
#include "tools.h"

enum { OT_COUNT = 1, OT_ROW, OT_PICK };

typedef struct { ToolsIO *io; int list, i, ret; } OtArgs;
