#include "ble_cmd_parser.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <esp_log.h>
#include <dirent.h>
#include <sys/stat.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_err.h>
#include <esp_vfs_fat.h>
#include <ctype.h>
#if CONFIG_BT_NIMBLE_ENABLED
#include <services/gap/ble_svc_gap.h>
#endif

#include "core/role_dispatcher.h"
#include "core/hht/hht_main.h"
#include "core/hht/hht_keys.h"
#include "config/version.h"
#include "protocol/ble_protocol.h"
#include "base/sdmmc/sdmmc_init.h"
#include "base/spiflash_fs/spiflash_fs.h"
#include "base/runtime_config/runtime_config.h"
#include "core/esp32_update/esp32_update.h"
#include "core/firmware_upload/fw_ble_legacy_context.h"
#include "core/firmware_upload/fw_upload_manager.h"
#include "config/storage_paths.h"
#include "base/console/cli_hht_view.h"
#include "base/net/tcp/tcp_server.h"

static const char *TAG = "ble_cmd";
static const size_t BLE_NAME_MAX_LEN = 9;
static const size_t BLE_NAME_SUFFIX_LEN = 4;
static char s_last_fw_type[8] = {0};
static const char *k_sdmmc_program_types[] = {
    "S00", "S01", "A00", "A01", "A02", "A03", "A04", "A05", "A06",
    "A07", "A08", "A09", "A10", "A11", "A12", "A13", "A14", "A15",
};

static const char *get_filename_ext(const char *name);
static bool ble_hht_key_from_code(uint8_t code, uint8_t *out_key, bool *out_long);

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

static void ble_set_last_fw_type(const uint8_t *payload, size_t payload_len) {
    if (!payload || payload_len < 3) {
        return;
    }
    char type_buf[8] = {0};
    memcpy(type_buf, payload, 3);
    type_buf[3] = '\0';
    if (!sdmmc_is_valid_type(type_buf)) {
        ESP_LOGW(TAG, "OTA update type invalid: %s", type_buf);
        return;
    }
    snprintf(s_last_fw_type, sizeof(s_last_fw_type), "%s", type_buf);
    ESP_LOGI(TAG, "OTA update type set: %s", s_last_fw_type);
}

static void ble_build_default_ota_name(char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    uint32_t ts = (uint32_t)esp_log_timestamp();
    snprintf(out, out_size, "ota_%02x%02x%02x%02x%02x%02x_%08" PRIx32,
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ts);
}

static bool ble_hht_key_from_code(uint8_t code, uint8_t *out_key, bool *out_long) {
    if (!out_key) {
        return false;
    }
    if (out_long) {
        *out_long = false;
    }
    switch (code) {
        case HHT_KEY_CODE_NONE:
            *out_key = HHT_KEY_NONE;
            return true;
        case HHT_KEY_CODE_ESC:
            *out_key = HHT_KEY_ESC;
            return true;
        case HHT_KEY_CODE_UP:
            *out_key = HHT_KEY_UP;
            return true;
        case HHT_KEY_CODE_DN:
            *out_key = HHT_KEY_DN;
            return true;
        case HHT_KEY_CODE_ENT:
            *out_key = HHT_KEY_ENT;
            return true;
        case HHT_KEY_CODE_ESC_ENT:
            *out_key = HHT_KEY_ESC_ENT;
            return true;
        case HHT_KEY_CODE_UP_ENT:
            *out_key = HHT_KEY_UP_ENT;
            return true;
        case HHT_KEY_CODE_ESC_LONG:
            *out_key = HHT_KEY_ESC;
            if (out_long) {
                *out_long = true;
            }
            return true;
        case HHT_KEY_CODE_UP_LONG:
            *out_key = HHT_KEY_UP;
            if (out_long) {
                *out_long = true;
            }
            return true;
        case HHT_KEY_CODE_DN_LONG:
            *out_key = HHT_KEY_DN;
            if (out_long) {
                *out_long = true;
            }
            return true;
        case HHT_KEY_CODE_ENT_LONG:
            *out_key = HHT_KEY_ENT;
            if (out_long) {
                *out_long = true;
            }
            return true;
        default:
            return false;
    }
}

