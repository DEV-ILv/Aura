#include "memory_manager.h"
#include "json_helpers.h"

MemoryManager memoryManager;

// ============================================================================
// Anonymous Namespace - Internal Helpers
// ============================================================================

namespace {

/**
 * @brief Convert MemoryCategory to C-string
 */
constexpr const char* categoryToString(MemoryCategory cat) noexcept {
    switch (cat) {
        case MemoryCategory::USER:         return "USER";
        case MemoryCategory::PREFERENCE:   return "PREFERENCE";
        case MemoryCategory::REMINDER:     return "REMINDER";
        case MemoryCategory::PROJECT:      return "PROJECT";
        case MemoryCategory::CONVERSATION: return "CONVERSATION";
        case MemoryCategory::FACT:         return "FACT";
        case MemoryCategory::LOCATION:     return "LOCATION";
        case MemoryCategory::GOAL:         return "GOAL";
        case MemoryCategory::CUSTOM:       return "CUSTOM";
        default:                           return "UNKNOWN";
    }
}

/**
 * @brief Convert C-string to MemoryCategory
 */
MemoryCategory stringToCategory(const String& str) noexcept {
    if (str == "USER")         return MemoryCategory::USER;
    if (str == "PREFERENCE")   return MemoryCategory::PREFERENCE;
    if (str == "REMINDER")     return MemoryCategory::REMINDER;
    if (str == "PROJECT")      return MemoryCategory::PROJECT;
    if (str == "CONVERSATION") return MemoryCategory::CONVERSATION;
    if (str == "FACT")         return MemoryCategory::FACT;
    if (str == "LOCATION")     return MemoryCategory::LOCATION;
    if (str == "GOAL")         return MemoryCategory::GOAL;
    return MemoryCategory::CUSTOM;
}

/**
 * @brief Extract string value from JSON by key
 */
String extractStr(const String& json, const char* key) noexcept {
    String search = String("\"") + key + "\":\"";
    int start = json.indexOf(search);
    if (start < 0) return "";
    start += search.length();
    int end = json.indexOf('"', start);
    if (end < 0) return "";
    return json.substring(start, end);
}

}  // namespace

// ============================================================================
// Constructor / Destructor
// ============================================================================

MemoryManager::MemoryManager() noexcept
    : m_initialized(false)
    , m_dirty(false)
    , m_maxEntries(kDefaultMaxEntries)
    , m_lastIdCounter(0)
    , m_lastMaintenance(0)
    , m_lastAgeRun(0) {
}

MemoryManager::~MemoryManager() noexcept {
    if (m_initialized && m_dirty) {
        save();
    }
}

// ============================================================================
// Public API - Lifecycle
// ============================================================================

bool MemoryManager::initialize() noexcept {
    if (m_initialized) {
        LOG_WARNING(kLogCategory, "Already initialized");
        return true;
    }

    if (!storageManager.isHealthy()) {
        LOG_ERROR(kLogCategory, "StorageManager not healthy");
        return false;
    }

    if (!load()) {
        LOG_WARNING(kLogCategory, "No saved memories found, starting fresh");
    }

    loadRevisions();

    m_initialized = true;
    LOG_INFO(kLogCategory, "Initialized (%u entries, %u revisions)",
        m_entries.size(), m_revisions.size());
    return true;
}

void MemoryManager::run() noexcept {
    update();
}

void MemoryManager::update() noexcept {
    if (!m_initialized) return;

    static unsigned long lastSave = 0;
    const unsigned long now = millis();

    if (m_dirty && (now - lastSave > 5000)) {
        lastSave = now;
        if (save()) {
            m_dirty = false;
        }
    }

    runAging();
    runMaintenance();
}

// ============================================================================
// Memory CRUD
// ============================================================================

String MemoryManager::remember(
    MemoryCategory category,
    const String& key,
    const String& value,
    uint8_t priority,
    bool persistent) noexcept {

    if (!m_initialized) {
        LOG_ERROR(kLogCategory, "Not initialized");
        return "";
    }

    if (key.isEmpty()) {
        LOG_ERROR(kLogCategory, "Cannot remember with empty key");
        return "";
    }

    evictIfNeeded();

    MemoryEntry entry(category, key, value, priority, persistent);
    entry.id = generateId();
    entry.timestamp = millis();
    entry.lastAccessed = entry.timestamp;
    entry.confidence = 1.0f;

    m_entries.push_back(entry);
    m_dirty = true;

    LOG_INFO(kLogCategory, "Remembered [%s] %s = %s",
        categoryToString(category), key.c_str(), value.c_str());

    return entry.id;
}

bool MemoryManager::forget(const String& id) noexcept {
    if (!m_initialized || id.isEmpty()) return false;

    const size_t idx = findEntry(id);
    if (idx == SIZE_MAX) {
        LOG_WARNING(kLogCategory, "Entry not found: %s", id.c_str());
        return false;
    }

    LOG_INFO(kLogCategory, "Forgot [%s] %s",
        categoryToString(m_entries[idx].category), m_entries[idx].key.c_str());

    m_entries.erase(m_entries.begin() + static_cast<ptrdiff_t>(idx));

    // Remove associated revisions
    for (auto it = m_revisions.begin(); it != m_revisions.end(); ) {
        if (it->memoryId == id) {
            it = m_revisions.erase(it);
        } else {
            ++it;
        }
    }

    m_dirty = true;
    return true;
}

bool MemoryManager::updateMemory(
    const String& id,
    const String& newKey,
    const String& newValue,
    uint8_t newPriority) noexcept {

    if (!m_initialized || id.isEmpty()) return false;

    const size_t idx = findEntry(id);
    if (idx == SIZE_MAX) return false;

    MemoryEntry& entry = m_entries[idx];

    // Save revision before modifying
    if (!newValue.isEmpty() && newValue != entry.value) {
        saveRevision(id, entry.value);
    }
    if (!newKey.isEmpty() && newKey != entry.key) {
        saveRevision(id, entry.key + "=" + entry.value);
    }

    if (!newKey.isEmpty())   entry.key = newKey;
    if (!newValue.isEmpty()) entry.value = newValue;
    if (newPriority != 0xFF) entry.priority = newPriority;

    entry.lastAccessed = millis();
    m_dirty = true;

    LOG_INFO(kLogCategory, "Updated [%s] %s",
        categoryToString(entry.category), entry.key.c_str());

    return true;
}

// ============================================================================
// Query
// ============================================================================

