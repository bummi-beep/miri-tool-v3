#include "can_fw_update.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
#include <cJSON.h>

#include "base/can/incan.h"
#include "config/storage_paths.h"
#include "core/firmware_upload/fw_upload_packaging.h"
#include "core/firmware_upload/fw_upload_state.h"

static const char *TAG = "can_update";

#define CAN_ID_ESP32_BOARD 0x1F000074
#define CAN_ID_TMS_BOARD   0x1F00007B
#define LEN_TMS_MESSAGE    8

#define CHECK_FRAME_SINGLE(a) ((a & 0xF0) == 0x00)
#define CHECK_FRAME_FLOWCONTROL(a) ((a & 0xF0) == 0x30)
#define SID_POSITIVE_RESPONSE 0x40
#define GET_ID_RESPONSE(a) ((a) | SID_POSITIVE_RESPONSE)

enum {
    SID_TARGET_DOWNLOAD_START    = 0x10,
    SID_TARGET_BOOT_RUN_CHECK    = 0x11,
    SID_TARGET_DOWNLOAD_FINISH   = 0x12,
    SID_TARGET_FLASH_ERASE       = 0x20,
    SID_TARGET_USER_SW_DATA_SEND = 0x21,
    SID_TARGET_FLASH_WRITE       = 0x22,
};

#define TIMEOUT_LONG   1000
#define TIMEOUT_WRITE  20
#define MAX_INDEX_FLASH 16
#define SIZE_FLASH_ONE_PAGE 1024

typedef struct {
    FILE *file;
    uint32_t offset;
    uint32_t max;
} bin_sink_ctx_t;

static uint32_t parse_hex_or_dec(const char *s) {
    if (!s || s[0] == '\0') {
        return 0;
    }
    char *end = NULL;
    unsigned long val = strtoul(s, &end, 0);
    if (end == s) {
        return 0;
    }
    return (uint32_t)val;
}

static void apply_meta_json_overrides(const fw_meta_t *meta, uint32_t *out_target_id, uint32_t *out_bitrate_fast) {
    if (!meta || !out_target_id || !out_bitrate_fast) {
        return;
    }
    if (meta->meta_json[0] == '\0') {
        return;
    }
    cJSON *root = cJSON_Parse(meta->meta_json);
    if (!root) {
        ESP_LOGW(TAG, "meta_json parse failed");
        return;
    }
    cJSON *j_can_id = cJSON_GetObjectItem(root, "can_id");
    cJSON *j_target_id = cJSON_GetObjectItem(root, "target_id");
    cJSON *j_bitrate = cJSON_GetObjectItem(root, "can_bitrate");

    if (cJSON_IsString(j_can_id)) {
        uint32_t v = parse_hex_or_dec(j_can_id->valuestring);
        if (v) {
            *out_target_id = v;
        }
    }
    if (cJSON_IsString(j_target_id)) {
        uint32_t v = parse_hex_or_dec(j_target_id->valuestring);
        if (v) {
            *out_target_id = v;
        }
    }
    if (cJSON_IsNumber(j_bitrate)) {
        uint32_t v = (uint32_t)j_bitrate->valuedouble;
        if (v) {
            *out_bitrate_fast = v;
        }
    }
    cJSON_Delete(root);
}

static esp_err_t bin_sink(uint32_t addr, const uint8_t *data, size_t len, void *ctx) {
    bin_sink_ctx_t *state = (bin_sink_ctx_t *)ctx;
    if (!state || !state->file) {
        return ESP_ERR_INVALID_ARG;
    }
    if (addr < state->offset) {
        return ESP_FAIL;
    }
    if (addr > state->offset) {
        uint32_t gap = addr - state->offset;
        uint8_t pad[64];
        memset(pad, 0xFF, sizeof(pad));
        while (gap > 0) {
            size_t chunk = (gap > sizeof(pad)) ? sizeof(pad) : gap;
            fwrite(pad, 1, chunk, state->file);
            gap -= (uint32_t)chunk;
        }
        state->offset = addr;
    }
    fwrite(data, 1, len, state->file);
    state->offset += (uint32_t)len;
    if (state->offset > state->max) {
        state->max = state->offset;
    }
    return ESP_OK;
}

