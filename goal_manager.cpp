#include "goal_manager.h"
#include "json_helpers.h"

GoalManager goalManager;

GoalManager::GoalManager() noexcept : m_initialized(false), m_dirty(false), m_lastIdCounter(0) {
}

GoalManager::~GoalManager() noexcept { if (m_dirty) save(); }

bool GoalManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u goals)", m_goals.size());
    return true;
}

void GoalManager::update() noexcept {
    if (!m_initialized) return;
    static unsigned long lastSave = 0;
    unsigned long now = millis();
    if (m_dirty && (now - lastSave > 5000)) { lastSave = now; if (save()) m_dirty = false; }
}

String GoalManager::createGoal(const String& title, GoalType type, const String& description,
                                 uint8_t priority, unsigned long deadline) noexcept {
    if (!m_initialized || m_goals.size() >= kMaxGoals || title.isEmpty()) return "";
    GoalEntry g;
    g.id = generateId(); g.title = title; g.description = description;
    g.type = type; g.priority = priority; g.deadline = deadline;
    g.progress = 0; g.createdAt = millis(); g.completed = false;
    m_goals.push_back(g); m_dirty = true;
    Logger::info(kLogCategory, "Goal '%s' created (%s)", title.c_str(),
        type == GoalType::DAILY ? "DAILY" : type == GoalType::WEEKLY ? "WEEKLY" : "LONG_TERM");
    return g.id;
}

bool GoalManager::updateGoal(const String& id, const String& title, const String& description,
                               uint8_t priority, uint8_t progress) noexcept {
    size_t idx = findGoal(id);
    if (idx == SIZE_MAX) return false;
    GoalEntry& g = m_goals[idx];
    if (!title.isEmpty()) g.title = title;
    if (!description.isEmpty()) g.description = description;
    g.priority = priority; g.progress = progress;
    if (g.progress >= 100) g.completed = true;
    m_dirty = true; return true;
}

bool GoalManager::deleteGoal(const String& id) noexcept {
    size_t idx = findGoal(id);
    if (idx == SIZE_MAX) return false;
    m_goals.erase(m_goals.begin() + static_cast<ptrdiff_t>(idx));
    m_dirty = true; return true;
}

bool GoalManager::completeGoal(const String& id) noexcept {
    size_t idx = findGoal(id);
    if (idx == SIZE_MAX) return false;
    m_goals[idx].completed = true; m_goals[idx].completedAt = millis(); m_goals[idx].progress = 100;
    m_dirty = true; Logger::info(kLogCategory, "Goal '%s' completed", m_goals[idx].title.c_str());
    return true;
}

String GoalManager::addMilestone(const String& goalId, const String& name) noexcept {
    size_t idx = findGoal(goalId);
    if (idx == SIZE_MAX || m_goals[idx].milestones.size() >= kMaxMilestones) return "";
    Milestone m; m.name = name;
    m_goals[idx].milestones.push_back(m); m_dirty = true;
    return name;
}

bool GoalManager::completeMilestone(const String& goalId, const String& milestoneName) noexcept {
    size_t idx = findGoal(goalId);
    if (idx == SIZE_MAX) return false;
    for (auto& m : m_goals[idx].milestones) {
        if (m.name == milestoneName && !m.completed) {
            m.completed = true; m.completedAt = millis(); m_dirty = true; return true;
        }
    }
    return false;
}

bool GoalManager::linkReminder(const String& goalId, const String& reminderId) noexcept {
    size_t idx = findGoal(goalId); if (idx == SIZE_MAX) return false;
    m_goals[idx].linkedReminders.push_back(reminderId); m_dirty = true; return true;
}

bool GoalManager::linkMemory(const String& goalId, const String& memoryId) noexcept {
    size_t idx = findGoal(goalId); if (idx == SIZE_MAX) return false;
    m_goals[idx].linkedMemories.push_back(memoryId); m_dirty = true; return true;
}

bool GoalManager::linkConversation(const String& goalId, const String& conversationId) noexcept {
    size_t idx = findGoal(goalId); if (idx == SIZE_MAX) return false;
    m_goals[idx].linkedConversations.push_back(conversationId); m_dirty = true; return true;
}

GoalEntry GoalManager::getGoal(const String& id) const noexcept {
    size_t idx = findGoal(id); return (idx != SIZE_MAX) ? m_goals[idx] : GoalEntry();
}

std::vector<GoalEntry> GoalManager::getActiveGoals() const noexcept {
    std::vector<GoalEntry> r;
    for (const auto& g : m_goals) { if (!g.completed) r.push_back(g); }
    return r;
}

std::vector<GoalEntry> GoalManager::getCompletedGoals() const noexcept {
    std::vector<GoalEntry> r;
    for (const auto& g : m_goals) { if (g.completed) r.push_back(g); }
    return r;
}

std::vector<GoalEntry> GoalManager::getGoalsByType(GoalType type) const noexcept {
    std::vector<GoalEntry> r;
    for (const auto& g : m_goals) { if (g.type == type) r.push_back(g); }
    return r;
}

const std::vector<GoalEntry>& GoalManager::getAllGoals() const noexcept { return m_goals; }
size_t GoalManager::goalCount() const noexcept { return m_goals.size(); }
bool GoalManager::isInitialized() const noexcept { return m_initialized; }

bool GoalManager::save() noexcept {
    String json; json.reserve(4096);
    json += "{\"goals\":[";
    for (size_t i = 0; i < m_goals.size(); ++i) {
        if (i > 0) json += ",";
        const auto& g = m_goals[i];
        json += "{\"id\":\"" + escapeJson(g.id) + "\",\"title\":\"" + escapeJson(g.title) + "\",";
        json += "\"desc\":\"" + escapeJson(g.description) + "\",";
        json += "\"type\":" + String(static_cast<int>(g.type)) + ",";
        json += "\"priority\":" + String(g.priority) + ",";
        json += "\"progress\":" + String(g.progress) + ",";
        json += "\"completed\":" + String(g.completed ? "true" : "false") + "}";
    }
    json += "]}";
    StorageStatus st = storageManager.writeFile(GOALS_PATH, json, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_dirty = false; return true; }
    return false;
}

bool GoalManager::load() noexcept {
    if (!storageManager.fileExists(GOALS_PATH, StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(GOALS_PATH, content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_goals.clear();
    int pos = 0;
    while (true) {
        int s = content.indexOf('{', pos); if (s < 0) break;
        int e = content.indexOf('}', s); if (e < 0) break;
        String obj = content.substring(s, e + 1);
        auto ext = [&](const char* k) -> String {
            String q = String("\"") + k + "\":\""; int st = obj.indexOf(q);
            if (st < 0) { q = String("\"") + k + "\":"; st = obj.indexOf(q); if (st < 0) return ""; st += q.length(); int en = st; while (en < (int)obj.length() && obj[en] != ',' && obj[en] != '}') en++; return obj.substring(st, en); }
            st += q.length(); int en = obj.indexOf('"', st); return (en < 0) ? "" : obj.substring(st, en);
        };
        GoalEntry g; g.id = ext("id"); g.title = ext("title");
        if (!g.id.isEmpty()) { m_goals.push_back(g); }
        pos = e + 1;
    }
    return true;
}

String GoalManager::generateId() noexcept {
    return ::generateId();
}

size_t GoalManager::findGoal(const String& id) const noexcept {
    for (size_t i = 0; i < m_goals.size(); ++i) { if (m_goals[i].id == id) return i; }
    return SIZE_MAX;
}
