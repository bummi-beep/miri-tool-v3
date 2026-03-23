/*
 * isp_uart.c
 *
 *  Created on: 2025. 1. 17.
 *      Author: 82102
 */

#include <stdbool.h>
#include <driver/uart.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lpc_isp.h"
#include "isp_uart.h"

#define MIN_CHARS       1       // DEBUG something is amiss with this, if VTIME is non-zero we get EAGAIN returned instead of zero (and no delay)
#define CHAR_TIMEOUT    0       // character timeout (read fails and returns if this much time passes without a character) in 1/10's sec


static void delayMicrosecondsToTicks(uint32_t microseconds) {
    // FreeRTOS 틱 단위 변환: 마이크로초 -> 밀리초 -> 틱
    uint32_t ticks = pdMS_TO_TICKS(microseconds / 1000);

    // 최소 지연은 한 틱 이상이어야 하므로 0 틱을 방지
    if (ticks == 0) {
        ticks = 1;
    }

    vTaskDelay(ticks);
}

static int SerialByteWaiting(int uart_num, int timeOutMs) {
    size_t bufferedSize     = 0; // UART 버퍼에 남은 데이터 크기
    int elapsedTime         = 0; // 경과 시간
    const int checkInterval = 10; // 10ms 간격으로 확인

    while (elapsedTime < timeOutMs) {
        // UART 드라이버에서 현재 버퍼에 남아있는 데이터 크기 확인
        if (uart_get_buffered_data_len(uart_num, &bufferedSize) == ESP_OK) {
            if (bufferedSize > 0) {
                return 1; // 데이터가 준비됨
            }
        }

        // 지정된 간격만큼 대기
        vTaskDelay(pdMS_TO_TICKS(checkInterval));
        elapsedTime += checkInterval;
    }

    return 0; // 타임아웃 발생, 데이터 없음
}

int LPCISP_SERIAL_ReadBytes(int fd, unsigned char *buf, unsigned int maxBytes, unsigned int timeOut)
// Attempt to read the given number of bytes before timeout (uS) occurs during any read attempt.
// Return the number of bytes received or -1 on error.  if maxBytes is zero will simply return zero.
//  fd -- file descriptor of serial device
//  buf -- pointer to buffer to be filled
//  maxBytes -- max number of bytes to read
//  timeout -- time to wait on any read, in microseconds
{
    int
            numRead;
    unsigned int
            readSoFar;

    readSoFar = 0;
    numRead   = 0;

    while ((numRead >= 0) && (readSoFar < maxBytes) && SerialByteWaiting(fd, timeOut)) {
        if ((numRead = uart_read_bytes(fd, &buf[readSoFar], maxBytes - readSoFar, 0)) > 0) {
            readSoFar += numRead;
        }
    }

    return (numRead >= 0 ? readSoFar : numRead);
}

unsigned int LPCISP_SERIAL_WriteBytes(int fd, const unsigned char *buf, unsigned int numBytes)
// Write bytes to device.  return number of bytes written (should be same as numBytes unless error).
//  fd -- file descriptor
//  buf -- pointer to buffer to be sent
//  numBytes -- number of bytes to send
{
    return (uart_write_bytes(fd, buf, numBytes));
}

void LPCISP_SERIAL_FlushDevice(int fd)
// flush the serial device.
{
    uart_flush(fd);
}

void LPCISP_SERIAL_SetDTR(int fd, int state) {
    return;
}
void LPCISP_SERIAL_SetRTS(int fd, int state) {
    return;
}
int LPCISP_SERIAL_OpenDevice(const char *name) {
    return 0;
}
void LPCISP_SERIAL_CloseDevice(int fd) {
}

int LPCISP_SERIAL_ChangeBaudRate(int fd, int baud) {
    return 0; //uart_set_baudrate(fd, baud);
}
