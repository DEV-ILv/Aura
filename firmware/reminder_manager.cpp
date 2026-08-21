#include "reminder_manager.h"
#include <ArduinoJson.h>
#include <ctime>
#include "aura_system.h"

/// Global ReminderManager instance
ReminderManager reminderManager;

// ============================================================================
// Anonymous Namespace - Internal Helpers
// ============================================================================

namespace {

constexpr uint32_t kMaxSnoozeMinutes = 1440U;  // 24 hours
constexpr uint32_t kTtsTimeoutMs = 30000UL;    // 30 seconds TTS timeout
constexpr uint32_t kStateTimeoutMs = 60000UL;  // 60 seconds state timeout

/**
 * @brief Convert time_t to local time struct
 */
struct tm timeToTm(time_t t) noexcept {
    struct tm tm = {};
    localtime_r(&t, &tm);
    return tm;
}

/**
 * @brief Convert local time struct to time_t
 */
time_t tmToTime(const struct tm& tm) noexcept {
    return mktime(const_cast<struct tm*>(&tm));
}

/**
 * @brief Add days to time_t
 */
time_t addDays(time_t t, int days) noexcept {
    struct tm tm = timeToTm(t);
    tm.tm_mday += days;
    return tmToTime(tm);
}

/**
 * @brief Add months to time_t
 */
time_t addMonths(time_t t, int months) noexcept {
    struct tm tm = timeToTm(t);
    tm.tm_mon += months;
    return tmToTime(tm);
}

/**
 * @brief Add years to time_t
 */
time_t addYears(time_t t, int years) noexcept {
    struct tm tm = timeToTm(t);
    tm.tm_year += years;
    return tmToTime(tm);
}

/**
 * @brief Serialize reminder to JSON
 */
String reminderToJson(const Reminder& reminder) noexcept {
    JsonDocument doc;
    doc["id"] = reminder.id;
    doc["title"] = reminder.title;
    doc["message"] = reminder.message;
    doc["triggerTime"] = static_cast<uint32_t>(reminder.triggerTime);
    doc["type"] = static_cast<uint8_t>(reminder.type);
    doc["priority"] = static_cast<uint8_t>(reminder.priority);
    doc["status"] = static_cast<uint8_t>(reminder.status);
    doc["enabled"] = reminder.enabled;
    doc["recurring"] = reminder.recurring;
    doc["createdTime"] = static_cast<uint32_t>(reminder.createdTime);
    doc["lastTriggeredTime"] = static_cast<uint32_t>(reminder.lastTriggeredTime);

    String output;
    serializeJson(doc, output);
    return output;
}

/**
 * @brief Deserialize reminder from JSON
 */
bool jsonToReminder(const String& json, Reminder& reminder) noexcept {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;

    reminder.id = doc["id"] | 0;
    reminder.title = doc["title"] | "";
    reminder.message = doc["message"] | "";
    reminder.triggerTime = static_cast<time_t>(doc["triggerTime"] | 0);
    reminder.type = static_cast<ReminderType>(doc["type"] | 0);
    reminder.priority = static_cast<ReminderPriority>(doc["priority"] | 0);
    reminder.status = static_cast<ReminderStatus>(doc["status"] | 0);
    reminder.enabled = doc["enabled"] | true;
    reminder.recurring = doc["recurring"] | false;
    reminder.createdTime = static_cast<time_t>(doc["createdTime"] | 0);
    reminder.lastTriggeredTime = static_cast<time_t>(doc["lastTriggeredTime"] | 0);
    return true;
}

/**
 * @brief Serialize history to JSON
 */
String historyToJson(const ReminderHistory& history) noexcept {
    JsonDocument doc;
    doc["reminderId"] = history.reminderId;
    doc["timestamp"] = history.timestamp;
    doc["acknowledged"] = history.acknowledged;

    String output;
    serializeJson(doc, output);
    return output;
}

/**
 * @brief Deserialize history from JSON
 */
bool jsonToHistory(const String& json, ReminderHistory& history) noexcept {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;

    history.reminderId = doc["reminderId"] | 0;
    history.timestamp = doc["timestamp"] | 0;
    history.acknowledged = doc["acknowledged"] | false;
    return true;
}

}  // namespace

// ============================================================================
// Constructor / Destructor
// ============================================================================

ReminderManager::ReminderManager() noexcept
    : m_initialized(false),
      m_currentState(ReminderState::IDLE),
      m_lastError(ReminderError::NONE),
      m_nextId(1),
      m_lastCheckTime(0) {
}

