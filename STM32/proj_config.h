#ifndef CONFIG_H
#define CONFIG_H

/* ── Network / BLE ──────────────────────────────────────────────── */
static const char* SERVER_IP   = "192.168.2.16";
static int         SERVER_PORT = 12345;
static const char* BLE_NAME    = "SEC-CAM";

/* ── Camera defaults (used by setup_cam) ────────────────────────── */
#define CAM_JPEG_SIZE    OV2640_1024x768   // see resolution table in cam_setup.h
#define CAM_JPEG_QUALITY 0x2A              // 0x36-low … 0x1C-very-high

#endif /* CONFIG_H */
