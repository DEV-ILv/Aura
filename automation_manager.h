#ifndef AURA_AUTOMATION_MANAGER_H
#define AURA_AUTOMATION_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

enum class AutoConditionType : uint8_t {
    TIME, WIFI_STATE, MEMORY_COUNT, GOAL_STATUS, HABIT_STATUS, SYSTEM_STATE
};

enum class AutoActionType : uint8_t {
    LED_SET, DISPLAY_SHOW, SOUND_PLAY, MEMORY_SAVE, SET_REMINDER, RUN_PLUGIN, CHANGE_SETTING
};

struct AutoCondition {
    AutoConditionType type;
    String param1;
    String param2;
    bool invert;

    AutoCondition() noexcept : type(AutoConditionType::TIME), invert(false) {}
};

struct AutoAction {
    AutoActionType type;
    String target;
    String value;

    AutoAction() noexcept : type(AutoActionType::LED_SET) {}
};

struct AutoScript {
    String id;
    String name;
    bool enabled;
    std::vector<AutoCondition> conditions;
    std::vector<AutoAction> actions;
    std::vector<AutoAction> elseActions;
    unsigned long createdAt;

    AutoScript() noexcept : enabled(true), createdAt(0) {}
};

struct NLPattern {
    String id;
    String pattern;
    String actionType;
    String actionParams;
    uint8_t priority;
    unsigned long createdAt;
    unsigned long lastMatched;
    uint32_t matchCount;

    NLPattern() noexcept : priority(0), createdAt(0), lastMatched(0), matchCount(0) {}
};

class AutomationManager {
public:
    AutomationManager() noexcept;
    ~AutomationManager() noexcept;

    AutomationManager(const AutomationManager&) = delete;
    AutomationManager& operator=(const AutomationManager&) = delete;
    AutomationManager(AutomationManager&&) = delete;
    AutomationManager& operator=(AutomationManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    [[nodiscard]] String createScript(const String& name) noexcept;
    [[nodiscard]] bool deleteScript(const String& id) noexcept;
    [[nodiscard]] bool enableScript(const String& id) noexcept;
    [[nodiscard]] bool disableScript(const String& id) noexcept;

    [[nodiscard]] bool addCondition(const String& scriptId, const AutoCondition& cond) noexcept;
    [[nodiscard]] bool addAction(const String& scriptId, const AutoAction& action) noexcept;
    [[nodiscard]] bool addElseAction(const String& scriptId, const AutoAction& action) noexcept;

    [[nodiscard]] bool evaluateScript(const String& scriptId) noexcept;
    [[nodiscard]] size_t evaluateAll() noexcept;

    [[nodiscard]] AutoScript getScript(const String& id) const noexcept;
    [[nodiscard]] std::vector<AutoScript> getEnabledScripts() const noexcept;
    [[nodiscard]] const std::vector<AutoScript>& getAllScripts() const noexcept;

    [[nodiscard]] size_t scriptCount() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;

    // NL Automation
    [[nodiscard]] bool addNLPattern(const NLPattern& pattern) noexcept;
    [[nodiscard]] bool removeNLPattern(const String& patternId) noexcept;
    [[nodiscard]] NLPattern matchNL(const String& text) const noexcept;
    [[nodiscard]] std::vector<NLPattern> getAllNLPatterns() const noexcept;
    [[nodiscard]] String parseNLToAction(const String& text) const noexcept;

private:
    static constexpr const char* kLogCategory = "AutomationManager";
    static constexpr size_t kMaxScripts = AUTO_MAX_SCRIPTS;

    String generateId() noexcept;
    size_t findScript(const String& id) const noexcept;
    bool evaluateCondition(const AutoCondition& cond) const noexcept;
    void executeAction(const AutoAction& action) noexcept;
    const char* conditionTypeToString(AutoConditionType t) const noexcept;
    AutoConditionType stringToConditionType(const String& s) const noexcept;
    const char* actionTypeToString(AutoActionType t) const noexcept;
    AutoActionType stringToActionType(const String& s) const noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<AutoScript> m_scripts;
    unsigned long m_lastEvaluation;
    unsigned long m_lastIdCounter;

    static constexpr size_t kMaxNLPatterns = 50;
    static constexpr const char* kNLPatternsPath = "/nl_patterns.json";

    std::vector<NLPattern> m_nlPatterns;

    void saveNLPatterns() noexcept;
    void loadNLPatterns() noexcept;
    bool wildcardMatch(const String& text, const String& pattern) const noexcept;
    size_t findNLPattern(const String& id) const noexcept;
};

extern AutomationManager automationManager;

#endif
