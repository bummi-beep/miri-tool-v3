/*
 * isp_uart.h
 *
 *  Created on: 2025. 1. 17.
 *      Author: 82102
 */

#ifndef APP_ESP32_MAIN_OPERATION_FIRMWARE_UPDATE_TARGET_NXP_ISP_ISP_UART_H_
#define APP_ESP32_MAIN_OPERATION_FIRMWARE_UPDATE_TARGET_NXP_ISP_ISP_UART_H_

int LPCISP_SERIAL_ReadBytes(int fd, unsigned char *buf, unsigned int maxBytes, unsigned int timeOut);
unsigned int LPCISP_SERIAL_WriteBytes(int fd, const unsigned char *buf, unsigned int numBytes);
void LPCISP_SERIAL_FlushDevice(int fd);
void LPCISP_SERIAL_SetDTR(int fd, int state);
void LPCISP_SERIAL_SetRTS(int fd, int state);
int LPCISP_SERIAL_OpenDevice(const char *name);
void LPCISP_SERIAL_CloseDevice(int fd);
int LPCISP_SERIAL_ChangeBaudRate(int fd, int baud);
#endif /* APP_ESP32_MAIN_OPERATION_FIRMWARE_UPDATE_TARGET_NXP_ISP_ISP_UART_H_ */
