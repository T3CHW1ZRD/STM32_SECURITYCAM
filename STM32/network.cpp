// File: network.cpp

#include "network.hpp"
#include "proj_config.h"         // SERVER_IP, SERVER_PORT, AES_KEY_PATH
#include "aes_util.hpp"
#include "mbed.h"
#include "TCPSocket.h"
#include "rtos/ThisThread.h"
#include "rtos/Thread.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include "cam_thread.hpp"

using namespace std::chrono_literals;

static TCPSocket  socket;
static EventFlags evt;
static rtos::Mutex io_mutex;     // <— protect all socket I/O

// sigio callback
static void on_socket_activity() {
    evt.set(0x01 | 0x02);           // 0x02 = TX space available
}


// low-level send used by commands.cpp
int send_packet(const void *buf, uint32_t len)
{
    const uint8_t *p = static_cast<const uint8_t*>(buf);
    uint32_t sent_total = 0;

    while (sent_total < len) {
        int ret = socket.send(p + sent_total, len - sent_total);

        if (ret == NSAPI_ERROR_WOULD_BLOCK) {
            /* Socket TX buffer full — wait for it to become writable.
               'evt' is already tied to sigio(). 0x01 bit is RX, we'll
               use bit 0x02 for TX-ready. */
            evt.wait_any(0x02, /*timeout*/ 500);   // 500 ms safety
            continue;
        }
        if (ret < 0)               // real error
            return ret;

        sent_total += ret;
    }
    return sent_total;              // all bytes queued
}



// Bring up Wi-Fi, return NSAPI_ERROR_OK or error code
nsapi_error_t connect_to_wifi(const char *ssid, const char *pwd) {
    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("No WiFiInterface found\n");
        return NSAPI_ERROR_NO_CONNECTION;
    }

    ThisThread::sleep_for(500ms);
    nsapi_error_t ret = wifi->connect(ssid, pwd, NSAPI_SECURITY_WPA_WPA2);
    if (ret != NSAPI_ERROR_OK) {
        printf("Wi-Fi connect failed (%d)\n", ret);
        return ret;
    }
    SocketAddress ip;
    ThisThread::sleep_for(500ms);
    wifi->get_ip_address(&ip);
    printf("IP: %s\n", ip.get_ip_address());
    return NSAPI_ERROR_OK;
}

// Blocking handshake as client (all I/O under lock)
bool perform_handshake() {
    uint8_t challenge[16], iv[16], resp[16], plain[16], key[24];

    fill_random(challenge, sizeof(challenge));
    fill_random(iv,        sizeof(iv));

    io_mutex.lock();
    bool ok = true;
    if (socket.send(challenge, 16) != 16) { ok = false; }
    if (ok && socket.send(iv, 16) != 16)   { ok = false; }
    if (ok) {
        int n = socket.recv(resp, 16);
        ok = (n == 16);
    }
    io_mutex.unlock();

    if (!ok) {
        printf("Handshake: I/O error\n");
        return false;
    }

    if (!load_aes_key(key, sizeof(key))) {
        printf("Handshake: failed to load key\n");
        return false;
    }
    aes_cbc_decrypt(key, sizeof(key), iv, resp, plain, 16);
    if (memcmp(plain, challenge, 16) != 0) {
        printf("Handshake: challenge mismatch\n");
        return false;
    }
    return true;
}

