#ifndef FW_UPLOAD_STATE_H
#define FW_UPLOAD_STATE_H

#include <stdint.h>
#include "core/firmware_upload/fw_upload_types.h"

void fw_state_set_step(fw_step_t step, const char *msg);
void fw_state_set_progress(uint8_t percent);
fw_step_t fw_state_get_step(void);
uint8_t fw_state_get_progress(void);
const char *fw_state_get_message(void);

#endif
