#include "rest_fat_get_list.h"

#include <dirent.h>
#include <sys/stat.h>

#include <cJSON.h>
#include <esp_log.h>
#include <string.h>

#include "base/spiflash_fs/spiflash_fs.h"
#include "config/storage_paths.h"

static const char *TAG = "http_fat_list";

static void add_timestamp_json(cJSON *root) {
    cJSON_AddNumberToObject(root, "timestamp", esp_log_timestamp());
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

esp_err_t fat_get_list_get_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;
    cJSON *root = cJSON_CreateObject();
    add_timestamp_json(root);
    httpd_resp_set_type(req, "application/json");

    if (spiflash_fs_init() != ESP_OK || !spiflash_fs_is_ready()) {
        ESP_LOGE(TAG, "fatfs init fail");
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

    DIR *dir = opendir(BASE_FILESYSTEM_PATH_SPIFLASH "/");
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
        struct stat st;
        char filename[384];
        snprintf(filename, sizeof(filename), "%s/%s", BASE_FILESYSTEM_PATH_SPIFLASH, ent->d_name);
        if (stat(filename, &st) != 0 || S_ISDIR(st.st_mode)) {
            continue;
        }
        cJSON *file = cJSON_CreateObject();
        cJSON_AddItemToArray(fileList, file);
        char name_buf[128];
        get_filename_without_extension(ent->d_name, name_buf, sizeof(name_buf));
        cJSON_AddStringToObject(file, "file_name", name_buf);
        cJSON_AddStringToObject(file, "file_type", get_filename_ext(ent->d_name));
        cJSON_AddNumberToObject(file, "file_size", st.st_size);
    }
    closedir(dir);

    cJSON_AddNumberToObject(root, "result", ret);
    const char *jsonString = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, jsonString);
    free((void *)jsonString);
    cJSON_Delete(root);
    return ret;
}


