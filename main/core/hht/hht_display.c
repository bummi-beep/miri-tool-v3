#include "hht_display.h"

#include <ctype.h>
#include <string.h>

#include <esp_log.h>

#include "core/hht/hht_main.h"
#include "core/hht/hht_uart.h"
#include "protocol/ble_protocol.h"
#include "base/console/cli_hht_view.h"

static const char *TAG = "hht_display";
static int s_dump_raw = 0;

#define HHT_ROWS_MAX 4
#define HHT_COLS_MAX 20

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

    const int rows = (int)hht_display_get_rows();
    const int cols = (int)hht_display_get_cols();
    const bool is_2x16 = (rows == 2 && cols == 16);

    int rd = hht_uart_read(buf, sizeof(buf), 10);
    if (rd <= 0) {
        return;
    }

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
            continue;
        }

        if (row < 0 || row >= rows || col < 0 || col >= cols) {
            continue;
        }

        s_lines[row][col] = isprint((int)ch) ? (char)ch : ' ';
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
#if CONFIG_BT_NIMBLE_ENABLED
            {
                char line20[21];
                memcpy(line20, s_lines[r], (size_t)cols);
                for (int k = cols; k < 20; k++) {
                    line20[k] = ' ';
                }
                line20[20] = '\0';
                ble_protocol_send_hht_line((uint8_t)r, line20);
            }
#endif
            if (cli_hht_view_is_active()) {
                char line20[21];
                memcpy(line20, s_lines[r], (size_t)cols);
                for (int k = cols; k < 20; k++) {
                    line20[k] = ' ';
                }
                line20[20] = '\0';
                cli_hht_view_update_line((uint8_t)r, line20);
            }
        }
    }
}
