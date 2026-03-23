/**
 * POST /fw_upload_run handler (Base64 펌웨어 업로드 + 저장 및/또는 실행).
 *
 * Legacy phone compatibility (fw_run_ui 없이 핸드폰 전용 사용 시):
 * 메타(meta)는 아래 3단계로 구성하여 웹(풀 JSON)과 폰(2.08 스타일 최소 JSON) 모두 호환.
 *   1차: 요청 JSON에서 추출 (file_name, file_type, file_data, file_size, file_save_only 등)
 *   2차: BLE로 수신한 데이터로 보완 (예: 0x0055 타입, 0x0040 크기)
 *   3차: file_type(S00, A00 등)에 맞춰 빈 필드 기본값 채우기 (exec, file_format, can_bitrate 등)
 * 이렇게 구성한 meta로 fw_upload_prepare_meta/fw_upload_save_buffer/fw_upload_run_from_sdmmc 에
 * 전달하면 레거시 폰과 호환됨.
 */
#include "rest_sdmmc_fw_upload_run_mobile.h"

#include <cJSON.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <mbedtls/base64.h>
#include <string.h>
#include <stdlib.h>

#include "base/sdmmc/sdmmc_init.h"
#include "core/firmware_upload/fw_ble_legacy_context.h"
#include "core/firmware_upload/fw_upload_manager.h"
#include "core/firmware_upload/fw_upload_meta_defaults.h"
#include "core/firmware_upload/fw_upload_types.h"
#include "protocol/ble_protocol.h"
#include "protocol/ble_cmd_parser.h"

static const char *TAG = "http_fw_upload";

#define LARGE_JSON_THRESHOLD  (512 * 1024)

/* 레거시 폰: 저장 완료 시 0x8054에 'v'(118) 전송 → 앱에 Complete 표시 */
static void send_ble_upload_complete(void) {
#if CONFIG_BT_NIMBLE_ENABLED
    uint8_t v = 'v';
    ble_protocol_send_cmd(ACK_PROGRAM_START_PHONE_ESP32, &v, 1);
#endif
}

/* 대용량 JSON: cJSON 파싱 없이 필드만 추출 (file_data 복사 없이 ptr+len만) */
typedef struct {
    char file_name[64];
    char file_type[16];
    const char *file_data;
    size_t file_data_len;
    bool save_only;
    size_t file_size;
    bool ok;
} large_fw_upload_parsed_t;

static void parse_large_fw_upload_json(const char *buf, size_t buf_len, large_fw_upload_parsed_t *out) {
    memset(out, 0, sizeof(*out));
    if (!buf || buf_len < 32) {
        return;
    }
    const char *p;
    const char *q;
    p = strstr(buf, "\"file_name\"");
    if (!p || (size_t)(p - buf) > buf_len - 32) return;
    p = strchr(p, ':');
    if (!p) return;
    p++;
    while (p < buf + buf_len && (*p == ' ' || *p == '\t')) p++;
    if (p >= buf + buf_len || *p != '"') return;
    p++;
    q = strchr(p, '"');
    if (!q || (size_t)(q - p) >= sizeof(out->file_name)) return;
    memcpy(out->file_name, p, (size_t)(q - p));
    out->file_name[q - p] = '\0';

    p = strstr(buf, "\"file_type\"");
    if (!p || (size_t)(p - buf) > buf_len - 16) return;
    p = strchr(p, ':');
    if (!p) return;
    p++;
    while (p < buf + buf_len && (*p == ' ' || *p == '\t')) p++;
    if (p >= buf + buf_len || *p != '"') return;
    p++;
    q = strchr(p, '"');
    if (!q || (size_t)(q - p) >= sizeof(out->file_type)) return;
    memcpy(out->file_type, p, (size_t)(q - p));
    out->file_type[q - p] = '\0';

    p = strstr(buf, "\"file_data\"");
    if (!p || (size_t)(p - buf) > buf_len - 2) return;
    p = strchr(p, ':');
    if (!p) return;
    p++;
    while (p < buf + buf_len && (*p == ' ' || *p == '\t')) p++;
    if (p >= buf + buf_len || *p != '"') return;
    p++;
    out->file_data = p;
    q = strchr(p, '"');
    if (!q || q <= p) return;
    out->file_data_len = (size_t)(q - p);

    p = strstr(buf, "\"file_save_only\"");
    if (p && (size_t)(p - buf) < buf_len - 4) {
        p = strchr(p, ':');
        if (p) {
            p++;
            out->save_only = (strchr(p, '1') != NULL);
        }
    }
    p = strstr(buf, "\"save_only\"");
    if (p && (size_t)(p - buf) < buf_len - 4) {
        p = strchr(p, ':');
        if (p) {
            p++;
            if (strchr(p, '1') != NULL) out->save_only = true;
        }
    }
    p = strstr(buf, "\"file_size\"");
    if (p && (size_t)(p - buf) < buf_len - 16) {
        p = strchr(p, ':');
        if (p) {
            p++;
            out->file_size = (size_t)strtoul(p, NULL, 10);
        }
    }
    out->ok = (out->file_name[0] != '\0' && out->file_type[0] != '\0' && out->file_data != NULL && out->file_data_len > 0);
}

