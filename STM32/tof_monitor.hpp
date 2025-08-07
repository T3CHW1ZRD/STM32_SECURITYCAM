#pragma once
#include "mbed.h"

namespace events { class EventQueue; }
class BluetoothAlert;

void start_tof_monitor(uint16_t trigger_range_mm = 600,
                       int      min_noise        = 20,
                       events::EventQueue* ble_q = nullptr,
                       BluetoothAlert*      ble  = nullptr);
