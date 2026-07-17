#include "services/KnowledgeStore.h"
#include <iostream>
#include <sstream>
#include <chrono>

namespace NetDiscovery {

KnowledgeStore::KnowledgeStore(std::unique_ptr<IKnowledgeStore> backend)
    : m_backend(std::move(backend))
{
}

void KnowledgeStore::Initialize() {
    if (m_backend) {
        m_backend->Initialize();
    }
}

void KnowledgeStore::ResolveKnownNetwork(const NetworkFingerprint& network) {
    m_currentNetwork = network;
    m_entities.clear();

    if (!m_backend) return;

    std::vector<std::string> rawDataList = m_backend->LoadAllEntities(network.CalculateId());
    for (const auto& raw : rawDataList) {
        try {
            KnowledgeEntity entity = DeserializeEntity(raw);
            m_entities[entity.persistentId] = entity;
        } catch (const std::exception& e) {
            std::cerr << "[KnowledgeStore] Failed to deserialize entity: " << e.what() << "\n";
        }
    }
}

void KnowledgeStore::UpdateFromDiscovery(const LogicalDevice& liveDevice) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // Map LogicalDevice -> KnowledgeEntity
    // If it exists in memory, merge it. If not, create it.
    
    // Simple lookup by identity (LogicalDevice::id is transient, but for this basic implementation we assume it's stable enough to map to persistentId if no other matching logic exists. In a full implementation, we'd do a deep evidence match.)
    std::string entityId = liveDevice.id;

    if (m_entities.find(entityId) == m_entities.end()) {
        // New Entity
        KnowledgeEntity newEntity;
        newEntity.persistentId = entityId;
        newEntity.lastObservedIdentity = liveDevice.id;
        newEntity.displayName = liveDevice.displayName;
        newEntity.aliases.systemAliases.push_back(liveDevice.displayName);
        newEntity.primaryClass = liveDevice.primaryClass;
        newEntity.roles = liveDevice.roles;
        newEntity.capabilities = liveDevice.capabilities;
        newEntity.endpoints = liveDevice.endpoints;
        newEntity.firstDiscovered = now;
        newEntity.lastSeen = now;
        
        for (const auto& candidate : liveDevice.controllerCandidates) {
            if (!candidate.isRejected) {
                newEntity.compatibleControllers.push_back(candidate.name);
            }
        }
        
        AddJournalEntry(newEntity, JournalEventType::Discovered, "Newly discovered on network.");
        m_entities[entityId] = newEntity;
    } else {
        // Merge Existing Entity
        KnowledgeEntity& existing = m_entities[entityId];
        existing.lastObservedIdentity = liveDevice.id;
        existing.lastSeen = now;
        
        // Live overrides persisted (optimization)
        if (!liveDevice.displayName.empty()) {
            existing.displayName = liveDevice.displayName;
        }
        existing.primaryClass = liveDevice.primaryClass;
        existing.roles = liveDevice.roles; // Overwrite
        
        MergeCapabilities(existing, liveDevice.capabilities);
        MergeEndpoints(existing, liveDevice.endpoints);
        
        AddJournalEntry(existing, JournalEventType::Validated, "Validated via active discovery.");
    }

    PersistEntity(m_entities[entityId]);
}

void KnowledgeStore::ArchiveEntity(const std::string& entityId) {
    if (m_entities.find(entityId) != m_entities.end()) {
        AddJournalEntry(m_entities[entityId], JournalEventType::Archived, "User archived entity.");
        PersistEntity(m_entities[entityId]);
        // We do NOT delete it from backend. It just stays archived.
    }
}

void KnowledgeStore::AppendCommunicationRecord(const std::string& entityId, const CommunicationRecord& record) {
    if (m_entities.find(entityId) != m_entities.end()) {
        KnowledgeEntity& entity = m_entities[entityId];
        entity.commHistory.push_back(record);
        
        JournalEventType evType = (record.status == ExecutionStatus::Success) ? JournalEventType::CommSucceeded : JournalEventType::CommFailed;
        AddJournalEntry(entity, evType, "Transport: " + record.transportName);
        
        PersistEntity(entity);
    }
}

