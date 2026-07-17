/**
 * @file main.cpp
 * @brief NetDiscovery diagnostic utility -- main orchestrator.
 *
 * Modes:
 *   (default)  Active M-SEARCH for all configured search targets.
 *   --listen   Passive multicast listener (Ctrl+C to stop).
 *   --verbose  Enable per-stage pipeline diagnostics (off by default).
 *   --help     Print usage.
 */

#include "../include/SSDPClient.h"
#include "../include/AnalyzerDispatcher.h"
#include "../include/SSDPAnalyzer.h"
#include "../include/XmlAnalyzer.h"
#include "../include/DeviceRegistry.h"
#include "../include/CaptureWriter.h"
#include "../include/PacketUtilities.h"
#include "../include/DescriptionDownloader.h"
#include "../include/ControllerRegistry.h"
#include "../include/ControllerResolver.h"
#include "../include/ProtocolNormalizer.h"
#include "../include/DeviceClassifier.h"
#include "../include/CapabilityResolver.h"
#include "../include/ActionResolver.h"
#include "../include/IdentityResolutionEngine.h"
#include "../include/PresentationFormatter.h"
#include "../include/TransportRegistry.h"
#include "../include/DummyTransport.h"
#include "../include/DeviceExecutor.h"
#include "core/Packet.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace {

constexpr int MX_SECONDS      = 3;
constexpr int TIMEOUT_SECONDS = 5;

const std::vector<std::string> DEFAULT_SEARCH_TARGETS = {
    "ssdp:all",
    "upnp:rootdevice",
    "urn:schemas-upnp-org:device:MediaRenderer:1",
    "urn:schemas-upnp-org:device:MediaServer:1",
    "urn:dial-multiscreen-org:service:dial:1",
    "urn:samsung.com:device:RemoteControlReceiver:1",
};

constexpr const char* CAPTURE_BASE_DIR = "captures";
bool g_verbose = false;

void PrintSeparator(char ch = '=', int width = 70)
{
    std::cout << std::string(static_cast<std::size_t>(width), ch) << "\n";
}

void PrintHeader(const std::string& title)
{
    PrintSeparator();
    std::cout << "  " << title << "\n";
    PrintSeparator();
}

void PrintUsage(const char* programName)
{
    PrintHeader("NetDiscovery -- SSDP / UPnP Diagnostic Utility");
    std::cout
        << "\nUsage: " << programName << " [--help] [--listen] [--verbose]\n\n"
        << "Modes:\n"
        << "  (default)  Active M-SEARCH across all configured search targets.\n"
        << "  --listen   Passive multicast listener (Ctrl+C to stop).\n"
        << "  --verbose  Enable per-stage pipeline diagnostics.\n"
        << "  --help     Show this message.\n\n"
        << "Captures are written to: " << CAPTURE_BASE_DIR << "/\n\n";
}

void PrintPacket(const NetDiscovery::Packet& pkt, const std::string& label = "")
{
    using NetDiscovery::PacketUtilities;
    const std::string ts       = PacketUtilities::FormatTimestamp(pkt.timestamp);
    const std::string typeStr  = PacketUtilities::TypeToString(PacketUtilities::DetectType(pkt.rawPayload));
    const std::string uuid     = NetDiscovery::SSDPAnalyzer::ExtractUuid(pkt.rawPayload);
    const std::string location = PacketUtilities::ExtractHeaderValue(pkt.rawPayload, "LOCATION");
    const std::string server   = PacketUtilities::ExtractHeaderValue(pkt.rawPayload, "SERVER");
    PrintSeparator('-', 70);
    if (!label.empty()) std::cout << "  [" << label << "]\n";
    std::cout << "  Time     : " << ts << "\n"
              << "  From     : " << pkt.source.address << ":" << pkt.source.port << "\n"
              << "  Type     : " << typeStr << "\n"
              << "  UUID     : " << uuid << "\n";
    if (!location.empty()) std::cout << "  Location : " << location << "\n";
    if (!server.empty())   std::cout << "  Server   : " << server   << "\n";
}

void PrintDiscoverySummary(const std::vector<NetDiscovery::DiscoveryResult>&  results,
                            const std::vector<NetDiscovery::LogicalDevice>&    logicalDevices,
                            const NetDiscovery::CaptureWriter&                 writer)
{
    int totalPackets = 0;
    for (const auto& r : results) totalPackets += static_cast<int>(r.packets.size());
    PrintHeader("DISCOVERY SUMMARY");
    std::cout << "  Search targets   : " << results.size()        << "\n"
              << "  Total responses  : " << totalPackets          << "\n"
              << "  Logical devices  : " << logicalDevices.size() << "\n"
              << "  Files written    : " << writer.FileCount()    << "\n"
              << "  Capture dir      : " << writer.BasePath()     << "\n\n";
    std::cout << "  Per-target breakdown:\n";
    for (const auto& r : results)
        std::cout << "    " << std::left << std::setw(52) << r.searchTarget
                  << "  " << r.packets.size() << " response(s)\n";
    std::cout << "\n";
}

} // anonymous namespace

