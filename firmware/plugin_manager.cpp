#include "plugin_manager.h"
#include "json_helpers.h"

PluginManager pluginManager;

namespace {

/**
 * @brief Simple JSON string parser for plugin metadata
 */
String extractJsonString(const String& json, const char* key) noexcept {
    String search = String("\"") + key + "\":\"";
    int start = json.indexOf(search);
    if (start < 0) return "";
    start += search.length();
    int end = json.indexOf('"', start);
    if (end < 0) return "";
    return json.substring(start, end);
}

bool extractJsonBool(const String& json, const char* key, bool defaultVal) noexcept {
    String search = String("\"") + key + "\":";
    int start = json.indexOf(search);
    if (start < 0) return defaultVal;
    start += search.length();
    if (json.substring(start, start + 4) == "true") return true;
    if (json.substring(start, start + 5) == "false") return false;
    return defaultVal;
}

unsigned long extractJsonUnsignedLong(const String& json, const char* key, unsigned long defaultVal) noexcept {
    String search = String("\"") + key + "\":";
    int start = json.indexOf(search);
    if (start < 0) return defaultVal;
    start += search.length();
    unsigned long val = 0;
    while (start < static_cast<int>(json.length()) && json[start] >= '0' && json[start] <= '9') {
        val = val * 10 + static_cast<unsigned long>(json[start] - '0');
        ++start;
    }
    return val;
}

uint32_t extractJsonUint32(const String& json, const char* key, uint32_t defaultVal) noexcept {
    return static_cast<uint32_t>(extractJsonUnsignedLong(json, key, static_cast<unsigned long>(defaultVal)));
}

uint8_t extractJsonUint8(const String& json, const char* key, uint8_t defaultVal) noexcept {
    return static_cast<uint8_t>(extractJsonUnsignedLong(json, key, static_cast<unsigned long>(defaultVal)));
}

int findMatchingBrace(const String& json, int openPos) noexcept {
    if (openPos < 0 || openPos >= static_cast<int>(json.length()) || json[openPos] != '{') return -1;
    int depth = 0;
    for (int i = openPos; i < static_cast<int>(json.length()); ++i) {
        if (json[i] == '{') ++depth;
        else if (json[i] == '}') {
            --depth;
            if (depth == 0) return i;
        }
    }
    return -1;
}

} // namespace

PluginManager::PluginManager() noexcept
    : m_initialized(false), m_dirty(false) {
}

PluginManager::~PluginManager() noexcept {
    if (m_dirty) saveState();
}

