#include "fw_upload_types.h"
#include <string.h>
#include <strings.h>

const char *fw_format_to_str(fw_format_t fmt) {
    switch (fmt) {
        case FW_FMT_BIN: return "BIN";
        case FW_FMT_IHEX: return "IHEX";
        case FW_FMT_COFF: return "COFF";
        case FW_FMT_ELF: return "ELF";
        default: return "UNKNOWN";
    }
}

fw_format_t fw_format_from_str(const char *str) {
    if (!str) {
        return FW_FMT_UNKNOWN;
    }
    if (strcasecmp(str, "BIN") == 0) return FW_FMT_BIN;
    if (strcasecmp(str, "IHEX") == 0) return FW_FMT_IHEX;
    if (strcasecmp(str, "HEX") == 0) return FW_FMT_IHEX;
    if (strcasecmp(str, "COFF") == 0) return FW_FMT_COFF;
    if (strcasecmp(str, "ELF") == 0) return FW_FMT_ELF;
    return FW_FMT_UNKNOWN;
}

const char *fw_exec_to_str(fw_exec_t exec) {
    switch (exec) {
        case FW_EXEC_ESP32_OTA: return "ESP32_OTA";
        case FW_EXEC_UART_ISP: return "UART_ISP";
        case FW_EXEC_CAN: return "CAN";
        case FW_EXEC_SWD: return "SWD";
        case FW_EXEC_JTAG: return "JTAG";
        case FW_EXEC_BOOTLOADER: return "BOOTLOADER";
        case FW_EXEC_STM32_UART: return "STM32_UART";
        default: return "UNKNOWN";
    }
}

fw_exec_t fw_exec_from_str(const char *str) {
    if (!str) {
        return FW_EXEC_UNKNOWN;
    }
    if (strcasecmp(str, "ESP32_OTA") == 0) return FW_EXEC_ESP32_OTA;
    if (strcasecmp(str, "UART_ISP") == 0) return FW_EXEC_UART_ISP;
    if (strcasecmp(str, "CAN") == 0) return FW_EXEC_CAN;
    if (strcasecmp(str, "SWD") == 0) return FW_EXEC_SWD;
    if (strcasecmp(str, "JTAG") == 0) return FW_EXEC_JTAG;
    if (strcasecmp(str, "BOOTLOADER") == 0) return FW_EXEC_BOOTLOADER;
    if (strcasecmp(str, "STM32_UART") == 0) return FW_EXEC_STM32_UART;
    return FW_EXEC_UNKNOWN;
}
