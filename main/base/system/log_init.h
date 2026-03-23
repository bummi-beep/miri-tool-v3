#ifndef LOG_INIT_H
#define LOG_INIT_H

/*
 * log_init()
 * - Central place to configure log output and default levels.
 * - Keep it minimal so it is safe to call very early at boot.
 */
void log_init(void);

#endif
