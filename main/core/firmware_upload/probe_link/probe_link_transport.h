#ifndef PROBE_LINK_TRANSPORT_H
#define PROBE_LINK_TRANSPORT_H

/*
 * Probe link transport layer
 * - UART framing, ACK/NAK, timeouts, retransmission
 * - Used by probe_link_protocol to send GDB remote packets
 */

#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>

typedef struct {
    int uart_port;
    int baudrate;
} probe_link_transport_cfg_t;

esp_err_t probe_link_transport_open(const probe_link_transport_cfg_t *cfg);
void probe_link_transport_close(void);
esp_err_t probe_link_transport_send_packet(const uint8_t *buf, size_t len);
esp_err_t probe_link_transport_recv_packet(uint8_t *buf, size_t len, size_t *out_len, uint32_t timeout_ms);

#endif
