#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>   // for uint16_t, etc.

// PC server settings
static constexpr const char* SERVER_IP    = "192.168.0.29";
static constexpr uint16_t      SERVER_PORT = 12345;

// Path on your LittleFileSystem where the 24-byte AES-192 key lives
static constexpr const char*   AES_KEY_PATH = "/fs/key.txt";

#endif // CONFIG_HPP
