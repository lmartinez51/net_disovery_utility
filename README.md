# NetDiscovery (Device Intelligence & Execution Framework)

This repository is a complete **Device Intelligence & Execution Framework** built in C++17. Its responsibilities are explicitly separated into five distinct stages:

**Discover &rarr; Understand &rarr; Decide &rarr; Execute &rarr; Maintain State**

Discovery is only the first stage of a much larger execution architecture. The long-term goal is an execution framework capable of controlling heterogeneous devices regardless of manufacturer or protocol. Samsung is merely the first validation platform; the architecture must always remain strictly vendor-neutral.

While it currently runs natively on Windows/Linux, it is designed as the foundation for the ESP32 `esp32s3-camila` project. The framework will eventually provide tool-calling capabilities for an LLM to autonomously control heterogeneous IoT environments.

The Windows backend is the first implementation; the architecture is completely 
decoupled and designed for future portability to **ESP-IDF** with minimal code changes.

---

## Current Phase

**Phase 9.1 — Vendor Transport Framework & Samsung Backend**

The project has successfully moved beyond discovery and foundational intelligence into execution:
- Active and Passive SSDP Discovery
- Concurrent HTTP fetching and UPnP XML Parsing
- Heuristic Evidence-based Identity Resolution
- Deterministic Device Classification and Normalization
- Device Knowledge Store and Persistence
- Universal Execution Framework (Execution Engine, Transport Registry)
- Shared Execution Services (Execution Context, Knowledge Synchronization)
- Communication Transports (SOAP, DIAL)
- Architectural Hardening & Validation Subsystem
- Vendor-Specific Controllers (SamsungController)

---

## Directory Layout

```
NetDiscovery/
├── CMakeLists.txt          — CMake build (C++17, MSVC, WinSock2)
├── README.md               — This file
│
├── include/                — Public API headers (platform-neutral)
│   ├── core/               — Core models (LogicalDevice, ActionDescriptor, IdentityEvidence)
│   ├── controllers/        — Protocol controllers (Samsung, GenericDLNA, Unknown)
│   ├── Device.h            — Raw UPnP/SSDP device models
│   ├── SSDPClient.h        — Multicast UDP discovery client
│   ├── HttpClient.h        — Non-blocking TCP HTTP client
│   ├── XmlAnalyzer.h       — Modular UPnP XML parsing architecture
│   ├── IdentityResolutionEngine.h — Fuses evidence into distinct Logical Devices
│   ├── ProtocolNormalizer.h— Maps protocol specifics to normalized features
│   ├── DeviceClassifier.h  — Evaluates primary class and roles deterministically
│   ├── CapabilityResolver.h— Translates services/roles into capabilities
│   └── ControllerResolver.h— Dynamically assigns controllers to devices
│
└── src/                    — Implementations
    ├── main.cpp            — Discovery & Intelligence orchestration
    ├── SSDPClient.cpp      — WinSock2 UDP implementation
    ├── HttpClient.cpp      — WinSock2 TCP implementation
    ├── XmlAnalyzer.cpp     — XML ingestion and metadata extraction
    └── ...                 — Intelligence pipeline implementations
```

---

## Build Instructions

### Prerequisites

| Requirement | Version |
|---|---|
| Windows | 7 or later |
| Visual Studio | 2019 or 2022 (with C++ Desktop workload) |
| CMake | 3.20 or later |

### CMake build (command line)

```powershell
# From the NetDiscovery project root:
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The executable will be placed at:

```
build\bin\Release\NetDiscovery.exe
```

### Run

```powershell
.\build\bin\Release\NetDiscovery.exe
```

### Expected Output

The executable discovers devices, downloads their metadata, and pipes them through the intelligence engine:

```
======================================================================
  DEVICE INTELLIGENCE ENGINE — Logical Devices
======================================================================

  [TV] Samsung

  Identity Confidence: 90%
    +30 Manufacturer
    +25 Model
    +20 Serial
    +15 FriendlyName

  Device Signature Evidence
  - Manufacturer : Samsung Electronics
  - Model        : UN40J5200
  - FriendlyName : [TV] Samsung
  - Primary Class: Smart TV

  Capabilities
  ✓ Application Launching
  ✓ Power Control
  ✓ Volume Control
  ✓ Mute
  ✓ Input Selection
  ✓ Media Playback
  ✓ Media Transport
  ✓ Remote Control

  Supported Actions
  ✓ PowerOn
  ✓ PowerOff
  ✓ LaunchApplication(name)
  ✓ VolumeUp
  ✓ VolumeDown
  ✓ SetVolume(level)
  ✓ Mute
  ✓ Unmute
  ✓ SelectInput(input)
  ✓ Play
  ✓ Pause
  ✓ Stop
  ✓ Next
  ✓ Previous
  ✓ Seek
  ✓ SendKey(key)

  Protocol Endpoints
  - SSDP Discovery (192.168.1.13)
  - UPnP RemoteControlReceiver (192.168.1.13)

  Controller Candidates
  > [PREFERRED] SamsungController (Confidence: 130)
      +100 Samsung Manufacturer (confirmed)
      +30 Samsung Namespace
  ✓ [ACCEPTED] UnknownController (Confidence: 1)
      +1 Fallback matching
  ✗ [REJECTED] GenericDLNAController
      Reason: Failed ValidateEndpoints