KnowledgeConfidence KnowledgeStore::ComputeConfidence(const KnowledgeEntity& entity) const {
    KnowledgeConfidence conf;
    conf.score = 50; // Base score
    
    // Check if there's any communication history
    if (!entity.commHistory.empty()) {
        const auto& lastComm = entity.commHistory.back();
        if (lastComm.status == ExecutionStatus::Success) {
            conf.score += 30;
            conf.computedState = KnowledgeState::Validated;
        } else {
            conf.score -= 20;
            conf.computedState = KnowledgeState::Stale;
        }
    }
    
    // Check aging
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    long long ageMs = now - entity.lastSeen;
    
    if (ageMs > 86400000) { // Older than 1 day
        conf.score -= 40;
        conf.computedState = KnowledgeState::Stale;
    }
    
    if (conf.score < 0) conf.score = 0;
    if (conf.score > 100) conf.score = 100;
    
    return conf;
}

std::vector<KnowledgeEntity>& KnowledgeStore::GetLoadedEntities() {
    // This isn't efficient, but sufficient for Phase 5.5 placeholder
    static std::vector<KnowledgeEntity> temp;
    temp.clear();
    for (auto& kv : m_entities) {
        temp.push_back(kv.second);
    }
    return temp; // Warning: returns static vector reference. Refactor for thread safety later.
}

// -------------------------------------------------------------------------
// Internal Merge Logic
// -------------------------------------------------------------------------

void KnowledgeStore::MergeEndpoints(KnowledgeEntity& existing, const std::vector<ProtocolEndpoint>& liveEndpoints) {
    for (const auto& liveEp : liveEndpoints) {
        bool found = false;
        for (auto& existEp : existing.endpoints) {
            // Very simplified endpoint matching
            if (existEp.ip == liveEp.ip) {
                // Update it
                existEp = liveEp; 
                found = true;
                break;
            }
        }
        if (!found) {
            existing.endpoints.push_back(liveEp);
        }
    }
}

void KnowledgeStore::MergeCapabilities(KnowledgeEntity& existing, const std::vector<Capability>& liveCaps) {
    for (const auto& liveCap : liveCaps) {
        bool found = false;
        for (const auto& existCap : existing.capabilities) {
            if (existCap == liveCap) {
                found = true;
                break;
            }
        }
        if (!found) {
            existing.capabilities.push_back(liveCap);
        }
    }
}

void KnowledgeStore::AddJournalEntry(KnowledgeEntity& entity, JournalEventType type, const std::string& description) {
    JournalEntry entry;
    entry.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    entry.type = type;
    entry.description = description;
    entity.journal.push_back(entry);
}

// -------------------------------------------------------------------------
// Serialization (Lightweight Key-Value approach to avoid external libs)
// -------------------------------------------------------------------------

