/**
 * @file SSDPClient.cpp
 * @brief Windows WinSock2 implementation of SSDP discovery client.
 *
 * Transport only — no SSDP header parsing, no printing, no registry.
 *
 * Ctrl+C handling for ListenPassive() uses a file-scope atomic<bool>
 * (s_stopListening) because SetConsoleCtrlHandler requires a plain
 * function pointer, not a lambda or member function.
 */

#include "../include/SSDPClient.h"
#include "../include/SSDPAnalyzer.h"   // For BuildMSearchRequest()
#include "../include/MulticastSocket.h"
#include "../include/PacketUtilities.h"

// Windows console Ctrl+C handler (passive listen only).
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace NetDiscovery {

// ============================================================
// Static member definition
// ============================================================

std::atomic<bool> SSDPClient::s_stopListening{false};

// ============================================================
// Constants
// ============================================================

namespace {

constexpr const char* SSDP_MULTICAST_ADDR = "239.255.255.250";
constexpr uint16_t    SSDP_PORT           = 1900;
constexpr int         RECV_BUFFER_SIZE    = 4096;

// ControlHandler for ListenPassive() Ctrl+C.
BOOL WINAPI CtrlHandler(DWORD ctrlType)
{
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT) {
        std::cout << "\n[SSDPClient] Interrupt received. Stopping passive listener.\n";
        SSDPClient::s_stopListening = true;
        return TRUE;
    }
    return FALSE;
}

} // anonymous namespace

// ============================================================
// Destructor
// ============================================================

SSDPClient::~SSDPClient() noexcept
{
    Shutdown();
}

// ============================================================
// Initialize()
// ============================================================

void SSDPClient::Initialize()
{
    m_udpSocket.Open();
}

// ============================================================
// Shutdown()
// ============================================================

void SSDPClient::Shutdown() noexcept
{
    m_udpSocket.Close();
}

// ============================================================
// IsInitialized()
// ============================================================

bool SSDPClient::IsInitialized() const noexcept
{
    return m_udpSocket.IsOpen();
}

// ============================================================
// Discover()
// ============================================================

std::vector<Packet> SSDPClient::Discover(const std::string& searchTarget,
                                          int                mxSeconds,
                                          int                timeoutSeconds)
{
    if (!IsInitialized()) {
        throw std::runtime_error("SSDPClient::Discover — not initialized. Call Initialize() first.");
    }

    // Set per-call receive timeout.
    m_udpSocket.SetReceiveTimeout(timeoutSeconds);

    // Build and send the M-SEARCH request.
    const std::string request = SSDPAnalyzer::BuildMSearchRequest(
        SSDP_MULTICAST_ADDR, SSDP_PORT, searchTarget, mxSeconds);

    m_udpSocket.Send(request, SSDP_MULTICAST_ADDR, SSDP_PORT);

    // Collect responses until timeout.
    std::vector<Packet> packets;
    char recvBuf[RECV_BUFFER_SIZE];

    while (true) {
        std::string senderIp;
        uint16_t    senderPort = 0;

        const int received = m_udpSocket.Receive(recvBuf, RECV_BUFFER_SIZE,
                                                  senderIp, senderPort);
        if (received == 0) {
            // Timeout — no more responses in this window.
            std::cout << "[SSDPClient] Receive window closed for ST: " << searchTarget << "\n";
            break;
        }

        // Wrap raw bytes into a protocol-independent Packet.
        Packet pkt;
        pkt.timestamp           = std::chrono::system_clock::now();
        pkt.transport           = TransportProtocol::UDP;
        pkt.protocol            = ProtocolType::SSDP;
        pkt.source.address      = senderIp;
        pkt.source.port         = senderPort;
        pkt.destination.address = SSDP_MULTICAST_ADDR;
        pkt.destination.port    = SSDP_PORT;
        pkt.rawPayload          = std::string(recvBuf, static_cast<std::size_t>(received));
        pkt.metadata["ssdp.searchTarget"] = searchTarget;

        packets.push_back(std::move(pkt));
    }

    return packets;
}

// ============================================================
// DiscoverMultiple()
// ============================================================

std::vector<DiscoveryResult> SSDPClient::DiscoverMultiple(
    const std::vector<std::string>& targets,
    int                             mxSeconds,
    int                             timeoutSeconds)
{
    std::vector<DiscoveryResult> results;
    results.reserve(targets.size());

    for (const auto& target : targets) {
        DiscoveryResult result;
        result.searchTarget = target;
        result.packets      = Discover(target, mxSeconds, timeoutSeconds);
        results.push_back(std::move(result));
    }

    return results;
}

// ============================================================
// DiscoverAll()  — P5: single shared receive window
// ============================================================