```

---

## Troubleshooting

### Zero Devices Discovered on Windows Wi-Fi
If discovery unexpectedly returns 0 responses across all search targets (including `ssdp:all`) despite devices being online and the application compiling successfully, you may be hitting a known Windows Wi-Fi driver bug (WDI architecture, introduced in Windows 10) that silently drops multicast traffic after a Wi-Fi reconnect event (e.g., toggling airplane mode, roaming, waking from sleep).

**Workaround**: Simply disconnect and reconnect your Wi-Fi adapter (or disable/re-enable the interface). Discovery will immediately start working again.

---

## Architecture

The public API in `include/` is **platform-neutral**. All platform-specific
code lives exclusively in `src/SSDPClient.cpp` and `src/HttpClient.cpp`.

To port to ESP-IDF:

1. Replace `WSAStartup` / `WSACleanup` in `SSDPClient` / `HttpClient` with lwIP init.
2. Replace `INVALID_SOCKET` / `SOCKET_ERROR` with `-1` / `errno` checks.
3. Replace `closesocket()` with `close()`.
4. Replace `WSAETIMEDOUT` with `EAGAIN` / `EWOULDBLOCK`.
5. The core BSD socket calls — `socket()`, `sendto()`, `recvfrom()`, `setsockopt()`, `inet_pton()`, `htons()` — are identical on both platforms.

The pipeline architecture (`DeviceFusionEngine`, `ControllerResolver`, `XmlAnalyzer`, etc.) requires **zero changes** between platforms.

---

## Future Roadmap

| Phase | Feature | Status |
|---|---|---|
| **1** | Core Networking (TCP/UDP/Multicast) | ✅ |
| **2** | SSDP Protocol (M-SEARCH, Notifications) | ✅ |
| **3** | HTTP Client & UPnP/XML Parsing | ✅ |
| **4** | Discovery | ✅ |
| **4.5** | Intelligence Pipeline | ✅ |
| **5** | Execution Framework | ✅ |
| **5.5** | Device Knowledge Store | ✅ |
| **6** | DIAL Transport | ✅ |
| **7** | SOAP Transport | ✅ |
| **7.5** | **Shared Execution Services**<br>- Execution Context (✅ Built)<br>- Retry Policy (⏳ Deferred)<br>- Authentication Manager (⏳ Deferred)<br>- Transport Capabilities (⏳ Deferred) | ✅ |
| **8** | **SOAP Response Parsing Framework**<br>- Service-Specific Parsers (`RenderingControlParser`, etc.)<br>- Parsers interpret protocol payloads into fields | ✅ |
| **8.5** | **Architectural Hardening**<br>- Capability-driven validation subsystem<br>- Universal vocabulary refinement | ✅ |
| **9** | **Vendor Transport Framework**<br>- `VendorTransport` base interface<br>- Transport Registry (decoupled backend registration) | ✅ |
| **9.1** | **First Vendor Backend (Samsung)**<br>- `SamsungController` (IR, SOAP, DIAL, WebSocket strategies)<br>- Samsung Remote (WebSocket / Proprietary APIs) | ✅ |
| **10** | **Semantic Execution Layer**<br>- Application Resolver<br>- Vendor Parameter Mapping<br>- Action Normalization<br>- Semantic Layer reasons over typed information | ⭐ |
| **11** | **ESP32 Runtime Port**<br>- Migration to esp32s3-camila<br>- ESP-Claw integration<br>- Lua bindings<br>- Tool Calling bridge<br>- NVS backend<br>- FreeRTOS services | ⏳ |

---

## Target Architecture

The Execution Engine remains completely agnostic to the specific manufacturer or protocol. It simply executes an `ExecutionRoute` provided by the Controller Resolver.

The framework enforces a strict separation of concerns for execution:
- **Controller**: Decides *HOW* an action should be executed based on the manufacturer/device.
- **Strategy**: Decides *WHICH* execution mechanism should be used.
- **Transport**: Performs the actual protocol communication.

```text
Semantic Action
        │
        ▼
Execution Engine
        │
        ▼
Controller Resolver
        │
        ├── SamsungController
        │      ├── IR Strategy          -> IR Transport
        │      ├── SOAP Strategy        -> SOAP Transport
        │      ├── DIAL Strategy        -> DIAL Transport
        │      └── WebSocket Strategy   -> WebSocket Transport
        │
        ├── RokuController
        │      ├── ECP Strategy         -> ECP Transport
        │      └── DIAL Strategy        -> DIAL Transport
        │
        ├── PhilipsHueController
        │      └── REST Strategy        -> REST Transport
        │
        ├── ChromecastController
        │      └── DIAL Strategy        -> DIAL Transport
        │
        └── GenericDLNAController
               ├── SOAP Strategy        -> SOAP Transport
               └── DIAL Strategy        -> DIAL Transport
```

---

## Architecture Principles

- **Controllers decide.**
- **Strategies choose execution mechanisms.**
- **Transports communicate.**
- **Parsers interpret protocol payloads.**
- **Semantic Layer reasons over typed information.**
- **Execution Engine orchestrates.**
- **Device Intelligence understands devices.**

---

## Design Principles

- **Separation of Concerns** — Discovery, Data Extraction, Classification, and Controller Execution are rigorously separated.
- **Evidence-Based Identity** — Devices are identified heuristically without rigid assumptions.
- **Minimal external dependencies** — heavily relies on pure C++17 standard library functionality, using only tinyxml2 for lightweight XML parsing.
- **Platform Agnostic** — Windows winsock API is hidden entirely behind interfaces.

---

## License

Internal / research use. Not for public distribution.
