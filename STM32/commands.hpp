// File: commands.hpp
#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <cstdint>

// Packed command packet header (no gaps)
struct __attribute__((packed)) CommandPacket {
    uint8_t  command_id;     // Identifies the command
    uint16_t command_arg;    // Optional argument
    uint32_t data_len;       // Length of payload
    uint8_t  padding_len;    // Padding to multiple of 16
    uint32_t timestamp;      // Timestamp (set to 0 when sending)
    uint8_t  data[];         // Flexible array member
};

// Command IDs
static constexpr uint8_t CMD_GET_PHOTO      = 0x01;  // Server → Device
static constexpr uint8_t CMD_SEND_PHOTO     = 0x02;  // Device → Server
static constexpr uint8_t CMD_ALARM_TRIPPED  = 0x03;  // Device → Server

// Application hook for inbound server command
void on_get_photo(void);

// Device‑side senders (defined in commands.cpp)
int send_send_photo(const uint8_t *data, uint32_t len);
int send_alarm_tripped(void);

// Dispatcher for inbound encrypted packets
void process_incoming_command(const CommandPacket *pkt);

#endif // COMMANDS_HPP
