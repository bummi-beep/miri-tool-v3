#include "probe_link_protocol.h"

#include <string.h>
#include <stdio.h>

static bool probe_link_protocol_needs_escape(uint8_t ch) {
    return (ch == '$' || ch == '#' || ch == '}');
}

esp_err_t probe_link_protocol_make_packet(const char *payload, uint8_t *out, size_t out_size, size_t *out_len) {
    if (!payload || !out || !out_len || out_size < 5) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t csum = 0;
    size_t payload_len = strlen(payload);
    size_t idx = 0;
    if (out_size < payload_len + 4) {
        return ESP_ERR_NO_MEM;
    }
    out[idx++] = '$';
    for (size_t i = 0; i < payload_len; i++) {
        uint8_t ch = (uint8_t)payload[i];
        out[idx++] = ch;
        csum = (uint8_t)(csum + ch);
    }
    out[idx++] = '#';
    static const char hex[] = "0123456789abcdef";
    out[idx++] = (uint8_t)hex[(csum >> 4) & 0x0F];
    out[idx++] = (uint8_t)hex[csum & 0x0F];
    *out_len = idx;
    return ESP_OK;
}

esp_err_t probe_link_protocol_make_packet_binary(const char *prefix, const uint8_t *data, size_t data_len,
                                                 uint8_t *out, size_t out_size, size_t *out_len) {
    if (!prefix || !data || !out || !out_len || out_size < 8) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t csum = 0;
    size_t idx = 0;
    out[idx++] = '$';

    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < prefix_len; i++) {
        uint8_t ch = (uint8_t)prefix[i];
        if (idx + 1 >= out_size) {
            return ESP_ERR_NO_MEM;
        }
        out[idx++] = ch;
        csum = (uint8_t)(csum + ch);
    }

    for (size_t i = 0; i < data_len; i++) {
        uint8_t ch = data[i];
        if (probe_link_protocol_needs_escape(ch)) {
            if (idx + 2 >= out_size) {
                return ESP_ERR_NO_MEM;
            }
            out[idx++] = '}';
            csum = (uint8_t)(csum + '}');
            uint8_t esc = (uint8_t)(ch ^ 0x20);
            out[idx++] = esc;
            csum = (uint8_t)(csum + esc);
        } else {
            if (idx + 1 >= out_size) {
                return ESP_ERR_NO_MEM;
            }
            out[idx++] = ch;
            csum = (uint8_t)(csum + ch);
        }
    }

    if (idx + 3 >= out_size) {
        return ESP_ERR_NO_MEM;
    }
    out[idx++] = '#';
    static const char hex[] = "0123456789abcdef";
    out[idx++] = (uint8_t)hex[(csum >> 4) & 0x0F];
    out[idx++] = (uint8_t)hex[csum & 0x0F];
    *out_len = idx;
    return ESP_OK;
}

esp_err_t probe_link_protocol_check_ack(const uint8_t *buf, size_t len, bool *out_ack) {
    if (!buf || len == 0 || !out_ack) {
        return ESP_ERR_INVALID_ARG;
    }
    if (buf[0] == '+') {
        *out_ack = true;
        return ESP_OK;
    }
    if (buf[0] == '-') {
        *out_ack = false;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_RESPONSE;
}
