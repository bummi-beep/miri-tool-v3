#include "rest_common_format.h"

#include <cJSON.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>

#include "base/sdmmc/sdmmc_init.h"
#include "base/spiflash_fs/spiflash_fs.h"
#include "config/storage_paths.h"

static const char *TAG = "http_format";

static void add_timestamp_json(cJSON *root) {
    cJSON_AddNumberToObject(root, "timestamp", esp_log_timestamp());
}

esp_err_t format_sdmmc_get_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;
    cJSON *root = cJSON_CreateObject();
    add_timestamp_json(root);
    httpd_resp_set_type(req, "application/json");

    if (sdmmc_init() == ESP_OK && sdmmc_is_ready()) {
        ret = esp_vfs_fat_sdcard_format(sdmmc_get_mount(), sdmmc_get_card());
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to format SDMMC (%s)", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "sdmmc init fail");
        ret = ESP_ERR_NOT_SUPPORTED;
    }

    cJSON_AddNumberToObject(root, "result", ret);
    cJSON_AddStringToObject(root, "resultString", esp_err_to_name(ret));
    const char *jsonString = cJSON_PrintUnformatted(root);
    if (ret == ESP_OK) {
        httpd_resp_sendstr(req, jsonString);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, jsonString);
    }
    free((void *)jsonString);
    cJSON_Delete(root);
    return ret;
}

esp_err_t format_fat_get_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;
    cJSON *root = cJSON_CreateObject();
    add_timestamp_json(root);
    httpd_resp_set_type(req, "application/json");

    if (spiflash_fs_init() == ESP_OK && spiflash_fs_is_ready()) {
        ret = esp_vfs_fat_spiflash_format_rw_wl(spiflash_fs_get_mount(), STORAGE_LABEL_SPIFLASH);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to format FATFS (%s)", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "fatfs init fail");
        ret = ESP_ERR_NOT_SUPPORTED;
    }

    cJSON_AddNumberToObject(root, "result", ret);
    cJSON_AddStringToObject(root, "resultString", esp_err_to_name(ret));
    const char *jsonString = cJSON_PrintUnformatted(root);
    if (ret == ESP_OK) {
        httpd_resp_sendstr(req, jsonString);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, jsonString);
    }
    free((void *)jsonString);
    cJSON_Delete(root);
    return ret;
}


