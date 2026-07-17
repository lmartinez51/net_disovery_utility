/**
 * @file ExecutionRoute.h
 * @brief Metadata required to execute an action over a specific transport.
 */

#pragma once

#include "TransportFamily.h"
#include "ProtocolEndpoint.h"
#include <string>
#include <vector>
#include <map>

namespace NetDiscovery {

/**
 * @brief Provides the transport execution details for a specific action.
 * Supplied by an IDeviceController.
 */
struct ExecutionRoute {
    TransportFamily transport{TransportFamily::Unknown}; 
    const ProtocolEndpoint* preferredEndpoint{nullptr};
    std::vector<const ProtocolEndpoint*> additionalEndpoints;
    std::map<std::string, std::string> metadata;
};

} // namespace NetDiscovery