static void ble_send_json_ack(uint16_t ack_cmd, const char *json) {
    if (!json) {
        ble_protocol_send_cmd(ack_cmd, NULL, 0);
        return;
    }
    ble_protocol_send_cmd(ack_cmd, (const uint8_t *)json, strlen(json));
}

static void ble_trim_whitespace(char *s) {
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

static const char *ble_current_name(void) {
#if CONFIG_BT_NIMBLE_ENABLED
    const char *name = ble_svc_gap_device_name();
    if (name && name[0] != '\0') {
        return name;
    }
#endif
    return runtime_config_get_device_name();
}

static bool ble_name_valid(const char *name) {
    size_t len = name ? strlen(name) : 0;
    if (!name || len != BLE_NAME_MAX_LEN) {
        return false;
    }
    if (strncmp(name, "MIRI-", 5) != 0) {
        return false;
    }
    for (size_t i = 0; i < BLE_NAME_SUFFIX_LEN; i++) {
        if (!isxdigit((unsigned char)name[5 + i])) {
            return false;
        }
    }
    return true;
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

static bool json_get_string(const char *json, const char *key, char *out, size_t out_size) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) {
        return false;
    }
    p = strchr(p, ':');
    if (!p) {
        return false;
    }
    p++;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p != '\"') {
        return false;
    }
    p++;
    const char *end = strchr(p, '\"');
    if (!end) {
        return false;
    }
    size_t len = (size_t)(end - p);
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

static void ble_send_file_list(uint16_t ack_cmd, const char *mount_path) {
    DIR *dir = opendir(mount_path);
    if (!dir) {
        ble_send_json_ack(ack_cmd, "{\"result\":-1}");
        return;
    }

    int total = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        char filepath[384];
        struct stat st;
        snprintf(filepath, sizeof(filepath), "%s/%s", mount_path, ent->d_name);
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
            total++;
        }
    }
    closedir(dir);

    if (total == 0) {
        ble_send_json_ack(ack_cmd, "{\"result\":0,\"chunk_idx\":0,\"chunk_total\":0}");
        return;
    }

    dir = opendir(mount_path);
    if (!dir) {
        ble_send_json_ack(ack_cmd, "{\"result\":-1}");
        return;
    }

    int idx = 0;
    while ((ent = readdir(dir)) != NULL) {
        char filepath[384];
        struct stat st;
        snprintf(filepath, sizeof(filepath), "%s/%s", mount_path, ent->d_name);
        if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        char name_buf[128];
        const char *ext = get_filename_ext(ent->d_name);
        get_filename_without_extension(ent->d_name, name_buf, sizeof(name_buf));

        char json[256];
        snprintf(json, sizeof(json),
                 "{\"result\":0,\"chunk_idx\":%d,\"chunk_total\":%d,"
                 "\"file_name\":\"%s\",\"file_type\":\"%s\",\"file_size\":%ld}",
                 idx, total, name_buf, ext, (long)st.st_size);
        ble_send_json_ack(ack_cmd, json);
        idx++;
    }

    closedir(dir);
}

static void ble_send_sdmmc_program_list(uint16_t ack_cmd) {
    const char *mount_path = sdmmc_get_mount();
    DIR *dir = opendir(mount_path);
    if (!dir) {
        ble_send_json_ack(ack_cmd, "{\"result\":-1}");
        return;
    }

    int total = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (sdmmc_is_valid_program_file(ent->d_name)) {
            total++;
        }
    }
    closedir(dir);

    if (total == 0) {
        ble_send_json_ack(ack_cmd, "{\"result\":0,\"chunk_idx\":0,\"chunk_total\":0}");
        return;
    }

    dir = opendir(mount_path);
    if (!dir) {
        ble_send_json_ack(ack_cmd, "{\"result\":-1}");
        return;
    }

    int idx = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (!sdmmc_is_valid_program_file(ent->d_name)) {
            continue;
        }
        char name_buf[128];
        const char *ext = get_filename_ext(ent->d_name);
        get_filename_without_extension(ent->d_name, name_buf, sizeof(name_buf));

        char json[256];
        snprintf(json, sizeof(json),
                 "{\"result\":0,\"chunk_idx\":%d,\"chunk_total\":%d,"
                 "\"file_name\":\"%s\",\"file_type\":\"%s\"}",
                 idx, total, name_buf, ext);
        ble_send_json_ack(ack_cmd, json);
        idx++;
    }

    closedir(dir);
}

