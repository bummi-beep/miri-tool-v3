#include "cli_router.h"

#include <string.h>
#include <esp_log.h>

#include "core/role_dispatcher.h"
#include "core/hht/hht_main.h"

static const char *TAG = "cli_router";

void cli_router_on_command(const char *cmd) {
    /*
     * This function is intentionally simple.
     * When a real CLI framework is wired, call this with the
     * parsed command keyword (e.g., "diag", "manual", "hht").
     */
    if (!cmd || cmd[0] == '\0') {
        ESP_LOGI(TAG, "empty cli command");
        return;
    }

    if (strcmp(cmd, "hht") == 0) {
        hht_set_type(HHT_TYPE_WB100);
        role_dispatcher_request(ROLE_HHT, ROLE_SOURCE_CLI);
    } else if (strcmp(cmd, "fw") == 0) {
        role_dispatcher_request(ROLE_FW_UPLOAD, ROLE_SOURCE_CLI);
    } else if (strcmp(cmd, "storage") == 0) {
        role_dispatcher_request(ROLE_STORAGE_UPLOAD, ROLE_SOURCE_CLI);
    } else if (strcmp(cmd, "esp32") == 0) {
        role_dispatcher_request(ROLE_FW_UPLOAD, ROLE_SOURCE_CLI);
    } else if (strcmp(cmd, "diag") == 0) {
        role_dispatcher_request(ROLE_DIAG, ROLE_SOURCE_CLI);
    } else if (strcmp(cmd, "manual") == 0) {
        role_dispatcher_request(ROLE_MANUAL, ROLE_SOURCE_CLI);
    } else {
        ESP_LOGI(TAG, "unknown cli role command: %s", cmd);
    }
}
