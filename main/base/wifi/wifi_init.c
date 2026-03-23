#include "wifi_init.h"

#include <string.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>

#include "base/runtime_config/runtime_config.h"

#define WIFI_TAG "wifi"

#ifndef CONFIG_MIRI_WIFI_SSID
#define CONFIG_MIRI_WIFI_SSID "MIRI-AP"
#endif
#ifndef CONFIG_MIRI_WIFI_PASSWORD
#define CONFIG_MIRI_WIFI_PASSWORD "1234567890"
#endif
#ifndef CONFIG_MIRI_WIFI_CHANNEL
#define CONFIG_MIRI_WIFI_CHANNEL 1
#endif
#ifndef CONFIG_MIRI_MAX_STA_CONN
#define CONFIG_MIRI_MAX_STA_CONN 4
#endif

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data) {
    if (event_base != WIFI_EVENT) {
        return;
    }

    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(WIFI_TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(WIFI_TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

void wifi_init(void) {
    ESP_LOGI(WIFI_TAG, "wifi_init (SoftAP)");

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    /*
     * Use WPA2 only. WPA2_WPA3_PSK (SAE) uses SHA HW and can abort when BLE
     * is already running (shared crypto HW conflict). That abort triggers reboot
     * and can lead to "Image hash failed" / infinite reboot.
     */
    wifi_config_t wifi_config = {
        .ap =
            {
                .ssid = CONFIG_MIRI_WIFI_SSID,
                .ssid_len = strlen(CONFIG_MIRI_WIFI_SSID),
                .channel = CONFIG_MIRI_WIFI_CHANNEL,
                .password = CONFIG_MIRI_WIFI_PASSWORD,
                .max_connection = CONFIG_MIRI_MAX_STA_CONN,
                .authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg = {.required = false},
            },
    };

    if (strlen(CONFIG_MIRI_WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    const char *device_name = runtime_config_get_device_name();
    if (device_name && device_name[0] != '\0') {
        snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "%s", device_name);
        wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "SoftAP started. SSID:%s channel:%d",
             CONFIG_MIRI_WIFI_SSID, CONFIG_MIRI_WIFI_CHANNEL);
}





