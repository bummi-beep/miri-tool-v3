#ifndef UART_ISP_UPDATE_H
#define UART_ISP_UPDATE_H

#include <stdbool.h>
#include <esp_err.h>

#include "core/firmware_upload/fw_upload_types.h"

bool uart_isp_update_set_file(const fw_meta_t *meta,
                              const char *name,
                              const char *type);

esp_err_t uart_isp_update_start(void);

#endif
