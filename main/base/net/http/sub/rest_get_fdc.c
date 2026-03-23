#include "rest_get_fdc.h"

#include <cJSON.h>
#include <esp_log.h>

static const char *TAG = "http_fdc";

esp_err_t get_fdc_get_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "result", -1);
    cJSON_AddStringToObject(root, "reason", "not_supported");
    const char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    ESP_LOGI(TAG, "get_fdc: not supported");
    return ESP_OK;
}
