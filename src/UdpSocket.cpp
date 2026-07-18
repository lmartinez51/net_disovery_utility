/**
 * @file UdpSocket.cpp
 * @brief Windows WinSock2 implementation of UdpSocket.
 *
 * Platform-specific code is entirely confined to this file.
 * The public header (UdpSocket.h) is platform-neutral.
 *
 * To port to ESP-IDF:
 *   Replace this file. Keep the .h unchanged.
 *   - WSAStartup / WSACleanup → no-op (lwIP initialized by esp-netif)
 *   - INVALID_SOCKET → -1; SOCKET_ERROR → -1; closesocket → close
 *   - WSAETIMEDOUT → EAGAIN / EWOULDBLOCK
 */

#include "../include/UdpSocket.h"
#include "WinSock2Helpers.h"   // INTERNAL: WsaErrorString, SockAddrToString, etc.

#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>

// Link Ws2_32 automatically in MSVC builds.
#pragma comment(lib, "Ws2_32.lib")

namespace NetDiscovery {

// ============================================================
// Construction / Destruction
// ============================================================

UdpSocket::UdpSocket() noexcept
    : m_handle(static_cast<uintptr_t>(INVALID_SOCKET))
{}

UdpSocket::~UdpSocket() noexcept
{
    Close();
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : m_handle(other.m_handle)
{
    other.m_handle = static_cast<uintptr_t>(INVALID_SOCKET);
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept
{
    if (this != &other) {
        Close();
        m_handle = other.m_handle;
        other.m_handle = static_cast<uintptr_t>(INVALID_SOCKET);
    }
    return *this;
}

// ============================================================
// Open()
// ============================================================

void UdpSocket::Open()
{
    if (IsOpen()) return;  // Idempotent.

    // --- Platform init (handled by NetworkStackGuard) ---

    // --- Create UDP socket ---
    // No bind() — the OS assigns an ephemeral source port.
    // SSDP devices reply to the ephemeral port of the M-SEARCH sender.
    const SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        const int err = WSAGetLastError();
        throw std::runtime_error(
            "UdpSocket::Open — socket() failed: "
            + Platform::WsaErrorString(err)
        );
    }

    m_handle = static_cast<uintptr_t>(sock);
    std::cout << "[UdpSocket] Opened (descriptor=" << sock << ").\n";
}

// ============================================================
// Close()
// ============================================================

void UdpSocket::Close() noexcept
{
    if (!IsOpen()) return;

    const SOCKET sock = static_cast<SOCKET>(m_handle);
    closesocket(sock);
    m_handle = static_cast<uintptr_t>(INVALID_SOCKET);
    std::cout << "[UdpSocket] Closed.\n";
}

// ============================================================
// IsOpen()
// ============================================================

bool UdpSocket::IsOpen() const noexcept
{
    return m_handle != static_cast<uintptr_t>(INVALID_SOCKET);
}

// ============================================================
// SetReceiveTimeout()
// ============================================================

void UdpSocket::SetReceiveTimeout(int seconds)
{
    Platform::SetSocketTimeoutMs(static_cast<SOCKET>(m_handle), seconds);
}

void UdpSocket::EnableBroadcast(bool enable)
{
    if (!IsOpen()) {
        throw std::runtime_error("Cannot set broadcast on a closed socket");
    }
    
    BOOL bBroadcast = enable ? TRUE : FALSE;
    if (setsockopt(static_cast<SOCKET>(m_handle), SOL_SOCKET, SO_BROADCAST,
                   reinterpret_cast<const char*>(&bBroadcast), sizeof(bBroadcast)) == SOCKET_ERROR) {
        throw std::runtime_error("setsockopt(SO_BROADCAST) failed");
    }
}

// ============================================================
// Send()
// ============================================================

int UdpSocket::Send(const std::string& data,
                    const std::string& destIp,
                    uint16_t           destPort)
{
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(destPort);
    inet_pton(AF_INET, destIp.c_str(), &dest.sin_addr);

    const int sent = sendto(
        static_cast<SOCKET>(m_handle),
        data.c_str(),
        static_cast<int>(data.size()),
        0,
        reinterpret_cast<const sockaddr*>(&dest),
        sizeof(dest)
    );

    if (sent == SOCKET_ERROR) {
        throw std::runtime_error(
            "UdpSocket::Send — sendto() failed: "
            + Platform::WsaErrorString(WSAGetLastError())
        );
    }

    return sent;
}

// ============================================================
// Receive()
// ============================================================

int UdpSocket::Receive(char*        buffer,
                       int          bufferSize,
                       std::string& senderIp,
                       uint16_t&    senderPort)
{
    sockaddr_in senderAddr{};
    int         addrLen = sizeof(senderAddr);

    std::memset(buffer, 0, static_cast<std::size_t>(bufferSize));

    const int received = recvfrom(
        static_cast<SOCKET>(m_handle),
        buffer,
        bufferSize - 1,   // leave room for null terminator
        0,
        reinterpret_cast<sockaddr*>(&senderAddr),
        &addrLen
    );

    if (received == SOCKET_ERROR) {
        const int err = WSAGetLastError();
        if (err == WSAETIMEDOUT) {
            return 0;  // Timeout sentinel — not an error.
        }
        throw std::runtime_error(
            "UdpSocket::Receive — recvfrom() failed: "
            + Platform::WsaErrorString(err)
        );
    }

    buffer[received] = '\0';
    senderIp   = Platform::SockAddrToString(senderAddr);
    senderPort = ntohs(senderAddr.sin_port);
    return received;
}

} // namespace NetDiscovery
