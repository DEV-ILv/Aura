#include "habit_manager.h"
#include "json_helpers.h"

HabitManager habitManager;

HabitManager::HabitManager() noexcept : m_initialized(false), m_dirty(false), m_lastIdCounter(0) {
    m_habits.reserve(kMaxHabits);
}

HabitManager::~HabitManager() noexcept { if (m_dirty) save(); }

bool HabitManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u habits)", m_habits.size());
    return true;
}

void HabitManager::update() noexcept {
    if (!m_initialized) return;
    static unsigned long lastSave = 0;
    unsigned long now = millis();
    if (m_dirty && (now - lastSave > 5000)) { lastSave = now; if (save()) m_dirty = false; }
}

String HabitManager::createHabit(const String& name, HabitSchedule schedule,
                                   const String& description, bool reminderEnabled) noexcept {
    if (!m_initialized || m_habits.size() >= kMaxHabits || name.isEmpty()) return "";
    HabitEntry h;
    h.id = generateId(); h.name = name; h.description = description;
    h.schedule = schedule; h.reminderEnabled = reminderEnabled;
    h.createdAt = millis();
    m_habits.push_back(h); m_dirty = true;
    Logger::info(kLogCategory, "Habit '%s' created", name.c_str());
    return h.id;
}

bool HabitManager::updateHabit(const String& id, const String& name, const String& description) noexcept {
    size_t idx = findHabit(id);
    if (idx == SIZE_MAX) return false;
    if (!name.isEmpty()) m_habits[idx].name = name;
    if (!description.isEmpty()) m_habits[idx].description = description;
    m_dirty = true; return true;
}

bool HabitManager::deleteHabit(const String& id) noexcept {
    size_t idx = findHabit(id);
    if (idx == SIZE_MAX) return false;
    m_habits.erase(m_habits.begin() + static_cast<ptrdiff_t>(idx));
    m_dirty = true; return true;
}

bool HabitManager::completeHabit(const String& id) noexcept {
    size_t idx = findHabit(id);
    if (idx == SIZE_MAX) return false;
    HabitEntry& h = m_habits[idx];
    h.totalCompletions++;
    h.lastCompletedDate = millis() / 86400000UL; // approximate day
    h.streak++;
    if (h.streak > h.longestStreak) h.longestStreak = h.streak;
    // Simple success rate: assume one expected completion per day since creation
    unsigned long daysActive = (millis() - h.createdAt) / 86400000UL;
    if (daysActive > 0) h.successRate = (static_cast<float>(h.totalCompletions) / daysActive) * 100.0f;
    if (h.successRate > 100.0f) h.successRate = 100.0f;
    m_dirty = true;
    Logger::info(kLogCategory, "Habit '%s' completed (streak: %u)", h.name.c_str(), h.streak);
    return true;
}

HabitEntry HabitManager::getHabit(const String& id) const noexcept {
    size_t idx = findHabit(id); return (idx != SIZE_MAX) ? m_habits[idx] : HabitEntry();
}

const std::vector<HabitEntry>& HabitManager::getAllHabits() const noexcept { return m_habits; }

std::vector<HabitEntry> HabitManager::getDueHabits() const noexcept {
    std::vector<HabitEntry> due;
    for (const auto& h : m_habits) { if (isDue(h)) due.push_back(h); }
    return due;
}

size_t HabitManager::habitCount() const noexcept { return m_habits.size(); }
bool HabitManager::isInitialized() const noexcept { return m_initialized; }

bool HabitManager::save() noexcept {
    String json; json.reserve(4096);
    json += "{\"habits\":[";
    for (size_t i = 0; i < m_habits.size(); ++i) {
        if (i > 0) json += ",";
        const auto& h = m_habits[i];
        json += "{\"id\":\"" + escapeJson(h.id) + "\",";
        json += "\"name\":\"" + escapeJson(h.name) + "\",";
        json += "\"desc\":\"" + escapeJson(h.description) + "\",";
        json += "\"schedule\":" + String(static_cast<int>(h.schedule)) + ",";
        json += "\"streak\":" + String(h.streak) + ",";
        json += "\"longest\":" + String(h.longestStreak) + ",";
        json += "\"total\":" + String(h.totalCompletions) + "}";
    }
    json += "]}";
    StorageStatus st = storageManager.writeFile(HABITS_PATH, json, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_dirty = false; return true; }
    return false;
}

bool HabitManager::load() noexcept {
    if (!storageManager.fileExists(HABITS_PATH, StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(HABITS_PATH, content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_habits.clear();
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
        HabitEntry h; h.id = ext("id"); h.name = ext("name");
        if (!h.id.isEmpty()) { m_habits.push_back(h); }
        pos = e + 1;
    }
    return true;
}

String HabitManager::generateId() noexcept {
    return ::generateId();
}

size_t HabitManager::findHabit(const String& id) const noexcept {
    for (size_t i = 0; i < m_habits.size(); ++i) { if (m_habits[i].id == id) return i; }
    return SIZE_MAX;
}

bool HabitManager::isDue(const HabitEntry& h) const noexcept {
    if (h.totalCompletions == 0) return true;
    unsigned long today = millis() / 86400000UL;
    return h.lastCompletedDate < today;
}
