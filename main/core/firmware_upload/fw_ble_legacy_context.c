/**
 * BLE 전용 레거시 컨텍스트 (2차 메타). TCP와 무관.
 */
#include "fw_ble_legacy_context.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define LEGACY_NAME_SZ  64
#define LEGACY_TYPE_SZ  8

static struct {
    char file_name[LEGACY_NAME_SZ];
    char file_type[LEGACY_TYPE_SZ];
    size_t expected_size;
    portMUX_TYPE lock;
} s_ctx = {
    .file_name = {0},
    .file_type = {0},
    .expected_size = 0,
    .lock = portMUX_INITIALIZER_UNLOCKED,
};
static bool s_init;

static void ensure_init(void) {
    if (!s_init) {
        s_ctx.file_name[0] = '\0';
        s_ctx.file_type[0] = '\0';
        s_ctx.expected_size = 0;
        s_init = true;
    }
}

void fw_ble_legacy_set_type(const char *type) {
    if (!type) return;
    ensure_init();
    portENTER_CRITICAL(&s_ctx.lock);
    strncpy(s_ctx.file_type, type, LEGACY_TYPE_SZ - 1);
    s_ctx.file_type[LEGACY_TYPE_SZ - 1] = '\0';
    portEXIT_CRITICAL(&s_ctx.lock);
}

void fw_ble_legacy_set_expected_size(size_t size) {
    ensure_init();
    portENTER_CRITICAL(&s_ctx.lock);
    s_ctx.expected_size = size;
    portEXIT_CRITICAL(&s_ctx.lock);
}

void fw_ble_legacy_set_file_name(const char *name) {
    if (!name) return;
    ensure_init();
    portENTER_CRITICAL(&s_ctx.lock);
    strncpy(s_ctx.file_name, name, LEGACY_NAME_SZ - 1);
    s_ctx.file_name[LEGACY_NAME_SZ - 1] = '\0';
    portEXIT_CRITICAL(&s_ctx.lock);
}

bool fw_ble_legacy_get_snapshot(char *name, size_t name_sz, char *type, size_t type_sz, size_t *out_expected_size) {
    ensure_init();
    portENTER_CRITICAL(&s_ctx.lock);
    if (name && name_sz > 0) {
        strncpy(name, s_ctx.file_name, name_sz - 1);
        name[name_sz - 1] = '\0';
    }
    if (type && type_sz > 0) {
        strncpy(type, s_ctx.file_type, type_sz - 1);
        type[type_sz - 1] = '\0';
    }
    if (out_expected_size) {
        *out_expected_size = s_ctx.expected_size;
    }
    bool has_any = (s_ctx.file_name[0] != '\0' || s_ctx.file_type[0] != '\0');
    portEXIT_CRITICAL(&s_ctx.lock);
    return has_any;
}
