#include "ihex2bin.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_err.h>

#include "ihex_read.h"
#include "ihex.h"

static const char *TAG = "ihex2bin";

typedef struct {
    uint8_t *data;
    size_t size;
    uint32_t base_addr;
    bool has_base;
    bool has_error;
    bool use_sink;
    ihex2bin_sink_fn sink;
    void *sink_ctx;
} ihex2bin_ctx_t;

static ihex2bin_ctx_t *s_ctx = NULL;

static esp_err_t ihex2bin_expand_front(ihex2bin_ctx_t *ctx, uint32_t new_base) {
    if (!ctx || !ctx->data) {
        return ESP_ERR_INVALID_STATE;
    }
    if (new_base >= ctx->base_addr) {
        return ESP_OK;
    }
    uint32_t delta = ctx->base_addr - new_base;
    size_t new_size = ctx->size + (size_t)delta;
    uint8_t *new_buf = (uint8_t *)realloc(ctx->data, new_size);
    if (!new_buf) {
        return ESP_ERR_NO_MEM;
    }
    memmove(new_buf + delta, new_buf, ctx->size);
    memset(new_buf, 0x00, delta);
    ctx->data = new_buf;
    ctx->size = new_size;
    ctx->base_addr = new_base;
    return ESP_OK;
}

static esp_err_t ihex2bin_ensure_capacity(ihex2bin_ctx_t *ctx, size_t required) {
    if (required <= ctx->size) {
        return ESP_OK;
    }
    uint8_t *new_buf = (uint8_t *)realloc(ctx->data, required);
    if (!new_buf) {
        return ESP_ERR_NO_MEM;
    }
    memset(new_buf + ctx->size, 0x00, required - ctx->size);
    ctx->data = new_buf;
    ctx->size = required;
    return ESP_OK;
}

ihex_bool_t ihex_data_read(struct ihex_state *ihex,
                           ihex_record_type_t type,
                           ihex_bool_t checksum_mismatch) {
    if (!s_ctx || checksum_mismatch) {
        if (checksum_mismatch) {
            ESP_LOGW(TAG, "checksum mismatch");
        }
        if (s_ctx) {
            s_ctx->has_error = true;
        }
        return false;
    }
    if (ihex->length < ihex->line_length) {
        ESP_LOGW(TAG, "line length error");
        s_ctx->has_error = true;
        return false;
    }
    if (type == IHEX_DATA_RECORD) {
        uint32_t address = (uint32_t)IHEX_LINEAR_ADDRESS(ihex);
        if (!s_ctx->has_base) {
            s_ctx->base_addr = address;
            s_ctx->has_base = true;
        } else if (address < s_ctx->base_addr) {
            if (s_ctx->use_sink) {
                ESP_LOGW(TAG, "address underflow for streaming output");
                s_ctx->has_error = true;
                return false;
            }
            if (ihex2bin_expand_front(s_ctx, address) != ESP_OK) {
                s_ctx->has_error = true;
                return false;
            }
        }
        size_t offset = (size_t)(address - s_ctx->base_addr);
        if (s_ctx->use_sink) {
            if (!s_ctx->sink || s_ctx->sink(offset, ihex->data, ihex->length, s_ctx->sink_ctx) != ESP_OK) {
                s_ctx->has_error = true;
                return false;
            }
            return true;
        }
        size_t end = offset + (size_t)ihex->length;
        if (ihex2bin_ensure_capacity(s_ctx, end) != ESP_OK) {
            s_ctx->has_error = true;
            return false;
        }
        memcpy(s_ctx->data + offset, ihex->data, ihex->length);
    }
    return true;
}

