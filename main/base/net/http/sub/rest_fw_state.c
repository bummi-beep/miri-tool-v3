#include "rest_fw_state.h"

#include <stdio.h>
#include <string.h>

#include "core/firmware_upload/fw_upload_state.h"
#include "core/firmware_upload/fw_upload_types.h"

static const char *fw_step_to_str(fw_step_t step) {
    switch (step) {
        case FW_STEP_PREPARE: return "PREPARE";
        case FW_STEP_WAIT_USER: return "WAIT_USER";
        case FW_STEP_TRANSFER: return "TRANSFER";
        case FW_STEP_DECODE: return "DECODE";
        case FW_STEP_WRITE: return "WRITE";
        case FW_STEP_VERIFY: return "VERIFY";
        case FW_STEP_DONE: return "DONE";
        case FW_STEP_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

esp_err_t fw_state_get_handler(httpd_req_t *req) {
    fw_step_t step = fw_state_get_step();
    uint8_t progress = fw_state_get_progress();
    const char *msg = fw_state_get_message();

    char json[192];
    snprintf(json, sizeof(json),
             "{\"step\":%d,\"step_name\":\"%s\",\"progress\":%u,\"message\":\"%s\"}",
             (int)step, fw_step_to_str(step), (unsigned)progress,
             msg ? msg : "");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}