void start_secure_client() {
    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    socket.open(wifi);

    SocketAddress addr;
    addr.set_ip_address(SERVER_IP);
    addr.set_port(SERVER_PORT);

    for (int attempt = 1; attempt <= 2; ++attempt) {
        printf("Connecting to %s:%d (attempt %d)...\n",
               SERVER_IP, SERVER_PORT, attempt);
        nsapi_error_t ret = socket.connect(addr);
        if (ret == NSAPI_ERROR_OK) {
            printf("Connected on attempt %d.\n", attempt);
            break;
        }
        printf("Connect attempt %d failed: %d\n", attempt, ret);
        if (attempt == 2) {
            printf("Failed to connect after 2 attempts.\n");
            return;
        }
        ThisThread::sleep_for(2s);
    }

    socket.set_blocking(true);
    for (int attempt = 1; attempt <= 2; ++attempt) {
        printf("Handshake attempt %d...\n", attempt);
        if (perform_handshake()) {
            printf("Handshake OK on attempt %d.\n", attempt);
            break;
        }
        printf("Handshake attempt %d failed.\n", attempt);
        if (attempt == 2) {
            printf("Handshake failed after 2 attempts.\n");
            return;
        }
        ThisThread::sleep_for(1s);
    }

    socket.set_blocking(false);
    socket.sigio(callback(on_socket_activity));

    // Alarm thread: calls send_alarm_tripped() under lock
    static rtos::Thread alarm_thread;
    alarm_thread.start([](){
        srand((unsigned)time(NULL));
        while (true) {
            int wait_s = 5 + (rand() % 11);
            ThisThread::sleep_for(std::chrono::seconds(wait_s));
            printf(">>> Triggering alarm after %d seconds\n", wait_s);

            // send_alarm_tripped() now already logs debug inside send_command()
            int result = send_alarm_tripped();
            printf("send_alarm_tripped() result = %d\n", result);
        }
    });

    // Camera thread — one call, that’s it
    start_cam_thread(io_mutex);

    // Encrypted‐receive loop starts here …
    socket.set_blocking(false);
    socket.sigio(callback(on_socket_activity));
    while (true) {


        evt.wait_any(0x01);

        // 1) Read IV + C0 under lock
        uint8_t iv[16], c0[16];
        io_mutex.lock();
        bool ok = true;
        if (socket.recv(iv, 16) != 16)      ok = false;
        if (ok && socket.recv(c0, 16) != 16) ok = false;
        io_mutex.unlock();
        if (!ok) continue;

        // 2) Decrypt header
        uint8_t p0[16], key[24];
        if (!load_aes_key(key, sizeof(key))) continue;
        aes_cbc_decrypt(key, sizeof(key), iv, c0, p0, 16);

        // 3) Parse header
        uint8_t  cmd_id    = p0[0];
        uint16_t cmd_arg   = p0[1] | (p0[2] << 8);
        uint32_t data_len  = p0[3] | (p0[4] << 8) | (p0[5] << 16) | (p0[6] << 24);
        uint8_t  pad_len   = p0[7];
        uint32_t timestamp = p0[8] | (p0[9] << 8) | (p0[10] << 16) | (p0[11] << 24);

        uint32_t plain_size  = sizeof(CommandPacket) + data_len + pad_len;
        uint32_t cipher_size = ((plain_size + 15) / 16) * 16;
        uint32_t rem_cipher  = cipher_size - 16;

        // 4) Read remaining ciphertext under lock
        auto *c_rest = (uint8_t*)malloc(rem_cipher);
        if (!c_rest) continue;
        io_mutex.lock();
        int got = (rem_cipher ? socket.recv(c_rest, rem_cipher) : 0);
        io_mutex.unlock();
        if (got != (int)rem_cipher) { free(c_rest); continue; }

        // 5) Decrypt full packet
        auto *full_cipher = (uint8_t*)malloc(cipher_size);
        memcpy(full_cipher, c0, 16);
        memcpy(full_cipher + 16, c_rest, rem_cipher);
        free(c_rest);

        auto *plain = (uint8_t*)malloc(cipher_size);
        aes_cbc_decrypt(key, sizeof(key), iv, full_cipher, plain, cipher_size);
        free(full_cipher);

        // 6) Dispatch
        CommandPacket *pkt = reinterpret_cast<CommandPacket*>(plain);
        pkt->command_id   = cmd_id;
        pkt->command_arg  = cmd_arg;
        pkt->data_len     = data_len;
        pkt->padding_len  = pad_len;
        pkt->timestamp    = timestamp;
        process_incoming_command(pkt);

        free(plain);
    }
}

void start_plain_client(const uint8_t *(*get_jpeg)(uint32_t &len)) {
    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    socket.open(wifi);

    SocketAddress addr;
    addr.set_ip_address(SERVER_IP);
    addr.set_port(SERVER_PORT);

    for (int attempt = 1; attempt <= 2; ++attempt) {
        printf("Connecting to %s:%d (attempt %d)...\n",
               SERVER_IP, SERVER_PORT, attempt);
        nsapi_error_t ret = socket.connect(addr);
        if (ret == NSAPI_ERROR_OK) {
            printf("Connected on attempt %d.\n", attempt);
            break;
        }
        printf("Connect attempt %d failed: %d\n", attempt, ret);
        if (attempt == 2) {
            printf("Failed to connect after 2 attempts.\n");
            return;
        }
        ThisThread::sleep_for(2s);
    }

    // JPEG send loop — send [length (4 LE)] + [JPEG bytes]
    while (true) {
        uint32_t len = 0;
        const uint8_t *jpeg = get_jpeg(len);  // blocking capture

        if (!jpeg || len == 0 || len > 2*1024*1024) {
            printf("Invalid JPEG capture\n");
            continue;
        }

        // Build 4-byte header
        uint8_t hdr[4] = {
            (uint8_t)(len & 0xFF),
            (uint8_t)((len >> 8) & 0xFF),
            (uint8_t)((len >> 16) & 0xFF),
            (uint8_t)((len >> 24) & 0xFF)
        };

        io_mutex.lock();
        int sent = socket.send(hdr, 4);
        if (sent == 4) {
            sent = socket.send(jpeg, len);
        }
        io_mutex.unlock();

        if (sent < 0) {
            printf("Send error: %d\n", sent);
        } else {
            printf("Sent JPEG: %lu bytes\n", (unsigned long)len);
        }

        ThisThread::sleep_for(1s);  // ~1 fps
    }
}
