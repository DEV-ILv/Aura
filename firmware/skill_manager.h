#ifndef AURA_SKILL_MANAGER_H
#define AURA_SKILL_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

/**
 * @enum SkillActionType
 * @brief Types of actions a skill can perform
 */
enum class SkillActionType : uint8_t {
    SHOW_DISPLAY,   ///< Show message on OLED
    LED,            ///< Set LED color
    SET_REMINDER,   ///< Create a reminder
    PLAY_SOUND,     ///< Play a sound
    STOP_CONVERSATION, ///< Stop conversation
    CUSTOM             ///< Custom string action
};

/**
 * @struct SkillAction
 * @brief A single action within a skill
 */
struct SkillAction {
    SkillActionType type;       ///< Action type
    String target;              ///< Target parameter (color, sound, etc.)
    String value;               ///< Value parameter (message, duration, etc.)
    String metadata;  // JSON string for extended action parameters

    SkillAction() noexcept : type(SkillActionType::CUSTOM) {}
};

/**
 * @enum SkillConditionType
 * @brief Types of conditions for skill execution
 */
enum class SkillConditionType : uint8_t {
    NONE,           ///< No condition
    TIME_RANGE,     ///< Within time range
    WIFI_STATE,     ///< WiFi connected/disconnected
    SYSTEM_STATE    ///< System in specific state
};

/**
 * @struct SkillCondition
 * @brief Condition for skill execution
 */
struct SkillCondition {
    SkillConditionType type;    ///< Condition type
    String param1;              ///< First parameter
    String param2;              ///< Second parameter

    SkillCondition() noexcept : type(SkillConditionType::NONE) {}
};

/**
 * @struct SkillEntry
 * @brief A custom skill definition
 */
struct SkillEntry {
    String id;                  ///< Unique identifier
    String name;                ///< Display name
    String voiceTrigger;        ///< Voice trigger phrase
    String description;         ///< Description
    SkillAction actions[SKILL_ACTIONS_MAX];  ///< Actions to execute
    SkillCondition conditions[SKILL_CONDITIONS_MAX];  ///< Execution conditions
    uint8_t actionCount;        ///< Number of actions
    uint8_t conditionCount;     ///< Number of conditions
    uint8_t priority;           ///< Priority (0-255, higher = more important)
    bool enabled;               ///< Whether skill is active
    unsigned long createdAt;    ///< Creation timestamp
    unsigned long lastTriggered; ///< Last execution timestamp
    uint32_t triggerCount;      ///< Number of times triggered
    String category;          // Skill category (e.g. "productivity", "entertainment")
    String icon;              // Icon identifier for UI
    String author;            // Author name
    String version;           // Version string
    String tags;              // Comma-separated tags
    uint8_t rating;           // User rating (0-5)
    bool isTemplate;          // Can be used as template
    String dependencies;      // Comma-separated skill IDs this depends on
    unsigned long modifiedAt; // Last modification time

    SkillEntry() noexcept
        : actionCount(0), conditionCount(0), priority(0),
          enabled(true), createdAt(0), lastTriggered(0), triggerCount(0),
          rating(0), isTemplate(false), modifiedAt(0) {}
};

/**
 * @class SkillManager
 * @brief Manages custom user skills with voice triggers and actions
 *
 * Skills are stored as JSON files on SD card under /skills/.
 * Each skill defines a voice trigger, conditions, and actions.
 */
class SkillManager {
public:
    SkillManager() noexcept;
    ~SkillManager() noexcept;

    SkillManager(const SkillManager&) = delete;
    SkillManager& operator=(const SkillManager&) = delete;
    SkillManager(SkillManager&&) = delete;
    SkillManager& operator=(SkillManager&&) = delete;

    /**
     * @brief Initialize skill manager
     * @return true if initialized
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Update skill manager
     */
    void update() noexcept;

    /**
     * @brief Find a skill by voice trigger text
     * @param text Voice input text
     * @return Matching SkillEntry (empty id if not found)
     */
    [[nodiscard]] SkillEntry matchTrigger(const String& text) const noexcept;

