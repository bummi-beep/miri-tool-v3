#include "hht_task.h"

#include <stdbool.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "core/hht/hht_main.h"
#include "core/hht/hht_wb100.h"
#include "core/hht/hht_3000.h"

static const char *TAG = "hht_task";
static TaskHandle_t s_hht_task = NULL;
static volatile bool s_stop_requested = false;
static SemaphoreHandle_t s_exit_done = NULL;

#define HHT_TASK_EXIT_TIMEOUT_MS 2000

static void hht_task(void *arg) {
    ESP_LOGI(TAG, "start");
    for (;;) {
        if (s_stop_requested) {
            ESP_LOGI(TAG, "stop requested, exiting");
            xSemaphoreGive(s_exit_done);
            vTaskDelete(NULL);
            return;
        }
        switch (hht_get_type()) {
            case HHT_TYPE_WB100:
                hht_wb100_poll();
                break;
            case HHT_TYPE_3000:
                hht_3000_poll();
                break;
            case HHT_TYPE_100:
                hht_wb100_poll();
                break;
            default:
                hht_wb100_poll();
                break;
        }
        /* 2.08 WB100 task cadence: keep main HHT loop around 20ms. */
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void hht_task_start(void) {
    if (s_hht_task) {
        return;
    }
    if (s_exit_done == NULL) {
        s_exit_done = xSemaphoreCreateBinary();
        if (s_exit_done == NULL) {
            ESP_LOGE(TAG, "failed to create exit semaphore");
            return;
        }
    }
    s_stop_requested = false;
    xTaskCreate(hht_task, "hht_task", 4096, NULL, 5, &s_hht_task);
}

void hht_task_stop(void) {
    if (!s_hht_task) {
        return;
    }
    s_stop_requested = true;
    BaseType_t taken = xSemaphoreTake(s_exit_done, pdMS_TO_TICKS(HHT_TASK_EXIT_TIMEOUT_MS));
    if (taken != pdTRUE) {
        ESP_LOGW(TAG, "task did not exit in time, forcing delete");
        vTaskDelete(s_hht_task);
    }
    s_hht_task = NULL;
    s_stop_requested = false;
}
