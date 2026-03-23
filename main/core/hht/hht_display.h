#ifndef HHT_DISPLAY_H
#define HHT_DISPLAY_H

#include <stdint.h>

void hht_display_init(void);
void hht_display_poll(void);
void hht_display_dump_raw(int enable);

/* 현재 HHT 타입 기준: 3000 → 2x16, WB100/100 → 4x20 */
uint8_t hht_display_get_rows(void);
uint8_t hht_display_get_cols(void);

#endif
