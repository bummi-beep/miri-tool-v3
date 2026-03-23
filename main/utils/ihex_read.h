/*
 * kk_ihex_read.h: A simple library for reading Intel HEX data.
 *
 * Copyright (c) 2013-2019 Kimmo Kulovesi, https://arkku.com/
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#ifndef KK_IHEX_READ_H
#define KK_IHEX_READ_H

#ifdef __cplusplus
#ifndef restrict
#define restrict
#endif
extern "C" {
#endif

#include "ihex.h"

void ihex_begin_read(struct ihex_state *ihex);
void ihex_read_at_address(struct ihex_state *ihex, ihex_address_t address);
void ihex_read_byte(struct ihex_state *ihex, char chr);
void ihex_read_bytes(struct ihex_state * restrict ihex,
                     const char * restrict data,
                     ihex_count_t count);
void ihex_end_read(struct ihex_state *ihex);

extern ihex_bool_t ihex_data_read(struct ihex_state *ihex,
                                  ihex_record_type_t type,
                                  ihex_bool_t checksum_mismatch);

#ifndef IHEX_DISABLE_SEGMENTS
void ihex_read_at_segment(struct ihex_state *ihex, ihex_segment_t segment);
#endif

#ifdef __cplusplus
}
#endif
#endif // !KK_IHEX_READ_H
/*
 * kk_ihex_read.h: A simple library for reading Intel HEX data.
 *
 * Copyright (c) 2013-2019 Kimmo Kulovesi, https://arkku.com/
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#ifndef KK_IHEX_READ_H
#define KK_IHEX_READ_H

#ifdef __cplusplus
#ifndef restrict
#define restrict
#endif
extern "C" {
#endif

#include "utils/ihex.h"

void ihex_begin_read(struct ihex_state *ihex);
void ihex_read_at_address(struct ihex_state *ihex, ihex_address_t address);
void ihex_read_byte(struct ihex_state *ihex, char chr);
void ihex_read_bytes(struct ihex_state *restrict ihex,
                     const char *restrict data,
                     ihex_count_t count);
void ihex_end_read(struct ihex_state *ihex);

extern ihex_bool_t ihex_data_read(struct ihex_state *ihex,
                                  ihex_record_type_t type,
                                  ihex_bool_t checksum_mismatch);

#ifndef IHEX_DISABLE_SEGMENTS
void ihex_read_at_segment(struct ihex_state *ihex, ihex_segment_t segment);
#endif

#ifdef __cplusplus
}
#endif
#endif
