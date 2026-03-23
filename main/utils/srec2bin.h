#ifndef SREC2BIN_UTILS_H
#define SREC2BIN_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <esp_err.h>

typedef size_t (*srec2bin_read_fn)(void *ctx, uint8_t *buf, size_t len);
typedef esp_err_t (*srec2bin_sink_fn)(uint32_t addr, const uint8_t *data, size_t len, void *ctx);

typedef struct {
    uint8_t *data;
    size_t size;
    uint32_t base_addr;
} srec2bin_result_t;

esp_err_t srec2bin_convert(srec2bin_read_fn read_fn, void *ctx, srec2bin_result_t *out);
esp_err_t srec2bin_convert_to_sink(srec2bin_read_fn read_fn, void *ctx,
                                   srec2bin_sink_fn sink, void *sink_ctx);
esp_err_t srec2bin_from_buffer(const uint8_t *data, size_t data_len, srec2bin_result_t *out);
esp_err_t srec2bin_file_to_file(FILE *in, FILE *out);
void srec2bin_free(srec2bin_result_t *out);

#endif