#if 0
static esp_err_t read_json_body(httpd_req_t *req, cJSON **out_root) {
    char content_type[64];
    httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
    if (strstr(content_type, HTTPD_TYPE_JSON) == NULL) {
        ESP_LOGW(TAG, "read_json_body: Content-Type not JSON");
        return ESP_ERR_INVALID_ARG;
    }

    size_t json_data_size = req->content_len;
    /* 2.1MB 등 대용량은 PSRAM 사용 (내부 RAM 부족 방지) */
    char *json_data = (char *)heap_caps_calloc(1, json_data_size + 1,
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!json_data) {
        json_data = (char *)calloc(1, json_data_size + 1);
    }
    if (!json_data) {
        ESP_LOGW(TAG, "read_json_body: alloc fail size=%u", (unsigned)json_data_size);
        return ESP_ERR_NO_MEM;
    }

    size_t recv_size = 0;
    int last_per = -1;
    while (recv_size < req->content_len) {
        int ret = httpd_req_recv(req, json_data + recv_size, req->content_len - recv_size);
        if (ret <= 0) {
            ESP_LOGW(TAG, "read_json_body: recv fail recv_size=%u ret=%d", (unsigned)recv_size, ret);
            free(json_data);
            return ESP_FAIL;
        }
        recv_size += (size_t)ret;
#if CONFIG_BT_NIMBLE_ENABLED
        if (req->content_len > 0) {
            int cur_per = (int)((recv_size * 100) / req->content_len);
            if (cur_per > 100) cur_per = 100;
            /* 10% 단위로만 전송해 BLE 과부하/끊김 방지 (2%마다 시 50회 → 10%마다 11회) */
            if (cur_per != last_per && (cur_per % 10 == 0)) {
                uint8_t pct = (uint8_t)cur_per;
                ble_protocol_send_cmd(ACK_PROGRAM_START_PHONE_ESP32, &pct, 1);
                last_per = cur_per;
            }
        }
#endif
    }
#if CONFIG_BT_NIMBLE_ENABLED
    if (last_per != 100) {
        uint8_t pct = 100;
        ble_protocol_send_cmd(ACK_PROGRAM_START_PHONE_ESP32, &pct, 1);
    }
#endif
    json_data[recv_size] = '\0';

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    if (!root) {
        ESP_LOGW(TAG, "read_json_body: cJSON_Parse fail len=%u", (unsigned)recv_size);
        return ESP_ERR_INVALID_ARG;
    }
    *out_root = root;
    return ESP_OK;
}
#endif

