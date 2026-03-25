#include "core/hht/hht_session.h"

#include <esp_log.h>

#include "core/hht/hht_wb100.h"
#include "core/hht/hht_3000.h"

static const char *TAG = "hht_session";

const char *hht_session_result_str(hht_session_result_t r)
{
    switch (r) {
    case HHT_SESSION_OK:
        return "OK";
    case HHT_SESSION_ERR_TIMEOUT:
        return "TIMEOUT";
    case HHT_SESSION_ERR_PROTOCOL:
        return "PROTOCOL";
    case HHT_SESSION_ERR_UART:
        return "UART";
    case HHT_SESSION_ERR_UNSUPPORTED:
        return "UNSUPPORTED";
    default:
        return "?";
    }
}

hht_session_result_t hht_session_open(hht_type_t type)
{
    ESP_LOGI(TAG, "open (type=%d)", (int)type);

    switch (type) {
    case HHT_TYPE_WB100:
        hht_wb100_init();
        {
            hht_session_result_t r = hht_wb100_session_open();
            if (r != HHT_SESSION_OK) {
                ESP_LOGW(TAG, "WB100 session failed → %s", hht_session_result_str(r));
            } else {
                hht_wb100_post_verify_workflow();
            }
            return r;
        }

    case HHT_TYPE_100:
        hht_wb100_init();
        {
            hht_session_result_t r = hht_wb100_session_open();
            if (r != HHT_SESSION_OK) {
                ESP_LOGW(TAG, "HHT100 session failed → %s", hht_session_result_str(r));
            }
            return r;
        }

    case HHT_TYPE_3000:
        hht_3000_init();
        if (!hht_3000_session_open()) {
            ESP_LOGW(TAG, "HHT3000 session failed → %s", hht_session_result_str(HHT_SESSION_ERR_UART));
            return HHT_SESSION_ERR_UART;
        }
        return HHT_SESSION_OK;

    default:
        ESP_LOGW(TAG, "unknown type %d, fallback WB100", (int)type);
        hht_wb100_init();
        {
            hht_session_result_t r = hht_wb100_session_open();
            if (r != HHT_SESSION_OK) {
                ESP_LOGW(TAG, "fallback session failed → %s", hht_session_result_str(r));
            } else {
                hht_wb100_post_verify_workflow();
            }
            return r;
        }
    }
}

void hht_session_close(hht_type_t type)
{
    ESP_LOGI(TAG, "close (type=%d)", (int)type);

    switch (type) {
    case HHT_TYPE_WB100:
    case HHT_TYPE_100:
        hht_wb100_session_close();
        break;
    case HHT_TYPE_3000:
        hht_3000_session_close();
        break;
    default:
        hht_wb100_session_close();
        break;
    }
}