std::vector<MemoryEntry> MemoryManager::search(
    const SearchCriteria& criteria) const noexcept {

    std::vector<MemoryEntry> results;

    for (const auto& entry : m_entries) {
        bool match = true;

        if (criteria.filterCategory && entry.category != criteria.category) {
            match = false;
        }

        if (match && criteria.filterPriority && entry.priority < criteria.minPriority) {
            match = false;
        }

        if (match && criteria.favoritesOnly && !entry.favorite) {
            match = false;
        }

        if (match && criteria.filterImportance && entry.importance < criteria.minImportance) {
            match = false;
        }

        if (match && !criteria.includeArchived && entry.archived) {
            match = false;
        }

        if (match && criteria.filterConfidence && entry.confidence < criteria.minConfidence) {
            match = false;
        }

        if (match && !criteria.contextFilter.isEmpty()) {
            if (entry.contextName != criteria.contextFilter) {
                match = false;
            }
        }

        if (match && !criteria.keyPattern.isEmpty()) {
            if (entry.key.indexOf(criteria.keyPattern) < 0 &&
                entry.value.indexOf(criteria.keyPattern) < 0 &&
                entry.summary.indexOf(criteria.keyPattern) < 0) {
                match = false;
            }
        }

        if (match) {
            results.push_back(entry);
            if (criteria.maxResults > 0 && results.size() >= criteria.maxResults) {
                break;
            }
        }
    }

    return results;
}

std::vector<MemoryEntry> MemoryManager::search(
    const String& keyPattern,
    bool exactMatch) const noexcept {

    std::vector<MemoryEntry> results;

    for (const auto& entry : m_entries) {
        bool match;
        if (exactMatch) {
            match = (entry.key == keyPattern);
        } else {
            match = (entry.key.indexOf(keyPattern) >= 0 ||
                     entry.value.indexOf(keyPattern) >= 0);
        }

        if (match) {
            results.push_back(entry);
        }
    }

    return results;
}

bool MemoryManager::exists(const String& key) const noexcept {
    if (key.isEmpty()) return false;

    for (const auto& entry : m_entries) {
        if (entry.key == key) return true;
    }
    return false;
}

MemoryEntry MemoryManager::get(const String& id) const noexcept {
    if (id.isEmpty()) return MemoryEntry();

    const size_t idx = findEntry(id);
    if (idx == SIZE_MAX) return MemoryEntry();

    return m_entries[idx];
}

const std::vector<MemoryEntry>& MemoryManager::getAll() const noexcept {
    return m_entries;
}

std::vector<MemoryEntry> MemoryManager::getByCategory(
    MemoryCategory category) const noexcept {

    std::vector<MemoryEntry> results;

    for (const auto& entry : m_entries) {
        if (entry.category == category) {
            results.push_back(entry);
        }
    }

    return results;
}

// ============================================================================
// Metadata Operations
// ============================================================================

bool MemoryManager::markFavorite(const String& id, bool favorite) noexcept {
    if (!m_initialized || id.isEmpty()) return false;

    const size_t idx = findEntry(id);
    if (idx == SIZE_MAX) return false;

    m_entries[idx].favorite = favorite;
    m_dirty = true;
    return true;
}

bool MemoryManager::setPriority(const String& id, uint8_t priority) noexcept {
    if (!m_initialized || id.isEmpty()) return false;

    const size_t idx = findEntry(id);
    if (idx == SIZE_MAX) return false;

    m_entries[idx].priority = priority;
    m_dirty = true;
    return true;
}

bool MemoryManager::incrementAccess(const String& id) noexcept {
    if (!m_initialized || id.isEmpty()) return false;

    const size_t idx = findEntry(id);
    if (idx == SIZE_MAX) return false;

    MemoryEntry& entry = m_entries[idx];
    entry.accessCount++;
    entry.lastAccessed = millis();
    return true;
}

// ============================================================================
// Bulk Operations
// ============================================================================

void MemoryManager::clear() noexcept {
    if (!m_initialized) return;

    m_entries.clear();
    m_dirty = true;
    LOG_INFO(kLogCategory, "All memories cleared");
}

void MemoryManager::clearCategory(MemoryCategory category) noexcept {
    if (!m_initialized) return;

    size_t before = m_entries.size();

    for (auto it = m_entries.begin(); it != m_entries.end(); ) {
        if (it->category == category) {
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }

    if (m_entries.size() != before) {
        m_dirty = true;
        LOG_INFO(kLogCategory, "Category %s cleared (%u entries)",
            categoryToString(category), before - m_entries.size());
    }
}

// ============================================================================
// Serialization
// ============================================================================

String MemoryManager::exportJson() const noexcept {
    String json;
    json.reserve(4096);
    json += "[";

    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (i > 0) json += ",";
        const MemoryEntry& e = m_entries[i];

        json += "{";
        json += "\"id\":\""       + escapeJson(e.id) + "\",";
        json += "\"category\":\"";
        json += categoryToString(e.category);
        json += "\",";
        json += "\"key\":\""      + escapeJson(e.key) + "\",";
        json += "\"value\":\""    + escapeJson(e.value) + "\",";
        json += "\"timestamp\":"  + String(e.timestamp) + ",";
        json += "\"priority\":"   + String(e.priority) + ",";
        json += "\"accessCount\":" + String(e.accessCount) + ",";
        json += "\"lastAccessed\":" + String(e.lastAccessed) + ",";
        json += "\"persistent\":";
        json += (e.persistent ? "true" : "false");
        json += ",";
        json += "\"favorite\":";
        json += (e.favorite ? "true" : "false");
        json += ",";
        json += "\"expiry\":" + String(e.expiryTime) + ",";
        json += "\"importance\":" + String(e.importance) + ",";
        json += "\"pinned\":";
        json += (e.pinned ? "true" : "false");
        json += ",";
        json += "\"archived\":";
        json += (e.archived ? "true" : "false");
        json += ",";
        json += "\"tags\":\"" + escapeJson(e.tags) + "\",";
        json += "\"confidence\":" + String(e.confidence, 2) + ",";
        json += "\"source\":\"" + escapeJson(e.source) + "\",";
        json += "\"summary\":\"" + escapeJson(e.summary) + "\",";
        json += "\"contextName\":\"" + escapeJson(e.contextName) + "\"";
        json += "}";
    }

    json += "]";
    return json;
}

size_t MemoryManager::importJson(const String& json) noexcept {
    if (!m_initialized || json.isEmpty()) return 0;

    size_t count = 0;
    size_t pos = 0;

    // Find opening bracket
    pos = json.indexOf('[');
    if (pos == SIZE_MAX) return 0;
    pos++;

    while (pos < json.length()) {
        // Find opening brace
        pos = json.indexOf('{', pos);
        if (pos == SIZE_MAX) break;

        MemoryEntry entry = parseEntry(json, pos);
        if (!entry.id.isEmpty()) {
            entry.id = generateId();
            entry.timestamp = millis();
            entry.lastAccessed = entry.timestamp;

            evictIfNeeded();
            m_entries.push_back(entry);
            count++;
        }

        // Find comma or closing bracket
        pos = json.indexOf('}', pos);
        if (pos == SIZE_MAX) break;
        pos++;
    }

    if (count > 0) {
        m_dirty = true;
        LOG_INFO(kLogCategory, "Imported %u memories", count);
    }

    return count;
}

