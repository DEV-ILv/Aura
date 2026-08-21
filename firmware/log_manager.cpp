#include "log_manager.h"
#include "event_bus.h"
#include "storage_manager.h"

LogManager logManager;

LogManager::LogManager() noexcept
    : Service(kStaticName, BootPriority::PRIORITY_LOW)
    , m_initialized(false)
    , m_logFileSize(0) {
    // Default: Serial enabled at DEBUG level
    m_outputs[0].enabled = true;   // SERIAL_PORT
    m_outputs[0].minLevel = LogLevelEx::DEBUG;
}

LogManager::~LogManager() noexcept = default;

bool LogManager::Initialize() noexcept {
    if (GetState() != ServiceState::UNINITIALIZED) return true;
    SetState(ServiceState::INITIALIZING);
    SetState(ServiceState::INITIALIZED);
    m_initialized = true;
    LOG_INFO(kLogCategory, "LogManager initialized (TRACE+CRITICAL support added)");
    return true;
}

void LogManager::Update() noexcept {}

bool LogManager::Stop() noexcept {
    m_buffer.clear();
    SetState(ServiceState::STOPPED);
    return true;
}

void LogManager::EnableOutput(LogOutput output, bool enable) noexcept {
    m_outputs[static_cast<uint8_t>(output)].enabled = enable;
}

bool LogManager::IsOutputEnabled(LogOutput output) const noexcept {
    return m_outputs[static_cast<uint8_t>(output)].enabled;
}

void LogManager::SetLevel(LogOutput output, LogLevelEx minLevel) noexcept {
    m_outputs[static_cast<uint8_t>(output)].minLevel = minLevel;
}

LogLevelEx LogManager::GetLevel(LogOutput output) const noexcept {
    return m_outputs[static_cast<uint8_t>(output)].minLevel;
}

void LogManager::Write(LogLevelEx level, const String& category, const String& message) noexcept {
    LogEntry entry;
    entry.timestamp = millis();
    entry.level = level;
    entry.category = category;
    entry.message = message;

    // Buffer
    m_buffer.push_back(entry);
    PruneBuffer();

    // Route to enabled outputs
    for (uint8_t i = 0; i < 4; ++i) {
        if (!m_outputs[i].enabled || level < m_outputs[i].minLevel) continue;

        switch (static_cast<LogOutput>(i)) {
            case LogOutput::SERIAL_PORT:
                WriteToSerial(level, category, message);
                break;
            case LogOutput::SD_CARD:
                WriteToSD(entry);
                break;
            case LogOutput::WEB_REMOTE:
                WriteToEventBus(entry);
                break;
            case LogOutput::EVENT_BUS:
                WriteToEventBus(entry);
                break;
        }
    }
}

void LogManager::Trace(const String& category, const String& msg) noexcept {
    Write(LogLevelEx::TRACE, category, msg);
}

void LogManager::Debug(const String& category, const String& msg) noexcept {
    Write(LogLevelEx::DEBUG, category, msg);
}

void LogManager::Info(const String& category, const String& msg) noexcept {
    Write(LogLevelEx::INFO, category, msg);
}

void LogManager::Warning(const String& category, const String& msg) noexcept {
    Write(LogLevelEx::WARNING, category, msg);
}

void LogManager::Error(const String& category, const String& msg) noexcept {
    Write(LogLevelEx::ERROR, category, msg);
}

void LogManager::Critical(const String& category, const String& msg) noexcept {
    Write(LogLevelEx::CRITICAL, category, msg);
}

std::vector<LogEntry> LogManager::Search(const String& query, LogLevelEx minLevel,
                                          size_t maxResults) const noexcept {
    std::vector<LogEntry> results;
    String lowerQuery = query;
    lowerQuery.toLowerCase();

    for (const auto& entry : m_buffer) {
        if (entry.level < minLevel) continue;
        String lowerMsg = entry.message;
        lowerMsg.toLowerCase();
        String lowerCat = entry.category;
        lowerCat.toLowerCase();

        if (lowerMsg.indexOf(lowerQuery) >= 0 || lowerCat.indexOf(lowerQuery) >= 0) {
            results.push_back(entry);
            if (results.size() >= maxResults) break;
        }
    }

    return results;
}

