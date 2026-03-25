#ifndef BLE_CMD_PARSER_H
#define BLE_CMD_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include "board/board_profile.h"

#define CMD_READ_PHONE_ESP32                0x0001
#define CMD_WRITE_PHONE_ESP32               0x0002
#define CMD_HANDLE_PHONE_ESP32              0x0005
#define CMD_SETTING_PHONE_ESP32             0x0010
#define CMD_INIT_PHONE_ESP32                0x0015

#define CMD_PROGRAM_ENTER_WIFI_ESP32        0x0040
#define ACK_PROGRAM_ENTER_WIFI_ESP32        0x8040
#define CMD_PROGRAM_ENTER_PHONE_ESP32       0x0050
#define ACK_PROGRAM_ENTER_PHONE_ESP32       0x8050
#define CMD_PROGRAM_WRITE_PHONE_ESP32       0x0051
#define ACK_PROGRAM_WRITE_PHONE_ESP32       0x8051
#define CMD_PROGRAM_INTERFACE_RESET_PHONE_ESP32 0x0052
#define ACK_PROGRAM_INTERFACE_RESET_PHONE_ESP32 0x8052
#define CMD_PROGRAM_STOP_PHONE_ESP32        0x0053
#define ACK_PROGRAM_STOP_PHONE_ESP32        0x8053
#define CMD_PROGRAM_START_PHONE_ESP32       0x0054
#define ACK_PROGRAM_START_PHONE_ESP32       0x8054
#define CMD_UPDATE_TYPE_PHONE_ESP32         0x0055
#define ACK_PROGRAM_STATUS_PHONE_ESP32      0x8056
#define CMD_PROGRAM_PROGRESS_MSG            0x0057
#define ACK_PROGRAM_PROGRESS_MSG_ESP32      0x8057

#define CMD_START_HHT_100                   0x0061
#define ACK_START_HHT_100                   0x8061
#define CMD_STOP_HHT_100                    0x0062
#define ACK_STOP_HHT_100                    0x8062
 //   CMD_HHT_MSG_100     = 0x0063,
 //   ACK_HHT_MSG_100     = 0x8063,

//#define CMD_HHT_MSG_100                     0x0063
#define CMD_HHT_MSG_100                     0x8063
#define ACK_HHT_MSG_100                     0x0063

#define CMD_HHT_BTN                         0x0064
#define ACK_HHT_BTN                         0x8064
#define CMD_HHT_PARAMS                      0x0065
#define CMD_HHT_PROJ_100                    0x0066
#define CMD_HHT_VER_CP_100                  0x0067
#define CMD_HHT_VER_INV_100                 0x0068
#define CMD_HHT_PRAMS_INV                   0x0069

#define CMD_ESP32_ADV_RENAME                0x0071
#define ACK_CMD_ESP32_ADV_RENAME            0x8071
#define CMD_ESP32_VERSION_INFO              0x0072
#define ACK_CMD_ESP32_VERSION_INFO          0x0072//0x8072
#define CMD_MCU_VERSION_INFO                0x0073
#define ACK_CMD_MCU_VERSION_INFO            0x0073//0x8073
#define CMD_MIRI_DIAGNOSIS_REQ              0x0074
#define CMD_MAC_BT_INFO                     0x0075
#define ACK_MAC_BT_INFO                     0x8075

#define CMD_MIRI_REBOOT                     0x0100
#define ACK_MIRI_REBOOT                     0x8100
#define CMD_LOG_DEVICE                      0x0101
#define CMD_UI_DEVICE                       0x0102
#define CMD_UPDATE_INFO_MOBILE              0x0103
#define ACK_UPDATE_INFO_MOBILE              0x8103
#define CMD_SDMMC_PROGRAM_ENTER             0x0104
#define ACK_SDMMC_PROGRAM_ENTER             0x8104
#define CMD_SDMMC_PROGRAM_LIST              0x0105
#define ACK_SDMMC_PROGRAM_LIST              0x8105
#define CMD_FAT_FILE_LIST                   0x0106
#define ACK_FAT_FILE_LIST                   0x8106
#define CMD_SDMMC_FORMAT                    0x0107
#define ACK_SDMMC_FORMAT                    0x8107
#define CMD_FAT_FORMAT                      0x0108
#define ACK_FAT_FORMAT                      0x8108
#define CMD_SDMMC_DELETE_FILE               0x0109
#define ACK_SDMMC_DELETE_FILE               0x8109
#define CMD_FAT_DELETE_FILE                 0x0110
#define ACK_FAT_DELETE_FILE                 0x8110

#define CMD_HHT_FDC                         0x0121

#define CMD_START_HHT_WB100                 0x0201
#define CMD_STOP_HHT_WB100                  0x0202
#define CMD_START_HHT_3000                  0x0203
#define CMD_STOP_HHT_3000                   0x0204
#define ACK_START_HHT_WB100                 0x8201
#define ACK_STOP_HHT_WB100                  0x8202
#define ACK_START_HHT_3000                  0x8203
#define ACK_STOP_HHT_3000                   0x8204

#define CMD_FW_UPLOAD                       0x0600
#define CMD_FW_UPLOAD_STOP                  0x0601
#define ACK_FW_UPLOAD_STOP                  0x8601

role_t ble_cmd_to_role(uint16_t cmd);
void ble_cmd_handle(uint16_t cmd);
void ble_cmd_handle_with_payload(uint16_t cmd, const uint8_t *payload, size_t payload_len);

#endif
