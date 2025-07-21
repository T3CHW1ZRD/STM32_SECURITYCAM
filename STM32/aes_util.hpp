#ifndef AES_UTIL_HPP
#define AES_UTIL_HPP

#include <cstdint>
#include <cstddef>

// Load 24-byte key from AES_KEY_PATH, return true on success
bool load_aes_key(uint8_t *key_out, size_t key_len);

// Fill buf[0..len) with pseudo-random bytes (for IV generation)
void fill_random(uint8_t *buf, size_t len);

// AES-192-CBC encrypt: in/out buffers must be length multiple of 16
void aes_cbc_encrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length);

// AES-192-CBC decrypt
void aes_cbc_decrypt(const uint8_t *key, size_t key_len,
                     uint8_t iv[16],
                     const uint8_t *in, uint8_t *out, size_t length);

#endif
