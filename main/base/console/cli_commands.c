#include "cli_commands.h"

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <esp_vfs_fat.h>
#include <dirent.h>
#include <sys/stat.h>

#include "base/console/cli_router.h"
#include "base/console/cli_task.h"
#include "base/console/cli_menu.h"
#include "base/console/cli_hht_view.h"
#include "base/console/cli_ota_view.h"
#include "config/version.h"
#include "protocol/ble_cmd_parser.h"
#include "core/esp32_update/esp32_update.h"
#include "base/sdmmc/sdmmc_init.h"
#include "base/spiflash_fs/spiflash_fs.h"
#include "config/storage_paths.h"
#include "core/hht/hht_keys.h"
#include "core/hht/hht_display.h"

static const char *TAG = "cli_cmd";

static void cmd_help(const char *args);
static void cmd_back(const char *args);
static void cmd_main(const char *args);
static void cmd_menu_a(const char *args);
static void cmd_menu_b(const char *args);
static void cmd_menu_c(const char *args);
static void cmd_menu_d(const char *args);
static void cmd_menu_b1(const char *args);
static void cmd_menu_b2(const char *args);
static void cmd_menu_c1(const char *args);
static void cmd_menu_c2(const char *args);
static void cmd_reboot(const char *args);
static void cmd_version(const char *args);
static void cmd_role_hht(const char *args);
static void cmd_role_fw(const char *args);
static void cmd_role_storage(const char *args);
static void cmd_role_esp32(const char *args);
static void cmd_role_diag(const char *args);
static void cmd_role_manual(const char *args);
static void cmd_mac(const char *args);
static void cmd_blecmd(const char *args);
static void cmd_blename(const char *args);
static void cmd_hhtkey(const char *args);
static void cmd_hhtraw(const char *args);
static void cmd_sdls(const char *args);
static void cmd_sdformat(const char *args);
static void cmd_sddel(const char *args);
static void cmd_sdrun(const char *args);
 
static void cmd_flashls(const char *args);
static void cmd_flashformat(const char *args);
static void cmd_flashdel(const char *args);
static void cmd_stm32(const char *args);

static TaskHandle_t s_sdformat_task = NULL;
static bool s_sdformat_running = false;

static const cli_command_t g_cli_cmds[] = {
    {"HELP", "Show available commands", cmd_help},
    {"MENU", "Show available commands", cmd_help},
    {"?", "Alias for HELP", cmd_help},
    {"MAIN", "Go to main menu", cmd_main},
    {"BACK", "Go to main menu", cmd_back},
    {"UP", "Go to main menu", cmd_back},
    {"A", "Main menu: System/Info", cmd_menu_a},
    {"B", "Main menu: Update", cmd_menu_b},
    {"C", "Main menu: HHT", cmd_menu_c},
    {"D", "Main menu: Storage/File", cmd_menu_d},
    {"B1", "Sub menu: OTA firmware update", cmd_menu_b1},
    {"B2", "Sub menu: Target firmware update", cmd_menu_b2},
    {"C1", "Sub menu: HHT WB100", cmd_menu_c1},
    {"C2", "Sub menu: HHT WB3000", cmd_menu_c2},
    {"REBOOT", "Reboot ESP32", cmd_reboot},
    {"VER", "Show version info", cmd_version},
    {"VERSION", "Show version info", cmd_version},
    {"MAC", "Show BT MAC", cmd_mac},
    {"HHT", "Start HHT role", cmd_role_hht},
    {"FW", "Start firmware update role", cmd_role_fw},
    {"STORAGE", "Start storage upload role", cmd_role_storage},
    {"ESP32", "Start OTA firmware update mode", cmd_role_esp32},
    {"DIAG", "Start diagnosis role", cmd_role_diag},
    {"MANUAL", "Start manual control role", cmd_role_manual},
    {"BLECMD", "Send BLE cmd by value (e.g., BLECMD 0x0100)", cmd_blecmd},
    {"BLENAME", "Set BLE name (e.g., BLENAME MIRI-1234)", cmd_blename},
    {"HHTKEY", "Send BLE HHT key code (e.g., HHTKEY 0x02)", cmd_hhtkey},
    {"HHTRAW", "Toggle HHT raw UART dump (HHTRAW 0/1)", cmd_hhtraw},
    {"SDLS", "List SDMMC program files (SDLS ALL for all files)", cmd_sdls},
    {"SDFORMAT", "Format SDMMC FATFS", cmd_sdformat},
    {"SDDEL", "Delete SDMMC file (SDDEL <name> <type>)", cmd_sddel},
    {"SDRUN", "Run SDMMC program (SDRUN <name36> <type>)", cmd_sdrun},
    {"FLASHLS", "List SPI flash files (/c)", cmd_flashls},
    {"FLASHFORMAT", "Format SPI flash FATFS (/c)", cmd_flashformat},
    {"FLASHDEL", "Delete SPI flash file (FLASHDEL <name>)", cmd_flashdel},
    {"STM32", "Run STM32 (S01) update from SDMMC", cmd_stm32},
};

