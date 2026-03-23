#include "ble_protocol.h"

#include <string.h>
#include <ctype.h>

#include <esp_log.h>

#if CONFIG_BT_NIMBLE_ENABLED
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <host/ble_uuid.h>
#include <services/gatt/ble_svc_gatt.h>
#include <services/gap/ble_svc_gap.h>
#include <nimble/ble.h>

#include "protocol/ble_cmd_parser.h"
#include "base/console/cli_hht_view.h"
#include "base/console/cli_ota_view.h"
#include "base/ble/ble_gatt_server.h"

static const char *TAG = "ble_proto";

static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_tx_handle = 0;

#define BLE_SVC_UUID16 0xFFE0
#define BLE_RX_UUID16  0xFFE1
#define BLE_TX_UUID16  0xFFE2

#if CONFIG_BT_NIMBLE_ENABLED
static bool payload_is_printable(const uint8_t *payload, size_t len) {
    if (!payload || len == 0) {
        return true;
    }
    for (size_t i = 0; i < len; i++) {
        uint8_t ch = payload[i];
        if (ch == '\0') {
            return false;
        }
        if (!isprint((int)ch) && !isspace((int)ch)) {
            return false;
        }
    }
    return true;
}

static void ble_protocol_cli_mirror(uint16_t cmd, const uint8_t *payload, size_t payload_len) {
    if (payload_len == 0 || !payload) {
        ESP_LOGI(TAG, "CLI mirror: cmd=0x%04x (no payload)", cmd);
        if (cli_ota_view_is_active()) {
            cli_ota_view_set_last_event(cmd, "-");
        }
        return;
    }

    if ((payload[0] == '{' || payload[0] == '[') && payload_is_printable(payload, payload_len)) {
        char buf[256];
        size_t n = payload_len >= sizeof(buf) ? sizeof(buf) - 1 : payload_len;
        memcpy(buf, payload, n);
        buf[n] = '\0';
        ESP_LOGI(TAG, "CLI mirror: cmd=0x%04x payload=%s", cmd, buf);
        if (cli_ota_view_is_active()) {
            cli_ota_view_set_last_event(cmd, buf);
        }
        return;
    }

    if (payload_is_printable(payload, payload_len)) {
        char buf[256];
        size_t n = payload_len >= sizeof(buf) ? sizeof(buf) - 1 : payload_len;
        memcpy(buf, payload, n);
        buf[n] = '\0';
        ESP_LOGI(TAG, "CLI mirror: cmd=0x%04x payload=%s", cmd, buf);
        if (cli_ota_view_is_active()) {
            cli_ota_view_set_last_event(cmd, buf);
        }
        return;
    }

    char hex[192];
    size_t limit = payload_len;
    if (limit > 32) {
        limit = 32;
    }
    size_t pos = 0;
    for (size_t i = 0; i < limit && (pos + 3) < sizeof(hex); i++) {
        pos += (size_t)snprintf(hex + pos, sizeof(hex) - pos, "%02x", payload[i]);
    }
    hex[pos] = '\0';
    ESP_LOGI(TAG, "CLI mirror: cmd=0x%04x payload_hex=%s%s", cmd, hex, payload_len > 32 ? "..." : "");
    if (cli_ota_view_is_active()) {
        cli_ota_view_set_last_event(cmd, hex);
    }
}
#endif

static int gatt_svr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const ble_uuid_t *uuid = ctxt->chr->uuid;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ble_uuid_u16(uuid) == BLE_RX_UUID16) {
            uint8_t buf[32];
            int len = ctxt->om->om_len;
            if (len > (int)sizeof(buf)) {
                len = sizeof(buf);
            }
            os_mbuf_copydata(ctxt->om, 0, len, buf);

            if (len >= 2) {
                uint16_t cmd = (uint16_t)(buf[0] | (buf[1] << 8));
                ESP_LOGI(TAG, "rx cmd from BLE: 0x%04x (len=%d)", cmd, len);
                ble_cmd_handle_with_payload(cmd, &buf[2], (size_t)(len - 2));
            } else {
                ESP_LOGW(TAG, "rx cmd too short (len=%d)", len);
            }
            return 0;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_UUID16),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(BLE_RX_UUID16),
                .access_cb = gatt_svr_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = BLE_UUID16_DECLARE(BLE_TX_UUID16),
                .access_cb = gatt_svr_access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_tx_handle,
            },
            {0},
        },
    },
    {0},
};
#endif

int ble_protocol_init(void) {
#if CONFIG_BT_NIMBLE_ENABLED
    int rc;

    ESP_LOGI(TAG, "ble_protocol_init");

    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return rc;
    }
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return rc;
    }
    return 0;
#else
    ESP_LOGI("ble_proto", "ble_protocol_init skipped");
    return 0;
#endif
}

#if CONFIG_BT_NIMBLE_ENABLED
void ble_protocol_on_gap_event(const struct ble_gap_event *event) {
    if (!event) {
        return;
    }

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                s_conn_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "BLE connected (handle=%d)", s_conn_handle);
                cli_hht_view_set_ble_connected(true);
            } else {
                s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
                ESP_LOGW(TAG, "BLE connect failed (status=%d)", event->connect.status);
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE disconnected (reason=%d)", event->disconnect.reason);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            cli_hht_view_set_ble_connected(false);
            break;
        default:
            break;
    }
}

void ble_protocol_send_cmd(uint16_t cmd, const uint8_t *payload, size_t payload_len) {
    ESP_LOGI(TAG, "BLE TX : 0x%04x", (unsigned)cmd);
    ble_protocol_cli_mirror(cmd, payload, payload_len);
    ble_gatt_server_send_packet(cmd, payload, payload_len);
}

void ble_protocol_send_hht_line(uint8_t row, const char *line20) {
    if (!line20) {
        return;
    }
    uint8_t payload[21];
    payload[0] = row;
    memcpy(&payload[1], line20, 20);
    ble_protocol_send_cmd(CMD_HHT_MSG_100, payload, sizeof(payload));
    ESP_LOGI(TAG, "BLE notify HHT row%d: %.20s", row, line20);
}

uint16_t ble_protocol_get_conn_handle(void) {
    return s_conn_handle;
}
#endif

