#ifndef HHT_3000_H
#define HHT_3000_H

#include <stdbool.h>

void hht_3000_init(void);
bool hht_3000_session_open(void);
void hht_3000_session_close(void);
void hht_3000_poll(void);

#endif
