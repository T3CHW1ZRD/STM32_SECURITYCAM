#include "mbed.h"
#include "VL53L0X.h"
#include "wifi_credentials.hpp"
#include "network.hpp"


#define TRIGGER_RANGE 600

I2C i2c(PB_11, PB_10); // IC2 from datasheet 
BusOut range_shutdown(PC_6);
VL53L0X range_sensor(&i2c);

Ticker ToFTicker; // Used to trigger the ToF sensor every 50 ms
EventQueue queue(32 * EVENTS_EVENT_SIZE);

bool triggered = false; // Last state of the ToF sensor

void entry_detected() {
    queue.call(printf, "Motion detected!");
}

void check_entry() {
    int distance = range_sensor.getRangeMillimeters();
    if (distance < TRIGGER_RANGE && triggered == false) {
        queue.call(entry_detected);
        triggered = true;
    } else if (distance >= TRIGGER_RANGE) {
        triggered = false;
    } else {
        triggered = true;
    }

}

void check_sensor() {
    queue.call(check_entry);
}

int main() {
    uint8_t shutdown_pin = 1;
    range_shutdown = shutdown_pin;
    // i2c.frequency(9600);
    // Initializing time of flight sensor
    // printf("Starting sensor\n");

    if (!range_sensor.init()) {
        printf("Sensor init failed!\n");
    }

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

    range_sensor.setModeContinuous();
    range_sensor.startContinuous();
    
    // Setting up Ticker to trigger time of flight sensor check
    ToFTicker.attach(check_sensor, 100ms);
    while (true) {
        queue.dispatch_forever();
    }
}
