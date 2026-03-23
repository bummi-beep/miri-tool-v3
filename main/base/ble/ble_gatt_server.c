#include "ble_gatt_server.h"

#include <string.h>
#include <stdlib.h>

#include <esp_log.h>
#include <sdkconfig.h>

#if CONFIG_BT_NIMBLE_ENABLED
#include <host/ble_hs.h>
#include <host/ble_hs_mbuf.h>
#include <host/ble_uuid.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <os/os_mbuf.h>
#endif

#include "packet.h"
#include "protocol/ble_cmd_parser.h"

static const char *TAG_GATT = "ble_gatt";

#define BLE_SVC_ANS_UUID16                       0x1811
#define BLE_SVC_ANS_CHR_UUID16_SUP_NEW_ALERT_CAT 0x2a47

/* 59462f12-9543-9999-12c8-58b459a2712d */
static const ble_uuid128_t gatt_svr_svc_sec_test_uuid =
    BLE_UUID128_INIT(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
                     0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);

/* 5c3a659e-897e-45e1-b016-007107c96df6 */
static const ble_uuid128_t gatt_svr_chr_sec_test_rand_uuid =
    BLE_UUID128_INIT(0xf6, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
                     0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

/* 5c3a659e-897e-45e1-b016-007107c96df7 */
static const ble_uuid128_t gatt_svr_chr_sec_test_static_uuid =
    BLE_UUID128_INIT(0xf7, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
                     0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

static uint8_t gatt_svr_sec_test_static_val;
static uint16_t s_ble_spp_val_handle;
static uint16_t s_ble_alert_val_handle;
static uint8_t s_demo_write_data[512];
static PRODUCT_PACKET s_ble_packet;

#if CONFIG_BT_NIMBLE_ENABLED
extern uint16_t g_connection_handle[CONFIG_BT_NIMBLE_MAX_CONNECTIONS];

static int gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                              void *dst, uint16_t *len);
int ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg);

static struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_sec_test_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &gatt_svr_chr_sec_test_rand_uuid.u,
                .access_cb = gatt_svr_chr_access_sec_test,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            },
            {
                .uuid = &gatt_svr_chr_sec_test_static_uuid.u,
                .access_cb = gatt_svr_chr_access_sec_test,
                .flags = BLE_GATT_CHR_F_READ |
                         BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
            },
            {0},
        },
    },
    {0},
};

static const struct ble_gatt_svc_def new_ble_svc_gatt_defs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_ANS_UUID16),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_ANS_CHR_UUID16_SUP_NEW_ALERT_CAT),
                .access_cb = ble_svc_gatt_handler,
                .val_handle = &s_ble_alert_val_handle,
                .flags = BLE_GATT_CHR_F_READ |
                         BLE_GATT_CHR_F_WRITE |
                         BLE_GATT_CHR_F_NOTIFY |
                         BLE_GATT_CHR_F_INDICATE,
            },
            {0},
        },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                                    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID128_DECLARE(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                                            0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E),
                .access_cb = ble_svc_gatt_handler,
                .val_handle = &s_ble_spp_val_handle,
                .flags = BLE_GATT_CHR_F_READ |
                         BLE_GATT_CHR_F_WRITE |
                         BLE_GATT_CHR_F_WRITE_NO_RSP |
                         BLE_GATT_CHR_F_NOTIFY |
                         BLE_GATT_CHR_F_INDICATE,
            },
            {0},
        },
    },
    {0},
};
#endif

static void ble_parsing_packet(void) {
    if (s_ble_packet.Length < 2) {
        return;
    }
    uint16_t cmd = (uint16_t)((s_ble_packet.rBuff[0] << 8) | s_ble_packet.rBuff[1]);
    size_t payload_len = (size_t)(s_ble_packet.Length - 2);
    ble_cmd_handle_with_payload(cmd, &s_ble_packet.rBuff[2], payload_len);
}

