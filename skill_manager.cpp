#include "skill_manager.h"
#include "json_helpers.h"

SkillManager skillManager;

namespace {

String extractStr(const String& json, const char* key) noexcept {
    String search = String("\"") + key + "\":\"";
    int start = json.indexOf(search);
    if (start < 0) return "";
    start += search.length();
    int end = json.indexOf('"', start);
    if (end < 0) return "";
    return json.substring(start, end);
}

int extractInt(const String& json, const char* key, int def) noexcept {
    String search = String("\"") + key + "\":";
    int start = json.indexOf(search);
    if (start < 0) return def;
    start += search.length();
    int end = start;
    while (end < (int)json.length() && json[end] >= '0' && json[end] <= '9') end++;
    if (end == start) return def;
    return json.substring(start, end).toInt();
}

bool extractBool(const String& json, const char* key, bool def) noexcept {
    String search = String("\"") + key + "\":";
    int start = json.indexOf(search);
    if (start < 0) return def;
    start += search.length();
    String val = json.substring(start, start + 5);
    if (val.startsWith("true")) return true;
    if (val.startsWith("false")) return false;
    return def;
}

} // namespace

SkillManager::SkillManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastIdCounter(0) {
}

SkillManager::~SkillManager() noexcept {
    if (m_dirty) save();
}

bool SkillManager::initialize() noexcept {
    if (m_initialized) {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    if (!storageManager.isHealthy()) {
        Logger::error(kLogCategory, "StorageManager not healthy");
        return false;
    }

    load();

    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u skills)", m_skills.size());
    return true;
}

void SkillManager::update() noexcept {
    if (!m_initialized) return;

    static unsigned long lastSave = 0;
    unsigned long now = millis();

    if (m_dirty && (now - lastSave > 5000)) {
        lastSave = now;
        if (save()) m_dirty = false;
    }
}

SkillEntry SkillManager::matchTrigger(const String& text) const noexcept {
    if (text.isEmpty()) return SkillEntry();

    String lowerText = text;
    lowerText.toLowerCase();

    SkillEntry best;
    int8_t bestPriority = -1;

    for (const auto& skill : m_skills) {
        if (!skill.enabled) continue;

        String trigger = skill.voiceTrigger;
        trigger.toLowerCase();

        if (lowerText.indexOf(trigger) >= 0) {
            if (static_cast<int8_t>(skill.priority) > bestPriority) {
                best = skill;
                bestPriority = static_cast<int8_t>(skill.priority);
            }
        }
    }

    return best;
}

void SkillManager::logExecution(const String& skillId, bool success) noexcept {
    for (auto& skill : m_skills) {
        if (skill.id == skillId) {
            skill.lastTriggered = millis();
            skill.triggerCount++;
            if (success) {
                Logger::info(kLogCategory, "Skill '%s' executed successfully", skill.name.c_str());
            } else {
                Logger::warning(kLogCategory, "Skill '%s' execution failed", skill.name.c_str());
            }
            m_dirty = true;
            return;
        }
    }
}

bool SkillManager::addSkill(const SkillEntry& skill) noexcept {
    if (m_skills.size() >= kMaxSkills || m_skills.size() >= kStudioMaxSkills) {
        Logger::warning(kLogCategory, "Maximum skills reached");
        return false;
    }

    SkillEntry entry = skill;
    entry.id = generateId();
    entry.createdAt = millis();

    m_skills.push_back(entry);
    m_dirty = true;

    Logger::info(kLogCategory, "Skill '%s' added", entry.name.c_str());
    return true;
}

bool SkillManager::removeSkill(const String& skillId) noexcept {
    size_t idx = findSkill(skillId);
    if (idx == SIZE_MAX) return false;

    m_skills.erase(m_skills.begin() + static_cast<ptrdiff_t>(idx));
    m_dirty = true;

    Logger::info(kLogCategory, "Skill '%s' removed", skillId.c_str());
    return true;
}

bool SkillManager::enableSkill(const String& skillId) noexcept {
    size_t idx = findSkill(skillId);
    if (idx == SIZE_MAX) return false;
    m_skills[idx].enabled = true;
    m_dirty = true;
    return true;
}

bool SkillManager::disableSkill(const String& skillId) noexcept {
    size_t idx = findSkill(skillId);
    if (idx == SIZE_MAX) return false;
    m_skills[idx].enabled = false;
    m_dirty = true;
    return true;
}

