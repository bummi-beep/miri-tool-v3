#ifndef BLE_GATT_SERVER_H
#define BLE_GATT_SERVER_H

#include <stddef.h>
#include <stdint.h>

#if CONFIG_BT_NIMBLE_ENABLED
#include <host/ble_gatt.h>
#endif

int new_gatt_svr_init(void);
int gatt_svr_register(void);
void ble_gatt_server_packet_init(void);
void ble_gatt_server_send_packet(uint16_t cmd, const uint8_t *payload, size_t payload_len);

#if CONFIG_BT_NIMBLE_ENABLED
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
#endif

#endif
