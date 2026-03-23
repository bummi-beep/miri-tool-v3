#ifndef SDMMC_INIT_H
#define SDMMC_INIT_H

#include <stdbool.h>
#include <esp_err.h>
#include <driver/sdmmc_types.h>

esp_err_t sdmmc_init(void);
bool sdmmc_is_ready(void);
sdmmc_card_t *sdmmc_get_card(void);
const char *sdmmc_get_mount(void);

#endif

