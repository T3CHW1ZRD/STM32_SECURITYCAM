#include "wifi_credentials.hpp"
#include "LittleFileSystem.h"
#include "FlashIAPBlockDevice.h"
#include "mbed.h"
#include <cstdio>
#include <cstring>

static FlashIAPBlockDevice bd(0x08040000, 64 * 1024);
static LittleFileSystem fs("fs");
static const char *settings_path = "/fs/wifi_settings.json";

void init_filesystem() {
    int err = fs.mount(&bd);
    if (err) {
        printf("Mount failed, formatting...\n");
        fs.reformat(&bd);
    }
}

WifiCredentials prompt_user_input() {
    char ssid[64], password[64];
    printf("Enter SSID: ");
    scanf("%63s", ssid);
    printf("Enter Password: ");
    scanf("%63s", password);
    return WifiCredentials{ssid, password};
}

void save_credentials(const WifiCredentials &cred) {
    FILE *file = fopen(settings_path, "w");
    if (file) {
        fprintf(file, "{\n  \"ssid\": \"%s\",\n  \"password\": \"%s\"\n}\n",
                cred.ssid.c_str(), cred.password.c_str());
        fclose(file);
    } else {
        printf("Failed to save settings!\n");
    }
}

bool load_credentials(WifiCredentials &cred) {
    FILE *file = fopen(settings_path, "r");
    if (!file) return false;

    char ssid[64] = {}, password[64] = {};
    fscanf(file, "{ \"ssid\": \"%63[^\"]\", \"password\": \"%63[^\"]\" }", ssid, password);
    fclose(file);

    if (strlen(ssid) == 0 || strlen(password) == 0) return false;

    cred.ssid = ssid;
    cred.password = password;
    return true;
}
