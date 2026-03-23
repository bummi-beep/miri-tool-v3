#include "hht_keys.h"

#include <esp_log.h>
#include <stdbool.h>

#include "core/hht/hht_main.h"
#include "core/hht/hht_uart.h"
#include "base/console/cli_hht_view.h"

static const char *TAG = "hht_keys";
static const int HHT_KEY_SHORT_TICKS = 15;
static const int HHT_KEY_LONG_TICKS = 25;

static int s_hold_ticks = 0;
static uint8_t s_hold_key = HHT_KEY_NONE;
static bool s_hold_active = false;

#define HHT3K_CMD_BTN 0x02

static void hht_keys_send_internal(uint8_t key, bool log) {
    if (hht_get_type() == HHT_TYPE_3000) {
        uint8_t buf[2] = { HHT3K_CMD_BTN, key };
        hht_uart_write(buf, 2);
    } else {
        hht_uart_write(&key, 1);
    }
    if (log && !cli_hht_view_is_active()) {
        ESP_LOGI(TAG, "send key: 0x%02x", key);
    }
}

void hht_keys_init(void) {
    ESP_LOGI(TAG, "init");
}

void hht_keys_poll(void) {
    /*
     * Placeholder for future key handling from BLE/CLI.
     */
    if (!s_hold_active) {
        return;
    }
    if (s_hold_ticks <= 0) {
        s_hold_active = false;
        s_hold_key = HHT_KEY_NONE;
        hht_keys_send_internal(HHT_KEY_NONE, false);
        if (!cli_hht_view_is_active()) {
            ESP_LOGI(TAG, "key released");
        }
        return;
    }
    hht_keys_send_internal(s_hold_key, false);
    s_hold_ticks--;
}

void hht_keys_send(uint8_t key) {
    if (key == HHT_KEY_NONE) {
        return;
    }
    s_hold_key = key;
    s_hold_ticks = HHT_KEY_SHORT_TICKS;
    s_hold_active = true;
    hht_keys_send_internal(s_hold_key, true);
}

void hht_keys_send_long(uint8_t key) {
    if (key == HHT_KEY_NONE) {
        return;
    }
    s_hold_key = key;
    s_hold_ticks = HHT_KEY_LONG_TICKS;
    s_hold_active = true;
    if (!cli_hht_view_is_active()) {
        ESP_LOGI(TAG, "start long key: 0x%02x", key);
    }
    hht_keys_send_internal(s_hold_key, false);
}
