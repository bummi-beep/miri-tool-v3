#ifndef ROLE_DISPATCHER_H
#define ROLE_DISPATCHER_H

#include <stdbool.h>
#include "board/board_profile.h"

typedef enum {
    ROLE_SOURCE_NONE = 0,
    ROLE_SOURCE_BLE,
    ROLE_SOURCE_CLI,
} role_source_t;

/*
 * role_dispatcher_init()
 * - Initializes internal state for role arbitration.
 * - Keep it lightweight; it should run after base init.
 */
void role_dispatcher_init(void);

/*
 * role_dispatcher_request()
 * - Common entry point for both BLE and CLI commands.
 * - Decides whether to switch roles, then starts tasks.
 */
void role_dispatcher_request(role_t role, role_source_t source);

/*
 * role_dispatcher_start_default()
 * - Deprecated: roles should start after target selection.
 */
void role_dispatcher_start_default(role_t role);

/*
 * role_dispatcher_get_active()
 * - Returns the currently active role.
 */
role_t role_dispatcher_get_active(void);

/*
 * role_dispatcher_get_source()
 * - Returns the source that triggered the active role.
 */
role_source_t role_dispatcher_get_source(void);

/*
 * role_dispatcher_clear()
 * - Clears the active role/source state.
 * - Does not stop tasks; caller should stop role logic first if needed.
 */
void role_dispatcher_clear(void);

#endif
