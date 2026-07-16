/**
 * @file LogicalDevice.h
 * @brief Represents a physical device, fused from multiple protocol endpoints.
 */

#pragma once

#include "ProtocolEndpoint.h"
#include "Capability.h"
#include "ActionDescriptor.h"
#include "ControllerCandidate.h"
#include "Provenance.h"
#include "DeviceFingerprint.h"
#include "DeviceSignature.h"
#include "DeviceClass.h"
#include "NormalizedService.h"

#include <string>
#include <vector>
#include <map>

namespace NetDiscovery {

/**
 * @brief The final representation of a discovered physical device.
 */
struct LogicalDevice {
    // ----------------------------------------------------------------
    // Identity & Naming
    // ----------------------------------------------------------------
    std::string id;            // Primary unique identifier (usually root UUID)
    std::string displayName;   // Best available human-readable name
    std::string suggestedName; // Name optimized for Voice/LLM
    std::vector<std::string> aliases;

    std::string manufacturer;
    std::string model;
    std::string serialNumber;
    
    // ----------------------------------------------------------------
    // Fusion Metadata
    // ----------------------------------------------------------------
    int confidence{0};         // 0-100 score of how certain the engine is about the fusion
    std::vector<std::pair<std::string, int>> confidenceBreakdown; // Details of identity confidence
    Provenance provenance;     // Tracks the origin of metadata values

    PrimaryDeviceClass primaryClass{PrimaryDeviceClass::Unknown};
    std::vector<DeviceRole> roles;
    std::vector<NormalizedService> normalizedServices;

    // ----------------------------------------------------------------
    // Network & Protocols
    // ----------------------------------------------------------------
    std::string primaryIp;
    std::vector<ProtocolEndpoint> endpoints;

    // ----------------------------------------------------------------
    // Execution & Controllers
    // ----------------------------------------------------------------
    std::vector<Capability> capabilities;
    std::vector<ActionDescriptor> actions;
    
    std::vector<ControllerCandidate> controllerCandidates;
    
    // ----------------------------------------------------------------
    // Resolved Evidence Snapshot
    // ----------------------------------------------------------------
    DeviceFingerprint fingerprint; // Legacy fingerprint (to be phased out)
    DeviceSignature signature;     // Fully normalized protocol-independent identity
};

} // namespace NetDiscovery
