#include "cli_menu.h"

#include <stdio.h>
#include <esp_log.h>

static const char *TAG = "cli_menu";
static cli_menu_t s_menu = CLI_MENU_MAIN;

void cli_menu_set(cli_menu_t menu) {
    s_menu = menu;
}

cli_menu_t cli_menu_get(void) {
    return s_menu;
}

static void menu_main(void) {
    printf("\r\n[MAIN]\r\n");
    printf("  A) 시스템/정보\r\n");
    printf("  D) 스토리지/파일\r\n");
    printf("  B) 업데이트\r\n");
    printf("  C) HHT\r\n");
    printf("  HELP) 전체 명령 목록\r\n");
}

static void menu_a(void) {
    printf("\r\n[A] 시스템/정보\r\n");
    printf("  VER      - 버전 정보\r\n");
    printf("  MAC      - BT MAC\r\n");
    printf("  BLENAME  - BLE 이름 변경\r\n");
    printf("  REBOOT   - 재부팅\r\n");
    printf("  BACK     - 메인으로\r\n");
}

static void menu_d(void) {
    printf("\r\n[D] 스토리지/파일\r\n");
    printf("  SDLS [ALL]      - SD 리스트\r\n");
    printf("  SDFORMAT        - SD 포맷\r\n");
    printf("  SDDEL <name> <type> - SD 파일 삭제\r\n");
    printf("  SDRUN <name36> <type> - SD 실행\r\n");
    printf("  FLASHLS         - 내장 플래시 리스트\r\n");
    printf("  FLASHFORMAT     - 내장 플래시 포맷\r\n");
    printf("  FLASHDEL <name> - 내장 플래시 파일 삭제\r\n");
    printf("  BACK            - 메인으로\r\n");
}

static void menu_b(void) {
    printf("\r\n[B] 업데이트\r\n");
    printf("  B1) OTA(ESP32) 펌웨어 업그레이드\r\n");
    printf("  B2) 타겟 펌웨어 업그레이드\r\n");
    printf("  BACK - 메인으로\r\n");
}

static void menu_c(void) {
    printf("\r\n[C] HHT\r\n");
    printf("  C1) WB100\r\n");
    printf("  C2) WB3000\r\n");
    printf("  BACK - 메인으로\r\n");
}

void cli_menu_render(void) {
    switch (s_menu) {
        case CLI_MENU_A:
            menu_a();
            break;
        case CLI_MENU_B:
            menu_b();
            break;
        case CLI_MENU_C:
            menu_c();
            break;
        case CLI_MENU_D:
            menu_d();
            break;
        case CLI_MENU_MAIN:
        default:
            menu_main();
            break;
    }
    ESP_LOGI(TAG, "menu=%d", (int)s_menu);
}

void cli_menu_show_main(void) {
    cli_menu_set(CLI_MENU_MAIN);
    cli_menu_render();
}
