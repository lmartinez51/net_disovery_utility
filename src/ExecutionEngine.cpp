#include "ExecutionEngine.h"
#include <chrono>

namespace NetDiscovery {

ExecutionEngine::ExecutionEngine(const TransportRegistry& transportRegistry, 
                                 const ControllerRegistry& controllerRegistry)
    : transportSelector(transportRegistry), controllerRegistry(controllerRegistry) {
}

ExecutionResult ExecutionEngine::Execute(const ExecutionRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    // 1. Find the best controller for the device
    // The device already has controllerCandidates sorted, but ExecutionEngine 
    // asks the registry to get the actual instance to query GetExecutionRoute.
    if (request.device.controllerCandidates.empty()) {
        ExecutionResult res;
        res.status = ExecutionStatus::ExecutionFailed;
        res.errorMessage = "No controllers available for this device.";
        return res;
    }
    
    // Get the top ranked controller name
    std::string topControllerName = request.device.controllerCandidates.front().name;
    auto& controllers = controllerRegistry.GetControllers();
    
    IDeviceController* activeController = nullptr;
    for (const auto& c : controllers) {
        if (c->ControllerName() == topControllerName) {
            activeController = c.get();
            break;
        }
    }

    if (!activeController) {
        ExecutionResult res;
        res.status = ExecutionStatus::ExecutionFailed;
        res.errorMessage = "Failed to resolve controller instance: " + topControllerName;
        return res;
    }

    // 2. Ask the controller for the execution route
    auto routeOpt = activeController->GetExecutionRoute(request.device, request.action);
    if (!routeOpt.has_value()) {
        ExecutionResult res;
        res.status = ExecutionStatus::UnsupportedAction;
        res.errorMessage = "Controller does not know how to execute this action.";
        auto endTime = std::chrono::steady_clock::now();
        res.elapsedTimeMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
        return res;
    }

    // 3. Resolve the transport
    auto transport = transportSelector.SelectTransport(routeOpt.value());
    if (!transport) {
        ExecutionResult res;
        res.status = ExecutionStatus::TransportUnavailable;
        res.errorMessage = "No transport found for family: " + ToString(routeOpt.value().transport);
        auto endTime = std::chrono::steady_clock::now();
        res.elapsedTimeMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
        return res;
    }

    // 4. Dispatch
    ExecutionResult result = transport->Execute(request, routeOpt.value());
    
    // In case the transport didn't fill in elapsed time
    if (result.elapsedTimeMs == 0) {
        auto endTime = std::chrono::steady_clock::now();
        result.elapsedTimeMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
    }

    return result;
}

} // namespace NetDiscovery
