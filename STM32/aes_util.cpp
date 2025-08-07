// File: aes_util.cpp

#include "aes_util.hpp"
#include "aes_key.hpp"
#include "proj_config.h"
#include "mbedtls/aes.h"
#include "mbed.h"
#include <cstdio>
#include <cstring>

bool load_aes_key(uint8_t *key_out, size_t key_len) {
    // 1) Try filesystem
    FILE *f = fopen(AES_KEY_PATH, "rb");
    if (f) {
        size_t r = fread(key_out, 1, key_len, f);
        fclose(f);
        if (r == key_len) {
            return true;
        }
    }
    // 2) Fallback to embedded key
    memcpy(key_out, EMBEDDED_AES_KEY, key_len);
    return true;
}

void fill_random(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        buf[i] = (uint8_t)(rand() & 0xFF);
    }
}

void aes_cbc_encrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length)
{
    // Copy the caller’s IV so mbedtls won’t clobber it
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, sizeof(iv_copy));

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, key_len * 8);

    // Use iv_copy here; iv stays as the original random IV
    mbedtls_aes_crypt_cbc(&ctx,
                          MBEDTLS_AES_ENCRYPT,
                          length,
                          iv_copy,
                          in,
                          out);

    mbedtls_aes_free(&ctx);
}

void aes_cbc_decrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length)
{
    // Same trick on decrypt: keep the caller’s iv intact
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, sizeof(iv_copy));

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_dec(&ctx, key, key_len * 8);

    mbedtls_aes_crypt_cbc(&ctx,
                          MBEDTLS_AES_DECRYPT,
                          length,
                          iv_copy,
                          in,
                          out);

    mbedtls_aes_free(&ctx);
}
