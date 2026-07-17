/**
 * @file ExecutionResult.h
 * @brief Represents the outcome of an execution request.
 */

#pragma once

#include <string>

namespace NetDiscovery {

enum class ExecutionStatus {
    Success,
    UnsupportedAction,
    TransportUnavailable,
    Timeout,
    AuthenticationRequired,
    ProtocolError,
    ExecutionFailed
};

inline std::string ToString(ExecutionStatus status) {
    switch (status) {
        case ExecutionStatus::Success:                return "Success";
        case ExecutionStatus::UnsupportedAction:      return "UnsupportedAction";
        case ExecutionStatus::TransportUnavailable:   return "TransportUnavailable";
        case ExecutionStatus::Timeout:                return "Timeout";
        case ExecutionStatus::AuthenticationRequired: return "AuthenticationRequired";
        case ExecutionStatus::ProtocolError:          return "ProtocolError";
        case ExecutionStatus::ExecutionFailed:        return "ExecutionFailed";
        default:                                      return "Unknown";
    }
}

/**
 * @brief Structured result from an ICommunicationTransport execution.
 */
struct ExecutionResult {
    ExecutionStatus status;
    int elapsedTimeMs{0};
    std::string errorMessage;
    std::string diagnosticInfo;
};

} // namespace NetDiscovery
