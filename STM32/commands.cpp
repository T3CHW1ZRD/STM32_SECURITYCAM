//commands.cpp

#include "commands.hpp"
#include "aes_util.hpp"
#include "network.hpp"
#include <cstdlib>
#include <cstring>


// Internal helper: builds a CommandPacket, sets timestamp, encrypts & sends
static int send_command(uint8_t         id,
                        uint16_t        arg,
                        uint32_t        timestamp,
                        const uint8_t  *data,
                        uint32_t        data_len)
{
    // 1) Compute unpadded length and initial pad to 16-byte boundary
    uint32_t unpadded = sizeof(CommandPacket) + data_len;
    uint8_t  pad      = (16 - (unpadded % 16)) % 16;


    uint32_t total = unpadded + pad;

    // Allocate buffers
    uint8_t *plain  = (uint8_t*)malloc(total);
    uint8_t *cipher = (uint8_t*)malloc(total);
    if (!plain || !cipher) {
        free(plain);
        free(cipher);
        return -1;
    }

    // Load AES-192 key
    uint8_t key[24];
    if (!load_aes_key(key, sizeof(key))) {
        free(plain);
        free(cipher);
        return -1;
    }

    // Fill header
    auto *pkt = reinterpret_cast<CommandPacket*>(plain);
    pkt->command_id   = id;
    pkt->command_arg  = arg;
    pkt->data_len     = data_len;
    pkt->padding_len  = pad;
    pkt->timestamp    = timestamp;

    // Copy payload (if any)
    if (data_len) {
        memcpy(pkt->data, data, data_len);
    }

    // Zero out padding bytes
    if (pad) {
        memset(plain + unpadded, 0, pad);
    }

    // Encrypt: generate IV and do AES-192-CBC over the entire buffer
    uint8_t iv[16];
    fill_random(iv, sizeof(iv));
    aes_cbc_encrypt(key, sizeof(key), iv, plain, cipher, total);

    // Send IV || ciphertext
    int sent = send_packet(iv, sizeof(iv));
    if (sent > 0) {
        sent = send_packet(cipher, total);
    }

    free(plain);
    free(cipher);
    return sent;
}

// Public APIs — always timestamp = 0 for now
int send_send_photo(const uint8_t *data, uint32_t len) {
    return send_command(CMD_SEND_PHOTO, 0, /*ts=*/0, data, len);
}

int send_alarm_tripped(void) {
    return send_command(CMD_ALARM_TRIPPED, 0, /*ts=*/0, nullptr, 0);
}

// Dispatcher — call on_get_photo() when we see that command
void process_incoming_command(const CommandPacket *pkt) {
    if (pkt->command_id == CMD_GET_PHOTO) {
        on_get_photo();
    }
}

void on_get_photo() {
    cam_request_photo();    // ONE line
}
