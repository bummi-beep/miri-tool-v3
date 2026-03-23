#include "fw_upload_meta_defaults.h"

#include <string.h>

#include "core/firmware_upload/fw_upload_types.h"
#include "core/firmware_upload/fw_upload_storage.h"

typedef struct {
    const char    file_type[4]; /* "S00", "A11" 등 */
    const fw_meta_t meta;       /* 이 타입에 대한 기본 meta 값(이름/사이즈 제외) */
} fw_meta_default_entry_t;

/* file_type 별 fw_meta_t 기본값 테이블
 * - file_name, original_size 등은 런타임에 채운다. */
static const fw_meta_default_entry_t k_fw_meta_defaults[] = {
    { "S00", {
        .file_name     = {0},
        .file_type     = "S00",
        .format        = FW_FMT_BIN,
        .exec          = FW_EXEC_ESP32_OTA,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 0,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "S01", {
        .file_name     = {0},
        .file_type     = "S01",
        .format        = FW_FMT_BIN,          /* STM32 부트로더용 순수 BIN */
        .exec          = FW_EXEC_STM32_UART,  /* 새 exec 타입 */
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 115200,              /* 기본 통신 속도 (필요 시 JSON으로 override) */
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A00", {
        .file_name     = {0},
        .file_type     = "A00",
        .format        = FW_FMT_UNKNOWN,
        .exec          = FW_EXEC_UNKNOWN,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 0,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A01", {
        .file_name     = {0},
        .file_type     = "A01",
        .format        = FW_FMT_UNKNOWN,
        .exec          = FW_EXEC_UNKNOWN,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 0,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A02", {
        .file_name     = {0},
        .file_type     = "A02",
        .format        = FW_FMT_COFF,
        .exec          = FW_EXEC_UART_ISP,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 0,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A03", {
        .file_name     = {0},
        .file_type     = "A03",
        .format        = FW_FMT_IHEX,
        .exec          = FW_EXEC_UART_ISP,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 0,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A04", {
        .file_name     = {0},
        .file_type     = "A04",
        .format        = FW_FMT_COFF,
        .exec          = FW_EXEC_UART_ISP,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 0,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A05", {
        .file_name     = {0},
        .file_type     = "A05",
        .format        = FW_FMT_COFF,
        .exec          = FW_EXEC_UART_ISP,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 0,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A06", {
        .file_name     = {0},
        .file_type     = "A06",
        .format        = FW_FMT_IHEX,
        .exec          = FW_EXEC_SWD,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 0,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A07", {
        .file_name     = {0},
        .file_type     = "A07",
        .format        = FW_FMT_COFF,
        .exec          = FW_EXEC_UART_ISP,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 0,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A08", {
        .file_name     = {0},
        .file_type     = "A08",
        .format        = FW_FMT_UNKNOWN,
        .exec          = FW_EXEC_UNKNOWN,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 0,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A09", {
        .file_name     = {0},
        .file_type     = "A09",
        .format        = FW_FMT_UNKNOWN,
        .exec          = FW_EXEC_UNKNOWN,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 0,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A10", {
        .file_name     = {0},
        .file_type     = "A10",
        .format        = FW_FMT_IHEX,
        .exec          = FW_EXEC_UART_ISP,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 115200,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A11", {
        .file_name     = {0},
        .file_type     = "A11",
        .format        = FW_FMT_IHEX,
        .exec          = FW_EXEC_UART_ISP,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 115200, /* A11 기본 baud 예시 */
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A12", {
        .file_name     = {0},
        .file_type     = "A12",
        .format        = FW_FMT_IHEX,
        .exec          = FW_EXEC_UART_ISP,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 57600,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A13", {
        .file_name     = {0},
        .file_type     = "A13",
        .format        = FW_FMT_IHEX,
        .exec          = FW_EXEC_UART_ISP,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 57600,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A14", {
        .file_name     = {0},
        .file_type     = "A14",
        .format        = FW_FMT_IHEX,
        .exec          = FW_EXEC_UART_ISP,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 57600,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
    { "A15", {
        .file_name     = {0},
        .file_type     = "A15",
        .format        = FW_FMT_IHEX,
        .exec          = FW_EXEC_SWD,
        .exec_format   = FW_FMT_BIN,
        .exec_override = false,
        .target_board  = {0},
        .target_id     = {0},
        .can_bitrate   = 0,
        .uart_baud     = 57600,
        .swd_clock_khz = 0,
        .jtag_clock_khz = 0,
        .meta_json     = {0},
        .encrypted     = (FW_STORAGE_ENABLE_ENCRYPT ? true : false),
        .save_only     = false,
        .original_size = 0,
    }},
};

esp_err_t fw_meta_init_defaults(fw_meta_t *meta)
{
    if (!meta) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 테이블에서 file_type 이 같은 엔트리를 찾는다. */
    for (size_t i = 0; i < sizeof(k_fw_meta_defaults) / sizeof(k_fw_meta_defaults[0]); i++) {
        if (strncmp(meta->file_type, k_fw_meta_defaults[i].file_type, 3) == 0) {
            const fw_meta_t *base = &k_fw_meta_defaults[i].meta;
            /* file_name 은 호출자가 이미 채운 값을 유지하고,
             * 나머지 필드들만 기본값으로 덮어쓴다. */
            char name_buf[sizeof(meta->file_name)];
            memcpy(name_buf, meta->file_name, sizeof(name_buf));
            *meta = *base;
            memcpy(meta->file_name, name_buf, sizeof(meta->file_name));
            return ESP_OK;
        }
    }

    /* 매칭되는 타입이 없으면 최소한의 안전한 기본값만 설정 */
    meta->format        = FW_FMT_BIN;
    meta->exec          = FW_EXEC_UNKNOWN;
    meta->exec_format   = FW_FMT_BIN;
    meta->exec_override = false;
    meta->target_board[0] = '\0';
    meta->target_id[0]    = '\0';
    meta->can_bitrate     = 0;
    meta->uart_baud       = 0;
    meta->swd_clock_khz   = 0;
    meta->jtag_clock_khz  = 0;
    meta->meta_json[0]    = '\0';
    meta->encrypted       = (FW_STORAGE_ENABLE_ENCRYPT ? true : false);
    meta->save_only       = false;
    meta->original_size   = 0;

    return ESP_OK;
}

