#include "runtime_config.h"

#include <stdio.h>
#include <string.h>

#include <esp_log.h>
#include <cJSON.h>

#include "base/spiflash_fs/spiflash_fs.h"

#define CONFIG_TAG "runtime_config"
#define CONFIG_VERSION 1
#define CONFIG_FILE_NAME "config.json"

static runtime_config_t s_config;

static void runtime_config_set_defaults(runtime_config_t *cfg) {
    if (!cfg) {
        return;
    }
    memset(cfg, 0, sizeof(*cfg));
    cfg->version = CONFIG_VERSION;
    cfg->debug_mode = false;
    cfg->device_alias[0] = '\0';
}

static void runtime_config_apply_json(cJSON *root, runtime_config_t *cfg) {
    if (!root || !cfg) {
        return;
    }
    cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (cJSON_IsNumber(version) && version->valuedouble > 0) {
        cfg->version = (uint32_t)version->valuedouble;
    }
    cJSON *debug_mode = cJSON_GetObjectItemCaseSensitive(root, "debug_mode");
    if (cJSON_IsBool(debug_mode)) {
        cfg->debug_mode = cJSON_IsTrue(debug_mode);
    }
    cJSON *alias = cJSON_GetObjectItemCaseSensitive(root, "device_alias");
    if (cJSON_IsString(alias) && alias->valuestring) {
        snprintf(cfg->device_alias, sizeof(cfg->device_alias), "%s", alias->valuestring);
    }
}

static cJSON *runtime_config_build_json(const runtime_config_t *cfg) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON_AddNumberToObject(root, "version", (double)cfg->version);
    cJSON_AddBoolToObject(root, "debug_mode", cfg->debug_mode);
    if (cfg->device_alias[0] != '\0') {
        cJSON_AddStringToObject(root, "device_alias", cfg->device_alias);
    }
    return root;
}

static esp_err_t runtime_config_write_file(const char *path, const runtime_config_t *cfg) {
    cJSON *root = runtime_config_build_json(cfg);
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }

    FILE *f = fopen(path, "wt");
    if (!f) {
        cJSON_free(json);
        return ESP_FAIL;
    }
    fprintf(f, "%s", json);
    fclose(f);
    cJSON_free(json);
    return ESP_OK;
}

esp_err_t runtime_config_save(void) {
    char pathbuf[128];
    snprintf(pathbuf, sizeof(pathbuf), "%s/%s", spiflash_fs_get_mount(), CONFIG_FILE_NAME);
    return runtime_config_write_file(pathbuf, &s_config);
}

esp_err_t runtime_config_init(void) {
    runtime_config_set_defaults(&s_config);

    char pathbuf[128];
    snprintf(pathbuf, sizeof(pathbuf), "%s/%s", spiflash_fs_get_mount(), CONFIG_FILE_NAME);

    FILE *f = fopen(pathbuf, "rb");
    if (!f) {
        ESP_LOGI(CONFIG_TAG, "config.json not found, creating defaults");
        return runtime_config_save();
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return ESP_FAIL;
    }
    long size = ftell(f);
    if (size <= 0) {
        fclose(f);
        ESP_LOGW(CONFIG_TAG, "config.json empty, resetting");
        return runtime_config_save();
    }
    rewind(f);

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[read] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGW(CONFIG_TAG, "config.json parse failed, resetting");
        return runtime_config_save();
    }

    runtime_config_apply_json(root, &s_config);
    cJSON_Delete(root);
    return ESP_OK;
}

const runtime_config_t *runtime_config_get(void) {
    return &s_config;
}

bool runtime_config_is_debug_mode(void) {
    return s_config.debug_mode;
}

const char *runtime_config_get_device_name(void) {
    if (s_config.device_alias[0] != '\0') {
        return s_config.device_alias;
    }
    return spiflash_fs_get_ble_device_name();
}

esp_err_t runtime_config_set_device_alias(const char *alias) {
    if (!alias) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(alias) >= sizeof(s_config.device_alias)) {
        return ESP_ERR_INVALID_ARG;
    }
    snprintf(s_config.device_alias, sizeof(s_config.device_alias), "%s", alias);
    return ESP_OK;
}

