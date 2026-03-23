// SPI Flash FATFS: ESP32 internal SPI Flash mounted as FATFS.
// Base mount path is BASE_FILESYSTEM_PATH_SPIFLASH (/c) for config/log files.
#include "spiflash_fs.h"

#include <esp_log.h>
#include <esp_mac.h>
#include <esp_vfs_fat.h>
#include <wear_levelling.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "config/storage_paths.h"

static const char *TAG = "fatfs";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static bool s_ready = false;
static char s_ble_name[32];

esp_err_t spiflash_fs_init(void) {
    if (s_ready) {
        return ESP_OK;
    }

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
    };

    ESP_LOGI(TAG, "Mounting FAT at %s", BASE_FILESYSTEM_PATH_SPIFLASH);
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(
        BASE_FILESYSTEM_PATH_SPIFLASH,
        STORAGE_LABEL_SPIFLASH,
        &mount_config,
        &s_wl_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FAT mount failed: %s", esp_err_to_name(err));
        return err;
    }

    s_ready = true;
    ESP_LOGI(TAG, "FAT mounted");
    return ESP_OK;
}

bool spiflash_fs_is_ready(void) {
    return s_ready;
}

const char *spiflash_fs_get_mount(void) {
    return BASE_FILESYSTEM_PATH_SPIFLASH;
}

static int spiflash_fs_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void spiflash_fs_make_default_ble_name(char *out, size_t out_len) {
    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_BT));
    snprintf(out, out_len, "MIRI-%02x%02x", mac[4], mac[5]);
}

const char *spiflash_fs_get_ble_device_name(void) {
    char pathbuf[128];
    char linebuf[64];

    spiflash_fs_make_default_ble_name(s_ble_name, sizeof(s_ble_name));
    if (!s_ready) {
        spiflash_fs_init();
    }

    snprintf(pathbuf, sizeof(pathbuf), "%s/%s", BASE_FILESYSTEM_PATH_SPIFLASH, "ble_setting.txt");
    if (!spiflash_fs_exists(pathbuf)) {
        FILE *f = fopen(pathbuf, "wt");
        if (f) {
            fprintf(f, "%s", s_ble_name);
            fclose(f);
        }
        return s_ble_name;
    }

    FILE *f = fopen(pathbuf, "r");
    if (!f) {
        esp_vfs_fat_spiflash_format_rw_wl(BASE_FILESYSTEM_PATH_SPIFLASH, STORAGE_LABEL_SPIFLASH);
        return s_ble_name;
    }

    if (!fgets(linebuf, sizeof(linebuf), f)) {
        fclose(f);
        return s_ble_name;
    }
    fclose(f);

    linebuf[strcspn(linebuf, "\r\n")] = '\0';
    if (strstr(linebuf, "MIRI-") == NULL) {
        esp_vfs_fat_spiflash_format_rw_wl(BASE_FILESYSTEM_PATH_SPIFLASH, STORAGE_LABEL_SPIFLASH);
        return s_ble_name;
    }

    snprintf(s_ble_name, sizeof(s_ble_name), "%.*s",
             (int)(sizeof(s_ble_name) - 1), linebuf);
    return s_ble_name;
}

esp_err_t spiflash_fs_set_ble_device_name(const char *name) {
    char pathbuf[128];

    if (!name || strstr(name, "MIRI-") == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ready) {
        spiflash_fs_init();
    }

    snprintf(pathbuf, sizeof(pathbuf), "%s/%s", BASE_FILESYSTEM_PATH_SPIFLASH, "ble_setting.txt");
    FILE *f = fopen(pathbuf, "wt");
    if (!f) {
        return ESP_FAIL;
    }
    fprintf(f, "%s", name);
    fclose(f);
    return ESP_OK;
}

bool spiflash_fs_apply_debug_mode(void) {
    char pathbuf[128];

    if (!s_ready) {
        spiflash_fs_init();
    }

    snprintf(pathbuf, sizeof(pathbuf), "%s/%s", BASE_FILESYSTEM_PATH_SPIFLASH, "debug.json");
    if (!spiflash_fs_exists(pathbuf)) {
        return false;
    }

    esp_log_level_set("*", ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "debug.json detected: log level set to DEBUG");
    return true;
}

