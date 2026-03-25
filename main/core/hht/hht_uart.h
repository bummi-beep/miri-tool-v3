#ifndef HHT_UART_H
#define HHT_UART_H

#include <stddef.h>
#include <stdint.h>

/* UART1: WB100용 핀 (hht_tx/rx/rts/cts) */
void hht_uart_init(void);

/* UART1: HHT 3000은 pinmap cmd_txd/cmd_rxd (IO13/IO14, Command UART). WB100과 배타 사용 */
void hht_uart_init_3000(void);

void hht_uart_deinit(void);
void hht_uart_flush(void);
void hht_uart_flush_rx(void);
/** WB100 세션 이후 프로젝트 응답 대기용: UART1 RX 큐에 쌓인 바이트 수 */
size_t hht_uart_rx_buffered_bytes(void);
int hht_uart_read(uint8_t *buf, size_t len, uint32_t timeout_ms);
int hht_uart_write(const uint8_t *buf, size_t len);

#endif