// ============================================================================
// Persistence
// ============================================================================

bool MemoryManager::save() noexcept {
    if (!m_initialized) return false;

    const String json = exportJson();
    const StorageStatus status = storageManager.writeFile(
        kStoragePath, json, StorageType::SPIFFS);

    if (status == StorageStatus::SUCCESS) {
        LOG_INFO(kLogCategory, "Saved %u memories (%u bytes)",
            m_entries.size(), json.length());
        saveRevisions();
        m_dirty = false;
        return true;
    }

    LOG_ERROR(kLogCategory, "Save failed: %d", static_cast<int>(status));
    return false;
}

bool MemoryManager::load() noexcept {
    if (!m_initialized) {
        if (!storageManager.isHealthy()) return false;
    }

    if (!storageManager.fileExists(kStoragePath, StorageType::SPIFFS)) {
        return false;
    }

    String content;
    const StorageStatus status = storageManager.readFile(
        kStoragePath, content, StorageType::SPIFFS);

    if (status != StorageStatus::SUCCESS || content.isEmpty()) {
        LOG_ERROR(kLogCategory, "Load failed: %d", static_cast<int>(status));
        return false;
    }

    m_entries.clear();

    size_t pos = 0;
    pos = content.indexOf('[');
    if (pos == SIZE_MAX) return false;
    pos++;

    while (pos < content.length()) {
        pos = content.indexOf('{', pos);
        if (pos == SIZE_MAX) break;

        MemoryEntry entry = parseEntry(content, pos);
        if (!entry.id.isEmpty()) {
            if (m_entries.size() < m_maxEntries) {
                m_entries.push_back(entry);
            }
        }

        pos = content.indexOf('}', pos);
        if (pos == SIZE_MAX) break;
        pos++;
    }

    LOG_INFO(kLogCategory, "Loaded %u memories", m_entries.size());
    m_dirty = false;

    loadConversations();
    return true;
}

// ============================================================================
// Status
// ============================================================================

bool MemoryManager::isInitialized() const noexcept {
    return m_initialized;
}

size_t MemoryManager::memoryCount() const noexcept {
    return m_entries.size();
}

// ============================================================================
// Private Helpers
// ============================================================================

String MemoryManager::generateId() noexcept {
    const unsigned long now = millis();
    m_lastIdCounter++;

    // Mix millis, counter, and ESP chip ID for uniqueness
    const uint32_t mix = static_cast<uint32_t>(now) ^
                         static_cast<uint32_t>(m_lastIdCounter << 16) ^
                         static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFFF);

    String id;
    id.reserve(kGeneratedIdLength);

    uint32_t val = mix;
    for (size_t i = 0; i < kGeneratedIdLength; ++i) {
        id += kHexChars[val & 0x0F];
        val = (val >> 2) ^ (val << 3) ^ (m_lastIdCounter + i);
    }

    return id;
}

size_t MemoryManager::findEntry(const String& id) const noexcept {
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id) return i;
    }
    return SIZE_MAX;
}

void MemoryManager::evictIfNeeded() noexcept {
    if (m_entries.size() < m_maxEntries) return;

    sortByEvictionPriority();

    size_t evicted = 0;
    for (auto it = m_entries.begin(); it != m_entries.end(); ) {
        if (evicted >= kEvictionBatchSize) break;

        if (!it->persistent && !it->pinned && it->priority < kEvictionMinPriority) {
            LOG_DEBUG(kLogCategory, "Evicting [%s] %s (prio=%u, access=%u)",
                categoryToString(it->category), it->key.c_str(),
                it->priority, it->accessCount);
            it = m_entries.erase(it);
            evicted++;
        } else {
            ++it;
        }
    }

    if (evicted > 0) {
        LOG_INFO(kLogCategory, "Evicted %u memories (total: %u)", evicted, m_entries.size());
        m_dirty = true;
    }
}

void MemoryManager::sortByEvictionPriority() noexcept {
    // Sort so lowest-priority, least-recently-used entries come first
    for (size_t i = 0; i < m_entries.size(); ++i) {
        for (size_t j = i + 1; j < m_entries.size(); ++j) {
            const MemoryEntry& a = m_entries[i];
            const MemoryEntry& b = m_entries[j];

            bool evictA = !a.persistent && !a.pinned && a.priority < kEvictionMinPriority;
            bool evictB = !b.persistent && !b.pinned && b.priority < kEvictionMinPriority;

            if (evictA && !evictB) continue;
            if (!evictA && evictB) {
                std::swap(m_entries[i], m_entries[j]);
                continue;
            }
            if (!evictA && !evictB) continue;

            // Both evictable: lower priority first, then older lastAccessed
            if (a.priority > b.priority) {
                std::swap(m_entries[i], m_entries[j]);
            } else if (a.priority == b.priority &&
                       a.lastAccessed > b.lastAccessed) {
                std::swap(m_entries[i], m_entries[j]);
            }
        }
    }
}

