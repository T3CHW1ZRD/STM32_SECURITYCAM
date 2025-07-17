#include "network.hpp"
#include "mbed.h"
#include "TCPSocket.h"
#include "config.h"

static EventFlags socket_event;
static TCPSocket socket;

// sigio callback to wake thread
static void on_socket_activity() {
    socket_event.set(0x01);
}

bool connect_to_wifi(const std::string &ssid, const std::string &password) {
    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("No WiFiInterface found.\n");
        return false;
    }

    printf("Connecting to %s...\n", ssid.c_str());
    int ret = wifi->connect(ssid.c_str(), password.c_str(), NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("Wi-Fi connect failed: %d\n", ret);
        return false;
    }

    SocketAddress ip;
    wifi->get_ip_address(&ip);
    printf("Connected! IP address: %s\n", ip.get_ip_address());

    return true;
}

void start_tcp_server() {

    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    socket.open(wifi);

    SocketAddress server_addr;
    // SERVER_IP and SERVER_PORT set in config.h
    server_addr.set_ip_address(SERVER_IP);
    server_addr.set_port(SERVER_PORT);

    printf("Connecting to server at %s:%d...\n", SERVER_IP, SERVER_PORT);
    int ret = socket.connect(server_addr);
    if (ret != 0) {
        printf("Failed to connect: %d\n", ret);
        return;
    }

    printf("Connected to server!\n");

    // Send handshake or hello
    const char* hello = "Hello from STM32\n";
    socket.send(hello, strlen(hello));

    // Set non-blocking + sigio
    socket.set_blocking(false);
    socket.sigio(callback(on_socket_activity));

    // Enter deep sleep receive loop
    char buffer[256];
    while (true) {
        // Sleep until data arrives
        socket_event.wait_any(0x01);

        while (true) {
            int size = socket.recv(buffer, sizeof(buffer) - 1);
            if (size <= 0) break;

            buffer[size] = '\0';
            printf("Received: %s\n", buffer);
        }
    }

}
