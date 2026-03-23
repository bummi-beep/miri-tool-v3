// SDMMC FATFS: 외장 SD 카드(슬롯)를 SDMMC 호스트로 마운트해 사용하는 저장소.
// 기본 마운트 경로는 BASE_FILESYSTEM_PATH_SDMMC(/sdcard)이며,
// SD 카드 기반 파일 목록/펌웨어 저장에 사용한다.
#include "sdmmc_init.h"

#include <esp_log.h>
#include <driver/sdmmc_host.h>
#include <esp_vfs_fat.h>

#include "config/pinmap.h"
#include "config/storage_paths.h"

static const char *TAG = "sdmmc";
static sdmmc_card_t *s_card = NULL;
static bool s_ready = false;

esp_err_t sdmmc_init(void) {
    if (s_ready) {
        return ESP_OK;
    }

    const pinmap_t *pins = pinmap_get();

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    slot_config.clk = pins->sd_clk;
    slot_config.cmd = pins->sd_cmd;
    slot_config.d0 = pins->sd_d0;
    slot_config.d1 = pins->sd_d1;
    slot_config.d2 = pins->sd_d2;
    slot_config.d3 = pins->sd_d3;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    ESP_LOGI(TAG, "Mounting SDMMC at %s", BASE_FILESYSTEM_PATH_SDMMC);
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(BASE_FILESYSTEM_PATH_SDMMC,
                                           &host,
                                           &slot_config,
                                           &mount_config,
                                           &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SDMMC mount failed: %s", esp_err_to_name(ret));
        s_card = NULL;
        s_ready = false;
        return ret;
    }

    s_ready = true;
    ESP_LOGI(TAG, "SDMMC mounted");
    return ESP_OK;
}

bool sdmmc_is_ready(void) {
    return s_ready;
}

sdmmc_card_t *sdmmc_get_card(void) {
    return s_card;
}

const char *sdmmc_get_mount(void) {
    return BASE_FILESYSTEM_PATH_SDMMC;
}

