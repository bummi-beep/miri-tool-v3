#include "esp32_update.h"

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <sys/stat.h>
#include <string.h>

#include "base/sdmmc/sdmmc_init.h"
#include "config/storage_paths.h"
#include "protocol/ble_protocol.h"
#include "protocol/ble_cmd_parser.h"
#include "base/console/cli_ota_view.h"
#include "core/firmware_upload/fw_upload_storage.h"
#include "core/firmware_upload/fw_upload_state.h"

static const char *TAG = "esp32_update";
static char s_file_name[64] = {0};
static char s_file_type[8] = {0};
static bool s_file_ready = false;
static bool s_skip_reboot = false;

bool esp32_update_set_file(const char *name, const char *type) {
    if (!name || !type) {
        return false;
    }
    size_t name_len = strlen(name);
    size_t type_len = strlen(type);
    /* 36자(UUID) 또는 BLE 레거시(ota_xxx_yyyy 등) 허용. type은 S00/A00 등 3자. */
    if (name_len == 0 || name_len >= sizeof(s_file_name) || type_len != 3) {
        return false;
    }
    strncpy(s_file_name, name, sizeof(s_file_name) - 1);
    s_file_name[sizeof(s_file_name) - 1] = '\0';
    strncpy(s_file_type, type, sizeof(s_file_type) - 1);
    s_file_type[sizeof(s_file_type) - 1] = '\0';
    s_file_ready = true;
    return true;
}

bool esp32_update_is_ready(void) {
    return s_file_ready;
}

void esp32_update_set_skip_reboot(bool skip) {
    s_skip_reboot = skip;
}

bool esp32_update_get_skip_reboot(void) {
    return s_skip_reboot;
}

static void ota_send_status(const char *status) {
    if (cli_ota_view_is_active()) {
        cli_ota_view_set_status(status);
    }
    if (status && status[0] != '\0') {
        char json[96];
        snprintf(json, sizeof(json), "{\"status\":\"%s\"}", status);
        ble_protocol_send_cmd(ACK_PROGRAM_PROGRESS_MSG_ESP32,
                              (const uint8_t *)json, strlen(json));
    }
}

static void ota_send_progress(int percent) {
    char json[64];
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    fw_state_set_progress((uint8_t)percent);
    snprintf(json, sizeof(json), "{\"progress\":%d}", percent);
    ble_protocol_send_cmd(ACK_PROGRAM_PROGRESS_MSG_ESP32,
                          (const uint8_t *)json, strlen(json));
#if CONFIG_BT_NIMBLE_ENABLED
    /* 앱 진행률 바(%)는 0x8054 1바이트로 표시하는 경우가 많음 — OTA 중에도 동일 전송 */
    {
        uint8_t pct = (uint8_t)percent;
        ble_protocol_send_cmd(ACK_PROGRAM_START_PHONE_ESP32, &pct, 1);
    }
#endif
}

static void ota_send_step(int step, int total, const char *message) {
    if (cli_ota_view_is_active()) {
        cli_ota_view_set_status(message);
    }
    if (message && message[0] != '\0') {
        if (step <= 1) {
            fw_state_set_step(FW_STEP_PREPARE, message);
        } else if (step == 2) {
            fw_state_set_step(FW_STEP_WRITE, message);
        } else if (step == 3 || step == 4) {
            fw_state_set_step(FW_STEP_VERIFY, message);
        } else if (step >= total) {
            fw_state_set_step(FW_STEP_DONE, message);
        }
        char json[128];
        snprintf(json, sizeof(json),
                 "{\"step\":%d,\"total\":%d,\"msg\":\"%s\"}", step, total, message);
        ble_protocol_send_cmd(ACK_PROGRAM_PROGRESS_MSG_ESP32,
                              (const uint8_t *)json, strlen(json));
    }
}