const cli_command_t *cli_get_commands(size_t *count) {
    if (count) {
        *count = sizeof(g_cli_cmds) / sizeof(g_cli_cmds[0]);
    }
    return g_cli_cmds;
}

static void cmd_help(const char *args) {
    size_t count = 0;
    const cli_command_t *cmds = cli_get_commands(&count);
    ESP_LOGI(TAG, "CLI commands:");
    for (size_t i = 0; i < count; i++) {
        ESP_LOGI(TAG, "  %02d) %-8s - %s", (int)(i + 1), cmds[i].name, cmds[i].help);
    }
    ESP_LOGI(TAG, "  Type the command name or its number.");
}

static void cmd_back(const char *args) {
    cli_menu_show_main();
}

static void cmd_main(const char *args) {
    cli_menu_show_main();
}

static void cmd_menu_a(const char *args) {
    (void)args;
    cli_menu_set(CLI_MENU_A);
    cli_menu_render();
}

static void cmd_menu_b(const char *args) {
    (void)args;
    cli_menu_set(CLI_MENU_B);
    cli_menu_render();
}

static void cmd_menu_c(const char *args) {
    (void)args;
    cli_menu_set(CLI_MENU_C);
    cli_menu_render();
}

static void cmd_menu_d(const char *args) {
    (void)args;
    cli_menu_set(CLI_MENU_D);
    cli_menu_render();
}

static void cmd_menu_b1(const char *args) {
    if (!esp32_update_is_ready()) {
        ESP_LOGI(TAG, "OTA file not set. Use SDRUN <name36> <type> first.");
        cli_menu_show_main();
        return;
    }
    cmd_role_esp32(args);
}

static void cmd_menu_b2(const char *args) {
    cmd_role_fw(args);
}

static void cmd_menu_c1(const char *args) {
    ble_cmd_handle(CMD_START_HHT_WB100);
    cli_task_set_hht_stop_cmd(CMD_STOP_HHT_WB100);
    cli_hht_view_set_boxed(true);
    cli_hht_view_set_active(true);
    cli_task_set_mode(CLI_MODE_HHT);
    ESP_LOGI(TAG, "HHT WB100 mode: UP/DOWN/ENTER/ESC, Q=exit");
}

static void cmd_menu_c2(const char *args) {
    ble_cmd_handle(CMD_START_HHT_3000);
    cli_task_set_hht_stop_cmd(CMD_STOP_HHT_3000);
    cli_hht_view_set_boxed(false);
    cli_hht_view_set_active(true);
    cli_task_set_mode(CLI_MODE_HHT);
    ESP_LOGI(TAG, "HHT WB3000 mode: UP/DOWN/ENTER/ESC, Q=exit");
}

static void cmd_reboot(const char *args) {
    ESP_LOGI(TAG, "rebooting...");
    esp_restart();
}

static void cmd_version(const char *args) {
    ESP_LOGI(TAG, "project : %s", PROJECT_NAME);
    ESP_LOGI(TAG, "app ver : %s", STRING_VER_MIRI_TOOL_DEVICE);
    ESP_LOGI(TAG, "build   : %s %s", __DATE__, __TIME__);
}

static void cmd_role_hht(const char *args) {
    ble_cmd_handle(CMD_START_HHT_100);
    cli_task_set_hht_stop_cmd(CMD_STOP_HHT_100);
    cli_hht_view_set_boxed(false);
    cli_hht_view_set_active(true);
    cli_task_set_mode(CLI_MODE_HHT);
    ESP_LOGI(TAG, "HHT mode: UP/DOWN/ENTER/ESC, Q=exit");
}

static void cmd_role_fw(const char *args) {
    ble_cmd_handle(CMD_FW_UPLOAD);
    cli_task_set_mode(CLI_MODE_FW);
    ESP_LOGI(TAG, "Firmware update mode: Q=exit");
}

