#include "cli_hht_view.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <esp_log.h>

#include "core/hht/hht_display.h"

static const char *TAG = "cli_hht_view";

#define HHT_ROWS_MAX 4
#define HHT_COLS_MAX 20
#define HHT_LOG_LINES 4
#define HHT_LOG_COLS  60
#define HHT_VIEW_COL 60
#define HHT_VIEW_ROW 1

static bool s_active = false;
static bool s_boxed = false;
static bool s_ble_connected = false;
static char s_lines[HHT_ROWS_MAX][HHT_COLS_MAX + 1];
static char s_last_key[16] = "-";
static char s_log_lines[HHT_LOG_LINES][HHT_LOG_COLS + 1];

static int cli_hht_view_line_row(uint8_t row) {
    return HHT_VIEW_ROW + 1 + row;
}

static int cli_hht_view_frame_bottom_row(void) {
    return HHT_VIEW_ROW + 1 + (int)hht_display_get_rows();
}

static int cli_hht_view_mode_row(void) {
    return cli_hht_view_frame_bottom_row() + 1;
}

static int cli_hht_view_last_key_row(void) {
    return cli_hht_view_mode_row() + 1;
}

static int cli_hht_view_log_base_row(void) {
    return cli_hht_view_last_key_row() + 1;
}

static void cli_hht_view_render_frame(void) {
    int row = HHT_VIEW_ROW;
    const int rows = (int)hht_display_get_rows();
    const int cols = (int)hht_display_get_cols();
    /* 상단: +----...+ */
    printf("\033[%d;%dH\033[K+", row++, HHT_VIEW_COL);
    for (int c = 0; c < cols; c++) putchar('-');
    printf("+");
    for (int i = 0; i < rows; i++) {
        printf("\033[%d;%dH\033[K|%-*s|", row++, HHT_VIEW_COL, cols, s_lines[i]);
    }
    printf("\033[%d;%dH\033[K+", row++, HHT_VIEW_COL);
    for (int c = 0; c < cols; c++) putchar('-');
    printf("+");
    printf("\033[%d;%dH\033[KHHT mode: UP/DOWN/ENTER/ESC, Q=exit", row++, HHT_VIEW_COL);
    printf("\033[%d;%dH\033[K1=ESC+ENT 2=UP+ENT 3=ESC_L 4=UP_L 5=DN_L 6=ENT_L", row++, HHT_VIEW_COL);
    printf("\033[%d;%dH\033[KLast key: %s", row++, HHT_VIEW_COL, s_last_key);
    for (int i = 0; i < HHT_LOG_LINES; i++) {
        printf("\033[%d;%dH\033[K%-60s", row++, HHT_VIEW_COL, s_log_lines[i]);
    }
}

static void cli_hht_view_render_line(uint8_t row) {
    const int rows = (int)hht_display_get_rows();
    const int cols = (int)hht_display_get_cols();
    if (row >= (unsigned)rows) {
        return;
    }
    printf("\033[%d;%dH\033[K|%-*s|", cli_hht_view_line_row(row), HHT_VIEW_COL, cols, s_lines[row]);
}

static void cli_hht_view_render_last_key(void) {
    printf("\033[%d;%dH\033[KLast key: %s", cli_hht_view_last_key_row(), HHT_VIEW_COL, s_last_key);
}

static void cli_hht_view_render_logs(void) {
    int row = cli_hht_view_log_base_row();
    for (int i = 0; i < HHT_LOG_LINES; i++) {
        printf("\033[%d;%dH\033[K%-60s", row++, HHT_VIEW_COL, s_log_lines[i]);
    }
}

static void cli_hht_view_render(void) {
    if (!s_active || !s_boxed || s_ble_connected) {
        return;
    }

    printf("\033[s");
    cli_hht_view_render_frame();
    printf("\033[u");
    fflush(stdout);
}

