#ifndef PROBE_LINK_PROTOCOL_H
#define PROBE_LINK_PROTOCOL_H

/*
 * Probe link protocol layer
 * - GDB Remote Protocol packet build/parse helpers
 * - monitor commands (swdp_scan/jtag_scan, attach, reset)
 */

#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>
#include <stdbool.h>

esp_err_t probe_link_protocol_make_packet(const char *payload, uint8_t *out, size_t out_size, size_t *out_len);
esp_err_t probe_link_protocol_make_packet_binary(const char *prefix, const uint8_t *data, size_t data_len,
                                                 uint8_t *out, size_t out_size, size_t *out_len);
esp_err_t probe_link_protocol_check_ack(const uint8_t *buf, size_t len, bool *out_ack);

#endif
