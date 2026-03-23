#include "hht_main.h"

#include <stdbool.h>
#include <esp_log.h>

#include "core/hht/hht_task.h"
#include "core/hht/hht_wb100.h"
#include "core/hht/hht_3000.h"

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
    switch (s_hht_type) {
        case HHT_TYPE_WB100:
            hht_wb100_init();
            hht_wb100_start_session();
            break;
        case HHT_TYPE_3000:
            hht_3000_init();
            hht_3000_start_session();
            break;
        case HHT_TYPE_100:
            /* 100: 현재 WB100 경로 공유. 전용 hht_100.c 추가 시 교체 */
            hht_wb100_init();
            hht_wb100_start_session();
            break;
        default:
            ESP_LOGW(TAG, "unknown HHT type %d, use WB100", s_hht_type);
            hht_wb100_init();
            hht_wb100_start_session();
            break;
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
    switch (s_hht_type) {
        case HHT_TYPE_WB100:
            hht_wb100_stop_session();
            break;
        case HHT_TYPE_3000:
            hht_3000_stop_session();
            break;
        case HHT_TYPE_100:
            hht_wb100_stop_session();
            break;
        default:
            hht_wb100_stop_session();
            break;
    }
    g_hht_running = false;
}

