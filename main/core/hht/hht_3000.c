/*
 * HHT 3000: 문서(HHT2K_UART_ARCHITECTURE.md) 제안 프로토콜.
 * - 진입: 모드 'H'(0x02) + 서브모드 Sniff('S', 0x02) → STM32 Sniff 루프.
 * - 디스플레이: 0x01(Report) 전송 → "0:<line0>\r\n1:<line1>\r\n" 수신 후 BLE/CLI 갱신.
 * - 버튼: 0x02 + 1바이트 mask (hht_keys에서 3000일 때 전송).
 * - 종료: 0x00 전송 후 UART deinit.
 */
#include "hht_3000.h"

#include <string.h>
#include <ctype.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/hht/hht_main.h"
#include "core/hht/hht_uart.h"
#include "core/hht/hht_display.h"
#include "core/hht/hht_keys.h"
#include "protocol/ble_protocol.h"
#include "base/console/cli_hht_view.h"

static const char *TAG = "hht_3000";

/* 문서 제안 명령 코드 (STM32 Sniff/Drive와 동일) */
#define HHT3K_CMD_EXIT   0x00
#define HHT3K_CMD_REPORT 0x01
#define HHT3K_CMD_BTN    0x02

#define HHT3K_MODE_HHT2K  0x02
#define HHT3K_SUB_SNIFF   0x02

#define HHT3K_LINE_LEN 16
#define HHT3K_ROWS     2
#define REPORT_PREFIX0 "0:"
#define REPORT_PREFIX1 "\r\n1:"
#define REPORT_SUFFIX  "\r\n"

/* Report 응답 수신: flush 후 0x01 전송, 잠시 대기 후 한 번 읽어 파싱 */
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

static void hht_3000_notify_line(uint8_t row, const char line16[HHT3K_LINE_LEN + 1])
{
    char line20[21];
    memcpy(line20, line16, HHT3K_LINE_LEN);
    for (int i = HHT3K_LINE_LEN; i < 20; i++) {
        line20[i] = ' ';
    }
    line20[20] = '\0';
#if CONFIG_BT_NIMBLE_ENABLED
    ble_protocol_send_hht_line(row, line20);
#endif
    if (cli_hht_view_is_active()) {
        cli_hht_view_update_line(row, line20);
    }
}

void hht_3000_init(void)
{
    ESP_LOGI(TAG, "init (UART1 TX=4 RX=5, protocol 0x01 Report 0x02 BTN)");
    hht_uart_init_3000();
    hht_display_init();
    hht_keys_init();
}

void hht_3000_start_session(void)
{
    ESP_LOGI(TAG, "start session: mode H + Sniff");
    hht_uart_flush();
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t mode[] = { HHT3K_MODE_HHT2K, HHT3K_SUB_SNIFF };
    hht_uart_write(mode, sizeof(mode));
    vTaskDelay(pdMS_TO_TICKS(80));

    /* STM32가 "SNIFF; 0x00=exit..." 를 보낼 수 있음: 버림 */
    uint8_t discard[64];
    (void)hht_uart_read(discard, sizeof(discard), 60);
}

void hht_3000_stop_session(void)
{
    ESP_LOGI(TAG, "stop session: send 0x00 exit");
    uint8_t cmd = HHT3K_CMD_EXIT;
    hht_uart_write(&cmd, 1);
    vTaskDelay(pdMS_TO_TICKS(30));
    hht_uart_flush();
    hht_uart_deinit();
}

#define REPORT_INTERVAL_TICKS 10  /* 10 * 10ms = 100ms, ~10Hz */

void hht_3000_poll(void)
{
    static int report_tick = 0;
    static char s_last0[HHT3K_LINE_LEN + 1] = {0};
    static char s_last1[HHT3K_LINE_LEN + 1] = {0};

    report_tick++;
    if (report_tick >= REPORT_INTERVAL_TICKS) {
        report_tick = 0;
        hht_uart_flush_rx();
        uint8_t cmd = HHT3K_CMD_REPORT;
        hht_uart_write(&cmd, 1);

        char line0[HHT3K_LINE_LEN + 1];
        char line1[HHT3K_LINE_LEN + 1];
        if (hht_3000_read_report_lines(line0, line1) == 0) {
            if (memcmp(line0, s_last0, HHT3K_LINE_LEN) != 0) {
                memcpy(s_last0, line0, HHT3K_LINE_LEN + 1);
                hht_3000_notify_line(0, line0);
            }
            if (memcmp(line1, s_last1, HHT3K_LINE_LEN) != 0) {
                memcpy(s_last1, line1, HHT3K_LINE_LEN + 1);
                hht_3000_notify_line(1, line1);
            }
        }
    }

    hht_keys_poll();
}