static void ble_send_package(uint8_t *pbuff, uint16_t length) {
#if CONFIG_BT_NIMBLE_ENABLED
    for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        if (g_connection_handle[i] != 0) {
            int            rc = 0;
            struct os_mbuf *txom;
            txom = ble_hs_mbuf_from_flat(pbuff, length);
            rc   = ble_gatts_notify_custom(g_connection_handle[i], s_ble_spp_val_handle, txom);

            if (rc == 0) {
                ESP_LOGV(TAG_GATT, "Notification sent successfully");
            } else {
                ESP_LOGW(TAG_GATT, "Error in sending notification rc = %d (%d)", rc, i);
            }
        }
    }
#else
    (void)pbuff;
    (void)length;
#endif
}

void ble_gatt_server_packet_init(void) {
    ProductPacket_Init(&s_ble_packet, ble_send_package, ble_parsing_packet);
}

void ble_gatt_server_send_packet(uint16_t cmd, const uint8_t *payload, size_t payload_len) {
    ProductPacket_PacketSendByUART(&s_ble_packet, cmd, (uint8_t *)payload, (uint8_t)payload_len);
}

#if CONFIG_BT_NIMBLE_ENABLED
static int gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const ble_uuid_t *uuid;
    int rc;
    int rand_num;

    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    uuid = ctxt->chr->uuid;

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_sec_test_rand_uuid.u) == 0) {
        rand_num = rand();
        rc = os_mbuf_append(ctxt->om, &rand_num, sizeof rand_num);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_sec_test_static_uuid.u) == 0) {
        switch (ctxt->op) {
            case BLE_GATT_ACCESS_OP_READ_CHR:
                rc = os_mbuf_append(ctxt->om, &gatt_svr_sec_test_static_val,
                                    sizeof gatt_svr_sec_test_static_val);
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            case BLE_GATT_ACCESS_OP_WRITE_CHR:
                rc = gatt_svr_chr_write(ctxt->om,
                                        sizeof gatt_svr_sec_test_static_val,
                                        sizeof gatt_svr_sec_test_static_val,
                                        &gatt_svr_sec_test_static_val, NULL);
                return rc;
            default:
                return BLE_ATT_ERR_UNLIKELY;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                              void *dst, uint16_t *len) {
    uint16_t om_len = OS_MBUF_PKTLEN(om);
    int rc;

    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

int new_gatt_svr_init(void) {
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }
    return 0;
}

int gatt_svr_register(void) {
    int rc = ble_gatts_count_cfg(new_ble_svc_gatt_defs);
    if (rc != 0) {
        return rc;
    }
    rc = ble_gatts_add_svcs(new_ble_svc_gatt_defs);
    if (rc != 0) {
        return rc;
    }
    return 0;
}

int ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            ESP_LOGI(TAG_GATT, "Callback for read");
            break;
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            ESP_LOGD(TAG_GATT, "Data received in write event,conn_handle = %x,attr_handle = %x",
                     conn_handle, attr_handle);
            uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
            if (om_len > sizeof(s_demo_write_data)) {
                om_len = sizeof(s_demo_write_data);
            }
            ble_hs_mbuf_to_flat(ctxt->om, &s_demo_write_data, sizeof(s_demo_write_data), &om_len);
            for (int i = 0; i < om_len; i++) {
                ProductPacket_ParserPacket(&s_ble_packet, s_demo_write_data[i]);
            }
            break;
        default:
            ESP_LOGI(TAG_GATT, "\nDefault Callback");
            break;
    }
    return 0;
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    (void)arg;
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGD(TAG_GATT, "registered service %s handle=%d",
                     ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                     ctxt->svc.handle);
            break;
        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGD(TAG_GATT, "registered chr %s def=%d val=%d",
                     ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                     ctxt->chr.def_handle, ctxt->chr.val_handle);
            break;
        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGD(TAG_GATT, "registered dsc %s handle=%d",
                     ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                     ctxt->dsc.handle);
            break;
        default:
            break;
    }
}
#else
int new_gatt_svr_init(void) { return 0; }
int gatt_svr_register(void) { return 0; }
int ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)ctxt;
    (void)arg;
    return 0;
}
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    (void)ctxt;
    (void)arg;
}
#endif
