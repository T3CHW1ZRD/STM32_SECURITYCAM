#ifndef WIFI_CREDENTIALS_HPP
#define WIFI_CREDENTIALS_HPP

#include <string>

struct WifiCredentials {
    std::string ssid;
    std::string password;
};

void init_filesystem();
WifiCredentials prompt_user_input();
bool load_credentials(WifiCredentials &cred);
void save_credentials(const WifiCredentials &cred);

#endif
