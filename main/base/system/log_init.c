#include "log_init.h"

#include <esp_log.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "base/console/cli_hht_view.h"

static int log_vprintf(const char *fmt, va_list args) {
    if (cli_hht_view_is_visible()) {
        return 0;
    }
    return vprintf(fmt, args);
}

void log_init(void) {
    /*
     * Default to INFO to match fw_esp32 baseline behavior.
     * Adjust here if you want a quieter or more verbose boot.
     */
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_set_vprintf(log_vprintf);
}
