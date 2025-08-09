#ifndef CONFIG_H
#define CONFIG_H

/* ── Network ──────────────────────────────────────────────── */
static const char* SERVER_IP   = "172.20.10.3";
static int         SERVER_PORT = 5001;

/* ── BLE ──────────────────────────────────────────────── */
static const char* BLE_NAME    = "SEC-CAM";

/* ── Camera defaults (used by setup_cam) ────────────────────────── */
#define CAM_JPEG_SIZE    OV2640_320x240   // see resolution table in cam_setup.h
#define CAM_JPEG_QUALITY 0x2A              // 0x36-low … 0x1C-very-high

// Path on your LittleFileSystem where the 24-byte AES-192 key lives
static constexpr const char*   AES_KEY_PATH = "/fs/key.txt";

#endif /* CONFIG_H */
