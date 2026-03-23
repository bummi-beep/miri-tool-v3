#ifndef CLI_OTA_VIEW_H
#define CLI_OTA_VIEW_H

#include <stdbool.h>
#include <stdint.h>

/*
 * CLI OTA view
 * - Renders a simple status panel for ESP32 OTA mode.
 */
void cli_ota_view_set_active(bool active);
bool cli_ota_view_is_active(void);
void cli_ota_view_set_status(const char *status);
void cli_ota_view_set_last_event(uint16_t cmd, const char *payload);

#endif