/* 본문 전체를 PSRAM 버퍼로 수신 (대용량 경로용). 호출자가 free(*out_buf).
 * 레거시 폰 UI: 수신 진행 시 BLE 0x8054에 퍼센트(0~100) 전송, 완료 후 100 전송. */
static esp_err_t read_body_to_buffer(httpd_req_t *req, char **out_buf, size_t *out_len) {
    char content_type[64];
    httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
    if (strstr(content_type, HTTPD_TYPE_JSON) == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t len = req->content_len;
    char *buf = (char *)heap_caps_calloc(1, len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) buf = (char *)calloc(1, len + 1);
    if (!buf) return ESP_ERR_NO_MEM;
    size_t recv_size = 0;
    int last_per = -1;
    while (recv_size < len) {
        int ret = httpd_req_recv(req, buf + recv_size, len - recv_size);
        if (ret <= 0) {
            free(buf);
            return ESP_FAIL;
        }
        recv_size += (size_t)ret;
#if CONFIG_BT_NIMBLE_ENABLED
        if (len > 0) {
            int cur_per = (int)((recv_size * 100) / len);
            if (cur_per > 100) cur_per = 100;
            /* 10% 단위로만 전송해 BLE 과부하/끊김 방지 */
            if (cur_per != last_per && (cur_per % 10 == 0)) {
                uint8_t pct = (uint8_t)cur_per;
                ble_protocol_send_cmd(ACK_PROGRAM_START_PHONE_ESP32, &pct, 1);
                last_per = cur_per;
            }
        }
#endif
    }
#if CONFIG_BT_NIMBLE_ENABLED
    if (last_per != 100) {
        uint8_t pct = 100;
        ble_protocol_send_cmd(ACK_PROGRAM_START_PHONE_ESP32, &pct, 1);
    }
#endif
    buf[recv_size] = '\0';
    *out_buf = buf;
    *out_len = recv_size;
    return ESP_OK;
}
int G_COUNT = 0;
esp_err_t sdmmc_fw_upload_run_post_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "sdmmc_fw_upload_run_post_handler (len=%d) heap=%u psram=%u",
             (int)req->content_len,
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "sdmmc init fail");
        return ESP_FAIL;
    }

    /* 대용량(2.1MB 등): cJSON 파싱 없이 필드만 추출 → 메모리 부족 방지 */
    if (1)//req->content_len > LARGE_JSON_THRESHOLD) {
    {    char *raw_buf = NULL;
        size_t raw_len = 0;
        esp_err_t ret = read_body_to_buffer(req, &raw_buf, &raw_len);
        if (ret != ESP_OK) {
            const char *err_msg = (ret == ESP_ERR_NO_MEM) ? "no memory" : (ret == ESP_FAIL) ? "recv fail" : "invalid json";
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err_msg);
            return ESP_FAIL;
        }

        large_fw_upload_parsed_t parsed;
        parse_large_fw_upload_json(raw_buf, raw_len, &parsed);
        if (!parsed.ok) {
            free(raw_buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid params");
            return ESP_FAIL;
        }
        char name_buf[64];
        char type_buf[16];
        snprintf(name_buf, sizeof(name_buf), "%s", parsed.file_name);
        snprintf(type_buf, sizeof(type_buf), "%s", parsed.file_type);
        size_t expected_size = parsed.file_size;
        bool save_only = parsed.save_only;

        /* 1차: 들어온 JSON 전체를 SDMMC에 그대로 저장.
         * 파일명은 "<file_name>.json" 형태로 한다. */
        {
            char json_path[256];
            G_COUNT++;
            snprintf(json_path, sizeof(json_path), "%s/%d_%s.%s.json",
                     sdmmc_get_mount(), G_COUNT,name_buf,type_buf);
            FILE *jf = fopen(json_path, "w");
            if (jf) {

                fwrite(raw_buf, 1, raw_len, jf);
                fclose(jf);
            }
        }

        /* 2차: BLE 보완 */
        {
            char ble_name[64] = {0};
            char ble_type[16] = {0};
            size_t ble_expected = 0;
            if (fw_ble_legacy_get_snapshot(ble_name, sizeof(ble_name), ble_type, sizeof(ble_type), &ble_expected)) {
                if (type_buf[0] == '\0' && ble_type[0] != '\0') snprintf(type_buf, sizeof(type_buf), "%s", ble_type);
                if (name_buf[0] == '\0' && ble_name[0] != '\0') snprintf(name_buf, sizeof(name_buf), "%s", ble_name);
                if (expected_size == 0 && ble_expected > 0) expected_size = ble_expected;
            }
        }

        size_t data_len = parsed.file_data_len;
        size_t out_len = (data_len * 3) / 4 + 4;

        /* In-place 디코딩: decoded 출력이 입력보다 짧아서(3/4) 같은 버퍼 재사용 가능.
         * mbedtls는 in-place를 문서화하지 않으나, 순차 읽기/쓰기 시 쓰기 위치가 읽기보다
         * 항상 뒤처지므로 동작상 안전함. 보수적으로 하려면 아래처럼 별도 버퍼 사용. */
        uint8_t *decoded = (uint8_t *)parsed.file_data;
        size_t decoded_len = 0;

        int mret = mbedtls_base64_decode(decoded, out_len, &decoded_len,
                                         (const unsigned char *)parsed.file_data, data_len);

        if (mret != 0) {
            free(raw_buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "base64 decode fail");
            return ESP_FAIL;
        }

        fw_meta_t meta;
        /* JSON에서 file_type 을 안 직후: 타입 기반 default meta 초기화 */
        memset(&meta, 0, sizeof(meta));
        snprintf(meta.file_name, sizeof(meta.file_name), "%s", name_buf);
        /* file_type 은 최대 3글자 코드("A11" 등)이므로 안전하게 3글자만 복사 */
        snprintf(meta.file_type, sizeof(meta.file_type), "%.3s", type_buf);
        if (fw_meta_init_defaults(&meta) != ESP_OK) {
            free(raw_buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "meta init fail");
            return ESP_FAIL;
        }
        meta.save_only = save_only;
        meta.original_size = decoded_len;
        if (expected_size > 0 && expected_size != decoded_len) {
            ESP_LOGW(TAG, "file_size mismatch: meta=%u decoded=%u", (unsigned)expected_size, (unsigned)decoded_len);
        }

        if (fw_upload_save_buffer(&meta, decoded, decoded_len) != ESP_OK) {
            free(raw_buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "store fail");
            return ESP_FAIL;
        }

        // 저장이 완료된 후 원본 버퍼(in-place 디코딩된 내용 포함)를 해제합니다.
        free(raw_buf);
        send_ble_upload_complete();

        if (save_only) {
            ESP_LOGI(TAG, "save_only=1 done (large): %s.%s + %s.%s.meta", name_buf, type_buf, name_buf, type_buf);
            // 다운로드(저장) 완료 후, 이후 재접속을 위해 AP는 유지하되 현재 스테이션 연결만 끊는다.
            esp_wifi_deauth_sta(0); // 0: 모든 연결된 스테이션 deauth
            httpd_resp_sendstr(req, "File uploaded (encrypted) successfully");
            return ESP_OK;
        }

        bool started = false;
        if (fw_upload_run_from_sdmmc(name_buf, type_buf, &started) == ESP_OK && started) {
            // 다운로드 및 실행 시작 후 폰과의 Wi-Fi 연결만 끊는다.
            esp_wifi_deauth_sta(0);
            httpd_resp_sendstr(req, "File uploaded and update started");
            return ESP_OK;
        }
        // 실행은 시작되지 않았지만 파일 저장은 완료된 경우에도 연결만 끊는다.
        esp_wifi_deauth_sta(0);
        httpd_resp_sendstr(req, "File uploaded (encrypted) successfully");
        return ESP_OK;
    }
