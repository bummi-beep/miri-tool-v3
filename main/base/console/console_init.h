#ifndef CONSOLE_INIT_H
#define CONSOLE_INIT_H

/*
 * console_init()
 * - Initializes UART0 for CLI input/output.
 * - This is part of the boot baseline so CLI can be used
 *   before any role starts.
 */
void console_init(void);

#endif
