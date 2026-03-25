#ifndef HHT_KEYS_H
#define HHT_KEYS_H

#include <stdint.h>

#define HHT_KEY_NONE    0x00
#define HHT_KEY_ESC     0x01
#define HHT_KEY_UP      0x02
#define HHT_KEY_DN      0x04
#define HHT_KEY_ENT     0x08
#define HHT_KEY_ESC_ENT (HHT_KEY_ESC | HHT_KEY_ENT)
#define HHT_KEY_UP_ENT  (HHT_KEY_UP | HHT_KEY_ENT)

// BLE/CLI input key codes (fw_esp32 compatible)
#define HHT_KEY_CODE_NONE     0x00
#define HHT_KEY_CODE_ESC      0x01
#define HHT_KEY_CODE_UP       0x02
#define HHT_KEY_CODE_DN       0x03
#define HHT_KEY_CODE_ENT      0x04
#define HHT_KEY_CODE_ESC_ENT  0x05
#define HHT_KEY_CODE_UP_ENT   0x06
#define HHT_KEY_CODE_ESC_LONG 0x07
#define HHT_KEY_CODE_UP_LONG  0x08
#define HHT_KEY_CODE_DN_LONG  0x09
#define HHT_KEY_CODE_ENT_LONG 0x0a

void hht_keys_init(void);
int hht_keys_poll(void);
void hht_keys_send(uint8_t key);
void hht_keys_send_long(uint8_t key);

/*
 * Display change 이벤트 발생 시,
 * 진행 중인 short-key 전송(남은 반복)을 강제로 취소하고 release(0x00)를 즉시 보낸다.
 * long-key는 release 전까지 유지한다.
 */
void hht_keys_cancel_short_on_display_change(void);

#endif
