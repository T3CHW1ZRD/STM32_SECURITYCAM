// File: wifi_credentials.cpp

#include "wifi_credentials.hpp"
#include "LittleFileSystem.h"
#include "FlashIAPBlockDevice.h"
#include "mbed.h"
#include <cstdio>
#include <cstring>

/// Move FS to 0x08080000 so it isn’t erased by app flashes
static FlashIAPBlockDevice bd(0x08080000, 64 * 1024);
static LittleFileSystem      fs("fs");
static constexpr const char *settings_path = "/fs/wifi_settings.json";

void init_filesystem() {
    int err = fs.mount(&bd);
    if (err) {
        printf("Mount failed (%d), reformatting...\n", err);
        err = fs.reformat(&bd);
        if (err) error("Cannot reformat filesystem");
        err = fs.mount(&bd);
        if (err) error("Cannot mount filesystem");
    }
}


WifiCredentials prompt_user_input() {
    char ssid_buf[64]     = {0};
    char password_buf[64] = {0};
    int  c, idx;

    // --- Prompt SSID ---
    printf("Enter SSID: ");
    fflush(stdout);           // ensure the prompt shows immediately
    idx = 0;
    while (true) {
        c = getchar();
        if (c == '\r' || c == '\n' || c == EOF) {
            break;
        }
        // Handle backspace or DEL
        if ((c == '\b' || c == 127) && idx > 0) {
            idx--;
            // Erase from screen
            printf("\b \b");
        }
        // Printable character
        else if (c >= 32 && c < 127 && idx < (int)sizeof(ssid_buf) - 1) {
            ssid_buf[idx++] = (char)c;
            putchar(c);
        }
        // otherwise ignore
    }
    ssid_buf[idx] = '\0';
    printf("\n");

    // --- Prompt Password ---
    printf("Enter Password: ");
    fflush(stdout);
    idx = 0;
    while (true) {
        c = getchar();
        if (c == '\r' || c == '\n' || c == EOF) {
            break;
        }
        if ((c == '\b' || c == 127) && idx > 0) {
            idx--;
            printf("\b \b");
        }
        else if (c >= 32 && c < 127 && idx < (int)sizeof(password_buf) - 1) {
            password_buf[idx++] = (char)c;
            putchar(c);
        }
    }
    password_buf[idx] = '\0';
    printf("\n");

    return WifiCredentials{
        std::string(ssid_buf),
        std::string(password_buf)
    };
}

void save_credentials(const WifiCredentials &cred) {
    init_filesystem();
    FILE *file = fopen(settings_path, "w");
    if (!file) {
        printf("Failed to open %s for writing\n", settings_path);
        return;
    }
    fprintf(file,
            "{\n"
            "  \"ssid\": \"%s\",\n"
            "  \"password\": \"%s\"\n"
            "}\n",
            cred.ssid.c_str(),
            cred.password.c_str());
    fclose(file);
    printf("Saved Wi-Fi credentials.\n");
}

bool load_credentials(WifiCredentials &cred) {
    init_filesystem();
    FILE *file = fopen(settings_path, "r");
    if (!file) {
        return false;
    }

    char ssid_buf[64]     = {0};
    char password_buf[64] = {0};
    int matched = fscanf(file,
                         " { %*[\n\r ] \"ssid\" : \"%63[^\"]\" ,"
                         " %*[\n\r ] \"password\" : \"%63[^\"]\" %*[\n\r ] } ",
                         ssid_buf, password_buf);
    fclose(file);

    if (matched != 2) {
        return false;
    }
    cred.ssid     = ssid_buf;
    cred.password = password_buf;
    return true;
}
