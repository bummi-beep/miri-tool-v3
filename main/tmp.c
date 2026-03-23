/**
 * 임시: IO47 버튼 동작 확인용.
 * 버튼 연결: IO47 -- [버튼] -- GND (풀업으로 눌렀을 때 LOW)
 * 빌드에 포함하려면 CMakeLists.txt 에 "tmp.c" 추가 후 app_main 에 tmp_button_test_start() 호출.
 */

#include "tmp.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "tmp_btn";

#define BTN_GPIO      47
#define BTN_DEBOUNCE_MS  80
#define POLL_MS       20

static void tmp_button_task(void *arg)
{
    bool was_released = true;  /* 이전에 버튼이 놓여 있었는지 */
    int press_count = 0;

    ESP_LOGI(TAG, "button poll task started (GPIO%d, pull-up, press=LOW)", BTN_GPIO);
    for (;;) {
        int level = gpio_get_level(BTN_GPIO);
        bool pressed = (level == 0);

        if (pressed && was_released) {
            press_count++;
            ESP_LOGI(TAG, "BUTTON PRESSED (#%d)", press_count);
            was_released = false;
            /* 디바운스: 눌린 상태가 끝날 때까지 대기 */
            vTaskDelay(pdMS_TO_TICKS(BTN_DEBOUNCE_MS));
            while (gpio_get_level(BTN_GPIO) == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            vTaskDelay(pdMS_TO_TICKS(BTN_DEBOUNCE_MS));
        }
        if (!pressed) {
            was_released = true;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

void tmp_button_test_start(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BTN_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(ret));
        return;
    }
    xTaskCreate(tmp_button_task, "tmp_btn", 4096, NULL, 5, NULL);
}
