#include "fw_upload.h"
#include <esp_log.h>
#include "core/firmware_upload/fw_upload_state.h"
#include "core/firmware_upload/fw_update_worker.h"

void fw_upload_start(void) {
    fw_state_set_step(FW_STEP_PREPARE, "firmware update role active");
    fw_update_worker_start();
    ESP_LOGI("fw_update", "fw_update_start");
}

