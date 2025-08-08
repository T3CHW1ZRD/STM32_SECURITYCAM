#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <cstdint>

// Photo chunking flags (reusing CMD_SEND_PHOTO)
#define PHOTO_FLAG_START   0x0001  // Marks first chunk of a photo
#define PHOTO_FLAG_LAST    0x0002  // Marks final chunk of a photo
#define PHOTO_SEQ_SHIFT    8       // Sequence number is in bits [15:8]
#define PHOTO_SEQ_MASK     0xFF00  // Mask for extracting sequence number

// Packed header (12 bytes) + flexible payload
struct __attribute__((packed)) CommandPacket {
    uint8_t  command_id;     // Identifies the command
    uint16_t command_arg;    // Optional argument
    uint32_t data_len;       // Length of data[] in bytes
    uint8_t  padding_len;    // Padding added to make total payload block-aligned
    uint32_t timestamp;      // Timestamp (set to 0 for now)
    uint8_t  data[];         // Flexible array member
};

// Command IDs
static constexpr uint8_t CMD_GET_PHOTO      = 0x01;  // Server → Device
static constexpr uint8_t CMD_SEND_PHOTO     = 0x02;  // Device → Server
static constexpr uint8_t CMD_ALARM_TRIPPED  = 0x03;  // Device → Server

// Application callback for incoming “Get Photo”
void on_get_photo();
void cam_request_photo();   // comes from cam_thread.hpp

// Device-side senders
int send_send_photo(const uint8_t *data, uint32_t len);
int send_alarm_tripped();
// Chunked photo send helpers
int send_photo_start(uint32_t session_id, uint32_t total_len,
                     const uint8_t* first_bytes, uint32_t n_first_bytes,
                     bool single_chunk);

int send_photo_chunk(uint32_t session_id, uint16_t seq,
                     const uint8_t* bytes, uint32_t n_bytes, bool is_last);
// Dispatch incoming decrypted packets
void process_incoming_command(const CommandPacket *pkt);

#endif 
