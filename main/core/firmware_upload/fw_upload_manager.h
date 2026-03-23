#ifndef FW_UPLOAD_MANAGER_H
#define FW_UPLOAD_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

#include "core/firmware_upload/fw_upload_types.h"
#include "core/firmware_upload/fw_upload_storage.h"

typedef struct {
    fw_meta_t meta;
    size_t received;
    fw_storage_writer_t writer;
} fw_upload_session_t;

esp_err_t fw_upload_prepare_meta(fw_meta_t *meta, const char *name, const char *type, fw_format_t format);
esp_err_t fw_upload_save_buffer(const fw_meta_t *meta, const uint8_t *data, size_t len);

esp_err_t fw_upload_session_begin(fw_upload_session_t *session, const fw_meta_t *meta, size_t total_size);
esp_err_t fw_upload_session_write(fw_upload_session_t *session, const uint8_t *data, size_t len);
esp_err_t fw_upload_session_finish(fw_upload_session_t *session);

esp_err_t fw_upload_run_from_sdmmc(const char *name, const char *type, bool *out_started);

#endif
