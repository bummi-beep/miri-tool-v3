/************************************************************************
 * @Filename    : swd.c
 * @Created on  : 2024-01-29
 * @Author      : insoo.kum@hdel.co.kr
 * @Description : 
 * @Copyright(s): HYUNDAI ELEVATOR CO,.LTD
 *
 * @Revision History
 *
 *
 */


//_____ I N C L U D E S ________________________________________________________

#include <rom/ets_sys.h>
#include "FlashAlgo_define.h"
#include "esp_intr_alloc.h"
#include "driver/uart.h"
#include "in_fatfs.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_intr_alloc.h"
#include "soc/uart_reg.h"
#include "soc/uart_struct.h"
#include "esp_task_wdt.h"
#include "rom/gpio.h"

/* tmp *
 * SWD TEST
 */

#include "general.h"
#include "target.h"
#include "target_internal.h"
#include "global_variable.h"
#include "operation_firmware_update.h"
#include "ble_packet_send.h"
#include "lpc_isp.h"
#include "lpcisp.h"
#include "report.h"
#include "isp_uart.h"
#include "in_uart.h"
#include "esp_heap_caps.h"
//_____ M A C R O S ____________________________________________________________
//_____ D E F I N I T I O N S __________________________________________________
#define    TIMEOUT_LPC_BOOTLAODER        (500 / portTICK_PERIOD_MS)
#define    TIMEOUT_LPC_ERASEFLASH        (5000 / portTICK_PERIOD_MS)


enum lpc_isp_mode_t {
    lpc_isp_idle = 0,
    lpc_isp_enter_mode,
    lpc_isp_eraseFlash,
    lpc_isp_writeFlash,
    lpc_isp_End,
};

struct lpc_isp_var_t {
    uint32_t file_size;
    uint8_t *file_buffer;
    uint32_t writing_offset;
    target_s *target_swd;
};

//_____ D E C L A R A T I O N S ________________________________________________
uint8_t isp_mode = 0;
struct lpc_isp_var_t lpc_isp_var;
enum lpc_isp_mode_t lpc_isp_mode;
lpcispcfg_t lpc_isp_cfg;
partinfo_t lpc_isp_partinfo;
// stand_alone_prog_mode
extern int standalone_prog_end;
extern int standalone_prog_fail;


//_____ F U N C T I O N S ______________________________________________________
static void _isp_mode_set(void);


