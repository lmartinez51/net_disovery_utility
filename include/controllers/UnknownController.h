/**
 * @file UnknownController.h
 * @brief Fallback controller when no others match.
 */

#pragma once

#include "../IDeviceController.h"

namespace NetDiscovery {

class UnknownController : public IDeviceController {
public:
    std::string ControllerName() const override {
        return "UnknownController";
    }

    std::vector<std::string> SupportedManufacturers() const override {
        return {}; 
    }

    std::vector<Capability> SupportedCapabilities() const override {
        return {};
    }

    bool IsMatch(const LogicalDevice& /*device*/) const override {
        return true;
    }

    ResolutionDiagnostics Evaluate(const LogicalDevice& /*device*/) const override {
        ResolutionDiagnostics diag;
        diag.score = 1; // Always matches, but with lowest possible score
        diag.scoreBreakdown.push_back({"Fallback matching", 1});
        diag.reason = "Fallback controller.";
        return diag;
    }

    bool ValidateEndpoints(const LogicalDevice& /*device*/) const override {
        return true;
    }
};

} // namespace NetDiscovery
