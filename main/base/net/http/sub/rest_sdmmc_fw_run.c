/**
 * POST /fw_run handler (SDMMC firmware run or upload+run).
 *
 * Legacy phone compatibility (fw_run_ui 없이 핸드폰 전용 사용 시):
 * 메타(meta)는 아래 3단계로 구성하여 웹(풀 JSON)과 폰(2.08 스타일 최소 JSON) 모두 호환.
 *   1차: 요청 JSON에서 추출 (file_name, file_type, [file_data], file_format, file_save_only, file_size 등)
 *   2차: BLE로 수신한 데이터로 보완 (예: 0x0055 타입, 0x0040 크기)
 *   3차: file_type(S00, A00 등)에 맞춰 빈 필드 기본값 채우기 (exec, file_format 등)
 * 이렇게 구성한 meta로 fw_upload_run_from_sdmmc() 등에 전달하면 레거시 폰과 호환됨.
 */
#include "rest_sdmmc_fw_run.h"

#include <cJSON.h>
#include <esp_err.h>
#include <esp_log.h>
#include <mbedtls/base64.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#include "base/sdmmc/sdmmc_init.h"
#include "base/net/tcp/tcp_server.h"
#include "core/firmware_upload/fw_ble_legacy_context.h"
#include "core/firmware_upload/fw_upload_manager.h"
#include "core/firmware_upload/fw_upload_state.h"
#include "core/firmware_upload/fw_upload_storage.h"
#include "core/firmware_upload/fw_upload_types.h"

static const char *TAG = "http_fw_run";

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

