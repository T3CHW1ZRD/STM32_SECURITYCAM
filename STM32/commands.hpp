#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <cstdint>

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

// Device-side senders
int send_send_photo(const uint8_t *data, uint32_t len);
int send_alarm_tripped();

// Dispatch incoming decrypted packets
void process_incoming_command(const CommandPacket *pkt);

#endif 
