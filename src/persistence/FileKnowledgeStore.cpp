#include "FileKnowledgeStore.h"
#include <fstream>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace NetDiscovery {

FileKnowledgeStore::FileKnowledgeStore(const std::string& baseDir)
    : m_baseDir(baseDir)
{
}

void FileKnowledgeStore::Initialize() {
    EnsureDirectoryExists(m_baseDir);
}

void FileKnowledgeStore::SaveEntityData(const std::string& networkId, 
                                        const std::string& entityId, 
                                        const std::string& serializedData) {
    const std::string netDir = GetNetworkDir(networkId);
    if (!EnsureDirectoryExists(netDir)) {
        std::cerr << "[FileKnowledgeStore] Failed to create network directory: " << netDir << "\n";
        return;
    }

    const std::string filePath = GetEntityFilePath(networkId, entityId);
    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (out.is_open()) {
        out << serializedData;
        out.close();
    } else {
        std::cerr << "[FileKnowledgeStore] Failed to open file for writing: " << filePath << "\n";
    }
}

std::string FileKnowledgeStore::LoadEntityData(const std::string& networkId, 
                                               const std::string& entityId) {
    const std::string filePath = GetEntityFilePath(networkId, entityId);
    std::ifstream in(filePath, std::ios::binary);
    if (in.is_open()) {
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
    return "";
}

std::vector<std::string> FileKnowledgeStore::LoadAllEntities(const std::string& networkId) {
    std::vector<std::string> entities;
    const std::string netDir = GetNetworkDir(networkId);

#ifdef _WIN32
    std::string searchPath = netDir + "\\*.json";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string entityId = findData.cFileName;
                // Strip extension if it's .json
                if (entityId.size() > 5 && entityId.substr(entityId.size() - 5) == ".json") {
                    entityId = entityId.substr(0, entityId.size() - 5);
                }
                
                std::string data = LoadEntityData(networkId, entityId);
                if (!data.empty()) {
                    entities.push_back(std::move(data));
                }
            }
        } while (FindNextFileA(hFind, &findData) != 0);
        FindClose(hFind);
    }
#else
    // Placeholder for non-Windows (ESP32 won't use FileKnowledgeStore anyway)
#endif

    return entities;
}

void FileKnowledgeStore::DeleteEntityData(const std::string& networkId, 
                                          const std::string& entityId) {
    const std::string filePath = GetEntityFilePath(networkId, entityId);
#ifdef _WIN32
    DeleteFileA(filePath.c_str());
#else
    std::remove(filePath.c_str());
#endif
}

std::string FileKnowledgeStore::GetNetworkDir(const std::string& networkId) const {
    return m_baseDir + "\\" + networkId;
}

std::string FileKnowledgeStore::GetEntityFilePath(const std::string& networkId, const std::string& entityId) const {
    // Basic sanitization of entityId to avoid path traversal (replace slashes, colons)
    std::string safeId = entityId;
    for (char& c : safeId) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return GetNetworkDir(networkId) + "\\" + safeId + ".json";
}

bool FileKnowledgeStore::EnsureDirectoryExists(const std::string& path) const {
#ifdef _WIN32
    std::string::size_type pos = 0;
    do {
        pos = path.find_first_of("\\/", pos + 1);
        std::string sub = path.substr(0, pos);
        DWORD ftyp = GetFileAttributesA(sub.c_str());
        if (ftyp == INVALID_FILE_ATTRIBUTES) {
            CreateDirectoryA(sub.c_str(), NULL);
        }
    } while (pos != std::string::npos);
    
    DWORD ftyp = GetFileAttributesA(path.c_str());
    return (ftyp != INVALID_FILE_ATTRIBUTES && (ftyp & FILE_ATTRIBUTE_DIRECTORY));
#else
    return true; // ESP32 placeholder
#endif
}

} // namespace NetDiscovery
