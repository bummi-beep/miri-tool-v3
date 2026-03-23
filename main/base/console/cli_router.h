#ifndef CLI_ROUTER_H
#define CLI_ROUTER_H

/*
 * cli_router_on_command()
 * - Parses a CLI command string and maps it to a role.
 * - This is a thin adapter so CLI can control roles without duplicating logic.
 */
void cli_router_on_command(const char *cmd);

#endif
