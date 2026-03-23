#include "fw_upload_state.h"
#include <esp_log.h>
#include <stdio.h>

static const char *TAG = "fw_state";
static fw_step_t s_step = FW_STEP_PREPARE;
static uint8_t s_progress = 0;
static char s_message[96] = {0};

void fw_state_set_step(fw_step_t step, const char *msg) {
    s_step = step;
    if (msg && msg[0] != '\0') {
        snprintf(s_message, sizeof(s_message), "%s", msg);
        ESP_LOGI(TAG, "step=%d %s", (int)step, msg);
    } else {
        s_message[0] = '\0';
        ESP_LOGI(TAG, "step=%d", (int)step);
    }
}

void fw_state_set_progress(uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }
    s_progress = percent;
    ESP_LOGI(TAG, "progress=%u", (unsigned)percent);
}

fw_step_t fw_state_get_step(void) {
    return s_step;
}

uint8_t fw_state_get_progress(void) {
    return s_progress;
}

const char *fw_state_get_message(void) {
    return s_message;
}
