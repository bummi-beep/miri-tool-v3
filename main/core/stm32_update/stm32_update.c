#include "stm32_update.h"

#include <string.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "base/sdmmc/sdmmc_init.h"

static const char *TAG = "stm32_update";

/* 업로드에 사용할 SDMMC 파일 정보 (단일 작업 가정) */
static fw_meta_t       s_meta;
static char            s_file_name[64];
static char            s_file_type[8];

/* STM32 UART 부트로더 프로토콜 상수 (AN2606 기반, 간략 버전) */
#define STM32_BL_ACK        0x79
#define STM32_BL_NACK       0x1F
#define STM32_BL_CMD_GET    0x00
#define STM32_BL_CMD_GETID  0x02
#define STM32_BL_CMD_ERASE  0x43
#define STM32_BL_CMD_WRITE  0x31

static esp_err_t stm32_uart_read_byte(int uart_num, uint8_t *out, TickType_t timeout_ticks)
{
    int len = uart_read_bytes(uart_num, out, 1, timeout_ticks);
    return (len == 1) ? ESP_OK : ESP_FAIL;
}

/* 명령 + 체크섬 바이트 전송 */
static esp_err_t stm32_bl_send_cmd(int uart_num, uint8_t cmd)
{
    uint8_t buf[2] = { cmd, (uint8_t)(cmd ^ 0xFF) };
    int res = uart_write_bytes(uart_num, (const char *)buf, 2);
    return (res == 2) ? ESP_OK : ESP_FAIL;
}

/* ACK 대기 */
static esp_err_t stm32_bl_wait_ack(int uart_num, TickType_t timeout_ticks)
{
    uint8_t b = 0;
    if (stm32_uart_read_byte(uart_num, &b, timeout_ticks) != ESP_OK) {
        return ESP_FAIL;
    }
    if (b == STM32_BL_ACK) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "bootloader NACK or unexpected: 0x%02X", b);
    return ESP_FAIL;
}

/* 간단한 전체 erase (Extended Erase 대신 Global Erase 0xFFFF 사용) */
static esp_err_t stm32_bl_global_erase(int uart_num)
{
    ESP_LOGI(TAG, "STM32 erase all...");
    if (stm32_bl_send_cmd(uart_num, STM32_BL_CMD_ERASE) != ESP_OK) {
        return ESP_FAIL;
    }
    if (stm32_bl_wait_ack(uart_num, pdMS_TO_TICKS(500)) != ESP_OK) {
        return ESP_FAIL;
    }

    /* 0xFFFF + checksum 전송 → 전체 erase */
    uint8_t payload[3];
    payload[0] = 0xFF;
    payload[1] = 0xFF;
    payload[2] = (uint8_t)(payload[0] ^ payload[1]);
    int res = uart_write_bytes(uart_num, (const char *)payload, sizeof(payload));
    if (res != sizeof(payload)) {
        return ESP_FAIL;
    }
    return stm32_bl_wait_ack(uart_num, pdMS_TO_TICKS(5000));
}

/* 플래시에 한 블록(최대 256바이트) 쓰기 */
static esp_err_t stm32_bl_write_block(int uart_num, uint32_t addr,
                                      const uint8_t *data, size_t len)
{
    if (len == 0 || len > 256) {
        return ESP_ERR_INVALID_ARG;
    }

    if (stm32_bl_send_cmd(uart_num, STM32_BL_CMD_WRITE) != ESP_OK) {
        return ESP_FAIL;
    }
    if (stm32_bl_wait_ack(uart_num, pdMS_TO_TICKS(500)) != ESP_OK) {
        return ESP_FAIL;
    }

    /* 주소(4바이트) + 체크섬 */
    uint8_t addr_buf[5];
    addr_buf[0] = (uint8_t)(addr >> 24);
    addr_buf[1] = (uint8_t)(addr >> 16);
    addr_buf[2] = (uint8_t)(addr >> 8);
    addr_buf[3] = (uint8_t)(addr);
    addr_buf[4] = addr_buf[0] ^ addr_buf[1] ^ addr_buf[2] ^ addr_buf[3];
    int res = uart_write_bytes(uart_num, (const char *)addr_buf, sizeof(addr_buf));
    if (res != sizeof(addr_buf)) {
        return ESP_FAIL;
    }
    if (stm32_bl_wait_ack(uart_num, pdMS_TO_TICKS(500)) != ESP_OK) {
        return ESP_FAIL;
    }

    /* 길이(N) + 데이터 + 체크섬 */
    uint8_t len_byte = (uint8_t)(len - 1);
    uint8_t cks = len_byte;
    for (uint8_t i = 0; i < len; i++) {
        cks ^= data[i];
    }
    uart_write_bytes(uart_num, (const char *)&len_byte, 1);
    uart_write_bytes(uart_num, (const char *)data, len);
    uart_write_bytes(uart_num, (const char *)&cks, 1);

    return stm32_bl_wait_ack(uart_num, pdMS_TO_TICKS(500));
}

