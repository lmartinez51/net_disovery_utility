/**
 * @file ActionResolver.cpp
 * @brief ActionResolver implementation.
 */

#include "../include/ActionResolver.h"
#include <algorithm>

namespace NetDiscovery {

void ActionResolver::Resolve(LogicalDevice& device) {
    device.actions.clear();
    
    auto addAction = [&device](const std::string& actionId) {
        // Prevent duplicates
        for (const auto& a : device.actions) {
            if (a.id == actionId) return;
        }
        ActionDescriptor desc;
        desc.id = actionId;
        // In a real system, parameters would be mapped here too.
        device.actions.push_back(desc);
    };
    
    for (const auto& cap : device.capabilities) {
        switch (cap) {
            case Capability::PowerControl:
                addAction("PowerOn");
                addAction("PowerOff");
                break;
            case Capability::VolumeControl:
                addAction("VolumeUp");
                addAction("VolumeDown");
                addAction("SetVolume(level)");
                break;
            case Capability::Mute:
                addAction("Mute");
                addAction("Unmute");
                break;
            case Capability::MediaPlayback:
                addAction("Play");
                addAction("Pause");
                addAction("Stop");
                break;
            case Capability::MediaTransport:
                addAction("Next");
                addAction("Previous");
                addAction("Seek");
                break;
            case Capability::ApplicationLaunching:
                addAction("LaunchApplication(name)");
                break;
            case Capability::InputSelection:
                addAction("SelectInput(input)");
                break;
            case Capability::RemoteControl:
                addAction("SendKey(key)");
                break;
            default:
                break;
        }
    }
}

} // namespace NetDiscovery
