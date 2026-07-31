#include "planner_manager.h"
#include "json_helpers.h"

PlannerManager plannerManager;

PlannerManager::PlannerManager() noexcept : m_initialized(false), m_dirty(false), m_lastIdCounter(0) {
    m_tasks.reserve(kMaxTasks);
}

PlannerManager::~PlannerManager() noexcept { if (m_dirty) save(); }

bool PlannerManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u tasks)", m_tasks.size());
    return true;
}

void PlannerManager::update() noexcept {
    if (!m_initialized) return;
    static unsigned long lastSave = 0;
    unsigned long now = millis();
    if (m_dirty && (now - lastSave > 5000)) { lastSave = now; if (save()) m_dirty = false; }
}

String PlannerManager::addTask(const String& goalId, const String& title,
                                 const String& description, uint8_t priority,
                                 unsigned long durationMs, unsigned long deadline) noexcept {
    if (!m_initialized || m_tasks.size() >= kMaxTasks || title.isEmpty()) return "";
    PlannedTask t;
    t.id = generateId(); t.goalId = goalId; t.title = title;
    t.description = description; t.priority = priority;
    t.estimatedDurationMs = durationMs; t.deadline = deadline;
    t.scheduledTime = millis() + 3600000UL; // default: schedule 1 hour from now
    t.completed = false;
    m_tasks.push_back(t); m_dirty = true;
    Logger::info(kLogCategory, "Task '%s' added", title.c_str());
    return t.id;
}

bool PlannerManager::removeTask(const String& taskId) noexcept {
    size_t idx = findTask(taskId);
    if (idx == SIZE_MAX) return false;
    m_tasks.erase(m_tasks.begin() + static_cast<ptrdiff_t>(idx));
    m_dirty = true; return true;
}

bool PlannerManager::completeTask(const String& taskId) noexcept {
    size_t idx = findTask(taskId);
    if (idx == SIZE_MAX) return false;
    m_tasks[idx].completed = true; m_dirty = true;
    Logger::info(kLogCategory, "Task '%s' completed", m_tasks[idx].title.c_str());
    return true;
}

bool PlannerManager::rescheduleTask(const String& taskId, unsigned long newTime) noexcept {
    size_t idx = findTask(taskId);
    if (idx == SIZE_MAX) return false;
    m_tasks[idx].scheduledTime = newTime; m_dirty = true; return true;
}

std::vector<PlannedTask> PlannerManager::getTasksForGoal(const String& goalId) const noexcept {
    std::vector<PlannedTask> r;
    for (const auto& t : m_tasks) { if (t.goalId == goalId) r.push_back(t); }
    return r;
}

std::vector<PlannedTask> PlannerManager::getTodaysTasks() const noexcept {
    std::vector<PlannedTask> r;
    unsigned long now = millis();
    unsigned long dayStart = now - (now % 86400000UL);
    unsigned long dayEnd = dayStart + 86400000UL;
    for (const auto& t : m_tasks) {
        if (!t.completed && t.scheduledTime >= dayStart && t.scheduledTime < dayEnd) r.push_back(t);
    }
    return r;
}

std::vector<PlannedTask> PlannerManager::getUpcomingTasks() const noexcept {
    std::vector<PlannedTask> r;
    unsigned long now = millis();
    for (const auto& t : m_tasks) { if (!t.completed && t.scheduledTime > now) r.push_back(t); }
    return r;
}

const std::vector<PlannedTask>& PlannerManager::getAllTasks() const noexcept { return m_tasks; }

String PlannerManager::suggestNextAction() const noexcept {
    unsigned long now = millis();
    String bestId;
    uint8_t bestPriority = 0;
    unsigned long bestTime = 0;

    for (const auto& t : m_tasks) {
        if (t.completed) continue;
        if (t.scheduledTime <= now + 3600000UL) { // within next hour
            if (t.priority > bestPriority || (t.priority == bestPriority && t.scheduledTime < bestTime)) {
                bestId = "Next: " + t.title;
                bestPriority = t.priority;
                bestTime = t.scheduledTime;
            }
        }
    }

    if (bestId.isEmpty()) {
        for (const auto& t : m_tasks) {
            if (!t.completed) { bestId = "Next: " + t.title; break; }
        }
    }

    return bestId.isEmpty() ? "No pending tasks" : bestId;
}

size_t PlannerManager::taskCount() const noexcept { return m_tasks.size(); }
bool PlannerManager::isInitialized() const noexcept { return m_initialized; }

bool PlannerManager::save() noexcept {
    String json; json.reserve(4096);
    json += "{\"tasks\":[";
    for (size_t i = 0; i < m_tasks.size(); ++i) {
        if (i > 0) json += ",";
        const auto& t = m_tasks[i];
        json += "{\"id\":\"" + escapeJson(t.id) + "\",";
        json += "\"goal\":\"" + escapeJson(t.goalId) + "\",";
        json += "\"title\":\"" + escapeJson(t.title) + "\",";
        json += "\"priority\":" + String(t.priority) + ",";
        json += "\"completed\":" + String(t.completed ? "true" : "false") + "}";
    }
    json += "]}";
    StorageStatus st = storageManager.writeFile(PLANNER_PATH, json, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_dirty = false; return true; }
    return false;
}

bool PlannerManager::load() noexcept {
    if (!storageManager.fileExists(PLANNER_PATH, StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(PLANNER_PATH, content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_tasks.clear();
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
        PlannedTask t; t.id = ext("id"); t.title = ext("title"); t.goalId = ext("goal");
        if (!t.id.isEmpty()) { m_tasks.push_back(t); }
        pos = e + 1;
    }
    return true;
}

String PlannerManager::generateId() noexcept {
    return ::generateId();
}

size_t PlannerManager::findTask(const String& id) const noexcept {
    for (size_t i = 0; i < m_tasks.size(); ++i) { if (m_tasks[i].id == id) return i; }
    return SIZE_MAX;
}
