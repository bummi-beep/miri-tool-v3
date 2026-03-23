#include "fw_upload_registry.h"
#include <string.h>

static const fw_route_t k_fw_routes[] = {
    {"S00", FW_FMT_BIN,  FW_EXEC_ESP32_OTA,  FW_FMT_BIN},
    {"S01", FW_FMT_BIN,  FW_EXEC_STM32_UART, FW_FMT_BIN},
    {"A00", FW_FMT_UNKNOWN, FW_EXEC_UNKNOWN, FW_FMT_BIN},
    {"A01", FW_FMT_UNKNOWN, FW_EXEC_UNKNOWN, FW_FMT_BIN},
    {"A02", FW_FMT_COFF, FW_EXEC_UART_ISP, FW_FMT_BIN},
    {"A03", FW_FMT_IHEX, FW_EXEC_UART_ISP, FW_FMT_BIN},
    {"A04", FW_FMT_COFF, FW_EXEC_UART_ISP, FW_FMT_BIN},
    {"A05", FW_FMT_COFF, FW_EXEC_UART_ISP, FW_FMT_BIN},
    {"A06", FW_FMT_IHEX, FW_EXEC_SWD, FW_FMT_BIN},
    {"A07", FW_FMT_COFF, FW_EXEC_UART_ISP, FW_FMT_BIN},
    {"A08", FW_FMT_UNKNOWN, FW_EXEC_UNKNOWN, FW_FMT_BIN},
    {"A09", FW_FMT_UNKNOWN, FW_EXEC_UNKNOWN, FW_FMT_BIN},
    {"A10", FW_FMT_IHEX, FW_EXEC_UART_ISP, FW_FMT_BIN},
    {"A11", FW_FMT_IHEX, FW_EXEC_UART_ISP, FW_FMT_BIN},
    {"A12", FW_FMT_IHEX, FW_EXEC_UART_ISP, FW_FMT_BIN},
    {"A13", FW_FMT_IHEX, FW_EXEC_UART_ISP, FW_FMT_BIN},
    {"A14", FW_FMT_IHEX, FW_EXEC_UART_ISP, FW_FMT_BIN},
    {"A15", FW_FMT_IHEX, FW_EXEC_SWD, FW_FMT_BIN},
};

bool fw_registry_lookup(const char *file_type, fw_route_t *out) {
    if (!file_type || !out) {
        return false;
    }
    for (size_t i = 0; i < sizeof(k_fw_routes) / sizeof(k_fw_routes[0]); i++) {
        if (strncmp(file_type, k_fw_routes[i].file_type, 3) == 0) {
            *out = k_fw_routes[i];
            return true;
        }
    }
    return false;
}
