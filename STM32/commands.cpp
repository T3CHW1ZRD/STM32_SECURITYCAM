#include "commands.hpp"
#include "aes_util.hpp"
#include "network.hpp"      // for send_packet()
#include <cstdlib>
#include <cstring>

// Build, encrypt and send a CommandPacket; timestamp=0
static int send_command(uint8_t id, uint16_t arg,
                        const uint8_t *data, uint32_t data_len) {
    uint8_t pad = (16 - (data_len % 16)) % 16;
    uint32_t total_len = sizeof(CommandPacket) + data_len + pad;

    uint8_t *plain = (uint8_t*)malloc(total_len);
    uint8_t *cipher = (uint8_t*)malloc(total_len);
    if (!plain || !cipher) {
        free(plain); free(cipher);
        return -1;
    }

    // load key
    uint8_t key[24];
    if (!load_aes_key(key, sizeof(key))) {
        free(plain); free(cipher);
        return -1;
    }

    // populate header
    CommandPacket *pkt = (CommandPacket*)plain;
    pkt->command_id   = id;
    pkt->command_arg  = arg;
    pkt->data_len     = data_len;
    pkt->padding_len  = pad;
    pkt->timestamp    = 0;
    if (data_len) {
        memcpy(pkt->data, data, data_len);
        if (pad) memset(pkt->data + data_len, 0, pad);
    }

    // IV + encrypt
    uint8_t iv[16];
    fill_random(iv, sizeof(iv));
    aes_cbc_encrypt(key, sizeof(key), iv, plain, cipher, total_len);

    // send IV || ciphertext
    int sent = send_packet(iv, sizeof(iv));
    if (sent > 0) {
        sent = send_packet(cipher, total_len);
    }

    free(plain);
    free(cipher);
    return sent;
}

int send_send_photo(const uint8_t *data, uint32_t len) {
    return send_command(CMD_SEND_PHOTO, 0, data, len);
}

int send_alarm_tripped(void) {
    return send_command(CMD_ALARM_TRIPPED, 0, nullptr, 0);
}

void process_incoming_command(const CommandPacket *pkt) {
    if (pkt->command_id == CMD_GET_PHOTO) {
        on_get_photo();
    }
    // else ignore
}
