#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <stdexcept>
#include <string>

namespace NetDiscovery {

/**
 * @brief RAII manager for the Windows Networking Stack (Winsock).
 * 
 * Ensures WSAStartup is called exactly once per application lifetime,
 * and WSACleanup is called when the application exits.
 * 
 * Should be instantiated as early as possible in main().
 */
class NetworkStackGuard {
public:
    NetworkStackGuard();
    ~NetworkStackGuard();

    // Prevent copying and moving
    NetworkStackGuard(const NetworkStackGuard&) = delete;
    NetworkStackGuard& operator=(const NetworkStackGuard&) = delete;
    NetworkStackGuard(NetworkStackGuard&&) = delete;
    NetworkStackGuard& operator=(NetworkStackGuard&&) = delete;
};

} // namespace NetDiscovery
#endif
