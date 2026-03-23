/************************************************************************
 * @Filename    : lpc_isp.h
 * @Created on  :
 * @Author      :
 * @Description : 
 * @Copyright(s):
 *
 * @Revision History
 *
 *
 */
#ifndef MIRI_MODULE_LPC_ISP_H
#define MIRI_MODULE_LPC_ISP_H

//_____ I N C L U D E S ________________________________________________________
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "report.h"

//_____ M A C R O S ____________________________________________________________
//_____ D E F I N I T I O N S __________________________________________________
//_____ D E C L A R A T I O N S ________________________________________________
//extern uint8_t isp_mode;
#define    TAG_LPCISP        "LpcIsp "

#define    LPC_UART                    UART_NUM_1
#define    LPC_GPIO_BOOT0                GPIO_NUM_16
#define    LPC_GPIO_RESET                GPIO_NUM_39//GPIO_NUM_16    //
#define    LPC_GPIO_BOOT1                GPIO_NUM_42


#define    LPC_GPIO_UART_RX            GPIO_NUM_38//GPIO_NUM_18    //
#define    LPC_GPIO_UART_TX            GPIO_NUM_40//GPIO_NUM_17    //
#define    LPC_GPIO_UART_RTS           GPIO_NUM_46//GPIO_NUM_18    //
#define    LPC_GPIO_UART_CTS           GPIO_NUM_39//GPIO_NUM_17    //


//_____ F U N C T I O N S ______________________________________________________
extern void vTaskLpcIsp(void *pvParameters);
extern void lpc_isp_Download(uint8_t *pbuff, uint32_t len, uint8_t binary_type);

extern void lpc_isp_init(uint8_t binary_type);
extern void lpc_isp_cleanup(void);
extern int lpc_isp_entermode(void);
extern int lpc_isp_eraseflash(void);
extern int lpc_isp_writeflash(void);
extern void lpc_isp_end(void);
extern void lpc_isp_load(void);

//_____ E N D  O F  F I L E ____________________________________________________


#endif //MIRI_MODULE_SWD_H
