#include "uart_isp_update.h"
#include "uart_isp_lowlevel.h"

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <esp_log.h>
#include <driver/uart.h>
#include <driver/gpio.h>

#include "base/sdmmc/sdmmc_init.h"
#include "config/storage_paths.h"
#include "core/firmware_upload/fw_upload_storage.h"
#include "core/firmware_upload/fw_upload_state.h"
#include "protocol/ble_protocol.h"
#include "protocol/ble_cmd_parser.h"

static const char *TAG = "uart_isp_update";

static fw_meta_t s_meta;
static char s_file_name[64];
static char s_file_type[8];
static bool s_file_ready = false;

bool uart_isp_update_set_file(const fw_meta_t *meta,
                              const char *name,
                              const char *type)
{
    if (!meta || !name || !type) return false;

    size_t name_len = strlen(name);
    size_t type_len = strlen(type);
    if (name_len == 0 || name_len >= sizeof(s_file_name) || type_len != 3) {
        return false;
    }
    s_meta = *meta;
    strncpy(s_file_name, name, sizeof(s_file_name) - 1);
    s_file_name[sizeof(s_file_name) - 1] = '\0';
    strncpy(s_file_type, type, sizeof(s_file_type) - 1);
    s_file_type[sizeof(s_file_type) - 1] = '\0';
    s_file_ready = true;
    return true;
}

static void isp_send_status(const char *status)
{
    if (!status || !status[0]) return;

    char json[96];
    snprintf(json, sizeof(json), "{\"status\":\"%s\"}", status);
    ble_protocol_send_cmd(ACK_PROGRAM_PROGRESS_MSG_ESP32,
                          (const uint8_t *)json, strlen(json));
}

static void isp_send_progress(int percent)
{
    char json[64];
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    fw_state_set_progress((uint8_t)percent);
    snprintf(json, sizeof(json), "{\"progress\":%d}", percent);
    ble_protocol_send_cmd(ACK_PROGRAM_PROGRESS_MSG_ESP32,
                          (const uint8_t *)json, strlen(json));
#if CONFIG_BT_NIMBLE_ENABLED
    {
        uint8_t pct = (uint8_t)percent;
        ble_protocol_send_cmd(ACK_PROGRAM_START_PHONE_ESP32, &pct, 1);
    }
#endif
}

static void isp_send_step(int step, int total, const char *message)
{
    if (!message || !message[0]) return;

    if (step <= 1) {
        fw_state_set_step(FW_STEP_PREPARE, message);
    } else if (step == 2) {
        fw_state_set_step(FW_STEP_WRITE, message);
    } else if (step >= total) {
        fw_state_set_step(FW_STEP_VERIFY, message);
    }

    char json[128];
    snprintf(json, sizeof(json),
             "{\"step\":%d,\"total\":%d,\"msg\":\"%s\"}", step, total, message);
    ble_protocol_send_cmd(ACK_PROGRAM_PROGRESS_MSG_ESP32,
                          (const uint8_t *)json, strlen(json));
}

esp_err_t uart_isp_update_start(void)
{
    if (!s_file_ready) {
        ESP_LOGW(TAG, "UART ISP file not set");
        fw_state_set_step(FW_STEP_ERROR, "UART ISP file not set");
        isp_send_status("No file");
        return ESP_FAIL;
    }

    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        ESP_LOGW(TAG, "SDMMC not ready");
        fw_state_set_step(FW_STEP_ERROR, "SDMMC not ready");
        isp_send_status("SDMMC not ready");
        return ESP_FAIL;
    }

    fw_storage_reader_t reader;
    if (fw_storage_reader_open(&reader, s_file_name, s_file_type) != ESP_OK) {
        ESP_LOGW(TAG, "UART ISP file open fail");
        fw_state_set_step(FW_STEP_ERROR, "UART ISP file open fail");
        isp_send_status("Open fail");
        return ESP_FAIL;
    }

    size_t total_size = reader.meta.original_size;
    if (!reader.encrypted || total_size == 0) {
        char filepath[384];
        struct stat st;
        snprintf(filepath, sizeof(filepath), "%s/%s.%s",
                 BASE_FILESYSTEM_PATH_SDMMC, s_file_name, s_file_type);
        if (stat(filepath, &st) != 0) {
            fw_storage_reader_close(&reader);
            fw_state_set_step(FW_STEP_ERROR, "UART ISP file not found");
            isp_send_status("File not found");
            return ESP_FAIL;
        }
        total_size = (size_t)st.st_size;
    }