void vTaskLpcIsp(void *pvParameters) {
    lpc_isp_mode = lpc_isp_idle;
    int ret      = 0;
    int cnt      = 0;
    ESP_LOGI(TAG_LPCISP, "vTaskLpcIsp\r\n");

    while (1) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
        switch (lpc_isp_mode) {
            case lpc_isp_idle:
            default:
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;
            case lpc_isp_enter_mode:
                ble_packet_send_ui_using_argv(C1_MIRITOOL_LOG_UPDATE, C2_ONGOING, C3_UPDATE_FW_SWD_STM32_ATTACHED_START, 0x00, 0x00);
                ble_packet_send_update_status_with_step(5, 2, "DEVICE ENTER MODE");
                ret = lpc_isp_entermode(); //(LPC_UART,&lpc_isp_cfg,0,0,1,0,0,PIN_NONE,PIN_NONE,&lpc_isp_partinfo);
                if (ret == 0) {
                    ESP_LOGI(TAG_MIRI, "detection %s", lpc_isp_partinfo.name);
                    if (strncmp(lpc_isp_partinfo.name, "LPC4357", 7) == 0) {
                        lpc_isp_mode               = lpc_isp_eraseFlash;
                        lpc_isp_var.writing_offset = 0x1A000000; //0x1a000000;
                        ble_packet_send_ui_using_argv(C1_MIRITOOL_LOG_UPDATE, C2_ONGOING, C3_UPDATE_FW_SWD_STM32_ATTACHED_SUCCESS, 0x00, 0x00);
                        ble_packet_send_update_status_with_step(5, 2, "DEVICE ATTACHED");
                        break;
                    }
                    else if (strncmp(lpc_isp_partinfo.name, "LPC1778", 6) == 0) {
                        // HPI
                        lpc_isp_mode               = lpc_isp_eraseFlash;
                        lpc_isp_var.writing_offset = 0x00000000;
                        ble_packet_send_ui_using_argv(C1_MIRITOOL_LOG_UPDATE, C2_ONGOING, C3_UPDATE_FW_SWD_STM32_ATTACHED_SUCCESS, 0x00, 0x00);
                        ble_packet_send_update_status_with_step(5, 2, "DEVICE ATTACHED");
                        break;
                    }
                    else {
                        ESP_LOGI(TAG_MIRI, "unknown device %s", lpc_isp_partinfo.name);
                        lpc_isp_mode = lpc_isp_End;
                        standalone_prog_fail = 1;
                    }
                }
                else {
                    ble_packet_send_ui_using_argv(C1_MIRITOOL_LOG_UPDATE, C2_ERROR, C3_UPDATE_FW_SWD_STM32_ATTACHED_ERROR_NOT_FOUND, 0x00, 0x00);
                    ble_packet_send_update_status_with_step(5, 5, "DEVICE NOT FOUND");
                    lpc_isp_mode = lpc_isp_End;
                    standalone_prog_fail = 1;
                }
                break;
            case lpc_isp_eraseFlash:

                ble_packet_send_ui_using_argv(C1_MIRITOOL_LOG_UPDATE, C2_ONGOING, C3_UPDATE_FW_SWD_STM32_ERASE_START, 0x00, 0x00);
                ble_packet_send_update_status_with_step(5, 3, "ERASE FLASH");
                ret = lpc_isp_eraseflash();
//                ble_packet_send_update_percentage_with_func(50, __func__, __LINE__, "erase"); // ble_update_per(tms_var.count_erase_flash + 1 * 100 / MAX_INDEX_FLASH);

                if (ret != 0) {
                    ble_packet_send_ui_using_argv(C1_MIRITOOL_LOG_UPDATE, C2_ERROR, C3_UPDATE_FW_SWD_STM32_ERASE_ERROR, cnt, ret);
                    ble_packet_send_update_status_with_step(5, 5, "ERASE FLASH ERROR");
                    lpc_isp_mode = lpc_isp_End;
                    standalone_prog_fail = 1;
                    break;
                }
                ble_packet_send_update_status_with_step(5, 4, "ERASE FLASH SUCCESS");
                ble_packet_send_ui_using_argv(C1_MIRITOOL_LOG_UPDATE, C2_ONGOING, C3_UPDATE_FW_SWD_STM32_ERASE_SUCCESS, cnt, ret);
                lpc_isp_mode = lpc_isp_writeFlash;
                break;
            case lpc_isp_writeFlash:
                cnt = 0;
                ble_packet_send_ui_using_argv(C1_MIRITOOL_LOG_UPDATE, C2_ONGOING, C3_UPDATE_FW_SWD_STM32_FLASHING_START, 0x00, 0x00);
                ble_packet_send_update_status_with_step(5, 4, "FLASHING START");
//                ret = target_flash_write(lpc_isp_var.target_swd, lpc_isp_var.writing_offset, lpc_isp_var.file_buffer, lpc_isp_var.file_size);
                ret = lpc_isp_writeflash();
                ble_packet_send_update_percentage_with_func(100, __func__, __LINE__, "Write"); // ble_update_per(tms_var.count_erase_flash + 1 * 100 / MAX_INDEX_FLASH);

                ESP_LOGI(TAG_MIRI, "target flash write %d %d ", cnt, ret);

                if (ret != 0) {
                    lpc_isp_mode = lpc_isp_End;
                    ble_packet_send_ui_using_argv(C1_MIRITOOL_LOG_UPDATE, C2_ERROR, C3_UPDATE_FW_SWD_STM32_FLASHING_ERROR, cnt, ret);
                    ble_packet_send_update_status_with_step(5, 5, "FLASHING ERROR");
                    standalone_prog_fail = 1;
                    break;
                }

                ble_packet_send_ui_using_argv(C1_MIRITOOL_LOG_UPDATE, C2_ONGOING, C3_UPDATE_FW_SWD_STM32_FLASHING_SUCCESS, 0x00, 0x00);
            // 성공 로깅
                lpc_isp_mode = lpc_isp_End;
                ble_packet_send_ui_using_argv(C1_MIRITOOL_LOG_UPDATE, C2_SUCCESS, C3_UPDATE_FW_SWD_STM32_SUCCESS, 0x00, 0x00);
                ble_packet_send_update_status_with_step(5, 5, "Completed");
                standalone_prog_end = 1;
                break;
            case lpc_isp_End:
                lpc_isp_end();
                break;
        }
    }
}

void lpc_isp_Download(uint8_t *pbuff, uint32_t len, uint8_t binary_type) {
    lpc_isp_init(binary_type);
    lpc_isp_var.file_buffer = pbuff;
    lpc_isp_var.file_size   = len;
    lpc_isp_var.target_swd  = NULL;
    lpc_isp_mode            = lpc_isp_enter_mode;
}

