#ifndef FW_UPLOAD_CRYPTO_H
#define FW_UPLOAD_CRYPTO_H

#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

esp_err_t fw_crypto_init_key(uint8_t out_key[16]);
esp_err_t fw_crypto_encrypt_block(const uint8_t key[16], const uint8_t *in, uint8_t *out, size_t len);
esp_err_t fw_crypto_decrypt_block(const uint8_t key[16], const uint8_t *in, uint8_t *out, size_t len);

#endif
