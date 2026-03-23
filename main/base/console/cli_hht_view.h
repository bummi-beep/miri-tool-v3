#ifndef CLI_HHT_VIEW_H
#define CLI_HHT_VIEW_H

#include <stdbool.h>
#include <stdint.h>

/*
 * CLI HHT view
 * - Renders a 4x20 HHT display on the console.
 * - Enabled only when CLI enters HHT mode.
 */
void cli_hht_view_set_active(bool active);
bool cli_hht_view_is_active(void);
bool cli_hht_view_is_visible(void);
void cli_hht_view_update_line(uint8_t row, const char *line20);
void cli_hht_view_set_last_key(const char *key_name);
void cli_hht_view_set_boxed(bool boxed);
bool cli_hht_view_is_boxed(void);
void cli_hht_view_push_log(const char *line);
void cli_hht_view_refresh(void);
void cli_hht_view_set_ble_connected(bool connected);

#endif
