#include "net_init.h"
#include <esp_log.h>
#include <esp_netif.h>

void net_init(void) {
    ESP_LOGI("net", "net_init");
    ESP_ERROR_CHECK(esp_netif_init());
}

