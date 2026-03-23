#ifndef CAN_FW_UPDATE_H
#define CAN_FW_UPDATE_H

#include <esp_err.h>

#include "core/firmware_upload/fw_upload_types.h"
#include "core/firmware_upload/fw_upload_storage.h"

esp_err_t can_fw_update_from_reader(const fw_meta_t *meta, fw_storage_reader_t *reader);

#endif
