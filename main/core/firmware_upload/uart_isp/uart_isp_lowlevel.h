#ifndef UART_ISP_LOWLEVEL_H
#define UART_ISP_LOWLEVEL_H

#include <stdint.h>
#include <esp_err.h>

#include "core/firmware_upload/fw_upload_types.h"
#include "lpcisp.h"

typedef struct {
    int rx_gpio;
    int tx_gpio;
    int rts_gpio;
    int cts_gpio;
    uint32_t baudrate;
} uart_isp_cfg_t;

typedef struct {
    lpcispcfg_t cfg;
    partinfo_t  part;
    uint32_t    flash_start;
    uint32_t    flash_size;
} uart_isp_ctx_t;

esp_err_t uart_isp_open(const uart_isp_cfg_t *cfg,
                        const fw_meta_t *meta,
                        uart_isp_ctx_t *out_ctx);

esp_err_t uart_isp_erase_all(uart_isp_ctx_t *ctx);

esp_err_t uart_isp_write_chunk(uart_isp_ctx_t *ctx,
                               uint32_t addr,
                               const uint8_t *data,
                               size_t len);

esp_err_t uart_isp_finalize(uart_isp_ctx_t *ctx);

void uart_isp_close(uart_isp_ctx_t *ctx);

#endif
