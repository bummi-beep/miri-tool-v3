#ifndef HHT_DISPLAY_H
#define HHT_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void hht_display_init(void);
void hht_display_poll(void);
void hht_display_dump_raw(int enable);

/* 현재 HHT 타입 기준: 3000 → 2x16, WB100/100 → 4x20 */
uint8_t hht_display_get_rows(void);
uint8_t hht_display_get_cols(void);

/**
 * HHT-3000(UART Report) 등에서 한 줄을 확정 반영하고 WB100과 동일하게 BLE·CLI에 통지.
 * @param force_ble true 이면 내용이 이전과 같아도 BLE notify 재전송(세션 직후 초기 동기화용).
 */
void hht_display_push_row_ascii(uint8_t row, const char *src, size_t src_len, bool force_ble);

#endif
