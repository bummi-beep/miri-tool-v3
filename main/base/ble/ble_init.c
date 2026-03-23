#include "ble_init.h"

#include <string.h>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <sdkconfig.h>

#if CONFIG_BT_NIMBLE_ENABLED
#include <esp_nimble_hci.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <host/ble_uuid.h>
#include <host/util/util.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#endif

#include "base/ble/ble_gatt_server.h"
#include "base/runtime_config/runtime_config.h"

#define BLE_TAG "ble"
#define GATT_SVR_SVC_ALERT_UUID 0x1811

static void ble_app_advertise(void);

#if CONFIG_BT_NIMBLE_ENABLED
uint16_t g_connection_handle[CONFIG_BT_NIMBLE_MAX_CONNECTIONS];

static uint8_t s_ble_own_addr_type;

/* ble_store_util_status_rr is provided by NimBLE but the header can differ by IDF version. */
extern int ble_store_util_status_rr(struct ble_store_status_event *event, void *arg);

/* new_gatt_svr_init / gatt_svr_register are provided by ble_gatt_server.c */

static void ble_spp_server_on_reset(int reason) {
    ESP_LOGW(BLE_TAG, "ble reset, reason=%d", reason);
}

static void ble_spp_server_on_sync(void) {
    int rc;
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGW(BLE_TAG, "ble_hs_util_ensure_addr failed: %d", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &s_ble_own_addr_type);
    if (rc != 0) {
        ESP_LOGW(BLE_TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(s_ble_own_addr_type, addr_val, NULL);
    if (rc == 0) {
        ESP_LOGI(BLE_TAG, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x",
                 addr_val[5], addr_val[4], addr_val[3],
                 addr_val[2], addr_val[1], addr_val[0]);
    }

    ble_app_advertise();
}

static void ble_spp_server_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(BLE_TAG, "connection %s; status=%d; handle=%d",
                     event->connect.status == 0 ? "established" : "failed",
                     event->connect.status,
                     event->connect.conn_handle);
            if (event->connect.status == 0) {
                uint16_t handle = event->connect.conn_handle;
                if (handle > 0 && handle <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS) {
                    g_connection_handle[handle - 1] = handle;
                }
            }
            if (event->connect.status != 0 || CONFIG_BT_NIMBLE_MAX_CONNECTIONS > 1) {
                ble_app_advertise();
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(BLE_TAG, "disconnect; reason=%d; handle=%d",
                     event->disconnect.reason,
                     event->disconnect.conn.conn_handle);
            if (event->disconnect.conn.conn_handle > 0 &&
                event->disconnect.conn.conn_handle <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS) {
                g_connection_handle[event->disconnect.conn.conn_handle - 1] = 0;
            }
            ble_app_advertise();
            break;
        default:
            break;
    }
    return 0;
}
#endif

void ble_init(void) {
#if CONFIG_BT_NIMBLE_ENABLED
    const char *name = runtime_config_get_device_name();

    ESP_LOGI(BLE_TAG, "ble_init (fw_esp32-like)");
    ESP_LOGI(BLE_TAG, "ble_init: before nimble_port_init");

    /*
     * fw_esp32 starts from nimble_port_init() without an explicit
     * esp_nimble_hci_init() call.
     * 기존 fw_esp32는 이후에 BLE SPP용 UART 브리지를 초기화했지만,
     * miri-tool 에서는 UART 자원을 STM32/콘솔에만 사용하기 위해
     * BLE SPP <-> UART 브리지는 제거하고, 순수 BLE GATT/SPS 경로만 유지한다.
     */
    nimble_port_init();
    ESP_LOGI(BLE_TAG, "ble_init: after nimble_port_init (no UART bridge)");

    ble_hs_cfg.reset_cb = ble_spp_server_on_reset;
    ble_hs_cfg.sync_cb = ble_spp_server_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /*
     * fw_esp32 configures security/bonding options. Keep defaults
     * unless project-specific configs are enabled.
     */
#ifdef CONFIG_EXAMPLE_IO_TYPE
    ble_hs_cfg.sm_io_cap = CONFIG_EXAMPLE_IO_TYPE;
#else
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
#endif
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_bonding = 1;
#else
    ble_hs_cfg.sm_bonding = 0;
#endif
#ifdef CONFIG_EXAMPLE_MITM
    ble_hs_cfg.sm_mitm = 1;
#else
    ble_hs_cfg.sm_mitm = 0;
#endif
#ifdef CONFIG_EXAMPLE_USE_SC
    ble_hs_cfg.sm_sc = 1;
#else
    ble_hs_cfg.sm_sc = 0;
#endif
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_our_key_dist = 1;
    ble_hs_cfg.sm_their_key_dist = 1;
#endif

    int rc = new_gatt_svr_init();
    if (rc != 0) {
        ESP_LOGW(BLE_TAG, "gatt init failed: %d", rc);
        return;
    }
    rc = gatt_svr_register();
    if (rc != 0) {
        ESP_LOGW(BLE_TAG, "gatt register failed: %d", rc);
        return;
    }
    ble_svc_gap_device_name_set(name);

    nimble_port_freertos_init(ble_spp_server_host_task);
    ble_gatt_server_packet_init();
#else
    ESP_LOGI(BLE_TAG, "ble_init skipped (CONFIG_BT_NIMBLE_ENABLED is off)");
#endif
}

static void ble_app_advertise(void) {
#if CONFIG_BT_NIMBLE_ENABLED
    struct ble_hs_adv_fields fields;
    struct ble_gap_adv_params adv_params;

    ESP_LOGI(BLE_TAG, "ble_app_advertise");

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    const char *name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID),
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGW(BLE_TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_ble_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGW(BLE_TAG, "ble_gap_adv_start failed: %d", rc);
        return;
    }
    ESP_LOGI(BLE_TAG, "advertising started");
#endif
}





