#include "probe_link_update.h"

#include "core/firmware_upload/fw_upload_state.h"
#include "core/firmware_upload/probe_link/probe_link_session.h"
#include "core/firmware_upload/probe_link/probe_link_transport.h"
#include "core/firmware_upload/probe_link/probe_link_protocol.h"

#include <cJSON.h>
#include <ctype.h>
#include <string.h>

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define PROBE_LINK_PACKET_MAX 1024

typedef struct {
    probe_link_session_cfg_t session;
    bool dry_run;
    bool do_erase;
    uint32_t base_addr;
} probe_link_meta_cfg_t;

static uint32_t probe_link_parse_u32(const char *text) {
    if (!text) {
        return 0;
    }
    while (isspace((unsigned char)*text)) {
        text++;
    }
    return (uint32_t)strtoul(text, NULL, 0);
}

static void probe_link_parse_meta(const fw_meta_t *meta, probe_link_meta_cfg_t *cfg) {
    if (!cfg) {
        return;
    }
    cfg->dry_run = true;
    cfg->do_erase = true;
    cfg->base_addr = 0x08000000;
    if (!meta || meta->meta_json[0] == '\0') {
        return;
    }
    cJSON *root = cJSON_Parse(meta->meta_json);
    if (!root) {
        return;
    }
    cJSON *j_dry = cJSON_GetObjectItem(root, "probe_dry_run");
    if (cJSON_IsBool(j_dry)) {
        cfg->dry_run = cJSON_IsTrue(j_dry);
    }
    cJSON *j_target = cJSON_GetObjectItem(root, "probe_target");
    if (cJSON_IsNumber(j_target)) {
        cfg->session.target_index = j_target->valueint;
    }
    cJSON *j_reset = cJSON_GetObjectItem(root, "probe_reset");
    if (cJSON_IsBool(j_reset)) {
        cfg->session.reset_after = cJSON_IsTrue(j_reset);
    }
    cJSON *j_erase = cJSON_GetObjectItem(root, "probe_erase");
    if (cJSON_IsBool(j_erase)) {
        cfg->do_erase = cJSON_IsTrue(j_erase);
    }
    cJSON *j_addr = cJSON_GetObjectItem(root, "probe_addr");
    if (cJSON_IsNumber(j_addr)) {
        cfg->base_addr = (uint32_t)j_addr->valuedouble;
    } else if (cJSON_IsString(j_addr) && j_addr->valuestring) {
        cfg->base_addr = probe_link_parse_u32(j_addr->valuestring);
    }
    cJSON_Delete(root);
}

