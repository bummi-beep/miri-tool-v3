#include "tcp_server.h"

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "core/firmware_upload/fw_upload_manager.h"
#include "core/firmware_upload/fw_upload_state.h"
#include "core/firmware_upload/fw_upload_storage.h"

#define TCP_SERVER_TAG "tcp"
#define TCP_SERVER_PORT 3333
#define TCP_RX_BUF_SIZE 1024

static TaskHandle_t s_tcp_task = NULL;
static portMUX_TYPE s_tcp_lock = portMUX_INITIALIZER_UNLOCKED;
static char s_pending_name[64] = {0};
static char s_pending_type[8] = {0};
static size_t s_expected_size = 0;

static void tcp_get_pending(char *name, size_t name_size, char *type, size_t type_size, size_t *out_size) {
    if (name && name_size > 0) {
        name[0] = '\0';
    }
    if (type && type_size > 0) {
        type[0] = '\0';
    }
    if (out_size) {
        *out_size = 0;
    }
    portENTER_CRITICAL(&s_tcp_lock);
    if (name && name_size > 0) {
        snprintf(name, name_size, "%s", s_pending_name);
    }
    if (type && type_size > 0) {
        snprintf(type, type_size, "%s", s_pending_type);
    }
    if (out_size) {
        *out_size = s_expected_size;
    }
    portEXIT_CRITICAL(&s_tcp_lock);
    ESP_LOGI(TCP_SERVER_TAG, "pending snapshot: name='%s' type='%s' expected=%u",
             (name ? name : ""), (type ? type : ""), (unsigned)(out_size ? *out_size : 0));
}

void tcp_server_set_pending_file(const char *name, const char *type) {
    if (!name || !type) {
        return;
    }
    portENTER_CRITICAL(&s_tcp_lock);
    snprintf(s_pending_name, sizeof(s_pending_name), "%s", name);
    snprintf(s_pending_type, sizeof(s_pending_type), "%s", type);
    portEXIT_CRITICAL(&s_tcp_lock);
    ESP_LOGI(TCP_SERVER_TAG, "pending file set: %s.%s", s_pending_name, s_pending_type);
}

void tcp_server_set_expected_size(size_t size) {
    portENTER_CRITICAL(&s_tcp_lock);
    s_expected_size = size;
    portEXIT_CRITICAL(&s_tcp_lock);
    ESP_LOGI(TCP_SERVER_TAG, "expected size set: %u", (unsigned)size);
}

bool tcp_server_has_pending(void) {
    bool ready = false;
    portENTER_CRITICAL(&s_tcp_lock);
    ready = (s_pending_name[0] != '\0' && s_pending_type[0] != '\0');
    portEXIT_CRITICAL(&s_tcp_lock);
    return ready;
}

static void tcp_delete_partial(const char *name, const char *type) {
    char path[384];
    if (fw_storage_build_path(path, sizeof(path), name, type)) {
        remove(path);
    }
    if (fw_storage_build_meta_path(path, sizeof(path), name, type)) {
        remove(path);
    }
}

static void tcp_handle_client(int sock) {
    char name[64];
    char type[8];
    size_t expected = 0;
    ESP_LOGI(TCP_SERVER_TAG, "client connected, socket=%d", sock);
    tcp_get_pending(name, sizeof(name), type, sizeof(type), &expected);
    if (name[0] == '\0' || type[0] == '\0') {
        ESP_LOGW(TCP_SERVER_TAG, "no pending file info; dropping upload");
        return;
    }
    if (expected == 0) {
        ESP_LOGW(TCP_SERVER_TAG, "expected size not set; dropping upload");
        return;
    }

    fw_meta_t meta;
    if (fw_upload_prepare_meta(&meta, name, type, FW_FMT_UNKNOWN) != ESP_OK) {
        ESP_LOGW(TCP_SERVER_TAG, "meta init failed");
        return;
    }

    fw_upload_session_t session;
    if (fw_upload_session_begin(&session, &meta, expected) != ESP_OK) {
        ESP_LOGW(TCP_SERVER_TAG, "store init failed");
        return;
    }

    ESP_LOGI(TCP_SERVER_TAG, "upload start: %s.%s size=%u", name, type, (unsigned)expected);
    fw_state_set_step(FW_STEP_TRANSFER, "tcp upload");

    uint8_t buf[TCP_RX_BUF_SIZE];
    size_t received = 0;
    while (received < expected) {
        int to_read = (int)((expected - received) > sizeof(buf) ? sizeof(buf) : (expected - received));
        int len = recv(sock, buf, to_read, 0);
        if (len <= 0) {
            break;
        }
        if (fw_upload_session_write(&session, buf, (size_t)len) != ESP_OK) {
            ESP_LOGW(TCP_SERVER_TAG, "store write failed");
            break;
        }
        received += (size_t)len;
        if (expected > 0) {
            uint32_t progress = (uint32_t)((received * 100) / expected);
            fw_state_set_progress(progress);
        }
    }

    fw_upload_session_finish(&session);
    if (received != expected) {
        ESP_LOGW(TCP_SERVER_TAG, "upload incomplete: %u/%u", (unsigned)received, (unsigned)expected);
        fw_state_set_step(FW_STEP_ERROR, "tcp upload incomplete");
        tcp_delete_partial(name, type);
        return;
    }

    ESP_LOGI(TCP_SERVER_TAG, "upload done: %s.%s", name, type);
    fw_state_set_step(FW_STEP_DONE, "tcp upload done");
}

static void tcp_server_task(void *arg) {
    (void)arg;
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(TCP_SERVER_PORT);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TCP_SERVER_TAG, "socket create failed: errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE(TCP_SERVER_TAG, "socket bind failed: errno=%d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    if (listen(listen_sock, 1) != 0) {
        ESP_LOGE(TCP_SERVER_TAG, "socket listen failed: errno=%d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TCP_SERVER_TAG, "listening on port %d", TCP_SERVER_PORT);
    while (true) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TCP_SERVER_TAG, "accept failed: errno=%d", errno);
            continue;
        }
        char addr_str[32];
        inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(TCP_SERVER_TAG, "accepted from %s", addr_str);
        tcp_handle_client(sock);
        shutdown(sock, 0);
        close(sock);
    }
}

void tcp_server_start(void) {
    if (s_tcp_task) {
        ESP_LOGI(TCP_SERVER_TAG, "tcp server already running");
        return;
    }
    BaseType_t ok = xTaskCreatePinnedToCore(
        tcp_server_task,
        "tcp_server",
        4096,
        NULL,
        5,
        &s_tcp_task,
        1);
    if (ok != pdPASS) {
        ESP_LOGE(TCP_SERVER_TAG, "tcp server task create failed");
        s_tcp_task = NULL;
    }
}
