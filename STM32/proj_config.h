#ifndef CONFIG_H
#define CONFIG_H

/* ── Network (server) ─────────────────────────────────────── */
static const char* SERVER_IP   = "192.168.0.29";
static int         SERVER_PORT = 12345;

/* ── Wi-Fi (client) ───────────────────────────────────────── */
static const char* WIFI_SSID     = "Hotspot";
static const char* WIFI_PASSWORD = "hotspot1";

/* Set to 1 to use static IP, 0 for DHCP */
#define WIFI_USE_STATIC_IP 0
static const char* WIFI_STATIC_IP      = "192.168.0.50";
static const char* WIFI_STATIC_NETMASK = "255.255.255.0";
static const char* WIFI_STATIC_GATEWAY = "192.168.0.1";

/* ── BLE ──────────────────────────────────────────────────── */
static const char* BLE_NAME    = "SEC-CAM";

/* ── Camera defaults (used by setup_cam) ──────────────────── */
#define CAM_JPEG_SIZE    OV2640_320x240   // see resolution table in cam_setup.h
#define CAM_JPEG_QUALITY 0x2A             // 0x36-low … 0x1C-very-high

/* Path on your LittleFileSystem where the 24-byte AES-192 key lives */
static constexpr const char* AES_KEY_PATH = "/fs/key.txt";

#endif /* CONFIG_H */
