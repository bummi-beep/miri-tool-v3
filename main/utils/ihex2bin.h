#ifndef IHEX2BIN_UTILS_H
#define IHEX2BIN_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <esp_err.h>

typedef size_t (*ihex2bin_read_fn)(void *ctx, uint8_t *buf, size_t len);
typedef esp_err_t (*ihex2bin_sink_fn)(uint32_t addr, const uint8_t *data, size_t len, void *ctx);

typedef struct {
    uint8_t *data;
    size_t size;
    uint32_t base_addr;
} ihex2bin_result_t;

esp_err_t ihex2bin_convert(ihex2bin_read_fn read_fn, void *ctx, ihex2bin_result_t *out);
esp_err_t ihex2bin_convert_to_sink(ihex2bin_read_fn read_fn, void *ctx,
                                   ihex2bin_sink_fn sink, void *sink_ctx);
esp_err_t ihex2bin_from_buffer(const uint8_t *data, size_t data_len, ihex2bin_result_t *out);
esp_err_t ihex2bin_file_to_file(FILE *in, FILE *out);
void ihex2bin_free(ihex2bin_result_t *out);

#endif
#ifndef IHEX2BIN_H
#define IHEX2BIN_H

#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

typedef struct {
    uint8_t *data;
    size_t size;
    uint32_t base_addr;
} ihex_bin_image_t;

esp_err_t ihex_to_bin(const uint8_t *ihex, size_t ihex_len, ihex_bin_image_t *out);
void ihex_bin_free(ihex_bin_image_t *out);

#endif
