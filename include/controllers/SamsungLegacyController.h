/**
 * @file SamsungLegacyController.h
 * @brief Controller for older Samsung Smart TVs.
 */

#pragma once

#include "../IDeviceController.h"
#include <algorithm>

namespace NetDiscovery {

class SamsungLegacyController : public IDeviceController {
public:
    std::string ControllerName() const override {
        return "SamsungLegacyController";
    }

    std::vector<std::string> SupportedManufacturers() const override {
        return {"Samsung", "Samsung Electronics"};
    }

    std::vector<Capability> SupportedCapabilities() const override {
        return {
            Capability::PowerControl,
            Capability::VolumeControl,
            Capability::Mute,
            Capability::InputSelection,
            Capability::ApplicationLaunching
        };
    }

    bool IsMatch(const LogicalDevice& device) const override {
        if (device.primaryClass != PrimaryDeviceClass::SmartTV) {
            return false;
        }

        std::string mfg = device.manufacturer;
        std::transform(mfg.begin(), mfg.end(), mfg.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (mfg.find("samsung") != std::string::npos) {
            return true;
        }
        
        for (const auto& svc : device.normalizedServices) {
            if (svc.domain == "samsung.com" || svc.domain == "samsung") {
                return true;
            }
        }
        
        return false;
    }

    ResolutionDiagnostics Evaluate(const LogicalDevice& device) const override {
        ResolutionDiagnostics diag;
        diag.score = 0;

        // Manufacturer confirmation is the strongest signal for a vendor-specific
        // controller. Score is set high enough to always outrank generic protocol
        // matches (GenericDLNAController maxes at 100 on a fully-equipped device).
        std::string mfg = device.manufacturer;
        std::transform(mfg.begin(), mfg.end(), mfg.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (mfg.find("samsung") != std::string::npos) {
            diag.score += 100;
            diag.scoreBreakdown.push_back({"Samsung Manufacturer (confirmed)", 100});
            diag.matchedManufacturer = true;
        }

        for (const auto& svc : device.normalizedServices) {
            if (svc.domain == "samsung.com" || svc.domain == "samsung") {
                diag.score += 30;
                diag.scoreBreakdown.push_back({"Samsung Namespace", 30});
                break;
            }
        }

        for (const auto& svc : device.normalizedServices) {
            if (svc.name == "RenderingControl") {
                diag.score += 5;
                diag.scoreBreakdown.push_back({"RenderingControl", 5});
            } else if (svc.name == "AVTransport") {
                diag.score += 5;
                diag.scoreBreakdown.push_back({"AVTransport", 5});
            } else if (svc.name == "dial") {
                diag.score += 2;
                diag.scoreBreakdown.push_back({"DIAL Service", 2});
            }
        }

        if (diag.score > 0) {
            diag.reason = "Samsung manufacturer or specific services matched.";
        } else {
            diag.reason = "Not a Samsung device.";
        }

        return diag;
    }

    bool ValidateEndpoints(const LogicalDevice& device) const override {
        for (const auto& svc : device.normalizedServices) {
            if (svc.name == "RemoteControlReceiver" || svc.name == "MultiScreenService") {
                return true;
            }
        }
        for (const auto& dt : device.signature.deviceTypes) {
            if (dt.find("RemoteControlReceiver") != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    std::vector<ActionDescriptor> VendorActions() const override {
        return {
            {"PowerOn", "Power On", ActionCategory::Power, {}, false, ""},
            {"PowerOff", "Power Off", ActionCategory::Power, {}, false, ""}
        };
    }

    std::optional<ExecutionRoute> GetExecutionRoute(
        const LogicalDevice& device, 
        const ActionDescriptor& action) const override {
        
        ExecutionRoute route;
        
        if (action.id == "PowerOn" || action.id == "PowerOff" || action.id == "SendKey(key)") {
            route.transport = TransportFamily::SamsungRemote;
            route.metadata["KeyCode"] = action.id; // Just as a hint
            
            for (const auto& ep : device.endpoints) {
                if (ep.evidence.upnp.has_value() && ep.evidence.upnp->deviceType.find("RemoteControlReceiver") != std::string::npos) {
                    route.preferredEndpoint = &ep;
                    break;
                }
            }
        } else {
            route.transport = TransportFamily::SOAP;
            for (const auto& ep : device.endpoints) {
                if (ep.evidence.upnp.has_value() && ep.evidence.upnp->deviceType.find("MediaRenderer") != std::string::npos) {
                    route.preferredEndpoint = &ep;
                    break;
                }
            }
        }

        if (!route.preferredEndpoint && !device.endpoints.empty()) {
            route.preferredEndpoint = &device.endpoints[0];
        }

        return route;
    }
};

} // namespace NetDiscovery
