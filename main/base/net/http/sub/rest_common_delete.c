#include "rest_common_delete.h"

#include <cJSON.h>
#include <esp_err.h>
#include <esp_log.h>
#include <string.h>

#include "base/sdmmc/sdmmc_init.h"
#include "base/spiflash_fs/spiflash_fs.h"
#include "config/storage_paths.h"

static const char *TAG = "http_delete";

static const char *k_sdmmc_program_types[] = {
    "S00", "S01", "A00", "A01", "A02", "A03", "A04", "A05", "A06",
    "A07", "A08", "A09", "A10", "A11", "A12", "A13", "A14", "A15",
};

static bool sdmmc_is_valid_type(const char *ext) {
    if (!ext || ext[0] == '\0') {
        return false;
    }
    for (size_t i = 0; i < sizeof(k_sdmmc_program_types) / sizeof(k_sdmmc_program_types[0]); i++) {
        if (strncmp(ext, k_sdmmc_program_types[i], 3) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t read_json_body(httpd_req_t *req, cJSON **out_root) {
    char content_type[64];
    httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
    if (strstr(content_type, HTTPD_TYPE_JSON) == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t json_data_size = req->content_len;
    char *json_data = calloc(1, json_data_size + 1);
    if (!json_data) {
        return ESP_ERR_NO_MEM;
    }

    size_t recv_size = 0;
    while (recv_size < req->content_len) {
        int ret = httpd_req_recv(req, json_data + recv_size, req->content_len - recv_size);
        if (ret <= 0) {
            free(json_data);
            return ESP_FAIL;
        }
        recv_size += ret;
    }

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_root = root;
    return ESP_OK;
}

static const char *json_get_string(cJSON *root, const char *key) {
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (!item || !cJSON_IsString(item)) {
        return NULL;
    }
    return item->valuestring;
}

esp_err_t delete_sdmmc_post_handler(httpd_req_t *req) {
    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "sdmmc init fail");
        return ESP_FAIL;
    }

    cJSON *root = NULL;
    esp_err_t ret = read_json_body(req, &root);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid json");
        return ESP_FAIL;
    }

    const char *file_name = json_get_string(root, "file_name");
    const char *file_type = json_get_string(root, "file_type");
    if (!file_name || !file_type || !sdmmc_is_valid_type(file_type)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid params");
        return ESP_FAIL;
    }

    char path[384];
    snprintf(path, sizeof(path), "%s/%s.%s", BASE_FILESYSTEM_PATH_SDMMC, file_name, file_type);
    int rc = remove(path);
    cJSON_Delete(root);
    if (rc == 0) {
        httpd_resp_sendstr(req, "File Delete Success");
        return ESP_OK;
    }
    ESP_LOGE(TAG, "delete fail: %s", path);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File Delete Error");
    return ESP_FAIL;
}

esp_err_t delete_fat_post_handler(httpd_req_t *req) {
    if (spiflash_fs_init() != ESP_OK || !spiflash_fs_is_ready()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fatfs init fail");
        return ESP_FAIL;
    }

    cJSON *root = NULL;
    esp_err_t ret = read_json_body(req, &root);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid json");
        return ESP_FAIL;
    }

    const char *file_name = json_get_string(root, "file_name");
    const char *file_type = json_get_string(root, "file_type");
    if (!file_name || !file_type) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid params");
        return ESP_FAIL;
    }

    char path[384];
    snprintf(path, sizeof(path), "%s/%s.%s", BASE_FILESYSTEM_PATH_SPIFLASH, file_name, file_type);
    int rc = remove(path);
    cJSON_Delete(root);
    if (rc == 0) {
        httpd_resp_sendstr(req, "File Delete Success");
        return ESP_OK;
    }
    ESP_LOGE(TAG, "delete fail: %s", path);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File Delete Error");
    return ESP_FAIL;
}


