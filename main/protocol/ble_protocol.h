#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

#include <stdint.h>

#if CONFIG_BT_NIMBLE_ENABLED
#include <host/ble_gap.h>
#endif

int ble_protocol_init(void);

#if CONFIG_BT_NIMBLE_ENABLED
void ble_protocol_send_hht_line(uint8_t row, const char *line20);
void ble_protocol_send_cmd(uint16_t cmd, const uint8_t *payload, size_t payload_len);
void ble_protocol_on_gap_event(const struct ble_gap_event *event);
uint16_t ble_protocol_get_conn_handle(void);
#endif

#endif

