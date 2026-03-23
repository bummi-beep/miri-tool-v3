#ifndef FW_UPLOAD_TYPES_H
#define FW_UPLOAD_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    FW_FMT_BIN = 0,
    FW_FMT_IHEX,
    FW_FMT_COFF,
    FW_FMT_ELF,
    FW_FMT_UNKNOWN,
} fw_format_t;

typedef enum {
    FW_EXEC_ESP32_OTA = 0,
    FW_EXEC_UART_ISP,
    FW_EXEC_CAN,
    FW_EXEC_SWD,
    FW_EXEC_JTAG,
    FW_EXEC_BOOTLOADER,
    FW_EXEC_STM32_UART,  /* STM32F4 UART 부트로더용 */
    FW_EXEC_UNKNOWN,
} fw_exec_t;

typedef enum {
    FW_INGRESS_BLE = 0,
    FW_INGRESS_HTTP,
    FW_INGRESS_TCP,
    FW_INGRESS_SDMMC,
} fw_ingress_t;

typedef enum {
    FW_STEP_PREPARE = 0,
    FW_STEP_WAIT_USER,
    FW_STEP_TRANSFER,
    FW_STEP_DECODE,
    FW_STEP_WRITE,
    FW_STEP_VERIFY,
    FW_STEP_DONE,
    FW_STEP_ERROR,
} fw_step_t;


typedef struct {
    char file_name[64];
    char file_type[8]; /* "S00", "A03", ... */
    fw_format_t format;      /* 업로드된 원본 포맷 */
    fw_exec_t   exec;        /* 실행 방식 (UART_ISP, ESP32_OTA, ...) */
    fw_format_t exec_format; /* 실제 실행 시 기대하는 포맷 (예: IHEX 업로드 → BIN 실행) */
    bool exec_override;
    char target_board[32];
    char target_id[16];
    uint32_t can_bitrate;
    uint32_t uart_baud;
    uint32_t swd_clock_khz;
    uint32_t jtag_clock_khz;
    char meta_json[256];
    bool encrypted;
    bool save_only;
    size_t original_size;
} fw_meta_t;

const char *fw_format_to_str(fw_format_t fmt);
fw_format_t fw_format_from_str(const char *str);
const char *fw_exec_to_str(fw_exec_t exec);
fw_exec_t fw_exec_from_str(const char *str);

#endif
