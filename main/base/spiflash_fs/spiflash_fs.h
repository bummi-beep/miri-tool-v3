#ifndef SPIFLASH_FS_H
#define SPIFLASH_FS_H

#include <stdbool.h>
#include <esp_err.h>

esp_err_t spiflash_fs_init(void);
bool spiflash_fs_is_ready(void);
const char *spiflash_fs_get_mount(void);
const char *spiflash_fs_get_ble_device_name(void);
esp_err_t spiflash_fs_set_ble_device_name(const char *name);
bool spiflash_fs_apply_debug_mode(void);

#endif

