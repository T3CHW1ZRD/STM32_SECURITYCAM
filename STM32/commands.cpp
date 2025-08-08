//commands.cpp

#include "commands.hpp"
#include "aes_util.hpp"
#include "network.hpp"
#include <cstdlib>
#include <cstring>


// Internal helper: builds a CommandPacket, sets timestamp, encrypts & sends
// Internal helper: builds a 12-byte header, streams AES-CBC, sends IV + ciphertext
static int send_command(uint8_t         id,
                        uint16_t        arg,
                        uint32_t        timestamp,
                        const uint8_t  *data,
                        uint32_t        data_len)
{
    // ----- 0) compute header + padding (header is ALWAYS 12 bytes on the wire) -----
    static constexpr uint32_t HEADER_LEN = 12;
    uint8_t pad = static_cast<uint8_t>((16 - ((HEADER_LEN + data_len) % 16)) % 16);

    // ----- 1) load key -----
    uint8_t key[24];
    if (!load_aes_key(key, sizeof key)) {
        return -1;
    }

    // ----- 2) build 12-byte header in a raw byte buffer -----
    uint8_t hdr[HEADER_LEN];
    hdr[0]  = id;
    hdr[1]  = static_cast<uint8_t>(arg & 0xFF);
    hdr[2]  = static_cast<uint8_t>((arg >> 8) & 0xFF);
    hdr[3]  = static_cast<uint8_t>(data_len & 0xFF);
    hdr[4]  = static_cast<uint8_t>((data_len >> 8) & 0xFF);
    hdr[5]  = static_cast<uint8_t>((data_len >> 16) & 0xFF);
    hdr[6]  = static_cast<uint8_t>((data_len >> 24) & 0xFF);
    hdr[7]  = pad;
    hdr[8]  = static_cast<uint8_t>(timestamp & 0xFF);
    hdr[9]  = static_cast<uint8_t>((timestamp >> 8) & 0xFF);
    hdr[10] = static_cast<uint8_t>((timestamp >> 16) & 0xFF);
    hdr[11] = static_cast<uint8_t>((timestamp >> 24) & 0xFF);

    // ----- 3) init IV and streaming AES-CBC -----
    uint8_t iv[16];
    fill_random(iv, sizeof iv);

    aes_cbc_stream s;
    aes_cbc_stream_init(&s, key, sizeof key, iv, /*encrypt=*/true);

    // Small staging buffers so we only ever feed multiples of 16 to AES
    uint8_t inblk[16], outblk[16];
    size_t  carry = 0;

    auto feed_byte = [&](uint8_t b) {
        inblk[carry++] = b;
        if (carry == 16) {
            aes_cbc_stream_update(&s, inblk, outblk, 16);
            send_packet(outblk, 16);
            carry = 0;
        }
    };

    // ----- 4) send IV, then stream-encrypt header + data + padding -----
    socket_io_lock();

    int sent = send_packet(iv, sizeof iv);
    if (sent <= 0) {
        socket_io_unlock();
        aes_cbc_stream_free(&s);
        return sent;
    }

    // 4a) header (12 bytes)
    for (size_t i = 0; i < HEADER_LEN; ++i) feed_byte(hdr[i]);

    // 4b) payload (may be nullptr if data_len == 0)
    if (data_len && data) {
        // push the raw data as-is; we don’t need a second buffer
        for (uint32_t i = 0; i < data_len; ++i) feed_byte(data[i]);
    }

    // 4c) padding with zeros so total plaintext % 16 == 0
    for (uint8_t i = 0; i < pad; ++i) feed_byte(0);

    // At this point ‘carry’ must be 0 (we finished on a 16-byte boundary)
    socket_io_unlock();

    aes_cbc_stream_free(&s);
    return 1;   // we already sent everything in chunks
}


// Public APIs — always timestamp = 0 for now
int send_send_photo(const uint8_t *data, uint32_t len) {
    return send_command(CMD_SEND_PHOTO, 0, /*ts=*/0, data, len);
}

int send_alarm_tripped(void) {
    return send_command(CMD_ALARM_TRIPPED, 0, /*ts=*/0, nullptr, 0);
}
// New chunked-photo send helpers — reuse CMD_SEND_PHOTO
int send_photo_start(uint32_t session_id, uint32_t total_len,
                     const uint8_t* first_bytes, uint32_t n_first_bytes,
                     bool single_chunk)
{
    // Build payload: [total_len (4 bytes LE)] + first JPEG bytes
    const uint32_t L = 4 + n_first_bytes;
    uint8_t *buf = new uint8_t[L];
    buf[0] = static_cast<uint8_t>( total_len        & 0xFF);
    buf[1] = static_cast<uint8_t>((total_len >> 8)  & 0xFF);
    buf[2] = static_cast<uint8_t>((total_len >> 16) & 0xFF);
    buf[3] = static_cast<uint8_t>((total_len >> 24) & 0xFF);

    if (n_first_bytes && first_bytes) {
        memcpy(buf + 4, first_bytes, n_first_bytes);
    }

    uint16_t arg = PHOTO_FLAG_START | (single_chunk ? PHOTO_FLAG_LAST : 0);
    int ret = send_command(CMD_SEND_PHOTO, arg, session_id, buf, L);
    delete[] buf;
    return ret;
}

int send_photo_chunk(uint32_t session_id, uint16_t seq,
                     const uint8_t* bytes, uint32_t n_bytes, bool is_last)
{
    uint16_t arg = ((seq & 0xFF) << PHOTO_SEQ_SHIFT) |
                   (is_last ? PHOTO_FLAG_LAST : 0);
    return send_command(CMD_SEND_PHOTO, arg, session_id, bytes, n_bytes);
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
