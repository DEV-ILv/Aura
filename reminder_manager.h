#ifndef AURA_REMINDER_MANAGER_H
#define AURA_REMINDER_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <ctime>
#include "storage_manager.h"
#include "text_to_speech.h"
#include "display_manager.h"
#include "conversation_manager.h"
#include "config.h"
#include "logger.h"

/**
 * @enum ReminderState
 * @brief Reminder manager operational states
 */
enum class ReminderState : uint8_t {
    IDLE,           ///< No active reminder processing
    WAITING,        ///< Waiting for next reminder trigger time
    TRIGGERED,      ///< Reminder triggered, preparing notification
    SPEAKING,       ///< TextToSpeech speaking reminder
    COMPLETED,      ///< Reminder notification completed
    ERROR           ///< Error occurred
};

/**
 * @enum ReminderType
 * @brief Reminder recurrence types
 */
enum class ReminderType : uint8_t {
    ONCE,           ///< One-time reminder
    DAILY,          ///< Daily recurring reminder
    WEEKLY,         ///< Weekly recurring reminder
    MONTHLY,        ///< Monthly recurring reminder
    YEARLY          ///< Yearly recurring reminder
};

/**
 * @enum ReminderPriority
 * @brief Reminder priority levels
 */
enum class ReminderPriority : uint8_t {
    LOW_PRIORITY,     ///< Low priority
    NORMAL,           ///< Normal priority
    HIGH_PRIORITY,    ///< High priority
    CRITICAL          ///< Critical priority
};

/**
 * @enum ReminderStatus
 * @brief Individual reminder lifecycle status
 */
enum class ReminderStatus : uint8_t {
    PENDING,        ///< Waiting for trigger time
    ACTIVE,         ///< Currently triggered and awaiting acknowledgement
    SNOOZED,        ///< Temporarily postponed by user
    COMPLETED,      ///< Acknowledged and completed
    DISMISSED       ///< Dismissed without completion
};

/**
 * @enum ReminderError
 * @brief Reminder manager error codes
 */
enum class ReminderError : uint8_t {
    NONE,           ///< No error
    INVALID_TIME,   ///< Invalid trigger time
    INVALID_DATE,   ///< Invalid date
    STORAGE_ERROR,  ///< Storage operation failed
    FULL,           ///< Maximum reminders reached
    NOT_FOUND,      ///< Reminder not found
    UNKNOWN         ///< Unspecified error
};

/**
 * @struct Reminder
 * @brief Reminder data structure
 */
struct Reminder {
    uint32_t id;                    ///< Unique reminder identifier
    String title;                   ///< Reminder title
    String message;                 ///< Reminder message
    time_t triggerTime;             ///< Unix timestamp for next trigger
    ReminderType type;              ///< Recurrence type
    ReminderPriority priority;      ///< Priority level
    ReminderStatus status;          ///< Current reminder status
    bool enabled;                   ///< Reminder enabled flag
    bool recurring;                 ///< Recurring reminder flag
    time_t createdTime;             ///< Creation timestamp (Unix time)
    time_t lastTriggeredTime;       ///< Last trigger timestamp (Unix time)

    Reminder() noexcept
        : id(0), title(""), message(""), triggerTime(0),
          type(ReminderType::ONCE), priority(ReminderPriority::NORMAL),
          status(ReminderStatus::PENDING), enabled(true), recurring(false),
          createdTime(0), lastTriggeredTime(0) {}
};

/**
 * @struct ReminderHistory
 * @brief Reminder history entry
 */
struct ReminderHistory {
    uint32_t reminderId;            ///< Reminder identifier
    unsigned long timestamp;        ///< Trigger timestamp (millis)
    bool acknowledged;              ///< User acknowledged flag

    ReminderHistory() noexcept
        : reminderId(0), timestamp(0), acknowledged(false) {}
    ReminderHistory(uint32_t id, unsigned long ts, bool ack) noexcept
        : reminderId(id), timestamp(ts), acknowledged(ack) {}
};

/**
 * @class ReminderManager
 * @brief Single authority for reminder management and scheduling
 *
 * Manages:
 * - Reminder creation, editing, deletion
 * - One-time and recurring reminders (daily, weekly, monthly, yearly)
 * - Persistent storage via StorageManager
 * - Notification via TextToSpeech and DisplayManager
 * - Integration with ConversationManager
 * - Reminder history tracking
 * - Snooze functionality for temporary postponement
 * - Non-blocking scheduler with millisecond precision
 *
 * Thread-safe for main loop access. ESP32-optimized.
 */
class ReminderManager {
public:
    /**
     * @brief Constructor
     */
    ReminderManager() noexcept;

    /**
     * @brief Destructor
     */
    ~ReminderManager() noexcept;

    // Delete copy semantics
    ReminderManager(const ReminderManager&) = delete;
    ReminderManager& operator=(const ReminderManager&) = delete;

    // Delete move semantics
    ReminderManager(ReminderManager&&) = delete;
    ReminderManager& operator=(ReminderManager&&) = delete;

    /**
     * @brief Initialize reminder manager
     * @return true if initialization successful, false otherwise
     * @note Should be called once during setup()
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Scheduler-compatible update method
     * @note For compatibility with task schedulers
     */
    void run() noexcept;

    /**
     * @brief Update reminder manager state machine
     * @note Should be called regularly from loop()
     */
    void update() noexcept;

