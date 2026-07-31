#ifndef AURA_COMMAND_PALETTE_H
#define AURA_COMMAND_PALETTE_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "service.h"

enum class CommandCategory : uint8_t {
    NAVIGATION,
    SYSTEM,
    MEMORY,
    KNOWLEDGE,
    STUDIES,
    PROJECTS,
    REMINDERS,
    ANALYTICS,
    SETTINGS,
    DEVICE,
    AI,
    WORKSPACE,
    SCENE,
    CUSTOM
};

struct Command {
    String id;
    String name;
    String description;
    String keywords;   // Space-separated search keywords
    CommandCategory category;
    bool visible;       // Show in command palette UI
    uint8_t minArgs;
    uint8_t maxArgs;

    Command() noexcept : category(CommandCategory::CUSTOM), visible(true), minArgs(0), maxArgs(0) {}
    Command(const String& id_, const String& name_, const String& desc_, const String& keywords_,
            CommandCategory cat_, bool visible_, uint8_t minArgs_, uint8_t maxArgs_) noexcept
        : id(id_), name(name_), description(desc_), keywords(keywords_),
          category(cat_), visible(visible_), minArgs(minArgs_), maxArgs(maxArgs_) {}
};

struct CommandResult {
    bool success;
    String message;
    String data;  // Optional payload

    CommandResult() noexcept : success(false) {}
    CommandResult(bool ok, const String& msg) noexcept : success(ok), message(msg) {}
};

using CommandHandler = std::function<CommandResult(const std::vector<String>& args)>;

class CommandPalette : public Service {
public:
    CommandPalette() noexcept;
    ~CommandPalette() noexcept;

    bool Initialize() noexcept override;
    void Update() noexcept override;

    // Registration
    bool RegisterCommand(const Command& cmd, CommandHandler handler) noexcept;
    bool UnregisterCommand(const String& id) noexcept;

    // Execution
    CommandResult Execute(const String& commandLine) noexcept;
    CommandResult Execute(const String& id, const std::vector<String>& args) noexcept;

    // Search
    std::vector<Command> Search(const String& query) const noexcept;
    std::vector<Command> GetByCategory(CommandCategory category) const noexcept;
    std::vector<Command> GetAllCommands() const noexcept;

    // Query
    Command FindCommand(const String& id) const noexcept;
    size_t GetCommandCount() const noexcept;

    // Built-in commands
    void RegisterBuiltins() noexcept;

    void HandleEvent(const String& eventType, const String& eventData) noexcept override;

    static constexpr const char* kStaticName = "CommandPalette";

private:
    struct CommandEntry {
        Command cmd;
        CommandHandler handler;
    };

    int FindCommandIndex(const String& id) const noexcept;
    std::vector<String> ParseArgs(const String& commandLine) const noexcept;

    static constexpr const char* kLogCategory = "CommandPalette";
    static constexpr size_t kMaxCommands = 128;

    std::vector<CommandEntry> m_commands;
    bool m_initialized;
};

extern CommandPalette commandPalette;

#endif