#if 0
    // --- (작은 파일 처리 cJSON 분기) ---
    cJSON *root = NULL;
    esp_err_t ret = read_json_body(req, &root);
    if (ret != ESP_OK) {
        const char *err_msg = (ret == ESP_ERR_NO_MEM) ? "no memory" :
                              (ret == ESP_FAIL) ? "recv fail" : "invalid json";
        ESP_LOGE(TAG, "read_json_body ret=%d -> %s", (int)ret, err_msg);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err_msg);
        return ESP_FAIL;
    }

    cJSON *json_file_name = cJSON_GetObjectItem(root, "file_name");
    cJSON *json_file_type = cJSON_GetObjectItem(root, "file_type");
    cJSON *json_file_data = cJSON_GetObjectItem(root, "file_data");
    cJSON *json_file_format = cJSON_GetObjectItem(root, "file_format");
    cJSON *json_exec = cJSON_GetObjectItem(root, "exec");
    cJSON *json_target_board = cJSON_GetObjectItem(root, "target_board");
    cJSON *json_target_id = cJSON_GetObjectItem(root, "target_id");
    cJSON *json_can_bitrate = cJSON_GetObjectItem(root, "can_bitrate");
    cJSON *json_uart_baud = cJSON_GetObjectItem(root, "uart_baud");
    cJSON *json_swd_clock = cJSON_GetObjectItem(root, "swd_clock_khz");
    cJSON *json_jtag_clock = cJSON_GetObjectItem(root, "jtag_clock_khz");
    cJSON *json_meta = cJSON_GetObjectItem(root, "meta_json");
    cJSON *json_save_only = cJSON_GetObjectItem(root, "save_only");
    cJSON *json_file_save_only = cJSON_GetObjectItem(root, "file_save_only");
    cJSON *json_file_size = cJSON_GetObjectItem(root, "file_size");

    if (!cJSON_IsString(json_file_name) || !cJSON_IsString(json_file_type) || !cJSON_IsString(json_file_data)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid params");
        return ESP_FAIL;
    }

    char name_buf[64] = {0};
    char type_buf[16] = {0};
    snprintf(name_buf, sizeof(name_buf), "%s", json_file_name->valuestring);
    snprintf(type_buf, sizeof(type_buf), "%s", json_file_type->valuestring);

    const char *file_data = json_file_data->valuestring;
    fw_format_t fmt = FW_FMT_UNKNOWN;
    if (cJSON_IsString(json_file_format)) {
        fmt = fw_format_from_str(json_file_format->valuestring);
    }

    bool save_only = cJSON_IsNumber(json_save_only) && (json_save_only->valueint != 0);
    if (cJSON_IsNumber(json_file_save_only)) {
        save_only = (json_file_save_only->valueint != 0);
    }

    size_t expected_size = 0;
    if (cJSON_IsNumber(json_file_size) && json_file_size->valuedouble > 0) {
        expected_size = (size_t)json_file_size->valuedouble;
    }

    /* 2차: BLE 전용 컨텍스트 보완 */
    {
        char ble_name[64] = {0};
        char ble_type[16] = {0};
        size_t ble_expected = 0;
        if (fw_ble_legacy_get_snapshot(ble_name, sizeof(ble_name), ble_type, sizeof(ble_type), &ble_expected)) {
            if (type_buf[0] == '\0' && ble_type[0] != '\0') {
                snprintf(type_buf, sizeof(type_buf), "%s", ble_type);
                ESP_LOGI(TAG, "fw_upload_run: file_type from BLE: %s", type_buf);
            }
            if (name_buf[0] == '\0' && ble_name[0] != '\0') {
                snprintf(name_buf, sizeof(name_buf), "%s", ble_name);
                ESP_LOGI(TAG, "fw_upload_run: file_name from BLE: %s", name_buf);
            }
            if (expected_size == 0 && ble_expected > 0) {
                expected_size = ble_expected;
                ESP_LOGI(TAG, "fw_upload_run: expected_size from BLE: %u", (unsigned)expected_size);
            }
        }
    }

    const char *file_name = name_buf;
    const char *file_type = type_buf;

    /* 1차: 들어온 JSON 전체를 SDMMC에 저장.
     * 파일명은 "<file_name>.json" 형태로 한다. */
    {
        char *json_str = cJSON_PrintUnformatted(root);
        if (json_str) {
            char json_path[256];
            snprintf(json_path, sizeof(json_path), "%s/%s.json",
                     sdmmc_get_mount(), file_name);
            FILE *jf = fopen(json_path, "w");
            if (jf) {
                fputs(json_str, jf);
                fclose(jf);
            }
            cJSON_free(json_str);
        }
    }

    size_t data_len = strlen(file_data);
    size_t out_len = (data_len * 3) / 4 + 4;

    /* In-place 디코딩: cJSON이 할당한 file_data 버퍼를 출력으로 재사용.
     * Base64는 디코드 결과가 원문보다 짧아 동작상 안전. cJSON_Delete 시 해당 메모리 해제됨. */
    uint8_t *decoded = (uint8_t *)((void *)file_data);
    size_t decoded_len = 0;

    int mret = mbedtls_base64_decode(decoded, out_len, &decoded_len,
                                     (const unsigned char *)file_data, data_len);
    if (mret != 0) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "base64 decode fail");
        return ESP_FAIL;
    }

    fw_meta_t meta;
    /* JSON에서 file_type 을 안 직후: 타입 기반 default meta 초기화 */
    memset(&meta, 0, sizeof(meta));
    snprintf(meta.file_name, sizeof(meta.file_name), "%s", file_name);
    /* file_type 은 최대 3글자 코드("A11" 등)이므로 안전하게 3글자만 복사 */
    snprintf(meta.file_type, sizeof(meta.file_type), "%.3s", file_type);
    if (fw_meta_init_defaults(&meta) != ESP_OK) {
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
    meta.save_only = save_only;
    meta.original_size = decoded_len;

    if (expected_size > 0 && expected_size != decoded_len) {
        ESP_LOGW(TAG, "file_size mismatch: meta=%u decoded=%u",
                 (unsigned)expected_size, (unsigned)decoded_len);
    }

    if (fw_upload_save_buffer(&meta, decoded, decoded_len) != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "store fail");
        return ESP_FAIL;
    }

    // In-place 사용 시, 전체 JSON 트리를 메모리 해제할 때 decoded가 위치한 메모리도 정상적으로 같이 해제됩니다.
    cJSON_Delete(root);
    send_ble_upload_complete();

    /* 2.08 동일: 저장 시 SDMMC에 2개 파일 생성 — name.type(암호화 바이너리), name.type.meta(메타 JSON) */
    if (save_only) {
        ESP_LOGI(TAG, "save_only=1 done: %s.%s + %s.%s.meta", file_name, file_type, file_name, file_type);
    }
    if (!save_only) {
        bool started = false;
        if (fw_upload_run_from_sdmmc(file_name, file_type, &started) == ESP_OK && started) {
            ESP_LOGI(TAG, "upload+run done heap=%u psram=%u",
                     (unsigned)esp_get_free_heap_size(),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            // 실행 시작 후 현재 연결된 스테이션만 끊기
            esp_wifi_deauth_sta(0);
            httpd_resp_sendstr(req, "File uploaded and update started");
            return ESP_OK;
        }
    }
    ESP_LOGI(TAG, "upload done heap=%u psram=%u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    // 저장만 완료된 경우에도 다운로드 직후 연결만 끊기
    esp_wifi_deauth_sta(0);
    httpd_resp_sendstr(req, "File uploaded (encrypted) successfully");
    return ESP_OK;
#endif

}
