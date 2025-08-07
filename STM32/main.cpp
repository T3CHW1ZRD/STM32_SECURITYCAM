#include "mbed.h"
#include "wifi_credentials.hpp"
#include "network.hpp"
#include "commands.hpp"
#include <chrono>              // for chrono literals
using namespace std::chrono_literals;

int main() {
    printf("\n--- Secure TCP Client with AES-192 ---\n");
    init_filesystem();

    WifiCredentials cred;
    if (!load_credentials(cred)) {
        cred = prompt_user_input();
        save_credentials(cred);
    }

    // Loop until Wi-Fi connects
    while (true) {
        nsapi_error_t ret = connect_to_wifi(
            cred.ssid.c_str(),
            cred.password.c_str()
        );
        if (ret == NSAPI_ERROR_OK) {
            break;  // connected
        }
        if (ret == NSAPI_ERROR_AUTH_FAILURE) {
            printf("Wi-Fi authentication failed. Please re-enter credentials.\n");
            cred = prompt_user_input();
            save_credentials(cred);
        } else {
            printf("Wi-Fi connect failed (%d). Retrying...\n", ret);
            ThisThread::sleep_for(2s);
        }
    }

    // Now on Wi-Fi, start the secure client
    start_secure_client();  // does not return
}