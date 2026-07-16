cmake_minimum_required(VERSION 3.20)

# ============================================================
# Project declaration
# ============================================================
project(NetDiscovery
    VERSION     1.0.0
    DESCRIPTION "SSDP / UPnP network device discovery library"
    LANGUAGES   CXX
)

# ============================================================
# C++ standard
# ============================================================
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)   # Strict ISO C++17, no GNU extensions

# ============================================================
# Output directories
# ============================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ============================================================
# Sources
# ============================================================
set(NETDISCOVERY_SOURCES
    # ── Orchestrator ──────────────────────────────────
    src/main.cpp

    # ── Core layer helpers ────────────────────────────
    src/Header.cpp              # FindHeader / GetHeaderValue free functions

    # ── Transport layer ───────────────────────────────
    src/UdpSocket.cpp           # Active M-SEARCH transport
    src/MulticastSocket.cpp     # Passive multicast receiver
    src/SSDPClient.cpp          # Active + passive SSDP client (transport)
    src/TcpSocket.cpp           # Phase 3 stub

    # ── Protocol analysis layer ───────────────────────
    src/SSDPAnalyzer.cpp        # IProtocolAnalyzer: SSDP
    src/XmlAnalyzer.cpp         # IProtocolAnalyzer: UPnP XML
    src/AnalyzerDispatcher.cpp  # Routes Packets to registered analyzers

    # ── Utility layer ─────────────────────────────────
    src/PacketUtilities.cpp     # DetectType, ParseHeaders, FormatTimestamp

    # ── Framework (existing, unmodified) ──────────────
    src/CaptureWriter.cpp
    src/HttpClient.cpp          # HTTP fetcher
    src/DescriptionDownloader.cpp
    src/DeviceRegistry.cpp
    # ── Phase 3: Resolution Engine ────────────────────
    src/ControllerRegistry.cpp
    src/ControllerResolver.cpp

    # ── Phase 4.5: Architecture Consolidation ─────────
    src/ProtocolNormalizer.cpp
    src/DeviceClassifier.cpp
    src/CapabilityResolver.cpp
    src/ActionResolver.cpp

    # ── Phase 4: Device Fusion Engine ─────────────────
    src/DeviceFusionEngine.cpp
    src/PresentationFormatter.cpp
    src/IdentityResolutionEngine.cpp
)

set(NETDISCOVERY_HEADERS
    # ── Core layer (include/core/) ────────────────────
    include/core/TransportProtocol.h
    include/core/ProtocolType.h
    include/core/Endpoint.h
    include/core/Header.h
    include/core/Capability.h
    include/core/Packet.h
    include/core/Device.h

    # ── Phase 4 Models (core/) ────────────────────────
    include/core/ActionDescriptor.h
    include/core/LogicalDevice.h
    include/core/ProtocolEndpoint.h
    include/core/Provenance.h
    include/core/DeviceClass.h
    include/core/NormalizedService.h

    # ── Protocol interface & analyzers ────────────────
    include/IProtocolAnalyzer.h
    include/SSDPAnalyzer.h
    include/AnalyzerDispatcher.h

    # ── Phase 3: Resolution Engine ────────────────────
    include/IDeviceController.h
    include/ControllerRegistry.h
    include/ControllerResolver.h
    
    # ── Phase 4.5: Architecture Consolidation ─────────
    include/ProtocolNormalizer.h
    include/DeviceClassifier.h
    include/CapabilityResolver.h
    include/ActionResolver.h
    include/controllers/GenericDLNAController.h
    include/controllers/SamsungLegacyController.h
    include/controllers/UnknownController.h

    # ── Phase 4: Device Fusion Engine ─────────────────
    include/DeviceFusionEngine.h
    include/IdentityResolutionEngine.h
    include/PresentationFormatter.h

    # ── Transport layer ───────────────────────────────
    include/UdpSocket.h
    include/MulticastSocket.h
    include/TcpSocket.h

    # ── Utility layer ─────────────────────────────────
    include/PacketUtilities.h
    include/PacketAnalyzer.h    # Deprecated forwarding shim → PacketUtilities

    # ── Framework (existing) ──────────────────────────
    include/Device.h            # Deprecated forwarding shim → core/Device.h
    include/SSDPResponse.h      # Phase 1 legacy diagnostic type
    include/SSDPClient.h
    include/CaptureWriter.h
    include/HttpClient.h
    include/XmlAnalyzer.h
    include/DescriptionDownloader.h
    include/DeviceRegistry.h
)

# ============================================================
# Executable target
# ============================================================
add_executable(NetDiscovery
    ${NETDISCOVERY_SOURCES}
    ${NETDISCOVERY_HEADERS}   # Added so headers appear in IDE project trees
)

# ============================================================
# Include directories
# ============================================================
target_include_directories(NetDiscovery
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include       # Public headers
        ${CMAKE_CURRENT_SOURCE_DIR}/include/core  # Core layer (allows #include "Packet.h" style)
        ${CMAKE_CURRENT_SOURCE_DIR}/src           # Internal helpers (WinSock2Helpers.h)
)

# ============================================================
# Windows / WinSock2 configuration
# ============================================================
if(WIN32)
    # _WIN32_WINNT=0x0601 targets Windows 7 minimum.
    # This is required for inet_pton(), which is available from WS2tcpip.h
    # on Win7+ but was not present in earlier SDK headers.
    target_compile_definitions(NetDiscovery
        PRIVATE
            _WIN32_WINNT=0x0601
            WIN32_LEAN_AND_MEAN    # Exclude rarely-used Windows headers
            NOMINMAX               # Prevent windows.h from defining min/max macros
    )

    # Link Ws2_32.lib for WinSock2 APIs.
    # SSDPClient.cpp also does this via #pragma comment(lib, "Ws2_32.lib"),
    # but an explicit CMake target_link_libraries is the portable approach.
    target_link_libraries(NetDiscovery
        PRIVATE
            Ws2_32
    )
endif()

# ============================================================
# Compiler warning flags
# ============================================================
if(MSVC)
    target_compile_options(NetDiscovery
        PRIVATE
            /W4         # Warning level 4 (comprehensive)
            /WX         # Treat warnings as errors
            /permissive-  # Standards-conforming mode
            /utf-8      # Source and execution charset UTF-8
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Not used in Phase 1 (MSVC target), but defined for future cross-compilation.
    target_compile_options(NetDiscovery
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
    )
endif()

# ============================================================
# Installation rules (optional — for packaging Phase 6+)
# ============================================================
install(TARGETS NetDiscovery
    RUNTIME DESTINATION bin
)
install(FILES ${NETDISCOVERY_HEADERS}
    DESTINATION include/NetDiscovery
)

# ============================================================
# Status summary
# ============================================================
message(STATUS "--------------------------------------------")
message(STATUS "NetDiscovery configuration:")
message(STATUS "  Version    : ${PROJECT_VERSION}")
message(STATUS "  C++ std    : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler   : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output     : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "--------------------------------------------")