void lpc_isp_init(uint8_t binary_type) {
    ESP_LOGI(TAG_LPCISP, "lpc_isp_init");
    // ESP_LOGI(TAG_LPCISP, "ISP MODE");
    // ESP_LOGI(TAG_LPCISP, "----GPIO RESET");
    // ESP_LOGI(TAG_LPCISP, "----UART INIT");
    inuart_delete_driver(LPC_UART);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    uint32_t baudrate = 57600;
    if (binary_type == BINARY_TYPE_FW_DDC_HRTS_EMON_ONE || binary_type == BINARY_TYPE_FW_DDC_HRTS_EMON_WBVF) {
        baudrate = 115200;
    }
    ESP_LOGI(TAG_LPCISP, "baudrate %ld ", baudrate);
    inuart_init(LPC_UART, baudrate, LPC_GPIO_UART_TX, LPC_GPIO_UART_RX, UART_PARITY_DISABLE);

    _isp_mode_set();
/*
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_direction(LPC_GPIO_UART_CTS, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(LPC_GPIO_UART_CTS, 0);
    gpio_set_direction(LPC_GPIO_UART_RTS, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(LPC_GPIO_UART_RTS, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
*/
#if 0
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    do
    {
        LPCISP_SERIAL_WriteBytes(LPC_UART,(const unsigned char *)message,len);
        uart_write_bytes(LPC_UART,message,len);
        ReportString(0,"%d %d %s", count,len,message);
        printf("%d %d %s", count,len,message);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        count++;
    }while(1);
#endif


    memset(&lpc_isp_cfg, 0x00, sizeof(lpcispcfg_t));
    memset(&lpc_isp_partinfo, 0x00, sizeof(partinfo_t));
}

void lpc_isp_cleanup(void) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    inuart_delete_driver(LPC_UART);

    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_direction(LPC_GPIO_UART_RX, GPIO_MODE_DEF_INPUT);
    gpio_set_level(LPC_GPIO_UART_RX, 1);

    gpio_set_direction(LPC_GPIO_UART_RTS, GPIO_MODE_DEF_INPUT);
    gpio_set_level(LPC_GPIO_UART_RTS, 1);
}

int lpc_isp_entermode(void) {
    int retval;
    ESP_LOGI(TAG_LPCISP, "lpc_isp_entermode");
    retval = LPCISP_Sync(LPC_UART, &lpc_isp_cfg, 0, 0, 100, 0, 0, PIN_NONE, PIN_NONE, &lpc_isp_partinfo);
    ESP_LOGI(TAG_LPCISP, "entermode result %d ", retval);
    return retval;
}

int lpc_isp_eraseflash(void) {
    int retval = 0;
    ESP_LOGI(TAG_LPCISP, "lpc_isp_eraseflash %d", lpc_isp_partinfo.numSectors);
    if (lpc_isp_partinfo.numSectors) {
#if 0
           int  startSector,endSector;
           startSector=0;
           endSector=lpc_isp_partinfo.numSectors-1;

           printf("start %d end %d ", startSector,endSector);
           retval = LPCISP_Erase(LPC_UART,&lpc_isp_cfg,startSector,endSector,0,&lpc_isp_partinfo);   // @@@ only support bank zero for now
#elif 0
           int startSector,endSector, bank;
           startSector = 0;
           endSector = 0;
           bank = 0;
           for(int i=0; i < lpc_isp_partinfo.numSectors; i++)
           {
               if(lpc_isp_partinfo.sectorMap[i].bank != bank)
               {
                   retval = LPCISP_Erase(LPC_UART,&lpc_isp_cfg,startSector,endSector,bank,&lpc_isp_partinfo);   // @@@ only support bank zero for now
                   startSector = i;
                   bank = lpc_isp_partinfo.sectorMap[i].bank;
               }
               endSector = i;
//               retval = LPCISP_Erase(LPC_UART,&lpc_isp_cfg,i,i+1,lpc_isp_partinfo.sectorMap[i].bank,&lpc_isp_partinfo);   // @@@ only support bank zero for now

//               printf("addr %d bank %d ", lpc_isp_partinfo.sectorMap[i].base,lpc_isp_partinfo.sectorMap[i].bank);
           }
           retval = LPCISP_Erase(LPC_UART,&lpc_isp_cfg,startSector,endSector,bank,&lpc_isp_partinfo);   // @@@ only support bank zero for now
#else
        int startSector, endSector, bank;
        startSector = 0;
        endSector   = 0;
        bank        = 0;
        for (int i = 0; i < lpc_isp_partinfo.numSectors; i++) {
            if (bank != lpc_isp_partinfo.sectorMap[i].bank) {
                startSector = 0;
                endSector   = 0;
                bank        = lpc_isp_partinfo.sectorMap[i].bank;
            }
            retval = LPCISP_Erase(LPC_UART, &lpc_isp_cfg, startSector, endSector, lpc_isp_partinfo.sectorMap[i].bank, &lpc_isp_partinfo); // @@@ only support bank zero for now
            startSector++;
            endSector++;
            ble_packet_send_update_percentage_with_func(50 * (i + 1) / lpc_isp_partinfo.numSectors, __func__, __LINE__, "erase"); // ble_update_per(tms_var.count_erase_flash + 1 * 100 / MAX_INDEX_FLASH);
        }

#endif
    }
    return retval;
}

