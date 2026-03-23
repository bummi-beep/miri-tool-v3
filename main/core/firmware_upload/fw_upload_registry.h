#ifndef FW_UPLOAD_REGISTRY_H
#define FW_UPLOAD_REGISTRY_H

#include <stdbool.h>
#include "core/firmware_upload/fw_upload_types.h"

typedef struct {
    char file_type[4];
    fw_format_t format;
    fw_exec_t exec;
    fw_format_t exec_format;
} fw_route_t;

bool fw_registry_lookup(const char *file_type, fw_route_t *out);

#endif