const std::vector<SkillEntry>& SkillManager::getAllSkills() const noexcept {
    return m_skills;
}

SkillEntry SkillManager::getSkill(const String& skillId) const noexcept {
    size_t idx = findSkill(skillId);
    if (idx == SIZE_MAX) return SkillEntry();
    return m_skills[idx];
}

bool SkillManager::isInitialized() const noexcept {
    return m_initialized;
}

size_t SkillManager::skillCount() const noexcept {
    return m_skills.size();
}

bool SkillManager::updateSkill(const String& skillId, const SkillEntry& updates) noexcept {
    size_t idx = findSkill(skillId);
    if (idx == SIZE_MAX) return false;

    SkillEntry& existing = m_skills[idx];
    existing.name = updates.name;
    existing.voiceTrigger = updates.voiceTrigger;
    existing.description = updates.description;
    existing.category = updates.category;
    existing.icon = updates.icon;
    existing.author = updates.author;
    existing.version = updates.version;
    existing.tags = updates.tags;
    existing.rating = updates.rating;
    existing.isTemplate = updates.isTemplate;
    existing.dependencies = updates.dependencies;
    existing.modifiedAt = millis();
    existing.priority = updates.priority;
    existing.enabled = updates.enabled;
    existing.actionCount = updates.actionCount;
    for (uint8_t i = 0; i < updates.actionCount && i < SKILL_ACTIONS_MAX; ++i) {
        existing.actions[i] = updates.actions[i];
    }
    existing.conditionCount = updates.conditionCount;
    for (uint8_t i = 0; i < updates.conditionCount && i < SKILL_CONDITIONS_MAX; ++i) {
        existing.conditions[i] = updates.conditions[i];
    }

    m_dirty = true;
    return true;
}

bool SkillManager::duplicateSkill(const String& skillId) noexcept {
    size_t idx = findSkill(skillId);
    if (idx == SIZE_MAX) return false;

    SkillEntry copy = m_skills[idx];
    copy.name += " (Copy)";
    copy.lastTriggered = 0;
    copy.triggerCount = 0;

    return addSkill(copy);
}

bool SkillManager::importSkill(const String& json) noexcept {
    SkillEntry skill;
    if (!parseSkillJson(json, skill) || skill.id.isEmpty()) return false;
    return addSkill(skill);
}

String SkillManager::exportSkill(const String& skillId) const noexcept {
    size_t idx = findSkill(skillId);
    if (idx == SIZE_MAX) return "";

    const SkillEntry& s = m_skills[idx];
    String json;
    json.reserve(1024);
    json += "{";
    json += "\"id\":\"" + escapeJson(s.id) + "\",";
    json += "\"name\":\"" + escapeJson(s.name) + "\",";
    json += "\"voice_trigger\":\"" + escapeJson(s.voiceTrigger) + "\",";
    json += "\"description\":\"" + escapeJson(s.description) + "\",";
    json += "\"category\":\"" + escapeJson(s.category) + "\",";
    json += "\"icon\":\"" + escapeJson(s.icon) + "\",";
    json += "\"author\":\"" + escapeJson(s.author) + "\",";
    json += "\"version\":\"" + escapeJson(s.version) + "\",";
    json += "\"tags\":\"" + escapeJson(s.tags) + "\",";
    json += "\"rating\":" + String(s.rating) + ",";
    json += "\"is_template\":" + String(s.isTemplate ? "true" : "false") + ",";
    json += "\"dependencies\":\"" + escapeJson(s.dependencies) + "\",";
    json += "\"priority\":" + String(s.priority) + ",";
    json += "\"enabled\":" + String(s.enabled ? "true" : "false") + ",";
    json += "\"created_at\":" + String(s.createdAt) + ",";
    json += "\"modified_at\":" + String(s.modifiedAt) + ",";
    json += "\"trigger_count\":" + String(s.triggerCount) + ",";
    json += "\"actions\":[";
    for (uint8_t a = 0; a < s.actionCount; ++a) {
        if (a > 0) json += ",";
        json += "{";
        json += "\"type\":\"" + actionTypeToString(s.actions[a].type) + "\",";
        json += "\"target\":\"" + escapeJson(s.actions[a].target) + "\",";
        json += "\"value\":\"" + escapeJson(s.actions[a].value) + "\",";
        json += "\"metadata\":\"" + escapeJson(s.actions[a].metadata) + "\"";
        json += "}";
    }
    json += "],\"conditions\":[";
    for (uint8_t c = 0; c < s.conditionCount; ++c) {
        if (c > 0) json += ",";
        json += "{";
        json += "\"type\":\"" + conditionTypeToString(s.conditions[c].type) + "\",";
        json += "\"param1\":\"" + escapeJson(s.conditions[c].param1) + "\",";
        json += "\"param2\":\"" + escapeJson(s.conditions[c].param2) + "\"";
        json += "}";
    }
    json += "]}";
    return json;
}

