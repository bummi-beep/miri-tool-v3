#include "uart_isp_lowlevel.h"

#include <string.h>
#include <esp_log.h>
#include "driver/uart.h"
#include "protocol/ble_protocol.h"
#include "protocol/ble_cmd_parser.h"

static const char *TAG = "uart_isp_low";

static esp_err_t uart_isp_uart_init(const uart_isp_cfg_t *cfg)
{
    uart_config_t uart_cfg = {
        .baud_rate = (int)cfg->baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1,
                                 cfg->tx_gpio,
                                 cfg->rx_gpio,
                                 cfg->rts_gpio,
                                 cfg->cts_gpio));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 4096, 4096, 0, NULL, 0));
    return ESP_OK;
}

static uint32_t uart_isp_calc_flash_size(const partinfo_t *p)
{
    uint32_t size = 0;
    if (!p || !p->sectorMap) return 0;
    for (int i = 0; i < p->numSectors; ++i) {
        size += p->sectorMap[i].size;
    }
    return size;
}

esp_err_t uart_isp_open(const uart_isp_cfg_t *cfg,
                        const fw_meta_t *meta,
                        uart_isp_ctx_t *out_ctx)
{
    (void)meta; /* meta는 현재 보레이트 외에는 사용하지 않는다 */

    if (!cfg || !out_ctx) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_ERROR_CHECK(uart_isp_uart_init(cfg));

   // memset(out_ctx, 0, sizeof(*out_ctx));

    /* 2.08과 동일하게: 시리얼로 ID를 읽고 nxp_partdesc의 partDef[]에서 칩/파티션 정보 자동 인식 */
    int ret = LPCISP_Sync(UART_NUM_1,
                          &out_ctx->cfg,
                          (int)cfg->baudrate,
                          0,
                          100,
                          0,
                          1,
                          PIN_NONE,
                          PIN_NONE,
                          &out_ctx->part);
    if (ret != 0) {
        ESP_LOGW(TAG, "LPCISP_Sync failed, ret=%d", ret);
        uart_driver_delete(UART_NUM_1);
        return ESP_ERR_NOT_FOUND;
    }

    if (out_ctx->part.numSectors > 0 && out_ctx->part.sectorMap) {
        out_ctx->flash_start = out_ctx->part.sectorMap[0].base;
        out_ctx->flash_size  = uart_isp_calc_flash_size(&out_ctx->part);
    } else {
        out_ctx->flash_start = 0;
        out_ctx->flash_size  = 0;
    }

    ESP_LOGI(TAG, "UART ISP open: part=%s flash=0x%08x size=%u",
             out_ctx->part.name ? out_ctx->part.name : "unknown",
             (unsigned)out_ctx->flash_start,
             (unsigned)out_ctx->flash_size);
    return ESP_OK;
}

esp_err_t uart_isp_erase_all(uart_isp_ctx_t *ctx)
{
    if (!ctx || !ctx->part.sectorMap || ctx->part.numSectors <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int retval = 0;
    int startSector = 0;
    int endSector   = 0;
    int bank        = 0;

    for (int i = 0; i < ctx->part.numSectors; ++i) {
        if (bank != (int)ctx->part.sectorMap[i].bank) {
            startSector = 0;
            endSector   = 0;
            bank        = (int)ctx->part.sectorMap[i].bank;
        }
        retval = LPCISP_Erase(UART_NUM_1,
                              &ctx->cfg,
                              startSector,
                              endSector,
                              (int)ctx->part.sectorMap[i].bank,
                              &ctx->part);
        if (retval != 0) {
            ESP_LOGW(TAG, "erase failed at sector=%d bank=%d, ret=%d", i, bank, retval);
            return ESP_FAIL;
        }
        startSector++;
        endSector++;
        //ble_packet_send_update_percentage_with_func(50 * (i + 1) / lpc_isp_partinfo.numSectors, __func__, __LINE__, "erase"); // ble_update_per(tms_var.count_erase_flash + 1 * 100 / MAX_INDEX_FLASH);
        uint8_t pct = 50 * (i + 1) / ctx->part.numSectors;
        ble_protocol_send_cmd(ACK_PROGRAM_START_PHONE_ESP32, &pct, 1);


    }

    return ESP_OK;
}

esp_err_t uart_isp_write_chunk(uart_isp_ctx_t *ctx,
                               uint32_t addr,
                               const uint8_t *data,
                               size_t len)
{
    if (!ctx || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int ret = LPCISP_WriteToFlash(UART_NUM_1,
                                  &ctx->cfg,
                                  (unsigned char *)data,
                                  addr,
                                  (unsigned int)len,
                                  &ctx->part);
    if (ret != 0) {
        ESP_LOGW(TAG, "LPCISP_WriteToFlash failed addr=0x%08x len=%u ret=%d",
                 (unsigned)addr, (unsigned)len, ret);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t uart_isp_finalize(uart_isp_ctx_t *ctx)
{
    if (!ctx) return ESP_ERR_INVALID_ARG;
    /* 현재는 별도 동작 없음 (2.08도 Verify는 선택 사항으로만 사용) */
    return ESP_OK;
}

void uart_isp_close(uart_isp_ctx_t *ctx)
{
    (void)ctx;
    uart_driver_delete(UART_NUM_1);
}
