/**
 * @file AnalyzerDispatcher.cpp
 * @brief AnalyzerDispatcher implementation.
 */

#include "../include/AnalyzerDispatcher.h"

#include <iostream>
#include <stdexcept>

namespace NetDiscovery {

// ============================================================
// Register()
// ============================================================

void AnalyzerDispatcher::Register(std::unique_ptr<IProtocolAnalyzer> analyzer)
{
    if (!analyzer) {
        throw std::invalid_argument("AnalyzerDispatcher::Register — analyzer must not be null.");
    }
    std::cout << "[Dispatcher] Registered analyzer: " << analyzer->Name() << "\n";
    m_analyzers.push_back(std::move(analyzer));
}

// ============================================================
// Dispatch()
// ============================================================

void AnalyzerDispatcher::Dispatch(const Packet& packet, DeviceRegistry& registry) const
{
    for (const auto& analyzer : m_analyzers) {
        try {
            analyzer->Analyze(packet, registry);
        } catch (const std::exception& ex) {
            std::cerr << "[Dispatcher] Analyzer '" << analyzer->Name()
                      << "' threw: " << ex.what() << "\n";
            // Continue to next analyzer — one failure must not block others.
        }
    }
}

// ============================================================
// GetAnalyzerNames()
// ============================================================

std::vector<std::string> AnalyzerDispatcher::GetAnalyzerNames() const
{
    std::vector<std::string> names;
    names.reserve(m_analyzers.size());
    for (const auto& a : m_analyzers) {
        names.push_back(a->Name());
    }
    return names;
}

// ============================================================
// AnalyzerCount()
// ============================================================

std::size_t AnalyzerDispatcher::AnalyzerCount() const noexcept
{
    return m_analyzers.size();
}

} // namespace NetDiscovery
