#pragma once

#include <stdint.h>

#define STRING_VER_MIRI_TOOL_DEVICE            "v2.08"
#define VER_MIRI_TOOL_DEVICE                   0x0175
#define PROJECT_NAME                           "________MIRI-TOOL-DEVICE________"
#define TAG_MIRI                               "MIRI-TOOL"

#define MIRI_TOOL_VERSION_STR STRING_VER_MIRI_TOOL_DEVICE
#define MIRI_TOOL_VERSION_HEX VER_MIRI_TOOL_DEVICE

const char *version_get_short(void);
const char *version_get_string(void);
void version_log(void);
