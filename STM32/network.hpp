#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <cstdint>
#include "nsapi_types.h"    // for nsapi_error_t
#include "commands.hpp"

// Connect to Wi-Fi; returns NSAPI_ERROR_OK on success or an NSAPI_ERROR_* on failure
nsapi_error_t connect_to_wifi(const char *ssid, const char *pwd);

// Blocking challenge/response handshake
bool perform_handshake();

// Encrypted receive loop (never returns)
void start_secure_client();

// Low-level send used by commands.cpp
int send_packet(const void *buf, uint32_t len);

#endif
