#include "base/system/system_init.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "base/net/net_init.h"
#include "base/net/tcp/tcp_server.h"
#include "base/ble/ble_init.h"
#include "base/wifi/wifi_init.h"
#include "base/sdmmc/sdmmc_init.h"
#include "base/http/http_init.h"
#include "config/version.h"
#include "core/role_dispatcher.h"
#include "tasks/task_manager.h"
#include "tmp.h"

static const char *TAG = "app_main";

/* Yield between heavy inits so idle tasks can run and task WDT does not trigger. */
#define INIT_YIELD_MS 50

void app_main(void)
{
    system_init();
    version_log();
    vTaskDelay(pdMS_TO_TICKS(INIT_YIELD_MS));

    esp_err_t sd_ret = sdmmc_init();
    if (sd_ret == ESP_OK) {
        ESP_LOGI(TAG, "sdmmc_init: OK");
    } else {
        ESP_LOGW(TAG, "sdmmc_init: FAIL (%s)", esp_err_to_name(sd_ret));
    }
    vTaskDelay(pdMS_TO_TICKS(INIT_YIELD_MS));

    net_init();
    vTaskDelay(pdMS_TO_TICKS(INIT_YIELD_MS));

    ble_init();
    vTaskDelay(pdMS_TO_TICKS(INIT_YIELD_MS));
    wifi_init();
    vTaskDelay(pdMS_TO_TICKS(INIT_YIELD_MS));

    tcp_server_start();
    http_init();
    tasks_start_common();
    role_dispatcher_init();

    /* 임시: IO47 버튼 테스트 (확인 후 tmp.c 제거 및 이 호출 삭제) */
    tmp_button_test_start();
}