    /**
     * @brief Log skill execution
     * @param skillId Skill that was executed
     * @param success Whether execution was successful
     */
    void logExecution(const String& skillId, bool success) noexcept;

    /**
     * @brief Add a new skill
     * @param skill The skill to add
     * @return true if added
     */
    [[nodiscard]] bool addSkill(const SkillEntry& skill) noexcept;

    /**
     * @brief Remove a skill by ID
     * @param skillId Skill ID
     * @return true if removed
     */
    [[nodiscard]] bool removeSkill(const String& skillId) noexcept;

    /**
     * @brief Enable a skill
     * @param skillId Skill ID
     * @return true if enabled
     */
    [[nodiscard]] bool enableSkill(const String& skillId) noexcept;

    /**
     * @brief Disable a skill
     * @param skillId Skill ID
     * @return true if disabled
     */
    [[nodiscard]] bool disableSkill(const String& skillId) noexcept;

    /**
     * @brief Get all skills
     * @return Vector of all skills
     */
    [[nodiscard]] const std::vector<SkillEntry>& getAllSkills() const noexcept;

    /**
     * @brief Get a skill by ID
     * @param skillId Skill ID
     * @return SkillEntry (empty id if not found)
     */
    [[nodiscard]] SkillEntry getSkill(const String& skillId) const noexcept;

    /**
     * @brief Check if initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Get total skill count
     * @return Number of skills
     */
    [[nodiscard]] size_t skillCount() const noexcept;

    // Skill Studio
    [[nodiscard]] bool updateSkill(const String& skillId, const SkillEntry& updates) noexcept;
    [[nodiscard]] bool duplicateSkill(const String& skillId) noexcept;
    [[nodiscard]] bool importSkill(const String& json) noexcept;
    [[nodiscard]] String exportSkill(const String& skillId) const noexcept;
    [[nodiscard]] std::vector<SkillEntry> getSkillsByCategory(const String& category) const noexcept;
    [[nodiscard]] std::vector<SkillEntry> searchSkills(const String& query) const noexcept;
    [[nodiscard]] std::vector<SkillEntry> getTemplates() const noexcept;
    void recordExecution(const String& skillId, bool success, unsigned long durationMs) noexcept;

    /**
     * @brief Save all skills to storage
     * @return true if saved
     */
    [[nodiscard]] bool save() noexcept;

    /**
     * @brief Load all skills from storage
     * @return true if loaded
     */
    [[nodiscard]] bool load() noexcept;

private:
    static constexpr const char* kLogCategory = "SkillManager";
    static constexpr const char* kStoragePath = "/skills.json";
    static constexpr size_t kMaxSkills = SKILL_MAX_COUNT;

    String generateId() noexcept;
    size_t findSkill(const String& id) const noexcept;
    SkillActionType parseActionType(const String& str) const noexcept;
    String actionTypeToString(SkillActionType type) const noexcept;
    SkillConditionType parseConditionType(const String& str) const noexcept;
    String conditionTypeToString(SkillConditionType type) const noexcept;
    bool parseSkillJson(const String& json, SkillEntry& skill) const noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<SkillEntry> m_skills;
    unsigned long m_lastIdCounter;

    static constexpr size_t kMaxExecutionLog = 200;
    static constexpr size_t kStudioMaxSkills = 100;

    struct ExecutionRecord {
        String skillId;
        bool success;
        unsigned long timestamp;
        unsigned long durationMs;
        ExecutionRecord() noexcept : success(false), timestamp(0), durationMs(0) {}
    };

    void saveExecutionLog() noexcept;
    void loadExecutionLog() noexcept;
    size_t findExecutionRecord(const String& skillId, unsigned long since) const noexcept;

    std::vector<ExecutionRecord> m_executionLog;
};

extern SkillManager skillManager;

#endif // AURA_SKILL_MANAGER_H
