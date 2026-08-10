#include "command_palette.h"
#include <algorithm>
#include <new>
#include <WiFi.h>
#include "ui_framework.h"
#include "system_manager.h"
#include "memory_manager.h"
#include "study_manager.h"
#include "reminder_manager.h"

CommandPalette commandPalette;

CommandPalette::CommandPalette() noexcept
    : Service(kStaticName, BootPriority::NORMAL)
    , m_initialized(false) {
}

CommandPalette::~CommandPalette() noexcept = default;

bool CommandPalette::Initialize() noexcept {
    if (GetState() != ServiceState::UNINITIALIZED) return true;
    SetState(ServiceState::INITIALIZING);
    RegisterBuiltins();
    SetState(ServiceState::INITIALIZED);
    m_initialized = true;
    LOG_INFO(kLogCategory, "CommandPalette initialized (%zu commands)", m_commands.size());
    return true;
}

void CommandPalette::Update() noexcept {}

bool CommandPalette::RegisterCommand(const Command& cmd, CommandHandler handler) noexcept {
    if (m_commands.size() >= kMaxCommands || !handler) return false;
    if (FindCommandIndex(cmd.id) >= 0) return false; // No duplicates

    CommandEntry entry;
    entry.cmd = cmd;
    entry.handler = std::move(handler);
    try {
        m_commands.push_back(std::move(entry));
    } catch (const std::bad_alloc&) {
        LOG_ERROR(kLogCategory, "RegisterCommand: heap exhausted for '%s', skipping",
            cmd.id.c_str());
        return false;
    }
    return true;
}

bool CommandPalette::UnregisterCommand(const String& id) noexcept {
    int idx = FindCommandIndex(id);
    if (idx < 0) return false;
    m_commands.erase(m_commands.begin() + idx);
    return true;
}

CommandResult CommandPalette::Execute(const String& commandLine) noexcept {
    auto args = ParseArgs(commandLine);
    if (args.empty()) return CommandResult(false, "Empty command");

    const String& cmdId = args[0];
    std::vector<String> cmdArgs(args.begin() + 1, args.end());

    return Execute(cmdId, cmdArgs);
}

CommandResult CommandPalette::Execute(const String& id, const std::vector<String>& args) noexcept {
    int idx = FindCommandIndex(id);
    if (idx < 0) return CommandResult(false, "Unknown command: " + id);

    const auto& entry = m_commands[idx];

    if (args.size() < entry.cmd.minArgs)
        return CommandResult(false, "Too few arguments for: " + id);
    if (entry.cmd.maxArgs > 0 && args.size() > entry.cmd.maxArgs)
        return CommandResult(false, "Too many arguments for: " + id);

    LOG_DEBUG(kLogCategory, "Executing command: %s", id.c_str());
    return entry.handler(args);
}

std::vector<Command> CommandPalette::Search(const String& query) const noexcept {
    std::vector<Command> results;
    String lowerQuery = query;
    lowerQuery.toLowerCase();

    for (const auto& entry : m_commands) {
        String lowerName = entry.cmd.name;
        lowerName.toLowerCase();
        String lowerKeywords = entry.cmd.keywords;
        lowerKeywords.toLowerCase();
        String lowerDesc = entry.cmd.description;
        lowerDesc.toLowerCase();

        if (lowerName.indexOf(lowerQuery) >= 0 ||
            lowerKeywords.indexOf(lowerQuery) >= 0 ||
            lowerDesc.indexOf(lowerQuery) >= 0) {
            results.push_back(entry.cmd);
        }
    }

    return results;
}

std::vector<Command> CommandPalette::GetByCategory(CommandCategory category) const noexcept {
    std::vector<Command> results;
    for (const auto& entry : m_commands) {
        if (entry.cmd.category == category) results.push_back(entry.cmd);
    }
    return results;
}

std::vector<Command> CommandPalette::GetAllCommands() const noexcept {
    std::vector<Command> results;
    for (const auto& entry : m_commands) {
        results.push_back(entry.cmd);
    }
    return results;
}

Command CommandPalette::FindCommand(const String& id) const noexcept {
    int idx = FindCommandIndex(id);
    if (idx < 0) return Command();
    return m_commands[idx].cmd;
}

size_t CommandPalette::GetCommandCount() const noexcept {
    return m_commands.size();
}

void CommandPalette::HandleEvent(const String& eventType, const String& eventData) noexcept {
    Service::HandleEvent(eventType, eventData);
}

