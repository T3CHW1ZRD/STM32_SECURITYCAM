// File: main.cpp
#include "mbed.h"
#include "wifi_credentials.hpp"
#include "network.hpp"

int main() {
    printf("\n--- Secure TCP Client with AES-192 ---\n");
    init_filesystem();

    WifiCredentials cred;
    if (!load_credentials(cred)) {
        cred = prompt_user_input();
        save_credentials(cred);
    }

    if (!connect_to_wifi(cred.ssid, cred.password)) {
        printf("Wi-Fi connect failed\n");
        return -1;
    }

    start_secure_client();  // loops forever
}
