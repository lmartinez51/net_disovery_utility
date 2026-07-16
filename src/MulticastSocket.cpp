/**
 * @file MulticastSocket.cpp
 * @brief Windows WinSock2 implementation of MulticastSocket.
 *
 * Platform-specific code is entirely confined to this file.
 * The public header (MulticastSocket.h) is platform-neutral.
 *
 * MulticastSocket is architecturally INDEPENDENT of UdpSocket.
 * They share implementation helpers through WinSock2Helpers.h only.
 *
 * To port to ESP-IDF:
 *   Replace this file. Keep the .h unchanged.
 *   - WSAStartup / WSACleanup → no-op
 *   - ip_mreq → struct ip_mreq (lwIP defines it the same way)
 *   - WSAETIMEDOUT → EAGAIN / EWOULDBLOCK
 */

#include "../include/MulticastSocket.h"
#include "WinSock2Helpers.h"   // INTERNAL

#include <cstring>
#include <iostream>
#include <stdexcept>

#pragma comment(lib, "Ws2_32.lib")

namespace NetDiscovery {

// ============================================================
// Construction / Destruction
// ============================================================

MulticastSocket::MulticastSocket() noexcept
    : m_handle(static_cast<uintptr_t>(INVALID_SOCKET))
    , m_groupJoined(false)
{}

MulticastSocket::~MulticastSocket() noexcept
{
    Close();
}

MulticastSocket::MulticastSocket(MulticastSocket&& other) noexcept
    : m_handle(other.m_handle)
    , m_joinedGroup(std::move(other.m_joinedGroup))
    , m_groupJoined(other.m_groupJoined)
{
    other.m_handle      = static_cast<uintptr_t>(INVALID_SOCKET);
    other.m_groupJoined = false;
}

MulticastSocket& MulticastSocket::operator=(MulticastSocket&& other) noexcept
{
    if (this != &other) {
        Close();
        m_handle      = other.m_handle;
        m_joinedGroup = std::move(other.m_joinedGroup);
        m_groupJoined = other.m_groupJoined;
        other.m_handle      = static_cast<uintptr_t>(INVALID_SOCKET);
        other.m_groupJoined = false;
    }
    return *this;
}

// ============================================================
// Open()
// ============================================================

void MulticastSocket::Open(uint16_t port)
{
    if (IsOpen()) return;

    // --- Platform init ---
    WSADATA wsaData{};
    const int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        throw std::runtime_error(
            "MulticastSocket::Open — WSAStartup failed: "
            + Platform::WsaErrorString(wsaResult)
        );
    }

    // --- Create socket ---
    const SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        const int err = WSAGetLastError();
        WSACleanup();
        throw std::runtime_error(
            "MulticastSocket::Open — socket() failed: "
            + Platform::WsaErrorString(err)
        );
    }

    // --- SO_REUSEADDR ---
    // Allows multiple processes to bind to port 1900 simultaneously
    // (e.g. both NetDiscovery and the Windows SSDP service).
    const int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse)) == SOCKET_ERROR) {
        const int err = WSAGetLastError();
        closesocket(sock);
        WSACleanup();
        throw std::runtime_error(
            "MulticastSocket::Open — setsockopt(SO_REUSEADDR) failed: "
            + Platform::WsaErrorString(err)
        );
    }

    // --- Bind to INADDR_ANY:port ---
    sockaddr_in bindAddr{};
    bindAddr.sin_family      = AF_INET;
    bindAddr.sin_port        = htons(port);
    bindAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
        const int err = WSAGetLastError();
        closesocket(sock);
        WSACleanup();
        throw std::runtime_error(
            "MulticastSocket::Open — bind() to port "
            + std::to_string(port) + " failed: "
            + Platform::WsaErrorString(err)
            + "\nHint: Disable Windows SSDP Service first: sc stop SSDPSRV"
        );
    }

    m_handle = static_cast<uintptr_t>(sock);
    std::cout << "[MulticastSocket] Opened, bound to port " << port << ".\n";
}

// ============================================================
// Close()
// ============================================================

void MulticastSocket::Close() noexcept
{
    if (!IsOpen()) return;

    // Leave multicast group before closing (clean shutdown).
    LeaveGroup();

    closesocket(static_cast<SOCKET>(m_handle));
    m_handle = static_cast<uintptr_t>(INVALID_SOCKET);
    WSACleanup();
    std::cout << "[MulticastSocket] Closed.\n";
}

// ============================================================
// IsOpen()
// ============================================================

bool MulticastSocket::IsOpen() const noexcept
{
    return m_handle != static_cast<uintptr_t>(INVALID_SOCKET);
}

// ============================================================
// JoinGroup()
// ============================================================

void MulticastSocket::JoinGroup(const std::string& multicastAddr)
{
    ip_mreq mreq{};
    inet_pton(AF_INET, multicastAddr.c_str(), &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(static_cast<SOCKET>(m_handle),
                   IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR) {
        throw std::runtime_error(
            "MulticastSocket::JoinGroup — IP_ADD_MEMBERSHIP failed: "
            + Platform::WsaErrorString(WSAGetLastError())
        );
    }

    m_joinedGroup = multicastAddr;
    m_groupJoined = true;
    std::cout << "[MulticastSocket] Joined group " << multicastAddr << ".\n";
}

// ============================================================
// LeaveGroup()
// ============================================================

void MulticastSocket::LeaveGroup() noexcept
{
    if (!m_groupJoined || !IsOpen()) return;

    ip_mreq mreq{};
    inet_pton(AF_INET, m_joinedGroup.c_str(), &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;

    setsockopt(static_cast<SOCKET>(m_handle),
               IPPROTO_IP, IP_DROP_MEMBERSHIP,
               reinterpret_cast<const char*>(&mreq), sizeof(mreq));

    m_groupJoined = false;
    std::cout << "[MulticastSocket] Left group " << m_joinedGroup << ".\n";
}

// ============================================================
// SetReceiveTimeout()
// ============================================================

void MulticastSocket::SetReceiveTimeout(int seconds)
{
    Platform::SetSocketTimeoutMs(static_cast<SOCKET>(m_handle), seconds);
}

// ============================================================
// Receive()
// ============================================================

int MulticastSocket::Receive(char*        buffer,
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
        bufferSize - 1,
        0,
        reinterpret_cast<sockaddr*>(&senderAddr),
        &addrLen
    );

    if (received == SOCKET_ERROR) {
        const int err = WSAGetLastError();
        if (err == WSAETIMEDOUT) {
            return 0;  // Timeout — caller checks stop condition and loops.
        }
        throw std::runtime_error(
            "MulticastSocket::Receive — recvfrom() failed: "
            + Platform::WsaErrorString(err)
        );
    }

    buffer[received] = '\0';
    senderIp   = Platform::SockAddrToString(senderAddr);
    senderPort = ntohs(senderAddr.sin_port);
    return received;
}

} // namespace NetDiscovery