int main(int argc, char* argv[])
{
    using namespace NetDiscovery;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    bool listenMode = false;
    bool helpMode   = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--listen")                  listenMode = true;
        else if (arg == "--help")                    helpMode   = true;
        else if (arg == "--verbose" || arg == "-v")  g_verbose  = true;
    }

    if (helpMode) { PrintUsage(argv[0]); return 0; }

    AnalyzerDispatcher dispatcher;
    dispatcher.Register(std::make_unique<SSDPAnalyzer>());
    dispatcher.Register(std::make_unique<XmlAnalyzer>());
    DeviceRegistry registry;
    CaptureWriter  writer(CAPTURE_BASE_DIR);

    if (listenMode) {
        PrintHeader("NetDiscovery -- Passive Multicast Listener");
        std::cout << "  Capture dir : " << writer.BasePath() << "\n  Press Ctrl+C to stop.\n\n";
        SSDPClient client;
        client.ListenPassive([&](const Packet& pkt) {
            PrintPacket(pkt, "PASSIVE");
            writer.SavePassivePacket(pkt);
            dispatcher.Dispatch(pkt, registry);
        });
        IdentityResolutionEngine idEngine;
        std::vector<LogicalDevice> logicalDevices = idEngine.Resolve(registry.GetAll());
        PresentationFormatter::PrintLogicalDevices(logicalDevices);
        return 0;
    }

    PrintHeader("NetDiscovery -- Active M-SEARCH Discovery");
    std::cout << "  Targets     : " << DEFAULT_SEARCH_TARGETS.size() << "\n"
              << "  MX          : " << MX_SECONDS << "s\n"
              << "  Timeout     : " << TIMEOUT_SECONDS << "s\n"
              << "  Capture dir : " << writer.BasePath() << "\n\n";

    SSDPClient client;
    try { client.Initialize(); }
    catch (const std::exception& ex) {
        std::cerr << "[ERROR] Failed to initialize SSDPClient: " << ex.what() << "\n";
        return 1;
    }

    const auto startTime = std::chrono::steady_clock::now();
    std::vector<DiscoveryResult> allResults;
    allResults.reserve(DEFAULT_SEARCH_TARGETS.size());

    bool usedCombined = false;
    try {
        auto combined = client.DiscoverAll(DEFAULT_SEARCH_TARGETS, MX_SECONDS, TIMEOUT_SECONDS);
        if (combined.size() == DEFAULT_SEARCH_TARGETS.size()) {
            allResults = std::move(combined);
            usedCombined = true;
            for (auto& r : allResults) {
                PrintSeparator();
                std::cout << "  ST: " << r.searchTarget << "  -- " << r.packets.size() << " response(s)\n";
                PrintSeparator();
                if (r.packets.empty()) std::cout << "  (no responses)\n";
                for (const auto& pkt : r.packets) {
                    PrintPacket(pkt);
                    writer.SaveActivePacket(r.searchTarget, pkt);
                    dispatcher.Dispatch(pkt, registry);
                }
            }
        }
    } catch (...) { usedCombined = false; }

    if (!usedCombined) {
        for (std::size_t i = 0; i < DEFAULT_SEARCH_TARGETS.size(); ++i) {
            const auto& target = DEFAULT_SEARCH_TARGETS[i];
            PrintSeparator();
            std::cout << "  Search [" << (i+1) << "/" << DEFAULT_SEARCH_TARGETS.size() << "]  ST: " << target << "\n";
            PrintSeparator();
            std::vector<Packet> packets;
            try { packets = client.Discover(target, MX_SECONDS, TIMEOUT_SECONDS); }
            catch (const std::exception& ex) {
                std::cerr << "[WARNING] Discover() failed for '" << target << "': " << ex.what() << "\n";
            }
            if (packets.empty()) std::cout << "  (no responses)\n";
            for (const auto& pkt : packets) {
                PrintPacket(pkt);
                writer.SaveActivePacket(target, pkt);
                dispatcher.Dispatch(pkt, registry);
            }
            allResults.push_back({target, std::move(packets)});
        }
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::cout << "\n  Total discovery time: " << PacketUtilities::FormatElapsed(elapsed) << "\n\n";

    PrintHeader("DOWNLOADING DEVICE DESCRIPTIONS");
    DescriptionDownloader downloader(registry, dispatcher);
    downloader.ProcessPending();
    std::cout << "  Finished fetching descriptions.\n\n";

    client.Shutdown();

    PrintHeader("IDENTITY RESOLUTION ENGINE");
    IdentityResolutionEngine idEngine;
    std::vector<LogicalDevice> logicalDevices = idEngine.Resolve(registry.GetAll());

    if (g_verbose) {
        std::cout << "[DEBUG] IdentityResolutionEngine\n";
        std::cout << "  Logical Devices: " << logicalDevices.size() << "\n";
        for (const auto& d : logicalDevices)
            std::cout << "  - " << (d.displayName.empty() ? d.id : d.displayName)
                      << "\n    Endpoints: " << d.endpoints.size() << "\n";
    }

    PrintHeader("DEVICE INTELLIGENCE PIPELINE");
    ControllerRegistry controllerRegistry;
    ControllerResolver controllerResolver(controllerRegistry);

    for (auto& logicalDev : logicalDevices) {
        if (g_verbose)
            std::cout << "\n[DEBUG] Pipeline for "
                      << (logicalDev.displayName.empty() ? logicalDev.id : logicalDev.displayName) << "\n";

        ProtocolNormalizer::Normalize(logicalDev);
        if (g_verbose) {
            std::cout << "  [ProtocolNormalizer] Normalized Services: " << logicalDev.normalizedServices.size() << "\n";
            for (const auto& svc : logicalDev.normalizedServices)
                std::cout << "    - " << svc.domain << ":" << svc.name << " v" << svc.version << "\n";
        }

        DeviceClassifier::Classify(logicalDev);
        if (g_verbose) {
            std::cout << "  [DeviceClassifier] Primary Class: " << static_cast<int>(logicalDev.primaryClass) << "\n";
            std::cout << "  [DeviceClassifier] Roles: " << logicalDev.roles.size() << "\n";
        }

        CapabilityResolver::Resolve(logicalDev);
        if (g_verbose)
            std::cout << "  [CapabilityResolver] Capabilities: " << logicalDev.capabilities.size() << "\n";

        controllerResolver.Resolve(logicalDev);
        if (g_verbose) {
            std::cout << "  [ControllerResolver] Controller Candidates: " << logicalDev.controllerCandidates.size() << "\n";
            for (const auto& c : logicalDev.controllerCandidates)
                std::cout << "    - " << c.name << " (Rejected: " << c.isRejected
                          << ", Score: " << c.confidence << ", Reason: " << c.diagnosticReason << ")\n";
        }

        ActionResolver::Resolve(logicalDev);
        if (g_verbose)
            std::cout << "  [ActionResolver] Actions: " << logicalDev.actions.size() << "\n";
    }

    if (g_verbose) std::cout << "  Finished intelligence pipeline.\n\n";

    PresentationFormatter::PrintLogicalDevices(logicalDevices);
    PrintDiscoverySummary(allResults, logicalDevices, writer);

    PrintHeader("PHASE 5: COMMAND EXECUTION DEMONSTRATION");
    
    // 1. Initialize execution framework
    TransportRegistry transportRegistry;
    transportRegistry.RegisterTransport(std::make_shared<DummyTransport>());
    DeviceExecutor executor(transportRegistry, controllerRegistry);

    // 2. Find a device with an actionable command to demonstrate execution
    const LogicalDevice* targetDevice = nullptr;
    ActionDescriptor actionToExecute;
    bool isSimulated = false;

    for (const auto& dev : logicalDevices) {
        if (!dev.actions.empty()) {
            targetDevice = &dev;
            actionToExecute = dev.actions[0];
            // Prefer VolumeUp if available
            for (const auto& action : dev.actions) {
                if (action.id == "VolumeUp") {
                    actionToExecute = action;
                    break;
                }
            }
            break;
        }
    }

    // Fallback: If no devices had actions, but we at least found a device, use it to mock an action
    if (!targetDevice && !logicalDevices.empty()) {
        targetDevice = &logicalDevices.front();
        actionToExecute.id = "MockAction";
        actionToExecute.displayName = "Mock Action";
        isSimulated = true;
    }

    if (targetDevice) {
        if (isSimulated) {
            std::cout << "  [SIMULATED -- no real actionable device found on this run]\n";
            std::cout << "  Injecting a synthetic action into the first discovered device to demonstrate the execution framework.\n\n";
        }

        // 3. Build and dispatch the request
        ExecutionRequest req { *targetDevice, actionToExecute, {}, 5000, 0 };
        ExecutionResult res = executor.Execute(req);
        
        std::cout << "  Execution Result: " << ToString(res.status) 
                  << " -- Dummy transport executed in " << res.elapsedTimeMs << "ms.\n";
        if (!res.diagnosticInfo.empty()) {
            std::cout << "  Diagnostic Info : " << res.diagnosticInfo << "\n";
        }
        std::cout << "\n";
    } else {
        std::cout << "  No devices found to demonstrate execution.\n";
    }

    return 0;
}