int lpc_isp_writeflash(void) {
    int retval;
    ESP_LOGI(TAG_LPCISP, "lpc_isp_writeflash %ld", lpc_isp_var.file_size);
    retval = LPCISP_WriteToFlash(LPC_UART, &lpc_isp_cfg, lpc_isp_var.file_buffer, lpc_isp_var.writing_offset, lpc_isp_var.file_size, &lpc_isp_partinfo);
    //LPCISP_VerifyFlash(LPC_UART,&lpc_isp_cfg,lpc_isp_var.file_buffer,start,lpc_isp_var.file_size,&lpc_isp_partinfo);
    return retval;
}

void lpc_isp_end(void) {
    ESP_LOGI(TAG_LPCISP, "lpc_isp_end");
    clear_firmware_memory();
    lpc_isp_mode = lpc_isp_idle;
}

void lpc_isp_load(void) {
    ESP_LOGI(TAG_LPCISP, "lpc_isp_load");
    // file open 2c_67.bin
    char *pathbuf = "/sdcard/695e3a80-57a2-4a56-b5fd-69d963aaeba5.hex\0";
    FILE *file    = fopen(pathbuf, "r");
    if (file == NULL) {
        ESP_LOGI(TAG_MIRI, "%s Failed to open file for reading", pathbuf);
        return;
    }
    ESP_LOGI(TAG_MIRI, "%s open file success", pathbuf);
    fseek(file, 0, SEEK_END);
    long stm_firmware_real_size = ftell(file);
    ESP_LOGI(TAG_MIRI, "file size : %ld", stm_firmware_real_size);


    malloc_firmware_memory(stm_firmware_real_size);

    uint64_t stm_firmware_memory_size = 0;
    uint8_t *stm_firmware_memory      = get_firmware_memory(&stm_firmware_memory_size);

    fseek(file, 0, SEEK_SET);
    fread(stm_firmware_memory, 1, stm_firmware_real_size, file);

    fclose(file);
    printf("Free heap size: %ld\n", esp_get_free_heap_size());

    lpc_isp_var.file_buffer = stm_firmware_memory;
    lpc_isp_var.file_size   = stm_firmware_real_size;
    lpc_isp_var.target_swd  = NULL;
}


static void _isp_mode_set(void) {
    //   gpio_set_direction(LPC_GPIO_RESET, GPIO_MODE_DEF_OUTPUT);
    //   gpio_set_direction(LPC_GPIO_BOOT0, GPIO_MODE_DEF_OUTPUT);
    //   gpio_set_direction(LPC_GPIO_BOOT1, GPIO_MODE_DEF_OUTPUT);
    gpio_pad_select_gpio(LPC_GPIO_UART_RTS);
    gpio_pad_select_gpio(LPC_GPIO_UART_CTS);
    gpio_set_direction(LPC_GPIO_UART_RTS, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(LPC_GPIO_UART_CTS, GPIO_MODE_INPUT_OUTPUT);

#if 1

    gpio_set_level(LPC_GPIO_UART_CTS, 1);
    gpio_set_level(LPC_GPIO_UART_RTS, 1);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    gpio_set_level(LPC_GPIO_UART_CTS, 0);
    vTaskDelay(1300 / portTICK_PERIOD_MS);
    gpio_set_level(LPC_GPIO_UART_RTS, 0);
#endif

#if 0


    gpio_set_direction(LPC_GPIO_UART_CTS, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(LPC_GPIO_UART_CTS, 0);
    gpio_set_direction(LPC_GPIO_UART_RTS, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(LPC_GPIO_UART_RTS, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(LPC_GPIO_RESET, 1);
    gpio_set_level(LPC_GPIO_BOOT1, 0);
    gpio_set_level(LPC_GPIO_BOOT0, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(LPC_GPIO_RESET, 0);
    gpio_set_level(LPC_GPIO_BOOT1, 1);
    gpio_set_level(LPC_GPIO_BOOT0, 1);
    vTaskDelay(700 / portTICK_PERIOD_MS);
    gpio_set_level(LPC_GPIO_RESET, 1);
    gpio_set_level(LPC_GPIO_BOOT1, 0);
    gpio_set_level(LPC_GPIO_BOOT0, 0);
    vTaskDelay(2500 / portTICK_PERIOD_MS);

    gpio_set_level(LPC_GPIO_BOOT0, 1);
    gpio_set_level(LPC_GPIO_RESET, 0);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    gpio_set_level(LPC_GPIO_RESET, 1);
#else
    //   gpio_set_level(LPC_GPIO_RESET, 0);
    //   vTaskDelay(1 / portTICK_PERIOD_MS);
#endif
}

//_____ E N D  O F  F I L E ____________________________________________________
