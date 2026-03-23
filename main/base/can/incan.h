#ifndef INCAN_H
#define INCAN_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include <driver/twai.h>

esp_err_t incan_open(uint32_t bps, uint32_t filter_id);
void incan_stop(void);
esp_err_t incan_tx(uint32_t id, uint8_t extd, uint8_t dlc, const uint8_t *buff, TickType_t timeout);
bool incan_get_message(twai_message_t *pmessage, uint16_t time_ms);

#endif
