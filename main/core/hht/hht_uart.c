#include "hht_uart.h"

#include <esp_log.h>
#include <driver/uart.h>

#include "config/pinmap.h"

static const char *TAG = "hht_uart";
static bool g_hht_uart_ready = false;

static const uart_config_t s_uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

void hht_uart_init(void) {
    if (g_hht_uart_ready) {
        return;
    }

    const pinmap_t *pins = pinmap_get();
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 4096, 4096, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &s_uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, pins->hht_tx, pins->hht_rx,
                                 pins->hht_rts, pins->hht_cts));

    ESP_LOGI(TAG, "UART1 (WB100) ready (TX=%d RX=%d RTS=%d CTS=%d)",
             pins->hht_tx, pins->hht_rx, pins->hht_rts, pins->hht_cts);
    g_hht_uart_ready = true;
}

void hht_uart_init_3000(void) {
    const pinmap_t *pins = pinmap_get();
    /* HHT-3000 / STM32 hht2k: Command UART (pinmap cmd_txd/cmd_rxd = IO13/IO14).
     * stm32_update·fw_update_worker 와 동일 물리 링크(UART1 ↔ STM32 USART1). */
    int tx = pins->cmd_txd;
    int rx = pins->cmd_rxd;

    if (g_hht_uart_ready) {
        uart_driver_delete(UART_NUM_1);
        g_hht_uart_ready = false;
    }

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 4096, 4096, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &s_uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART1 (HHT3000) Command UART TX=%d RX=%d (IO13/IO14 → STM32 PA10/PA9)", tx, rx);
    g_hht_uart_ready = true;
}

void hht_uart_deinit(void) {
    if (!g_hht_uart_ready) {
        return;
    }
    uart_driver_delete(UART_NUM_1);
    g_hht_uart_ready = false;
}

void hht_uart_flush(void) {
    if (!g_hht_uart_ready) {
        return;
    }
    uart_flush(UART_NUM_1);
}

void hht_uart_flush_rx(void) {
    if (!g_hht_uart_ready) {
        return;
    }
    uart_flush_input(UART_NUM_1);
}

size_t hht_uart_rx_buffered_bytes(void) {
    size_t n = 0;
    if (!g_hht_uart_ready) {
        return 0;
    }
    if (uart_get_buffered_data_len(UART_NUM_1, &n) != ESP_OK) {
        return 0;
    }
    return n;
}

int hht_uart_read(uint8_t *buf, size_t len, uint32_t timeout_ms) {
    if (!g_hht_uart_ready) {
        return -1;
    }
    return uart_read_bytes(UART_NUM_1, buf, len, pdMS_TO_TICKS(timeout_ms));
}

int hht_uart_write(const uint8_t *buf, size_t len) {
    if (!g_hht_uart_ready) {
        return -1;
    }
    return uart_write_bytes(UART_NUM_1, buf, len);
}
