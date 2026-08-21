#include "workspace_manager.h"
#include "json_helpers.h"
#include <algorithm>

WorkspaceManager workspaceManager;

WorkspaceManager::WorkspaceManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastIdCounter(0) {
}

WorkspaceManager::~WorkspaceManager() noexcept {
    if (m_dirty) save();
}

bool WorkspaceManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    storageManager.createDirectory(WORKSPACES_PATH, StorageType::SPIFFS);
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u workspaces)", m_workspaces.size());
    return true;
}

void WorkspaceManager::update() noexcept {
    if (!m_initialized) return;
    static unsigned long lastSave = 0;
    unsigned long now = millis();
    if (m_dirty && (now - lastSave > 5000)) { lastSave = now; if (save()) m_dirty = false; }
}

String WorkspaceManager::generateId() noexcept {
    unsigned long now = millis(); m_lastIdCounter++;
    uint32_t mix = static_cast<uint32_t>(now) ^ (m_lastIdCounter << 16) ^ (ESP.getEfuseMac() & 0xFFFFFFFF);
    String id; id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) { id += hex[val & 0x0F]; val = (val >> 2) ^ (val << 3) ^ (m_lastIdCounter + i); }
    return id;
}

String WorkspaceManager::createWorkspace(const String& name, const String& description) noexcept {
    if (!m_initialized || name.isEmpty()) return "";
    trimToMax();
    
    Workspace ws;
    ws.id = generateId();
    ws.name = name;
    ws.description = description;
    ws.createdAt = millis();
    ws.updatedAt = ws.createdAt;
    ws.active = false;
    ws.members.reserve(16);
    m_workspaces.push_back(ws);
    m_dirty = true;
    Logger::info(kLogCategory, "Created workspace: %s", name.c_str());
    return ws.id;
}

bool WorkspaceManager::deleteWorkspace(const String& id) noexcept {
    for (auto it = m_workspaces.begin(); it != m_workspaces.end(); ++it) {
        if (it->id == id) {
            m_workspaces.erase(it);
            m_dirty = true;
            return true;
        }
    }
    return false;
}

bool WorkspaceManager::updateWorkspace(const String& id, const String& name, const String& description) noexcept {
    for (auto& ws : m_workspaces) {
        if (ws.id != id) continue;
        if (!name.isEmpty()) ws.name = name;
        ws.description = description;
        ws.updatedAt = millis();
        m_dirty = true;
        return true;
    }
    return false;
}

bool WorkspaceManager::addMember(const String& workspaceId, const String& entityType, const String& entityId) noexcept {
    for (auto& ws : m_workspaces) {
        if (ws.id != workspaceId) continue;
        if (ws.members.size() >= kMaxMembers) return false;
        // Check for duplicate
        for (const auto& m : ws.members) {
            if (m.entityId == entityId && m.entityType == entityType) return true; // Already present
        }
        ws.members.push_back(WorkspaceMember(entityType, entityId));
        ws.updatedAt = millis();
        m_dirty = true;
        return true;
    }
    return false;
}

bool WorkspaceManager::removeMember(const String& workspaceId, const String& entityId) noexcept {
    for (auto& ws : m_workspaces) {
        if (ws.id != workspaceId) continue;
        for (auto it = ws.members.begin(); it != ws.members.end(); ++it) {
            if (it->entityId == entityId) {
                ws.members.erase(it);
                ws.updatedAt = millis();
                m_dirty = true;
                return true;
            }
        }
    }
    return false;
}

std::vector<WorkspaceMember> WorkspaceManager::getMembers(const String& workspaceId) const noexcept {
    for (const auto& ws : m_workspaces) {
        if (ws.id == workspaceId) return ws.members;
    }
    return {};
}

bool WorkspaceManager::activateWorkspace(const String& id) noexcept {
    for (auto& ws : m_workspaces) {
        ws.active = (ws.id == id);
    }
    m_dirty = true;
    for (const auto& ws : m_workspaces) {
        if (ws.id == id && ws.active) return true;
    }
    return false;
}

bool WorkspaceManager::deactivateWorkspace(const String& id) noexcept {
    for (auto& ws : m_workspaces) {
        if (ws.id == id) { ws.active = false; m_dirty = true; return true; }
    }
    return false;
}

const Workspace* WorkspaceManager::getActiveWorkspace() const noexcept {
    for (const auto& ws : m_workspaces) {
        if (ws.active) return &ws;
    }
    return nullptr;
}

const Workspace* WorkspaceManager::getWorkspace(const String& id) const noexcept {
    for (const auto& ws : m_workspaces) {
        if (ws.id == id) return &ws;
    }
    return nullptr;
}

std::vector<Workspace> WorkspaceManager::getAllWorkspaces() const noexcept {
    return m_workspaces;
}

std::vector<Workspace> WorkspaceManager::findWorkspacesByEntity(const String& entityType, const String& entityId) const noexcept {
    std::vector<Workspace> results;
    for (const auto& ws : m_workspaces) {
        for (const auto& m : ws.members) {
            if (m.entityType == entityType && m.entityId == entityId) {
                results.push_back(ws);
                break;
            }
        }
    }
    return results;
}