MemoryEntry MemoryManager::parseEntry(const String& json, size_t& pos) const noexcept {
    MemoryEntry entry;

    // Find field pairs within braces
    while (pos < json.length()) {
        // Skip whitespace and commas
        while (pos < json.length() &&
               (json[pos] == ' ' || json[pos] == ',' || json[pos] == '\n' ||
                json[pos] == '\r' || json[pos] == '\t')) {
            pos++;
        }

        if (pos >= json.length() || json[pos] == '}') {
            pos++;
            break;
        }

        // Read key
        if (json[pos] != '"') { pos++; continue; }
        pos++;
        String key;
        while (pos < json.length() && json[pos] != '"') {
            if (json[pos] == '\\') { pos++; if (pos < json.length()) key += json[pos]; }
            else { key += json[pos]; }
            pos++;
        }
        if (pos < json.length()) pos++; // skip closing quote

        // Skip colon
        while (pos < json.length() && json[pos] != ':') pos++;
        if (pos < json.length()) pos++;

        // Skip whitespace
        while (pos < json.length() && json[pos] == ' ') pos++;

        // Read value
        if (pos >= json.length()) break;

        if (json[pos] == '"') {
            // String value
            pos++;
            String val;
            while (pos < json.length() && json[pos] != '"') {
                if (json[pos] == '\\') { pos++; if (pos < json.length()) val += json[pos]; }
                else { val += json[pos]; }
                pos++;
            }
            if (pos < json.length()) pos++;

            if (key == "id")         entry.id = val;
            else if (key == "category") entry.category = stringToCategory(val);
            else if (key == "key")   entry.key = val;
            else if (key == "value") entry.value = val;
            else if (key == "tags")  entry.tags = val;
            else if (key == "source")  entry.source = val;
            else if (key == "summary") entry.summary = val;
            else if (key == "contextName") entry.contextName = val;
        } else {
            // Numeric or boolean value
            String val;
            while (pos < json.length() && json[pos] != ',' &&
                   json[pos] != '}' && json[pos] != ' ') {
                val += json[pos];
                pos++;
            }

            if (key == "timestamp")    entry.timestamp = val.toInt();
            else if (key == "priority") entry.priority = static_cast<uint8_t>(val.toInt());
            else if (key == "accessCount") entry.accessCount = static_cast<uint16_t>(val.toInt());
            else if (key == "lastAccessed") entry.lastAccessed = val.toInt();
            else if (key == "persistent") entry.persistent = (val == "true");
            else if (key == "favorite") entry.favorite = (val == "true");
            else if (key == "expiry") entry.expiryTime = val.toInt();
            else if (key == "importance") entry.importance = static_cast<uint8_t>(val.toInt());
            else if (key == "pinned")  entry.pinned = (val == "true");
            else if (key == "archived") entry.archived = (val == "true");
            else if (key == "confidence") entry.confidence = val.toFloat();
        }
    }

    return entry;
}

String MemoryManager::unescapeJson(const String& escaped, size_t& pos) const noexcept {
    String result;

    while (pos < escaped.length()) {
        if (escaped[pos] == '"') {
            pos++;
            break;
        }
        if (escaped[pos] == '\\') {
            pos++;
            if (pos >= escaped.length()) break;
            switch (escaped[pos]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case 'u': {
                    if (pos + 4 < escaped.length()) {
                        char hex[5] = {escaped[pos+1], escaped[pos+2],
                                       escaped[pos+3], escaped[pos+4], 0};
                        result += static_cast<char>(strtol(hex, nullptr, 16));
                        pos += 4;
                    }
                    break;
                }
                default: result += escaped[pos]; break;
            }
            pos++;
        } else {
            result += escaped[pos];
            pos++;
        }
    }

    return result;
}

// ============================================================================
// Conversation History
// ============================================================================

bool MemoryManager::saveConversation(const ConversationRecord& record) noexcept {
    if (!m_initialized) return false;

    // Update existing or add new
    for (auto& c : m_conversations) {
        if (c.id == record.id) {
            c = record;
            saveConversations();
            return true;
        }
    }

    m_conversations.push_back(record);
    while (m_conversations.size() > kMaxConversations) {
        m_conversations.erase(m_conversations.begin());
    }

    saveConversations();
    LOG_INFO(kLogCategory, "Conversation '%s' saved", record.title.c_str());
    return true;
}

const std::vector<ConversationRecord>& MemoryManager::getConversations() const noexcept {
    return m_conversations;
}

std::vector<ConversationRecord> MemoryManager::searchConversations(const String& query) const noexcept {
    std::vector<ConversationRecord> results;
    if (query.isEmpty()) {
        results = m_conversations;
        return results;
    }

    String lowerQuery = query;
    lowerQuery.toLowerCase();

    for (const auto& c : m_conversations) {
        String lowerTitle = c.title;
        lowerTitle.toLowerCase();
        String lowerSummary = c.summary;
        lowerSummary.toLowerCase();
        String lowerImportant = c.importantMessages;
        lowerImportant.toLowerCase();

        if (lowerTitle.indexOf(lowerQuery) >= 0 ||
            lowerSummary.indexOf(lowerQuery) >= 0 ||
            lowerImportant.indexOf(lowerQuery) >= 0) {
            results.push_back(c);
        }
    }

    return results;
}

bool MemoryManager::deleteConversation(const String& conversationId) noexcept {
    for (auto it = m_conversations.begin(); it != m_conversations.end(); ++it) {
        if (it->id == conversationId) {
            m_conversations.erase(it);
            saveConversations();
            LOG_INFO(kLogCategory, "Conversation '%s' deleted", conversationId.c_str());
            return true;
        }
    }
    return false;
}

bool MemoryManager::archiveConversation(const String& conversationId) noexcept {
    for (auto& c : m_conversations) {
        if (c.id == conversationId) {
            c.archived = true;
            saveConversations();
            return true;
        }
    }
    return false;
}

bool MemoryManager::favoriteConversation(const String& conversationId, bool favorite) noexcept {
    for (auto& c : m_conversations) {
        if (c.id == conversationId) {
            c.favorite = favorite;
            saveConversations();
            return true;
        }
    }
    return false;
}

size_t MemoryManager::conversationCount() const noexcept {
    return m_conversations.size();
}

// ============================================================================
// Smart Memory Ranking
// ============================================================================

std::vector<MemoryEntry> MemoryManager::getRankedMemories(size_t topN) const noexcept {
    std::vector<MemoryEntry> ranked = m_entries;

    // Sort by relevance score descending
    for (size_t i = 0; i < ranked.size(); ++i) {
        for (size_t j = i + 1; j < ranked.size(); ++j) {
            if (ranked[j].computeRelevanceScore() > ranked[i].computeRelevanceScore()) {
                std::swap(ranked[i], ranked[j]);
            }
        }
    }

    if (topN > 0 && ranked.size() > topN) {
        ranked.resize(topN);
    }

    return ranked;
}

void MemoryManager::updateImportanceScores() noexcept {
    if (!m_initialized) return;

    for (auto& entry : m_entries) {
        unsigned long age = millis() - entry.timestamp;
        uint8_t newImportance = entry.priority;

        if (entry.favorite) newImportance += 40;
        if (entry.persistent) newImportance += 20;
        if (entry.accessCount > 10) newImportance += 15;
        else if (entry.accessCount > 5) newImportance += 10;
        else if (entry.accessCount > 2) newImportance += 5;

        // Recent entries get a boost
        if (age < 3600000UL) newImportance += 20;           // < 1 hour
        else if (age < 86400000UL) newImportance += 10;     // < 1 day
        else if (age < 604800000UL) newImportance += 5;     // < 1 week

        entry.importance = newImportance;
    }

    LOG_DEBUG(kLogCategory, "Importance scores updated for %u entries", m_entries.size());
}

// ============================================================================
// Confidence & Source
// ============================================================================

