/**
 * @file CaptureWriter.cpp
 * @brief CaptureWriter implementation — writes Packets to the filesystem.
 */

#include "../include/CaptureWriter.h"
#include "../include/PacketUtilities.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace NetDiscovery {

// ============================================================
// Constructor
// ============================================================

CaptureWriter::CaptureWriter(const std::string& basePath)
    : m_basePath(std::filesystem::absolute(basePath))
    , m_activeDir(m_basePath / "active")
    , m_passiveDir(m_basePath / "passive")
{
    std::filesystem::create_directories(m_activeDir);
    std::filesystem::create_directories(m_passiveDir);
}

// ============================================================
// SaveActivePacket()
// ============================================================

void CaptureWriter::SaveActivePacket(const std::string& searchTarget,
                                      const Packet&       packet)
{
    ++m_activeCount;
    ++m_fileCount;

    const std::string senderSafe  = SanitizeForFilename(packet.source.address);
    const std::string targetSafe  = SanitizeForFilename(searchTarget);
    const std::string prefix      = targetSafe + "_" + senderSafe;

    const std::filesystem::path filepath =
        BuildFilename(m_activeDir, prefix, ".txt");

    // Build a human-readable metadata header for the file.
    std::ostringstream meta;
    meta << "Timestamp : " << PacketUtilities::FormatTimestamp(packet.timestamp) << "\n"
         << "ST        : " << searchTarget                                        << "\n"
         << "Source    : " << packet.source.address << ":" << packet.source.port << "\n"
         << "Dest      : " << packet.destination.address << ":" << packet.destination.port << "\n"
         << "Transport : " << ToString(packet.transport)                          << "\n"
         << "Protocol  : " << ToString(packet.protocol)                           << "\n";

    WritePacketFile(filepath, meta.str(), packet);
}

// ============================================================
// SavePassivePacket()
// ============================================================

void CaptureWriter::SavePassivePacket(const Packet& packet)
{
    ++m_passiveCount;
    ++m_fileCount;

    const PacketType type = PacketUtilities::DetectType(packet.rawPayload);
    const std::string typeSafe   = SanitizeForFilename(PacketUtilities::TypeToString(type));
    const std::string senderSafe = SanitizeForFilename(packet.source.address);
    const std::string prefix     = typeSafe + "_" + senderSafe;

    const std::filesystem::path filepath =
        BuildFilename(m_passiveDir, prefix, ".txt");

    std::ostringstream meta;
    meta << "Timestamp : " << PacketUtilities::FormatTimestamp(packet.timestamp) << "\n"
         << "Type      : " << PacketUtilities::TypeToString(type)                 << "\n"
         << "Source    : " << packet.source.address << ":" << packet.source.port << "\n"
         << "Transport : " << ToString(packet.transport)                          << "\n"
         << "Protocol  : " << ToString(packet.protocol)                           << "\n";

    WritePacketFile(filepath, meta.str(), packet);
}

// ============================================================
// BasePath()
// ============================================================

const std::filesystem::path& CaptureWriter::BasePath() const noexcept
{
    return m_basePath;
}

// ============================================================
// FileCount()
// ============================================================

uint32_t CaptureWriter::FileCount() const noexcept
{
    return m_fileCount;
}

// ============================================================
// BuildFilename()
// ============================================================

std::filesystem::path CaptureWriter::BuildFilename(
    const std::filesystem::path& dir,
    const std::string&            prefix,
    const std::string&            suffix) const
{
    // Use the combined active+passive sequence so numbers never repeat
    // within a session, making it easy to correlate timestamps.
    std::ostringstream name;
    name << std::setw(3) << std::setfill('0') << m_fileCount
         << "_" << prefix << suffix;

    std::filesystem::path candidate = dir / name.str();

    // Guard against extremely rare collisions (e.g. rapid same-IP responses).
    int disambig = 0;
    while (std::filesystem::exists(candidate)) {
        ++disambig;
        std::ostringstream retry;
        retry << std::setw(3) << std::setfill('0') << m_fileCount
              << "_" << prefix << "_" << disambig << suffix;
        candidate = dir / retry.str();
    }
    return candidate;
}

// ============================================================
// SanitizeForFilename()
// ============================================================

std::string CaptureWriter::SanitizeForFilename(const std::string& raw)
{
    std::string result;
    result.reserve(raw.size());
    for (const char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-') {
            result += c;
        } else {
            result += '_';
        }
    }
    // Collapse repeated underscores.
    std::string collapsed;
    collapsed.reserve(result.size());
    bool prevUnderscore = false;
    for (const char c : result) {
        if (c == '_' && prevUnderscore) continue;
        collapsed += c;
        prevUnderscore = (c == '_');
    }
    return collapsed;
}

// ============================================================
// WritePacketFile()
// ============================================================

void CaptureWriter::WritePacketFile(const std::filesystem::path& filepath,
                                     const std::string&            header,
                                     const Packet&                 packet) const
{
    std::ofstream file(filepath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error(
            "CaptureWriter: failed to open: " + filepath.string());
    }

    file << "========================================\n"
         << header
         << "========================================\n\n"
         << packet.rawPayload
         << "\n";
}

} // namespace NetDiscovery