String WorkspaceManager::getWorkspacesJson() const noexcept {
    String json; json.reserve(4096);
    json += "{\"workspaces\":[";
    bool first = true;
    for (const auto& ws : m_workspaces) {
        if (!first) json += ",";
        first = false;
        json += serializeWorkspace(ws);
    }
    json += "]}";
    return json;
}

bool WorkspaceManager::save() noexcept {
    String path = String(WORKSPACES_PATH) + "/data.json";
    String j; j.reserve(8192);
    j += "{\"version\":1,\"workspaces\":[";
    for (size_t i = 0; i < m_workspaces.size(); ++i) {
        if (i > 0) j += ",";
        j += serializeWorkspace(m_workspaces[i]);
    }
    j += "]}";
    StorageStatus st = storageManager.writeFile(path.c_str(), j, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_dirty = false; return true; }
    return false;
}

bool WorkspaceManager::load() noexcept {
    String path = String(WORKSPACES_PATH) + "/data.json";
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_workspaces.clear();
    int pos = content.indexOf("\"workspaces\":[");
    if (pos < 0) return false;
    pos = content.indexOf('[', pos) + 1;
    while (pos < (int)content.length()) {
        int braceStart = content.indexOf('{', pos);
        if (braceStart < 0) break;
        int braceEnd = content.indexOf('}', braceStart);
        if (braceEnd < 0) break;
        String obj = content.substring(braceStart, braceEnd + 1);
        Workspace ws = deserializeWorkspace(obj);
        if (!ws.id.isEmpty()) m_workspaces.push_back(ws);
        pos = braceEnd + 1;
    }
    return true;
}

bool WorkspaceManager::isInitialized() const noexcept { return m_initialized; }

void WorkspaceManager::trimToMax() noexcept {
    while (m_workspaces.size() >= kMaxWorkspaces) {
        m_workspaces.erase(m_workspaces.begin());
        m_dirty = true;
    }
}

String WorkspaceManager::serializeMember(const WorkspaceMember& m) const noexcept {
    String j; j.reserve(64);
    j += "{";
    j += "\"type\":\"" + escapeJson(m.entityType) + "\",";
    j += "\"id\":\"" + escapeJson(m.entityId) + "\"";
    j += "}";
    return j;
}

String WorkspaceManager::serializeWorkspace(const Workspace& w) const noexcept {
    String j; j.reserve(512);
    j += "{";
    j += "\"id\":\"" + escapeJson(w.id) + "\",";
    j += "\"name\":\"" + escapeJson(w.name) + "\",";
    j += "\"desc\":\"" + escapeJson(w.description) + "\",";
    j += "\"created\":" + String(w.createdAt) + ",";
    j += "\"updated\":" + String(w.updatedAt) + ",";
    j += "\"active\":" + String(w.active ? "true" : "false") + ",";
    j += "\"members\":[";
    for (size_t i = 0; i < w.members.size(); ++i) {
        if (i > 0) j += ",";
        j += serializeMember(w.members[i]);
    }
    j += "]}";
    return j;
}

WorkspaceMember WorkspaceManager::deserializeMember(const String& json, int& pos) const noexcept {
    WorkspaceMember m;
    auto eS = [&](const char* key) -> String {
        String s = String("\"") + key + "\":\"";
        int start = json.indexOf(s, pos);
        if (start < 0) return "";
        start += s.length();
        int end = json.indexOf('"', start);
        return (end < 0) ? "" : json.substring(start, end);
    };
    m.entityType = eS("type");
    m.entityId = eS("id");
    return m;
}

Workspace WorkspaceManager::deserializeWorkspace(const String& json) const noexcept {
    Workspace w;
    auto eS = [&](const char* key) -> String {
        String s = String("\"") + key + "\":\"";
        int start = json.indexOf(s);
        if (start < 0) return "";
        start += s.length();
        int end = json.indexOf('"', start);
        return (end < 0) ? "" : json.substring(start, end);
    };
    auto eI = [&](const char* key, int def) -> int {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s);
        if (start < 0) return def;
        start += s.length();
        int end = start;
        while (end < (int)json.length() && json[end] >= '0' && json[end] <= '9') end++;
        return (end == start) ? def : json.substring(start, end).toInt();
    };
    auto eB = [&](const char* key, bool def) -> bool {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s);
        if (start < 0) return def;
        start += s.length();
        return json.substring(start).startsWith("true");
    };
    w.id = eS("id");
    w.name = eS("name");
    w.description = eS("desc");
    w.createdAt = static_cast<unsigned long>(eI("created", 0));
    w.updatedAt = static_cast<unsigned long>(eI("updated", 0));
    w.active = eB("active", false);
    
    // Parse members array
    int memStart = json.indexOf("\"members\":[");
    if (memStart >= 0) {
        int pos = json.indexOf('[', memStart) + 1;
        while (pos < (int)json.length()) {
            int braceStart = json.indexOf('{', pos);
            if (braceStart < 0) break;
            int braceEnd = json.indexOf('}', braceStart);
            if (braceEnd < 0) break;
            String obj = json.substring(braceStart, braceEnd + 1);
            w.members.push_back(deserializeMember(obj, pos));
            pos = braceEnd + 1;
            if (pos < (int)json.length() && json[pos] == ',') pos++;
        }
    }
    
    return w;
}
