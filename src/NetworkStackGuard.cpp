#include "../include/NetworkStackGuard.h"

#ifdef _WIN32
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

namespace NetDiscovery {

NetworkStackGuard::NetworkStackGuard() {
    WSADATA wsaData{};
    const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        throw std::runtime_error("NetworkStackGuard: WSAStartup failed with error: " + std::to_string(result));
    }
    std::cout << "[Network] WSAStartup (NetworkStackGuard initialized)\n";
}

NetworkStackGuard::~NetworkStackGuard() {
    std::cout << "[Network] WSACleanup (NetworkStackGuard destroyed)\n";
    WSACleanup();
}

} // namespace NetDiscovery
#endif
