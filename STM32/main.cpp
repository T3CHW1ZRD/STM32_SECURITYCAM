#include "mbed.h"
#include "network.hpp"
#include "commands.hpp"
#include <chrono>
#include <ctime>

#include "rtos/Kernel.h"
#include "mbed_power_mgmt.h"
#include "hal/sleep_api.h"

using namespace std::chrono_literals;

// Idle hook: enter deep sleep when allowed; otherwise light sleep
static void idle_sleep_hook() {
#if defined(MBED_SLEEP_MANAGER_ENABLED)
    if (sleep_manager_can_deep_sleep()) {
        hal_deepsleep();
    } else {
        hal_sleep();
    }
#else
    hal_sleep();
#endif
}

// Minimal interactive prompt (no saving)
static void prompt_wifi(char ssid_out[64], char pass_out[64]) {
    auto read_line = [&](const char *label, char *buf, size_t cap) {
        printf("%s", label);
        fflush(stdout);
        size_t i = 0;
        while (true) {
            int c = getchar();
            if (c == '\r' || c == '\n' || c == EOF) break;
            if ((c == '\b' || c == 127) && i > 0) { i--; printf("\b \b"); continue; }
            if (c >= 32 && c < 127 && i + 1 < cap) { buf[i++] = (char)c; putchar(c); }
        }
        buf[i] = '\0';
        printf("\n");
    };

    read_line("Enter SSID: ", ssid_out, 64);
    read_line("Enter Password: ", pass_out, 64);
}

int main() {
    printf("\nSecure TCP Client (AES-192)\n");

    // Attach idle hook (portable for Mbed OS 6)
    rtos::Kernel::attach_idle_hook(idle_sleep_hook);

    // Seed rand() used for IVs/backoff (non-crypto)
    srand((unsigned)time(NULL));

    char ssid[64] = {0}, pass[64] = {0};
    prompt_wifi(ssid, pass);

    while (true) {
        nsapi_error_t ret = connect_to_wifi(ssid, pass);
        if (ret == NSAPI_ERROR_OK) break;
        printf("Wi-Fi not connected (%d). Try again.\n", ret);
        prompt_wifi(ssid, pass);
    }

    start_secure_client(); // blocking
}
