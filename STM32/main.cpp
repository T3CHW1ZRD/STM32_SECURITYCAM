#include "mbed.h"
#include <cstdint>
#include "ArduCAM.h"
#include "memorysaver.h"
#include "cam_setup.h"       // ← new include!

// ── Pins for DISCO-L475VG-IOT01A ───────────────────────────────
static BufferedSerial pc(USBTX, USBRX, 115200);
static SPI  spi(PA_7 /*MOSI*/, PA_6 /*MISO*/, PA_5 /*SCK*/);
static I2C  i2c(PB_9 /*SDA*/, PB_8 /*SCL*/);

ArduCAM myCAM(OV2640, PA_2, &spi);

#ifndef CAP_DONE_MASK
#define CAP_DONE_MASK 0x08   // ArduCAM TRIG “capture done” bit
#endif

static void write_str(const char* s) { pc.write(s, strlen(s)); }

int main()
{
    spi.format(8, 0);          spi.frequency(8'000'000);
    i2c.frequency(400'000);
    ThisThread::sleep_for(500ms);

    write_str("ArduCAM Mini 2MP Plus test\r\n");

    /* one-liner replaces ~25 lines of init */
    setup_cam(myCAM, i2c);

    // Main loop: wait for frame, dump FIFO over serial, restart
    while (true) {
        if (myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
            write_str("Capture done, dumping JPEG.\r\n");

            uint32_t len = myCAM.read_fifo_length();
            if (len == 0 || len > MAX_FIFO_SIZE) {
                write_str("‼  FIFO error\r\n");
            } else {
                const size_t CHUNK = 512;
                uint8_t buf[CHUNK];
                while (len > 0) {
                    size_t n = (len > CHUNK) ? CHUNK : (size_t)len;
                    myCAM.burst_read_fifo(buf, n);
                    pc.write(buf, n);
                    len -= n;
                }
                write_str("✔️  JPEG sent\r\n");
            }
            myCAM.flush_fifo();
            myCAM.start_capture();
        }
        ThisThread::sleep_for(3000ms);
    }
}
