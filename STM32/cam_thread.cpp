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
            //cam.flush_fifo();
            cam.clear_fifo_flag();
            //cam.clear_capture_done();
            ThisThread::sleep_for(1ms);
            
            
            /* Capture one frame */
            cam.start_capture();
            if(cam.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)){printf("DIDNT CLEAR BIT");}
            printf("Starting capture \n");

            while (!cam.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)){printf("NOT DONE YET");ThisThread::sleep_for(5ms);}
                ThisThread::sleep_for(5ms);

            uint32_t len = cam.read_fifo_length();
            printf("FIFO LENGTH: %d \n", len);
            if (len && len <= MAX_FIFO_SIZE) {
                std::unique_ptr<uint8_t[]> frame(new uint8_t[len]);
                cam.burst_read_fifo(frame.get(), len);
                printf("[cam] JPEG starts: %02X %02X (%lu B)\r\n",
                       frame[0], frame[1], (unsigned long)len);
                
                send_send_photo(frame.get(), len);   // encrypt & queue
            }
            cam.flush_fifo();
            cam.clear_fifo_flag();
            
            
        }
    });
}