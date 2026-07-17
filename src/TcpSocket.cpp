/**
 * @file TcpSocket.cpp
 * @brief Platform-neutral TCP socket — WinSock2 implementation.
 *
 * Phase 3: Implements Connect/Send/Receive using WinSock2 TCP.
 */

#include "../include/TcpSocket.h"
#include "WinSock2Helpers.h"

#include <stdexcept>
#include <iostream> // For basic debugging if needed

namespace NetDiscovery {

TcpSocket::TcpSocket() noexcept : m_handle(0) {}

TcpSocket::~TcpSocket() noexcept { Disconnect(); }

TcpSocket::TcpSocket(TcpSocket&& other) noexcept : m_handle(other.m_handle)
{
    other.m_handle = 0;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept
{
    if (this != &other) {
        Disconnect();
        m_handle = other.m_handle;
        other.m_handle = 0;
    }
    return *this;
}

void TcpSocket::Connect(const std::string& host, uint16_t port)
{
    if (IsConnected()) {
        Disconnect();
    }

    // Resolve host
    struct addrinfo hints = {};
    hints.ai_family = AF_INET; // IPv4 for now
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);

    int r = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (r != 0) {
        throw std::runtime_error("TcpSocket::Connect — getaddrinfo failed for " + host + ": " + std::to_string(r));
    }

    SOCKET sock = INVALID_SOCKET;
    for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            continue;
        }

        // Apply receive timeout
        try {
            Platform::SetSocketTimeoutMs(sock, 15); // 15 second timeout for slow DIAL devices
        } catch (...) {
            closesocket(sock);
            continue;
        }

        if (connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
            closesocket(sock);
            sock = INVALID_SOCKET;
            continue;
        }
        
        break; // Connected successfully
    }

    freeaddrinfo(result);

    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("TcpSocket::Connect — unable to connect to " + host + ":" + portStr);
    }

    m_handle = static_cast<uintptr_t>(sock);
}

void TcpSocket::Disconnect() noexcept
{
    if (m_handle != 0) {
        SOCKET sock = static_cast<SOCKET>(m_handle);
        shutdown(sock, SD_BOTH);
        closesocket(sock);
        m_handle = 0;
    }
}

bool TcpSocket::IsConnected() const noexcept
{
    return m_handle != 0;
}

int TcpSocket::Send(const std::string& data)
{
    if (!IsConnected()) {
        throw std::runtime_error("TcpSocket::Send — Not connected.");
    }

    SOCKET sock = static_cast<SOCKET>(m_handle);
    int totalSent = 0;
    int dataLen = static_cast<int>(data.length());
    const char* ptr = data.c_str();

    while (totalSent < dataLen) {
        int r = send(sock, ptr + totalSent, dataLen - totalSent, 0);
        if (r == SOCKET_ERROR) {
            throw std::runtime_error("TcpSocket::Send failed: " + Platform::WsaErrorString(WSAGetLastError()));
        }
        totalSent += r;
    }

    return totalSent;
}

int TcpSocket::Receive(char* buffer, int bufferSize)
{
    if (!IsConnected()) {
        throw std::runtime_error("TcpSocket::Receive — Not connected.");
    }
    
    if (bufferSize <= 0) return 0;

    SOCKET sock = static_cast<SOCKET>(m_handle);
    int r = recv(sock, buffer, bufferSize, 0);
    
    if (r == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAETIMEDOUT) {
            return 0; // Timeout is not an exception here, just 0 bytes read
        }
        throw std::runtime_error("TcpSocket::Receive failed: " + Platform::WsaErrorString(err));
    }
    
    return r; // Can be 0 if connection gracefully closed
}

} // namespace NetDiscovery
