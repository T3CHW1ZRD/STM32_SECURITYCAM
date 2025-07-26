#include "mbed.h"
#include "wifi_credentials.hpp"
#include "network.hpp"
#include "VL53L0X.h" // Time of Flight Sensor

#define TRIGGER_RANGE 600 // The threshold range below which the range sensor should trigger, in mm
#define CHECK_FREQUENCY 200ms // How often the range sensor should check for an obstruction (in ms)

I2C i2c(PB_11, PB_10); // IC2 from datasheet 
VL53L0X range_sensor(&i2c);
LowPowerTicker range_ticker; // Used to trigger the ToF sensor every 50 ms
EventQueue queue(32 * EVENTS_EVENT_SIZE); // Used to queue events for executing outside of the motion detection ISR

bool triggered = false; // Last state of the ToF sensor

void entry_detected() {
    queue.call(printf, "Motion detected!"); // Replace with actual logic to execute when the ToF detects something
    // __disable_irq(); // Disable interrupts until the entry detection event is handled
}

void check_entry() { // Checks if someone is within range of the range sensor
    int distance = range_sensor.getRangeMillimeters();
    if (distance < TRIGGER_RANGE && triggered == false) {
        queue.call(entry_detected);
        triggered = true;
    } else if (distance >= TRIGGER_RANGE) {
        triggered = false;
    }
}

void check_sensor() {
    queue.call(check_entry);
}

int main() {
    // Init range sensor
    // Initializing time of flight sensor
    printf("Starting sensor\n");

    if (!range_sensor.init()) {
        printf("Sensor initialization failed!\n");
        return 1;
    }
    range_sensor.setModeContinuous();
    range_sensor.startContinuous();
    
    // Setting up Ticker to trigger time of flight sensor check
    range_ticker.attach(check_sensor, CHECK_FREQUENCY);
    while (true) {
        queue.dispatch_forever();
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
}
