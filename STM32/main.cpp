#include "mbed.h"
#include "wifi_credentials.hpp"
#include "network.hpp"

int main() {
    printf("\n--- TCP Server with Saved Wi-Fi ---\n");

    init_filesystem();

    WifiCredentials cred;
    if (!load_credentials(cred)) {
        printf("No saved Wi-Fi credentials.\n");
        cred = prompt_user_input();
        save_credentials(cred);
    }

    if (!connect_to_wifi(cred.ssid, cred.password)) {
        printf("Wi-Fi connection failed.\n");
        return -1;
    }

    start_tcp_server();
}