    /**
     * @brief Add a new reminder
     * @param title Reminder title
     * @param message Reminder message
     * @param triggerTime Unix timestamp for first trigger
     * @param type Recurrence type
     * @param priority Priority level
     * @return Reminder ID if successful, 0 on failure
     */
    [[nodiscard]] uint32_t addReminder(
        const String& title,
        const String& message,
        time_t triggerTime,
        ReminderType type = ReminderType::ONCE,
        ReminderPriority priority = ReminderPriority::NORMAL) noexcept;

    /**
     * @brief Remove a reminder by ID
     * @param id Reminder identifier
     * @return true if removed, false if not found
     */
    [[nodiscard]] bool removeReminder(uint32_t id) noexcept;

    /**
     * @brief Edit an existing reminder
     * @param id Reminder identifier
     * @param title New title (empty = unchanged)
     * @param message New message (empty = unchanged)
     * @param triggerTime New trigger time (0 = unchanged)
     * @param type New type (ReminderType::ONCE = unchanged)
     * @param priority New priority (ReminderPriority::NORMAL = unchanged)
     * @return true if edited, false if not found
     */
    [[nodiscard]] bool editReminder(
        uint32_t id,
        const String& title = "",
        const String& message = "",
        time_t triggerTime = 0,
        ReminderType type = ReminderType::ONCE,
        ReminderPriority priority = ReminderPriority::NORMAL) noexcept;

    /**
     * @brief Clear all reminders
     */
    void clearReminders() noexcept;

    /**
     * @brief Enable a reminder
     * @param id Reminder identifier
     * @return true if enabled, false if not found
     */
    [[nodiscard]] bool enableReminder(uint32_t id) noexcept;

    /**
     * @brief Disable a reminder
     * @param id Reminder identifier
     * @return true if disabled, false if not found
     */
    [[nodiscard]] bool disableReminder(uint32_t id) noexcept;

    /**
     * @brief Acknowledge a triggered reminder
     * @param id Reminder identifier
     * @return true if acknowledged, false if not found
     */
    [[nodiscard]] bool acknowledgeReminder(uint32_t id) noexcept;

    /**
     * @brief Snooze a reminder by specified minutes
     * @param id Reminder identifier
     * @param minutes Minutes to postpone (1-1440)
     * @return true if snoozed, false if not found or invalid minutes
     * @note Postpones triggerTime without changing recurrence settings
     */
    [[nodiscard]] bool snoozeReminder(uint32_t id, uint32_t minutes) noexcept;

    /**
     * @brief Get a reminder by ID
     * @param id Reminder identifier
     * @param reminder Output reference to reminder
     * @return true if found, false otherwise
     */
    [[nodiscard]] bool getReminder(uint32_t id, Reminder& reminder) const noexcept;

    /**
     * @brief Get all reminders
     * @param reminders Output vector of reminders
     * @return Number of reminders
     */
    [[nodiscard]] size_t getReminders(std::vector<Reminder>& reminders) const noexcept;

    /**
     * @brief Get reminder count
     * @return Number of active reminders
     */
    [[nodiscard]] size_t getReminderCount() const noexcept;

    /**
     * @brief Get reminder history
     * @param history Output vector of history entries
     * @return Number of history entries
     */
    [[nodiscard]] size_t getHistory(std::vector<ReminderHistory>& history) const noexcept;

    /**
     * @brief Save reminders to persistent storage
     * @return true if saved successfully
     */
    [[nodiscard]] bool save() noexcept;

    /**
     * @brief Load reminders from persistent storage
     * @return true if loaded successfully
     */
    [[nodiscard]] bool load() noexcept;

    /**
     * @brief Check if manager is busy processing
     * @return true if not IDLE
     */
    [[nodiscard]] bool isBusy() const noexcept;

    /**
     * @brief Check if manager is initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Get current state
     * @return Current ReminderState
     */
    [[nodiscard]] ReminderState getState() const noexcept;

    /**
     * @brief Get last error code
     * @return Current ReminderError
     */
    [[nodiscard]] ReminderError getError() const noexcept;

private:
    // Constants
    static constexpr size_t kMaxReminders = 32;
    static constexpr size_t kMaxHistoryEntries = 50;
    static constexpr unsigned long kCheckIntervalMs = 1000UL;
    static constexpr uint16_t kStorageVersion = 1;  ///< Storage format version for forward compatibility
    static constexpr const char* kLogCategory = "ReminderManager";

    // Private methods
    void changeState(ReminderState newState) noexcept;
    void setError(ReminderError error) noexcept;
    void checkReminders() noexcept;
    void triggerReminder(const Reminder& reminder) noexcept;
    void repeatReminder(Reminder& reminder) noexcept;
    void cleanupExpired() noexcept;
    void storeHistory(uint32_t reminderId, bool acknowledged) noexcept;
    void sortReminders() noexcept;
    uint32_t generateId() noexcept;
    time_t calculateNextTrigger(const Reminder& reminder) const noexcept;

    // Member variables
    bool m_initialized;
    ReminderState m_currentState;
    ReminderError m_lastError;
    std::vector<Reminder> m_reminders;
    std::vector<ReminderHistory> m_history;
    uint32_t m_nextId;
    unsigned long m_lastCheckTime;
};

/**
 * @brief Global reminder manager instance
 */
extern ReminderManager reminderManager;

#endif // AURA_REMINDER_MANAGER_H