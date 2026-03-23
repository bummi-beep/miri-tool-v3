#include "probe_link_transport.h"

#include <string.h>

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config/pinmap.h"

#define PROBE_LINK_UART_BUF 2048

static bool s_open = false;
static probe_link_transport_cfg_t s_cfg;

esp_err_t probe_link_transport_open(const probe_link_transport_cfg_t *cfg) {
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg = *cfg;

    if (s_open) {
        return ESP_OK;
    }

    const pinmap_t *pins = pinmap_get();
    uart_config_t uart_config = {
        .baud_rate = s_cfg.baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(s_cfg.uart_port, PROBE_LINK_UART_BUF, PROBE_LINK_UART_BUF, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(s_cfg.uart_port, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(s_cfg.uart_port, pins->cmd_txd, pins->cmd_rxd, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    uart_flush(s_cfg.uart_port);

    s_open = true;
    return ESP_OK;
}

void probe_link_transport_close(void) {
    if (!s_open) {
        return;
    }
    uart_driver_delete(s_cfg.uart_port);
    s_open = false;
}

esp_err_t probe_link_transport_send_packet(const uint8_t *buf, size_t len) {
    if (!s_open || !buf || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    int written = uart_write_bytes(s_cfg.uart_port, (const char *)buf, len);
    if (written < 0 || (size_t)written != len) {
        return ESP_FAIL;
    }
    uart_wait_tx_done(s_cfg.uart_port, pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t probe_link_transport_recv_packet(uint8_t *buf, size_t len, size_t *out_len, uint32_t timeout_ms) {
    if (!s_open || !buf || len == 0 || !out_len) {
        return ESP_ERR_INVALID_ARG;
    }
    int rd = uart_read_bytes(s_cfg.uart_port, buf, len, pdMS_TO_TICKS(timeout_ms));
    if (rd <= 0) {
        *out_len = 0;
        return ESP_ERR_TIMEOUT;
    }
    *out_len = (size_t)rd;
    return ESP_OK;
}
