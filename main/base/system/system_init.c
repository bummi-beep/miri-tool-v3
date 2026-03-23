#include "system_init.h"
#include "event_init.h"
#include "log_init.h"
#include "nvs_init.h"
#include "base/runtime_config/runtime_config.h"
#include "base/console/console_init.h"
#include "base/spiflash_fs/spiflash_fs.h"
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include <driver/uart.h>

// 미사용 GPIO를 출력+풀다운으로 고정 (PHY/노이즈 안정화)
//#define UNUSED_GPIO_LIST  { 4, 5, 6, 7, 15, 16, 17, 18, 8, 9, 10, 11, 12, 13, 14, 21, 47, 48, 38, 39, 40, 41, 42, 2, 1 }  // 예시
//#define UNUSED_GPIO_LIST  { 6, 7, 15, 16, 17, 18, 9, 10, 11, 12, 2, 1 }  // 예시
#define UNUSED_GPIO_LIST  {10,11}  // ADC로 사용가능.
void init_unused_gpio(void)
{
    const int unused[] = UNUSED_GPIO_LIST;
    uint64_t pin_mask = 0;

    for (size_t i = 0; i < sizeof(unused)/sizeof(unused[0]); i++) {
        pin_mask |= (1ULL << unused[i]);
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,             //
        .pull_up_en = GPIO_PULLUP_DISABLE,    //
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // 0V 출력 유지
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);
}

void system_init(void) {
    /*
     * Boot baseline:
     * 1) Log configuration (default INFO)
     * 2) NVS initialization (erase-on-error retry)
     * 3) SPI Flash FATFS mount (/c)
     *    - ble_setting.txt: BLE name persistence
     *    - debug.json: optional debug-mode trigger
     * 4) Default event loop creation
     *
     * This follows the same order used in the existing fw_esp32 flow,
     * with storage mounted early for configuration files.
     */
    init_unused_gpio();
   // uart_set_pin(UART_NUM_1, 10, 9, 11, 12); /* ADC -> UART for phy */
    log_init();
    ESP_LOGI("mem", "boot heap=%u psram=%u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    nvs_init();
    spiflash_fs_init();
    spiflash_fs_apply_debug_mode();
    runtime_config_init();
    event_init();
    console_init();

}





