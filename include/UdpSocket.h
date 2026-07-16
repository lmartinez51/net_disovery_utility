/**
 * @file UdpSocket.h
 * @brief Platform-neutral UDP socket for sending and receiving datagrams.
 *
 * Transport layer — no protocol knowledge.
 *
 * This class is responsible ONLY for:
 *   - Opening and closing a UDP socket.
 *   - Sending a buffer to a remote address:port.
 *   - Receiving one datagram at a time with a configurable timeout.
 *
 * It has NO knowledge of SSDP, HTTP, or any other application protocol.
 * It does NOT join multicast groups — see MulticastSocket for that.
 *
 * The public header contains NO platform-specific types.
 * All WinSock2 / POSIX code is confined to UdpSocket.cpp.
 *
 * To port to ESP-IDF:
 *   Provide a replacement UdpSocket.cpp using lwIP BSD socket API.
 *   This header remains unchanged.
 *
 * Portability: Header — STL only. Implementation — platform-specific.
 */

#pragma once

#include <cstdint>
#include <string>

namespace NetDiscovery {

// ============================================================
// UdpSocket
// ============================================================

/**
 * @brief RAII UDP datagram socket.
 *
 * Open() acquires platform resources (calls WSAStartup on Windows).
 * Close() releases them. Both are idempotent.
 *
 * Usage:
 * @code
 *   UdpSocket sock;
 *   sock.Open();
 *   sock.SetReceiveTimeout(5);
 *   sock.Send(request, "239.255.255.250", 1900);
 *   char buf[4096]; std::string ip; uint16_t port;
 *   int n = sock.Receive(buf, sizeof(buf), ip, port);
 *   sock.Close();
 * @endcode
 */
class UdpSocket {
public:
    // ----------------------------------------------------------------
    // Construction / Destruction
    // ----------------------------------------------------------------

    UdpSocket() noexcept;
    ~UdpSocket() noexcept;

    // Non-copyable — sockets are unique OS resources.
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Movable.
    UdpSocket(UdpSocket&&) noexcept;
    UdpSocket& operator=(UdpSocket&&) noexcept;

    // ----------------------------------------------------------------
    // Lifecycle
    // ----------------------------------------------------------------

    /**
     * @brief Open the UDP socket and initialize platform networking.
     *
     * On Windows: calls WSAStartup(2.2) then socket(AF_INET, SOCK_DGRAM).
     * Idempotent: does nothing if already open.
     *
     * @throws std::runtime_error on WSAStartup or socket() failure.
     */
    void Open();

    /**
     * @brief Close the socket and clean up platform networking.
     *
     * On Windows: calls closesocket() + WSACleanup().
     * Safe to call multiple times (idempotent).
     */
    void Close() noexcept;

    /**
     * @brief Return true if the socket is currently open.
     */
    bool IsOpen() const noexcept;

    // ----------------------------------------------------------------
    // Configuration
    // ----------------------------------------------------------------

    /**
     * @brief Set the socket-level receive timeout.
     *
     * A subsequent Receive() call that receives no data within the timeout
     * returns 0 (timeout sentinel) instead of blocking indefinitely.
     *
     * @param seconds  Timeout in whole seconds. Must be > 0.
     * @throws std::runtime_error if setsockopt fails.
     */
    void SetReceiveTimeout(int seconds);

    // ----------------------------------------------------------------
    // I/O
    // ----------------------------------------------------------------

    /**
     * @brief Send a buffer to a remote endpoint.
     *
     * @param data      Data to send.
     * @param destIp    Destination IPv4 address string (dotted-decimal).
     * @param destPort  Destination port (host byte order).
     * @return Number of bytes actually sent.
     * @throws std::runtime_error on sendto() failure.
     */
    int Send(const std::string& data,
             const std::string& destIp,
             uint16_t           destPort);

    /**
     * @brief Receive one UDP datagram.
     *
     * Blocks until a datagram arrives or the receive timeout elapses.
     *
     * @param buffer      Caller-allocated receive buffer.
     * @param bufferSize  Capacity of @p buffer in bytes.
     * @param senderIp    [out] IPv4 address of the sender.
     * @param senderPort  [out] Port of the sender (host byte order).
     *
     * @return > 0  Number of bytes written into @p buffer.
     * @return   0  Receive timeout elapsed — no data arrived.
     *
     * @throws std::runtime_error on an unexpected socket error.
     */
    int Receive(char*       buffer,
                int         bufferSize,
                std::string& senderIp,
                uint16_t&    senderPort);

private:
    /// Platform socket descriptor.
    /// INVALID_SOCKET (Windows) / -1 (POSIX) when closed.
    /// Stored as uintptr_t so the public header has no WinSock2 dependency.
    uintptr_t m_handle;
};

} // namespace NetDiscovery
