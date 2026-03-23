/*
 * kk_ihex.h: A simple library for reading and writing the Intel HEX
 * or IHEX format. Intended mainly for embedded systems, and thus
 * somewhat optimised for size at the expense of error handling and
 * generality.
 *
 * Copyright (c) 2013-2019 Kimmo Kulovesi, https://arkku.com/
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#ifndef KK_IHEX_H
#define KK_IHEX_H

#define KK_IHEX_VERSION "2019-08-07"

#include <stdint.h>

#ifdef IHEX_USE_STDBOOL
#include <stdbool.h>
typedef bool ihex_bool_t;
#else
typedef uint_fast8_t ihex_bool_t;
#endif

typedef uint_least32_t ihex_address_t;
typedef uint_least16_t ihex_segment_t;
typedef int ihex_count_t;

#ifndef IHEX_LINE_MAX_LENGTH
#define IHEX_LINE_MAX_LENGTH 255
#endif

enum ihex_flags {
    IHEX_FLAG_ADDRESS_OVERFLOW = 0x80
};
typedef uint8_t ihex_flags_t;

typedef struct ihex_state {
    ihex_address_t  address;
#ifndef IHEX_DISABLE_SEGMENTS
    ihex_segment_t  segment;
#endif
    ihex_flags_t    flags;
    uint8_t         line_length;
    uint8_t         length;
    uint8_t         data[IHEX_LINE_MAX_LENGTH + 1];
} kk_ihex_t;

enum ihex_record_type {
    IHEX_DATA_RECORD,
    IHEX_END_OF_FILE_RECORD,
    IHEX_EXTENDED_SEGMENT_ADDRESS_RECORD,
    IHEX_START_SEGMENT_ADDRESS_RECORD,
    IHEX_EXTENDED_LINEAR_ADDRESS_RECORD,
    IHEX_START_LINEAR_ADDRESS_RECORD
};
typedef uint8_t ihex_record_type_t;

#ifndef IHEX_DISABLE_SEGMENTS
#define IHEX_LINEAR_ADDRESS(ihex) (((ihex)->address + (((ihex_address_t)((ihex)->segment)) << 4)) & 0xFFFFFF)
#define IHEX_BYTE_ADDRESS(ihex, byte_index) ((((ihex)->address + (byte_index)) & 0xFFFFU) + (((ihex_address_t)((ihex)->segment)) << 4))
#else
#define IHEX_LINEAR_ADDRESS(ihex) ((ihex)->address)
#define IHEX_BYTE_ADDRESS(ihex, byte_index) ((ihex)->address + (byte_index))
#endif

#ifndef IHEX_NEWLINE_STRING
#define IHEX_NEWLINE_STRING "\n"
#endif

#endif // !KK_IHEX_H
/*
 * kk_ihex.h: A simple library for reading and writing the Intel HEX
 * or IHEX format. Intended mainly for embedded systems, and thus
 * somewhat optimised for size at the expense of error handling and
 * generality.
 *
 * Copyright (c) 2013-2019 Kimmo Kulovesi, https://arkku.com/
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#ifndef KK_IHEX_H
#define KK_IHEX_H

#define KK_IHEX_VERSION "2019-08-07"

#include <stdint.h>

#ifdef IHEX_USE_STDBOOL
#include <stdbool.h>
typedef bool ihex_bool_t;
#else
typedef uint_fast8_t ihex_bool_t;
#endif

typedef uint_least32_t ihex_address_t;
typedef uint_least16_t ihex_segment_t;
typedef int ihex_count_t;

#ifndef IHEX_LINE_MAX_LENGTH
#define IHEX_LINE_MAX_LENGTH 255
#endif

enum ihex_flags {
    IHEX_FLAG_ADDRESS_OVERFLOW = 0x80
};
typedef uint8_t ihex_flags_t;

typedef struct ihex_state {
    ihex_address_t  address;
#ifndef IHEX_DISABLE_SEGMENTS
    ihex_segment_t  segment;
#endif
    ihex_flags_t    flags;
    uint8_t         line_length;
    uint8_t         length;
    uint8_t         data[IHEX_LINE_MAX_LENGTH + 1];
} kk_ihex_t;

enum ihex_record_type {
    IHEX_DATA_RECORD,
    IHEX_END_OF_FILE_RECORD,
    IHEX_EXTENDED_SEGMENT_ADDRESS_RECORD,
    IHEX_START_SEGMENT_ADDRESS_RECORD,
    IHEX_EXTENDED_LINEAR_ADDRESS_RECORD,
    IHEX_START_LINEAR_ADDRESS_RECORD
};
typedef uint8_t ihex_record_type_t;

#ifndef IHEX_DISABLE_SEGMENTS
#define IHEX_LINEAR_ADDRESS(ihex) (((ihex)->address + (((ihex_address_t)((ihex)->segment)) << 4)) & 0xFFFFFF)
#define IHEX_BYTE_ADDRESS(ihex, byte_index) ((((ihex)->address + (byte_index)) & 0xFFFFU) + (((ihex_address_t)((ihex)->segment)) << 4))
#else
#define IHEX_LINEAR_ADDRESS(ihex) ((ihex)->address)
#define IHEX_BYTE_ADDRESS(ihex, byte_index) ((ihex)->address + (byte_index))
#endif

#ifndef IHEX_NEWLINE_STRING
#define IHEX_NEWLINE_STRING "\n"
#endif

#endif
