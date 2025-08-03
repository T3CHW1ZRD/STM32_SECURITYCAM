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

// AES-192-CBC encrypt/decrypt in-place (length must be multiple of 16)
void aes_cbc_encrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length);
void aes_cbc_decrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length);

#endif // AES_UTIL_HPP