static bool ble_cmd_is_start(uint16_t cmd) {
    switch (cmd) {
        case CMD_START_HHT_100:
        case CMD_START_HHT_WB100:
        case CMD_START_HHT_3000:
        case CMD_FW_UPLOAD:
        case CMD_MIRI_DIAGNOSIS_REQ:
        case CMD_HANDLE_PHONE_ESP32:
            return true;
        default:
            return false;
    }
}

static bool ble_cmd_is_stop(uint16_t cmd) {
    switch (cmd) {
        case CMD_STOP_HHT_100:
        case CMD_STOP_HHT_WB100:
        case CMD_STOP_HHT_3000:
        case CMD_PROGRAM_STOP_PHONE_ESP32:
        case CMD_FW_UPLOAD_STOP:
            return true;
        default:
            return false;
    }
}

static bool ble_cmd_is_hht_action(uint16_t cmd) {
    switch (cmd) {
        case CMD_HHT_BTN:
        case CMD_HHT_MSG_100:
        case CMD_HHT_PARAMS:
        case CMD_HHT_PROJ_100:
        case CMD_HHT_VER_CP_100:
        case CMD_HHT_VER_INV_100:
        case CMD_HHT_PRAMS_INV:
        case CMD_HHT_FDC:
            return true;
        default:
            return false;
    }
}

role_t ble_cmd_to_role(uint16_t cmd) {
    switch (cmd) {
        case CMD_START_HHT_100:
        case CMD_STOP_HHT_100:
        case CMD_HHT_MSG_100:
        case CMD_HHT_BTN:
        case CMD_START_HHT_WB100:
        case CMD_STOP_HHT_WB100:
        case CMD_START_HHT_3000:
        case CMD_STOP_HHT_3000:
        case CMD_HHT_PARAMS:
        case CMD_HHT_PROJ_100:
        case CMD_HHT_VER_CP_100:
        case CMD_HHT_VER_INV_100:
        case CMD_HHT_PRAMS_INV:
        case CMD_HHT_FDC:
            return ROLE_HHT;
        case CMD_FW_UPLOAD:
            return ROLE_FW_UPLOAD;
        case CMD_SDMMC_PROGRAM_ENTER:
        case CMD_SDMMC_PROGRAM_LIST:
        case CMD_FAT_FILE_LIST:
        case CMD_SDMMC_FORMAT:
        case CMD_FAT_FORMAT:
        case CMD_SDMMC_DELETE_FILE:
        case CMD_FAT_DELETE_FILE:
            return ROLE_STORAGE_UPLOAD;
        case CMD_PROGRAM_START_PHONE_ESP32:
        case CMD_PROGRAM_ENTER_PHONE_ESP32:
        case CMD_PROGRAM_WRITE_PHONE_ESP32:
        case CMD_PROGRAM_INTERFACE_RESET_PHONE_ESP32:
        case CMD_PROGRAM_STOP_PHONE_ESP32:
        case CMD_UPDATE_TYPE_PHONE_ESP32:
        case CMD_PROGRAM_PROGRESS_MSG:
            return ROLE_FW_UPLOAD;
        case CMD_MIRI_DIAGNOSIS_REQ:
            return ROLE_DIAG;
        case CMD_HANDLE_PHONE_ESP32:
            return ROLE_MANUAL;
        default:
            return ROLE_NONE;
    }
}

void ble_cmd_handle(uint16_t cmd) {
    ble_cmd_handle_with_payload(cmd, NULL, 0);
}