static void trim_ws(char *s) {
    if (!s) {
        return;
    }
    char *start = s;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

esp_err_t sdmmc_fw_run_post_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "sdmmc_fw_run_post_handler");
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

    cJSON *json_file_name = cJSON_GetObjectItem(root, "file_name");
    cJSON *json_file_type = cJSON_GetObjectItem(root, "file_type");
    cJSON *json_file_data = cJSON_GetObjectItem(root, "file_data");
    cJSON *json_file_format = cJSON_GetObjectItem(root, "file_format");
    cJSON *json_file_save_only = cJSON_GetObjectItem(root, "file_save_only");
    cJSON *json_file_size = cJSON_GetObjectItem(root, "file_size");
    if (!cJSON_IsString(json_file_name) || !cJSON_IsString(json_file_type)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid params");
        return ESP_FAIL;
    }

    char name_buf[64];
    char type_buf[16];
    snprintf(name_buf, sizeof(name_buf), "%s", json_file_name->valuestring);
    snprintf(type_buf, sizeof(type_buf), "%s", json_file_type->valuestring);
    bool has_inline_data = cJSON_IsString(json_file_data) &&
                           json_file_data->valuestring &&
                           json_file_data->valuestring[0] != '\0';
    const char *file_data = has_inline_data ? json_file_data->valuestring : NULL;
    bool save_only = cJSON_IsNumber(json_file_save_only) && (json_file_save_only->valueint != 0);
    size_t expected_size = 0;
    if (cJSON_IsNumber(json_file_size) && json_file_size->valuedouble > 0) {
        expected_size = (size_t)json_file_size->valuedouble;
    }
    fw_format_t fmt = FW_FMT_UNKNOWN;
    if (cJSON_IsString(json_file_format)) {
        fmt = fw_format_from_str(json_file_format->valuestring);
    }

    /* JSON에서 file_name을 보냈는지(저장 시 사용한 UUID와 동일) — run-only에서 파일 찾을 때 JSON 이름 우선 */
    bool name_from_json = (name_buf[0] != '\0');

    /* 2차: BLE 전용 컨텍스트(0x0055/0x0040)로 빈 필드 보완. TCP와 무관. */
    {
        char ble_name[64] = {0};
        char ble_type[16] = {0};
        size_t ble_expected = 0;
        if (fw_ble_legacy_get_snapshot(ble_name, sizeof(ble_name), ble_type, sizeof(ble_type), &ble_expected)) {
            if (type_buf[0] == '\0' && ble_type[0] != '\0') {
                snprintf(type_buf, sizeof(type_buf), "%s", ble_type);
                ESP_LOGI(TAG, "fw_run: file_type from BLE: %s", type_buf);
            }
            if (name_buf[0] == '\0' && ble_name[0] != '\0') {
                snprintf(name_buf, sizeof(name_buf), "%s", ble_name);
                ESP_LOGI(TAG, "fw_run: file_name from BLE: %s", name_buf);
            }
            if (expected_size == 0 && ble_expected > 0) {
                expected_size = ble_expected;
                ESP_LOGI(TAG, "fw_run: expected_size from BLE: %u", (unsigned)expected_size);
            }
            /* run-only: JSON에 file_name(UUID)이 있으면 저장된 파일명과 같으므로 그대로 사용. 없을 때만 BLE 이름 사용. */
            if (!has_inline_data && ble_name[0] != '\0' && !name_from_json) {
                snprintf(name_buf, sizeof(name_buf), "%s", ble_name);
                if (ble_type[0] != '\0') {
                    snprintf(type_buf, sizeof(type_buf), "%s", ble_type);
                }
                ESP_LOGI(TAG, "fw_run: run-only, using BLE file: %s.%s", name_buf, type_buf);
            }
        }
    }

    cJSON_Delete(root);
    trim_ws(name_buf);
    trim_ws(type_buf);
    /* 3차: file_type에 맞춰 exec/format 등은 fw_upload_prepare_meta 내부(fw_registry_lookup)에서 채워짐 */
    tcp_server_set_pending_file(name_buf, type_buf);
    if (expected_size > 0) {
        tcp_server_set_expected_size(expected_size);
    }

    /* 2.08 호환: run-only일 때 파일 없으면 worker 기동 없이 500만 반환 → BLE 0x8057 미전송 → 앱이 /fw_upload_run으로 진행 */
    if (!file_data) {
        if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error file not found on sd memory");
            return ESP_FAIL;
        }
        char path[384];
        if (fw_storage_build_path(path, sizeof(path), name_buf, type_buf)) {
            struct stat st;
            if (stat(path, &st) != 0) {
                ESP_LOGI(TAG, "fw_run: run-only, file not present -> 500 (no BLE)");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error file not found on sd memory");
                return ESP_FAIL;
            }
        }
    }

    if (file_data) {
        size_t data_len = strlen(file_data);
        size_t out_len = (data_len * 3) / 4 + 4;
        uint8_t *decoded = malloc(out_len);
        if (!decoded) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
            return ESP_FAIL;
        }
        size_t decoded_len = 0;
        int mret = mbedtls_base64_decode(decoded, out_len, &decoded_len,
                                         (const unsigned char *)file_data, data_len);
        if (mret != 0) {
            free(decoded);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "base64 decode fail");
            return ESP_FAIL;
        }
        if (expected_size > 0 && expected_size != decoded_len) {
            ESP_LOGW(TAG, "file_size mismatch: meta=%u decoded=%u",
                     (unsigned)expected_size, (unsigned)decoded_len);
        }
        fw_meta_t meta;
        if (fw_upload_prepare_meta(&meta, name_buf, type_buf, fmt) != ESP_OK) {
            free(decoded);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "meta init fail");
            return ESP_FAIL;
        }
        meta.save_only = save_only;
        if (fw_upload_save_buffer(&meta, decoded, decoded_len) != ESP_OK) {
            free(decoded);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "store fail");
            return ESP_FAIL;
        }
        free(decoded);
        if (save_only) {
            httpd_resp_sendstr(req, "File stored");
            return ESP_OK;
        }
    }

    bool started = false;
    esp_err_t ret_run = fw_upload_run_from_sdmmc(name_buf, type_buf, &started);
    if (ret_run == ESP_OK) {
        if (started) {
            httpd_resp_sendstr(req, "Firmware Update Start Success");
        } else {
            httpd_resp_sendstr(req, "Firmware processing done");
        }
        return ESP_OK;
    }
    const char *msg = fw_state_get_message();
    if (!msg || msg[0] == '\0') {
        msg = "update failed";
    }
    /* 2.08과 동일: run-only에서 파일 없음/오픈 실패 시 500 + 동일 문구 → 앱이 /fw_upload_run으로 진행 */
    if (!file_data && msg && (
            strstr(msg, "open fail") != NULL ||
            strstr(msg, "file not found") != NULL ||
            strstr(msg, "file invalid") != NULL ||
            strstr(msg, "not set") != NULL)) {
        msg = "Error file not found on sd memory";
    }
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, msg);
    return ESP_FAIL;
}