// 임시 for A11 EMON
   // s_meta.uart_baud = 115200;

    uart_isp_cfg_t cfg = {
        .rx_gpio = 9,
        .tx_gpio = 10,
        .rts_gpio = 11,//UART_PIN_NO_CHANGE, //
        .cts_gpio = 12,//UART_PIN_NO_CHANGE, // 12,//
        .baudrate = s_meta.uart_baud ? s_meta.uart_baud : 57600,
    };
    uart_isp_ctx_t ctx;

    ctx.cfg.echo = 1;
    ctx.cfg.ispPin = 11; // RTS
    ctx.cfg.resetPin = 12; // CTS
   // ctx.cfg.lineTermination;

    ESP_LOGI(TAG, "uart_isp_cfg  baudrate %ld",cfg.baudrate);
    isp_send_progress(0);
    /* 2.08 LPC ISP 흐름과 맞추기: DEVICE ENTER MODE → DEVICE ATTACHED → ERASE FLASH → FLASHING START → Completed */
    isp_send_step(1, 5, "DEVICE ENTER MODE");
    esp_err_t ret = uart_isp_open(&cfg, &s_meta, &ctx);
    if (ret != ESP_OK) {
        fw_storage_reader_close(&reader);
        fw_state_set_step(FW_STEP_ERROR, "DEVICE NOT FOUND");
        isp_send_status("DEVICE NOT FOUND");
        return ret;
    }

    isp_send_step(2, 5, "DEVICE ATTACHED");

    isp_send_step(3, 5, "ERASE FLASH");
    ret = uart_isp_erase_all(&ctx);
    if (ret != ESP_OK) {
        uart_isp_close(&ctx);
        fw_storage_reader_close(&reader);
        fw_state_set_step(FW_STEP_ERROR, "ERASE FLASH ERROR");
        isp_send_status("ERASE FLASH ERROR");
        return ret;
    }

    isp_send_step(4, 5, "ERASE FLASH SUCCESS");
    /* erase 구간 완료 시 50%로 고정 */
    isp_send_progress(50);

    fw_state_set_step(FW_STEP_WRITE, "FLASHING START");
    isp_send_step(4, 5, "FLASHING START");

    /* 2.08처럼 전체 이미지를 한 번에 플래시에 쓰기 위해 버퍼에 모두 적재 */
    uint8_t *image = (uint8_t *)heap_caps_calloc(1, total_size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!image) {
        image = (uint8_t *)calloc(1, total_size + 1);
    }

//    uint8_t *image = (uint8_t *)malloc(total_size);
    if (!image) {
        uart_isp_close(&ctx);
        fw_storage_reader_close(&reader);
        fw_state_set_step(FW_STEP_ERROR, "UART ISP no memory");
        isp_send_status("No memory");
        return ESP_ERR_NO_MEM;
    }

    size_t total = 0;
    while (total < total_size) {
        size_t rd = fw_storage_reader_read(&reader, image + total, total_size - total);
        if (rd == 0) {
            break;
        }
        total += rd;
        /* 파일 읽기 단계는 진행률(%)와 무관하게 처리한다. */
    }

    fw_storage_reader_close(&reader);

    if (total != total_size) {
        free(image);
        uart_isp_close(&ctx);
        fw_state_set_step(FW_STEP_ERROR, "UART ISP read mismatch");
        isp_send_status("Read mismatch");
        return ESP_FAIL;
    }

    /* 저수준은 LPCISP_WriteToFlash 가 알아서 섹터/블록 단위로 처리 */
    ret = uart_isp_write_chunk(&ctx, ctx.flash_start, image, total_size);
    free(image);
    if (ret != ESP_OK) {
        uart_isp_close(&ctx);
        fw_state_set_step(FW_STEP_ERROR, "FLASHING ERROR");
        isp_send_status("FLASHING ERROR");
        return ret;
    }

    fw_state_set_step(FW_STEP_VERIFY, "Completed");
    isp_send_step(5, 5, "Completed");
    ret = uart_isp_finalize(&ctx);
    uart_isp_close(&ctx);
    if (ret != ESP_OK) {
        fw_state_set_step(FW_STEP_ERROR, "UART ISP finalize fail");
        isp_send_status("UART ISP finalize fail");
        return ret;
    }

    isp_send_progress(100);
    fw_state_set_step(FW_STEP_DONE, "UART ISP success");
    isp_send_status("UART ISP success");
    return ESP_OK;
}
