#include "cam_thread.hpp"

#include "mbed.h"
#include "ArduCAM.h"
#include "memorysaver.h"
#include "cam_setup.h"          // your existing inline setup_cam(cam,i2c)
#include "commands.hpp"         // send_send_photo()
#include "rtos/ThisThread.h"
#include <algorithm>            // std::min

/* ------------------------------------------------------------------ */
/* Hardware objects local to this translation unit                    */
/* ------------------------------------------------------------------ */
static SPI     spi(PA_7 /*MOSI*/, PA_6 /*MISO*/, PA_5 /*SCK*/);
static I2C     i2c(PB_9 /*SDA*/,  PB_8 /*SCL*/);
static ArduCAM cam(OV2640, PA_2 /*CS*/, &spi);   // hard-code CS pin

/* If the driver didn’t give us this bit mask, define it once here.   */
#ifndef CAP_DONE_MASK
#define CAP_DONE_MASK 0x08      /* ARDUCHIP_TRIG bit3 = capture done */
#endif

/* ------------------------------------------------------------------ */
/* Camera thread implementation                                       */
/* ------------------------------------------------------------------ */
void start_cam_thread(rtos::Mutex & /*net_io_mutex*/)
{
    setup_cam(cam, i2c);                       // one-time OV2640 config

    static rtos::Thread cam_thread;
    cam_thread.start([&]{
        using namespace std::chrono;
        constexpr auto FRAME_INTERVAL = 15000ms;     // 1 frame / 15 s

        while (true) {
            const auto t0 = Kernel::Clock::now();

            if (cam.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {

                uint32_t len = cam.read_fifo_length();
                if (len && len <= MAX_FIFO_SIZE) {

                    /* 1. read full JPEG */
                    std::unique_ptr<uint8_t[]> frame(new uint8_t[len]);
                    cam.burst_read_fifo(frame.get(), len);

                    printf("[cam] JPEG starts: 0x%02X 0x%02X  (%lu B)\r\n",
                           frame[0], frame[1], (unsigned long)len);

                    /* 2. ONE command – send_send_photo locks io_mutex itself */
                    send_send_photo(frame.get(), len);
                    printf("[cam]   → whole frame sent\n");
                }

                /* 3. restart capture */
                cam.flush_fifo();
                cam.start_capture();
            }

            /* 4. FPS throttle */
            auto dt = Kernel::Clock::now() - t0;
            if (dt < FRAME_INTERVAL)
                ThisThread::sleep_for(FRAME_INTERVAL - dt);
        }
    });
}
