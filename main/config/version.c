#include "config/version.h"

#include <stdbool.h>
#include <stdio.h>

#include "esp_log.h"

static const char *TAG = "VERSION";

const char *version_get_short(void) {
    return MIRI_TOOL_VERSION_STR;
}

const char *version_get_string(void) {
    static char version_buf[64];
    static bool is_inited = false;

    if (!is_inited) {
        snprintf(version_buf, sizeof(version_buf), "%s, %s %s",
                 MIRI_TOOL_VERSION_STR, __DATE__, __TIME__);
        is_inited = true;
    }

    return version_buf;
}

void version_log(void) {
    ESP_LOGI(TAG, "app version : %s", MIRI_TOOL_VERSION_STR);
    ESP_LOGI(TAG, "build date  : %s %s", __DATE__, __TIME__);
}
