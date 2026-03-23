#ifndef HHT_UART_H
#define HHT_UART_H

#include <stddef.h>
#include <stdint.h>

/* UART1: WB100용 핀 (hht_tx/rx/rts/cts) */
void hht_uart_init(void);

/* UART1: HHT 3000용 핀 (hht3000_tx/rx). WB100과 배타 사용, 같은 UART */
void hht_uart_init_3000(void);

void hht_uart_deinit(void);
void hht_uart_flush(void);
void hht_uart_flush_rx(void);
int hht_uart_read(uint8_t *buf, size_t len, uint32_t timeout_ms);
int hht_uart_write(const uint8_t *buf, size_t len);

#endif