static void cli_hht_view_set_line(uint8_t row, const char *line20) {
    if (row >= HHT_ROWS_MAX) {
        return;
    }
    const int cols = (int)hht_display_get_cols();
    for (int i = 0; i < HHT_COLS_MAX; i++) {
        char ch = (line20 && i < cols) ? line20[i] : ' ';
        if (ch == '\0') {
            ch = ' ';
        }
        s_lines[row][i] = (char)(isprint((unsigned char)ch) ? ch : ' ');
    }
    s_lines[row][cols] = '\0';
}

void cli_hht_view_set_active(bool active) {
    s_active = active;
    if (s_active) {
        for (int i = 0; i < HHT_ROWS_MAX; i++) {
            memset(s_lines[i], ' ', HHT_COLS_MAX);
            s_lines[i][HHT_COLS_MAX] = '\0';
        }
        for (int i = 0; i < HHT_LOG_LINES; i++) {
            memset(s_log_lines[i], ' ', HHT_LOG_COLS);
            s_log_lines[i][HHT_LOG_COLS] = '\0';
        }
        strncpy(s_last_key, "-", sizeof(s_last_key) - 1);
        s_last_key[sizeof(s_last_key) - 1] = '\0';
        ESP_LOGI(TAG, "HHT view enabled");
        if (!s_ble_connected) {
            cli_hht_view_render();
        }
    } else {
        ESP_LOGI(TAG, "HHT view disabled");
        s_boxed = false;
    }
}

bool cli_hht_view_is_active(void) {
    return s_active;
}

bool cli_hht_view_is_visible(void) {
    return s_active && s_boxed && !s_ble_connected;
}

void cli_hht_view_set_boxed(bool boxed) {
    s_boxed = boxed;
    if (s_active && s_boxed && !s_ble_connected) {
        cli_hht_view_render();
    }
}

bool cli_hht_view_is_boxed(void) {
    return s_boxed;
}

void cli_hht_view_update_line(uint8_t row, const char *line20) {
    if (!s_active) {
        return;
    }
    cli_hht_view_set_line(row, line20);
    if (!s_boxed || s_ble_connected) {
        return;
    }
    printf("\033[s");
    cli_hht_view_render_line(row);
    printf("\033[u");
    fflush(stdout);
}

void cli_hht_view_set_last_key(const char *key_name) {
    if (!s_active) {
        return;
    }
    if (!key_name || key_name[0] == '\0') {
        key_name = "-";
    }
    strncpy(s_last_key, key_name, sizeof(s_last_key) - 1);
    s_last_key[sizeof(s_last_key) - 1] = '\0';
    if (!s_boxed || s_ble_connected) {
        return;
    }
    printf("\033[s");
    cli_hht_view_render_last_key();
    printf("\033[u");
    fflush(stdout);
}

void cli_hht_view_push_log(const char *line) {
    if (!s_active || !s_boxed || s_ble_connected) {
        return;
    }
    for (int i = 0; i < HHT_LOG_LINES - 1; i++) {
        memcpy(s_log_lines[i], s_log_lines[i + 1], HHT_LOG_COLS + 1);
    }
    if (!line) {
        line = "";
    }
    size_t len = strlen(line);
    if (len > HHT_LOG_COLS) {
        line = line + (len - HHT_LOG_COLS);
        len = HHT_LOG_COLS;
    }
    memset(s_log_lines[HHT_LOG_LINES - 1], ' ', HHT_LOG_COLS);
    memcpy(s_log_lines[HHT_LOG_LINES - 1], line, len);
    s_log_lines[HHT_LOG_LINES - 1][HHT_LOG_COLS] = '\0';
    printf("\033[s");
    cli_hht_view_render_logs();
    printf("\033[u");
    fflush(stdout);
}

void cli_hht_view_refresh(void) {
    if (!s_active || !s_boxed || s_ble_connected) {
        return;
    }
    printf("\033[s");
    cli_hht_view_render_frame();
    printf("\033[u");
    fflush(stdout);
}

void cli_hht_view_set_ble_connected(bool connected) {
    s_ble_connected = connected;
    if (!s_active || !s_boxed) {
        return;
    }
    if (s_ble_connected) {
        return;
    }
    cli_hht_view_refresh();
}
