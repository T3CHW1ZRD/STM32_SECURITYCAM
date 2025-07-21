#include "aes_util.hpp"
#include "mbedtls/aes.h"
#include "config.h"
#include "mbed.h"
#include <cstdio>

// Load raw key bytes from filesystem
bool load_aes_key(uint8_t *key_out, size_t key_len) {
    FILE *f = fopen(AES_KEY_PATH, "rb");
    if (!f) return false;
    size_t r = fread(key_out, 1, key_len, f);
    fclose(f);
    return r == key_len;
}

// Simple PRNG for IV (mbed::rand)
void fill_random(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        buf[i] = (uint8_t)(rand() & 0xFF);
    }
}

void aes_cbc_encrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, key_len * 8);
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, length, iv, in, out);
    mbedtls_aes_free(&ctx);
}

void aes_cbc_decrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_dec(&ctx, key, key_len * 8);
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, length, iv, in, out);
    mbedtls_aes_free(&ctx);
}
