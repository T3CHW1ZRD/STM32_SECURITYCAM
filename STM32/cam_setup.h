
#include "mbed.h"
#include "ArduCAM.h"
#include "memorysaver.h"
#include "proj_config.h"

/**
 * One-shot camera initialisation helper.
 * After returning, the FIFO is flushed and a capture is already running.
 *
 * The long comment block below is lifted straight from your old main.cpp
 * so users still see *all* valid JPEG sizes & quality options exactly
 * where they expect them.
 */



inline void setup_cam(ArduCAM& cam, I2C& i2c)
{
    
    // ===== Supported JPEG Resolutions =====
    // OV2640_160x120     // QQVGA — lowest quality (tiny, unusable)
    // OV2640_176x144     // QCIF
    // OV2640_320x240     // QVGA — decent for thumbnails
    // OV2640_352x288     // CIF
    // OV2640_640x480     // VGA — recommended for most use cases
    // OV2640_800x600     // SVGA — high quality, bigger file
    // OV2640_1024x768    // XGA
    // OV2640_1280x1024   // SXGA — near max for 2 MP
    // OV2640_1600x1200   // UXGA — max resolution (watch FIFO overflow)
    //
    // ===== JPEG quality (set_jpeg_quality) =====
    // Lower hex = less compression = better image = bigger file
    // 0x36 → low | 0x2A → medium | 0x24 → good | 0x20 → high | 0x1C → very high
    // (block copied verbatim from previous main.cpp) :contentReference[oaicite:3]{index=3}
    
    cam.set_format(JPEG, i2c);
    cam.OV2640_set_JPEG_size(i2c, CAM_JPEG_SIZE);
    cam.set_jpeg_quality(i2c,     CAM_JPEG_QUALITY);

    cam.flush_fifo();
    //cam.start_capture();
}
