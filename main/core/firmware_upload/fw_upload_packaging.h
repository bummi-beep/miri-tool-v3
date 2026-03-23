#ifndef FW_UPLOAD_PACKAGING_H
#define FW_UPLOAD_PACKAGING_H

#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

#include "core/firmware_upload/fw_upload_types.h"
#include "core/firmware_upload/fw_upload_storage.h"

typedef esp_err_t (*fw_packaging_sink_t)(uint32_t addr, const uint8_t *data, size_t len, void *ctx);

esp_err_t fw_packaging_process(const fw_meta_t *meta, fw_storage_reader_t *reader,
                               fw_packaging_sink_t sink, void *ctx);

#endif
