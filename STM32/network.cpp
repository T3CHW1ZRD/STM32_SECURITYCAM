// File: network.cpp
#include "network.hpp"
#include "proj_config.h"
#include "aes_util.hpp"
#include "commands.hpp"

#include "mbed.h"
#include "mbed_power_mgmt.h"      // DeepSleepLock
#include "TCPSocket.h"
#include "WiFiAccessPoint.h"
#include "events/EventQueue.h"
#include "rtos/ThisThread.h"
#include "rtos/Thread.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <chrono>

// Optional subsystems; remove if unused to save flash/ram.
#include "cam_thread.hpp"
#include "tof_monitor.hpp"
#include "bluetooth_alert.hpp"

using namespace std::chrono_literals;

// ---- Socket, events, and I/O protection ------------------------------------

static TCPSocket  socket;
static EventFlags evt;

static rtos::Mutex io_mutex; // protect all socket I/O visible from multiple threads
void socket_io_lock()   { io_mutex.lock(); }
void socket_io_unlock() { io_mutex.unlock(); }

static constexpr uint32_t EVT_RX = 0x01;
static constexpr uint32_t EVT_TX = 0x02;

// sigio callback (ISR context): signal RX/TX readiness
static void on_socket_activity() {
    evt.set(EVT_RX | EVT_TX);
}

// Low-level send used by commands.cpp. Deep sleep locked only during driver calls.
int send_packet(const void *buf, uint32_t len) {
    const uint8_t *p = static_cast<const uint8_t *>(buf);
    uint32_t sent_total = 0;

    while (sent_total < len) {
        int ret;
        {   DeepSleepLock lock;  ret = socket.send(p + sent_total, len - sent_total); }
        if (ret == NSAPI_ERROR_WOULD_BLOCK) { evt.wait_any(EVT_TX); continue; }
        if (ret < 0) return ret;
        sent_total += static_cast<uint32_t>(ret);
    }
    return static_cast<int>(sent_total);
}

// ---- Wi-Fi bring-up with scan + retry/backoff -------------------------------

static nsapi_security_t resolve_security_and_channel(WiFiInterface *wifi,
                                                     const char *ssid,
                                                     int *out_channel) {
    *out_channel = 0;
    int count = wifi->scan(nullptr, 0);
    if (count <= 0) return NSAPI_SECURITY_WPA_WPA2;

    WiFiAccessPoint *aps = new WiFiAccessPoint[count];
    int n = wifi->scan(aps, count);
    int best = -1, best_rssi = -1000;

    for (int i = 0; i < n; ++i) {
        if (std::strcmp(aps[i].get_ssid(), ssid) == 0) {
            const int rssi = aps[i].get_rssi();
            if (rssi > best_rssi) { best = i; best_rssi = rssi; }
        }
    }

    nsapi_security_t sec = NSAPI_SECURITY_WPA_WPA2;
    if (best >= 0) {
        sec = aps[best].get_security();
        *out_channel = aps[best].get_channel(); // 0 if unknown
        printf("Found SSID \"%s\": sec=%d, channel=%d, RSSI=%d\n",
               ssid, static_cast<int>(sec), *out_channel, best_rssi);
    } else {
        printf("SSID \"%s\" not seen in scan; using default security WPA/WPA2.\n", ssid);
    }
    delete[] aps;
    return sec;
}

