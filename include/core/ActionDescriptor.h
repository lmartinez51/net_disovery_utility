/**
 * @file ActionDescriptor.h
 * @brief Structured representation of an executable action.
 */

#pragma once

#include <string>
#include <vector>

namespace NetDiscovery {

/**
 * @brief High-level categories for actions.
 */
enum class ActionCategory {
    Power,
    MediaPlayback,
    MediaTransport,
    Navigation,
    System,
    Display,
    Unknown
};

/**
 * @brief A parameter required to execute an action.
 */
struct ActionParameter {
    std::string name;
    std::string type; // e.g., "int", "string", "bool"
};

/**
 * @brief Full description of an action that can be executed by a controller.
 */
struct ActionDescriptor {
    std::string id;            // e.g., "SetVolume", "PowerOn"
    std::string displayName;   // e.g., "Set Volume", "Turn On"
    ActionCategory category;
    
    std::vector<ActionParameter> supportedParameters;
    
    bool requiresConfirmation{false};
    
    // Metadata specific to the controller, opaque to LLM
    std::string controllerMetadata;
};

} // namespace NetDiscovery
