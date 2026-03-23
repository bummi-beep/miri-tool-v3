#include "probe_link_session.h"

#include <string.h>
#include <stdio.h>

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "probe_link_transport.h"
#include "probe_link_protocol.h"

#define PROBE_LINK_PACKET_MAX 512
#define PROBE_LINK_RESP_MAX 256

static esp_err_t probe_link_read_response(char *out, size_t out_size, uint32_t timeout_ms) {
    if (!out || out_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t idx = 0;
    uint8_t ch = 0;
    size_t rd = 0;
    TickType_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        if (probe_link_transport_recv_packet(&ch, 1, &rd, 50) != ESP_OK || rd == 0) {
            continue;
        }
        if (ch == '$') {
            break;
        }
    }
    if (ch != '$') {
        return ESP_ERR_TIMEOUT;
    }

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        if (probe_link_transport_recv_packet(&ch, 1, &rd, 50) != ESP_OK || rd == 0) {
            continue;
        }
        if (ch == '#') {
            break;
        }
        if (ch == '}') {
            if (probe_link_transport_recv_packet(&ch, 1, &rd, 50) != ESP_OK || rd == 0) {
                return ESP_ERR_TIMEOUT;
            }
            ch = (uint8_t)(ch ^ 0x20);
        }
        if (idx + 1 >= out_size) {
            return ESP_ERR_NO_MEM;
        }
        out[idx++] = (char)ch;
    }

    if (ch != '#') {
        return ESP_ERR_TIMEOUT;
    }

    uint8_t csum[2];
    if (probe_link_transport_recv_packet(&csum[0], 1, &rd, 50) != ESP_OK || rd == 0) {
        return ESP_ERR_TIMEOUT;
    }
    if (probe_link_transport_recv_packet(&csum[1], 1, &rd, 50) != ESP_OK || rd == 0) {
        return ESP_ERR_TIMEOUT;
    }

    out[idx] = '\0';
    probe_link_transport_send_packet((const uint8_t *)"+", 1);
    return ESP_OK;
}

static esp_err_t probe_link_send_raw(const char *payload, char *response, size_t resp_size, uint32_t timeout_ms) {
    uint8_t packet[PROBE_LINK_PACKET_MAX];
    size_t packet_len = 0;
    esp_err_t ret = probe_link_protocol_make_packet(payload, packet, sizeof(packet), &packet_len);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = probe_link_transport_send_packet(packet, packet_len);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t ack_buf[8];
    size_t ack_len = 0;
    ret = probe_link_transport_recv_packet(ack_buf, sizeof(ack_buf), &ack_len, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }
    bool ack = false;
    ret = probe_link_protocol_check_ack(ack_buf, ack_len, &ack);
    if (ret != ESP_OK || !ack) {
        return ESP_FAIL;
    }

    if (!response || resp_size == 0) {
        return ESP_OK;
    }
    return probe_link_read_response(response, resp_size, timeout_ms);
}

esp_err_t probe_link_session_run(const probe_link_session_cfg_t *cfg) {
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    probe_link_transport_cfg_t transport_cfg = {
        .uart_port = UART_NUM_2,
        .baudrate = 115200,
    };
    esp_err_t ret = probe_link_transport_open(&transport_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    char resp[PROBE_LINK_RESP_MAX];
    (void)probe_link_send_raw("qSupported", resp, sizeof(resp), 200);

    const char *scan_cmd = (cfg->iface == PROBE_LINK_IFACE_JTAG) ? "qRcmd,6a7461675f7363616e" : "qRcmd,737764705f7363616e";
    ret = probe_link_send_raw(scan_cmd, resp, sizeof(resp), 500);
    if (ret != ESP_OK) {
        probe_link_transport_close();
        return ret;
    }

    if (cfg->target_index > 0) {
        char attach_cmd[64];
        snprintf(attach_cmd, sizeof(attach_cmd), "vAttach;%d", cfg->target_index);
        ret = probe_link_send_raw(attach_cmd, resp, sizeof(resp), 500);
        if (ret != ESP_OK) {
            probe_link_transport_close();
            return ret;
        }
    }

    if (cfg->reset_after) {
        ret = probe_link_send_raw("qRcmd,7265736574", resp, sizeof(resp), 500);
        if (ret != ESP_OK) {
            probe_link_transport_close();
            return ret;
        }
    }

    probe_link_transport_close();
    return ESP_OK;
}