std::string KnowledgeStore::SerializeEntity(const KnowledgeEntity& entity) const {
    std::ostringstream oss;
    oss << "SCHEMA_VERSION=" << entity.schemaVersion << "\n";
    oss << "PERSISTENT_ID=" << entity.persistentId << "\n";
    oss << "DISPLAY_NAME=" << entity.displayName << "\n";
    oss << "FIRST_DISCOVERED=" << entity.firstDiscovered << "\n";
    oss << "LAST_SEEN=" << entity.lastSeen << "\n";
    
    // Arrays as comma-separated or pipe-separated lists
    if (!entity.compatibleControllers.empty()) {
        oss << "CONTROLLERS=";
        for (size_t i = 0; i < entity.compatibleControllers.size(); ++i) {
            oss << entity.compatibleControllers[i] << (i + 1 == entity.compatibleControllers.size() ? "" : ",");
        }
        oss << "\n";
    }

    if (!entity.capabilities.empty()) {
        oss << "CAPABILITIES=";
        for (size_t i = 0; i < entity.capabilities.size(); ++i) {
            oss << static_cast<int>(entity.capabilities[i]) << (i + 1 == entity.capabilities.size() ? "" : ",");
        }
        oss << "\n";
    }

    if (!entity.roles.empty()) {
        oss << "ROLES=";
        for (size_t i = 0; i < entity.roles.size(); ++i) {
            oss << static_cast<int>(entity.roles[i]) << (i + 1 == entity.roles.size() ? "" : ",");
        }
        oss << "\n";
    }

    // Endpoints (we save multiple ENDPOINT lines: IP|LocationUrl|ApplicationUrl)
    for (const auto& ep : entity.endpoints) {
        oss << "ENDPOINT=" << ep.ip << "|" << ep.serverHeader << "|" << ep.uuid;
        if (ep.evidence.upnp.has_value()) {
            if (!ep.evidence.upnp->applicationUrl.empty()) {
                std::cout << "[Metadata] KnowledgeStore serializing Application-URL: " << ep.evidence.upnp->applicationUrl << "\n";
            }
            oss << "|" << ep.evidence.upnp->locationUrl << "|" << ep.evidence.upnp->applicationUrl;
        }
        oss << "\n";
    }

    return oss.str();
}

KnowledgeEntity KnowledgeStore::DeserializeEntity(const std::string& data) const {
    KnowledgeEntity entity;
    std::istringstream iss(data);
    std::string line;
    while (std::getline(iss, line)) {
        auto eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (key == "PERSISTENT_ID") entity.persistentId = val;
            else if (key == "DISPLAY_NAME") entity.displayName = val;
            else if (key == "FIRST_DISCOVERED") entity.firstDiscovered = std::stoll(val);
            else if (key == "LAST_SEEN") entity.lastSeen = std::stoll(val);
            else if (key == "CONTROLLERS") {
                std::istringstream css(val);
                std::string c;
                while (std::getline(css, c, ',')) {
                    if (!c.empty()) entity.compatibleControllers.push_back(c);
                }
            }
            else if (key == "CAPABILITIES") {
                std::istringstream css(val);
                std::string c;
                while (std::getline(css, c, ',')) {
                    if (!c.empty()) entity.capabilities.push_back(static_cast<Capability>(std::stoi(c)));
                }
            }
            else if (key == "ROLES") {
                std::istringstream css(val);
                std::string c;
                while (std::getline(css, c, ',')) {
                    if (!c.empty()) entity.roles.push_back(static_cast<DeviceRole>(std::stoi(c)));
                }
            }
            else if (key == "ENDPOINT") {
                // Parse: IP|ServerHeader|UUID|LocationUrl|ApplicationUrl
                std::istringstream ess(val);
                std::string token;
                std::vector<std::string> parts;
                while (std::getline(ess, token, '|')) {
                    parts.push_back(token);
                }
                if (parts.size() >= 3) {
                    ProtocolEndpoint ep;
                    ep.ip = parts[0];
                    ep.serverHeader = parts[1];
                    ep.uuid = parts[2];
                    if (parts.size() >= 5) { // Has UPnP evidence
                        UPnPEvidence upnp;
                        upnp.locationUrl = parts[3];
                        upnp.applicationUrl = parts[4];
                        if (!upnp.applicationUrl.empty()) {
                            std::cout << "[Metadata] KnowledgeStore deserialized Application-URL: " << upnp.applicationUrl << "\n";
                        }
                        ep.evidence.upnp = std::move(upnp);
                    }
                    entity.endpoints.push_back(ep);
                }
            }
        }
    }
    return entity;
}

void KnowledgeStore::PersistEntity(const KnowledgeEntity& entity) {
    if (m_backend && !m_currentNetwork.CalculateId().empty()) {
        m_backend->SaveEntityData(m_currentNetwork.CalculateId(), entity.persistentId, SerializeEntity(entity));
    }
}

} // namespace NetDiscovery
