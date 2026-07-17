#include "DeviceExecutor.h"

namespace NetDiscovery {

DeviceExecutor::DeviceExecutor(const TransportRegistry& transportRegistry, 
                               const ControllerRegistry& controllerRegistry)
    : engine(transportRegistry, controllerRegistry) {
}

ExecutionResult DeviceExecutor::Execute(const ExecutionRequest& request) {
    return engine.Execute(request);
}

} // namespace NetDiscovery