std::vector<SkillEntry> SkillManager::getSkillsByCategory(const String& category) const noexcept {
    std::vector<SkillEntry> result;
    for (const auto& skill : m_skills) {
        if (skill.category == category) {
            result.push_back(skill);
        }
    }
    return result;
}

std::vector<SkillEntry> SkillManager::searchSkills(const String& query) const noexcept {
    if (query.isEmpty()) return m_skills;

    std::vector<SkillEntry> result;
    String lowerQuery = query;
    lowerQuery.toLowerCase();

    for (const auto& skill : m_skills) {
        String lowerName = skill.name;
        lowerName.toLowerCase();
        String lowerTrigger = skill.voiceTrigger;
        lowerTrigger.toLowerCase();
        String lowerDesc = skill.description;
        lowerDesc.toLowerCase();
        String lowerTags = skill.tags;
        lowerTags.toLowerCase();

        if (lowerName.indexOf(lowerQuery) >= 0 ||
            lowerTrigger.indexOf(lowerQuery) >= 0 ||
            lowerDesc.indexOf(lowerQuery) >= 0 ||
            lowerTags.indexOf(lowerQuery) >= 0) {
            result.push_back(skill);
        }
    }
    return result;
}

std::vector<SkillEntry> SkillManager::getTemplates() const noexcept {
    std::vector<SkillEntry> result;
    for (const auto& skill : m_skills) {
        if (skill.isTemplate) {
            result.push_back(skill);
        }
    }
    return result;
}

void SkillManager::recordExecution(const String& skillId, bool success, unsigned long durationMs) noexcept {
    if (m_executionLog.size() >= kMaxExecutionLog) {
        m_executionLog.erase(m_executionLog.begin());
    }
    ExecutionRecord rec;
    rec.skillId = skillId;
    rec.success = success;
    rec.timestamp = millis();
    rec.durationMs = durationMs;
    m_executionLog.push_back(rec);
}

bool SkillManager::save() noexcept {
    String json;
    json.reserve(4096);
    json += "{\"skills\":[";
    for (size_t i = 0; i < m_skills.size(); ++i) {
        if (i > 0) json += ",";
        const SkillEntry& s = m_skills[i];
        json += "{";
        json += "\"id\":\"" + escapeJson(s.id) + "\",";
        json += "\"name\":\"" + escapeJson(s.name) + "\",";
        json += "\"voice_trigger\":\"" + escapeJson(s.voiceTrigger) + "\",";
        json += "\"description\":\"" + escapeJson(s.description) + "\",";
        json += "\"priority\":" + String(s.priority) + ",";
        json += "\"enabled\":" + String(s.enabled ? "true" : "false") + ",";
        json += "\"created_at\":" + String(s.createdAt) + ",";
        json += "\"trigger_count\":" + String(s.triggerCount) + ",";
        json += "\"category\":\"" + escapeJson(s.category) + "\",";
        json += "\"icon\":\"" + escapeJson(s.icon) + "\",";
        json += "\"author\":\"" + escapeJson(s.author) + "\",";
        json += "\"version\":\"" + escapeJson(s.version) + "\",";
        json += "\"tags\":\"" + escapeJson(s.tags) + "\",";
        json += "\"rating\":" + String(s.rating) + ",";
        json += "\"is_template\":" + String(s.isTemplate ? "true" : "false") + ",";
        json += "\"dependencies\":\"" + escapeJson(s.dependencies) + "\",";
        json += "\"modified_at\":" + String(s.modifiedAt) + ",";
        json += "\"actions\":[";
        for (uint8_t a = 0; a < s.actionCount; ++a) {
            if (a > 0) json += ",";
            json += "{";
            json += "\"type\":\"" + actionTypeToString(s.actions[a].type) + "\",";
            json += "\"target\":\"" + escapeJson(s.actions[a].target) + "\",";
            json += "\"value\":\"" + escapeJson(s.actions[a].value) + "\",";
            json += "\"metadata\":\"" + escapeJson(s.actions[a].metadata) + "\"";
            json += "}";
        }
        json += "],\"conditions\":[";
        for (uint8_t c = 0; c < s.conditionCount; ++c) {
            if (c > 0) json += ",";
            json += "{";
            json += "\"type\":\"" + conditionTypeToString(s.conditions[c].type) + "\",";
            json += "\"param1\":\"" + escapeJson(s.conditions[c].param1) + "\",";
            json += "\"param2\":\"" + escapeJson(s.conditions[c].param2) + "\"";
            json += "}";
        }
        json += "]}";
    }
    json += "]}";

    StorageStatus status = storageManager.writeFile(kStoragePath, json, StorageType::SPIFFS);
    if (status == StorageStatus::SUCCESS) {
        saveExecutionLog();
        return true;
    }

    Logger::error(kLogCategory, "Save failed: %d", static_cast<int>(status));
    return false;
}