esp_err_t ihex2bin_convert(ihex2bin_read_fn read_fn, void *ctx, ihex2bin_result_t *out) {
    if (!read_fn || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    ihex2bin_ctx_t conv = {
        .data = NULL,
        .size = 0,
        .base_addr = 0,
        .has_base = false,
        .has_error = false,
        .use_sink = false,
        .sink = NULL,
        .sink_ctx = NULL,
    };

    s_ctx = &conv;
    struct ihex_state ihex;
    ihex_read_at_address(&ihex, 0);

    uint8_t buf[256];
    while (1) {
        size_t rd = read_fn(ctx, buf, sizeof(buf));
        if (rd == 0) {
            break;
        }
        ihex_read_bytes(&ihex, (const char *)buf, (ihex_count_t)rd);
    }
    ihex_end_read(&ihex);
    s_ctx = NULL;

    if (conv.has_error) {
        free(conv.data);
        return ESP_FAIL;
    }
    if (!conv.has_base || conv.size == 0) {
        free(conv.data);
        return ESP_ERR_INVALID_SIZE;
    }

    out->data = conv.data;
    out->size = conv.size;
    out->base_addr = conv.base_addr;
    return ESP_OK;
}

esp_err_t ihex2bin_convert_to_sink(ihex2bin_read_fn read_fn, void *ctx,
                                   ihex2bin_sink_fn sink, void *sink_ctx) {
    if (!read_fn || !sink) {
        return ESP_ERR_INVALID_ARG;
    }
    ihex2bin_ctx_t conv = {
        .data = NULL,
        .size = 0,
        .base_addr = 0,
        .has_base = false,
        .has_error = false,
        .use_sink = true,
        .sink = sink,
        .sink_ctx = sink_ctx,
    };

    s_ctx = &conv;
    struct ihex_state ihex;
    ihex_read_at_address(&ihex, 0);

    uint8_t buf[256];
    while (1) {
        size_t rd = read_fn(ctx, buf, sizeof(buf));
        if (rd == 0) {
            break;
        }
        ihex_read_bytes(&ihex, (const char *)buf, (ihex_count_t)rd);
    }
    ihex_end_read(&ihex);
    s_ctx = NULL;

    return conv.has_error ? ESP_FAIL : ESP_OK;
}

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
} ihex2bin_mem_reader_t;

static size_t ihex2bin_mem_read(void *ctx, uint8_t *buf, size_t len) {
    ihex2bin_mem_reader_t *reader = (ihex2bin_mem_reader_t *)ctx;
    if (!reader || reader->offset >= reader->size) {
        return 0;
    }
    size_t remaining = reader->size - reader->offset;
    size_t to_copy = remaining < len ? remaining : len;
    memcpy(buf, reader->data + reader->offset, to_copy);
    reader->offset += to_copy;
    return to_copy;
}

esp_err_t ihex2bin_from_buffer(const uint8_t *data, size_t data_len, ihex2bin_result_t *out) {
    if (!data || data_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    ihex2bin_mem_reader_t reader = {
        .data = data,
        .size = data_len,
        .offset = 0,
    };
    return ihex2bin_convert(ihex2bin_mem_read, &reader, out);
}

typedef struct {
    FILE *file;
} ihex2bin_file_reader_t;

static size_t ihex2bin_file_read(void *ctx, uint8_t *buf, size_t len) {
    ihex2bin_file_reader_t *reader = (ihex2bin_file_reader_t *)ctx;
    if (!reader || !reader->file) {
        return 0;
    }
    return fread(buf, 1, len, reader->file);
}

typedef struct {
    FILE *file;
} ihex2bin_file_sink_t;

static esp_err_t ihex2bin_file_sink(uint32_t addr, const uint8_t *data, size_t len, void *ctx) {
    ihex2bin_file_sink_t *sink = (ihex2bin_file_sink_t *)ctx;
    if (!sink || !sink->file || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (fseek(sink->file, (long)addr, SEEK_SET) != 0) {
        return ESP_FAIL;
    }
    size_t wr = fwrite(data, 1, len, sink->file);
    return (wr == len) ? ESP_OK : ESP_FAIL;
}

esp_err_t ihex2bin_file_to_file(FILE *in, FILE *out) {
    if (!in || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    ihex2bin_file_reader_t reader = {.file = in};
    ihex2bin_file_sink_t sink = {.file = out};
    return ihex2bin_convert_to_sink(ihex2bin_file_read, &reader, ihex2bin_file_sink, &sink);
}

void ihex2bin_free(ihex2bin_result_t *out) {
    if (!out) {
        return;
    }
    free(out->data);
    out->data = NULL;
    out->size = 0;
    out->base_addr = 0;
}
