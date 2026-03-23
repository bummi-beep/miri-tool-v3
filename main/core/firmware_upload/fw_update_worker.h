#ifndef FW_UPDATE_WORKER_H
#define FW_UPDATE_WORKER_H

#include <esp_err.h>

#include "core/firmware_upload/fw_upload_types.h"

esp_err_t fw_update_worker_start(void);
esp_err_t fw_update_enqueue_job(const fw_meta_t *meta, const char *name, const char *type);

#endif
