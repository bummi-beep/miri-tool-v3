#include "cli_task.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <esp_log.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "base/console/cli_core.h"
#include "base/console/cli_menu.h"
#include "base/console/cli_hht_view.h"
#include "base/console/cli_ota_view.h"
#include "core/hht/hht_keys.h"
#include "protocol/ble_cmd_parser.h"
#include "core/role_dispatcher.h"

static const char *TAG = "cli_task";
static volatile cli_mode_t s_cli_mode = CLI_MODE_NORMAL;
static uint16_t s_hht_stop_cmd = CMD_STOP_HHT_100;
static void cli_print_prompt(void) {
    printf("MIRI> ");
    fflush(stdout);
}

static const char *cli_key_code_name(uint8_t code) {
    switch (code) {
        case HHT_KEY_CODE_NONE: return "NONE";
        case HHT_KEY_CODE_ESC: return "ESC";
        case HHT_KEY_CODE_UP: return "UP";
        case HHT_KEY_CODE_DN: return "DOWN";
        case HHT_KEY_CODE_ENT: return "ENTER";
        case HHT_KEY_CODE_ESC_ENT: return "ESC+ENTER";
        case HHT_KEY_CODE_UP_ENT: return "UP+ENTER";
        case HHT_KEY_CODE_ESC_LONG: return "ESC_LONG";
        case HHT_KEY_CODE_UP_LONG: return "UP_LONG";
        case HHT_KEY_CODE_DN_LONG: return "DOWN_LONG";
        case HHT_KEY_CODE_ENT_LONG: return "ENTER_LONG";
        default: return "UNKNOWN";
    }
}

static void cli_task_send_hht_key_code(uint8_t code) {
    cli_hht_view_set_last_key(cli_key_code_name(code));
    ble_cmd_handle_with_payload(CMD_HHT_BTN, &code, 1);
    cli_hht_view_refresh();
}

static void cli_task_handle_ota_input(void) {
    uint8_t ch = 0;
    int rd = uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(10));
    if (rd <= 0) {
        return;
    }
    if (ch == 'q' || ch == 'Q') {
        ESP_LOGI(TAG, "exit ESP32 mode (back to CLI)");
        ble_cmd_handle(CMD_PROGRAM_STOP_PHONE_ESP32);
        cli_ota_view_set_active(false);
        role_dispatcher_clear();
        cli_task_set_mode(CLI_MODE_NORMAL);
        uart_flush_input(UART_NUM_0);
        cli_menu_show_main();
        cli_print_prompt();
    }
}

static void cli_task_handle_fw_input(void) {
    uint8_t ch = 0;
    int rd = uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(50));
    if (rd <= 0) {
        return;
    }
    if (ch == 'q' || ch == 'Q') {
        ESP_LOGI(TAG, "exit firmware update mode (back to CLI)");
        ble_cmd_handle(CMD_FW_UPLOAD_STOP);
        role_dispatcher_clear();
        cli_task_set_mode(CLI_MODE_NORMAL);
        uart_flush_input(UART_NUM_0);
        cli_menu_show_main();
        cli_print_prompt();
    }
}
static void cli_task_handle_hht_input(void) {
    uint8_t ch = 0;
    int rd = uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(50));
    if (rd <= 0) {
        return;
    }

    if (ch == 'q' || ch == 'Q') {
        ESP_LOGI(TAG, "exit HHT mode (back to CLI)");
        ble_cmd_handle(s_hht_stop_cmd);
        role_dispatcher_clear();
        cli_hht_view_set_active(false);
        cli_task_set_mode(CLI_MODE_NORMAL);
        uart_flush_input(UART_NUM_0);
        printf("\033[2J\033[H");
        fflush(stdout);
        cli_menu_show_main();
        cli_print_prompt();
        return;
    }

    if (ch == 0x1B) { // ESC or arrow sequence
        uint8_t seq1 = 0;
        uint8_t seq2 = 0;
        int r1 = uart_read_bytes(UART_NUM_0, &seq1, 1, pdMS_TO_TICKS(10));
        if (r1 > 0 && seq1 == '[') {
            int r2 = uart_read_bytes(UART_NUM_0, &seq2, 1, pdMS_TO_TICKS(10));
            if (r2 > 0) {
                if (seq2 == 'A') {
                    cli_task_send_hht_key_code(HHT_KEY_CODE_UP);
                    return;
                }
                if (seq2 == 'B') {
                    cli_task_send_hht_key_code(HHT_KEY_CODE_DN);
                    return;
                }
            }
        }
        cli_task_send_hht_key_code(HHT_KEY_CODE_ESC);
        return;
    }

    if (ch == '\r' || ch == '\n') {
        cli_task_send_hht_key_code(HHT_KEY_CODE_ENT);
        return;
    }

    switch (ch) {
        case '1':
            cli_task_send_hht_key_code(HHT_KEY_CODE_ESC_ENT);
            return;
        case '2':
            cli_task_send_hht_key_code(HHT_KEY_CODE_UP_ENT);
            return;
        case '3':
            cli_task_send_hht_key_code(HHT_KEY_CODE_ESC_LONG);
            return;
        case '4':
            cli_task_send_hht_key_code(HHT_KEY_CODE_UP_LONG);
            return;
        case '5':
            cli_task_send_hht_key_code(HHT_KEY_CODE_DN_LONG);
            return;
        case '6':
            cli_task_send_hht_key_code(HHT_KEY_CODE_ENT_LONG);
            return;
        default:
            break;
    }

    if (ch == 'q' || ch == 'Q') {
        ESP_LOGI(TAG, "exit HHT mode (back to CLI)");
        ble_cmd_handle(s_hht_stop_cmd);
        role_dispatcher_clear();
        cli_hht_view_set_active(false);
        cli_task_set_mode(CLI_MODE_NORMAL);
        uart_flush_input(UART_NUM_0);
        printf("\033[2J\033[H");
        fflush(stdout);
        cli_menu_show_main();
        cli_print_prompt();
        return;
    }
}

