#ifndef BOARD_PROFILE_H
#define BOARD_PROFILE_H

#include <stdbool.h>

typedef enum {
    ROLE_NONE = 0,
    ROLE_HHT,
    ROLE_FW_UPLOAD,
    ROLE_STORAGE_UPLOAD,
    ROLE_DIAG,
    ROLE_MANUAL,
} role_t;

typedef enum {
    FW_TARGET_NONE   = 0,
    FW_TARGET_EZPORT = 1 << 0,
    FW_TARGET_STM    = 1 << 1,
    FW_TARGET_LPC    = 1 << 2,
    FW_TARGET_DSP    = 1 << 3,
    FW_TARGET_SWD    = 1 << 4,
} fw_target_mask_t;

typedef struct {
    const char *name;
    role_t role;
    fw_target_mask_t fw_targets;
} board_profile_t;

const board_profile_t *board_get_profile(void);

#endif

