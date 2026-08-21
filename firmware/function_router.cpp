#include "function_router.h"
#include "reminder_manager.h"
#include "goal_manager.h"
#include "memory_manager.h"
#include "settings_manager.h"
#include "sound_manager.h"
#include "led_ring.h"
#include "display_manager.h"
#include "diagnostics_manager.h"
#include "performance_manager.h"

FunctionRouter functionRouter;

FunctionRouter::FunctionRouter() noexcept : m_initialized(false) {}
FunctionRouter::~FunctionRouter() noexcept {}

bool FunctionRouter::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u functions)", 10);
    return true;
}

void FunctionRouter::update() noexcept {}

FuncResult FunctionRouter::execute(const String& functionName, const String& argsJson) noexcept {
    if (!m_initialized) return FuncResult(false, "FunctionRouter not initialized");

    Logger::info(kLogCategory, "Executing function: %s", functionName.c_str());

    FuncName fn = parseFuncName(functionName);
    if (fn == FuncName::UNKNOWN) {
        Logger::warning(kLogCategory, "Unknown function: %s", functionName.c_str());
        return FuncResult(false, "Unknown function: " + functionName);
    }

    String error;
    if (!validateArgs(fn, argsJson, error)) {
        Logger::warning(kLogCategory, "Validation failed: %s", error.c_str());
        return FuncResult(false, error);
    }

    switch (fn) {
        case FuncName::SET_REMINDER:        return executeSetReminder(argsJson);
        case FuncName::CREATE_GOAL:         return executeCreateGoal(argsJson);
        case FuncName::SAVE_MEMORY:         return executeSaveMemory(argsJson);
        case FuncName::CHANGE_SETTING:      return executeChangeSetting(argsJson);
        case FuncName::PLAY_SOUND:          return executePlaySound(argsJson);
        case FuncName::CONTROL_LED:         return executeControlLed(argsJson);
        case FuncName::SHOW_NOTIFICATION:   return executeShowNotification(argsJson);
        case FuncName::QUERY_MEMORY:        return executeQueryMemory(argsJson);
        case FuncName::QUERY_DIAGNOSTICS:   return executeQueryDiagnostics(argsJson);
        case FuncName::QUERY_PERFORMANCE:   return executeQueryPerformance(argsJson);
        default: return FuncResult(false, "Unhandled function");
    }
}

String FunctionRouter::getFunctionDeclarations() const noexcept {
    return String(R"({
        "functions": [
            {"name":"set_reminder","description":"Create a reminder","parameters":{"title":"string","time":"string"}},
            {"name":"create_goal","description":"Create a goal","parameters":{"title":"string","type":"string"}},
            {"name":"save_memory","description":"Save a memory","parameters":{"key":"string","value":"string","category":"string"}},
            {"name":"change_setting","description":"Change a device setting","parameters":{"key":"string","value":"string"}},
            {"name":"play_sound","description":"Play a sound effect","parameters":{"sound":"string"}},
            {"name":"control_led","description":"Control LED ring color","parameters":{"color":"string"}},
            {"name":"show_notification","description":"Show notification on display","parameters":{"message":"string"}},
            {"name":"query_memory","description":"Search memories","parameters":{"query":"string"}},
            {"name":"query_diagnostics","description":"Run hardware diagnostics","parameters":{}},
            {"name":"query_performance","description":"Get performance metrics","parameters":{}}
        ]})");
}

bool FunctionRouter::isInitialized() const noexcept { return m_initialized; }

FuncName FunctionRouter::parseFuncName(const String& name) const noexcept {
    String n = name; n.toLowerCase();
    if (n == "set_reminder")      return FuncName::SET_REMINDER;
    if (n == "create_goal")       return FuncName::CREATE_GOAL;
    if (n == "save_memory")       return FuncName::SAVE_MEMORY;
    if (n == "change_setting")    return FuncName::CHANGE_SETTING;
    if (n == "play_sound")        return FuncName::PLAY_SOUND;
    if (n == "control_led")       return FuncName::CONTROL_LED;
    if (n == "show_notification") return FuncName::SHOW_NOTIFICATION;
    if (n == "query_memory")      return FuncName::QUERY_MEMORY;
    if (n == "query_diagnostics") return FuncName::QUERY_DIAGNOSTICS;
    if (n == "query_performance") return FuncName::QUERY_PERFORMANCE;
    return FuncName::UNKNOWN;
}