static esp_err_t tms_send_cmd(uint8_t cmd, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6) {
    uint8_t tx_buff[LEN_TMS_MESSAGE] = {0x01, cmd, b2, b3, b4, b5, b6, 0x00};
    return incan_tx(CAN_ID_ESP32_BOARD, TWAI_MSG_FLAG_EXTD, LEN_TMS_MESSAGE, tx_buff, TIMEOUT_LONG);
}

static esp_err_t tms_flash_write(uint32_t offset, uint16_t size) {
    uint8_t tx_buff[LEN_TMS_MESSAGE] = {0x06, SID_TARGET_FLASH_WRITE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    tx_buff[2] = (offset & 0x0000FF);
    tx_buff[3] = (offset & 0x00FF00) >> 8;
    tx_buff[4] = (offset & 0xFF0000) >> 16;
    tx_buff[5] = (size & 0x00FF);
    tx_buff[6] = (size & 0xFF00) >> 8;
    return incan_tx(CAN_ID_ESP32_BOARD, TWAI_MSG_FLAG_EXTD, LEN_TMS_MESSAGE, tx_buff, TIMEOUT_LONG);
}

static esp_err_t tms_user_sw_send(uint8_t *data, uint16_t size) {
    esp_err_t ret = ESP_OK;
    uint16_t total_len = size + 1;
    uint16_t length_sent = 0;
    uint8_t tx_buff[LEN_TMS_MESSAGE] = {
        (uint8_t)(0x10 | ((total_len & 0xF00) >> 8)),
        (uint8_t)(total_len & 0xFF),
        SID_TARGET_USER_SW_DATA_SEND, 0, 0, 0, 0, 0
    };
    memcpy(&tx_buff[3], data, 5);
    length_sent = 5;

    ret = incan_tx(CAN_ID_ESP32_BOARD, TWAI_MSG_FLAG_EXTD, LEN_TMS_MESSAGE, tx_buff, TIMEOUT_LONG);
    if (ret != ESP_OK) {
        return ret;
    }

    twai_message_t can_msg;
    while (1) {
        if (incan_get_message(&can_msg, 1000)) {
            if (can_msg.identifier == CAN_ID_TMS_BOARD && CHECK_FRAME_FLOWCONTROL(can_msg.data[0])) {
                break;
            }
        }
    }

    tx_buff[0] = 0x21;
    while (length_sent < total_len) {
        memset(&tx_buff[1], 0x00, 7);
        size_t send_len = ((total_len - length_sent) >= 7) ? 7 : (total_len - length_sent);
        memcpy(&tx_buff[1], &data[length_sent], send_len);
        ret = incan_tx(CAN_ID_ESP32_BOARD, TWAI_MSG_FLAG_EXTD, LEN_TMS_MESSAGE, tx_buff, TIMEOUT_WRITE);
        if (ret != ESP_OK) {
            return ret;
        }
        length_sent += (uint16_t)send_len;
        tx_buff[0] = (uint8_t)((tx_buff[0] + 1) & 0x2F);
    }

    while (1) {
        if (incan_get_message(&can_msg, 1000)) {
            if (can_msg.identifier == CAN_ID_TMS_BOARD) {
                break;
            }
        }
    }
    if (!(CHECK_FRAME_SINGLE(can_msg.data[0]) &&
          (can_msg.data[1] == GET_ID_RESPONSE(SID_TARGET_USER_SW_DATA_SEND)))) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t tms_wait_response(uint8_t sid, uint32_t target_id, uint32_t timeout_ms) {
    twai_message_t can_msg;
    uint32_t waited = 0;
    while (waited < timeout_ms) {
        if (incan_get_message(&can_msg, 100)) {
            if (can_msg.identifier == target_id &&
                CHECK_FRAME_SINGLE(can_msg.data[0]) &&
                can_msg.data[1] == GET_ID_RESPONSE(sid)) {
                return ESP_OK;
            }
        }
        waited += 100;
    }
    return ESP_FAIL;
}

static esp_err_t can_send_file(FILE *f, uint32_t size, uint32_t target_id, uint32_t bitrate_fast) {
    fw_state_set_step(FW_STEP_PREPARE, "CAN enter boot");
    if (incan_open(125000, target_id) != ESP_OK) {
        return ESP_FAIL;
    }
    if (tms_send_cmd(SID_TARGET_DOWNLOAD_START, 0, 0, 0, 0, 0) != ESP_OK) {
        return ESP_FAIL;
    }
    if (tms_wait_response(SID_TARGET_DOWNLOAD_START, target_id, TIMEOUT_LONG) != ESP_OK) {
        return ESP_FAIL;
    }

    fw_state_set_step(FW_STEP_VERIFY, "CAN boot check");
    if (incan_open(bitrate_fast, target_id) != ESP_OK) {
        return ESP_FAIL;
    }
    if (tms_send_cmd(SID_TARGET_BOOT_RUN_CHECK, 0, 0, 0, 0, 0) != ESP_OK) {
        return ESP_FAIL;
    }
    if (tms_wait_response(SID_TARGET_BOOT_RUN_CHECK, target_id, TIMEOUT_LONG) != ESP_OK) {
        return ESP_FAIL;
    }

    fw_state_set_step(FW_STEP_WRITE, "CAN erase");
    for (uint8_t i = 0; i < MAX_INDEX_FLASH; i++) {
        if (tms_send_cmd(SID_TARGET_FLASH_ERASE, i, 0, 0, 0, 0) != ESP_OK) {
            return ESP_FAIL;
        }
        if (tms_wait_response(SID_TARGET_FLASH_ERASE, target_id, TIMEOUT_LONG) != ESP_OK) {
            return ESP_FAIL;
        }
        fw_state_set_progress((uint8_t)((i + 1) * 100 / MAX_INDEX_FLASH));
    }

    fw_state_set_step(FW_STEP_WRITE, "CAN write");
    uint8_t buf[SIZE_FLASH_ONE_PAGE];
    uint32_t offset = 0;
    while (offset < size) {
        size_t to_read = (size - offset) > sizeof(buf) ? sizeof(buf) : (size - offset);
        size_t rd = fread(buf, 1, to_read, f);
        if (rd == 0) {
            break;
        }
        if (tms_user_sw_send(buf, (uint16_t)rd) != ESP_OK) {
            return ESP_FAIL;
        }
        if (tms_flash_write(offset, (uint16_t)rd) != ESP_OK) {
            return ESP_FAIL;
        }
        if (tms_wait_response(SID_TARGET_FLASH_WRITE, target_id, TIMEOUT_LONG) != ESP_OK) {
            return ESP_FAIL;
        }
        offset += (uint32_t)rd;
        fw_state_set_progress((uint8_t)((offset * 100U) / size));
    }

    fw_state_set_step(FW_STEP_DONE, "CAN finish");
    if (tms_send_cmd(SID_TARGET_DOWNLOAD_FINISH, 0x01, 0, 0, 0, 0) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t can_fw_update_from_reader(const fw_meta_t *meta, fw_storage_reader_t *reader) {
    if (!meta || !reader) {
        return ESP_ERR_INVALID_ARG;
    }

    char tmp_path[384];
    snprintf(tmp_path, sizeof(tmp_path), "%s/%s.%s.can.bin",
             BASE_FILESYSTEM_PATH_SDMMC, meta->file_name, meta->file_type);

    FILE *tmp = fopen(tmp_path, "wb");
    if (!tmp) {
        fw_state_set_step(FW_STEP_ERROR, "CAN temp open fail");
        return ESP_FAIL;
    }
    bin_sink_ctx_t ctx = {.file = tmp, .offset = 0, .max = 0};
    esp_err_t ret = fw_packaging_process((fw_meta_t *)meta, reader, bin_sink, &ctx);
    fclose(tmp);
    if (ret != ESP_OK) {
        fw_state_set_step(FW_STEP_ERROR, "CAN packaging fail");
        return ret;
    }

    FILE *f = fopen(tmp_path, "rb");
    if (!f) {
        fw_state_set_step(FW_STEP_ERROR, "CAN file open fail");
        return ESP_FAIL;
    }
    uint32_t target_id = CAN_ID_TMS_BOARD;
    uint32_t parsed_id = parse_hex_or_dec(meta->target_id);
    if (parsed_id != 0) {
        target_id = parsed_id;
    }
    uint32_t bitrate_fast = meta->can_bitrate ? meta->can_bitrate : 1000000;
    apply_meta_json_overrides(meta, &target_id, &bitrate_fast);
    ESP_LOGI(TAG, "CAN target_id=0x%08lx bitrate=%lu",
             (unsigned long)target_id, (unsigned long)bitrate_fast);

    ret = can_send_file(f, ctx.max, target_id, bitrate_fast);
    fclose(f);
    return ret;
}
