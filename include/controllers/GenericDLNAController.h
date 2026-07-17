/**
 * @file GenericDLNAController.h
 * @brief Generic DLNA/UPnP controller metadata.
 */

#pragma once

#include "../IDeviceController.h"

namespace NetDiscovery {

class GenericDLNAController : public IDeviceController {
public:
    std::string ControllerName() const override {
        return "GenericDLNAController";
    }

    std::vector<std::string> SupportedManufacturers() const override {
        return {}; // Generic supports any
    }

    std::vector<Capability> SupportedCapabilities() const override {
        return {
            Capability::MediaPlayback,
            Capability::MediaTransport,
            Capability::VolumeControl,
            Capability::Mute
        };
    }

    bool IsMatch(const LogicalDevice& device) const override {
        if (device.primaryClass == PrimaryDeviceClass::InternetGateway || 
            device.primaryClass == PrimaryDeviceClass::Unknown ||
            device.primaryClass == PrimaryDeviceClass::IoTDevice) {
            return false;
        }

        if (device.primaryClass == PrimaryDeviceClass::MediaRenderer ||
            device.primaryClass == PrimaryDeviceClass::MediaServer ||
            device.primaryClass == PrimaryDeviceClass::SmartTV) {
            return true;
        }

        return false;
    }

    ResolutionDiagnostics Evaluate(const LogicalDevice& device) const override {
        ResolutionDiagnostics diag;
        diag.score = 0;

        for (const auto& svc : device.normalizedServices) {
            if (svc.domain == "schemas-upnp-org" || svc.domain == "upnp") {
                diag.score += 20;
                diag.scoreBreakdown.push_back({"UPnP Namespace", 20});
                break; // Only score once for namespace
            }
        }
        
        for (const auto& svc : device.normalizedServices) {
            if (svc.name == "MediaRenderer") {
                diag.score += 40;
                diag.scoreBreakdown.push_back({"MediaRenderer", 40});
            } else if (svc.name == "AVTransport") {
                diag.score += 30;
                diag.scoreBreakdown.push_back({"AVTransport", 30});
            } else if (svc.name == "RenderingControl") {
                diag.score += 10;
                diag.scoreBreakdown.push_back({"RenderingControl", 10});
            }
        }

        if (diag.score >= 50) {
            diag.reason = "Generic DLNA services detected.";
        } else {
            diag.reason = "Not a full DLNA device.";
        }

        return diag;
    }

    bool ValidateEndpoints(const LogicalDevice& device) const override {
        for (const auto& svc : device.normalizedServices) {
            if (svc.name == "RenderingControl" || svc.name == "AVTransport" || svc.name == "ContentDirectory") {
                return true;
            }
        }
        return false;
    }

    std::optional<ExecutionRoute> GetExecutionRoute(
        const LogicalDevice& device, 
        const ActionDescriptor& action) const override {
        
        ExecutionRoute route;
        route.transport = TransportFamily::SOAP;
        
        // Find the most appropriate endpoint based on action category
        std::string targetService;
        if (action.category == ActionCategory::MediaPlayback || action.category == ActionCategory::MediaTransport) {
            targetService = "AVTransport";
        } else if (action.category == ActionCategory::System || action.category == ActionCategory::Unknown) {
            targetService = "RenderingControl"; // default for volume etc
        } else {
            targetService = "RenderingControl";
        }

        // Try to find the endpoint that provides this service
        for (const auto& ep : device.endpoints) {
            if (ep.evidence.upnp.has_value()) {
                // Simplified matching for now - just returning the first UPnP endpoint
                // A real implementation would parse the xml to find the specific control URL
                route.preferredEndpoint = &ep;
                break;
            }
        }

        if (!route.preferredEndpoint && !device.endpoints.empty()) {
            route.preferredEndpoint = &device.endpoints[0];
        }

        route.metadata["ServiceType"] = targetService;
        route.metadata["SOAPACTION"] = action.id;

        return route;
    }
};

} // namespace NetDiscovery
