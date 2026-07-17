/**
 * @file ExecutionEngine.h
 * @brief Internal orchestrator for executing commands on a device.
 */

#pragma once

#include "core/ExecutionRequest.h"
#include "core/ExecutionResult.h"
#include "TransportSelector.h"
#include "ControllerRegistry.h"
#include <memory>

namespace NetDiscovery {

class ExecutionEngine {
public:
    ExecutionEngine(const TransportRegistry& transportRegistry, 
                    const ControllerRegistry& controllerRegistry);

    ExecutionResult Execute(const ExecutionRequest& request);

private:
    TransportSelector transportSelector;
    const ControllerRegistry& controllerRegistry;
};

} // namespace NetDiscovery