bool MemoryManager::setConfidence(const String& id, float confidence) noexcept {
    if (!m_initialized || id.isEmpty()) return false;
    const size_t idx = findEntry(id);
    if (idx == SIZE_MAX) return false;
    m_entries[idx].confidence = constrain(confidence, 0.0f, 1.0f);
    m_dirty = true;
    return true;
}

bool MemoryManager::setSource(const String& id, const String& source) noexcept {
    if (!m_initialized || id.isEmpty()) return false;
    const size_t idx = findEntry(id);
    if (idx == SIZE_MAX) return false;
    m_entries[idx].source = source;
    m_dirty = true;
    return true;
}

bool MemoryManager::setSummary(const String& id, const String& summary) noexcept {
    if (!m_initialized || id.isEmpty()) return false;
    const size_t idx = findEntry(id);
    if (idx == SIZE_MAX) return false;
    m_entries[idx].summary = summary;
    m_dirty = true;
    return true;
}

std::vector<MemoryEntry> MemoryManager::getByContext(const String& contextName) const noexcept {
    std::vector<MemoryEntry> results;
    for (const auto& e : m_entries) {
        if (e.contextName == contextName) results.push_back(e);
    }
    return results;
}

std::vector<MemoryEntry> MemoryManager::getLowConfidence(float maxConfidence) const noexcept {
    std::vector<MemoryEntry> results;
    for (const auto& e : m_entries) {
        if (e.confidence <= maxConfidence) results.push_back(e);
    }
    return results;
}

// ============================================================================
// Memory Importance Engine
// ============================================================================

bool MemoryManager::setPin(const String& id, bool pinned) noexcept {
    if (!m_initialized || id.isEmpty()) return false;

    const size_t idx = findEntry(id);
    if (idx == SIZE_MAX) return false;

    m_entries[idx].pinned = pinned;
    if (pinned) {
        m_entries[idx].persistent = true;
    }
    m_dirty = true;
    LOG_INFO(kLogCategory, "Memory %s %s", id.c_str(), pinned ? "pinned" : "unpinned");
    return true;
}

bool MemoryManager::setArchive(const String& id, bool archived) noexcept {
    if (!m_initialized || id.isEmpty()) return false;

    const size_t idx = findEntry(id);
    if (idx == SIZE_MAX) return false;

    m_entries[idx].archived = archived;
    m_dirty = true;
    LOG_INFO(kLogCategory, "Memory %s %s", id.c_str(), archived ? "archived" : "unarchived");
    return true;
}

std::vector<MemoryEntry> MemoryManager::getArchived() const noexcept {
    std::vector<MemoryEntry> results;
    for (const auto& e : m_entries) {
        if (e.archived) results.push_back(e);
    }
    return results;
}

std::vector<MemoryEntry> MemoryManager::getPinned() const noexcept {
    std::vector<MemoryEntry> results;
    for (const auto& e : m_entries) {
        if (e.pinned) results.push_back(e);
    }
    return results;
}

void MemoryManager::runAging() noexcept {
    if (!m_initialized) return;

    const unsigned long now = millis();
    if (now - m_lastAgeRun < kAutoAgeIntervalMs) return;
    m_lastAgeRun = now;

    for (auto& entry : m_entries) {
        if (entry.pinned) continue;

        unsigned long age = now - entry.lastAccessed;
        unsigned long ageHours = age / 3600000UL;
        if (ageHours == 0) continue;

        float decay = 1.0f;
        for (unsigned long h = 0; h < ageHours && h < 168; ++h) {
            decay *= kAgeDecayFactorPerHour;
        }

        uint8_t aged = static_cast<uint8_t>(static_cast<float>(entry.importance) * decay);

        if (aged < kMinImportanceAfterAge && entry.importance > kMinImportanceAfterAge) {
            aged = kMinImportanceAfterAge;
        }

        if (entry.favorite && aged < 30) aged = 30;
        if (entry.persistent && aged < 20) aged = 20;

        entry.importance = aged;

        // Age confidence for non-pinned entries
        if (!entry.pinned && entry.confidence < 1.0f) {
            float confidenceDecay = 1.0f - (0.05f * static_cast<float>(ageHours) / 24.0f);
            if (confidenceDecay < 0.5f) confidenceDecay = 0.5f;
            entry.confidence *= confidenceDecay;
        }
    }

    updateImportanceScores();
}

// ============================================================================
// Versioned Memory
// ============================================================================

String MemoryManager::saveRevision(const String& memoryId, const String& previousValue) noexcept {
    if (!m_initialized || memoryId.isEmpty()) return "";

    // Limit revisions per memory
    size_t revCount = 0;
    for (const auto& r : m_revisions) {
        if (r.memoryId == memoryId) revCount++;
    }

    while (revCount >= kMaxRevisionsPerMemory) {
        // Remove oldest revision for this memory
        unsigned long oldest = 0xFFFFFFFFUL;
        auto oldestIt = m_revisions.end();
        for (auto it = m_revisions.begin(); it != m_revisions.end(); ++it) {
            if (it->memoryId == memoryId && it->timestamp < oldest) {
                oldest = it->timestamp;
                oldestIt = it;
            }
        }
        if (oldestIt != m_revisions.end()) {
            m_revisions.erase(oldestIt);
        }
        revCount--;
    }

    MemoryRevision rev;
    rev.id = generateId();
    rev.memoryId = memoryId;
    rev.previousValue = previousValue;
    rev.timestamp = millis();

    // Snapshot current tags
    size_t idx = findEntry(memoryId);
    if (idx != SIZE_MAX) {
        rev.tags = m_entries[idx].tags;
    }

    m_revisions.push_back(rev);
    LOG_DEBUG(kLogCategory, "Revision saved for memory %s: %s", memoryId.c_str(), rev.id.c_str());
    return rev.id;
}

std::vector<MemoryRevision> MemoryManager::getRevisions(const String& memoryId) const noexcept {
    std::vector<MemoryRevision> results;
    for (const auto& r : m_revisions) {
        if (r.memoryId == memoryId) {
            results.push_back(r);
        }
    }
    return results;
}

bool MemoryManager::restoreRevision(const String& revisionId) noexcept {
    if (!m_initialized || revisionId.isEmpty()) return false;

    const size_t revIdx = findRevision(revisionId);
    if (revIdx == SIZE_MAX) return false;

    const MemoryRevision& rev = m_revisions[revIdx];
    const size_t entryIdx = findEntry(rev.memoryId);
    if (entryIdx == SIZE_MAX) {
        LOG_WARNING(kLogCategory, "Revision's parent memory %s not found", rev.memoryId.c_str());
        return false;
    }

    MemoryEntry& entry = m_entries[entryIdx];

    // Save current value as a revision before overwriting
    saveRevision(rev.memoryId, entry.value);

    // Restore from revision
    entry.value = rev.previousValue;
    entry.lastAccessed = millis();
    m_dirty = true;

    LOG_INFO(kLogCategory, "Memory %s restored to revision %s", rev.memoryId.c_str(), revisionId.c_str());
    return true;
}

