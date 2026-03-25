#include "hht_display.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <esp_log.h>

#include "core/hht/hht_main.h"
#include "core/hht/hht_uart.h"
#include "protocol/ble_protocol.h"
#include "base/console/cli_hht_view.h"
#include "core/hht/hht_keys.h"

static const char *TAG = "hht_display";
static int s_dump_raw = 0;

#define HHT_ROWS_MAX 4
#define HHT_COLS_MAX 20

/*
 * 2.08 update_display() 의 count_frame > 200 과 동일 개념: LCD 내용이 변하지 않아도
 * 주기적으로 각 행을 BLE로 다시 보내 앱 타임아웃·화면 소실을 막는다.
 * hht_task 를 약 20ms cadence 로 맞추면 100 ≈ 2초.
 */
#define HHT_DISPLAY_BLE_PERIODIC_TICKS 100

static char s_lines[HHT_ROWS_MAX][HHT_COLS_MAX + 1];
static char s_last_lines[HHT_ROWS_MAX][HHT_COLS_MAX + 1];

uint8_t hht_display_get_rows(void) {
    return (hht_get_type() == HHT_TYPE_3000) ? 2 : 4;
}

uint8_t hht_display_get_cols(void) {
    return (hht_get_type() == HHT_TYPE_3000) ? 16 : 20;
}

void hht_display_init(void) {
    ESP_LOGI(TAG, "init");
    for (int i = 0; i < HHT_ROWS_MAX; i++) {
        memset(s_lines[i], ' ', HHT_COLS_MAX);
        s_lines[i][HHT_COLS_MAX] = '\0';
        memset(s_last_lines[i], 0, sizeof(s_last_lines[i]));
    }
}

void hht_display_dump_raw(int enable) {
    s_dump_raw = enable ? 1 : 0;
    ESP_LOGI(TAG, "raw dump %s", s_dump_raw ? "on" : "off");
}

static void send_row_to_ble_and_cli(uint8_t r, int cols) {
    char line20[21];
    memcpy(line20, s_lines[r], (size_t)cols);
    for (int k = cols; k < 20; k++) {
        line20[k] = ' ';
    }
    line20[20] = '\0';
#if CONFIG_BT_NIMBLE_ENABLED
    ble_protocol_send_hht_line(r, line20);
#endif
    if (cli_hht_view_is_active()) {
        cli_hht_view_update_line(r, line20);
    }
}

void hht_display_push_row_ascii(uint8_t row, const char *src, size_t src_len, bool force_ble) {
    const int rows = (int)hht_display_get_rows();
    const int cols = (int)hht_display_get_cols();
    if (!src || (int)row >= rows || cols <= 0 || cols > HHT_COLS_MAX) {
        return;
    }

    memset(s_lines[row], ' ', (size_t)cols);
    size_t n = src_len;
    if (n > (size_t)cols) {
        n = (size_t)cols;
    }
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)src[i];
        s_lines[row][i] = (c != '\0' && isprint((int)c)) ? (char)c : ' ';
    }
    s_lines[row][cols] = '\0';

    if (!force_ble && strcmp(s_lines[row], s_last_lines[row]) == 0) {
        return;
    }

    ESP_LOGI(TAG, "row%u push: %.*s", (unsigned)row, cols, s_lines[row]);
    memcpy(s_last_lines[row], s_lines[row], (size_t)cols + 1);
    send_row_to_ble_and_cli(row, cols);
}

static void decode_cursor_2x16(int offset, int *row, int *col) {
    if (offset >= 0x00 && offset <= 0x0F) {
        *row = 0;
        *col = offset;
    } else if (offset >= 0x40 && offset <= 0x4F) {
        *row = 1;
        *col = offset - 0x40;
    } else {
        *row = 0;
        *col = 0;
    }
}

static void decode_cursor_4x20(int offset, int *row, int *col) {
    if (offset >= 0x00 && offset <= 0x13) {
        *row = 0;
        *col = offset;
    } else if (offset >= 0x40 && offset <= 0x53) {
        *row = 1;
        *col = offset - 0x40;
    } else if (offset >= 0x14 && offset <= 0x27) {
        *row = 2;
        *col = offset - 0x14;
    } else if (offset >= 0x54 && offset <= 0x67) {
        *row = 3;
        *col = offset - 0x54;
    } else {
        *row = 0;
        *col = 0;
    }
}

void hht_display_poll(void) {
    uint8_t buf[64];
    static int row = 0;
    static int col = 0;
    static bool row_dirty[HHT_ROWS_MAX] = {0};
    bool any_row_pushed = false;

    const int rows = (int)hht_display_get_rows();
    const int cols = (int)hht_display_get_cols();
    const bool is_2x16 = (rows == 2 && cols == 16);

    /* 2.08 read timeout aligned: 20ms */
    memset(buf,0x00,sizeof(buf));
    int rd = hht_uart_read(buf, sizeof(buf), 20);
    if (rd > 0) {
//        ESP_LOGI(TAG, "hht_uart_read: %d", rd);
        for (int i = 0; i < rd; i++) {
            uint8_t ch = buf[i];

            if (s_dump_raw) {
                ESP_LOGI(TAG, "rx: 0x%02x", ch);
            }

            if (ch & 0x80) {
                int offset = ch & 0x7F;
                if (is_2x16) {
                    decode_cursor_2x16(offset, &row, &col);
                } else {
                    decode_cursor_4x20(offset, &row, &col);
                }
              //  ESP_LOGI(TAG, "1: 0x%02x", ch);
                continue;
            }

            if (row < 0 || row >= rows || col < 0 || col >= cols) {
                ESP_LOGI(TAG, "2: 0x%02x", ch);
                continue;
            }

            /* 2.08-like raw update: store received byte directly. */
            s_lines[row][col] = (char)ch;
            row_dirty[row] = true;

            col++;
            if (col >= cols) {
                col = 0;
                row = (row + 1) % rows;
            }
        }

        for (int r = 0; r < rows; r++) {
            if (!row_dirty[r]) {
                continue;
            }
            row_dirty[r] = false;
            s_lines[r][cols] = '\0';
            if (strcmp(s_lines[r], s_last_lines[r]) != 0) {
                ESP_LOGI(TAG, "row%d: %s", r, s_lines[r]);
                strcpy(s_last_lines[r], s_lines[r]);
                send_row_to_ble_and_cli((uint8_t)r, cols);
                any_row_pushed = true;
            }
        }

        /* Screen transition detected -> cancel remaining short key repeats. */
        if (any_row_pushed) {
            hht_keys_cancel_short_on_display_change();
        }
    }
/*
    static uint32_t s_ble_periodic_ticks;
    s_ble_periodic_ticks++;
    if (s_ble_periodic_ticks > HHT_DISPLAY_BLE_PERIODIC_TICKS) {
        s_ble_periodic_ticks = 0;
        for (int r = 0; r < rows; r++) {
            s_lines[r][cols] = '\0';
            strcpy(s_last_lines[r], s_lines[r]);
            send_row_to_ble_and_cli((uint8_t)r, cols);
        }
    }
*/
}
