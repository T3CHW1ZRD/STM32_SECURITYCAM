#include "wifi_credentials.hpp"
#include "mbed.h"
#include <cstdio>
#include <cstring>

void init_filesystem() {
    // No-op: credential persistence removed
}

WifiCredentials prompt_user_input() {
    char ssid_buf[64]     = {0};
    char password_buf[64] = {0};

    auto read_line = [&](const char *label, char *buf, size_t cap) {
        printf("%s", label);
        fflush(stdout);
        size_t i = 0;
        while (true) {
            int c = getchar();
            if (c == '\r' || c == '\n' || c == EOF) break;
            if ((c == '\b' || c == 127) && i > 0) {
                i--; printf("\b \b"); continue;
            }
            if (c >= 32 && c < 127 && i + 1 < cap) {
                buf[i++] = (char)c; putchar(c);
            }
        }
        buf[i] = '\0';
        printf("\n");
    };

    read_line("Enter SSID: ", ssid_buf, sizeof ssid_buf);
    read_line("Enter Password: ", password_buf, sizeof password_buf);

    return WifiCredentials{ std::string(ssid_buf), std::string(password_buf) };
}

void save_credentials(const WifiCredentials &) {
    printf("Not saving Wi-Fi credentials (persistence disabled).\n");
}

bool load_credentials(WifiCredentials &) {
    return false; // always force prompt
}
