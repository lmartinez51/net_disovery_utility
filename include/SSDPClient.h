/**
 * @file SSDPClient.h
 * @brief SSDP discovery client — transport layer only.
 *
 * Refactored responsibilities (architectural stabilization):
 *
 *   SSDPClient is responsible ONLY for:
 *     - Sending UDP M-SEARCH datagrams (via UdpSocket).
 *     - Receiving raw responses and wrapping them into Packet objects.
 *     - Managing socket lifecycle (Initialize / Shutdown).
 *     - Passive multicast listening (via MulticastSocket, on-stack).
 *
 *   SSDPClient is NOT responsible for:
 *     - Parsing SSDP headers         → SSDPAnalyzer
 *     - Populating DeviceRegistry    → SSDPAnalyzer via AnalyzerDispatcher
 *     - Printing anything            → main()
 *     - Grouping by UUID             → DeviceRegistry
 *
 * The M-SEARCH request string is built by SSDPAnalyzer::BuildMSearchRequest()
 * so that SSDP protocol knowledge stays in the protocol layer, not here.
 *
 * Public header is platform-neutral (no WinSock2 types).
 *
 * Portability: Header — STL + UdpSocket only.
 *              Implementation — uses WinSock2 through UdpSocket/MulticastSocket.
 */

#pragma once

#include "UdpSocket.h"
#include "core/Packet.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace NetDiscovery
{

    // ============================================================
    // DiscoveryResult
    // ============================================================

    /**
     * @brief Aggregated result for one search target.
     *
     * DiscoverMultiple() returns one DiscoveryResult per search target.
     * Each result holds all Packets received for that target during the
     * discovery window.
     */
    struct DiscoveryResult
    {
        /// The search target (ST) string used for this scan round.
        std::string searchTarget;

        /// All raw Packets received in response to the M-SEARCH
        /// for this search target.
        std::vector<Packet> packets;
    };

    // ============================================================
    // SSDPClient
    // ============================================================

    /**
     * @brief UDP-based SSDP discovery client (transport only).
     *
     * Composes a UdpSocket for active discovery. Creates a MulticastSocket
     * on the stack inside ListenPassive() for passive reception.
     */
    class SSDPClient
    {
    public:
        // ----------------------------------------------------------------
        // Construction / Destruction
        // ----------------------------------------------------------------

        SSDPClient() = default;
        ~SSDPClient() noexcept;

        SSDPClient(const SSDPClient &) = delete;
        SSDPClient &operator=(const SSDPClient &) = delete;
        SSDPClient(SSDPClient &&) noexcept = default;
        SSDPClient &operator=(SSDPClient &&) noexcept = default;

        // ----------------------------------------------------------------
        // Lifecycle
        // ----------------------------------------------------------------

        /**
         * @brief Open the UDP socket and initialize platform networking.
         *
         * Must be called before Discover() or DiscoverMultiple().
         * ListenPassive() manages its own socket internally.
         *
         * @throws std::runtime_error if UdpSocket::Open() fails.
         */
        void Initialize();

        /**
         * @brief Close the UDP socket and clean up platform networking.
         *
         * Safe to call multiple times.
         */
        void Shutdown() noexcept;

        /**
         * @brief Return true if the client is initialized and ready.
         */
        bool IsInitialized() const noexcept;

        // ----------------------------------------------------------------
        // Active discovery
        // ----------------------------------------------------------------

        /**
         * @brief Send one M-SEARCH and collect all responses within the timeout.
         *
         * Sends the M-SEARCH datagram to 239.255.255.250:1900 and reads
         * responses until recvfrom() times out. Each valid response is
         * wrapped into a Packet with:
         *   - packet.source.address  = sender IP
         *   - packet.source.port     = sender port
         *   - packet.destination     = {239.255.255.250, 1900}
         *   - packet.transport       = TransportProtocol::UDP
         *   - packet.protocol        = ProtocolType::SSDP
         *   - packet.rawPayload      = raw UDP bytes
         *   - packet.timestamp       = system_clock::now() after recvfrom()
         *
         * Headers and body are left empty — set by SSDPAnalyzer::Analyze().
         *
         * Does NOT print. Does NOT parse headers.
         *
         * @param searchTarget  SSDP ST header value (e.g. "ssdp:all").
         * @param mxSeconds     MX value sent in the request (response delay hint).
         * @param timeoutSeconds Receive timeout per call to recvfrom().
         *
         * @return All received Packets for this search target.
         * @throws std::runtime_error if not initialized or on socket error.
         */
        std::vector<Packet> Discover(const std::string &searchTarget,
                                     int mxSeconds,
                                     int timeoutSeconds);

        /**
         * @brief Run Discover() for each target in @p targets.
         *
         * Convenience wrapper. Each target gets its own Discover() call
         * with independent send + receive windows.
         *
         * @param targets         List of SSDP ST header values.
         * @param mxSeconds       MX value for each request.
         * @param timeoutSeconds  Receive timeout per target.
         *
         * @return One DiscoveryResult per target (same order as @p targets).
         */
        std::vector<DiscoveryResult> DiscoverMultiple(
            const std::vector<std::string> &targets,
            int mxSeconds,
            int timeoutSeconds);

        /**
         * @brief Send ALL M-SEARCH requests upfront, then listen once.
         *
         * Unlike DiscoverMultiple() which opens a new receive window per
         * target, this method sends every M-SEARCH in rapid succession
         * and then listens for a single shared @p timeoutSeconds window.
         * Responses are demuxed into per-ST buckets by matching the ST
         * header in each response against the requested targets list.
         * Total wall-clock time is ~1x timeoutSeconds instead of N×timeoutSeconds.
         *
         * @return One DiscoveryResult per target (same order as @p targets).
         */
        std::vector<DiscoveryResult> DiscoverAll(
            const std::vector<std::string> &targets,
            int mxSeconds,
            int timeoutSeconds);

        // ----------------------------------------------------------------
        // Passive listening
        // ----------------------------------------------------------------

        /**
         * @brief Join the SSDP multicast group and invoke @p onPacket for
         *        every datagram received.
         *
         * Creates a MulticastSocket internally (on the stack). Does not use
         * the member UdpSocket. Ctrl+C sets an internal atomic stop flag.
         *
         * @param onPacket  Callback invoked for each received Packet.
         *                  Called synchronously in this thread.
         *                  Must not throw (exceptions are caught and logged).
         *
         * @param durationSeconds  Stop after this many seconds (0 = run until Ctrl+C).
         */
        void ListenPassive(std::function<void(const Packet &)> onPacket,
                           int durationSeconds = 0);

    private:
        /// Socket used for active M-SEARCH discovery.
        UdpSocket m_udpSocket;

        /// Shared stop flag for ListenPassive(); set by Ctrl+C handler.
    public:
        /// Shared stop flag for ListenPassive(); set by Ctrl+C handler.
        static std::atomic<bool> s_stopListening;
    };

} // namespace NetDiscovery