std::vector<DiscoveryResult> SSDPClient::DiscoverAll(
    const std::vector<std::string>& targets,
    int                             mxSeconds,
    int                             timeoutSeconds)
{
    if (!IsInitialized()) {
        throw std::runtime_error("SSDPClient::DiscoverAll -- not initialized. Call Initialize() first.");
    }

    // Build one result slot per target (preserves order).
    std::vector<DiscoveryResult> results;
    results.reserve(targets.size());
    for (const auto& t : targets) {
        results.push_back({t, {}});
    }

    // Send all M-SEARCH requests upfront, back-to-back.
    for (const auto& target : targets) {
        const std::string request = SSDPAnalyzer::BuildMSearchRequest(
            SSDP_MULTICAST_ADDR, SSDP_PORT, target, mxSeconds);
        m_udpSocket.Send(request, SSDP_MULTICAST_ADDR, SSDP_PORT);
    }

    // Single shared receive window.
    m_udpSocket.SetReceiveTimeout(timeoutSeconds);

    char recvBuf[RECV_BUFFER_SIZE];
    while (true) {
        std::string senderIp;
        uint16_t    senderPort = 0;

        const int received = m_udpSocket.Receive(recvBuf, RECV_BUFFER_SIZE,
                                                  senderIp, senderPort);
        if (received == 0) {
            // Timeout — done.
            break;
        }

        Packet pkt;
        pkt.timestamp           = std::chrono::system_clock::now();
        pkt.transport           = TransportProtocol::UDP;
        pkt.protocol            = ProtocolType::SSDP;
        pkt.source.address      = senderIp;
        pkt.source.port         = senderPort;
        pkt.destination.address = SSDP_MULTICAST_ADDR;
        pkt.destination.port    = SSDP_PORT;
        pkt.rawPayload          = std::string(recvBuf, static_cast<std::size_t>(received));

        // Demux: match ST header value in the response to a target bucket.
        // Devices responding to ssdp:all will echo back their own ST value.
        // We put ssdp:all responses in the ssdp:all bucket by default.
        using PacketUtilities = NetDiscovery::PacketUtilities;
        const std::string st = PacketUtilities::ExtractHeaderValue(pkt.rawPayload, "ST");

        bool placed = false;
        for (auto& result : results) {
            if (result.searchTarget == st ||
                result.searchTarget == "ssdp:all") {
                // Primary: exact ST match. Fallback: ssdp:all catches everything.
                if (result.searchTarget == st) {
                    pkt.metadata["ssdp.searchTarget"] = st;
                    result.packets.push_back(pkt);
                    placed = true;
                    break;
                }
            }
        }
        // If no exact match found, drop into ssdp:all bucket if present.
        if (!placed) {
            for (auto& result : results) {
                if (result.searchTarget == "ssdp:all") {
                    pkt.metadata["ssdp.searchTarget"] = "ssdp:all";
                    result.packets.push_back(pkt);
                    break;
                }
            }
        }
    }

    return results;
}


// ============================================================
// ListenPassive()
// ============================================================

void SSDPClient::ListenPassive(std::function<void(const Packet&)> onPacket,
                                int                                durationSeconds)
{
    // --- Set up multicast socket (on stack, independent of m_udpSocket) ---
    MulticastSocket mcast;
    mcast.Open(SSDP_PORT);
    mcast.JoinGroup(SSDP_MULTICAST_ADDR);
    mcast.SetReceiveTimeout(1);   // 1-second chunks so stop flag is checked

    // --- Install Ctrl+C handler ---
    s_stopListening = false;
    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    const auto startTime = std::chrono::steady_clock::now();
    char recvBuf[RECV_BUFFER_SIZE];

    std::cout << "[SSDPClient] Passive listening on " << SSDP_MULTICAST_ADDR
              << ":" << SSDP_PORT
              << (durationSeconds > 0
                     ? " (max " + std::to_string(durationSeconds) + "s)"
                     : " (Ctrl+C to stop)")
              << ".\n";

    while (!s_stopListening) {
        // --- Check duration limit ---
        if (durationSeconds > 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed >= static_cast<long long>(durationSeconds)) {
                std::cout << "[SSDPClient] Duration limit reached.\n";
                break;
            }
        }

        // --- Receive one datagram (returns 0 on 1-second timeout) ---
        std::string senderIp;
        uint16_t    senderPort = 0;
        const int received = mcast.Receive(recvBuf, RECV_BUFFER_SIZE,
                                            senderIp, senderPort);

        if (received == 0) continue;   // Check stop flag and loop.

        // --- Wrap into Packet ---
        Packet pkt;
        pkt.timestamp           = std::chrono::system_clock::now();
        pkt.transport           = TransportProtocol::Multicast;
        pkt.protocol            = ProtocolType::SSDP;
        pkt.source.address      = senderIp;
        pkt.source.port         = senderPort;
        pkt.destination.address = SSDP_MULTICAST_ADDR;
        pkt.destination.port    = SSDP_PORT;
        pkt.rawPayload          = std::string(recvBuf, static_cast<std::size_t>(received));

        // --- Invoke callback (printing / saving / analyzing is caller's job) ---
        if (onPacket) {
            try {
                onPacket(pkt);
            } catch (const std::exception& ex) {
                std::cerr << "[SSDPClient] onPacket callback threw: " << ex.what() << "\n";
            }
        }
    }

    // --- Clean up ---
    mcast.Close();
    SetConsoleCtrlHandler(CtrlHandler, FALSE);
    std::cout << "[SSDPClient] Passive listener stopped.\n";
}

} // namespace NetDiscovery
