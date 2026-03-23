#ifndef HHT_MAIN_H
#define HHT_MAIN_H

typedef enum {
    HHT_TYPE_WB100 = 0,
    HHT_TYPE_3000,
    HHT_TYPE_100,
    HHT_TYPE_COUNT,
} hht_type_t;

void hht_set_type(hht_type_t type);
hht_type_t hht_get_type(void);

void hht_start(void);
void hht_stop(void);

#endif

