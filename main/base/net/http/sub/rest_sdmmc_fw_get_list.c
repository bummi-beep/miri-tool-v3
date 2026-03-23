#include "rest_sdmmc_fw_get_list.h"

#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#include <cJSON.h>
#include <esp_log.h>
#include <string.h>

#include "base/sdmmc/sdmmc_init.h"
#include "config/storage_paths.h"

static const char *TAG = "http_sdmmc_list";

static const char *k_sdmmc_program_types[] = {
    "S00", "S01", "A00", "A01", "A02", "A03", "A04", "A05", "A06",
    "A07", "A08", "A09", "A10", "A11", "A12", "A13", "A14", "A15",
};

static void add_timestamp_json(cJSON *root) {
    cJSON_AddNumberToObject(root, "timestamp", esp_log_timestamp());
}

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

static const char *get_filename_ext(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) {
        return "";
    }
    return dot + 1;
}

static void get_filename_without_extension(const char *name, char *out, size_t out_size) {
    const char *dot = strrchr(name, '.');
    size_t len = dot ? (size_t)(dot - name) : strlen(name);
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, name, len);
    out[len] = '\0';
}

static bool sdmmc_is_valid_program_file(const char *name) {
    if (!name) {
        return false;
    }
    const char *ext = get_filename_ext(name);
    if (!sdmmc_is_valid_type(ext)) {
        return false;
    }
    return strlen(name) == 40;
}

esp_err_t sdmmc_fw_get_list_get_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;
    cJSON *root = cJSON_CreateObject();
    add_timestamp_json(root);
    httpd_resp_set_type(req, "application/json");

    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        ESP_LOGE(TAG, "sdmmc init fail");
        ret = ESP_ERR_NOT_SUPPORTED;
        cJSON_AddNumberToObject(root, "result", ret);
        const char *jsonString = cJSON_PrintUnformatted(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, jsonString);
        free((void *)jsonString);
        cJSON_Delete(root);
        return ret;
    }

    cJSON *fileList = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "fileList", fileList);

    DIR *dir = opendir(BASE_FILESYSTEM_PATH_SDMMC "/");
    if (!dir) {
        ret = ESP_FAIL;
        cJSON_AddNumberToObject(root, "result", ret);
        const char *jsonString = cJSON_PrintUnformatted(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, jsonString);
        free((void *)jsonString);
        cJSON_Delete(root);
        return ret;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!sdmmc_is_valid_program_file(ent->d_name)) {
            continue;
        }
        struct stat st;
        char filename[384];
        snprintf(filename, sizeof(filename), "%s/%s", BASE_FILESYSTEM_PATH_SDMMC, ent->d_name);
        if (stat(filename, &st) != 0) {
            continue;
        }

        cJSON *file = cJSON_CreateObject();
        cJSON_AddItemToArray(fileList, file);
        char name_buf[128];
        get_filename_without_extension(ent->d_name, name_buf, sizeof(name_buf));
        cJSON_AddStringToObject(file, "file_name", name_buf);
        cJSON_AddStringToObject(file, "file_type", get_filename_ext(ent->d_name));

        time_t t = st.st_mtime;
        struct tm lt;
        localtime_r(&t, &lt);
        char date_raw[21] = {0};
        strftime(date_raw, sizeof(date_raw), "%Y-%m-%d %H:%M:%S", &lt);
        cJSON_AddStringToObject(file, "file_date", date_raw);
    }
    closedir(dir);

    cJSON_AddNumberToObject(root, "result", ret);
    const char *jsonString = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, jsonString);
    free((void *)jsonString);
    cJSON_Delete(root);
    return ret;
}