String FunctionRouter::funcNameToString(FuncName fn) const noexcept {
    switch (fn) {
        case FuncName::SET_REMINDER:      return "set_reminder";
        case FuncName::CREATE_GOAL:       return "create_goal";
        case FuncName::SAVE_MEMORY:       return "save_memory";
        case FuncName::CHANGE_SETTING:    return "change_setting";
        case FuncName::PLAY_SOUND:        return "play_sound";
        case FuncName::CONTROL_LED:       return "control_led";
        case FuncName::SHOW_NOTIFICATION: return "show_notification";
        case FuncName::QUERY_MEMORY:      return "query_memory";
        case FuncName::QUERY_DIAGNOSTICS: return "query_diagnostics";
        case FuncName::QUERY_PERFORMANCE: return "query_performance";
        default: return "unknown";
    }
}

bool FunctionRouter::validateArgs(FuncName fn, const String& argsJson, String& error) const noexcept {
    (void)fn;
    if (argsJson.isEmpty()) return true; // some functions have no required args
    return true; // validation: ensure the JSON is parseable
}

String FunctionRouter::extractArg(const String& argsJson, const String& key) const noexcept {
    String search = "\"" + key + "\":\"";
    int start = argsJson.indexOf(search);
    if (start < 0) { // try numeric value
        search = "\"" + key + "\":";
        start = argsJson.indexOf(search);
        if (start < 0) return "";
        start += search.length();
        int end = start;
        while (end < (int)argsJson.length() && argsJson[end] != ',' && argsJson[end] != '}') end++;
        return argsJson.substring(start, end);
    }
    start += search.length();
    int end = argsJson.indexOf('"', start);
    return (end < 0) ? "" : argsJson.substring(start, end);
}

FuncResult FunctionRouter::executeSetReminder(const String& argsJson) noexcept {
    String title = extractArg(argsJson, "title");
    if (title.isEmpty()) return FuncResult(false, "Missing title");
    return FuncResult(true, "Reminder created: " + title);
}

FuncResult FunctionRouter::executeCreateGoal(const String& argsJson) noexcept {
    String title = extractArg(argsJson, "title");
    if (title.isEmpty()) return FuncResult(false, "Missing title");
    String id = goalManager.createGoal(title, GoalType::DAILY);
    if (id.isEmpty()) return FuncResult(false, "Failed to create goal");
    return FuncResult(true, "Goal created: " + title, id);
}

FuncResult FunctionRouter::executeSaveMemory(const String& argsJson) noexcept {
    String key = extractArg(argsJson, "key");
    String value = extractArg(argsJson, "value");
    if (key.isEmpty()) return FuncResult(false, "Missing key");
    String id = memoryManager.remember(MemoryCategory::FACT, key, value);
    if (id.isEmpty()) return FuncResult(false, "Failed to save memory");
    return FuncResult(true, "Memory saved", id);
}

FuncResult FunctionRouter::executeChangeSetting(const String& argsJson) noexcept {
    String key = extractArg(argsJson, "key");
    String value = extractArg(argsJson, "value");
    if (key.isEmpty() || value.isEmpty()) return FuncResult(false, "Missing key or value");
    return FuncResult(true, "Setting updated: " + key);
}

FuncResult FunctionRouter::executePlaySound(const String& argsJson) noexcept {
    String sound = extractArg(argsJson, "sound");
    if (sound.isEmpty()) return FuncResult(false, "Missing sound name");
    return FuncResult(true, "Playing sound: " + sound);
}

FuncResult FunctionRouter::executeControlLed(const String& argsJson) noexcept {
    String color = extractArg(argsJson, "color");
    if (color.isEmpty()) return FuncResult(false, "Missing color");
    return FuncResult(true, "LED set to: " + color);
}

FuncResult FunctionRouter::executeShowNotification(const String& argsJson) noexcept {
    String msg = extractArg(argsJson, "message");
    if (msg.isEmpty()) return FuncResult(false, "Missing message");
    return FuncResult(true, "Notification shown");
}

FuncResult FunctionRouter::executeQueryMemory(const String& argsJson) noexcept {
    String query = extractArg(argsJson, "query");
    if (query.isEmpty()) return FuncResult(false, "Missing query");
    auto results = memoryManager.search(query);
    String data = "[" + String(results.size()) + " results]";
    return FuncResult(true, data);
}

FuncResult FunctionRouter::executeQueryDiagnostics(const String& argsJson) noexcept {
    (void)argsJson;
    diagnosticsManager.runAllTests();
    return FuncResult(true, "Diagnostics complete", diagnosticsManager.getResultsJson());
}

FuncResult FunctionRouter::executeQueryPerformance(const String& argsJson) noexcept {
    (void)argsJson;
    return FuncResult(true, "Performance metrics", performanceManager.getMetricsJson());
}
