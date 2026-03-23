#include "fw_upload_packaging.h"

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <esp_log.h>

static const char *TAG = "fw_packaging";

#define COFF_C28X_ID 0x9D
#define COFF_VERSION_0 0xC0
#define COFF_VERSION_1 0xC1
#define COFF_VERSION_2 0xC2

#define STYP_DSECT 0x0001
#define STYP_NOLOAD 0x0002
#define STYP_PAD 0x0008
#define STYP_COPY 0x0010
#define STYP_BSS 0x0080

#define ELF_MAGIC0 0x7f
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'
#define ELF_CLASS_32 1
#define ELF_DATA_LSB 1
#define PT_LOAD 1

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

typedef struct {
    char buf[256];
    size_t len;
    bool eof;
} line_reader_t;

static bool read_line(line_reader_t *lr, fw_storage_reader_t *reader) {
    if (!lr || !reader || lr->eof) {
        return false;
    }
    lr->len = 0;
    while (lr->len < sizeof(lr->buf) - 1) {
        uint8_t ch;
        size_t rd = fw_storage_reader_read(reader, &ch, 1);
        if (rd == 0) {
            lr->eof = true;
            break;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            break;
        }
        lr->buf[lr->len++] = (char)ch;
    }
    lr->buf[lr->len] = '\0';
    return lr->len > 0;
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static bool hex_byte(const char *s, uint8_t *out) {
    int hi = hex_nibble(s[0]);
    int lo = hex_nibble(s[1]);
    if (hi < 0 || lo < 0) {
        return false;
    }
    *out = (uint8_t)((hi << 4) | lo);
    return true;
}

static esp_err_t process_bin(fw_storage_reader_t *reader, fw_packaging_sink_t sink, void *ctx) {
    uint8_t buf[512];
    uint32_t addr = 0;
    while (1) {
        size_t rd = fw_storage_reader_read(reader, buf, sizeof(buf));
        if (rd == 0) {
            break;
        }
        esp_err_t ret = sink(addr, buf, rd, ctx);
        if (ret != ESP_OK) {
            return ret;
        }
        addr += (uint32_t)rd;
    }
    return ESP_OK;
}

static esp_err_t process_ihex(fw_storage_reader_t *reader, fw_packaging_sink_t sink, void *ctx) {
    line_reader_t lr = {.len = 0, .eof = false};
    uint32_t base_addr = 0;
    while (read_line(&lr, reader)) {
        if (lr.buf[0] == '\0') {
            continue;
        }
        if (lr.buf[0] != ':') {
            ESP_LOGW(TAG, "ihex invalid line");
            return ESP_FAIL;
        }
        size_t line_len = strlen(lr.buf);
        if (line_len < 11) {
            ESP_LOGW(TAG, "ihex short line");
            return ESP_FAIL;
        }
        uint8_t byte_count = 0;
        uint8_t addr_hi = 0;
        uint8_t addr_lo = 0;
        uint8_t rec_type = 0;
        if (!hex_byte(&lr.buf[1], &byte_count) ||
            !hex_byte(&lr.buf[3], &addr_hi) ||
            !hex_byte(&lr.buf[5], &addr_lo) ||
            !hex_byte(&lr.buf[7], &rec_type)) {
            return ESP_FAIL;
        }
        uint16_t addr = (uint16_t)((addr_hi << 8) | addr_lo);
        size_t data_chars = (size_t)byte_count * 2;
        if (line_len < 9 + data_chars + 2) {
            return ESP_FAIL;
        }
        uint8_t checksum = 0;
        for (size_t i = 1; i + 1 < line_len; i += 2) {
            uint8_t b = 0;
            if (!hex_byte(&lr.buf[i], &b)) {
                return ESP_FAIL;
            }
            checksum = (uint8_t)(checksum + b);
        }
        if (checksum != 0) {
            ESP_LOGW(TAG, "ihex checksum fail");
            return ESP_FAIL;
        }
        if (rec_type == 0x00) {
            uint8_t data[256];
            for (size_t i = 0; i < byte_count; i++) {
                if (!hex_byte(&lr.buf[9 + i * 2], &data[i])) {
                    return ESP_FAIL;
                }
            }
            uint32_t out_addr = base_addr + addr;
            esp_err_t ret = sink(out_addr, data, byte_count, ctx);
            if (ret != ESP_OK) {
                return ret;
            }
        } else if (rec_type == 0x01) {
            break;
        } else if (rec_type == 0x02) {
            uint8_t msb = 0, lsb = 0;
            if (!hex_byte(&lr.buf[9], &msb) || !hex_byte(&lr.buf[11], &lsb)) {
                return ESP_FAIL;
            }
            base_addr = (uint32_t)(((msb << 8) | lsb) << 4);
        } else if (rec_type == 0x04) {
            uint8_t msb = 0, lsb = 0;
            if (!hex_byte(&lr.buf[9], &msb) || !hex_byte(&lr.buf[11], &lsb)) {
                return ESP_FAIL;
            }
            base_addr = (uint32_t)(((msb << 8) | lsb) << 16);
        } else {
            continue;
        }
    }
    return ESP_OK;
}

static esp_err_t process_coff(fw_storage_reader_t *reader, fw_packaging_sink_t sink, void *ctx) {
    size_t size = reader->meta.original_size;
    if (size == 0) {
        ESP_LOGW(TAG, "coff size unknown");
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t *buf = (uint8_t *)malloc(size);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    size_t read_total = 0;
    while (read_total < size) {
        size_t rd = fw_storage_reader_read(reader, buf + read_total, size - read_total);
        if (rd == 0) {
            break;
        }
        read_total += rd;
    }
    if (read_total != size) {
        free(buf);
        return ESP_FAIL;
    }

    uint16_t version = le16(buf);
    uint16_t section_count = 0;
    uint16_t opt_header_bytes = 0;
    size_t header_size = 0;
    size_t section_header_size = 0;

    if (version == COFF_VERSION_1 || version == COFF_VERSION_2) {
        section_count = le16(buf + 2);
        opt_header_bytes = le16(buf + 16);
        header_size = 22;
        section_header_size = (version == COFF_VERSION_2) ? 48 : 40;
        uint16_t target_id = le16(buf + 20);
        if (target_id != COFF_C28X_ID) {
            ESP_LOGW(TAG, "coff target id mismatch: 0x%04x", target_id);
        }
    } else if (version == COFF_C28X_ID) {
        version = COFF_VERSION_0;
        section_count = le16(buf + 2);
        opt_header_bytes = le16(buf + 16);
        header_size = 20;
        section_header_size = 40;
    } else {
        free(buf);
        ESP_LOGW(TAG, "coff unsupported version: 0x%04x", version);
        return ESP_ERR_NOT_SUPPORTED;
    }

    size_t section_offset = header_size + opt_header_bytes;
    if (section_offset + section_header_size * (size_t)section_count > size) {
        free(buf);
        ESP_LOGW(TAG, "coff section table out of range");
        return ESP_FAIL;
    }

    for (uint16_t i = 0; i < section_count; i++) {
        const uint8_t *sh = buf + section_offset + (size_t)i * section_header_size;
        uint32_t phys_addr = le32(sh + 8);
        uint32_t sec_size = le32(sh + 16);
        uint32_t raw_ptr = le32(sh + 20);
        uint32_t flags = 0;
        if (version == COFF_VERSION_2) {
            flags = le32(sh + 40);
            sec_size *= 2; /* word -> byte */
        } else {
            flags = le16(sh + 36);
        }
        if (sec_size == 0 || raw_ptr == 0) {
            continue;
        }
        if (flags & (STYP_DSECT | STYP_NOLOAD | STYP_PAD | STYP_COPY | STYP_BSS)) {
            continue;
        }
        if ((size_t)raw_ptr + sec_size > size) {
            ESP_LOGW(TAG, "coff section %u out of range", (unsigned)i);
            free(buf);
            return ESP_FAIL;
        }
        const uint8_t *data = buf + raw_ptr;
        size_t remaining = sec_size;
        uint32_t addr = phys_addr;
        while (remaining > 0) {
            size_t chunk = remaining > 1024 ? 1024 : remaining;
            esp_err_t ret = sink(addr, data, chunk, ctx);
            if (ret != ESP_OK) {
                free(buf);
                return ret;
            }
            data += chunk;
            addr += (uint32_t)chunk;
            remaining -= chunk;
        }
    }

    free(buf);
    return ESP_OK;
}

static esp_err_t process_elf(fw_storage_reader_t *reader, fw_packaging_sink_t sink, void *ctx) {
    size_t size = reader->meta.original_size;
    if (size < 52) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t *buf = (uint8_t *)malloc(size);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    size_t read_total = 0;
    while (read_total < size) {
        size_t rd = fw_storage_reader_read(reader, buf + read_total, size - read_total);
        if (rd == 0) {
            break;
        }
        read_total += rd;
    }
    if (read_total != size) {
        free(buf);
        return ESP_FAIL;
    }

    if (buf[0] != ELF_MAGIC0 || buf[1] != ELF_MAGIC1 ||
        buf[2] != ELF_MAGIC2 || buf[3] != ELF_MAGIC3) {
        free(buf);
        ESP_LOGW(TAG, "elf magic mismatch");
        return ESP_ERR_INVALID_ARG;
    }
    if (buf[4] != ELF_CLASS_32 || buf[5] != ELF_DATA_LSB) {
        free(buf);
        ESP_LOGW(TAG, "elf unsupported class/data");
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint16_t phentsize = le16(buf + 42);
    uint16_t phnum = le16(buf + 44);
    uint32_t phoff = le32(buf + 28);
    if (phoff == 0 || phentsize == 0 || phnum == 0) {
        free(buf);
        return ESP_ERR_INVALID_SIZE;
    }
    if ((size_t)phoff + (size_t)phentsize * phnum > size) {
        free(buf);
        ESP_LOGW(TAG, "elf program header out of range");
        return ESP_FAIL;
    }

    for (uint16_t i = 0; i < phnum; i++) {
        const uint8_t *ph = buf + phoff + (size_t)i * phentsize;
        uint32_t p_type = le32(ph + 0);
        if (p_type != PT_LOAD) {
            continue;
        }
        uint32_t p_offset = le32(ph + 4);
        uint32_t p_paddr = le32(ph + 12);
        uint32_t p_filesz = le32(ph + 16);
        if (p_filesz == 0) {
            continue;
        }
        if ((size_t)p_offset + p_filesz > size) {
            free(buf);
            ESP_LOGW(TAG, "elf segment out of range");
            return ESP_FAIL;
        }
        const uint8_t *data = buf + p_offset;
        size_t remaining = p_filesz;
        uint32_t addr = p_paddr;
        while (remaining > 0) {
            size_t chunk = remaining > 1024 ? 1024 : remaining;
            esp_err_t ret = sink(addr, data, chunk, ctx);
            if (ret != ESP_OK) {
                free(buf);
                return ret;
            }
            data += chunk;
            addr += (uint32_t)chunk;
            remaining -= chunk;
        }
    }

    free(buf);
    return ESP_OK;
}

esp_err_t fw_packaging_process(const fw_meta_t *meta, fw_storage_reader_t *reader,
                               fw_packaging_sink_t sink, void *ctx) {
    if (!meta || !reader || !sink) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (meta->format) {
        case FW_FMT_BIN:
            return process_bin(reader, sink, ctx);
        case FW_FMT_IHEX:
            return process_ihex(reader, sink, ctx);
        case FW_FMT_COFF:
            return process_coff(reader, sink, ctx);
        case FW_FMT_ELF:
            return process_elf(reader, sink, ctx);
        default:
            ESP_LOGW(TAG, "unknown format");
            return ESP_ERR_NOT_SUPPORTED;
    }
}
