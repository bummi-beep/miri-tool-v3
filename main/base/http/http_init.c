#include "http_init.h"

#include <esp_log.h>

#include "base/net/http/http_server.h"

static const char *TAG = "http_init";

void http_init(void) {
    ESP_LOGI(TAG, "http_init");
    esp_err_t ret = http_server_start();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "http_server_start: OK");
    } else {
        ESP_LOGW(TAG, "http_server_start: FAIL (%s)", esp_err_to_name(ret));
    }
}