static void cmd_role_storage(const char *args) {
    ble_cmd_handle(CMD_SDMMC_PROGRAM_ENTER);
}

static void cmd_role_esp32(const char *args) {
    if (args && args[0] != '\0') {
        char name[64];
        char type[16];
        if (sscanf(args, "%63s %15s", name, type) == 2) {
            char json[128];
            snprintf(json, sizeof(json),
                     "{\"file_name\":\"%s\",\"file_type\":\"%s\"}", name, type);
            ble_cmd_handle_with_payload(CMD_PROGRAM_START_PHONE_ESP32,
                                        (const uint8_t *)json,
                                        strlen(json));
        } else {
            ESP_LOGI(TAG, "usage: ESP32 <name36> <type>");
            return;
        }
    } else {
        ble_cmd_handle(CMD_PROGRAM_START_PHONE_ESP32);
    }
    cli_ota_view_set_active(true);
    cli_ota_view_set_status("Start");
    cli_task_set_mode(CLI_MODE_ESP32);
    ESP_LOGI(TAG, "ESP32 mode: Q=exit");
}

static void cmd_role_diag(const char *args) {
    ble_cmd_handle(CMD_MIRI_DIAGNOSIS_REQ);
}

static void cmd_role_manual(const char *args) {
    ble_cmd_handle(CMD_HANDLE_PHONE_ESP32);
}

static void cmd_blecmd(const char *args) {
    if (!args || args[0] == '\0') {
        ESP_LOGI(TAG, "usage: BLECMD <hex_or_dec>");
        return;
    }
    uint16_t cmd = (uint16_t)strtoul(args, NULL, 0);
    ble_cmd_handle(cmd);
}

static void cmd_mac(const char *args) {
    (void)args;
    ble_cmd_handle(CMD_MAC_BT_INFO);
}

static void cmd_blename(const char *args) {
    if (!args || args[0] == '\0') {
        ESP_LOGI(TAG, "usage: BLENAME <MIRI-xx>");
        return;
    }
    while (*args == ' ' || *args == '\t') {
        args++;
    }
    if (*args == '\0') {
        ESP_LOGI(TAG, "usage: BLENAME <MIRI-xx>");
        return;
    }
    size_t len = strlen(args);
    if (len != 9 || strncmp(args, "MIRI-", 5) != 0) {
        ESP_LOGI(TAG, "invalid format: use MIRI-XXXX (4 hex digits)");
        return;
    }
    for (size_t i = 0; i < 4; i++) {
        char c = args[5 + i];
        bool is_hex = (c >= '0' && c <= '9') ||
                      (c >= 'a' && c <= 'f') ||
                      (c >= 'A' && c <= 'F');
        if (!is_hex) {
            ESP_LOGI(TAG, "invalid format: use MIRI-XXXX (4 hex digits)");
            return;
        }
    }
    ble_cmd_handle_with_payload(CMD_ESP32_ADV_RENAME,
                                (const uint8_t *)args,
                                strlen(args));
}

static void cmd_hhtkey(const char *args) {
    if (!args || args[0] == '\0') {
        ESP_LOGI(TAG, "usage: HHTKEY <ble_key_code>");
        return;
    }
    uint8_t key = (uint8_t)strtoul(args, NULL, 0);
    ble_cmd_handle_with_payload(CMD_HHT_BTN, &key, 1);
}

static void cmd_hhtraw(const char *args) {
    if (!args || args[0] == '\0') {
        ESP_LOGI(TAG, "usage: HHTRAW <0|1>");
        return;
    }
    int enable = (int)strtoul(args, NULL, 0);
    hht_display_dump_raw(enable);
}

static const char *k_sdmmc_program_types[] = {
    "S00", "S01", "A00", "A01", "A02", "A03", "A04", "A05", "A06",
    "A07", "A08", "A09", "A10", "A11", "A12", "A13", "A14", "A15",
};

static bool sdmmc_is_valid_type_cli(const char *ext) {
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

static const char *get_filename_ext_cli(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) {
        return "";
    }
    return dot + 1;
}

static void get_filename_without_extension_cli(const char *name, char *out, size_t out_size) {
    const char *dot = strrchr(name, '.');
    size_t len = dot ? (size_t)(dot - name) : strlen(name);
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, name, len);
    out[len] = '\0';
}

static bool sdmmc_is_valid_program_file_cli(const char *name) {
    if (!name) {
        return false;
    }
    const char *ext = get_filename_ext_cli(name);
    if (!sdmmc_is_valid_type_cli(ext)) {
        return false;
    }
    return strlen(name) == 40;
}

