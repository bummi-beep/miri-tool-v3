#include "can_init.h"

#include <esp_log.h>
#include <driver/twai.h>

#include "config/pinmap.h"

static const char *TAG = "can_init";
static bool s_ready = false;

static twai_timing_config_t can_timing_from_bitrate(can_bitrate_t bitrate) {
    switch (bitrate) {
        case CAN_BITRATE_1M:
            return (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
        case CAN_BITRATE_500K:
            return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
        case CAN_BITRATE_250K:
            return (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
        case CAN_BITRATE_125K:
            return (twai_timing_config_t)TWAI_TIMING_CONFIG_125KBITS();
        default:
            ESP_LOGW(TAG, "Unknown bitrate %d, fallback to 1M", (int)bitrate);
            return (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
    }
}

esp_err_t can_init(can_bitrate_t bitrate) {
    if (s_ready) {
        ESP_LOGI(TAG, "CAN already initialized");
        return ESP_OK;
    }

    const pinmap_t *pins = pinmap_get();
    if (!pins) {
        ESP_LOGE(TAG, "pinmap is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        pins->target_can_tx,
        pins->target_can_rx,
        TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = can_timing_from_bitrate(bitrate);
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }
    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_start failed: %s", esp_err_to_name(err));
        twai_driver_uninstall();
        return err;
    }

    s_ready = true;
    ESP_LOGI(TAG, "CAN ready (tx=%d rx=%d bitrate=%d)",
             pins->target_can_tx, pins->target_can_rx, (int)bitrate);
    return ESP_OK;
}

void can_deinit(void) {
    if (!s_ready) {
        return;
    }
    twai_stop();
    twai_driver_uninstall();
    s_ready = false;
    ESP_LOGI(TAG, "CAN deinit");
}

bool can_is_ready(void) {
    return s_ready;
}