String MemoryManager::compareRevisions(const String& revisionId1, const String& revisionId2) const noexcept {
    String result;

    const size_t idx1 = findRevision(revisionId1);
    const size_t idx2 = findRevision(revisionId2);

    if (idx1 == SIZE_MAX || idx2 == SIZE_MAX) {
        return "Error: one or both revision IDs not found";
    }

    const MemoryRevision& r1 = m_revisions[idx1];
    const MemoryRevision& r2 = m_revisions[idx2];

    if (r1.memoryId != r2.memoryId) {
        return "Error: revisions belong to different memories";
    }

    result += "Revision " + r1.id + " (t=" + String(r1.timestamp) + "):\n";
    result += "  Value: \"" + r1.previousValue + "\"\n";
    result += "  Tags: \"" + r1.tags + "\"\n\n";
    result += "Revision " + r2.id + " (t=" + String(r2.timestamp) + "):\n";
    result += "  Value: \"" + r2.previousValue + "\"\n";
    result += "  Tags: \"" + r2.tags + "\"\n\n";

    if (r1.previousValue == r2.previousValue) {
        result += "No difference in value.\n";
    } else {
        result += "Value changed from \"" + r1.previousValue + "\" to \"" + r2.previousValue + "\".\n";
    }

    return result;
}

// ============================================================================
// Revision Persistence
// ============================================================================

void MemoryManager::saveRevisions() noexcept {
    if (!m_initialized) return;

    String json;
    json.reserve(4096);
    json += "{\"revisions\":[";
    for (size_t i = 0; i < m_revisions.size(); ++i) {
        if (i > 0) json += ",";
        const MemoryRevision& r = m_revisions[i];
        json += "{";
        json += "\"id\":\"" + escapeJson(r.id) + "\",";
        json += "\"memoryId\":\"" + escapeJson(r.memoryId) + "\",";
        json += "\"previousValue\":\"" + escapeJson(r.previousValue) + "\",";
        json += "\"timestamp\":" + String(r.timestamp) + ",";
        json += "\"tags\":\"" + escapeJson(r.tags) + "\"";
        json += "}";
    }
    json += "]}";

    StorageStatus status = storageManager.writeFile(kRevisionsPath, json, StorageType::SPIFFS);
    if (status != StorageStatus::SUCCESS) {
        LOG_ERROR(kLogCategory, "Failed to save revisions");
    } else {
        LOG_DEBUG(kLogCategory, "Saved %u revisions", m_revisions.size());
    }
}

void MemoryManager::loadRevisions() noexcept {
    if (!storageManager.fileExists(kRevisionsPath, StorageType::SPIFFS)) return;

    String content;
    StorageStatus status = storageManager.readFile(kRevisionsPath, content, StorageType::SPIFFS);
    if (status != StorageStatus::SUCCESS || content.isEmpty()) return;

    m_revisions.clear();

    size_t pos = 0;
    pos = content.indexOf('[');
    if (pos == SIZE_MAX) return;
    pos++;

    while (pos < content.length()) {
        pos = content.indexOf('{', pos);
        if (pos == SIZE_MAX) break;

        MemoryRevision rev;
        // Parse fields
        while (pos < content.length()) {
            while (pos < content.length() &&
                   (content[pos] == ' ' || content[pos] == ',' || content[pos] == '\n' ||
                    content[pos] == '\r' || content[pos] == '\t')) {
                pos++;
            }
            if (pos >= content.length() || content[pos] == '}') {
                pos++;
                break;
            }
            if (content[pos] != '"') { pos++; continue; }
            pos++;
            String key;
            while (pos < content.length() && content[pos] != '"') {
                if (content[pos] == '\\') { pos++; if (pos < content.length()) key += content[pos]; }
                else { key += content[pos]; }
                pos++;
            }
            if (pos < content.length()) pos++;
            while (pos < content.length() && content[pos] != ':') pos++;
            if (pos < content.length()) pos++;
            while (pos < content.length() && content[pos] == ' ') pos++;
            if (pos >= content.length()) break;

            if (content[pos] == '"') {
                pos++;
                String val;
                while (pos < content.length() && content[pos] != '"') {
                    if (content[pos] == '\\') { pos++; if (pos < content.length()) val += content[pos]; }
                    else { val += content[pos]; }
                    pos++;
                }
                if (pos < content.length()) pos++;
                if (key == "id")             rev.id = val;
                else if (key == "memoryId")  rev.memoryId = val;
                else if (key == "previousValue") rev.previousValue = val;
                else if (key == "tags")      rev.tags = val;
            } else {
                String val;
                while (pos < content.length() && content[pos] != ',' &&
                       content[pos] != '}' && content[pos] != ' ') {
                    val += content[pos];
                    pos++;
                }
                if (key == "timestamp") rev.timestamp = val.toInt();
            }
        }

        if (!rev.id.isEmpty()) {
            m_revisions.push_back(rev);
        }
    }

    LOG_INFO(kLogCategory, "Loaded %u revisions", m_revisions.size());
}

size_t MemoryManager::findRevision(const String& revisionId) const noexcept {
    for (size_t i = 0; i < m_revisions.size(); ++i) {
        if (m_revisions[i].id == revisionId) return i;
    }
    return SIZE_MAX;
}

// ============================================================================
// Semantic Search
// ============================================================================

std::vector<MemoryEntry> MemoryManager::semanticSearch(const String& query, size_t maxResults) const noexcept {
    std::vector<std::pair<float, MemoryEntry>> scored;

    // Extract words from query for simple token matching
    String lowerQuery = query;
    lowerQuery.toLowerCase();

    for (const auto& entry : m_entries) {
        float score = 0.0f;

        // Score by direct key/value match
        String lowerKey = entry.key;
        lowerKey.toLowerCase();
        String lowerValue = entry.value;
        lowerValue.toLowerCase();

        if (lowerKey.indexOf(lowerQuery) >= 0) score += 10.0f;
        if (lowerValue.indexOf(lowerQuery) >= 0) score += 8.0f;

        // Score by tag similarity
        if (!entry.tags.isEmpty()) {
            score += computeTagSimilarity(entry.tags, query) * 5.0f;
        }

        // Boost favorites and high importance
        if (entry.favorite) score += 3.0f;
        score += static_cast<float>(entry.importance) * 0.1f;

        if (score > 0.0f) {
            scored.push_back({score, entry});
        }
    }

    // Sort by score descending
    for (size_t i = 0; i < scored.size(); ++i) {
        for (size_t j = i + 1; j < scored.size(); ++j) {
            if (scored[j].first > scored[i].first) {
                std::swap(scored[i], scored[j]);
            }
        }
    }

    std::vector<MemoryEntry> results;
    size_t limit = (maxResults > 0) ? maxResults : scored.size();
    for (size_t i = 0; i < limit && i < scored.size(); ++i) {
        results.push_back(scored[i].second);
    }

    return results;
}

