#include "incan.h"

#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config/pinmap.h"

static const char *TAG = "incan";
static bool s_can_open = false;

static twai_timing_config_t timing_from_bps(uint32_t bps) {
    switch (bps) {
        case 1000000:
            return (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
        case 500000:
            return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
        case 250000:
            return (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
        case 125000:
        default:
            return (twai_timing_config_t)TWAI_TIMING_CONFIG_125KBITS();
    }
}

esp_err_t incan_open(uint32_t bps, uint32_t filter_id) {
    if (s_can_open) {
        incan_stop();
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    const pinmap_t *pins = pinmap_get();
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)pins->target_can_tx,
        (gpio_num_t)pins->target_can_rx,
        TWAI_MODE_NORMAL);

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    if (filter_id != 0) {
        f_config.acceptance_code = (filter_id << 3);
        f_config.acceptance_mask = ~(0x1fffffff << 3);
        f_config.single_filter = true;
    }

    twai_timing_config_t t_config = timing_from_bps(bps);
    ESP_LOGI(TAG, "CAN open bps=%lu filter=0x%08lx", (unsigned long)bps, (unsigned long)filter_id);

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "twai_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }
    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "twai_start failed: %s", esp_err_to_name(err));
        twai_driver_uninstall();
        return err;
    }

    s_can_open = true;
    return ESP_OK;
}

void incan_stop(void) {
    if (!s_can_open) {
        return;
    }
    twai_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    twai_driver_uninstall();
    vTaskDelay(pdMS_TO_TICKS(100));
    s_can_open = false;
}

esp_err_t incan_tx(uint32_t id, uint8_t extd, uint8_t dlc, const uint8_t *buff, TickType_t timeout) {
    twai_message_t message = {0};
    message.identifier = id;
    message.extd = extd;
    message.rtr = 0;
    message.data_length_code = dlc;
    if (buff && dlc) {
        memcpy(message.data, buff, dlc);
    }
    return twai_transmit(&message, timeout);
}

bool incan_get_message(twai_message_t *pmessage, uint16_t time_ms) {
    if (!pmessage) {
        return false;
    }
    memset(pmessage, 0, sizeof(*pmessage));
    return (twai_receive(pmessage, pdMS_TO_TICKS(time_ms)) == ESP_OK);
}
