# NetDiscovery

A production-quality, portable C++17 network device discovery and intelligence library.

NetDiscovery discovers network devices — Samsung TVs, Sonos speakers,
Philips Hue bridges, Chromecasts, Home Assistant — using standard discovery
protocols (SSDP, UPnP, etc.). It features an advanced **Device Intelligence Pipeline**
that automatically classifies devices, resolves physical identity, computes 
capabilities, and dynamically maps execution controllers.

The Windows backend is the first implementation; the architecture is completely 
decoupled and designed for future portability to **ESP-IDF** with minimal code changes.

---

## Current Phase

**Phase 4.5 — Architecture Consolidation & Intelligence Pipeline**

The project has successfully moved beyond discovery into automated device intelligence:
- Active and Passive SSDP Discovery
- Concurrent HTTP fetching and UPnP XML Parsing
- Deterministic Device Classification and Normalization
- Heuristic Evidence-based Identity Resolution (merging multi-protocol endpoints into single Logical Devices)
- Automated Capability and Action resolving
- Three-stage Controller Validation (IsMatch, Evaluate, ValidateEndpoints)

---

## Directory Layout

```
NetDiscovery/
├── CMakeLists.txt          — CMake build (C++17, MSVC, WinSock2)
├── README.md               — This file
│
├── include/                — Public API headers (platform-neutral)
│   ├── core/               — Core models (LogicalDevice, ActionDescriptor, IdentityEvidence)
│   ├── controllers/        — Protocol controllers (SamsungLegacy, GenericDLNA, Unknown)
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
  > [PREFERRED] SamsungLegacyController (Confidence: 130)
      +100 Samsung Manufacturer (confirmed)
      +30 Samsung Namespace
  ✓ [ACCEPTED] UnknownController (Confidence: 1)
      +1 Fallback matching
  ✗ [REJECTED] GenericDLNAController
      Reason: Failed ValidateEndpoints
```

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
| **1-2** | SSDP Discovery & Parsing | ✅ Complete |
| **3** | HTTP Client & UPnP/XML Parsing | ✅ Complete |
| **4** | LogicalDevice & Identity Fusion | ✅ Complete |
| **4.5**| Deterministic Intelligence Pipeline | ✅ Complete |
| **5** | Execution Protocols (SOAP/DIAL/WS) | 🔲 Next |
| **6** | Embedded ESP-Claw Porting | 🔲 Future |

---

## Design Principles

- **Separation of Concerns** — Discovery, Data Extraction, Classification, and Controller Execution are rigorously separated.
- **Evidence-Based Identity** — Devices are identified heuristically without rigid assumptions.
- **No external libraries** — heavily relies on pure C++17 standard library functionality.
- **Platform Agnostic** — Windows winsock API is hidden entirely behind interfaces.

---

## License

Internal / research use. Not for public distribution.