static void cmd_sdls(const char *args) {
    bool list_all = false;
    if (args) {
        while (*args == ' ' || *args == '\t') {
            args++;
        }
        if (*args != '\0') {
            char buf[8] = {0};
            size_t len = 0;
            while (*args != '\0' && !isspace((unsigned char)*args) && len < sizeof(buf) - 1) {
                buf[len++] = (char)toupper((unsigned char)*args);
                args++;
            }
            if (strcmp(buf, "ALL") == 0) {
                list_all = true;
            }
        }
    }
    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        ESP_LOGI(TAG, "SDMMC not ready");
        return;
    }
    DIR *dir = opendir(sdmmc_get_mount());
    if (!dir) {
        ESP_LOGI(TAG, "Error opening SDMMC directory");
        return;
    }
    ESP_LOGI(TAG, "Idx  Name                                   Type");
    ESP_LOGI(TAG, "------------------------------------------------");
    int idx = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!list_all && !sdmmc_is_valid_program_file_cli(ent->d_name)) {
            continue;
        }
        char name_buf[128];
        const char *ext = get_filename_ext_cli(ent->d_name);
        get_filename_without_extension_cli(ent->d_name, name_buf, sizeof(name_buf));
        ESP_LOGI(TAG, "%-4d %-38s %s", idx, name_buf, ext);
        idx++;
    }
    closedir(dir);
}

static void sdformat_task(void *arg) {
    (void)arg;
    s_sdformat_running = true;
    vTaskDelay(pdMS_TO_TICKS(1));

    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        ESP_LOGI(TAG, "SDMMC not ready");
        s_sdformat_running = false;
        s_sdformat_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "SDMMC format: start");
    esp_err_t ret = esp_vfs_fat_sdcard_format(sdmmc_get_mount(), sdmmc_get_card());
    ESP_LOGI(TAG, "SDMMC format: %s", ret == ESP_OK ? "OK" : "FAIL");

    s_sdformat_running = false;
    s_sdformat_task = NULL;
    vTaskDelete(NULL);
}

static void cmd_sdformat(const char *args) {
    (void)args;
    if (s_sdformat_running) {
        ESP_LOGI(TAG, "SDMMC format already running");
        return;
    }
    BaseType_t ok = xTaskCreatePinnedToCore(
        sdformat_task,
        "sdformat_task",
        4096,
        NULL,
        5,
        &s_sdformat_task,
        1);
    if (ok != pdPASS) {
        ESP_LOGW(TAG, "SDMMC format task create failed");
        s_sdformat_task = NULL;
        s_sdformat_running = false;
        return;
    }
    ESP_LOGI(TAG, "SDMMC format: scheduled");
}

static void cmd_sddel(const char *args) {
    if (!args || args[0] == '\0') {
        ESP_LOGI(TAG, "usage: SDDEL <name> <type>");
        return;
    }
    char name[128];
    char type[16];
    if (sscanf(args, "%127s %15s", name, type) != 2) {
        ESP_LOGI(TAG, "usage: SDDEL <name> <type>");
        return;
    }
    if (!sdmmc_is_valid_type_cli(type)) {
        ESP_LOGI(TAG, "invalid type: %s", type);
        return;
    }
    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        ESP_LOGI(TAG, "SDMMC not ready");
        return;
    }
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.%s", sdmmc_get_mount(), name, type);
    int rc = remove(path);
    ESP_LOGI(TAG, "delete %s: %s", path, rc == 0 ? "OK" : "FAIL");
}

static void cmd_sdrun(const char *args) {
    if (!args || args[0] == '\0') {
        ESP_LOGI(TAG, "usage: SDRUN <name36> <type>");
        return;
    }
    char name[64];
    char type[16];
    if (sscanf(args, "%63s %15s", name, type) != 2) {
        ESP_LOGI(TAG, "usage: SDRUN <name36> <type>");
        return;
    }
    if (strlen(name) != 36) {
        ESP_LOGI(TAG, "invalid name length: need 36 chars");
        return;
    }
    if (!sdmmc_is_valid_type_cli(type)) {
        ESP_LOGI(TAG, "invalid type: %s", type);
        return;
    }
    char json[128];
    snprintf(json, sizeof(json),
             "{\"file_name\":\"%s\",\"file_type\":\"%s\"}", name, type);
    ble_cmd_handle_with_payload(CMD_SDMMC_PROGRAM_ENTER,
                                (const uint8_t *)json,
                                strlen(json));
}

