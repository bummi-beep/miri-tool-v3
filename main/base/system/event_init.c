#include "event_init.h"

#include <esp_event.h>

void event_init(void) {
    /*
     * Default event loop is required by many ESP-IDF components
     * (Wi-Fi, BLE, IP stack, etc.).
     */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}
