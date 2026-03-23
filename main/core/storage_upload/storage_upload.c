#include "storage_upload.h"
#include <esp_log.h>
#include "base/spiflash_fs/spiflash_fs.h"
#include "base/net/http/http_server.h"
#include "base/net/tcp/tcp_server.h"
#include "base/sdmmc/sdmmc_init.h"

void storage_upload_start(void) {
    sdmmc_init();
    spiflash_fs_init();
    http_server_start();
    tcp_server_start();
    ESP_LOGI("storage_upload", "storage_upload_start (placeholder)");
}


