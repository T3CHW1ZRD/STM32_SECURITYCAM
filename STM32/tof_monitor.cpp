#include "tof_monitor.hpp"
#include "VL53L0X.h"
#include "commands.hpp"        // send_alarm_tripped()
#include <atomic>
#include "events/EventQueue.h"      // ← add this
#include "bluetooth_alert.hpp"      // ← and this
#include <chrono>
using namespace std::chrono;

// ---- Pins (from your old main) ----
static I2C    tof_i2c(PB_11, PB_10);
static BusOut tof_shutdown(PC_6);
static VL53L0X tof(&tof_i2c);

// ---- Worker thread ----
static rtos::Thread tof_thread(osPriorityNormal, 2048);
static std::atomic<bool> started{false};

// Small parameter bundle passed to the thread entry
struct ToFParams {
    uint16_t trigger_range_mm;
    int      min_noise;
    events::EventQueue* ble_q;
    BluetoothAlert*     ble;
};

// C-style thread entry – no captures, fits Callback storage
static void tof_worker(void* arg) {
    ToFParams p = *static_cast<ToFParams*>(arg);
    delete static_cast<ToFParams*>(arg);  // free heap param
    const uint32_t RELAX_SENSOR = 5000;
    uint32_t lastTrig = 0;

    bool triggered = false;

    while (true) {
        int distance = tof.getRangeMillimeters();
        //printf("dist = %d\n", distance);
        if (distance > p.min_noise &&
            distance < static_cast<int>(p.trigger_range_mm) &&
            !triggered)
        {
            uint32_t now = duration_cast<milliseconds>(rtos::Kernel::Clock::now().time_since_epoch()).count();
            if(now - lastTrig >= RELAX_SENSOR)
            {
                printf("[ToF] Motion detected (dist=%d)\n", distance);

                int rc = send_alarm_tripped();
                if (rc < 0) printf("[ToF] send_alarm_tripped err=%d\n", rc);

                if (p.ble_q && p.ble) {
                    p.ble_q->call(callback(p.ble, &BluetoothAlert::trigger_alert));
                }
                lastTrig = now;
                triggered = true;
            }
            
        }
        else if (distance >= static_cast<int>(p.trigger_range_mm)) {
            triggered = false;
        } 
        else{
            triggered =false;
        }
        ThisThread::sleep_for(100ms);
    }
}

void start_tof_monitor(uint16_t trigger_range_mm,
                       int      min_noise,
                       events::EventQueue* ble_q,
                       BluetoothAlert*      ble)
{
    if (started.exchange(true)) return;

    tof_shutdown = 1;
    ThisThread::sleep_for(10ms);
    tof_i2c.frequency(400000);

    if (!tof.init()) {
        printf("[ToF] init failed\n");
    } else {
        tof.setModeContinuous();
        tof.startContinuous();
        printf("[ToF] init OK (continuous)\n");
    }

    // allocate params on heap and pass pointer (small and safe)
    auto *params = new ToFParams{ trigger_range_mm, min_noise, ble_q, ble };
    tof_thread.start(mbed::callback(tof_worker, params));
}