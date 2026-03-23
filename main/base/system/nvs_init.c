#include "nvs_init.h"

#include <nvs_flash.h>

void nvs_init(void) {
    /*
     * Standard NVS init flow used across ESP-IDF projects:
     * - If NVS has no free pages or a new version is detected,
     *   erase and retry exactly once.
     *
     * Why we still init NVS here:
     * - Wi-Fi uses NVS internally (calibration and settings storage).
     * - BLE/NimBLE persistence is disabled in fw_esp32 (no bonding data stored),
     *   but NVS init remains necessary for Wi-Fi and system components.
     */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}
