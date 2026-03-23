#include "task_manager.h"
#include "sdkconfig.h"
#include <esp_log.h>
#include "core/hht/hht_main.h"
#include "core/firmware_upload/fw_upload.h"
#include "core/storage_upload/storage_upload.h"
#include "core/diagnosis/diagnosis_main.h"
#include "core/manual/manual_control.h"
#include "base/console/cli_task.h"

void tasks_start_common(void) {
    /*
     * Common tasks that should run regardless of role.
     * CLI is kept always-on for system commands (reboot, version, etc.).
     */
#if CONFIG_COMPILER_OPTIMIZATION_LEVEL_DEBUG
    cli_task_start();
#else
    ESP_LOGI("tasks", "CLI disabled in release build");
#endif
    ESP_LOGI("tasks", "tasks_start_common");
}

void tasks_start_for_role(role_t role) {
    switch (role) {
        case ROLE_HHT:
            hht_start();
            break;
        case ROLE_FW_UPLOAD:
            fw_upload_start();
            break;
        case ROLE_STORAGE_UPLOAD:
            storage_upload_start();
            break;
        case ROLE_DIAG:
            diagnosis_start();
            break;
        case ROLE_MANUAL:
            manual_control_start();
            break;
        default:
            ESP_LOGI("tasks", "no role selected");
            break;
    }
}