static void cmd_flashls(const char *args) {
    (void)args;
    if (!spiflash_fs_is_ready()) {
        ESP_LOGI(TAG, "SPI flash FATFS not ready");
        return;
    }
    DIR *dir = opendir(BASE_FILESYSTEM_PATH_SPIFLASH "/");
    if (!dir) {
        ESP_LOGI(TAG, "Error opening SPI flash directory");
        return;
    }
    ESP_LOGI(TAG, "T        Size\t\tName");
    ESP_LOGI(TAG, "-----------------------------------");
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        struct stat st;
        char filename[PATH_MAX] = {'\0'};
        snprintf(filename, sizeof(filename), "%s/%s", BASE_FILESYSTEM_PATH_SPIFLASH, ent->d_name);
        if (stat(filename, &st) == 0) {
            ESP_LOGI(TAG, "%8ld Bytes\t%s", st.st_size, ent->d_name);
        }
    }
    closedir(dir);
}

static void cmd_flashformat(const char *args) {
    (void)args;
    ESP_LOGW(TAG, "Formatting SPI flash FATFS (/c) - config files will be removed");
    esp_err_t err = esp_vfs_fat_spiflash_format_rw_wl(BASE_FILESYSTEM_PATH_SPIFLASH, STORAGE_LABEL_SPIFLASH);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to format FATFS (%s)", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Success to format FATFS (%s)", esp_err_to_name(err));
}

static void cmd_flashdel(const char *args) {
    if (!args || args[0] == '\0') {
        ESP_LOGI(TAG, "Usage: FLASHDEL <name>");
        return;
    }
    if (!spiflash_fs_is_ready()) {
        ESP_LOGI(TAG, "SPI flash FATFS not ready");
        return;
    }
    char name[64];
    snprintf(name, sizeof(name), "%s", args);
    for (char *p = name; *p; p++) {
        if (*p == '\r' || *p == '\n') {
            *p = '\0';
            break;
        }
    }
    if (name[0] == '\0') {
        ESP_LOGI(TAG, "Usage: FLASHDEL <name>");
        return;
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", BASE_FILESYSTEM_PATH_SPIFLASH, name);
    if (remove(path) != 0) {
        ESP_LOGI(TAG, "Delete failed: %s", path);
        return;
    }
    ESP_LOGI(TAG, "Deleted: %s", path);
}

/* STM32F4 (S01 타입) 펌웨어를 SDMMC에서 찾아 실행.
 * 사용법:
 *   STM32             -> SDMMC 에서 첫 번째 S01 파일을 찾아 실행
 *   STM32 <name36>    -> 주어진 이름의 S01 파일을 실행
 */
static void cmd_stm32(const char *args) {
    if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
        ESP_LOGI(TAG, "SDMMC not ready");
        return;
    }

    char name[64] = {0};
    const char *type = "S01";

    if (args && args[0] != '\0') {
        /* 이름이 인자로 주어진 경우: STM32 <name36> */
        if (sscanf(args, "%63s", name) != 1) {
            ESP_LOGI(TAG, "usage: STM32 <name36>");
            return;
        }
        if (strlen(name) != 36) {
            ESP_LOGI(TAG, "invalid name length: need 36 chars");
            return;
        }
    } else {
        /* 인자가 없으면 SDMMC 루트에서 첫 번째 S01 프로그램 파일을 찾는다. */
        DIR *dir = opendir(sdmmc_get_mount());
        if (!dir) {
            ESP_LOGI(TAG, "Error opening SDMMC directory");
            return;
        }
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (!sdmmc_is_valid_program_file_cli(ent->d_name)) {
                continue;
            }
            const char *ext = get_filename_ext_cli(ent->d_name);
            if (strncmp(ext, "S01", 3) != 0) {
                continue;
            }
            get_filename_without_extension_cli(ent->d_name, name, sizeof(name));
            if (strlen(name) == 36) {
                break;
            }
            name[0] = '\0';
        }
        closedir(dir);
        if (name[0] == '\0') {
            ESP_LOGI(TAG, "no S01 program file found on SDMMC");
            return;
        }
    }

    char json[128];
    snprintf(json, sizeof(json),
             "{\"file_name\":\"%s\",\"file_type\":\"%s\"}", name, type);
    ble_cmd_handle_with_payload(CMD_SDMMC_PROGRAM_ENTER,
                                (const uint8_t *)json,
                                strlen(json));
}
