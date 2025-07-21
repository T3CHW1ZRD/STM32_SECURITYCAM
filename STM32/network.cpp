#include "network.hpp"
#include "config.h"
#include "aes_util.hpp"
#include "mbed.h"
#include "TCPSocket.h"

static TCPSocket socket;
static EventFlags evt;

// Sigio callback
static void on_socket_activity() { evt.set(0x01); }

bool connect_to_wifi(const std::string &ssid, const std::string &pwd) {
    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    if (!wifi) return false;
    if (wifi->connect(ssid.c_str(), pwd.c_str(), NSAPI_SECURITY_WPA_WPA2) != 0)
        return false;
    printf("Wi-Fi up. IP: %s\n", wifi->get_ip_address());
    return true;
}

bool perform_handshake() {
    uint8_t challenge[16], iv[16];
    fill_random(challenge, sizeof(challenge));
    fill_random(iv,        sizeof(iv));

    // send plain challenge ⨁ IV
    if (socket.send(challenge, 16) < 0) return false;
    if (socket.send(iv,        16) < 0) return false;

    // await encrypted echo (16 bytes)
    uint8_t resp[16];
    if (socket.recv(resp, sizeof(resp)) != 16) return false;

    // decrypt and compare
    uint8_t plain[16], key[24];
    if (!load_aes_key(key, sizeof(key))) return false;
    aes_cbc_decrypt(key, sizeof(key), iv, resp, plain, sizeof(plain));

    return (memcmp(plain, challenge, sizeof(challenge)) == 0);
}

void start_secure_client() {
    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    socket.open(wifi);

    SocketAddress addr;
    addr.set_ip_address(SERVER_IP);
    addr.set_port(SERVER_PORT);
    socket.connect(addr);

    if (!perform_handshake()) {
        printf("Handshake failed\n");
        return;
    }
    printf("Handshake OK\n");

    socket.set_blocking(false);
    socket.sigio(callback(on_socket_activity));

    while (true) {
        evt.wait_any(0x01);

        // read IV
        uint8_t iv[16];
        if (socket.recv(iv, sizeof(iv)) <= 0) continue;

        // read header (8 bytes)
        uint8_t hdr[8];
        if (socket.recv(hdr, sizeof(hdr)) != 8) continue;

        uint32_t data_len  = *(uint32_t*)(hdr + 3);
        uint8_t  pad       = hdr[7];
        uint32_t total     = sizeof(CommandPacket) + data_len + pad;

        // read ciphertext
        uint8_t *cipher = (uint8_t*)malloc(total);
        socket.recv(cipher, total);

        // decrypt
        uint8_t *plain = (uint8_t*)malloc(total);
        uint8_t key[24];
        load_aes_key(key, sizeof(key));
        aes_cbc_decrypt(key, sizeof(key), iv, cipher, plain, total);

        // dispatch
        process_incoming_command((CommandPacket*)plain);

        free(cipher);
        free(plain);
    }
}
