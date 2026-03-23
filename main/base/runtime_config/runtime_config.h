#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

typedef struct {
    uint32_t version;
    bool debug_mode;
    char device_alias[32];
} runtime_config_t;

esp_err_t runtime_config_init(void);
const runtime_config_t *runtime_config_get(void);
bool runtime_config_is_debug_mode(void);
const char *runtime_config_get_device_name(void);
esp_err_t runtime_config_set_device_alias(const char *alias);
esp_err_t runtime_config_save(void);

#endif