bool SkillManager::load() noexcept {
    if (!storageManager.fileExists(kStoragePath, StorageType::SPIFFS)) return false;

    String content;
    StorageStatus status = storageManager.readFile(kStoragePath, content, StorageType::SPIFFS);
    if (status != StorageStatus::SUCCESS || content.isEmpty()) return false;

    m_skills.clear();

    // Parse skills array
    int pos = 0;
    while (true) {
        int start = content.indexOf('{', pos);
        if (start < 0) break;

        // Find the end of this skill object
        int braceCount = 0;
        int end = start;
        for (; end < (int)content.length(); ++end) {
            if (content[end] == '{') braceCount++;
            else if (content[end] == '}') { braceCount--; if (braceCount == 0) break; }
        }
        if (end >= (int)content.length()) break;

        String skillJson = content.substring(start, end + 1);
        SkillEntry skill;
        if (parseSkillJson(skillJson, skill) && !skill.id.isEmpty()) {
            m_skills.push_back(skill);
        }

        pos = end + 1;
    }

    loadExecutionLog();

    Logger::info(kLogCategory, "Loaded %u skills", m_skills.size());
    return true;
}

String SkillManager::generateId() noexcept {
    unsigned long now = millis();
    m_lastIdCounter++;
    uint32_t mix = static_cast<uint32_t>(now) ^
                   static_cast<uint32_t>(m_lastIdCounter << 16) ^
                   static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFFF);

    String id;
    id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) {
        id += hex[val & 0x0F];
        val = (val >> 2) ^ (val << 3) ^ (m_lastIdCounter + i);
    }
    return id;
}

size_t SkillManager::findSkill(const String& id) const noexcept {
    for (size_t i = 0; i < m_skills.size(); ++i) {
        if (m_skills[i].id == id) return i;
    }
    return SIZE_MAX;
}

SkillActionType SkillManager::parseActionType(const String& str) const noexcept {
    if (str == "DISPLAY") return SkillActionType::SHOW_DISPLAY;
    if (str == "LED") return SkillActionType::LED;
    if (str == "SET_REMINDER") return SkillActionType::SET_REMINDER;
    if (str == "PLAY_SOUND") return SkillActionType::PLAY_SOUND;
    if (str == "STOP_CONVERSATION") return SkillActionType::STOP_CONVERSATION;
    return SkillActionType::CUSTOM;
}

String SkillManager::actionTypeToString(SkillActionType type) const noexcept {
    switch (type) {
        case SkillActionType::SHOW_DISPLAY: return "DISPLAY";
        case SkillActionType::LED: return "LED";
        case SkillActionType::SET_REMINDER: return "SET_REMINDER";
        case SkillActionType::PLAY_SOUND: return "PLAY_SOUND";
        case SkillActionType::STOP_CONVERSATION: return "STOP_CONVERSATION";
        default: return "CUSTOM";
    }
}

