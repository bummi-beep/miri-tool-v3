#ifndef FW_UPLOAD_META_DEFAULTS_H
#define FW_UPLOAD_META_DEFAULTS_H

#include <esp_err.h>

#include "core/firmware_upload/fw_upload_types.h"

/* file_type("A11", "S00" 등)에 따른 fw_meta_t 기본값을 제공 */
esp_err_t fw_meta_init_defaults(fw_meta_t *meta);

#endif

