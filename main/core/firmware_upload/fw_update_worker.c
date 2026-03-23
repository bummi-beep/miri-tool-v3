#include "fw_update_worker.h"

#include <string.h>
#include <esp_log.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "core/firmware_upload/fw_upload_packaging.h"
#include "core/firmware_upload/fw_upload_state.h"
#include "core/firmware_upload/fw_upload_storage.h"
#include "core/firmware_upload/can_fw_update.h"
#include "core/esp32_update/esp32_update.h"
#include "core/firmware_upload/probe_link_update.h"
#include "core/firmware_upload/uart_isp/uart_isp_update.h"
#include "core/stm32_update/stm32_update.h"
#include "base/sdmmc/sdmmc_init.h"
#include "utils/ihex2bin.h"

static const char *TAG = "fw_update_worker";

#define FW_UPDATE_QUEUE_LEN 2
#define FW_UPDATE_TASK_STACK 12288
#define FW_UPDATE_TASK_PRIO 3

typedef struct {
    fw_meta_t meta;
    char file_name[64];
    char file_type[8];
} fw_update_job_t;

typedef struct {
    size_t total;
    uint32_t last_addr;
} fw_process_stats_t;

static QueueHandle_t s_queue = NULL;
static TaskHandle_t s_task = NULL;

static esp_err_t fw_process_sink(uint32_t addr, const uint8_t *data, size_t len, void *ctx) {
    (void)data;
    fw_process_stats_t *stats = (fw_process_stats_t *)ctx;
    if (stats) {
        stats->total += len;
        stats->last_addr = addr;
    }
    return ESP_OK;
}

static esp_err_t fw_update_exec_ota(const fw_update_job_t *job) {
    if (job && job->meta.meta_json[0] != '\0') {
        cJSON *root = cJSON_Parse(job->meta.meta_json);
        if (root) {
            cJSON *j_reboot = cJSON_GetObjectItem(root, "reboot");
            if (cJSON_IsBool(j_reboot)) {
                esp32_update_set_skip_reboot(!cJSON_IsTrue(j_reboot));
            }
            cJSON_Delete(root);
        }
    }
    if (!esp32_update_set_file(job->file_name, job->file_type)) {
        fw_state_set_step(FW_STEP_ERROR, "OTA file invalid");
        return ESP_FAIL;
    }
    esp32_update_start();
    return ESP_OK;
}

static esp_err_t fw_update_exec_unknown(const fw_update_job_t *job) {
    (void)job;
    fw_state_set_step(FW_STEP_ERROR, "unknown exec");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t fw_update_exec_packaged(const fw_update_job_t *job)
{
    ESP_LOGI(TAG, "fw_update_exec_packaged");

    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        fw_state_set_step(FW_STEP_ERROR, "SDMMC not ready");
        return ESP_FAIL;
    }
    fw_storage_reader_t reader;
    if (fw_storage_reader_open(&reader, job->file_name, job->file_type) != ESP_OK) {
        fw_state_set_step(FW_STEP_ERROR, "file open fail");
        return ESP_FAIL;
    }
    fw_process_stats_t stats = {0};
    esp_err_t ret = fw_packaging_process(&job->meta, &reader, fw_process_sink, &stats);
    fw_storage_reader_close(&reader);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "processed %u bytes (last addr 0x%08x)",
                 (unsigned)stats.total, (unsigned)stats.last_addr);
    } else {
        fw_state_set_step(FW_STEP_ERROR, "process fail");
    }
    return ret;
}

static esp_err_t fw_update_exec_can(const fw_update_job_t *job) {
    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        fw_state_set_step(FW_STEP_ERROR, "SDMMC not ready");
        return ESP_FAIL;
    }
    fw_storage_reader_t reader;
    if (fw_storage_reader_open(&reader, job->file_name, job->file_type) != ESP_OK) {
        fw_state_set_step(FW_STEP_ERROR, "file open fail");
        return ESP_FAIL;
    }
    esp_err_t ret = can_fw_update_from_reader(&job->meta, &reader);
    fw_storage_reader_close(&reader);
    return ret;
}

static esp_err_t fw_update_exec_uart(const fw_update_job_t *job) {
    if (!uart_isp_update_set_file(&job->meta, job->file_name, job->file_type)) {
        fw_state_set_step(FW_STEP_ERROR, "UART ISP file invalid");
        return ESP_FAIL;
    }
    return uart_isp_update_start();
}

static esp_err_t fw_update_exec_stm32_uart(const fw_update_job_t *job) {
    /* S01 전용: STM32F412RET6 UART 부트로더 사용 */
    stm32_update_uart_cfg_t cfg = {
        .uart_num = 1,   /* UART_NUM_1 (드라이버 enum 대신 정수 사용) */
        .tx_io    = 13,   /* CMD TXD: IO13 */
        .rx_io    = 14,   /* CMD RXD: IO14 */
        .baudrate = (int)(job->meta.uart_baud ? job->meta.uart_baud : 115200),
    };

    if (stm32_update_set_file(&job->meta, job->file_name, job->file_type) != ESP_OK) {
        fw_state_set_step(FW_STEP_ERROR, "STM32 file invalid");
        return ESP_FAIL;
    }
    return stm32_update_start(&cfg);
}
static esp_err_t fw_update_exec_swd(const fw_update_job_t *job) {
    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        fw_state_set_step(FW_STEP_ERROR, "SDMMC not ready");
        return ESP_FAIL;
    }
    fw_storage_reader_t reader;
    if (fw_storage_reader_open(&reader, job->file_name, job->file_type) != ESP_OK) {
        fw_state_set_step(FW_STEP_ERROR, "file open fail");
        return ESP_FAIL;
    }
    esp_err_t ret = probe_link_update_from_reader(&job->meta, &reader);
    fw_storage_reader_close(&reader);
    return ret;
}