esp_err_t stm32_update_set_file(const fw_meta_t *meta,
                                const char *file_name,
                                const char *file_type)
{
    if (!meta || !file_name || !file_type) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(&s_meta, 0, sizeof(s_meta));
    s_meta = *meta;
    snprintf(s_file_name, sizeof(s_file_name), "%s", file_name);
    snprintf(s_file_type, sizeof(s_file_type), "%.3s", file_type);
    return ESP_OK;
}

esp_err_t stm32_update_start(const stm32_update_uart_cfg_t *cfg)
{
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "STM32 update start: file=%s.%s baud=%d uart=%d",
             s_file_name, s_file_type, cfg->baudrate, cfg->uart_num);

    /* UART 설정 (기본: IO13 TX, IO14 RX, 8N1) */
    uart_config_t ucfg = {
        .baud_rate = cfg->baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_param_config(cfg->uart_num, &ucfg);
    uart_set_pin(cfg->uart_num, cfg->tx_io, cfg->rx_io,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(cfg->uart_num, 1024, 1024, 0, NULL, 0);

    /* STM32 부트로더 sync 시퀀스:
     * - 0x7F 전송
     * - ACK 대기
     */
    uint8_t init_byte = 0x7F;
    uart_flush_input(cfg->uart_num);
    uart_write_bytes(cfg->uart_num, (const char *)&init_byte, 1);
    if (stm32_bl_wait_ack(cfg->uart_num, pdMS_TO_TICKS(500)) != ESP_OK) {
        ESP_LOGE(TAG, "STM32 bootloader sync failed");
        uart_driver_delete(cfg->uart_num);
        return ESP_FAIL;
    }

    /* 여기서부터는 매우 단순화된 흐름:
     * 1) 글로벌 erase
     * 2) SDMMC에서 바이너리를 순차적으로 읽어와서 256바이트 단위로 WRITE MEMORY
     *    - 시작 주소는 F412RET6의 Flash Base(0x08000000) 로 가정.
     */
    if (stm32_bl_global_erase(cfg->uart_num) != ESP_OK) {
        ESP_LOGE(TAG, "STM32 erase failed");
        uart_driver_delete(cfg->uart_num);
        return ESP_FAIL;
    }

    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        ESP_LOGE(TAG, "SDMMC not ready");
        uart_driver_delete(cfg->uart_num);
        return ESP_FAIL;
    }

    fw_storage_reader_t reader;
    if (fw_storage_reader_open(&reader, s_file_name, s_file_type) != ESP_OK) {
        ESP_LOGE(TAG, "open firmware file fail");
        uart_driver_delete(cfg->uart_num);
        return ESP_FAIL;
    }

    uint8_t  buf[256];
    uint32_t addr = 0x08000000u; /* STM32F412RET6 Flash start */
    size_t   rd;

    while ((rd = fw_storage_reader_read(&reader, buf, sizeof(buf))) > 0) {
        if (stm32_bl_write_block(cfg->uart_num, addr, buf, (uint8_t)rd) != ESP_OK) {
            ESP_LOGE(TAG, "write block failed at 0x%08X", (unsigned)addr);
            fw_storage_reader_close(&reader);
            uart_driver_delete(cfg->uart_num);
            return ESP_FAIL;
        }
        addr += rd;
    }

    fw_storage_reader_close(&reader);
    uart_driver_delete(cfg->uart_num);
    ESP_LOGI(TAG, "STM32 update complete, last addr=0x%08X", (unsigned)addr);
    return ESP_OK;
}