void CommandPalette::RegisterBuiltins() noexcept {
    // Navigation
    RegisterCommand(
        {"dashboard", "Open Dashboard", "Open the main dashboard", "home main", CommandCategory::NAVIGATION, true, 0, 0},
        [](const std::vector<String>&) -> CommandResult {
            uiFramework.showScreen(ScreenID::DASHBOARD);
            return CommandResult(true, "Dashboard opened");
        });

    RegisterCommand(
        {"settings", "Open Settings", "Open the settings panel", "preferences config", CommandCategory::NAVIGATION, true, 0, 0},
        [](const std::vector<String>&) -> CommandResult {
            uiFramework.showNotification("Settings", "Opening settings...");
            return CommandResult(true, "Settings opened");
        });

    RegisterCommand(
        {"health", "Device Health", "Show device health dashboard", "status diagnostics", CommandCategory::NAVIGATION, true, 0, 0},
        [](const std::vector<String>&) -> CommandResult {
            uiFramework.showScreen(ScreenID::DEVICE_HEALTH);
            return CommandResult(true, "Device health shown");
        });

    RegisterCommand(
        {"notifications", "Show Notifications", "Open notification center", "alerts bell", CommandCategory::NAVIGATION, true, 0, 0},
        [](const std::vector<String>&) -> CommandResult {
            uiFramework.showScreen(ScreenID::NOTIFICATION_CENTER);
            return CommandResult(true, "Notifications opened");
        });

    // System
    RegisterCommand(
        {"restart", "Restart Device", "Restart the device", "reboot reset", CommandCategory::SYSTEM, true, 0, 0},
        [](const std::vector<String>&) -> CommandResult {
            uiFramework.showNotification("System", "Restarting...");
            delay(500);
            ESP.restart();
            return CommandResult(true, "Restarting");
        });

    RegisterCommand(
        {"shutdown", "Shutdown", "Shut down the system gracefully", "power off", CommandCategory::SYSTEM, true, 0, 0},
        [](const std::vector<String>&) -> CommandResult {
            systemManager.shutdown();
            return CommandResult(true, "System shut down");
        });

    RegisterCommand(
        {"sleep", "Sleep Mode", "Enter low power sleep mode", "lowpower standby", CommandCategory::SYSTEM, true, 0, 0},
        [](const std::vector<String>&) -> CommandResult {
            systemManager.enterLowPower();
            return CommandResult(true, "Entering sleep mode");
        });

    // Memory
    RegisterCommand(
        {"remember", "Save Memory", "Save a memory: remember what I said", "memory save", CommandCategory::MEMORY, true, 1, 10},
        [](const std::vector<String>& args) -> CommandResult {
            String text;
            for (const auto& a : args) { text += a + " "; }
            text.trim();
            if (memoryManager.isInitialized()) {
                memoryManager.remember(MemoryCategory::USER, "command", text, 10, false);
            }
            return CommandResult(true, "Memory saved");
        });

    RegisterCommand(
        {"search", "Search Memory", "Search memories: search keyword", "find lookup query", CommandCategory::MEMORY, true, 1, 10},
        [](const std::vector<String>& args) -> CommandResult {
            String query;
            for (const auto& a : args) { query += a + " "; }
            uiFramework.showNotification("Search", "Searching: " + query);
            return CommandResult(true, "Search initiated");
        });

    // Studies
    RegisterCommand(
        {"study", "Start Study", "Start a study session: study physics", "learn focus", CommandCategory::STUDIES, true, 1, 10},
        [](const std::vector<String>& args) -> CommandResult {
            String subject;
            for (const auto& a : args) { subject += a + " "; }
            uiFramework.showNotification("Study", "Starting: " + subject);
            if (studyManager.isInitialized()) {
                String s = subject;
                s.trim();
                studyManager.startSession(s);
            }
            return CommandResult(true, "Study session started");
        });

    // Reminders
    RegisterCommand(
        {"remind", "Create Reminder", "Create a reminder: remind me to do X", "alert notify", CommandCategory::REMINDERS, true, 1, 10},
        [](const std::vector<String>& args) -> CommandResult {
            String text;
            for (const auto& a : args) { text += a + " "; }
            if (reminderManager.isInitialized()) {
                reminderManager.addReminder(text, "", 0);
            }
            return CommandResult(true, "Reminder created");
        });

    // Scenes
    RegisterCommand(
        {"scene", "Activate Scene", "Switch to a scene: scene study", "mode profile", CommandCategory::SCENE, true, 1, 1},
        [](const std::vector<String>& args) -> CommandResult {
            uiFramework.showNotification("Scene", "Activating: " + args[0]);
            return CommandResult(true, "Scene activated");
        });

    // Device
    RegisterCommand(
        {"wifi", "WiFi Status", "Show WiFi connection status", "network status", CommandCategory::DEVICE, true, 0, 0},
        [](const std::vector<String>&) -> CommandResult {
            bool connected = WiFi.isConnected();
            String status = connected ? "Connected" : "Disconnected";
            return CommandResult(true, "WiFi: " + status);
        });

    RegisterCommand(
        {"help", "Show Help", "Show available commands", "commands list", CommandCategory::NAVIGATION, true, 0, 1},
        [this](const std::vector<String>& args) -> CommandResult {
            if (args.empty()) {
                return CommandResult(true, "Available: " + String(m_commands.size()) + " commands. Type 'help <cmd>' for details.");
            }
            int idx = FindCommandIndex(args[0]);
            if (idx < 0) return CommandResult(false, "Unknown command: " + args[0]);
            const auto& c = m_commands[idx].cmd;
            return CommandResult(true, c.name + ": " + c.description);
        });
}

int CommandPalette::FindCommandIndex(const String& id) const noexcept {
    for (size_t i = 0; i < m_commands.size(); ++i) {
        if (m_commands[i].cmd.id == id) return static_cast<int>(i);
    }
    return -1;
}

std::vector<String> CommandPalette::ParseArgs(const String& commandLine) const noexcept {
    std::vector<String> args;
    String current;

    for (size_t i = 0; i < commandLine.length(); ++i) {
        char c = commandLine[i];
        if (c == ' ') {
            if (current.length() > 0) {
                args.push_back(current);
                current = "";
            }
        } else {
            current += c;
        }
    }

    if (current.length() > 0) {
        args.push_back(current);
    }

    return args;
}