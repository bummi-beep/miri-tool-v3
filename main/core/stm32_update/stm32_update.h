#ifndef STM32_UPDATE_H
#define STM32_UPDATE_H

#include <stdint.h>
#include <esp_err.h>

#include "core/firmware_upload/fw_upload_types.h"
#include "core/firmware_upload/fw_upload_storage.h"

typedef struct {
    int uart_num;   /* ESP32 UART 포트 번호 (예: UART_NUM_1) */
    int tx_io;      /* ESP32 TXD GPIO (예: IO13) */
    int rx_io;      /* ESP32 RXD GPIO (예: IO14) */
    int baudrate;   /* 기본 통신 속도 (예: 115200) */
} stm32_update_uart_cfg_t;

/* STM32F412RET6 UART 부트로더를 통해 펌웨어를 올리는 고수준 API.
 * - meta/file_name/file_type 는 fw_update_worker 쪽과 동일한 의미.
 * - Boot 모드는 외부에서 이미 잡혀 있다고 가정한다. */

esp_err_t stm32_update_set_file(const fw_meta_t *meta,
                                const char *file_name,
                                const char *file_type);

esp_err_t stm32_update_start(const stm32_update_uart_cfg_t *cfg);

#endif

