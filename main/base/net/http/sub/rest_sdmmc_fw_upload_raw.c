#include "rest_sdmmc_fw_upload_raw.h"

#include <ctype.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <stdlib.h>

#include "base/sdmmc/sdmmc_init.h"
#include "core/firmware_upload/fw_upload_manager.h"
#include "core/firmware_upload/fw_upload_storage.h"
#include "core/firmware_upload/fw_upload_types.h"

static const char *TAG = "http_fw_upload_raw";

static bool is_valid_file_type(const char *type) {
    if (!type || strlen(type) != 3) {
        return false;
    }
    if ((type[0] == 'S' || type[0] == 'A') &&
        isdigit((unsigned char)type[1]) &&
        isdigit((unsigned char)type[2])) {
        if (type[0] == 'S') {
            return (type[1] == '0' && (type[2] == '0' || type[2] == '1'));
        }
        return true;
    }
    return false;
}

static bool header_get_str(httpd_req_t *req, const char *key, char *out, size_t out_size) {
    if (httpd_req_get_hdr_value_len(req, key) <= 0) {
        return false;
    }
    if (httpd_req_get_hdr_value_str(req, key, out, out_size) != ESP_OK) {
        return false;
    }
    return true;
}

static uint32_t parse_u32_or_zero(const char *s) {
    if (!s || s[0] == '\0') {
        return 0;
    }
    char *end = NULL;
    unsigned long val = strtoul(s, &end, 0);
    if (end == s) {
        return 0;
    }
    return (uint32_t)val;
}

esp_err_t sdmmc_fw_upload_raw_post_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "sdmmc_fw_upload_raw_post_handler (len=%d) heap=%u psram=%u",
             (int)req->content_len,
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "sdmmc init fail");
        return ESP_FAIL;
    }

    char file_name[64] = {0};
    char file_type[8] = {0};
    char file_format[16] = {0};
    char exec_str[24] = {0};
    char target_board[32] = {0};
    char target_id[16] = {0};
    char can_bitrate[16] = {0};
    char uart_baud[16] = {0};
    char swd_clock[16] = {0};
    char jtag_clock[16] = {0};
    if (!header_get_str(req, "X-File-Name", file_name, sizeof(file_name)) ||
        !header_get_str(req, "X-File-Type", file_type, sizeof(file_type))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing headers");
        return ESP_FAIL;
    }
    header_get_str(req, "X-File-Format", file_format, sizeof(file_format));
    header_get_str(req, "X-Exec", exec_str, sizeof(exec_str));
    header_get_str(req, "X-Target-Board", target_board, sizeof(target_board));
    header_get_str(req, "X-Target-Id", target_id, sizeof(target_id));
    header_get_str(req, "X-Can-Bitrate", can_bitrate, sizeof(can_bitrate));
    header_get_str(req, "X-Uart-Baud", uart_baud, sizeof(uart_baud));
    header_get_str(req, "X-Swd-Clock", swd_clock, sizeof(swd_clock));
    header_get_str(req, "X-Jtag-Clock", jtag_clock, sizeof(jtag_clock));

    if (strlen(file_name) != 36 || !is_valid_file_type(file_type)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid name/type");
        return ESP_FAIL;
    }

    fw_format_t fmt = fw_format_from_str(file_format);
    fw_meta_t meta;
    if (fw_upload_prepare_meta(&meta, file_name, file_type, fmt) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "meta init fail");
        return ESP_FAIL;
    }
    fw_meta_t existing;
    if (fw_storage_meta_read(file_name, file_type, &existing) == ESP_OK) {
        meta = existing;
        snprintf(meta.file_name, sizeof(meta.file_name), "%s", file_name);
        snprintf(meta.file_type, sizeof(meta.file_type), "%s", file_type);
        if (fmt != FW_FMT_UNKNOWN) {
            meta.format = fmt;
        }
    }
    if (exec_str[0] != '\0') {
        meta.exec = fw_exec_from_str(exec_str);
        meta.exec_override = (meta.exec != FW_EXEC_UNKNOWN);
    }
    if (target_board[0] != '\0') {
        snprintf(meta.target_board, sizeof(meta.target_board), "%s", target_board);
    }
    if (target_id[0] != '\0') {
        snprintf(meta.target_id, sizeof(meta.target_id), "%s", target_id);
    }
    meta.can_bitrate = parse_u32_or_zero(can_bitrate);
    meta.uart_baud = parse_u32_or_zero(uart_baud);
    meta.swd_clock_khz = parse_u32_or_zero(swd_clock);
    meta.jtag_clock_khz = parse_u32_or_zero(jtag_clock);

    fw_upload_session_t session;
    if (fw_upload_session_begin(&session, &meta, req->content_len) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "store init fail");
        return ESP_FAIL;
    }

    const size_t buf_len = 1024;
    uint8_t *buf = (uint8_t *)malloc(buf_len);
    if (!buf) {
        fw_upload_session_finish(&session);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "alloc fail");
        return ESP_FAIL;
    }
    size_t remaining = req->content_len;
    while (remaining > 0) {
        int to_read = (remaining > buf_len) ? (int)buf_len : (int)remaining;
        int ret = httpd_req_recv(req, (char *)buf, to_read);
        if (ret <= 0) {
            fw_upload_session_finish(&session);
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv fail");
            return ESP_FAIL;
        }
        if (fw_upload_session_write(&session, buf, (size_t)ret) != ESP_OK) {
            fw_upload_session_finish(&session);
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "store write fail");
            return ESP_FAIL;
        }
        remaining -= ret;
    }
    free(buf);

    if (fw_upload_session_finish(&session) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "store finish fail");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "upload done heap=%u psram=%u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    httpd_resp_sendstr(req, "File uploaded (raw/encrypted) successfully");
    return ESP_OK;
}
