#include "board_profile.h"

static const board_profile_t g_default_profile = {
    .name = "board_mcn8r8",
    .role = ROLE_NONE,
    .fw_targets = FW_TARGET_NONE,
};

const board_profile_t *board_get_profile(void) {
    return &g_default_profile;
}

