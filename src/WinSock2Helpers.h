/**
 * @file WinSock2Helpers.h
 * @brief INTERNAL WinSock2 implementation helpers.
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │  INTERNAL — include ONLY from .cpp files.                   │
 * │  Never include from public headers (include/ directory).    │
 * │  This file contains platform-specific (Windows) code.       │
 * └─────────────────────────────────────────────────────────────┘
 *
 * Shared between UdpSocket.cpp and MulticastSocket.cpp to eliminate
 * code duplication. Do not add any NetDiscovery domain logic here.
 *
 * To port to ESP-IDF:
 *   Replace this file with an equivalent using lwIP / POSIX headers.
 *   The function signatures must remain identical so that .cpp files
 *   that include this header compile without modification.
 */

#pragma once

// WinSock2 must come before windows.h.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601   // Windows 7 minimum
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace NetDiscovery {
namespace Platform {

// ============================================================
// WsaErrorString
// ============================================================

/**
 * @brief Convert a WinSock error code to a descriptive string.
 * @param errorCode  Value from WSAGetLastError().
 * @return Formatted string: "WinSock error <N>: <OS message>"
 */
inline std::string WsaErrorString(int errorCode)
{
    char* msgBuf = nullptr;
    const DWORD msgLen = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM     |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(errorCode),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&msgBuf),
        0,
        nullptr
    );

    std::string result = "WinSock error " + std::to_string(errorCode) + ": ";
    if (msgLen > 0 && msgBuf) {
        result += msgBuf;
        while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) {
            result.pop_back();
        }
        LocalFree(msgBuf);
    } else {
        result += "(unknown)";
    }
    return result;
}

// ============================================================
// SockAddrToString
// ============================================================

/**
 * @brief Convert a sockaddr_in to a dotted-decimal IPv4 string.
 * @param addr  Source address.
 * @return "192.168.x.y" style string.
 */
inline std::string SockAddrToString(const sockaddr_in& addr)
{
    char buf[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    return std::string(buf);
}

// ============================================================
// SetSocketTimeoutMs
// ============================================================

/**
 * @brief Apply SO_RCVTIMEO to a socket (milliseconds, on Windows).
 * @param sock           Socket descriptor.
 * @param timeoutSeconds Timeout in whole seconds.
 * @throws std::runtime_error if setsockopt fails.
 */
inline void SetSocketTimeoutMs(SOCKET sock, int timeoutSeconds)
{
    const DWORD ms = static_cast<DWORD>(timeoutSeconds) * 1000u;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&ms), sizeof(ms)) == SOCKET_ERROR) {
        throw std::runtime_error(
            "setsockopt(SO_RCVTIMEO) failed: " + WsaErrorString(WSAGetLastError())
        );
    }
}

// ============================================================
// SocketGuard
// ============================================================

/**
 * @brief RAII wrapper that calls closesocket() on destruction.
 *
 * Used for temporary sockets (e.g. MulticastSocket inside ListenPassive)
 * to guarantee cleanup on all exit paths including exceptions.
 */
struct SocketGuard {
    SOCKET handle{INVALID_SOCKET};

    explicit SocketGuard(SOCKET s) noexcept : handle(s) {}

    ~SocketGuard() noexcept
    {
        if (handle != INVALID_SOCKET) {
            closesocket(handle);
            handle = INVALID_SOCKET;
        }
    }

    // Non-copyable.
    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;

    // Movable — transfers ownership.
    SocketGuard(SocketGuard&& other) noexcept : handle(other.handle)
    {
        other.handle = INVALID_SOCKET;
    }

    SocketGuard& operator=(SocketGuard&& other) noexcept
    {
        if (this != &other) {
            if (handle != INVALID_SOCKET) closesocket(handle);
            handle = other.handle;
            other.handle = INVALID_SOCKET;
        }
        return *this;
    }

    void Dismiss() noexcept { handle = INVALID_SOCKET; }
};

} // namespace Platform
} // namespace NetDiscovery
