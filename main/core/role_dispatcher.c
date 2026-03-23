#include "role_dispatcher.h"

#include <esp_log.h>
#include "tasks/task_manager.h"
#include "core/hht/hht_main.h"

static const char *TAG = "role_disp";

static role_t g_active_role = ROLE_NONE;
static role_source_t g_active_source = ROLE_SOURCE_NONE;

void role_dispatcher_init(void) {
    /*
     * This module arbitrates role execution requests coming from
     * BLE and CLI. Keep only minimal state here.
     */
    g_active_role = ROLE_NONE;
    g_active_source = ROLE_SOURCE_NONE;
}

void role_dispatcher_request(role_t role, role_source_t source) {
    if (role == ROLE_NONE) {
//        ESP_LOGI(TAG, "ignore role request: none");
        return;
    }

    if (role == g_active_role) {
        ESP_LOGI(TAG, "role already active: %d (source=%d)", role, source);
        return;
    }

    /*
     * In the current skeleton there is no stop API per role.
     * If role switching becomes necessary, add stop handlers here.
     */
    ESP_LOGI(TAG, "role switch: %d -> %d (source=%d)", g_active_role, role, source);
    g_active_role = role;
    g_active_source = source;
    tasks_start_for_role(role);
}

void role_dispatcher_start_default(role_t role) {
    role_dispatcher_request(role, ROLE_SOURCE_NONE);
}

role_t role_dispatcher_get_active(void) {
    return g_active_role;
}

role_source_t role_dispatcher_get_source(void) {
    return g_active_source;
}

void role_dispatcher_clear(void) {
    if (g_active_role == ROLE_HHT) {
        hht_stop();
    } else if (g_active_role == ROLE_STORAGE_UPLOAD) {
        ESP_LOGI(TAG, "storage role cleanup not implemented");
    }
    ESP_LOGI(TAG, "role cleared: %d -> none (source=%d)", g_active_role, g_active_source);
    g_active_role = ROLE_NONE;
    g_active_source = ROLE_SOURCE_NONE;
}