static esp_err_t probe_link_wait_ack(uint32_t timeout_ms) {
    uint8_t ack_buf[8];
    size_t ack_len = 0;
    esp_err_t ret = probe_link_transport_recv_packet(ack_buf, sizeof(ack_buf), &ack_len, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }
    bool ack = false;
    ret = probe_link_protocol_check_ack(ack_buf, ack_len, &ack);
    if (ret != ESP_OK || !ack) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

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

static esp_err_t probe_link_send_text_cmd(const char *payload, char *resp, size_t resp_size, uint32_t timeout_ms) {
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
    ret = probe_link_wait_ack(timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }
    return probe_link_read_response(resp, resp_size, timeout_ms);
}

static esp_err_t probe_link_send_binary_cmd(const char *prefix, const uint8_t *data, size_t data_len,
                                            char *resp, size_t resp_size, uint32_t timeout_ms) {
    uint8_t packet[PROBE_LINK_PACKET_MAX];
    size_t packet_len = 0;
    esp_err_t ret = probe_link_protocol_make_packet_binary(prefix, data, data_len, packet, sizeof(packet), &packet_len);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = probe_link_transport_send_packet(packet, packet_len);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = probe_link_wait_ack(timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }
    return probe_link_read_response(resp, resp_size, timeout_ms);
}

static void probe_link_hex_encode(const char *text, char *out, size_t out_size) {
    static const char hex[] = "0123456789abcdef";
    size_t idx = 0;
    while (text && *text && (idx + 2) < out_size) {
        uint8_t ch = (uint8_t)*text++;
        out[idx++] = hex[(ch >> 4) & 0x0F];
        out[idx++] = hex[ch & 0x0F];
    }
    out[idx] = '\0';
}

esp_err_t probe_link_update_from_reader(const fw_meta_t *meta, fw_storage_reader_t *reader) {
    if (!meta || !reader) {
        fw_state_set_step(FW_STEP_ERROR, "probe link invalid args");
        return ESP_ERR_INVALID_ARG;
    }

    fw_state_set_step(FW_STEP_PREPARE, "probe link prepare");
    fw_state_set_progress(0);

    probe_link_meta_cfg_t cfg = {
        .session = {
            .iface = (meta->exec == FW_EXEC_JTAG) ? PROBE_LINK_IFACE_JTAG : PROBE_LINK_IFACE_SWD,
            .target_index = 1,
            .reset_after = false,
        },
    };
    probe_link_parse_meta(meta, &cfg);

    if (cfg.dry_run) {
        fw_state_set_step(FW_STEP_WRITE, "probe link dry-run");
    } else {
        fw_state_set_step(FW_STEP_WRITE, "probe link write");
    }

    const size_t total_size = reader->meta.original_size;
    size_t total_read = 0;

    if (cfg.dry_run) {
        uint8_t buf[1024];
        while (1) {
            size_t rd = fw_storage_reader_read(reader, buf, sizeof(buf));
            if (rd == 0) {
                break;
            }
            total_read += rd;
            if (total_size > 0) {
                int percent = (int)((total_read * 100UL) / total_size);
                fw_state_set_progress((uint8_t)percent);
            }
        }
        fw_state_set_step(FW_STEP_DONE, "probe link dry-run done");
        fw_state_set_progress(100);
        return ESP_OK;
    }

    probe_link_transport_cfg_t transport_cfg = {
        .uart_port = UART_NUM_2,
        .baudrate = 115200,
    };
    esp_err_t ret = probe_link_transport_open(&transport_cfg);
    if (ret != ESP_OK) {
        fw_state_set_step(FW_STEP_ERROR, "probe link uart open fail");
        return ret;
    }

    char resp[128];
    (void)probe_link_send_text_cmd("qSupported", resp, sizeof(resp), 500);

    char hex_cmd[64];
    if (cfg.session.iface == PROBE_LINK_IFACE_JTAG) {
        probe_link_hex_encode("jtag_scan", hex_cmd, sizeof(hex_cmd));
    } else {
        probe_link_hex_encode("swdp_scan", hex_cmd, sizeof(hex_cmd));
    }
    char cmd_buf[96];
    snprintf(cmd_buf, sizeof(cmd_buf), "qRcmd,%s", hex_cmd);
    ret = probe_link_send_text_cmd(cmd_buf, resp, sizeof(resp), 1000);
    if (ret != ESP_OK) {
        fw_state_set_step(FW_STEP_ERROR, "probe link scan fail");
        probe_link_transport_close();
        return ret;
    }

    snprintf(cmd_buf, sizeof(cmd_buf), "vAttach;%d", cfg.session.target_index);
    ret = probe_link_send_text_cmd(cmd_buf, resp, sizeof(resp), 1000);
    if (ret != ESP_OK) {
        fw_state_set_step(FW_STEP_ERROR, "probe link attach fail");
        probe_link_transport_close();
        return ret;
    }

    if (cfg.do_erase) {
        snprintf(cmd_buf, sizeof(cmd_buf), "vFlashErase:%08lx,%08lx",
                 (unsigned long)cfg.base_addr, (unsigned long)total_size);
        ret = probe_link_send_text_cmd(cmd_buf, resp, sizeof(resp), 2000);
        if (ret != ESP_OK) {
            fw_state_set_step(FW_STEP_ERROR, "probe link erase fail");
            probe_link_transport_close();
            return ret;
        }
    }

    uint8_t data_buf[256];
    uint32_t addr = cfg.base_addr;
    while (1) {
        size_t rd = fw_storage_reader_read(reader, data_buf, sizeof(data_buf));
        if (rd == 0) {
            break;
        }
        snprintf(cmd_buf, sizeof(cmd_buf), "vFlashWrite:%08lx:", (unsigned long)addr);
        ret = probe_link_send_binary_cmd(cmd_buf, data_buf, rd, resp, sizeof(resp), 2000);
        if (ret != ESP_OK || strncmp(resp, "OK", 2) != 0) {
            fw_state_set_step(FW_STEP_ERROR, "probe link write fail");
            probe_link_transport_close();
            return ESP_FAIL;
        }
        addr += (uint32_t)rd;
        total_read += rd;
        if (total_size > 0) {
            int percent = (int)((total_read * 100UL) / total_size);
            fw_state_set_progress((uint8_t)percent);
        }
    }

    ret = probe_link_send_text_cmd("vFlashDone", resp, sizeof(resp), 2000);
    if (ret != ESP_OK || strncmp(resp, "OK", 2) != 0) {
        fw_state_set_step(FW_STEP_ERROR, "probe link done fail");
        probe_link_transport_close();
        return ESP_FAIL;
    }

    if (cfg.session.reset_after) {
        probe_link_hex_encode("reset", hex_cmd, sizeof(hex_cmd));
        snprintf(cmd_buf, sizeof(cmd_buf), "qRcmd,%s", hex_cmd);
        (void)probe_link_send_text_cmd(cmd_buf, resp, sizeof(resp), 1000);
    }

    probe_link_transport_close();
    fw_state_set_step(FW_STEP_DONE, "probe link done");
    fw_state_set_progress(100);
    return ESP_OK;
}