SkillConditionType SkillManager::parseConditionType(const String& str) const noexcept {
    if (str == "TIME_RANGE") return SkillConditionType::TIME_RANGE;
    if (str == "WIFI_STATE") return SkillConditionType::WIFI_STATE;
    if (str == "SYSTEM_STATE") return SkillConditionType::SYSTEM_STATE;
    return SkillConditionType::NONE;
}

String SkillManager::conditionTypeToString(SkillConditionType type) const noexcept {
    switch (type) {
        case SkillConditionType::TIME_RANGE: return "TIME_RANGE";
        case SkillConditionType::WIFI_STATE: return "WIFI_STATE";
        case SkillConditionType::SYSTEM_STATE: return "SYSTEM_STATE";
        default: return "NONE";
    }
}

bool SkillManager::parseSkillJson(const String& json, SkillEntry& skill) const noexcept {
    skill.id = extractStr(json, "id");
    skill.name = extractStr(json, "name");
    skill.voiceTrigger = extractStr(json, "voice_trigger");
    skill.description = extractStr(json, "description");
    skill.priority = static_cast<uint8_t>(extractInt(json, "priority", 0));
    skill.enabled = extractBool(json, "enabled", true);
    skill.createdAt = static_cast<unsigned long>(extractInt(json, "created_at", 0));
    skill.triggerCount = static_cast<uint32_t>(extractInt(json, "trigger_count", 0));
    skill.category = extractStr(json, "category");
    skill.icon = extractStr(json, "icon");
    skill.author = extractStr(json, "author");
    skill.version = extractStr(json, "version");
    skill.tags = extractStr(json, "tags");
    skill.rating = static_cast<uint8_t>(extractInt(json, "rating", 0));
    skill.isTemplate = extractBool(json, "is_template", false);
    skill.dependencies = extractStr(json, "dependencies");
    skill.modifiedAt = static_cast<unsigned long>(extractInt(json, "modified_at", 0));
    skill.lastTriggered = skill.createdAt;

    return !skill.id.isEmpty();
}

void SkillManager::saveExecutionLog() noexcept {
    String json;
    json.reserve(2048);
    json += "{\"executions\":[";
    for (size_t i = 0; i < m_executionLog.size(); ++i) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"skill_id\":\"" + escapeJson(m_executionLog[i].skillId) + "\",";
        json += "\"success\":" + String(m_executionLog[i].success ? "true" : "false") + ",";
        json += "\"timestamp\":" + String(m_executionLog[i].timestamp) + ",";
        json += "\"duration_ms\":" + String(m_executionLog[i].durationMs) + "";
        json += "}";
    }
    json += "]}";
    storageManager.writeFile("/skill_execution_log.json", json, StorageType::SPIFFS);
}

void SkillManager::loadExecutionLog() noexcept {
    if (!storageManager.fileExists("/skill_execution_log.json", StorageType::SPIFFS)) return;

    String content;
    StorageStatus status = storageManager.readFile("/skill_execution_log.json", content, StorageType::SPIFFS);
    if (status != StorageStatus::SUCCESS || content.isEmpty()) return;

    m_executionLog.clear();

    int pos = 0;
    while (true) {
        int start = content.indexOf('{', pos);
        if (start < 0) break;

        int braceCount = 0;
        int end = start;
        for (; end < (int)content.length(); ++end) {
            if (content[end] == '{') braceCount++;
            else if (content[end] == '}') { braceCount--; if (braceCount == 0) break; }
        }
        if (end >= (int)content.length()) break;

        String recJson = content.substring(start, end + 1);
        ExecutionRecord rec;
        rec.skillId = extractStr(recJson, "skill_id");
        rec.success = extractBool(recJson, "success", false);
        rec.timestamp = static_cast<unsigned long>(extractInt(recJson, "timestamp", 0));
        rec.durationMs = static_cast<unsigned long>(extractInt(recJson, "duration_ms", 0));

        if (!rec.skillId.isEmpty()) {
            m_executionLog.push_back(rec);
        }

        pos = end + 1;
    }
}

size_t SkillManager::findExecutionRecord(const String& skillId, unsigned long since) const noexcept {
    for (size_t i = 0; i < m_executionLog.size(); ++i) {
        if (m_executionLog[i].skillId == skillId && m_executionLog[i].timestamp >= since) {
            return i;
        }
    }
    return SIZE_MAX;
}
