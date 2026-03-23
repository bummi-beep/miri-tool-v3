#ifndef PROBE_LINK_UPDATE_H
#define PROBE_LINK_UPDATE_H

/*
 * Probe link update entry
 * - Integrates GDB remote flashing flow into fw_update_worker
 * - Reads firmware from SDMMC and streams blocks to the probe link
 */

#include <esp_err.h>

#include "core/firmware_upload/fw_upload_storage.h"
#include "core/firmware_upload/fw_upload_types.h"

esp_err_t probe_link_update_from_reader(const fw_meta_t *meta, fw_storage_reader_t *reader);

#endif