void ble_cmd_handle_with_payload(uint16_t cmd, const uint8_t *payload, size_t payload_len) {

    ESP_LOGI(TAG, "BLE RX : 0x%04x",cmd);

    if (ble_cmd_is_hht_action(cmd)) {
        if (cmd == CMD_HHT_MSG_100) {
            ESP_LOGI(TAG, "HHT msg rx from BLE (len=%u)", (unsigned)payload_len);
            return;
        }
        if (cmd == CMD_HHT_BTN) {
            if (!payload || payload_len < 1) {
                ESP_LOGW(TAG, "HHT key cmd missing payload");
                return;
            }
            uint8_t key = 0;
            bool is_long = false;
            if (!ble_hht_key_from_code(payload[0], &key, &is_long)) {
                ESP_LOGW(TAG, "HHT key cmd invalid: 0x%02x", payload[0]);
                return;
            }
            if (role_dispatcher_get_active() == ROLE_HHT) {
                if (!cli_hht_view_is_active()) {
                    ESP_LOGI(TAG, "HHT key code=0x%02x -> 0x%02x%s",
                             payload[0], key, is_long ? " (long)" : "");
                }
                if (is_long) {
                    hht_keys_send_long(key);
                } else {
                    hht_keys_send(key);
                }
            }
            ble_protocol_send_cmd(ACK_HHT_BTN, NULL, 0);
            return;
        }
        if (!payload || payload_len == 0) {
            ESP_LOGW(TAG, "HHT key cmd missing payload");
            return;
        }
        ESP_LOGI(TAG, "HHT action cmd=0x%04x (len=%u)", cmd, (unsigned)payload_len);
        return;
    }

    if (ble_cmd_is_stop(cmd)) {
        if (cmd == CMD_STOP_HHT_100 || cmd == CMD_STOP_HHT_WB100 || cmd == CMD_STOP_HHT_3000) {
            ESP_LOGI(TAG, "HHT stop");
            role_dispatcher_clear();
            ble_protocol_send_cmd(ACK_STOP_HHT_100, NULL, 0);
            return;
        }
        if (cmd == CMD_PROGRAM_STOP_PHONE_ESP32) {
            ESP_LOGI(TAG, "OTA update stop");
            role_dispatcher_clear();
            ble_protocol_send_cmd(ACK_PROGRAM_STOP_PHONE_ESP32, NULL, 0);
            return;
        }
        if (cmd == CMD_FW_UPLOAD_STOP) {
            ESP_LOGI(TAG, "Firmware update stop");
            role_dispatcher_clear();
            ble_protocol_send_cmd(ACK_FW_UPLOAD_STOP, NULL, 0);
            return;
        }
        ESP_LOGW(TAG, "stop cmd not handled: 0x%04x", cmd);
        return;
    }

    /*
     * BLE command -> role translation.
     * The caller should already have validated that this is a role command.
     */
    if (ble_cmd_is_start(cmd)) {
        if (cmd == CMD_START_HHT_100 || cmd == CMD_START_HHT_WB100 || cmd == CMD_START_HHT_3000) {
            if (cmd == CMD_START_HHT_WB100) {
                hht_set_type(HHT_TYPE_WB100);
            } else if (cmd == CMD_START_HHT_3000) {
                hht_set_type(HHT_TYPE_3000);
            } else {
                hht_set_type(HHT_TYPE_100);
            }
            ESP_LOGI(TAG, "HHT start (type=%d)", hht_get_type());
            role_dispatcher_request(ROLE_HHT, ROLE_SOURCE_BLE);
            ble_protocol_send_cmd(ACK_START_HHT_100, NULL, 0);
            return;
        }
        role_t role = ble_cmd_to_role(cmd);
        ESP_LOGI(TAG, "start cmd=0x%04x -> role=%d", cmd, role);
        role_dispatcher_request(role, ROLE_SOURCE_BLE);
        return;
    }

    switch (cmd) {
        case CMD_MIRI_REBOOT:
            ESP_LOGW(TAG, "reboot by BLE cmd");
            ble_protocol_send_cmd(ACK_MIRI_REBOOT, NULL, 0);
            esp_restart();
            return;
        case CMD_ESP32_VERSION_INFO:
        {
            char ver_buf[64];
            snprintf(ver_buf, sizeof(ver_buf), "%s, %s", MIRI_TOOL_VERSION_STR, __DATE__);
            ble_protocol_send_cmd(ACK_CMD_ESP32_VERSION_INFO, (const uint8_t *)ver_buf,
                                  strlen(ver_buf) + 1);
            ESP_LOGI(TAG, "%s", ver_buf);
            return;
        }
        case CMD_MAC_BT_INFO: {
            uint8_t mac[6];
            ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_BT));
            ESP_LOGI(TAG, "BT MAC %02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            ble_protocol_send_cmd(ACK_MAC_BT_INFO, mac, sizeof(mac));
            return;
        }
        case CMD_ESP32_ADV_RENAME:
            if (payload && payload_len > 0) {
                char name_buf[32];
                size_t n = (payload_len >= sizeof(name_buf)) ? sizeof(name_buf) - 1 : payload_len;
                memcpy(name_buf, payload, n);
                name_buf[n] = '\0';
                ble_trim_whitespace(name_buf);
                ESP_LOGI(TAG, "BLE rename req: %s", name_buf);
                if (!ble_name_valid(name_buf)) {
                    char json[128];
                    snprintf(json, sizeof(json),
                             "{\"result\":-3,\"reason\":\"invalid_format\",\"name\":\"%s\"}",
                             ble_current_name());
                    ble_send_json_ack(ACK_CMD_ESP32_ADV_RENAME, json);
                    return;
                }
                esp_err_t err = spiflash_fs_set_ble_device_name(name_buf);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "BLE rename store failed: %s", esp_err_to_name(err));
                    char json[128];
                    snprintf(json, sizeof(json),
                             "{\"result\":-1,\"reason\":\"%s\",\"name\":\"%s\"}",
                             esp_err_to_name(err), ble_current_name());
                    ble_send_json_ack(ACK_CMD_ESP32_ADV_RENAME, json);
                    return;
                } else {
                    runtime_config_set_device_alias(name_buf);
                    runtime_config_save();
#if CONFIG_BT_NIMBLE_ENABLED
                    ble_svc_gap_device_name_set(name_buf);
#endif
                    char json[128];
                    snprintf(json, sizeof(json),
                             "{\"result\":0,\"name\":\"%s\"}", name_buf);
                    ble_send_json_ack(ACK_CMD_ESP32_ADV_RENAME, json);
                    return;
                }
            } else {
                ESP_LOGW(TAG, "BLE rename req without payload");
                char json[128];
                snprintf(json, sizeof(json),
                         "{\"result\":-2,\"reason\":\"no_payload\",\"name\":\"%s\"}",
                         ble_current_name());
                ble_send_json_ack(ACK_CMD_ESP32_ADV_RENAME, json);
            }
            return;
        case CMD_UPDATE_INFO_MOBILE:
            if (payload && payload_len > 0) {
                ESP_LOGI(TAG, "UPDATE_INFO_MOBILE len=%u", (unsigned)payload_len);
                ble_protocol_send_cmd(ACK_UPDATE_INFO_MOBILE, NULL, 0);
            } else {
                ESP_LOGW(TAG, "UPDATE_INFO_MOBILE empty");
            }
            return;
        case CMD_MCU_VERSION_INFO:
        {
            const char *msg = "MCU=N/A";
            ble_protocol_send_cmd(ACK_CMD_MCU_VERSION_INFO, (const uint8_t *)msg, strlen(msg) + 1);
            return;
        }
        case CMD_PROGRAM_ENTER_WIFI_ESP32:
        {
            uint8_t ack = 0x55;
            if (payload_len >= 3) {
                uint32_t length = (uint32_t)payload[0] |
                                  ((uint32_t)payload[1] << 8) |
                                  ((uint32_t)payload[2] << 16);
                ESP_LOGI(TAG, "OTA update enter(wifi) len=%u", (unsigned)length);
                tcp_server_set_expected_size((size_t)length);
                fw_ble_legacy_set_expected_size((size_t)length);
                if (!tcp_server_has_pending() && s_last_fw_type[0] != '\0') {
                    char name_buf[64];
                    ble_build_default_ota_name(name_buf, sizeof(name_buf));
                    tcp_server_set_pending_file(name_buf, s_last_fw_type);
                    fw_ble_legacy_set_file_name(name_buf);
                    ESP_LOGI(TAG, "OTA pending file auto-set: %s.%s", name_buf, s_last_fw_type);
                }
            } else {
                ESP_LOGW(TAG, "OTA update enter(wifi) payload too short");
                ack = 0x56;
            }
            role_dispatcher_request(ROLE_FW_UPLOAD, ROLE_SOURCE_BLE);
            ble_protocol_send_cmd(ACK_PROGRAM_ENTER_WIFI_ESP32, &ack, 1);
            return;
        }
        case CMD_PROGRAM_ENTER_PHONE_ESP32:
            ESP_LOGI(TAG, "OTA update enter");
            role_dispatcher_request(ROLE_FW_UPLOAD, ROLE_SOURCE_BLE);
            ble_protocol_send_cmd(ACK_PROGRAM_ENTER_PHONE_ESP32, NULL, 0);
            return;
        case CMD_PROGRAM_WRITE_PHONE_ESP32:
            if (payload_len >= 3) {
                uint32_t addr = (uint32_t)payload[0] << 16 |
                                (uint32_t)payload[1] << 8 |
                                (uint32_t)payload[2];
                ESP_LOGI(TAG, "OTA update write addr=0x%06x len=%u",
                         (unsigned)addr, (unsigned)(payload_len - 3));
            } else {
                ESP_LOGW(TAG, "OTA update write payload too short");
            }
            role_dispatcher_request(ROLE_FW_UPLOAD, ROLE_SOURCE_BLE);
            ble_protocol_send_cmd(ACK_PROGRAM_WRITE_PHONE_ESP32, NULL, 0);
            return;
        case CMD_PROGRAM_INTERFACE_RESET_PHONE_ESP32:
            ESP_LOGI(TAG, "OTA update interface reset");
            ble_protocol_send_cmd(ACK_PROGRAM_INTERFACE_RESET_PHONE_ESP32, NULL, 0);
            return;
        case CMD_PROGRAM_STOP_PHONE_ESP32:
            ESP_LOGI(TAG, "OTA update stop");
            ble_protocol_send_cmd(ACK_PROGRAM_STOP_PHONE_ESP32, NULL, 0);
            return;
        case CMD_PROGRAM_START_PHONE_ESP32:
            ESP_LOGI(TAG, "OTA update start");
            if (payload && payload_len > 0) {
                char json_buf[256];
                size_t n = (payload_len >= sizeof(json_buf)) ? sizeof(json_buf) - 1 : payload_len;
                memcpy(json_buf, payload, n);
                json_buf[n] = '\0';
                char file_name[64];
                char file_type[32];
                bool ok_name = json_get_string(json_buf, "file_name", file_name, sizeof(file_name));
                bool ok_type = json_get_string(json_buf, "file_type", file_type, sizeof(file_type));
                if (ok_name && ok_type) {
                    bool started = false;
                    esp_err_t run_ret = fw_upload_run_from_sdmmc(file_name, file_type, &started);
                    if (run_ret == ESP_OK) {
                        char json[96];
                        snprintf(json, sizeof(json),
                                 "{\"result\":0,\"started\":%d}", started ? 1 : 0);
                        ble_send_json_ack(ACK_PROGRAM_START_PHONE_ESP32, json);
                    } else {
                        ble_send_json_ack(ACK_PROGRAM_START_PHONE_ESP32,
                                          "{\"result\":-2,\"reason\":\"run_fail\"}");
                    }
                    return;
                }
                ble_send_json_ack(ACK_PROGRAM_START_PHONE_ESP32, "{\"result\":-1,\"reason\":\"bad_json\"}");
                return;
            } else {
                ble_protocol_send_cmd(ACK_PROGRAM_START_PHONE_ESP32, NULL, 0);
            }
            role_dispatcher_request(ROLE_FW_UPLOAD, ROLE_SOURCE_BLE);
            return;
        case CMD_UPDATE_TYPE_PHONE_ESP32:
            ESP_LOGI(TAG, "OTA update type len=%u", (unsigned)payload_len);
            ble_set_last_fw_type(payload, payload_len);
            if (s_last_fw_type[0] != '\0') {
                fw_ble_legacy_set_type(s_last_fw_type);
                char name_buf[64];
                ble_build_default_ota_name(name_buf, sizeof(name_buf));
                fw_ble_legacy_set_file_name(name_buf);
            }
            ble_protocol_send_cmd(ACK_PROGRAM_STATUS_PHONE_ESP32, payload, payload_len);
            return;
        case CMD_PROGRAM_PROGRESS_MSG:
            ESP_LOGI(TAG, "OTA progress len=%u", (unsigned)payload_len);
            ble_protocol_send_cmd(ACK_PROGRAM_PROGRESS_MSG_ESP32, payload, payload_len);
            return;
        case CMD_SDMMC_PROGRAM_ENTER:
            ESP_LOGI(TAG, "SDMMC program enter");
            role_dispatcher_request(ROLE_STORAGE_UPLOAD, ROLE_SOURCE_BLE);
            if (payload && payload_len > 0) {
                char json_buf[256];
                size_t n = (payload_len >= sizeof(json_buf)) ? sizeof(json_buf) - 1 : payload_len;
                memcpy(json_buf, payload, n);
                json_buf[n] = '\0';
                ESP_LOGI(TAG, "SDMMC enter payload: %s", json_buf);

                char file_name[64];
                char file_type[32];
                bool ok_name = json_get_string(json_buf, "file_name", file_name, sizeof(file_name));
                bool ok_type = json_get_string(json_buf, "file_type", file_type, sizeof(file_type));
                if (ok_name && ok_type) {
                    char path[256];
                    snprintf(path, sizeof(path), "%s/%s.%s",
                             sdmmc_get_mount(), file_name, file_type);
                    struct stat st;
                    if (stat(path, &st) == 0) {
                        /* SDMMC에 파일이 이미 있는 경우:
                         *  - 바로 fw_upload_run_from_sdmmc 를 호출해서 업데이트를 시작한다.
                         *  - 실행 결과(started 여부 포함)를 BLE ACK로 돌려준다.
                         *  이렇게 하면 Wi-Fi 경로와 동일하게 fw_update_worker + UART_ISP 가 동작한다. */
                        bool started = false;
                        esp_err_t run_ret = fw_upload_run_from_sdmmc(file_name, file_type, &started);
                        if (run_ret == ESP_OK) {
                            char ack_json[96];
                            snprintf(ack_json, sizeof(ack_json),
                                     "{\"result\":0,\"started\":%d}", started ? 1 : 0);
                            ble_send_json_ack(ACK_SDMMC_PROGRAM_ENTER, ack_json);
                        } else {
                            ble_send_json_ack(ACK_SDMMC_PROGRAM_ENTER,
                                              "{\"result\":-3,\"reason\":\"run_fail\"}");
                        }
                        return;
                    }
                    /* 파일이 없으면 이전과 같이 not_found 반환 */
                    ble_send_json_ack(ACK_SDMMC_PROGRAM_ENTER, "{\"result\":-2,\"reason\":\"not_found\"}");
                    return;
                }
            }
            ble_send_json_ack(ACK_SDMMC_PROGRAM_ENTER, "{\"result\":-1,\"reason\":\"bad_json\"}");
            return;
        case CMD_SDMMC_PROGRAM_LIST:
            sdmmc_init();
            ble_send_sdmmc_program_list(ACK_SDMMC_PROGRAM_LIST);
            return;
        case CMD_FAT_FILE_LIST:
            spiflash_fs_init();
            ble_send_file_list(ACK_FAT_FILE_LIST, spiflash_fs_get_mount());
            return;
        case CMD_SDMMC_FORMAT:
            if (sdmmc_init() == ESP_OK && sdmmc_is_ready()) {
                esp_err_t ret = esp_vfs_fat_sdcard_format(sdmmc_get_mount(), sdmmc_get_card());
                ble_send_json_ack(ACK_SDMMC_FORMAT, ret == ESP_OK ? "{\"result\":0}" : "{\"result\":-1}");
            } else {
                ble_send_json_ack(ACK_SDMMC_FORMAT, "{\"result\":-1,\"reason\":\"not_ready\"}");
            }
            return;
        case CMD_FAT_FORMAT:
            if (spiflash_fs_init() == ESP_OK && spiflash_fs_is_ready()) {
                esp_err_t ret = esp_vfs_fat_spiflash_format_rw_wl(
                    spiflash_fs_get_mount(), STORAGE_LABEL_SPIFLASH);
                ble_send_json_ack(ACK_FAT_FORMAT, ret == ESP_OK ? "{\"result\":0}" : "{\"result\":-1}");
            } else {
                ble_send_json_ack(ACK_FAT_FORMAT, "{\"result\":-1,\"reason\":\"not_ready\"}");
            }
            return;
        case CMD_SDMMC_DELETE_FILE:
            if (payload && payload_len > 0) {
                char del_buf[256];
                size_t n = (payload_len >= sizeof(del_buf)) ? sizeof(del_buf) - 1 : payload_len;
                memcpy(del_buf, payload, n);
                del_buf[n] = '\0';
                ESP_LOGI(TAG, "SDMMC delete payload: %s", del_buf);
                char file_name[64];
                char file_type[32];
                if (json_get_string(del_buf, "file_name", file_name, sizeof(file_name)) &&
                    json_get_string(del_buf, "file_type", file_type, sizeof(file_type))) {
                    if (!sdmmc_is_valid_type(file_type)) {
                        ble_send_json_ack(ACK_SDMMC_DELETE_FILE,
                                          "{\"result\":-2,\"reason\":\"invalid_type\"}");
                        return;
                    }
                    char path[256];
                    snprintf(path, sizeof(path), "%s/%s.%s",
                             sdmmc_get_mount(), file_name, file_type);
                    int rc = remove(path);
                    ble_send_json_ack(ACK_SDMMC_DELETE_FILE, rc == 0 ? "{\"result\":0}" : "{\"result\":-1}");
                    return;
                }
            }
            ble_send_json_ack(ACK_SDMMC_DELETE_FILE, "{\"result\":-1,\"reason\":\"bad_json\"}");
            return;
        case CMD_FAT_DELETE_FILE:
            if (payload && payload_len > 0) {
                char del_buf[256];
                size_t n = (payload_len >= sizeof(del_buf)) ? sizeof(del_buf) - 1 : payload_len;
                memcpy(del_buf, payload, n);
                del_buf[n] = '\0';
                ESP_LOGI(TAG, "FAT delete payload: %s", del_buf);
                char file_name[64];
                char file_type[32];
                if (json_get_string(del_buf, "file_name", file_name, sizeof(file_name)) &&
                    json_get_string(del_buf, "file_type", file_type, sizeof(file_type))) {
                    char path[256];
                    snprintf(path, sizeof(path), "%s/%s.%s",
                             spiflash_fs_get_mount(), file_name, file_type);
                    int rc = remove(path);
                    ble_send_json_ack(ACK_FAT_DELETE_FILE, rc == 0 ? "{\"result\":0}" : "{\"result\":-1}");
                    return;
                }
            }
            ble_send_json_ack(ACK_FAT_DELETE_FILE, "{\"result\":-1,\"reason\":\"bad_json\"}");
            return;
        default:
            break;
    }

    role_t role = ble_cmd_to_role(cmd);
    ESP_LOGI(TAG, "cmd=0x%04x (unclassified) -> role=%d", cmd, role);
    role_dispatcher_request(role, ROLE_SOURCE_BLE);
}




