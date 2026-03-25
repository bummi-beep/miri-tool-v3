#include "hht_wb100.h"

#include <string.h>

#include <esp_log.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

#include "core/hht/hht_uart.h"
#include "core/hht/hht_display.h"
#include "core/hht/hht_keys.h"

#if CONFIG_BT_NIMBLE_ENABLED
#include "protocol/ble_cmd_parser.h"
#include "protocol/ble_protocol.h"
#endif

static const char *TAG = "hht_wb100";

static const uint16_t s_crc16_table[] = {
    0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
    0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
    0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
    0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
    0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
    0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
    0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
    0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
    0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
    0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
    0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
    0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
    0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
    0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
    0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
    0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
    0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
    0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
    0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
    0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
    0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
    0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
    0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
    0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
    0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
    0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
    0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
    0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
    0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
    0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
    0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
    0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040
};

static void make_seed(const uint8_t *digit, uint8_t *result) {
    uint16_t sum = 0xFFFF;
    uint16_t sum_value = 0;

    sum_value = sum ^ digit[0];
    sum = (sum >> 8) ^ s_crc16_table[sum_value & 0x00FF];
    sum_value = sum ^ digit[1];
    sum = (sum >> 8) ^ s_crc16_table[sum_value & 0x00FF];

    result[1] = (sum & 0xff);
    result[0] = ((sum >> 8) & 0xff);
}

static uint8_t make_session(uint8_t *session) {
    uint8_t digit1[2];
    uint8_t digit2[2];
    uint8_t crc1[2];
    uint8_t crc2[2];

    digit1[0] = (uint8_t)(esp_random() & 0xFF);
    digit1[1] = (uint8_t)(esp_random() & 0xFF);
    digit2[0] = (uint8_t)(esp_random() & 0xFF);
    digit2[1] = (uint8_t)(esp_random() & 0xFF);

    make_seed(digit1, crc1);
    make_seed(digit2, crc2);

    session[0] = digit1[0];
    session[1] = digit1[1];
    session[2] = crc1[0];
    session[3] = crc1[1];
    session[4] = digit2[0];
    session[5] = digit2[1];
    session[6] = crc2[0];
    session[7] = crc2[1];

    ESP_LOGI(TAG, "session req: %02x%02x%02x%02x%02x%02x%02x%02x",
             session[0], session[1], session[2], session[3],
             session[4], session[5], session[6], session[7]);
    return 8;
}

static bool verify_session(const uint8_t *buf, size_t len) {
    if (len < 8) {
        return false;
    }
    uint8_t digit1[2] = {buf[0], buf[1]};
    uint8_t digit2[2] = {buf[4], buf[5]};
    uint8_t crc1[2];
    uint8_t crc2[2];

    make_seed(digit1, crc1);
    make_seed(digit2, crc2);

    return (buf[2] == crc1[0] && buf[3] == crc1[1] &&
            buf[6] == crc2[0] && buf[7] == crc2[1]);
}

void hht_wb100_init(void) {
    ESP_LOGI(TAG, "init");
    hht_uart_init();
    hht_display_init();
    hht_keys_init();
}

