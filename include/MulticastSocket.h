/**
 * @file MulticastSocket.h
 * @brief Platform-neutral UDP multicast socket for passive reception.
 *
 * Transport layer — no protocol knowledge.
 *
 * MulticastSocket is INDEPENDENT of UdpSocket by design.
 * Multicast is not a specialization of UDP from an architectural
 * standpoint — it has entirely different OS semantics (SO_REUSEADDR,
 * bind to a shared port, IP_ADD_MEMBERSHIP) and a different use case
 * (passive receive of group-addressed traffic, no sending).
 *
 * Responsibilities:
 *   - Bind to a well-known port with SO_REUSEADDR.
 *   - Join an IPv4 multicast group (IP_ADD_MEMBERSHIP).
 *   - Receive datagrams sent to the multicast group.
 *   - Leave the group and release the socket cleanly on destruction.
 *
 * It does NOT send anything. For sending M-SEARCH requests, use UdpSocket.
 *
 * The public header contains NO platform-specific types.
 * All WinSock2 / POSIX code is confined to MulticastSocket.cpp.
 *
 * To port to ESP-IDF:
 *   Provide a replacement MulticastSocket.cpp using lwIP BSD socket API.
 *   This header remains unchanged.
 */

#pragma once

#include <cstdint>
#include <string>

namespace NetDiscovery {

// ============================================================
// MulticastSocket
// ============================================================

/**
 * @brief RAII multicast group receiver socket.
 *
 * Usage:
 * @code
 *   MulticastSocket mcast;
 *   mcast.Open(1900);
 *   mcast.JoinGroup("239.255.255.250");
 *   mcast.SetReceiveTimeout(1);
 *   char buf[4096]; std::string ip; uint16_t port;
 *   while (running) {
 *       int n = mcast.Receive(buf, sizeof(buf), ip, port);
 *       if (n > 0) process(buf, n);
 *   }
 *   mcast.Close();  // also leaves the group
 * @endcode
 */
class MulticastSocket {
public:
    // ----------------------------------------------------------------
    // Construction / Destruction
    // ----------------------------------------------------------------

    MulticastSocket() noexcept;
    ~MulticastSocket() noexcept;

    // Non-copyable.
    MulticastSocket(const MulticastSocket&) = delete;
    MulticastSocket& operator=(const MulticastSocket&) = delete;

    // Movable.
    MulticastSocket(MulticastSocket&&) noexcept;
    MulticastSocket& operator=(MulticastSocket&&) noexcept;

    // ----------------------------------------------------------------
    // Lifecycle
    // ----------------------------------------------------------------

    /**
     * @brief Open the socket, set SO_REUSEADDR, and bind to INADDR_ANY:port.
     *
     * On Windows: calls WSAStartup(2.2), then socket(), setsockopt(REUSEADDR),
     * then bind(). Does NOT join a multicast group yet — call JoinGroup().
     *
     * @param port  Local port to bind to (e.g. 1900 for SSDP).
     * @throws std::runtime_error on any socket API failure.
     */
    void Open(uint16_t port);

    /**
     * @brief Leave the multicast group (if joined) and close the socket.
     *
     * Calls IP_DROP_MEMBERSHIP before closesocket() to cleanly leave
     * the multicast group. Safe to call multiple times (idempotent).
     */
    void Close() noexcept;

    /**
     * @brief Return true if the socket is currently open.
     */
    bool IsOpen() const noexcept;

    // ----------------------------------------------------------------
    // Multicast group management
    // ----------------------------------------------------------------

    /**
     * @brief Join an IPv4 multicast group on all interfaces.
     *
     * Calls IP_ADD_MEMBERSHIP with imr_interface = INADDR_ANY.
     * Must be called after Open().
     *
     * @param multicastAddr  Group address string (e.g. "239.255.255.250").
     * @throws std::runtime_error if IP_ADD_MEMBERSHIP fails.
     */
    void JoinGroup(const std::string& multicastAddr);

    /**
     * @brief Leave the currently joined multicast group.
     *
     * No-op if no group has been joined. Called automatically by Close().
     */
    void LeaveGroup() noexcept;

    // ----------------------------------------------------------------
    // Configuration
    // ----------------------------------------------------------------

    /**
     * @brief Set a receive timeout so Receive() returns periodically.
     *
     * Allows the caller to check stop conditions in a tight loop without
     * blocking indefinitely.
     *
     * @param seconds  Timeout in whole seconds. Typically 1.
     * @throws std::runtime_error if setsockopt fails.
     */
    void SetReceiveTimeout(int seconds);

    // ----------------------------------------------------------------
    // I/O (receive only — MulticastSocket does not send)
    // ----------------------------------------------------------------

    /**
     * @brief Receive one multicast datagram.
     *
     * Blocks until a datagram arrives or the receive timeout elapses.
     *
     * @param buffer      Caller-allocated receive buffer.
     * @param bufferSize  Capacity of @p buffer in bytes.
     * @param senderIp    [out] IPv4 address of the sender.
     * @param senderPort  [out] Port of the sender (host byte order).
     *
     * @return > 0  Number of bytes written into @p buffer.
     * @return   0  Receive timeout — no data in the window.
     *
     * @throws std::runtime_error on an unexpected socket error.
     */
    int Receive(char*       buffer,
                int         bufferSize,
                std::string& senderIp,
                uint16_t&    senderPort);

private:
    uintptr_t  m_handle;         ///< Platform socket descriptor.
    std::string m_joinedGroup;   ///< Multicast group currently joined, or "".
    bool        m_groupJoined;   ///< True after a successful JoinGroup().
};

} // namespace NetDiscovery
