#include "rest_sdmmc_fw_meta.h"

#include <cJSON.h>
#include <esp_err.h>
#include <esp_log.h>
#include <string.h>

#include "base/sdmmc/sdmmc_init.h"
#include "core/firmware_upload/fw_upload_manager.h"
#include "core/firmware_upload/fw_upload_storage.h"
#include "core/firmware_upload/fw_upload_types.h"
#include "base/net/tcp/tcp_server.h"

static const char *TAG = "http_fw_meta";

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

esp_err_t sdmmc_fw_meta_post_handler(httpd_req_t *req) {
    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "sdmmc init fail");
        return ESP_FAIL;
    }

    cJSON *root = NULL;
    esp_err_t ret = read_json_body(req, &root);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }

    cJSON *json_file_name = cJSON_GetObjectItem(root, "file_name");
    cJSON *json_file_type = cJSON_GetObjectItem(root, "file_type");
    cJSON *json_file_format = cJSON_GetObjectItem(root, "file_format");
    cJSON *json_exec = cJSON_GetObjectItem(root, "exec");
    cJSON *json_target_board = cJSON_GetObjectItem(root, "target_board");
    cJSON *json_target_id = cJSON_GetObjectItem(root, "target_id");
    cJSON *json_can_bitrate = cJSON_GetObjectItem(root, "can_bitrate");
    cJSON *json_uart_baud = cJSON_GetObjectItem(root, "uart_baud");
    cJSON *json_swd_clock = cJSON_GetObjectItem(root, "swd_clock_khz");
    cJSON *json_jtag_clock = cJSON_GetObjectItem(root, "jtag_clock_khz");
    cJSON *json_meta = cJSON_GetObjectItem(root, "meta_json");
    cJSON *json_file_size = cJSON_GetObjectItem(root, "file_size");

    if (!cJSON_IsString(json_file_name) || !cJSON_IsString(json_file_type)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid params");
        return ESP_FAIL;
    }

    fw_format_t fmt = FW_FMT_UNKNOWN;
    if (cJSON_IsString(json_file_format)) {
        fmt = fw_format_from_str(json_file_format->valuestring);
    }
    fw_meta_t meta;
    if (fw_upload_prepare_meta(&meta, json_file_name->valuestring, json_file_type->valuestring, fmt) != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "meta init fail");
        return ESP_FAIL;
    }
    if (cJSON_IsString(json_exec)) {
        meta.exec = fw_exec_from_str(json_exec->valuestring);
        meta.exec_override = (meta.exec != FW_EXEC_UNKNOWN);
    }
    if (cJSON_IsString(json_target_board)) {
        snprintf(meta.target_board, sizeof(meta.target_board), "%s", json_target_board->valuestring);
    }
    if (cJSON_IsString(json_target_id)) {
        snprintf(meta.target_id, sizeof(meta.target_id), "%s", json_target_id->valuestring);
    }
    if (cJSON_IsNumber(json_can_bitrate)) {
        meta.can_bitrate = (uint32_t)json_can_bitrate->valuedouble;
    }
    if (cJSON_IsNumber(json_uart_baud)) {
        meta.uart_baud = (uint32_t)json_uart_baud->valuedouble;
    }
    if (cJSON_IsNumber(json_swd_clock)) {
        meta.swd_clock_khz = (uint32_t)json_swd_clock->valuedouble;
    }
    if (cJSON_IsNumber(json_jtag_clock)) {
        meta.jtag_clock_khz = (uint32_t)json_jtag_clock->valuedouble;
    }
    if (cJSON_IsString(json_meta)) {
        snprintf(meta.meta_json, sizeof(meta.meta_json), "%s", json_meta->valuestring);
    }
    if (cJSON_IsNumber(json_file_size) && json_file_size->valuedouble > 0) {
        meta.original_size = (size_t)json_file_size->valuedouble;
    }

    cJSON_Delete(root);
    ret = fw_storage_meta_write(&meta);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "meta write failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "meta write fail");
        return ESP_FAIL;
    }
    tcp_server_set_pending_file(meta.file_name, meta.file_type);
    if (meta.original_size > 0) {
        tcp_server_set_expected_size(meta.original_size);
    }
    httpd_resp_sendstr(req, "meta stored");
    return ESP_OK;
}
