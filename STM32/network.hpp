#ifndef NETWORK_HPP
#define NETWORK_HPP

#include "wifi_credentials.hpp"
#include "commands.hpp"
#include <string>

// Bring up Wi‑Fi
bool connect_to_wifi(const std::string &ssid, const std::string &pwd);

// Do plain-text challenge/response handshake
bool perform_handshake();

// Enter encrypted receive loop (never returns)
void start_secure_client();

#endif // NETWORK_HPP