/* Enterprise connect shim
   NOTE: Mbed's generic WiFiInterface API is SSID+pass only; Enterprise (EAP)
   is exposed only by some vendor drivers (e.g., ODIN-W2).
   On platforms without such API, we return NSAPI_ERROR_UNSUPPORTED with a clear message.
*/
static nsapi_error_t connect_enterprise(WiFiInterface *wifi, const char *ssid) {
    (void)wifi; (void)ssid;

    // If you switch hardware to u-blox ODIN-W2, we can add the specific calls here
    // (PEAP and EAP-TLS), using WIFI_EAP_* params from config.hpp.

    printf("WPA/WPA2-Enterprise requested but not supported by this platform's Wi-Fi driver via Mbed OS.\n");
    printf("Use PSK mode or move to a Wi-Fi driver that exposes EAP (e.g., u-blox ODIN-W2).\n");
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t connect_to_wifi(const char *ssid, const char *pwd) {
    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    if (!wifi) { printf("No WiFiInterface found\n"); return NSAPI_ERROR_NO_CONNECTION; }

    // Optional: configure static IP
#if defined(WIFI_USE_STATIC_IP) && (WIFI_USE_STATIC_IP)
    {
        SocketAddress ip, nm, gw;
        ip.set_ip_address(WIFI_STATIC_IP);
        nm.set_ip_address(WIFI_STATIC_NETMASK);
        gw.set_ip_address(WIFI_STATIC_GATEWAY);
        nsapi_error_t e1 = wifi->set_dhcp(false);
        if (e1 != NSAPI_ERROR_OK) printf("set_dhcp(false) failed (%d)\n", e1);
        nsapi_error_t e2 = wifi->set_network(ip, nm, gw);
        if (e2 != NSAPI_ERROR_OK) {
            printf("set_network(%s/%s gw %s) failed (%d)\n",
                   WIFI_STATIC_IP, WIFI_STATIC_NETMASK, WIFI_STATIC_GATEWAY, e2);
        } else {
            printf("Static IP configured: %s / %s gw %s\n",
                   WIFI_STATIC_IP, WIFI_STATIC_NETMASK, WIFI_STATIC_GATEWAY);
        }
    }
#else
    wifi->set_dhcp(true);
#endif

    // Enterprise branch (returns immediately on unsupported drivers)
#if (WIFI_AUTH_MODE == WIFI_AUTH_ENTERPRISE)
    printf("Connecting with WPA/WPA2-Enterprise (method=%s)\n",
           (WIFI_EAP_METHOD == WIFI_EAP_TLS) ? "EAP-TLS" : "PEAP/MSCHAPv2");
    return connect_enterprise(wifi, ssid);
#else
    // PSK (Personal) path below
#endif

    int channel = 0;
    nsapi_security_t sec = resolve_security_and_channel(wifi, ssid, &channel);
    if (channel > 0) {
        nsapi_error_t ch = wifi->set_channel(static_cast<uint8_t>(channel));
        if (ch == NSAPI_ERROR_OK) printf("Locking to channel %d\n", channel);
    }

    const int max_attempts = 6;
    nsapi_error_t last = NSAPI_ERROR_NO_CONNECTION;

    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        wifi->disconnect();
        rtos::ThisThread::sleep_for(200ms); // allow radio to settle

        printf("Wi-Fi connect attempt %d/%d...\n", attempt, max_attempts);
        nsapi_error_t ret;
        {   DeepSleepLock lock; ret = wifi->connect(ssid, pwd, sec); }

        if (ret == NSAPI_ERROR_OK) {
            SocketAddress ip;
            for (int i = 0; i < 10; ++i) {
                if (wifi->get_ip_address(&ip) == NSAPI_ERROR_OK && ip.get_ip_address()) break;
                rtos::ThisThread::sleep_for(200ms);
            }
            if (ip.get_ip_address()) { printf("Device IP: %s\n", ip.get_ip_address()); return NSAPI_ERROR_OK; }
            printf("Connected but no IP yet, retrying...\n");
            last = NSAPI_ERROR_NO_ADDRESS;
        } else {
            printf("Wi-Fi connect failed (%d)\n", ret);
            last = ret;
        }

        const uint32_t ms = 200 + attempt * 300 + (rand() % 200);
        rtos::ThisThread::sleep_for(std::chrono::milliseconds(ms));

        if (attempt % 3 == 0) {
            sec = resolve_security_and_channel(wifi, ssid, &channel);
            if (channel > 0) wifi->set_channel(static_cast<uint8_t>(channel));
        }
    }

    return last;
}

// ---- Encrypted handshake + client loop (unchanged) --------------------------

bool perform_handshake() {
    uint8_t challenge[16], iv[16], resp[16], plain[16], key[24];

    fill_random(challenge, sizeof(challenge));
    fill_random(iv,        sizeof(iv));

    bool ok = true;
    {
        DeepSleepLock lock;
        io_mutex.lock();
        if (socket.send(challenge, 16) != 16) ok = false;
        if (ok && socket.send(iv, 16) != 16)   ok = false;
        if (ok) { int n = socket.recv(resp, 16); ok = (n == 16); }
        io_mutex.unlock();
    }

    if (!ok) { printf("Handshake: I/O error\n"); return false; }

    if (!load_aes_key(key, sizeof(key))) { printf("Handshake: failed to load key\n"); return false; }

    aes_cbc_decrypt(key, sizeof(key), iv, resp, plain, 16);
    if (memcmp(plain, challenge, 16) != 0) { printf("Handshake: challenge mismatch\n"); return false; }
    return true;
}

