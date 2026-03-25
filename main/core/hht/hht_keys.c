#include "hht_keys.h"

#include <esp_log.h>
#include <stdbool.h>

#include "core/hht/hht_main.h"
#include "core/hht/hht_uart.h"
#include "base/console/cli_hht_view.h"

static const char *TAG = "hht_keys";
/* Short key should be transmitted at least 3-4 times. */
static const int HHT_KEY_SHORT_SEND_COUNT = 4;

static int s_hold_ticks = 0;
static uint8_t s_hold_key = HHT_KEY_NONE;
static bool s_hold_active = false;
static bool s_hold_long = false;
static bool s_release_pending = false;
static bool s_release_log_immediate = false;
static bool s_press_log_pending = false;

#define HHT3K_CMD_BTN 0x02

static void hht_keys_send_internal(uint8_t key, bool log) {
    if (hht_get_type() == HHT_TYPE_3000) {
        uint8_t buf[2] = { HHT3K_CMD_BTN, key };
        hht_uart_write(buf, 2);
    } else {
 //       ESP_LOGI(TAG,"WB300 key 0x%02x", key);
        hht_uart_write(&key, 1);
    }
    if (log && !cli_hht_view_is_active()) {
        ESP_LOGI(TAG, "send key: 0x%02x", key);
    }
}

void hht_keys_init(void) {
    ESP_LOGI(TAG, "init");
    s_hold_ticks = 0;
    s_hold_key = HHT_KEY_NONE;
    s_hold_active = false;
    s_hold_long = false;
    s_release_pending = false;
    s_release_log_immediate = false;
    s_press_log_pending = false;
}

void hht_keys_cancel_short_on_display_change(void) {
    /*
     * Cancel remaining short-key repeats when screen content changes.
     * This matches the intent of 2.08's "forced release on display update" behavior.
     */
    if (!s_hold_active || s_hold_long) {
        return;
    }

    s_hold_active = false;
    s_hold_long = false;
    s_hold_ticks = 0;
    s_hold_key = HHT_KEY_NONE;
    s_press_log_pending = false;

    /* Cancel any deferred release/key states and emit release immediately. */
    s_release_pending = false;
    s_release_log_immediate = false;

    hht_keys_send_internal(HHT_KEY_NONE, false);

    if (!cli_hht_view_is_active()) {
        ESP_LOGI(TAG, "key short cancelled (display change)");
    }
}

int hht_keys_poll(void) {
    /*
     * Process UART key TX in hht_task context:
     * - short: fixed count sends
     * - long : keep sending until explicit release(0x00)
     */
    if (s_release_pending) {
        /*
         * Keep short-key minimum send count even if app release(0x00) arrives early.
         * Long key is released immediately on 0x00.
         */
        if (s_hold_active && !s_hold_long && s_hold_ticks > 0) {
            /* defer release until short burst is fully sent */
        } else {
        s_release_pending = false;
        s_hold_active = false;
        s_hold_long = false;
        s_hold_ticks = 0;
        s_hold_key = HHT_KEY_NONE;
        s_press_log_pending = false;
        hht_keys_send_internal(HHT_KEY_NONE, false);
        if (!cli_hht_view_is_active()) {
            if (s_release_log_immediate) {
                ESP_LOGI(TAG, "key released (immediate)");
            } else {
                ESP_LOGI(TAG, "key released");
            }
        }
        s_release_log_immediate = false;
        return 1;
        }
    }

    if (!s_hold_active) {
        /* 2.08처럼 키 폴링 주기마다 "release 상태(0x00)"도 계속 전송 */
        hht_keys_send_internal(HHT_KEY_NONE, false);
        return 1;
    }
    hht_keys_send_internal(s_hold_key, s_press_log_pending);
    s_press_log_pending = false;

    if (!s_hold_long) {
        s_hold_ticks--;
        if (s_hold_ticks <= 0) {
            s_hold_active = false;
            s_hold_key = HHT_KEY_NONE;
            s_release_pending = true;      /* send one explicit release on next 40ms tick */
            s_release_log_immediate = false;
        }
    }
    return 1;
}

void hht_keys_send(uint8_t key) {
    if (key == HHT_KEY_NONE) {
        /* Defer release write to hht_keys_poll() (single task UART path). */
        s_release_pending = true;
        s_release_log_immediate = true;
        return;
    }
    s_release_pending = false;
    s_release_log_immediate = false;
    s_hold_key = key;
    s_hold_ticks = HHT_KEY_SHORT_SEND_COUNT;
    s_hold_active = true;
    s_hold_long = false;
    s_press_log_pending = true;
}

void hht_keys_send_long(uint8_t key) {
    if (key == HHT_KEY_NONE) {
        return;
    }
    s_hold_key = key;
    s_hold_ticks = 0;
    s_hold_active = true;
    s_hold_long = true;
    s_release_pending = false;
    s_release_log_immediate = false;
    s_press_log_pending = false;
    if (!cli_hht_view_is_active()) {
        ESP_LOGI(TAG, "start long key: 0x%02x", key);
    }
}
