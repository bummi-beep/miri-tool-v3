#include "hht_main.h"

#include <stdbool.h>
#include <esp_log.h>

#include "core/hht/hht_task.h"
#include "core/hht/hht_session.h"

static const char *TAG = "hht";
static bool g_hht_running = false;
static hht_type_t s_hht_type = HHT_TYPE_WB100;

void hht_set_type(hht_type_t type) {
    if (type < HHT_TYPE_COUNT) {
        s_hht_type = type;
    }
}

hht_type_t hht_get_type(void) {
    return s_hht_type;
}

void hht_start(void) {
    if (g_hht_running) {
        ESP_LOGI(TAG, "HHT already running");
        return;
    }

    ESP_LOGI(TAG, "HHT start (type=%d)", s_hht_type);
    hht_session_result_t sr = hht_session_open(s_hht_type);
    if (sr != HHT_SESSION_OK) {
        ESP_LOGW(TAG, "session open: %s — poll task still runs (retry/display may recover)",
                 hht_session_result_str(sr));
    }
    hht_task_start();
    g_hht_running = true;
}

void hht_stop(void) {
    if (!g_hht_running) {
        ESP_LOGI(TAG, "HHT already stopped");
        return;
    }

    ESP_LOGI(TAG, "HHT stop (type=%d)", s_hht_type);
    hht_task_stop();
    hht_session_close(s_hht_type);
    g_hht_running = false;
}

