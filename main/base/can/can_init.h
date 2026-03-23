#ifndef CAN_INIT_H
#define CAN_INIT_H

#include <stdbool.h>
#include <esp_err.h>

typedef enum {
    CAN_BITRATE_1M = 1000000,
    CAN_BITRATE_500K = 500000,
    CAN_BITRATE_250K = 250000,
    CAN_BITRATE_125K = 125000,
} can_bitrate_t;

esp_err_t can_init(can_bitrate_t bitrate);
void can_deinit(void);
bool can_is_ready(void);

#endif
