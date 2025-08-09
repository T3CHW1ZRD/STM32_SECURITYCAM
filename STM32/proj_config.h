#ifndef CONFIG_H
#define CONFIG_H

/* ── Network (server) ─────────────────────────────────────── */
static const char* SERVER_IP   = "192.168.0.29";
static int         SERVER_PORT = 12345;

/* ── Wi-Fi auth mode ──────────────────────────────────────── */
enum WiFiAuthMode {
    WIFI_AUTH_PSK = 0,
    WIFI_AUTH_ENTERPRISE = 1
};
#define WIFI_AUTH_MODE WIFI_AUTH_PSK   // change to WIFI_AUTH_ENTERPRISE for WPA/WPA2-Enterprise

/* ── Wi-Fi PSK (Personal) ─────────────────────────────────── */
static const char* WIFI_SSID     = "YourNetworkName";
static const char* WIFI_PASSWORD = "YourPassword";

/* ── Wi-Fi Enterprise (802.1X/EAP) ──────────────────────────
   For PEAP/MSCHAPv2 you typically need ID/username/password (+ optional CA).
   For EAP-TLS you need CA + client cert + client key.
   Store cert/keys on your filesystem and point these paths at them.
*/
enum WiFiEapMethod { WIFI_EAP_PEAP = 1, WIFI_EAP_TLS = 2 };
#define WIFI_EAP_METHOD WIFI_EAP_PEAP

// Outer identity shown to RADIUS (often same as username or "anonymous@realm")
static const char* WIFI_EAP_IDENTITY         = "user@realm";
// Inner identity (for PEAP/MSCHAPv2)
static const char* WIFI_EAP_USERNAME         = "user";
static const char* WIFI_EAP_PASSWORD_EAP     = "pass";

// PEM files (full paths) for validating/authenticating to the RADIUS server
static const char* WIFI_EAP_CA_CERT_PATH     = "/fs/ca.pem";      // optional for PEAP, required for TLS
static const char* WIFI_EAP_CLIENT_CERT_PATH = "/fs/client.crt";  // required for TLS
static const char* WIFI_EAP_CLIENT_KEY_PATH  = "/fs/client.key";  // required for TLS

/* ── Optional: static IP for THIS device ───────────────────── */
#define WIFI_USE_STATIC_IP 0
static const char* WIFI_STATIC_IP      = "192.168.0.50";
static const char* WIFI_STATIC_NETMASK = "255.255.255.0";
static const char* WIFI_STATIC_GATEWAY = "192.168.0.1";

/* ── BLE ──────────────────────────────────────────────────── */
static const char* BLE_NAME    = "SEC-CAM";

/* ── Camera defaults (used by setup_cam) ──────────────────── */
#define CAM_JPEG_SIZE    OV2640_320x240
#define CAM_JPEG_QUALITY 0x2A             // 0x36-low … 0x1C-very-high

/* Path on your LittleFileSystem where the 24-byte AES-192 key lives */
static constexpr const char* AES_KEY_PATH = "/fs/key.txt";

#endif /* CONFIG_H */