hht_session_result_t hht_wb100_session_open(void) {
    uint8_t req[8];
    uint8_t resp[8];

    ESP_LOGI(TAG, "session open (8-byte handshake)");
    hht_uart_flush();
    vTaskDelay(pdMS_TO_TICKS(HHT_SESSION_SETTLE_MS));

    uint8_t req_len = make_session(req);
    int wn = hht_uart_write(req, req_len);
    if (wn < 0 || (size_t)wn != (size_t)req_len) {
        ESP_LOGW(TAG, "session request write failed (%d)", wn);
        return HHT_SESSION_ERR_UART;
    }

    int got = 0;
    for (unsigned i = 0; i < HHT_SESSION_WB100_RETRY_MAX; i++) {
        int rd = hht_uart_read(resp + got, 8 - got, HHT_SESSION_WB100_READ_TIMEOUT_MS);
        if (rd > 0) {
            got += rd;
            if (got >= 8) {
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(HHT_SESSION_WB100_RETRY_GAP_MS));
    }

    if (got != 8) {
        ESP_LOGW(TAG, "session response timeout (%d/8 bytes)", got);
        return HHT_SESSION_ERR_TIMEOUT;
    }

    if (!verify_session(resp, 8)) {
        ESP_LOGW(TAG, "session response verify failed");
        return HHT_SESSION_ERR_PROTOCOL;
    }

    ESP_LOGI(TAG, "session response ok");
    return HHT_SESSION_OK;
}

/* 2.08 operation_hht_wb100_session.c 와 동일 문자열 (CP_WBVF_NORMAL 기본) */
static const char s_code_wbvf[] = "WBHHTWBSMARTHHT00012";

void hht_wb100_post_verify_workflow(void) {
#if CONFIG_BT_NIMBLE_ENABLED
    uint8_t wb100_attached = 0x01;
    uint8_t hht3000_off    = 0x00;
    ble_protocol_send_cmd(ACK_START_HHT_WB100, &wb100_attached, 1);
    ble_protocol_send_cmd(ACK_START_HHT_3000, &hht3000_off, 1);
    ESP_LOGI(TAG, "post-verify: BLE 0x8201(01), 0x8203(00)");
#endif

    vTaskDelay(pdMS_TO_TICKS(50));
    const char *ok = "OK";
    int wn         = hht_uart_write((const uint8_t *)ok, strlen(ok));
    if (wn < 0) {
        ESP_LOGW(TAG, "post-verify: OK write failed");
    }

    vTaskDelay(pdMS_TO_TICKS(150));

    wn = hht_uart_write((const uint8_t *)s_code_wbvf, strlen(s_code_wbvf));
    if (wn < 0) {
        ESP_LOGW(TAG, "post-verify: code string write failed");
    }

    uint8_t txb = 0x01;
    (void)hht_uart_write(&txb, 1);
    txb = 0x00;
    hht_uart_flush();
    (void)hht_uart_write(&txb, 1);

    for (int countdown = 10; countdown > 0; countdown--) {
        size_t avail = hht_uart_rx_buffered_bytes();
        if (avail >= 16) {
            uint8_t stack_buf[256];
            size_t  to_read = (avail > sizeof(stack_buf)) ? sizeof(stack_buf) : avail;
            int     rd      = hht_uart_read(stack_buf, to_read, 1000);
            uint8_t packet_proj[10];
            memset(packet_proj, 0, sizeof(packet_proj));
            if (rd >= 16) {
                memcpy(packet_proj, &stack_buf[6], 9);
#if CONFIG_BT_NIMBLE_ENABLED
                ble_protocol_send_cmd(CMD_HHT_PROJ_100, packet_proj, sizeof(packet_proj));
#endif
                ESP_LOGI(TAG, "post-verify: CMD_HHT_PROJ_100 (from CP)");
            } else {
                ESP_LOGW(TAG, "post-verify: proj read short (%d)", rd);
#if CONFIG_BT_NIMBLE_ENABLED
                ble_protocol_send_cmd(CMD_HHT_PROJ_100, packet_proj, sizeof(packet_proj));
#endif
            }
            break;
        }

        uint8_t packet_proj_empty[10] = {0};
#if CONFIG_BT_NIMBLE_ENABLED
        ble_protocol_send_cmd(CMD_HHT_PROJ_100, packet_proj_empty, sizeof(packet_proj_empty));
#endif
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void hht_wb100_session_close(void) {
    ESP_LOGI(TAG, "session close");
    hht_uart_flush();
}

void hht_wb100_poll(void) {
    /*
     * Placeholder polling loop:
     * - read UART stream
     * - update display buffer
     * - process key input
     */
    /*
     * Key TX cadence policy:
     * - hht_task loop is ~20ms
     * - send key every 2 polls (~40ms)
     * Display RX should run every poll regardless of key traffic.
     */
    static uint8_t s_key_tick_div = 0;
    s_key_tick_div++;
    if (s_key_tick_div >= 2) {
        s_key_tick_div = 0;
        (void)hht_keys_poll();
    }
    hht_display_poll();
}
