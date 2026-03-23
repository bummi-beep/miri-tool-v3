#include "fw_upload_crypto.h"

#include <string.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <mbedtls/aes.h>
#include <stdbool.h>

static const char *TAG = "fw_crypto";

esp_err_t fw_crypto_init_key(uint8_t out_key[16]) {
    if (!out_key) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t mac[6] = {0};
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_read_mac failed: %s", esp_err_to_name(err));
        return err;
    }
    /*
     * Derive a 16-byte key by repeating the base MAC.
     * This keeps it deterministic without storing secrets in flash.
     */
    for (size_t i = 0; i < 16; i++) {
        out_key[i] = mac[i % sizeof(mac)] ^ (uint8_t)(0xA5 + i);
    }
    return ESP_OK;
}

static esp_err_t aes_crypt(const uint8_t key[16], const uint8_t *in, uint8_t *out, size_t len, bool encrypt) {
    if (!key || !in || !out || (len % 16) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int ret = encrypt
        ? mbedtls_aes_setkey_enc(&aes, key, 128)
        : mbedtls_aes_setkey_dec(&aes, key, 128);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return ESP_FAIL;
    }
    for (size_t i = 0; i < len; i += 16) {
        ret = mbedtls_aes_crypt_ecb(&aes,
                                    encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT,
                                    in + i, out + i);
        if (ret != 0) {
            mbedtls_aes_free(&aes);
            return ESP_FAIL;
        }
    }
    mbedtls_aes_free(&aes);
    return ESP_OK;
}

esp_err_t fw_crypto_encrypt_block(const uint8_t key[16], const uint8_t *in, uint8_t *out, size_t len) {
    return aes_crypt(key, in, out, len, true);
}

esp_err_t fw_crypto_decrypt_block(const uint8_t key[16], const uint8_t *in, uint8_t *out, size_t len) {
    return aes_crypt(key, in, out, len, false);
}
