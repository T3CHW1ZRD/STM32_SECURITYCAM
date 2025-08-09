#ifndef WIFI_CREDENTIALS_HPP
#define WIFI_CREDENTIALS_HPP

#include <string>

/// Holds SSID and password
struct WifiCredentials {
    std::string ssid;
    std::string password;
};

/// Mounts (or formats+mounts) the onboard filesystem.
void init_filesystem();

/// Prompts the user for SSID/password, echoing each keystroke.
/// Returns a WifiCredentials struct.
WifiCredentials prompt_user_input();

/// Saves credentials to JSON under settings_path.
/// Overwrites any existing file.
void save_credentials(const WifiCredentials &cred);

/// Loads credentials from JSON; returns false if missing or parse error.
bool load_credentials(WifiCredentials &cred);

#endif // WIFI_CREDENTIALS_HPP
