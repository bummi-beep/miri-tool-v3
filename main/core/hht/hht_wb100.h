#ifndef HHT_WB100_H
#define HHT_WB100_H

#include "core/hht/hht_session.h"

void hht_wb100_init(void);
/** 8바이트 핸드셰이크. OK / TIMEOUT / PROTOCOL / UART */
hht_session_result_t hht_wb100_session_open(void);
/**
 * 8바이트 검증 성공 후 2.08과 동일: BLE 부착(0x8201/0x8203) → UART OK·코드·프로젝트 → CMD_HHT_PROJ_100(0x0066).
 * Nimble 비활성 시 UART 단계만 수행.
 */
void hht_wb100_post_verify_workflow(void);
void hht_wb100_session_close(void);
void hht_wb100_poll(void);

#endif
