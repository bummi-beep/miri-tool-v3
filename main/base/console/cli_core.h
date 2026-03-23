#ifndef CLI_CORE_H
#define CLI_CORE_H

/*
 * cli_execute()
 * - Entry point for a single CLI line (UART/USB/etc.).
 * - Parses the command, finds the handler, and runs it.
 * - Keep this generic so any transport can feed it.
 */
void cli_execute(const char *line);

#endif