static void cli_task(void *arg) {
    char line[128];
    size_t pos = 0;

    ESP_LOGI(TAG, "cli task started (type HELP)");
    cli_menu_show_main();
    cli_print_prompt();
    for (;;) {
        if (s_cli_mode == CLI_MODE_HHT && role_dispatcher_get_active() != ROLE_HHT) {
            ESP_LOGW(TAG, "HHT role cleared; return to CLI");
            cli_hht_view_set_active(false);
            s_cli_mode = CLI_MODE_NORMAL;
            uart_flush_input(UART_NUM_0);
            cli_menu_show_main();
            cli_print_prompt();
        }
        if (s_cli_mode == CLI_MODE_ESP32 && role_dispatcher_get_active() != ROLE_FW_UPLOAD) {
            ESP_LOGW(TAG, "ESP32 role cleared; return to CLI");
            cli_ota_view_set_active(false);
            s_cli_mode = CLI_MODE_NORMAL;
            uart_flush_input(UART_NUM_0);
            cli_menu_show_main();
            cli_print_prompt();
        }
        if (s_cli_mode == CLI_MODE_FW && role_dispatcher_get_active() != ROLE_FW_UPLOAD) {
            ESP_LOGW(TAG, "FW role cleared; return to CLI");
            s_cli_mode = CLI_MODE_NORMAL;
            uart_flush_input(UART_NUM_0);
            cli_menu_show_main();
            cli_print_prompt();
        }
        if (s_cli_mode == CLI_MODE_HHT) {
            cli_task_handle_hht_input();
            continue;
        }
        if (s_cli_mode == CLI_MODE_ESP32) {
            cli_task_handle_ota_input();
            continue;
        }
        if (s_cli_mode == CLI_MODE_FW) {
            cli_task_handle_fw_input();
            continue;
        }

        uint8_t ch = 0;
        int rd = uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(20));
        if (rd <= 0) {
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            line[pos] = '\0';
            printf("\r\n");
            if (pos > 0) {
                cli_execute(line);
            } else {
                ESP_LOGI(TAG, "empty line");
            }
            pos = 0;
            if (s_cli_mode == CLI_MODE_NORMAL) {
                cli_print_prompt();
            }
            continue;
        }

        if (ch == 0x08 || ch == 0x7f) { // backspace/delete
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        if (isprint((int)ch)) {
            if (pos < sizeof(line) - 1) {
                line[pos++] = (char)ch;
                putchar((int)ch);
                fflush(stdout);
            }
        }
    }
}

void cli_task_start(void) {
    xTaskCreate(cli_task, "cli_task", 12288, NULL, 5, NULL);
}

void cli_task_set_mode(cli_mode_t mode) {
    s_cli_mode = mode;
}

void cli_task_set_hht_stop_cmd(uint16_t cmd) {
    s_hht_stop_cmd = cmd;
}

cli_mode_t cli_task_get_mode(void) {
    return s_cli_mode;
}
