#ifndef AURA_FUNCTION_ROUTER_H
#define AURA_FUNCTION_ROUTER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"

enum class FuncName : uint8_t {
    SET_REMINDER, CREATE_GOAL, SAVE_MEMORY, CHANGE_SETTING,
    PLAY_SOUND, CONTROL_LED, SHOW_NOTIFICATION,
    QUERY_MEMORY, QUERY_DIAGNOSTICS, QUERY_PERFORMANCE,
    UNKNOWN
};

struct FuncCall {
    FuncName name;
    String rawName;
    std::vector<String> argKeys;
    std::vector<String> argValues;

    FuncCall() noexcept : name(FuncName::UNKNOWN) {}
};

struct FuncResult {
    bool success;
    String message;
    String data;

    FuncResult() noexcept : success(false) {}
    FuncResult(bool success, const String& message, const String& data = "") noexcept
        : success(success), message(message), data(data) {}
};

class FunctionRouter {
public:
    FunctionRouter() noexcept;
    ~FunctionRouter() noexcept;

    FunctionRouter(const FunctionRouter&) = delete;
    FunctionRouter& operator=(const FunctionRouter&) = delete;
    FunctionRouter(FunctionRouter&&) = delete;
    FunctionRouter& operator=(FunctionRouter&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    /**
     * @brief Parse a Gemini function call response and execute it
     * @param functionName Raw function name string
     * @param argsJson JSON object with arguments
     * @return FuncResult with success status
     */
    [[nodiscard]] FuncResult execute(const String& functionName, const String& argsJson) noexcept;

    /**
     * @brief Get available functions schema for Gemini API
     * @return JSON string of function declarations
     */
    [[nodiscard]] String getFunctionDeclarations() const noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;

private:
    static constexpr const char* kLogCategory = "FunctionRouter";

    FuncName parseFuncName(const String& name) const noexcept;
    String funcNameToString(FuncName fn) const noexcept;
    bool validateArgs(FuncName fn, const String& argsJson, String& error) const noexcept;
    FuncResult executeSetReminder(const String& argsJson) noexcept;
    FuncResult executeCreateGoal(const String& argsJson) noexcept;
    FuncResult executeSaveMemory(const String& argsJson) noexcept;
    FuncResult executeChangeSetting(const String& argsJson) noexcept;
    FuncResult executePlaySound(const String& argsJson) noexcept;
    FuncResult executeControlLed(const String& argsJson) noexcept;
    FuncResult executeShowNotification(const String& argsJson) noexcept;
    FuncResult executeQueryMemory(const String& argsJson) noexcept;
    FuncResult executeQueryDiagnostics(const String& argsJson) noexcept;
    FuncResult executeQueryPerformance(const String& argsJson) noexcept;
    String extractArg(const String& argsJson, const String& key) const noexcept;

    bool m_initialized;
};

extern FunctionRouter functionRouter;

#endif
