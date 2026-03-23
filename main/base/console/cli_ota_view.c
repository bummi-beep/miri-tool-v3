#include "cli_ota_view.h"

#include <stdio.h>
#include <string.h>

#include <esp_log.h>

static const char *TAG = "cli_ota_view";

static bool s_active = false;
static char s_status[32] = "Idle";
static char s_last_cmd[16] = "-";
static char s_last_payload[32] = "-";

static void cli_ota_view_render(void) {
    if (!s_active) {
        return;
    }
    printf("\033[s");
    printf("\033[H");
    printf("+------------------------------+\r\n");
    printf("| OTA UPDATE MODE              |\r\n");
    printf("| Status : %-20s |\r\n", s_status);
    printf("| Cmd    : %-20s |\r\n", s_last_cmd);
    printf("| Data   : %-20s |\r\n", s_last_payload);
    printf("+------------------------------+\r\n");
    printf("ESP32 mode: Q=exit (back to CLI)\r\n");
    printf("\033[u");
    fflush(stdout);
}

void cli_ota_view_set_active(bool active) {
    s_active = active;
    if (s_active) {
        strncpy(s_status, "Idle", sizeof(s_status) - 1);
        s_status[sizeof(s_status) - 1] = '\0';
        strncpy(s_last_cmd, "-", sizeof(s_last_cmd) - 1);
        s_last_cmd[sizeof(s_last_cmd) - 1] = '\0';
        strncpy(s_last_payload, "-", sizeof(s_last_payload) - 1);
        s_last_payload[sizeof(s_last_payload) - 1] = '\0';
        ESP_LOGI(TAG, "OTA view enabled");
        cli_ota_view_render();
    } else {
        ESP_LOGI(TAG, "OTA view disabled");
    }
}

bool cli_ota_view_is_active(void) {
    return s_active;
}

void cli_ota_view_set_status(const char *status) {
    if (!s_active) {
        return;
    }
    if (!status || status[0] == '\0') {
        status = "-";
    }
    strncpy(s_status, status, sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';
    cli_ota_view_render();
}

void cli_ota_view_set_last_event(uint16_t cmd, const char *payload) {
    if (!s_active) {
        return;
    }
    snprintf(s_last_cmd, sizeof(s_last_cmd), "0x%04x", cmd);
    if (!payload || payload[0] == '\0') {
        strncpy(s_last_payload, "-", sizeof(s_last_payload) - 1);
        s_last_payload[sizeof(s_last_payload) - 1] = '\0';
    } else {
        strncpy(s_last_payload, payload, sizeof(s_last_payload) - 1);
        s_last_payload[sizeof(s_last_payload) - 1] = '\0';
    }
    cli_ota_view_render();
}