bool PluginManager::initialize() noexcept {
    if (m_initialized) {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    discoverPlugins();
    loadState();
    m_initialized = true;

    Logger::info(kLogCategory, "Initialized (%u plugins, %u enabled)",
        m_plugins.size(), enabledCount());
    return true;
}

void PluginManager::update() noexcept {
    // Periodic refresh is handled on demand via discoverPlugins()
}

size_t PluginManager::discoverPlugins() noexcept {
    m_plugins.clear();
    scanDirectory();

    Logger::info(kLogCategory, "Discovered %u plugins", m_plugins.size());
    return m_plugins.size();
}

size_t PluginManager::enabledCount() const noexcept {
    size_t count = 0;
    for (const auto& p : m_plugins) {
        if (p.enabled) count++;
    }
    return count;
}

bool PluginManager::loadPlugin(const String& pluginId) noexcept {
    String path = String(PLUGINS_PATH) + "/" + pluginId + "/" + PLUGIN_METADATA_FILE;

    String content;
    StorageStatus status = storageManager.readFile(path.c_str(), content, StorageType::SD_CARD);
    if (status != StorageStatus::SUCCESS) {
        Logger::warning(kLogCategory, "Plugin '%s' metadata not found", pluginId.c_str());
        return false;
    }

    PluginMetadata meta;
    if (!parsePluginJson(content, meta)) {
        Logger::error(kLogCategory, "Failed to parse plugin '%s' metadata", pluginId.c_str());
        return false;
    }

    // Check if already loaded, update if so
    for (auto& p : m_plugins) {
        if (p.id == pluginId) {
            p = meta;
            return true;
        }
    }

    m_plugins.push_back(meta);
    return true;
}

bool PluginManager::enablePlugin(const String& pluginId) noexcept {
    for (auto& p : m_plugins) {
        if (p.id == pluginId) {
            if (!p.enabled) {
                p.enabled = true;
                m_dirty = true;
                Logger::info(kLogCategory, "Plugin '%s' enabled", pluginId.c_str());
            }
            return true;
        }
    }
    Logger::warning(kLogCategory, "Plugin '%s' not found", pluginId.c_str());
    return false;
}

bool PluginManager::disablePlugin(const String& pluginId) noexcept {
    for (auto& p : m_plugins) {
        if (p.id == pluginId) {
            if (p.enabled) {
                p.enabled = false;
                m_dirty = true;
                Logger::info(kLogCategory, "Plugin '%s' disabled", pluginId.c_str());
            }
            return true;
        }
    }
    Logger::warning(kLogCategory, "Plugin '%s' not found", pluginId.c_str());
    return false;
}

PluginMetadata PluginManager::getPlugin(const String& pluginId) const noexcept {
    for (const auto& p : m_plugins) {
        if (p.id == pluginId) return p;
    }
    return PluginMetadata();
}

const std::vector<PluginMetadata>& PluginManager::getAllPlugins() const noexcept {
    return m_plugins;
}

bool PluginManager::isInitialized() const noexcept {
    return m_initialized;
}

bool PluginManager::registerPlugin(const PluginMetadata& plugin) noexcept {
    if (findPlugin(plugin.id) != m_plugins.size()) {
        Logger::warning(kLogCategory, "Plugin '%s' already registered", plugin.id.c_str());
        return false;
    }
    if (m_plugins.size() >= kMaxPlugins) {
        Logger::warning(kLogCategory, "Max plugins reached (%u)", static_cast<unsigned>(kMaxPlugins));
        return false;
    }
    m_plugins.push_back(plugin);
    m_dirty = true;
    Logger::info(kLogCategory, "Plugin '%s' registered", plugin.id.c_str());
    return true;
}

bool PluginManager::unregisterPlugin(const String& pluginId) noexcept {
    size_t idx = findPlugin(pluginId);
    if (idx >= m_plugins.size()) {
        Logger::warning(kLogCategory, "Plugin '%s' not found", pluginId.c_str());
        return false;
    }
    m_plugins.erase(m_plugins.begin() + static_cast<ptrdiff_t>(idx));
    m_dirty = true;
    Logger::info(kLogCategory, "Plugin '%s' unregistered", pluginId.c_str());
    return true;
}

bool PluginManager::updatePlugin(const String& pluginId, const PluginMetadata& updates) noexcept {
    size_t idx = findPlugin(pluginId);
    if (idx >= m_plugins.size()) {
        Logger::warning(kLogCategory, "Plugin '%s' not found", pluginId.c_str());
        return false;
    }
    m_plugins[idx] = updates;
    m_plugins[idx].id = pluginId;
    m_dirty = true;
    Logger::info(kLogCategory, "Plugin '%s' updated", pluginId.c_str());
    return true;
}

PluginMetadata PluginManager::getPluginInfo(const String& pluginId) const noexcept {
    return getPlugin(pluginId);
}

std::vector<PluginMetadata> PluginManager::searchPlugins(const String& query) const noexcept {
    std::vector<PluginMetadata> results;
    String lowerQuery = query;
    lowerQuery.toLowerCase();
    for (const auto& p : m_plugins) {
        String lowerName = p.name; lowerName.toLowerCase();
        String lowerDesc = p.description; lowerDesc.toLowerCase();
        String lowerAuthor = p.author; lowerAuthor.toLowerCase();
        String lowerTags = p.tags; lowerTags.toLowerCase();
        String lowerId = p.id; lowerId.toLowerCase();
        String lowerCategory = p.category; lowerCategory.toLowerCase();
        if (lowerName.indexOf(lowerQuery) >= 0 ||
            lowerDesc.indexOf(lowerQuery) >= 0 ||
            lowerAuthor.indexOf(lowerQuery) >= 0 ||
            lowerTags.indexOf(lowerQuery) >= 0 ||
            lowerId.indexOf(lowerQuery) >= 0 ||
            lowerCategory.indexOf(lowerQuery) >= 0) {
            results.push_back(p);
        }
    }
    return results;
}

std::vector<PluginMetadata> PluginManager::getPluginsByCategory(const String& category) const noexcept {
    std::vector<PluginMetadata> results;
    for (const auto& p : m_plugins) {
        if (p.category == category) {
            results.push_back(p);
        }
    }
    return results;
}

bool PluginManager::isPluginEnabled(const String& pluginId) const noexcept {
    for (const auto& p : m_plugins) {
        if (p.id == pluginId) return p.enabled;
    }
    return false;
}

size_t PluginManager::findPlugin(const String& id) const noexcept {
    for (size_t i = 0; i < m_plugins.size(); ++i) {
        if (m_plugins[i].id == id) return i;
    }
    return m_plugins.size();
}

bool PluginManager::saveState() noexcept {
    String json;
    json.reserve(2048);
    json += "{\"plugins\":[";
    for (size_t i = 0; i < m_plugins.size(); ++i) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"id\":\"" + escapeJson(m_plugins[i].id) + "\",";
        json += "\"name\":\"" + escapeJson(m_plugins[i].name) + "\",";
        json += "\"version\":\"" + escapeJson(m_plugins[i].version) + "\",";
        json += "\"author\":\"" + escapeJson(m_plugins[i].author) + "\",";
        json += "\"description\":\"" + escapeJson(m_plugins[i].description) + "\",";
        json += "\"category\":\"" + escapeJson(m_plugins[i].category) + "\",";
        json += "\"tags\":\"" + escapeJson(m_plugins[i].tags) + "\",";
        json += "\"dependencies\":\"" + escapeJson(m_plugins[i].dependencies) + "\",";
        json += "\"icon\":\"" + escapeJson(m_plugins[i].icon) + "\",";
        json += "\"required_firmware\":\"" + escapeJson(m_plugins[i].requiredFirmware) + "\",";
        json += "\"path\":\"" + escapeJson(m_plugins[i].path) + "\",";
        json += "\"source_url\":\"" + escapeJson(m_plugins[i].sourceUrl) + "\",";
        json += "\"checksum\":\"" + escapeJson(m_plugins[i].checksum) + "\",";
        json += "\"enabled\":" + String(m_plugins[i].enabled ? "true" : "false") + ",";
        json += "\"install_date\":" + String(m_plugins[i].installDate) + ",";
        json += "\"download_count\":" + String(m_plugins[i].downloadCount) + ",";
        json += "\"rating\":" + String(m_plugins[i].rating) + ",";
        json += "\"last_updated\":" + String(m_plugins[i].lastUpdated);
        json += "}";
    }
    json += "]}";

    StorageStatus status = storageManager.writeFile(kMarketplacePath, json, StorageType::SPIFFS);
    if (status == StorageStatus::SUCCESS) {
        m_dirty = false;
        return true;
    }
    Logger::error(kLogCategory, "Failed to save marketplace state");
    return false;
}

