#ifndef ESP32_UPDATE_H
#define ESP32_UPDATE_H

#include <stdbool.h>

void esp32_update_start(void);
bool esp32_update_set_file(const char *name, const char *type);
bool esp32_update_is_ready(void);
void esp32_update_set_skip_reboot(bool skip);
bool esp32_update_get_skip_reboot(void);

#endif
