/**
 * @file TcpSocket.h
 * @brief Platform-neutral TCP socket — architecture stub.
 *
 * Transport layer — no protocol knowledge.
 *
 * TcpSocket will be required in Phase 3 (HTTP fetching of LOCATION URLs).
 * The interface is defined now so that HttpClient.h can be written against
 * a stable API without requiring implementation.
 *
 * Phase 3 (NOT YET IMPLEMENTED):
 *   TODO: Connect to a remote host:port.
 *   TODO: Send an HTTP request string.
 *   TODO: Receive the response into a buffer.
 *   TODO: Handle TLS upgrade (Phase 8+).
 *
 * All methods currently throw std::runtime_error("Not implemented").
 *
 * Portability: Header — STL only. Implementation — platform-specific.
 */

#pragma once

#include <cstdint>
#include <string>

namespace NetDiscovery {

// ============================================================
// TcpSocket
// ============================================================

/**
 * @brief RAII TCP stream socket (architecture stub).
 *
 * The interface is intentionally minimal: Connect / Disconnect / Send / Receive.
 * TLS and keep-alive will be added as optional capabilities in a later phase.
 */
class TcpSocket {
public:
    TcpSocket() noexcept;
    ~TcpSocket() noexcept;

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;
    TcpSocket(TcpSocket&&) noexcept;
    TcpSocket& operator=(TcpSocket&&) noexcept;

    /**
     * @brief Connect to a remote host:port.
     *
     * @throws std::runtime_error("Not implemented") — Phase 3 stub.
     */
    void Connect(const std::string& host, uint16_t port);

    /**
     * @brief Disconnect and close the socket.
     */
    void Disconnect() noexcept;

    /**
     * @brief Return true if currently connected.
     */
    bool IsConnected() const noexcept;

    /**
     * @brief Send data over the TCP connection.
     *
     * @throws std::runtime_error("Not implemented") — Phase 3 stub.
     */
    int Send(const std::string& data);

    /**
     * @brief Receive data from the TCP connection.
     *
     * @throws std::runtime_error("Not implemented") — Phase 3 stub.
     */
    int Receive(char* buffer, int bufferSize);

private:
    uintptr_t m_handle;
};

} // namespace NetDiscovery
