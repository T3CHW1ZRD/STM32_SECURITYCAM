#pragma once
#include "rtos/Mutex.h"

/// Spawns a background thread that grabs JPEGs from the OV2640
/// and streams them over the encrypted socket.
/// Call once, passing the same mutex that guards socket I/O.
void start_cam_thread(rtos::Mutex &net_io_mutex);
