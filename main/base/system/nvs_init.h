#ifndef NVS_INIT_H
#define NVS_INIT_H

/*
 * nvs_init()
 * - Initializes NVS (key-value storage in flash).
 * - Mirrors the fw_esp32 behavior:
 *   if there are no free pages or a new version is found,
 *   erase once and re-initialize.
 */
void nvs_init(void);

#endif