std::vector<MemoryEntry> MemoryManager::findRelated(const String& memoryId, size_t maxResults) const noexcept {
    size_t idx = findEntry(memoryId);
    if (idx == SIZE_MAX) return {};

    const MemoryEntry& source = m_entries[idx];
    std::vector<std::pair<float, MemoryEntry>> scored;

    for (const auto& entry : m_entries) {
        if (entry.id == memoryId) continue;

        float score = 0.0f;

        // Same category
        if (entry.category == source.category) score += 5.0f;

        // Tag overlap
        if (!source.tags.isEmpty() && !entry.tags.isEmpty()) {
            score += computeTagSimilarity(source.tags, entry.tags) * 5.0f;
        }

        // Key/value word overlap
        String sourceWords = source.key + " " + source.value;
        String entryWords = entry.key + " " + entry.value;
        sourceWords.toLowerCase();
        entryWords.toLowerCase();

        // Simple word overlap count
        int start = 0;
        while (start < (int)sourceWords.length()) {
            int space = sourceWords.indexOf(' ', start);
            String word;
            if (space < 0) {
                word = sourceWords.substring(start);
                start = sourceWords.length();
            } else {
                word = sourceWords.substring(start, space);
                start = space + 1;
            }
            if (word.length() > 2 && entryWords.indexOf(word) >= 0) {
                score += 1.0f;
            }
        }

        if (score > 0.0f) {
            scored.push_back({score, entry});
        }
    }

    for (size_t i = 0; i < scored.size(); ++i) {
        for (size_t j = i + 1; j < scored.size(); ++j) {
            if (scored[j].first > scored[i].first) {
                std::swap(scored[i], scored[j]);
            }
        }
    }

    std::vector<MemoryEntry> results;
    size_t limit = (maxResults > 0) ? maxResults : scored.size();
    for (size_t i = 0; i < limit && i < scored.size(); ++i) {
        results.push_back(scored[i].second);
    }

    return results;
}

// ============================================================================
// Automatic Maintenance
// ============================================================================

size_t MemoryManager::runMaintenance() noexcept {
    if (!m_initialized) return 0;

    unsigned long now = millis();
    if (now - m_lastMaintenance < kMaintenanceInterval) return 0;
    m_lastMaintenance = now;

    size_t totalCleaned = 0;

    totalCleaned += removeExpired();
    totalCleaned += mergeDuplicates();
    totalCleaned += semanticDedup();
    totalCleaned += consolidateFacts();
    updateImportanceScores();

    if (totalCleaned > 0) {
        m_dirty = true;
        LOG_INFO(kLogCategory, "Maintenance complete: %u entries cleaned", totalCleaned);
    }

    return totalCleaned;
}

size_t MemoryManager::removeExpired() noexcept {
    if (!m_initialized) return 0;

    size_t before = m_entries.size();
    unsigned long now = millis();

    for (auto it = m_entries.begin(); it != m_entries.end(); ) {
        if (!it->persistent && !it->pinned && it->expiryTime > 0 && now >= it->expiryTime) {
            LOG_DEBUG(kLogCategory, "Removing expired memory: %s", it->key.c_str());
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }

    size_t removed = before - m_entries.size();
    if (removed > 0) {
        m_dirty = true;
        LOG_INFO(kLogCategory, "Removed %u expired memories", removed);
    }

    return removed;
}

size_t MemoryManager::mergeDuplicates() noexcept {
    if (!m_initialized) return 0;

    size_t merged = 0;

    for (size_t i = 0; i < m_entries.size(); ++i) {
        for (size_t j = i + 1; j < m_entries.size(); ) {
            if (m_entries[i].key == m_entries[j].key &&
                m_entries[i].value == m_entries[j].value &&
                m_entries[i].category == m_entries[j].category) {

                // Merge metadata: keep higher priority, accumulate access
                if (m_entries[j].priority > m_entries[i].priority) {
                    m_entries[i].priority = m_entries[j].priority;
                }
                m_entries[i].accessCount += m_entries[j].accessCount;
                if (m_entries[j].lastAccessed > m_entries[i].lastAccessed) {
                    m_entries[i].lastAccessed = m_entries[j].lastAccessed;
                }
                if (m_entries[j].favorite) m_entries[i].favorite = true;
                if (m_entries[j].persistent) m_entries[i].persistent = true;

                m_entries.erase(m_entries.begin() + static_cast<ptrdiff_t>(j));
                merged++;
            } else {
                ++j;
            }
        }
    }

    if (merged > 0) {
        m_dirty = true;
        LOG_INFO(kLogCategory, "Merged %u duplicate memories", merged);
    }

    return merged;
}

size_t MemoryManager::semanticDedup(float threshold) noexcept {
    if (!m_initialized) return 0;

    size_t merged = 0;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        for (size_t j = i + 1; j < m_entries.size(); ) {
            if (m_entries[i].id == m_entries[j].id) { j++; continue; }

            float sim = 0.0f;
            if (m_entries[i].category == m_entries[j].category) {
                sim = computeKeySimilarity(m_entries[i].key, m_entries[j].key);
            }

            if (sim >= threshold) {
                if (m_entries[j].priority > m_entries[i].priority) {
                    m_entries[i].priority = m_entries[j].priority;
                }
                m_entries[i].accessCount += m_entries[j].accessCount;
                if (m_entries[j].lastAccessed > m_entries[i].lastAccessed) {
                    m_entries[i].lastAccessed = m_entries[j].lastAccessed;
                }
                if (m_entries[j].favorite) m_entries[i].favorite = true;
                if (m_entries[j].persistent) m_entries[i].persistent = true;
                if (m_entries[j].confidence > m_entries[i].confidence) {
                    m_entries[i].confidence = m_entries[j].confidence;
                }
                if (!m_entries[j].source.isEmpty() && m_entries[i].source.isEmpty()) {
                    m_entries[i].source = m_entries[j].source;
                }

                m_entries.erase(m_entries.begin() + static_cast<ptrdiff_t>(j));
                merged++;
            } else {
                ++j;
            }
        }
    }

    if (merged > 0) {
        m_dirty = true;
        LOG_INFO(kLogCategory, "Semantic dedup merged %u memories (threshold=%.2f)", merged, threshold);
    }
    return merged;
}

