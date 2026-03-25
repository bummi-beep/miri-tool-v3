/**
 * HHT 세션 공통 레이어: 타입별 UART/프로토콜은 wb100·3000 모듈에 두고,
 * open/close·타임아웃·에러 코드는 여기서 통일한다.
 */
#ifndef HHT_SESSION_H
#define HHT_SESSION_H

#include "core/hht/hht_main.h"

typedef enum {
    HHT_SESSION_OK = 0,
    HHT_SESSION_ERR_TIMEOUT,   /* 응답/바이트 수신 시간 초과 */
    HHT_SESSION_ERR_PROTOCOL,  /* CRC·길이·검증 실패 */
    HHT_SESSION_ERR_UART,        /* write 실패 등 */
    HHT_SESSION_ERR_UNSUPPORTED, /* 알 수 없는 HHT 타입 */
} hht_session_result_t;

/* --- 공통 정책 (타입별 구현에서 사용) --- */
#define HHT_SESSION_SETTLE_MS           50u   /* flush 후 버스 안정 */

/* WB100: 8바이트 응답 수신 */
#define HHT_SESSION_WB100_READ_TIMEOUT_MS   100u
#define HHT_SESSION_WB100_RETRY_MAX           5u
#define HHT_SESSION_WB100_RETRY_GAP_MS        50u

/* HHT-3000: 모드 전송 후 STM32 배너 수신 */
#define HHT_SESSION_3000_AFTER_MODE_MS      80u
#define HHT_SESSION_3000_DISCARD_MS         60u
#define HHT_SESSION_3000_STOP_WAIT_MS       30u

/** 타입에 맞게 init + 세션 오픈. */
hht_session_result_t hht_session_open(hht_type_t type);

/** 타입에 맞게 세션 종료·UART 정리. */
void hht_session_close(hht_type_t type);

const char *hht_session_result_str(hht_session_result_t r);

#endif /* HHT_SESSION_H */
