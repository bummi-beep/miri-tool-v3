/*
 * HHT 3000: 문서(HHT2K_UART_ARCHITECTURE.md) 제안 프로토콜.
 * UART: pinmap Command UART cmd_txd/cmd_rxd (IO13 TX / IO14 RX) → STM32 USART1 PA10/PA9.
 * STM32 단독 펌(hht2k_standalone) 상수 동기화: STM32/hht2k/src/hht2k_miri_proto.h
 * - 진입: 모드 'H'(0x02) + 서브모드 Sniff('S', 0x02) → STM32 Sniff 루프.
 * - 디스플레이: 0x01(Report) → "0:<line0>\r\n1:<line1>\r\n" 수신 후
 *   **hht_display_push_row_ascii** 로 WB100과 동일하게 BLE(CMD_HHT_MSG_100)·CLI 갱신.
 * - 버튼: 0x02 + 1바이트 mask (hht_keys에서 3000일 때 전송).
 * - 종료: 0x00 전송 후 UART deinit.
 */
#include "hht_3000.h"

#include <string.h>
#include <ctype.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/hht/hht_uart.h"
#include "core/hht/hht_display.h"
#include "core/hht/hht_keys.h"
#include "core/hht/hht_session.h"

static const char *TAG = "hht_3000";

#define HHT3K_CMD_EXIT   0x00
#define HHT3K_CMD_REPORT 0x01
#define HHT3K_CMD_BTN    0x02

#define HHT3K_MODE_HHT2K  0x02
#define HHT3K_SUB_SNIFF   0x02

#define HHT3K_LINE_LEN 16
#define REPORT_PREFIX0 "0:"
#define REPORT_PREFIX1 "\r\n1:"
#define REPORT_SUFFIX  "\r\n"

static int hht_3000_read_report_lines(char line0[HHT3K_LINE_LEN + 1], char line1[HHT3K_LINE_LEN + 1])
{
    uint8_t buf[96];
    vTaskDelay(pdMS_TO_TICKS(5));
    int rd = hht_uart_read(buf, sizeof(buf), 100);
    if (rd <= 0) {
        return -1;
    }
    buf[rd] = '\0';
    char *p = (char *)buf;

    char *q = strstr(p, REPORT_PREFIX0);
    if (!q) {
        return -1;
    }
    q += strlen(REPORT_PREFIX0);
    char *r = strstr(q, REPORT_PREFIX1);
    if (!r || (size_t)(r - q) < (size_t)HHT3K_LINE_LEN) {
        return -1;
    }
    memcpy(line0, q, HHT3K_LINE_LEN);
    line0[HHT3K_LINE_LEN] = '\0';
    for (int i = 0; i < HHT3K_LINE_LEN; i++) {
        if (!line0[i]) line0[i] = ' ';
        if (!isprint((unsigned char)line0[i])) line0[i] = ' ';
    }

    q = r + strlen(REPORT_PREFIX1);
    r = strstr(q, REPORT_SUFFIX);
    if (!r || (size_t)(r - q) < (size_t)HHT3K_LINE_LEN) {
        return -1;
    }
    memcpy(line1, q, HHT3K_LINE_LEN);
    line1[HHT3K_LINE_LEN] = '\0';
    for (int i = 0; i < HHT3K_LINE_LEN; i++) {
        if (!line1[i]) line1[i] = ' ';
        if (!isprint((unsigned char)line1[i])) line1[i] = ' ';
    }
    return 0;
}

/** 세션 직후·핸드폰 연결 직후: 첫 Report로 2줄을 BLE에 반드시 올림(WB100 스트림과 동등). */
static void hht_3000_ble_sync_initial(void)
{
    vTaskDelay(pdMS_TO_TICKS(20));

    hht_uart_flush_rx();
    uint8_t cmd = HHT3K_CMD_REPORT;
    hht_uart_write(&cmd, 1);

    char line0[HHT3K_LINE_LEN + 1];
    char line1[HHT3K_LINE_LEN + 1];
    if (hht_3000_read_report_lines(line0, line1) == 0) {
        hht_display_push_row_ascii(0, line0, HHT3K_LINE_LEN, true);
        hht_display_push_row_ascii(1, line1, HHT3K_LINE_LEN, true);
        ESP_LOGI(TAG, "phone BLE: initial 2 lines (Report OK, same path as WB100)");
        return;
    }

    static const char blank16[] = "                ";
    hht_display_push_row_ascii(0, blank16, 16, true);
    hht_display_push_row_ascii(1, blank16, 16, true);
    ESP_LOGW(TAG, "phone BLE: initial Report parse fail — pushed blank 2x16");
}

void hht_3000_init(void)
{
    ESP_LOGI(TAG, "init Command UART IO13/IO14 (UART1→STM32), proto 0x01 Report 0x02+BTN");
    hht_uart_init_3000();
    hht_display_init();
    hht_keys_init();
}

bool hht_3000_session_open(void)
{
    ESP_LOGI(TAG, "session open: mode 0x02 + Sniff");
    hht_uart_flush();
    vTaskDelay(pdMS_TO_TICKS(HHT_SESSION_SETTLE_MS));

    uint8_t mode[] = { HHT3K_MODE_HHT2K, HHT3K_SUB_SNIFF };
    int wn = hht_uart_write(mode, sizeof(mode));
    if (wn < 0 || (size_t)wn != sizeof(mode)) {
        ESP_LOGW(TAG, "mode write failed (%d)", wn);
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(HHT_SESSION_3000_AFTER_MODE_MS));

    uint8_t discard[64];
    (void)hht_uart_read(discard, sizeof(discard), HHT_SESSION_3000_DISCARD_MS);

    hht_3000_ble_sync_initial();
    return true;
}

void hht_3000_session_close(void)
{
    ESP_LOGI(TAG, "session close: 0x00 exit + UART deinit");
    uint8_t cmd = HHT3K_CMD_EXIT;
    (void)hht_uart_write(&cmd, 1);
    vTaskDelay(pdMS_TO_TICKS(HHT_SESSION_3000_STOP_WAIT_MS));
    hht_uart_flush();
    hht_uart_deinit();
}

#define REPORT_INTERVAL_TICKS 10  /* 10 * 10ms = 100ms, ~10Hz */

void hht_3000_poll(void)
{
    static int report_tick = 0;

    report_tick++;
    if (report_tick >= REPORT_INTERVAL_TICKS) {
        report_tick = 0;
        hht_uart_flush_rx();
        uint8_t cmd = HHT3K_CMD_REPORT;
        hht_uart_write(&cmd, 1);

        char line0[HHT3K_LINE_LEN + 1];
        char line1[HHT3K_LINE_LEN + 1];
        if (hht_3000_read_report_lines(line0, line1) == 0) {
            hht_display_push_row_ascii(0, line0, HHT3K_LINE_LEN, false);
            hht_display_push_row_ascii(1, line1, HHT3K_LINE_LEN, false);
        }
    }

    hht_keys_poll();
}