void start_secure_client() {
    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    socket.open(wifi);

    SocketAddress addr;
    addr.set_ip_address(SERVER_IP);
    addr.set_port(SERVER_PORT);

    for (int attempt = 1; attempt <= 2; ++attempt) {
        printf("Connecting to %s:%d (attempt %d)...\n", SERVER_IP, SERVER_PORT, attempt);
        nsapi_error_t ret;
        {   DeepSleepLock lock; ret = socket.connect(addr); }
        if (ret == NSAPI_ERROR_OK) { printf("Connected on attempt %d.\n", attempt); break; }
        printf("Connect attempt %d failed: %d\n", attempt, ret);
        if (attempt == 2) { printf("Failed to connect after 2 attempts.\n"); return; }
        rtos::ThisThread::sleep_for(2s);
    }

    socket.set_blocking(true);
    for (int attempt = 1; attempt <= 2; ++attempt) {
        printf("Handshake attempt %d...\n", attempt);
        if (perform_handshake()) { printf("Handshake OK on attempt %d.\n", attempt); break; }
        printf("Handshake attempt %d failed.\n", attempt);
        if (attempt == 2) { printf("Handshake failed after 2 attempts.\n"); return; }
        rtos::ThisThread::sleep_for(1s);
    }

    socket.set_blocking(false);
    socket.sigio(callback(on_socket_activity));

    // Optional: BLE and sensor threads
    static events::EventQueue ble_q(32 * EVENTS_EVENT_SIZE);
    static rtos::Thread       ble_thread(osPriorityNormal, 4096);
    static BluetoothAlert     bt(ble_q);
    ble_thread.start(callback(&ble_q, &events::EventQueue::dispatch_forever));
    rtos::ThisThread::sleep_for(50ms);
    bt.init();

    start_cam_thread(io_mutex);
    start_tof_monitor(/*trigger_range_mm=*/600, /*min_noise=*/20, &ble_q, &bt);

    while (true) {
        evt.wait_any(EVT_RX); // OS may deep sleep here

        uint8_t iv[16], c0[16];
        bool ok = true;

        {   DeepSleepLock lock; io_mutex.lock();
            if (socket.recv(iv, 16) != 16)       ok = false;
            if (ok && socket.recv(c0, 16) != 16) ok = false;
            io_mutex.unlock();
        }
        if (!ok) continue;

        uint8_t p0[16], key[24];
        if (!load_aes_key(key, sizeof(key))) continue;
        aes_cbc_decrypt(key, sizeof(key), iv, c0, p0, 16);

        uint8_t  cmd_id    = p0[0];
        uint16_t cmd_arg   = static_cast<uint16_t>(p0[1] | (p0[2] << 8));
        uint32_t data_len  = static_cast<uint32_t>(p0[3] | (p0[4] << 8) | (p0[5] << 16) | (p0[6] << 24));
        uint8_t  pad_len   = p0[7];
        uint32_t timestamp = static_cast<uint32_t>(p0[8] | (p0[9] << 8) | (p0[10] << 16) | (p0[11] << 24));

        uint32_t plain_size  = static_cast<uint32_t>(sizeof(CommandPacket)) + data_len + pad_len;
        uint32_t cipher_size = ((plain_size + 15) / 16) * 16;
        uint32_t rem_cipher  = cipher_size - 16;

        auto *c_rest = (uint8_t *)malloc(rem_cipher ? rem_cipher : 1);
        if (!c_rest) continue;

        int got = 0;
        {   DeepSleepLock lock; io_mutex.lock();
            got = (rem_cipher ? socket.recv(c_rest, rem_cipher) : 0);
            io_mutex.unlock();
        }
        if (got != static_cast<int>(rem_cipher)) { free(c_rest); continue; }

        auto *full_cipher = (uint8_t *)malloc(cipher_size);
        if (!full_cipher) { free(c_rest); continue; }
        memcpy(full_cipher, c0, 16);
        if (rem_cipher) memcpy(full_cipher + 16, c_rest, rem_cipher);
        free(c_rest);

        auto *plain = (uint8_t *)malloc(cipher_size);
        if (!plain) { free(full_cipher); continue; }
        aes_cbc_decrypt(key, sizeof(key), iv, full_cipher, plain, cipher_size);
        free(full_cipher);

        CommandPacket *pkt = reinterpret_cast<CommandPacket *>(plain);
        pkt->command_id   = cmd_id;
        pkt->command_arg  = cmd_arg;
        pkt->data_len     = data_len;
        pkt->padding_len  = pad_len;
        pkt->timestamp    = timestamp;

        process_incoming_command(pkt);
        free(plain);
    }
}
