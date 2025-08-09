// File: main.cpp
#include "mbed.h"
#include "network.hpp"
#include "commands.hpp"
#include "proj_config.h"
#include <chrono>
#include <ctime>

#include "rtos/Kernel.h"        // rtos::Kernel::attach_idle_hook
#include "mbed_power_mgmt.h"    // sleep_manager_can_deep_sleep()
#include "hal/sleep_api.h"      // hal_sleep(), hal_deepsleep()

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

int main() {
    printf("\nSecure TCP Client (AES-192)\n");

    // Attach idle hook so the MCU sleeps whenever idle
    rtos::Kernel::attach_idle_hook(idle_sleep_hook);

    // Seed rand() used for IVs/backoff (non-crypto)
    srand((unsigned)time(NULL));

    // Robust Wi-Fi connect (network.cpp already retries internally)
    while (true) {
        nsapi_error_t ret = connect_to_wifi(WIFI_SSID, WIFI_PASSWORD);
        if (ret == NSAPI_ERROR_OK) break;
        printf("Wi-Fi not connected (%d). Retrying...\n", ret);
        rtos::ThisThread::sleep_for(1s);
    }

    start_secure_client(); // blocking
}
