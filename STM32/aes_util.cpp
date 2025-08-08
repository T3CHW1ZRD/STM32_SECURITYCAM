// File: aes_util.cpp

#include "aes_util.hpp"
#include "aes_key.hpp"
#include "proj_config.h"
#include "mbedtls/aes.h"
#include "mbed.h"
#include <cstdio>
#include <cstring>

// ---------- existing helpers ----------
bool load_aes_key(uint8_t *key_out, size_t key_len) {
    FILE *f = fopen(AES_KEY_PATH, "rb");
    if (f) {
        size_t r = fread(key_out, 1, key_len, f);
        fclose(f);
        if (r == key_len) return true;
    }
    memcpy(key_out, EMBEDDED_AES_KEY, key_len);
    return true;
}

void fill_random(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) buf[i] = static_cast<uint8_t>(rand() & 0xFF);
}

void aes_cbc_encrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length)
{
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, sizeof(iv_copy));

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, key_len * 8);

    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, length, iv_copy, in, out);

    mbedtls_aes_free(&ctx);
}

void aes_cbc_decrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length)
{
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, sizeof(iv_copy));

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_dec(&ctx, key, key_len * 8);

    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, length, iv_copy, in, out);

    mbedtls_aes_free(&ctx);
}

// ---------- streaming layer ----------
struct aes_cbc_stream_impl {
    mbedtls_aes_context aes;
};

static inline aes_cbc_stream_impl* as_impl(void *p) {
    return static_cast<aes_cbc_stream_impl*>(p);
}

void aes_cbc_stream_init(aes_cbc_stream *s,
                         const uint8_t *key, size_t key_len,
                         const uint8_t iv_in[16],
                         bool encrypt)
{
    auto *impl = new aes_cbc_stream_impl{};
    mbedtls_aes_init(&impl->aes);

    if (encrypt) mbedtls_aes_setkey_enc(&impl->aes, key, key_len * 8);
    else         mbedtls_aes_setkey_dec(&impl->aes, key, key_len * 8);

    memcpy(s->iv, iv_in, 16);
    s->impl    = impl;
    s->encrypt = encrypt;
}

void aes_cbc_stream_update(aes_cbc_stream *s,
                           const uint8_t *in, uint8_t *out, size_t in_len)
{
    if (in_len == 0) return;            // no-op
    // REQUIRE: in_len % 16 == 0
    mbedtls_aes_crypt_cbc(&as_impl(s->impl)->aes,
                          s->encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT,
                          in_len,
                          s->iv,        // mbedTLS updates this IV to last ciphertext block
                          in,
                          out);
}

void aes_cbc_stream_free(aes_cbc_stream *s)
{
    if (!s || !s->impl) return;
    mbedtls_aes_free(&as_impl(s->impl)->aes);
    delete as_impl(s->impl);
    s->impl = nullptr;
}