static esp_err_t fw_update_exec_jtag(const fw_update_job_t *job) {
    return fw_update_exec_swd(job);
}

/* exec_format 과 format 이 다른 경우 (예: IHEX → BIN),
 * SDMMC 상의 원본 파일을 변환해 실행용 파일을 준비한다. */
static esp_err_t fw_prepare_exec_file(fw_update_job_t *job,
                                      char *out_name, size_t out_name_size,
                                      char *out_type, size_t out_type_size)
{
    /* 기본: 변환 없이 원본 파일 그대로 사용 */
    snprintf(out_name, out_name_size, "%s", job->file_name);
    snprintf(out_type, out_type_size, "%s", job->file_type);

    if (job->meta.format == job->meta.exec_format) {
        return ESP_OK;
    }

    /* 현재는 IHEX → BIN 변환만 지원 (추가 포맷은 여기서 확장) */
    if (job->meta.format == FW_FMT_IHEX && job->meta.exec_format == FW_FMT_BIN) {
        char in_path[384];
        char out_path[384];

        if (!fw_storage_build_path(in_path, sizeof(in_path),
                                   job->file_name, job->file_type)) {
            return ESP_ERR_INVALID_ARG;
        }

        /* 변환 결과 파일명: "<원본명>_bin.<원본타입>" 형식 예시 */
        snprintf(out_name, out_name_size, "%s_bin", job->file_name);
        snprintf(out_type, out_type_size, "%s", job->file_type);

        if (!fw_storage_build_path(out_path, sizeof(out_path),
                                   out_name, out_type)) {
            return ESP_ERR_INVALID_ARG;
        }

        FILE *fin  = fopen(in_path,  "rb");
        FILE *fout = fopen(out_path, "wb");
        if (!fin || !fout) {
            if (fin)  fclose(fin);
            if (fout) fclose(fout);
            return ESP_FAIL;
        }

        esp_err_t conv = ihex2bin_file_to_file(fin, fout);

        fclose(fin);
        fclose(fout);

        if (conv != ESP_OK) {
            remove(out_path);
            return conv;
        }

        /* 메타와 job 정보를 실행 포맷(BIN)에 맞게 갱신 */
        job->meta.format = job->meta.exec_format;
        snprintf(job->file_name, sizeof(job->file_name), "%s", out_name);
        snprintf(job->file_type, sizeof(job->file_type), "%s", out_type);

        return ESP_OK;
    }

    /* 아직 지원하지 않는 포맷 조합 */
    return ESP_OK;
}

static void fw_update_worker_task(void *arg) {
    (void)arg;
    fw_update_job_t job;
    for (;;) {
        if (xQueueReceive(s_queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        ESP_LOGI(TAG, "job exec=%s format=%s file=%s.%s",
                 fw_exec_to_str(job.meta.exec),
                 fw_format_to_str(job.meta.format),
                 job.file_name,
                 job.file_type);

        /* 실행 전에 필요하면 포맷 변환(IHEX → BIN 등)을 수행한다. */
        char exec_name[64];
        char exec_type[8];
        esp_err_t prep = fw_prepare_exec_file(&job,
                                              exec_name, sizeof(exec_name),
                                              exec_type, sizeof(exec_type));
        if (prep != ESP_OK) {
            fw_state_set_step(FW_STEP_ERROR, "prepare exec file fail");
            continue;
        }

        switch (job.meta.exec) {
            case FW_EXEC_ESP32_OTA:
                fw_update_exec_ota(&job);
                break;
            case FW_EXEC_CAN:
                fw_update_exec_can(&job);
                break;
            case FW_EXEC_UART_ISP:
                fw_update_exec_uart(&job);
                break;
            case FW_EXEC_STM32_UART:
                fw_update_exec_stm32_uart(&job);
                break;
            case FW_EXEC_SWD:
                fw_update_exec_swd(&job);
                break;
            case FW_EXEC_JTAG:
                fw_update_exec_jtag(&job);
                break;
            case FW_EXEC_BOOTLOADER:
                fw_update_exec_packaged(&job);
                break;
            case FW_EXEC_UNKNOWN:
            default:
                fw_update_exec_unknown(&job);
                break;
        }
    }
}

esp_err_t fw_update_worker_start(void) {
    if (s_queue == NULL) {
        s_queue = xQueueCreate(FW_UPDATE_QUEUE_LEN, sizeof(fw_update_job_t));
        if (s_queue == NULL) {
            ESP_LOGW(TAG, "queue create failed");
            return ESP_FAIL;
        }
    }
    if (s_task == NULL) {
        if (xTaskCreate(fw_update_worker_task, "fw_update_worker",
                        FW_UPDATE_TASK_STACK, NULL, FW_UPDATE_TASK_PRIO, &s_task) != pdPASS) {
            ESP_LOGW(TAG, "task create failed");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t fw_update_enqueue_job(const fw_meta_t *meta, const char *name, const char *type) {
    if (!meta || !name || !type || !s_queue) {
        fw_state_set_step(FW_STEP_ERROR, "invalid job");
        return ESP_ERR_INVALID_ARG;
    }
    fw_update_job_t job;
    memset(&job, 0, sizeof(job));
    job.meta = *meta;
    snprintf(job.file_name, sizeof(job.file_name), "%s", name);
    snprintf(job.file_type, sizeof(job.file_type), "%s", type);
    if (xQueueSend(s_queue, &job, 0) != pdTRUE) {
        fw_state_set_step(FW_STEP_ERROR, "update queue full");
        return ESP_FAIL;
    }
    return ESP_OK;
}