ReminderManager::~ReminderManager() noexcept {
    if (m_initialized) {
        save();
    }
}

// ============================================================================
// Public API - Lifecycle
// ============================================================================

bool ReminderManager::initialize() noexcept {
    if (m_initialized) {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    Logger::info(kLogCategory, "Initializing reminder manager");

    m_initialized = true;

    // Load existing reminders
    if (!load()) {
        Logger::warning(kLogCategory, "Failed to load reminders, starting fresh");
    }

    m_currentState = ReminderState::IDLE;
    m_lastError = ReminderError::NONE;
    m_lastCheckTime = millis();

    Logger::info(kLogCategory, "Reminder manager initialized with %zu reminders", m_reminders.size());
    return true;
}

void ReminderManager::run() noexcept {
    update();
}

void ReminderManager::update() noexcept {
    if (!m_initialized) return;

    unsigned long now = millis();

    // Check reminders every second
    if (now - m_lastCheckTime >= kCheckIntervalMs) {
        m_lastCheckTime = now;
        checkReminders();
    }

    // Handle state machine timeouts
    switch (m_currentState) {
        case ReminderState::TRIGGERED: {
            // Find the active reminder being processed
            for (auto& r : m_reminders) {
                if (r.status == ReminderStatus::ACTIVE) {
                    triggerReminder(r);
                    break;
                }
            }
            break;
        }
        case ReminderState::SPEAKING: {
            // Check if TTS is still busy
            if (!textToSpeech.isBusy()) {
                changeState(ReminderState::COMPLETED);
            } else if (now - m_lastCheckTime > kTtsTimeoutMs) {
                Logger::warning(kLogCategory, "TTS timeout, completing reminder");
                changeState(ReminderState::COMPLETED);
            }
            break;
        }
        case ReminderState::COMPLETED: {
            // Auto-transition back to waiting
            changeState(ReminderState::WAITING);
            break;
        }
        case ReminderState::ERROR: {
            // Auto-transition back to waiting after error
            changeState(ReminderState::WAITING);
            break;
        }
        default:
            break;
    }
}

// ============================================================================
// Public API - Reminder Management
// ============================================================================

uint32_t ReminderManager::addReminder(
    const String& title,
    const String& message,
    time_t triggerTime,
    ReminderType type,
    ReminderPriority priority) noexcept {

    if (!m_initialized) {
        setError(ReminderError::UNKNOWN);
        return 0;
    }

    // Validate inputs
    if (title.isEmpty() || message.isEmpty()) {
        Logger::error(kLogCategory, "Cannot add reminder: empty title or message");
        setError(ReminderError::INVALID_TIME);
        return 0;
    }

    if (triggerTime == 0) {
        Logger::error(kLogCategory, "Cannot add reminder: invalid trigger time");
        setError(ReminderError::INVALID_TIME);
        return 0;
    }

    if (m_reminders.size() >= kMaxReminders) {
        Logger::error(kLogCategory, "Cannot add reminder: maximum reached (%u)", kMaxReminders);
        setError(ReminderError::FULL);
        return 0;
    }

    // Check for duplicate ID (shouldn't happen but safety check)
    uint32_t newId = generateId();
    for (const auto& r : m_reminders) {
        if (r.id == newId) {
            // Collision - find next available
            while (true) {
                newId = generateId();
                bool found = false;
                for (const auto& r2 : m_reminders) {
                    if (r2.id == newId) {
                        found = true;
                        break;
                    }
                }
                if (!found) break;
            }
            break;
        }
    }

    Reminder reminder;
    reminder.id = newId;
    reminder.title = title;
    reminder.message = message;
    reminder.triggerTime = triggerTime;
    reminder.type = type;
    reminder.priority = priority;
    reminder.status = ReminderStatus::PENDING;
    reminder.enabled = true;
    reminder.recurring = (type != ReminderType::ONCE);
    reminder.createdTime = time(nullptr);
    reminder.lastTriggeredTime = 0;

    m_reminders.push_back(reminder);
    sortReminders();

    Logger::info(kLogCategory, "Added reminder ID=%u: %s (trigger=%lu, type=%d)",
        newId, title.c_str(), static_cast<unsigned long>(triggerTime), static_cast<int>(type));

    // Save immediately
    save();

    return newId;
}

bool ReminderManager::removeReminder(uint32_t id) noexcept {
    if (!m_initialized) {
        setError(ReminderError::UNKNOWN);
        return false;
    }

    auto it = std::find_if(m_reminders.begin(), m_reminders.end(),
        [id](const Reminder& r) { return r.id == id; });

    if (it == m_reminders.end()) {
        Logger::warning(kLogCategory, "Reminder not found for removal: %u", id);
        setError(ReminderError::NOT_FOUND);
        return false;
    }

    Logger::info(kLogCategory, "Removed reminder ID=%u: %s", id, it->title.c_str());
    m_reminders.erase(it);
    save();
    return true;
}

bool ReminderManager::editReminder(
    uint32_t id,
    const String& title,
    const String& message,
    time_t triggerTime,
    ReminderType type,
    ReminderPriority priority) noexcept {

    if (!m_initialized) {
        setError(ReminderError::UNKNOWN);
        return false;
    }

    auto it = std::find_if(m_reminders.begin(), m_reminders.end(),
        [id](const Reminder& r) { return r.id == id; });

    if (it == m_reminders.end()) {
        Logger::warning(kLogCategory, "Reminder not found for edit: %u", id);
        setError(ReminderError::NOT_FOUND);
        return false;
    }

    bool changed = false;
    if (!title.isEmpty() && it->title != title) {
        it->title = title;
        changed = true;
    }
    if (!message.isEmpty() && it->message != message) {
        it->message = message;
        changed = true;
    }
    if (triggerTime != 0 && it->triggerTime != triggerTime) {
        if (triggerTime == 0) {
            setError(ReminderError::INVALID_TIME);
            return false;
        }
        it->triggerTime = triggerTime;
        changed = true;
    }
    if (type != ReminderType::ONCE && it->type != type) {
        it->type = type;
        it->recurring = (type != ReminderType::ONCE);
        changed = true;
    }
    if (priority != ReminderPriority::NORMAL && it->priority != priority) {
        it->priority = priority;
        changed = true;
    }

    if (changed) {
        sortReminders();
        Logger::info(kLogCategory, "Edited reminder ID=%u", id);
        save();
    }

    return true;
}

void ReminderManager::clearReminders() noexcept {
    if (!m_initialized) return;

    Logger::info(kLogCategory, "Clearing all %zu reminders", m_reminders.size());
    m_reminders.clear();
    save();
}

bool ReminderManager::enableReminder(uint32_t id) noexcept {
    if (!m_initialized) {
        setError(ReminderError::UNKNOWN);
        return false;
    }

    auto it = std::find_if(m_reminders.begin(), m_reminders.end(),
        [id](const Reminder& r) { return r.id == id; });

    if (it == m_reminders.end()) {
        setError(ReminderError::NOT_FOUND);
        return false;
    }

    if (!it->enabled) {
        it->enabled = true;
        Logger::info(kLogCategory, "Enabled reminder ID=%u", id);
        save();
    }
    return true;
}

bool ReminderManager::disableReminder(uint32_t id) noexcept {
    if (!m_initialized) {
        setError(ReminderError::UNKNOWN);
        return false;
    }

    auto it = std::find_if(m_reminders.begin(), m_reminders.end(),
        [id](const Reminder& r) { return r.id == id; });

    if (it == m_reminders.end()) {
        setError(ReminderError::NOT_FOUND);
        return false;
    }

    if (it->enabled) {
        it->enabled = false;
        it->status = ReminderStatus::PENDING;
        Logger::info(kLogCategory, "Disabled reminder ID=%u", id);
        save();
    }
    return true;
}

bool ReminderManager::acknowledgeReminder(uint32_t id) noexcept {
    if (!m_initialized) {
        setError(ReminderError::UNKNOWN);
        return false;
    }

    auto it = std::find_if(m_reminders.begin(), m_reminders.end(),
        [id](const Reminder& r) { return r.id == id; });

    if (it == m_reminders.end()) {
        setError(ReminderError::NOT_FOUND);
        return false;
    }

    if (it->status != ReminderStatus::ACTIVE && it->status != ReminderStatus::SNOOZED) {
        Logger::warning(kLogCategory, "Reminder ID=%u not in acknowledgeable state: %d", id, static_cast<int>(it->status));
        return false;
    }

    // Store history before changing state
    storeHistory(id, true);

    if (it->recurring) {
        // For recurring reminders, calculate next trigger time
        repeatReminder(*it);
    } else {
        // One-time reminder - mark completed
        it->status = ReminderStatus::COMPLETED;
        it->enabled = false;
    }

    // Stop TTS if currently speaking this reminder
    if (textToSpeech.isBusy()) {
        textToSpeech.stop();
    }

    Logger::info(kLogCategory, "Acknowledged reminder ID=%u", id);
    changeState(ReminderState::COMPLETED);
    save();
    return true;
}

bool ReminderManager::snoozeReminder(uint32_t id, uint32_t minutes) noexcept {
    if (!m_initialized) {
        setError(ReminderError::UNKNOWN);
        return false;
    }

    if (minutes == 0 || minutes > kMaxSnoozeMinutes) {
        Logger::error(kLogCategory, "Invalid snooze minutes: %u", minutes);
        setError(ReminderError::INVALID_TIME);
        return false;
    }

    auto it = std::find_if(m_reminders.begin(), m_reminders.end(),
        [id](const Reminder& r) { return r.id == id; });

    if (it == m_reminders.end()) {
        setError(ReminderError::NOT_FOUND);
        return false;
    }

    if (it->status != ReminderStatus::ACTIVE && it->status != ReminderStatus::SNOOZED) {
        Logger::warning(kLogCategory, "Reminder ID=%u not in snoozeable state: %d", id, static_cast<int>(it->status));
        return false;
    }

    // Stop TTS if currently speaking
    if (textToSpeech.isBusy()) {
        textToSpeech.stop();
    }

    // Calculate new trigger time (current time + minutes)
    time_t now = time(nullptr);
    it->triggerTime = now + (static_cast<time_t>(minutes) * 60);
    it->status = ReminderStatus::SNOOZED;
    it->lastTriggeredTime = now;

    sortReminders();

    Logger::info(kLogCategory, "Snoozed reminder ID=%u for %u minutes (new trigger=%lu)",
        id, minutes, static_cast<unsigned long>(it->triggerTime));

    changeState(ReminderState::WAITING);
    save();
    return true;
}

bool ReminderManager::getReminder(uint32_t id, Reminder& reminder) const noexcept {
    if (!m_initialized) return false;

    auto it = std::find_if(m_reminders.begin(), m_reminders.end(),
        [id](const Reminder& r) { return r.id == id; });

    if (it == m_reminders.end()) return false;

    reminder = *it;
    return true;
}

size_t ReminderManager::getReminders(std::vector<Reminder>& reminders) const noexcept {
    if (!m_initialized) return 0;

    reminders = m_reminders;
    return m_reminders.size();
}

size_t ReminderManager::getReminderCount() const noexcept {
    return m_reminders.size();
}

size_t ReminderManager::getHistory(std::vector<ReminderHistory>& history) const noexcept {
    if (!m_initialized) return 0;

    history = m_history;
    return m_history.size();
}

// ============================================================================
// Public API - Persistence
// ============================================================================

bool ReminderManager::save() noexcept {
    if (!m_initialized) return false;

    Logger::debug(kLogCategory, "Saving %zu reminders to storage", m_reminders.size());

    // Save each reminder as a separate file
    for (const auto& reminder : m_reminders) {
        String json = reminderToJson(reminder);
        String filename = String("/reminders/reminder_") + String(reminder.id) + ".json";

        StorageStatus status = storageManager.writeFile(
            filename.c_str(),
            reinterpret_cast<const uint8_t*>(json.c_str()),
            json.length(),
            StorageType::SPIFFS);

        if (status != StorageStatus::SUCCESS) {
            Logger::error(kLogCategory, "Failed to save reminder ID=%u: %d",
                reminder.id, static_cast<int>(status));
            setError(ReminderError::STORAGE_ERROR);
            return false;
        }
    }

    // Save history
    for (size_t i = 0; i < m_history.size(); ++i) {
        String json = historyToJson(m_history[i]);
        String filename = String("/reminders/history_") + String(i) + ".json";

        StorageStatus status = storageManager.writeFile(
            filename.c_str(),
            reinterpret_cast<const uint8_t*>(json.c_str()),
            json.length(),
            StorageType::SPIFFS);

        if (status != StorageStatus::SUCCESS) {
            Logger::error(kLogCategory, "Failed to save history entry %zu: %d", i, static_cast<int>(status));
        }
    }

    Logger::info(kLogCategory, "Saved %zu reminders and %zu history entries",
        m_reminders.size(), m_history.size());
    return true;
}

bool ReminderManager::load() noexcept {
    if (!m_initialized && !initialize()) {
        return false;
    }

    // Clear existing reminders
    m_reminders.clear();
    m_history.clear();

    // List reminder files
    std::vector<FileInfo> files;
    StorageStatus status = storageManager.listDirectory("/reminders", files, StorageType::SPIFFS);

    if (status != StorageStatus::SUCCESS) {
        Logger::warning(kLogCategory, "Failed to list reminder directory: %d", static_cast<int>(status));
        return true;  // Not an error - just no reminders yet
    }

    // Load each reminder file
    for (const auto& file : files) {
        if (!file.isDirectory && file.name.endsWith(".json") && file.name.startsWith("reminder_")) {
            String content;
            String path = "/reminders/" + file.name;
            status = storageManager.readFile(path.c_str(), content, StorageType::SPIFFS);

            if (status == StorageStatus::SUCCESS) {
                Reminder reminder;
                if (jsonToReminder(content, reminder)) {
                    // Only load enabled reminders that haven't expired (for non-recurring)
                    time_t now = time(nullptr);
                    if (reminder.enabled && (reminder.recurring || reminder.triggerTime > now)) {
                        m_reminders.push_back(reminder);
                        if (reminder.id >= m_nextId) {
                            m_nextId = reminder.id + 1;
                        }
                    }
                }
            }
        }
    }

    // Load history files
    for (const auto& file : files) {
        if (!file.isDirectory && file.name.endsWith(".json") && file.name.startsWith("history_")) {
            String content;
            String path = "/reminders/" + file.name;
            status = storageManager.readFile(path.c_str(), content, StorageType::SPIFFS);

            if (status == StorageStatus::SUCCESS) {
                ReminderHistory history;
                if (jsonToHistory(content, history)) {
                    m_history.push_back(history);
                }
            }
        }
    }

    sortReminders();

    Logger::info(kLogCategory, "Loaded %zu reminders and %zu history entries",
        m_reminders.size(), m_history.size());
    return true;
}

// ============================================================================
// Public API - State Queries
// ============================================================================

bool ReminderManager::isBusy() const noexcept {
    return m_currentState != ReminderState::IDLE && m_currentState != ReminderState::COMPLETED && m_currentState != ReminderState::ERROR;
}

bool ReminderManager::isInitialized() const noexcept {
    return m_initialized;
}

ReminderState ReminderManager::getState() const noexcept {
    return m_currentState;
}

ReminderError ReminderManager::getError() const noexcept {
    return m_lastError;
}

// ============================================================================
// Private Methods
// ============================================================================

void ReminderManager::changeState(ReminderState newState) noexcept {
    if (m_currentState == newState) return;

    static constexpr bool validTransition[6][6] = {
        // IDLE, WAITING, TRIGGERED, SPEAKING, COMPLETED, ERROR
        {0, 1, 0, 0, 0, 1},      // IDLE
        {0, 0, 1, 0, 0, 1},      // WAITING
        {0, 0, 0, 1, 0, 1},      // TRIGGERED
        {0, 0, 0, 0, 1, 1},      // SPEAKING
        {1, 1, 0, 0, 0, 1},      // COMPLETED
        {1, 1, 0, 0, 0, 0}       // ERROR
    };

    if (!validTransition[static_cast<uint8_t>(m_currentState)][static_cast<uint8_t>(newState)]) {
        Logger::warning(kLogCategory, "Invalid state transition %d -> %d",
            static_cast<int>(m_currentState), static_cast<int>(newState));
        return;
    }

    Logger::debug(kLogCategory, "State: %d -> %d",
        static_cast<int>(m_currentState), static_cast<int>(newState));
    m_currentState = newState;
}

void ReminderManager::setError(ReminderError error) noexcept {
    if (m_lastError == error) return;
    m_lastError = error;
    Logger::error(kLogCategory, "Error: %d", static_cast<int>(error));
}

void ReminderManager::checkReminders() noexcept {
    if (!m_initialized) return;

    time_t now = time(nullptr);
    bool anyTriggered = false;

    for (auto& reminder : m_reminders) {
        if (!reminder.enabled) continue;
        if (reminder.status != ReminderStatus::PENDING && reminder.status != ReminderStatus::SNOOZED) continue;

        // Check if it's time to trigger
        if (now >= reminder.triggerTime) {
            // Prevent duplicate triggers
            if (reminder.lastTriggeredTime == reminder.triggerTime && reminder.status == ReminderStatus::ACTIVE) {
                continue;
            }

            reminder.status = ReminderStatus::ACTIVE;
            reminder.lastTriggeredTime = reminder.triggerTime;
            anyTriggered = true;

            Logger::info(kLogCategory, "Reminder triggered ID=%u: %s", reminder.id, reminder.title.c_str());
        }
    }

    if (anyTriggered) {
        changeState(ReminderState::TRIGGERED);
    }
}

void ReminderManager::triggerReminder(const Reminder& reminder) noexcept {
    // Update display
    displayManager.showReminder(reminder.title, reminder.message);
    auraSystem.reminder();

    // Speak reminder
    String speechText = "Reminder: " + reminder.title + ". " + reminder.message;
    if (textToSpeech.speak(speechText, reminder.priority == ReminderPriority::CRITICAL || reminder.priority == ReminderPriority::HIGH_PRIORITY)) {
        changeState(ReminderState::SPEAKING);
        Logger::info(kLogCategory, "Speaking reminder ID=%u", reminder.id);
    } else {
        Logger::error(kLogCategory, "Failed to speak reminder ID=%u", reminder.id);
        setError(ReminderError::UNKNOWN);
        changeState(ReminderState::ERROR);
    }
}

void ReminderManager::repeatReminder(Reminder& reminder) noexcept {
    time_t nextTrigger = calculateNextTrigger(reminder);

    if (nextTrigger == 0 || nextTrigger <= reminder.triggerTime) {
        Logger::error(kLogCategory, "Failed to calculate next trigger for ID=%u, disabling", reminder.id);
        reminder.enabled = false;
        reminder.status = ReminderStatus::COMPLETED;
        return;
    }

    reminder.triggerTime = nextTrigger;
    reminder.status = ReminderStatus::PENDING;
    reminder.lastTriggeredTime = 0;

    sortReminders();

    Logger::info(kLogCategory, "Repeated reminder ID=%u, next trigger=%lu",
        reminder.id, static_cast<unsigned long>(nextTrigger));
}

void ReminderManager::cleanupExpired() noexcept {
    time_t now = time(nullptr);
    size_t removed = 0;

    auto it = m_reminders.begin();
    while (it != m_reminders.end()) {
        if (!it->enabled && it->status == ReminderStatus::COMPLETED) {
            // Remove completed one-time reminders older than 7 days
            if (!it->recurring && it->lastTriggeredTime > 0) {
                if (now - it->lastTriggeredTime > 7 * 24 * 3600) {
                    Logger::debug(kLogCategory, "Cleaning up old completed reminder ID=%u", it->id);
                    it = m_reminders.erase(it);
                    ++removed;
                    continue;
                }
            }
        }
        ++it;
    }

    // Limit history size
    if (m_history.size() > kMaxHistoryEntries) {
        m_history.erase(m_history.begin(), m_history.begin() + (m_history.size() - kMaxHistoryEntries));
    }

    if (removed > 0) {
        save();
    }
}

void ReminderManager::storeHistory(uint32_t reminderId, bool acknowledged) noexcept {
    ReminderHistory entry(reminderId, millis(), acknowledged);
    m_history.push_back(entry);

    // Limit history size
    if (m_history.size() > kMaxHistoryEntries) {
        m_history.erase(m_history.begin());
    }
}

void ReminderManager::sortReminders() noexcept {
    std::sort(m_reminders.begin(), m_reminders.end(),
        [](const Reminder& a, const Reminder& b) {
            // Sort by trigger time (earliest first)
            if (a.triggerTime != b.triggerTime) {
                return a.triggerTime < b.triggerTime;
            }
            // Then by priority (higher first)
            return static_cast<uint8_t>(a.priority) > static_cast<uint8_t>(b.priority);
        });
}

uint32_t ReminderManager::generateId() noexcept {
    return m_nextId++;
}

time_t ReminderManager::calculateNextTrigger(const Reminder& reminder) const noexcept {
    time_t baseTime = reminder.triggerTime;
    time_t now = time(nullptr);

    // Use lastTriggeredTime if available and more recent
    if (reminder.lastTriggeredTime > 0 && reminder.lastTriggeredTime > baseTime) {
        baseTime = reminder.lastTriggeredTime;
    }

    // Ensure we're calculating from a time in the past or present
    if (baseTime > now) {
        baseTime = now;
    }

    switch (reminder.type) {
        case ReminderType::DAILY:
            return addDays(baseTime, 1);

        case ReminderType::WEEKLY:
            return addDays(baseTime, 7);

        case ReminderType::MONTHLY:
            return addMonths(baseTime, 1);

        case ReminderType::YEARLY:
            return addYears(baseTime, 1);

        case ReminderType::ONCE:
        default:
            return 0;  // No repeat for one-time
    }
}