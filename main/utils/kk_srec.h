/*
 * kk_srec.h: A simple library for reading Motorola SREC hex files.
 *
 * Copyright (c) 2015 Kimmo Kulovesi, http://arkku.com/
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#ifndef KK_SREC_H
#define KK_SREC_H

#define KK_SREC_VERSION "2015-03-06"

#include <stdint.h>

typedef uint_least32_t srec_address_t;
typedef int srec_count_t;
typedef uint_fast8_t srec_bool_t;

enum srec_record_number {
    SREC_HEADER             = 0,
    SREC_DATA_16BIT         = 1,
    SREC_DATA_24BIT         = 2,
    SREC_DATA_32BIT         = 3,
    SREC_COUNT_16BIT        = 5,
    SREC_COUNT_24BIT        = 6,
    SREC_TERMINATION_32BIT  = 7,
    SREC_TERMINATION_24BIT  = 8,
    SREC_TERMINATION_16BIT  = 9
};

typedef uint_fast8_t srec_record_number_t;

#ifndef SREC_LINE_MAX_BYTE_COUNT
#define SREC_LINE_MAX_BYTE_COUNT 37
#endif

typedef struct srec_state {
    uint8_t         flags;
    uint8_t         byte_count;
    uint8_t         length;
    uint8_t         data[SREC_LINE_MAX_BYTE_COUNT + 1];
} kk_srec_t;

void srec_begin_read(struct srec_state *srec);
void srec_read_byte(struct srec_state *srec, char chr);
void srec_read_bytes(struct srec_state * restrict srec,
                     const char * restrict data,
                     srec_count_t count);
void srec_end_read(struct srec_state *srec);

extern void srec_data_read(struct srec_state *srec,
                           srec_record_number_t record_type,
                           srec_address_t address,
                           uint8_t *data, srec_count_t length,
                           srec_bool_t checksum_error);

#define SREC_IS_HEADER(rnum)        (!(rnum))
#define SREC_IS_DATA(rnum)          ((rnum) && ((rnum) <= 3))
#define SREC_IS_TERMINATION(rnum)   ((rnum) >= 7)
#define SREC_IS_COUNT(rnum)         (((rnum) == 5) || ((rnum) == 6))
#define SREC_ADDRESS_BYTE_COUNT(rnum) (2 + ((((rnum) & 1) || !(rnum)) ? ((rnum) & 2) : 1))

#endif // !KK_SREC_H
