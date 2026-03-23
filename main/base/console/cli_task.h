#ifndef CLI_TASK_H
#define CLI_TASK_H

#include <stdint.h>

typedef enum {
    CLI_MODE_NORMAL = 0,
    CLI_MODE_HHT,
    CLI_MODE_ESP32,
    CLI_MODE_FW,
} cli_mode_t;

/*
 * cli_task_start()
 * - Starts a FreeRTOS task that reads console lines
 *   and feeds them to the CLI core.
 */
void cli_task_start(void);

/*
 * cli_task_set_mode()
 * - Switches CLI between normal command mode and HHT key mode.
 */
void cli_task_set_mode(cli_mode_t mode);

cli_mode_t cli_task_get_mode(void);

void cli_task_set_hht_stop_cmd(uint16_t cmd);

#endif
