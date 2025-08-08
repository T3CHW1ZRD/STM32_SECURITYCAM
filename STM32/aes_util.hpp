// File: aes_util.hpp
#ifndef AES_UTIL_HPP
#define AES_UTIL_HPP

#include <cstdint>
#include <cstddef>

// Try to load AES-192 key from AES_KEY_PATH, or fall back to EMBEDDED_AES_KEY.
// Always returns true if key_len == 24.
bool load_aes_key(uint8_t *key_out, size_t key_len);

// Fill buffer with PRNG bytes (for IV)
void fill_random(uint8_t *buf, size_t len);

// ----------------------
// One-shot AES-192-CBC
// ----------------------
// Encrypt/decrypt a whole buffer in one call (length must be multiple of 16).
// NOTE: 'iv' is NOT modified; you can reuse it for logging or resend it.
void aes_cbc_encrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length);

void aes_cbc_decrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length);

// ----------------------
// Streaming AES-192-CBC
// ----------------------
// For large messages: init once, feed full 16-byte blocks many times.
// The context keeps CBC state (the evolving IV) across calls.
struct aes_cbc_stream {
    void *impl;      // opaque (mbedTLS ctx)
    uint8_t iv[16];  // updated after each call
    bool encrypt;    // true = enc, false = dec
};

// Init once per message.
// 'iv_in' is the 16-byte IV you’ll send on the wire (for encrypt) or received (for decrypt).
void aes_cbc_stream_init(aes_cbc_stream *s,
                         const uint8_t *key, size_t key_len,
                         const uint8_t iv_in[16],
                         bool encrypt);

// Process any length that is a multiple of 16 (0 allowed).
// 'in_len % 16 == 0' must hold; function updates s->iv internally.
void aes_cbc_stream_update(aes_cbc_stream *s,
                           const uint8_t *in, uint8_t *out, size_t in_len);

// Free when done (releases internal mbedTLS ctx).
void aes_cbc_stream_free(aes_cbc_stream *s);

#endif // AES_UTIL_HPP