void esp32_update_start(void) {
    if (!s_file_ready) {
        ESP_LOGW(TAG, "OTA file not set");
        fw_state_set_step(FW_STEP_ERROR, "OTA file not set");
        ota_send_status("No file");
        return;
    }

    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        ESP_LOGW(TAG, "SDMMC not ready");
        fw_state_set_step(FW_STEP_ERROR, "SDMMC not ready");
        ota_send_status("SDMMC not ready");
        return;
    }

    fw_storage_reader_t reader;
    if (fw_storage_reader_open(&reader, s_file_name, s_file_type) != ESP_OK) {
        ESP_LOGW(TAG, "OTA file open fail");
        fw_state_set_step(FW_STEP_ERROR, "OTA file open fail");
        ota_send_status("Open fail");
        return;
    }
    size_t total_size = reader.meta.original_size;
    if (!reader.encrypted || total_size == 0) {
        char filepath[384];
        struct stat st;
        snprintf(filepath, sizeof(filepath), "%s/%s.%s",
                 BASE_FILESYSTEM_PATH_SDMMC, s_file_name, s_file_type);
        if (stat(filepath, &st) != 0) {
            fw_storage_reader_close(&reader);
            ESP_LOGW(TAG, "OTA file not found: %s", filepath);
            fw_state_set_step(FW_STEP_ERROR, "OTA file not found");
            ota_send_status("File not found");
            return;
        }
        total_size = (size_t)st.st_size;
    }

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) {
        fw_storage_reader_close(&reader);
        ESP_LOGW(TAG, "No OTA partition");
        fw_state_set_step(FW_STEP_ERROR, "No OTA partition");
        ota_send_status("No OTA partition");
        return;
    }

    ESP_LOGI(TAG, "OTA target: %s @0x%08lx size=%lu",
             part->label, (unsigned long)part->address, (unsigned long)part->size);

    esp_ota_handle_t handle = 0;
    fw_state_set_step(FW_STEP_PREPARE, "OTA begin");
    fw_state_set_progress(0);
    ota_send_step(1, 5, "OTA begin");
    esp_err_t ret = esp_ota_begin(part, total_size, &handle);
    if (ret != ESP_OK) {
        fw_storage_reader_close(&reader);
        ESP_LOGW(TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
        fw_state_set_step(FW_STEP_ERROR, "OTA begin fail");
        ota_send_step(1, 5, "OTA begin fail");
        return;
    }

    fw_state_set_step(FW_STEP_WRITE, "OTA write");
    ota_send_step(2, 5, "OTA write");
    ota_send_progress(0);

    uint8_t buf[4096];
    size_t total = 0;
    int last_percent = -1;
    while (1) {
        size_t rd = fw_storage_reader_read(&reader, buf, sizeof(buf));
        if (rd == 0) {
            break;
        }
        ret = esp_ota_write(handle, buf, rd);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_ota_write failed: %s", esp_err_to_name(ret));
            esp_ota_end(handle);
            fw_storage_reader_close(&reader);
            fw_state_set_step(FW_STEP_ERROR, "OTA write fail");
            ota_send_step(2, 5, "OTA write fail");
            return;
        }
        total += rd;
        int percent = (int)((total * 100UL) / total_size);
        /* 10% 단위로만 전송해 BLE 부하/끊김 방지 (다운로드와 동일) */
        if (percent != last_percent && (percent % 10 == 0)) {
            ota_send_progress(percent);
            last_percent = percent;
        }
    }

    fw_storage_reader_close(&reader);
    fw_state_set_step(FW_STEP_VERIFY, "OTA end");
    ota_send_step(3, 5, "OTA end");
    ret = esp_ota_end(handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_ota_end failed: %s", esp_err_to_name(ret));
        fw_state_set_step(FW_STEP_ERROR, "OTA end fail");
        ota_send_step(3, 5, "OTA end fail");
        return;
    }

    fw_state_set_step(FW_STEP_VERIFY, "Set boot");
    ota_send_step(4, 5, "Set boot");
    ret = esp_ota_set_boot_partition(part);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set boot partition failed: %s", esp_err_to_name(ret));
        fw_state_set_step(FW_STEP_ERROR, "Set boot fail");
        ota_send_step(4, 5, "Set boot fail");
        return;
    }

    ota_send_progress(100);
    fw_state_set_step(FW_STEP_DONE, "OTA success");
    ota_send_step(5, 5, "OTA success");
    if (s_skip_reboot) {
        ESP_LOGI(TAG, "OTA success, skip reboot");
        return;
    }
    ESP_LOGI(TAG, "OTA success, restarting");
    esp_restart();
}
