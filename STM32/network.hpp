#ifndef NETWORK_HPP
#define NETWORK_HPP

#include "WiFiInterface.h"
#include <cstdint>
#include <string>

// Connects using the given credentials, returns true on success
bool connect_to_wifi(const std::string &ssid, const std::string &password);

// Starts a TCP socket server that listens and prints received data
void start_tcp_server();

// Message packet definition
struct __attribute__((packed)) CommandPacket {
    uint8_t  command_id;     
    uint16_t command_arg;   
    uint32_t data_len;       
    uint8_t  padding_len;    // Extra padding at end of data = 7 when data len is 0 (data_len + 7) % 16
    uint32_t timestamp;      
    uint8_t  data[];         
};

#endif
