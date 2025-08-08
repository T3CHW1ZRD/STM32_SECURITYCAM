#include "cam_thread.hpp"

#include "mbed.h"
#include "ArduCAM.h"
#include "memorysaver.h"
#include "cam_setup.h"          // your existing inline setup_cam(cam,i2c)
#include "commands.hpp"         // send_send_photo()
#include "rtos/ThisThread.h"
#include <algorithm>            // std::min
#include <chrono>               //Kernel::Clock::now() 

/* ------------------------------------------------------------------ */
/* Hardware objects local to this translation unit                    */
/* ------------------------------------------------------------------ */
static SPI     spi(PA_7 /*MOSI*/, PA_6 /*MISO*/, PA_5 /*SCK*/);
static I2C     i2c(PB_9 /*SDA*/,  PB_8 /*SCL*/);
static ArduCAM cam(OV2640, PA_2 /*CS*/, &spi);   // hard-code CS pin
// Read & send in 20KB slices instead of the whole JPEG
static constexpr uint32_t PHOTO_CHUNK = 20 * 1024;


/* If the driver didn’t give us this bit mask, define it once here.   */
#ifndef CAP_DONE_MASK
#define CAP_DONE_MASK 0x08      /* ARDUCHIP_TRIG bit3 = capture done */
#endif


static rtos::EventFlags cam_evt;        // bit0 = capture-now

void cam_request_photo() {
    cam_evt.set(0x01);                  // wake the thread
}

/* ------------------------------------------------------------------ */
/* Camera thread implementation                                       */
/* ------------------------------------------------------------------ */
void start_cam_thread(rtos::Mutex & /*io_mutex*/)
{
    spi.format(8, 0);          // 8 bits, mode 0
    spi.frequency(8000000);    // 8 MHz is fine
    setup_cam(cam, i2c);
    


    static rtos::Thread cam_thread;
    cam_thread.start([&] {
        
        while (true) {
            cam_evt.wait_any(0x01);     // sleep until a request arrives
            //cam.clear_fifo_flag();
            
            cam.clear_fifo_flag();
            //cam.clear_capture_done();
            ThisThread::sleep_for(5ms);
            
            
            /* Capture one frame */
            cam.start_capture();
            if(cam.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)){printf("DIDNT CLEAR BIT \n");}
            printf("Starting capture \n");

            while (!cam.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)){printf("NOT DONE YET");ThisThread::sleep_for(5ms);}
                ThisThread::sleep_for(5ms);

            uint32_t len = cam.read_fifo_length();
            printf("FIFO LENGTH: %d \n", len);
            if (len && len <= MAX_FIFO_SIZE) {
                // ------ Chunked read & send (20 KB) ------
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            rtos::Kernel::Clock::now().time_since_epoch()
                        ).count();
                uint32_t session_id = static_cast<uint32_t>(ms & 0xFFFFFFFF);
                std::unique_ptr<uint8_t[]> chunk(new uint8_t[PHOTO_CHUNK]);

                // FIRST chunk: prepend total_len (4B) in payload via send_photo_start(),
                // so we only read up to (PHOTO_CHUNK - 4) bytes of JPEG for the first slice.
                uint32_t first = std::min<uint32_t>(PHOTO_CHUNK - 4, len);
                cam.burst_read_fifo(chunk.get(), first);

                // Keep your JPEG header print (now from the first slice)
                printf("[cam] JPEG starts: %02X %02X (%lu B)\r\n",
                    chunk[0], chunk[1], (unsigned long)len);

                // If it fits in one go, mark as single-chunk
                bool single = (first == len);
                send_photo_start(session_id, len, chunk.get(), first, single);

                uint32_t sent = first;
                uint16_t seq  = 1;

                // MIDDLE/LAST chunks
                while (sent < len) {
                    uint32_t n = std::min<uint32_t>(PHOTO_CHUNK, len - sent);
                    cam.burst_read_fifo(chunk.get(), n);
                    bool last = (sent + n == len);
                    send_photo_chunk(session_id, seq++, chunk.get(), n, last);
                    sent += n;
                    printf("[cam] SENT CHUNK \n");
                }
                // ------ end chunked ------

            }
            //cam.clear_fifo_flag();
            cam.flush_fifo();
            
        }
    });
}