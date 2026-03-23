#include "console_init.h"

#include <esp_log.h>
#include <driver/uart.h>
#include <esp_vfs_dev.h>

#include "config/pinmap.h"

static const char *TAG = "console";

void console_init(void) {
    /*
     * UART0 is the default ESP-IDF console. We install the driver and
     * route stdin/stdout to UART0 so CLI can read from the console.
     *
     * Pin assignment follows the board pinmap (UART0 RX/TX).
     */
    const pinmap_t *pins = pinmap_get();

    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, pins->uart0_tx, pins->uart0_rx, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    esp_vfs_dev_uart_use_driver(UART_NUM_0);

    ESP_LOGI(TAG, "console UART0 ready (TX=%d RX=%d)", pins->uart0_tx, pins->uart0_rx);
}
