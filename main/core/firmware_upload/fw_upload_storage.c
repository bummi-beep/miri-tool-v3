#include "fw_upload_storage.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <esp_log.h>
#include <cJSON.h>

#include "base/sdmmc/sdmmc_init.h"
#include "config/storage_paths.h"
#include "core/firmware_upload/fw_upload_crypto.h"

static const char *TAG = "fw_storage";

bool fw_storage_build_path(char *out, size_t out_size, const char *name, const char *type) {
    if (!out || !name || !type) {
        return false;
    }
    int rc = snprintf(out, out_size, "%s/%s.%s", BASE_FILESYSTEM_PATH_SDMMC, name, type);
    return (rc > 0 && (size_t)rc < out_size);
}

bool fw_storage_build_meta_path(char *out, size_t out_size, const char *name, const char *type) {
    if (!out || !name || !type) {
        return false;
    }
    int rc = snprintf(out, out_size, "%s/%s.%s.meta", BASE_FILESYSTEM_PATH_SDMMC, name, type);
    return (rc > 0 && (size_t)rc < out_size);
}

static esp_err_t fw_storage_open_sdmmc(void) {
    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        ESP_LOGW(TAG, "SDMMC not ready");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t fw_storage_meta_write(const fw_meta_t *meta) {
    if (!meta) {
        return ESP_ERR_INVALID_ARG;
    }
    if (fw_storage_open_sdmmc() != ESP_OK) {
        return ESP_FAIL;
    }
    char meta_path[384];
    if (!fw_storage_build_meta_path(meta_path, sizeof(meta_path), meta->file_name, meta->file_type)) {
        return ESP_ERR_INVALID_ARG;
    }
    FILE *f = fopen(meta_path, "wb");
    if (!f) {
        return ESP_FAIL;
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "file_name", meta->file_name);
    cJSON_AddStringToObject(root, "file_type", meta->file_type);
    cJSON_AddStringToObject(root, "format", fw_format_to_str(meta->format));
    cJSON_AddStringToObject(root, "exec", fw_exec_to_str(meta->exec));
    cJSON_AddBoolToObject(root, "exec_override", meta->exec_override);
    if (meta->target_board[0] != '\0') {
        cJSON_AddStringToObject(root, "target_board", meta->target_board);
    }
    if (meta->target_id[0] != '\0') {
        cJSON_AddStringToObject(root, "target_id", meta->target_id);
    }
    if (meta->can_bitrate > 0) {
        cJSON_AddNumberToObject(root, "can_bitrate", (double)meta->can_bitrate);
    }
    if (meta->uart_baud > 0) {
        cJSON_AddNumberToObject(root, "uart_baud", (double)meta->uart_baud);
    }
    if (meta->swd_clock_khz > 0) {
        cJSON_AddNumberToObject(root, "swd_clock_khz", (double)meta->swd_clock_khz);
    }
    if (meta->jtag_clock_khz > 0) {
        cJSON_AddNumberToObject(root, "jtag_clock_khz", (double)meta->jtag_clock_khz);
    }
    if (meta->meta_json[0] != '\0') {
        cJSON_AddStringToObject(root, "meta_json", meta->meta_json);
    }
    cJSON_AddBoolToObject(root, "encrypted", meta->encrypted);
    cJSON_AddNumberToObject(root, "size", (double)meta->original_size);
    char *json = cJSON_PrintUnformatted(root);
    if (json) {
        fwrite(json, 1, strlen(json), f);
        free(json);
    }
    cJSON_Delete(root);
    fclose(f);
    return ESP_OK;
}

esp_err_t fw_storage_meta_read(const char *name, const char *type, fw_meta_t *out_meta) {
    if (!name || !type || !out_meta) {
        return ESP_ERR_INVALID_ARG;
    }
    char meta_path[384];
    if (!fw_storage_build_meta_path(meta_path, sizeof(meta_path), name, type)) {
        return ESP_ERR_INVALID_ARG;
    }
    FILE *f = fopen(meta_path, "rb");
    if (!f) {
        return ESP_FAIL;
    }
    char json_buf[512];
    size_t rd = fread(json_buf, 1, sizeof(json_buf) - 1, f);
    fclose(f);
    if (rd == 0) {
        return ESP_FAIL;
    }
    json_buf[rd] = '\0';
    cJSON *root = cJSON_Parse(json_buf);
    if (!root) {
        return ESP_FAIL;
    }
    cJSON *jname = cJSON_GetObjectItem(root, "file_name");
    cJSON *jtype = cJSON_GetObjectItem(root, "file_type");
    cJSON *jfmt = cJSON_GetObjectItem(root, "format");
    cJSON *jexec = cJSON_GetObjectItem(root, "exec");
    cJSON *jexec_override = cJSON_GetObjectItem(root, "exec_override");
    cJSON *jboard = cJSON_GetObjectItem(root, "target_board");
    cJSON *jtarget_id = cJSON_GetObjectItem(root, "target_id");
    cJSON *jcan = cJSON_GetObjectItem(root, "can_bitrate");
    cJSON *juart = cJSON_GetObjectItem(root, "uart_baud");
    cJSON *jswd = cJSON_GetObjectItem(root, "swd_clock_khz");
    cJSON *jjtag = cJSON_GetObjectItem(root, "jtag_clock_khz");
    cJSON *jmeta = cJSON_GetObjectItem(root, "meta_json");
    cJSON *jenc = cJSON_GetObjectItem(root, "encrypted");
    cJSON *jsize = cJSON_GetObjectItem(root, "size");

    memset(out_meta, 0, sizeof(*out_meta));
    if (cJSON_IsString(jname)) {
        strncpy(out_meta->file_name, jname->valuestring, sizeof(out_meta->file_name) - 1);
    } else {
        strncpy(out_meta->file_name, name, sizeof(out_meta->file_name) - 1);
    }
    if (cJSON_IsString(jtype)) {
        strncpy(out_meta->file_type, jtype->valuestring, sizeof(out_meta->file_type) - 1);
    } else {
        strncpy(out_meta->file_type, type, sizeof(out_meta->file_type) - 1);
    }
    out_meta->format = cJSON_IsString(jfmt) ? fw_format_from_str(jfmt->valuestring) : FW_FMT_UNKNOWN;
    out_meta->exec = cJSON_IsString(jexec) ? fw_exec_from_str(jexec->valuestring) : FW_EXEC_UNKNOWN;
    out_meta->exec_override = cJSON_IsBool(jexec_override) ? cJSON_IsTrue(jexec_override) : false;
    if (cJSON_IsString(jboard)) {
        strncpy(out_meta->target_board, jboard->valuestring, sizeof(out_meta->target_board) - 1);
    }
    if (cJSON_IsString(jtarget_id)) {
        strncpy(out_meta->target_id, jtarget_id->valuestring, sizeof(out_meta->target_id) - 1);
    }
    out_meta->can_bitrate = cJSON_IsNumber(jcan) ? (uint32_t)jcan->valuedouble : 0;
    out_meta->uart_baud = cJSON_IsNumber(juart) ? (uint32_t)juart->valuedouble : 0;
    out_meta->swd_clock_khz = cJSON_IsNumber(jswd) ? (uint32_t)jswd->valuedouble : 0;
    out_meta->jtag_clock_khz = cJSON_IsNumber(jjtag) ? (uint32_t)jjtag->valuedouble : 0;
    if (cJSON_IsString(jmeta)) {
        strncpy(out_meta->meta_json, jmeta->valuestring, sizeof(out_meta->meta_json) - 1);
    }
    out_meta->encrypted = cJSON_IsBool(jenc) ? cJSON_IsTrue(jenc) : false;
    out_meta->original_size = cJSON_IsNumber(jsize) ? (size_t)jsize->valuedouble : 0;
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t fw_storage_writer_open(fw_storage_writer_t *writer, const fw_meta_t *meta) {
    if (!writer || !meta) {
        return ESP_ERR_INVALID_ARG;
    }
    if (fw_storage_open_sdmmc() != ESP_OK) {
        return ESP_FAIL;
    }
    memset(writer, 0, sizeof(*writer));
    writer->meta = *meta;
    char path[384];
    if (!fw_storage_build_path(path, sizeof(path), meta->file_name, meta->file_type)) {
        return ESP_ERR_INVALID_ARG;
    }
    writer->file = fopen(path, "wb");
    if (!writer->file) {
        return ESP_FAIL;
    }
#if FW_STORAGE_ENABLE_ENCRYPT
    if (fw_crypto_init_key(writer->key) != ESP_OK) {
        fclose(writer->file);
        writer->file = NULL;
        return ESP_FAIL;
    }
#endif
    writer->active = true;
    return ESP_OK;
}

esp_err_t fw_storage_writer_write(fw_storage_writer_t *writer, const uint8_t *data, size_t len) {
    if (!writer || !writer->active || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
#if !FW_STORAGE_ENABLE_ENCRYPT
    /* 암호화를 사용하지 않을 때는 그대로 써준다. */
    size_t written = fwrite(data, 1, len, writer->file);
    return (written == len) ? ESP_OK : ESP_FAIL;
#else
    size_t offset = 0;
    while (offset < len) {
        size_t to_copy = 16 - writer->buffered;
        if (to_copy > (len - offset)) {
            to_copy = len - offset;
        }
        memcpy(writer->buffer + writer->buffered, data + offset, to_copy);
        writer->buffered += to_copy;
        offset += to_copy;
        if (writer->buffered == 16) {
            uint8_t out[16];
            if (fw_crypto_encrypt_block(writer->key, writer->buffer, out, sizeof(out)) != ESP_OK) {
                return ESP_FAIL;
            }
            fwrite(out, 1, sizeof(out), writer->file);
            writer->encrypted_written += sizeof(out);
            writer->buffered = 0;
        }
    }
    return ESP_OK;
#endif
}

esp_err_t fw_storage_writer_close(fw_storage_writer_t *writer) {
    if (!writer || !writer->active) {
        return ESP_ERR_INVALID_ARG;
    }
#if FW_STORAGE_ENABLE_ENCRYPT
    if (writer->buffered > 0) {
        memset(writer->buffer + writer->buffered, 0, 16 - writer->buffered);
        uint8_t out[16];
        if (fw_crypto_encrypt_block(writer->key, writer->buffer, out, sizeof(out)) != ESP_OK) {
            return ESP_FAIL;
        }
        fwrite(out, 1, sizeof(out), writer->file);
        writer->encrypted_written += sizeof(out);
        writer->buffered = 0;
    }
#endif
    fclose(writer->file);
    writer->file = NULL;
    writer->active = false;
    return fw_storage_meta_write(&writer->meta);
}

esp_err_t fw_storage_reader_open(fw_storage_reader_t *reader, const char *name, const char *type) {
    if (!reader || !name || !type) {
        return ESP_ERR_INVALID_ARG;
    }
    if (fw_storage_open_sdmmc() != ESP_OK) {
        return ESP_FAIL;
    }
    memset(reader, 0, sizeof(*reader));
    fw_meta_t meta;
    bool has_meta = (fw_storage_meta_read(name, type, &meta) == ESP_OK);
    if (!has_meta) {
        memset(&meta, 0, sizeof(meta));
        strncpy(meta.file_name, name, sizeof(meta.file_name) - 1);
        strncpy(meta.file_type, type, sizeof(meta.file_type) - 1);
        meta.encrypted = false;
        meta.original_size = 0;
    }
    char path[384];
    if (!fw_storage_build_path(path, sizeof(path), name, type)) {
        return ESP_ERR_INVALID_ARG;
    }
    reader->file = fopen(path, "rb");
    if (!reader->file) {
        return ESP_FAIL;
    }
    if (!has_meta) {
        struct stat st;
        if (stat(path, &st) == 0) {
            meta.original_size = (size_t)st.st_size;
        }
    }
    reader->meta = meta;
#if FW_STORAGE_ENABLE_ENCRYPT
    if (fw_crypto_init_key(reader->key) != ESP_OK) {
        fclose(reader->file);
        reader->file = NULL;
        return ESP_FAIL;
    }
    reader->encrypted = meta.encrypted;
#else
    /* 암호화를 사용하지 않을 때는 항상 평문으로 취급 */
    reader->encrypted = false;
#endif
    reader->remaining = meta.original_size;
    reader->active = true;
    return ESP_OK;
}

size_t fw_storage_reader_read(fw_storage_reader_t *reader, uint8_t *out, size_t out_len) {
    if (!reader || !reader->active || !out || out_len == 0) {
        return 0;
    }
    if (!reader->encrypted) {
        return fread(out, 1, out_len, reader->file);
    }
    size_t produced = 0;
    while (produced < out_len && reader->remaining > 0) {
        if (reader->buffered == 0) {
            uint8_t enc[16];
            size_t rd = fread(enc, 1, sizeof(enc), reader->file);
            if (rd != sizeof(enc)) {
                break;
            }
            if (fw_crypto_decrypt_block(reader->key, enc, reader->buffer, sizeof(reader->buffer)) != ESP_OK) {
                break;
            }
            reader->buffered = 16;
        }
        size_t to_copy = reader->buffered;
        if (to_copy > reader->remaining) {
            to_copy = reader->remaining;
        }
        if (to_copy > (out_len - produced)) {
            to_copy = out_len - produced;
        }
        memcpy(out + produced, reader->buffer + (16 - reader->buffered), to_copy);
        reader->buffered -= to_copy;
        reader->remaining -= to_copy;
        produced += to_copy;
        if (reader->buffered == 0) {
            memset(reader->buffer, 0, sizeof(reader->buffer));
        }
    }
    return produced;
}

void fw_storage_reader_close(fw_storage_reader_t *reader) {
    if (!reader || !reader->active) {
        return;
    }
    fclose(reader->file);
    reader->file = NULL;
    reader->active = false;
}