size_t MemoryManager::consolidateFacts() noexcept {
    if (!m_initialized) return 0;

    size_t updated = 0;
    for (auto& entry : m_entries) {
        if (entry.category != MemoryCategory::FACT) continue;

        String lowerKey = entry.key;
        lowerKey.toLowerCase();
        String lowerValue = entry.value;
        lowerValue.toLowerCase();

        for (const auto& other : m_entries) {
            if (other.id == entry.id) continue;
            if (other.category != MemoryCategory::FACT) continue;

            String otherKey = other.key;
            otherKey.toLowerCase();
            String otherValue = other.value;
            otherValue.toLowerCase();

            if (lowerKey == otherKey && lowerValue != otherValue) {
                if (other.confidence > entry.confidence) {
                    entry.value = other.value;
                    entry.confidence = (entry.confidence + other.confidence) / 2.0f;
                    updated++;
                }
            }
        }
    }

    if (updated > 0) {
        m_dirty = true;
        LOG_INFO(kLogCategory, "Consolidated %u facts", updated);
    }
    return updated;
}

float MemoryManager::computeKeySimilarity(const String& k1, const String& k2) const noexcept {
    if (k1.isEmpty() && k2.isEmpty()) return 1.0f;
    if (k1.isEmpty() || k2.isEmpty()) return 0.0f;

    String a = k1; a.toLowerCase(); a.trim();
    String b = k2; b.toLowerCase(); b.trim();

    if (a == b) return 1.0f;
    if (a.indexOf(b) >= 0 || b.indexOf(a) >= 0) return 0.9f;

    // Simple word overlap
    std::vector<String> wordsA, wordsB;
    int start = 0;
    while (start < (int)a.length()) {
        int space = a.indexOf(' ', start);
        if (space < 0) { wordsA.push_back(a.substring(start)); break; }
        wordsA.push_back(a.substring(start, space));
        start = space + 1;
    }
    start = 0;
    while (start < (int)b.length()) {
        int space = b.indexOf(' ', start);
        if (space < 0) { wordsB.push_back(b.substring(start)); break; }
        wordsB.push_back(b.substring(start, space));
        start = space + 1;
    }

    size_t matches = 0;
    for (const auto& wa : wordsA) {
        if (wa.length() <= 2) continue;
        for (const auto& wb : wordsB) {
            if (wb.length() <= 2) continue;
            if (wa == wb) { matches++; break; }
        }
    }
    size_t maxWords = (wordsA.size() > wordsB.size()) ? wordsA.size() : wordsB.size();
    return (maxWords > 0) ? (float)matches / (float)maxWords : 0.0f;
}

size_t MemoryManager::fuzzyFindKey(const String& keyPattern, float threshold) const noexcept {
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (computeKeySimilarity(m_entries[i].key, keyPattern) >= threshold) {
            return i;
        }
    }
    return SIZE_MAX;
}

float MemoryManager::computeTagSimilarity(const String& tags1, const String& tags2) const noexcept {
    if (tags1.isEmpty() || tags2.isEmpty()) return 0.0f;

    std::vector<String> tagList1, tagList2;
    int start = 0;
    while (start < (int)tags1.length()) {
        int comma = tags1.indexOf(',', start);
        if (comma < 0) {
            tagList1.push_back(tags1.substring(start));
            break;
        }
        tagList1.push_back(tags1.substring(start, comma));
        start = comma + 1;
    }

    start = 0;
    while (start < (int)tags2.length()) {
        int comma = tags2.indexOf(',', start);
        if (comma < 0) {
            tagList2.push_back(tags2.substring(start));
            break;
        }
        tagList2.push_back(tags2.substring(start, comma));
        start = comma + 1;
    }

    size_t matches = 0;
    for (const auto& t1 : tagList1) {
        String lt1 = t1; lt1.toLowerCase(); lt1.trim();
        for (const auto& t2 : tagList2) {
            String lt2 = t2; lt2.toLowerCase(); lt2.trim();
            if (lt1 == lt2) matches++;
        }
    }

    size_t total = tagList1.size() + tagList2.size();
    return (total > 0) ? (2.0f * static_cast<float>(matches) / static_cast<float>(total)) : 0.0f;
}

// ============================================================================
// Conversation Persistence
// ============================================================================

void MemoryManager::saveConversations() noexcept {
    if (!m_initialized) return;

    String json;
    json.reserve(4096);
    json += "{\"conversations\":[";
    for (size_t i = 0; i < m_conversations.size(); ++i) {
        if (i > 0) json += ",";
        const ConversationRecord& c = m_conversations[i];
        json += "{";
        json += "\"id\":\"" + escapeJson(c.id) + "\",";
        json += "\"title\":\"" + escapeJson(c.title) + "\",";
        json += "\"timestamp\":" + String(c.timestamp) + ",";
        json += "\"last_activity\":" + String(c.lastActivity) + ",";
        json += "\"summary\":\"" + escapeJson(c.summary) + "\",";
        json += "\"important\":\"" + escapeJson(c.importantMessages) + "\",";
        json += "\"messages\":" + String(c.messageCount) + ",";
        json += "\"archived\":" + String(c.archived ? "true" : "false") + ",";
        json += "\"favorite\":" + String(c.favorite ? "true" : "false");
        json += "}";
    }
    json += "]}";

    StorageStatus status = storageManager.writeFile(kConversationsPath, json, StorageType::SPIFFS);
    if (status != StorageStatus::SUCCESS) {
        LOG_ERROR(kLogCategory, "Failed to save conversations");
    }
}

void MemoryManager::loadConversations() noexcept {
    if (!storageManager.fileExists(kConversationsPath, StorageType::SPIFFS)) return;

    String content;
    StorageStatus status = storageManager.readFile(kConversationsPath, content, StorageType::SPIFFS);
    if (status != StorageStatus::SUCCESS || content.isEmpty()) return;

    m_conversations.clear();

    int pos = 0;
    while (true) {
        int start = content.indexOf('{', pos);
        if (start < 0) break;
        int end = content.indexOf('}', start);
        if (end < 0) break;

        String obj = content.substring(start, end + 1);
        ConversationRecord cr;
        cr.id = extractStr(obj, "id");
        cr.title = extractStr(obj, "title");
        cr.summary = extractStr(obj, "summary");
        cr.importantMessages = extractStr(obj, "important");

        if (!cr.id.isEmpty()) {
            m_conversations.push_back(cr);
        }
        pos = end + 1;
    }

    LOG_INFO(kLogCategory, "Loaded %u conversations", m_conversations.size());
}

