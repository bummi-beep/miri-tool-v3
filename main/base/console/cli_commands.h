#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include <stddef.h>

typedef void (*cli_cmd_fn)(const char *args);

typedef struct {
    const char *name;
    const char *help;
    cli_cmd_fn fn;
} cli_command_t;

/*
 * cli_get_commands()
 * - Returns the command table and its size.
 * - Add new commands in cli_commands.c only.
 */
const cli_command_t *cli_get_commands(size_t *count);

#endif