std::vector<LogEntry> LogManager::GetRecent(size_t count) const noexcept {
    if (count >= m_buffer.size()) return m_buffer;

    std::vector<LogEntry> recent;
    recent.reserve(count);
    for (size_t i = m_buffer.size() - count; i < m_buffer.size(); ++i) {
        recent.push_back(m_buffer[i]);
    }
    return recent;
}

size_t LogManager::GetEntryCount() const noexcept {
    return m_buffer.size();
}

bool LogManager::RotateLog() noexcept {
    if (m_logFileSize < kMaxLogFileSize) return true;

    // Rename current log to .old, start fresh
    storageManager.renameFile(kLogFilePath, kLogFilePathOld);
    m_logFileSize = 0;
    LOG_INFO(kLogCategory, "Log rotated");
    return true;
}

size_t LogManager::GetLogSize() const noexcept {
    return m_logFileSize;
}

String LogManager::LevelToString(LogLevelEx level) const noexcept {
    switch (level) {
        case LogLevelEx::TRACE:    return "TRACE";
        case LogLevelEx::DEBUG:    return "DEBUG";
        case LogLevelEx::INFO:     return "INFO";
        case LogLevelEx::WARNING:  return "WARN";
        case LogLevelEx::ERROR:    return "ERROR";
        case LogLevelEx::CRITICAL: return "CRITICAL";
        default:                   return "UNKNOWN";
    }
}

LogLevelEx LogManager::StringToLevel(const String& str) const noexcept {
    if (str == "TRACE") return LogLevelEx::TRACE;
    if (str == "DEBUG") return LogLevelEx::DEBUG;
    if (str == "INFO") return LogLevelEx::INFO;
    if (str == "WARNING" || str == "WARN") return LogLevelEx::WARNING;
    if (str == "ERROR") return LogLevelEx::ERROR;
    if (str == "CRITICAL") return LogLevelEx::CRITICAL;
    return LogLevelEx::INFO;
}

void LogManager::WriteToSerial(LogLevelEx level, const String& category, const String& message) noexcept {
    // Use existing Logger for serial output (thread-safe, formatted)
    switch (level) {
        case LogLevelEx::TRACE:
        case LogLevelEx::DEBUG:
            Logger::debug(category.c_str(), "%s", message.c_str());
            break;
        case LogLevelEx::INFO:
            Logger::info(category.c_str(), "%s", message.c_str());
            break;
        case LogLevelEx::WARNING:
            Logger::warning(category.c_str(), "%s", message.c_str());
            break;
        case LogLevelEx::ERROR:
        case LogLevelEx::CRITICAL:
            Logger::error(category.c_str(), "%s", message.c_str());
            break;
        default:
            break;
    }
}

void LogManager::WriteToSD(const LogEntry& entry) noexcept {
    if (!storageManager.isInitialized()) return;

    String line = "[" + String(entry.timestamp) + "][" + LevelToString(entry.level) +
                  "][" + entry.category + "] " + entry.message + "\n";

    storageManager.appendFile(kLogFilePath, line.c_str());
    m_logFileSize += line.length();
    RotateLog();
}

void LogManager::WriteToEventBus(const LogEntry& entry) noexcept {
    if (!eventBus.isInitialized()) return;

    String payload = "{\"level\":\"" + LevelToString(entry.level) +
                     "\",\"category\":\"" + entry.category +
                     "\",\"msg\":\"" + entry.message + "\"}";

    eventBus.publish(EventType::USER_DEFINED, "LogManager", payload);
}

void LogManager::PruneBuffer() noexcept {
    while (m_buffer.size() > kMaxBufferedEntries) {
        m_buffer.erase(m_buffer.begin());
    }
}