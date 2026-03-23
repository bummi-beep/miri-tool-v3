#ifndef FW_UPLOAD_STORAGE_H
#define FW_UPLOAD_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <esp_err.h>

#include "core/firmware_upload/fw_upload_types.h"

/* SDMMC 저장 시 암호화 사용 여부
 *   0: 평문 저장 (기본값, iHEX → BIN 변환 등 디버깅에 유리)
 *   1: 암호화 저장 (fw_upload_crypto 사용)
 * 필요 시 컴파일 옵션이나 소스에서 재정의 가능하다. */
#ifndef FW_STORAGE_ENABLE_ENCRYPT
#define FW_STORAGE_ENABLE_ENCRYPT 0
#endif

typedef struct {
    FILE *file;
    fw_meta_t meta;
    size_t encrypted_written;
    size_t buffered;
    uint8_t buffer[16];
    uint8_t key[16];
    bool active;
} fw_storage_writer_t;

typedef struct {
    FILE *file;
    fw_meta_t meta;
    size_t remaining;
    size_t buffered;
    uint8_t buffer[16];
    uint8_t key[16];
    bool active;
    bool encrypted;
} fw_storage_reader_t;

esp_err_t fw_storage_writer_open(fw_storage_writer_t *writer, const fw_meta_t *meta);
esp_err_t fw_storage_writer_write(fw_storage_writer_t *writer, const uint8_t *data, size_t len);
esp_err_t fw_storage_writer_close(fw_storage_writer_t *writer);

esp_err_t fw_storage_reader_open(fw_storage_reader_t *reader, const char *name, const char *type);
size_t fw_storage_reader_read(fw_storage_reader_t *reader, uint8_t *out, size_t out_len);
void fw_storage_reader_close(fw_storage_reader_t *reader);

esp_err_t fw_storage_meta_write(const fw_meta_t *meta);
esp_err_t fw_storage_meta_read(const char *name, const char *type, fw_meta_t *out_meta);

bool fw_storage_build_path(char *out, size_t out_size, const char *name, const char *type);
bool fw_storage_build_meta_path(char *out, size_t out_size, const char *name, const char *type);

#endif