bool PluginManager::loadState() noexcept {
    String content;
    StorageStatus status = storageManager.readFile(kMarketplacePath, content, StorageType::SPIFFS);
    if (status != StorageStatus::SUCCESS) return false;

    m_plugins.clear();

    int pos = 0;
    while (true) {
        int objStart = content.indexOf('{', pos);
        if (objStart < 0) break;
        int objEnd = findMatchingBrace(content, objStart);
        if (objEnd < 0) break;

        String objStr = content.substring(objStart, objEnd + 1);

        PluginMetadata meta;
        meta.id = extractJsonString(objStr, "id");
        if (meta.id.isEmpty()) { pos = objEnd + 1; continue; }

        meta.name = extractJsonString(objStr, "name");
        meta.version = extractJsonString(objStr, "version");
        meta.author = extractJsonString(objStr, "author");
        meta.description = extractJsonString(objStr, "description");
        meta.category = extractJsonString(objStr, "category");
        meta.tags = extractJsonString(objStr, "tags");
        meta.dependencies = extractJsonString(objStr, "dependencies");
        meta.icon = extractJsonString(objStr, "icon");
        meta.requiredFirmware = extractJsonString(objStr, "required_firmware");
        meta.path = extractJsonString(objStr, "path");
        meta.sourceUrl = extractJsonString(objStr, "source_url");
        meta.checksum = extractJsonString(objStr, "checksum");
        meta.enabled = extractJsonBool(objStr, "enabled", true);
        meta.installDate = extractJsonUnsignedLong(objStr, "install_date", 0);
        meta.downloadCount = extractJsonUint32(objStr, "download_count", 0);
        meta.rating = extractJsonUint8(objStr, "rating", 0);
        meta.lastUpdated = extractJsonUnsignedLong(objStr, "last_updated", 0);

        if (m_plugins.size() < kMaxPlugins) {
            m_plugins.push_back(meta);
        }

        pos = objEnd + 1;
    }
    return true;
}

bool PluginManager::parsePluginJson(const String& json, PluginMetadata& meta) noexcept {
    meta.id = extractJsonString(json, "id");
    meta.name = extractJsonString(json, "name");
    meta.version = extractJsonString(json, "version");
    meta.author = extractJsonString(json, "author");
    meta.description = extractJsonString(json, "description");
    meta.icon = extractJsonString(json, "icon");
    meta.requiredFirmware = extractJsonString(json, "required_firmware");
    meta.enabled = extractJsonBool(json, "enabled", true);

    if (meta.id.isEmpty()) return false;

    meta.path = String(PLUGINS_PATH) + "/" + meta.id;
    return true;
}

bool PluginManager::scanDirectory() noexcept {
    // SD card directory scanning is delegated to storageManager
    // For now, we load individual known plugins on demand
    // A full directory scan would require storageManager to support listing
    return true;
}
