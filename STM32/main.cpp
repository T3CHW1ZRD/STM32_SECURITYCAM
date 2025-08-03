#include "mbed.h"
#include <cstdint>
#include "ArduCAM.h"
#include "memorysaver.h"

// ===== Pins for DISCO-L475VG-IOT01A =====
// SPI: MOSI=PA_7, MISO=PA_6, SCK=PA_5
// I2C: SDA=PB_9, SCL=PB_8
// CS for ArduCAM: PA_2
static BufferedSerial pc(USBTX, USBRX, 115200);
static SPI spi(PA_7 /*MOSI*/, PA_6 /*MISO*/, PA_5 /*SCK*/);
static I2C i2c(PB_9 /*SDA*/, PB_8 /*SCL*/);

// ArduCAM instance (uses Mbed-style constructor with SPI* and CS pin)
ArduCAM myCAM(OV2640, PA_2, &spi);  // ctor & methods: see ArduCAM.h/.cpp

// Not defined in your header; ArduCAM TRIG "capture done" bit is 0x08
#ifndef CAP_DONE_MASK
#define CAP_DONE_MASK 0x08
#endif

// Helper: write a C-string to USB serial
static void write_str(const char* s) {
    pc.write(s, strlen(s));
}

int main() {
    // Bring up peripherals
    spi.format(8, 0);
    spi.frequency(8'000'000);        // 8 MHz SPI
    i2c.frequency(400'000);          // 400 kHz I2C

    // Give host a moment to open the VCOM
    ThisThread::sleep_for(500ms);

    write_str("ArduCAM Mini 2MP Plus test\r\n");

    // ===== Camera init & JPEG setup (Mbed-style API) =====
    // --- Supported JPEG Resolutions (use with OV2640_set_JPEG_size):
    // OV2640_160x120     // QQVGA — lowest quality (tiny, unusable)
    // OV2640_176x144     // QCIF
    // OV2640_320x240     // QVGA — decent for thumbnails
    // OV2640_352x288     // CIF
    // OV2640_640x480     // VGA — recommended for most use cases
    // OV2640_800x600     // SVGA — high quality, bigger file
    // OV2640_1024x768    // XGA
    // OV2640_1280x1024   // SXGA — near max for 2MP
    // OV2640_1600x1200   // UXGA — max resolution (watch for FIFO overflow)

    // --- JPEG quality (set_jpeg_quality):
    // Lower hex value = less compression = better image = bigger file
    // 0x36  → low quality / high compression
    // 0x2A  → medium (default-ish)
    // 0x24  → good
    // 0x20  → high
    // 0x1C  → very high (may result in larger file than FIFO allows)
    myCAM.set_format(JPEG, i2c);
    myCAM.OV2640_set_JPEG_size(i2c, OV2640_1024x768); // or any from the enum
    myCAM.set_jpeg_quality(i2c, 0x2A);               // tweak to taste

    // Clear FIFO and start first capture
    myCAM.flush_fifo();
    myCAM.start_capture();

    // Main loop: wait for frame, dump FIFO over serial, trigger next frame
    while (true) {
        if (myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
            write_str("Capture done, dumping JPEG...\r\n");

            uint32_t len = myCAM.read_fifo_length();
            if (len == 0 || len > MAX_FIFO_SIZE) {
                write_str("  \xE2\x80\xBC  FIFO error\r\n"); // "‼"
            } else {
                // Stream out in chunks using burst reads
                const size_t CHUNK = 512;
                uint8_t buf[CHUNK];

                while (len > 0) {
                    size_t n = (len > CHUNK) ? CHUNK : (size_t)len;
                    myCAM.burst_read_fifo(buf, n);   // reads n bytes from FIFO
                    pc.write(buf, n);                // write raw JPEG bytes
                    len -= n;
                }
                write_str("\r\n\xE2\x9C\x94\xEF\xB8\x8F  JPEG sent\r\n"); // "✔️"
            }

            // Prepare next frame
            myCAM.flush_fifo();
            myCAM.start_capture();
        }

        ThisThread::sleep_for(3000ms);
    }
}
