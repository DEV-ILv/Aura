#ifndef AURA_LOG_MANAGER_H
#define AURA_LOG_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "logger.h"
#include "service.h"

enum class LogOutput : uint8_t {
    SERIAL_PORT,
    SD_CARD,
    WEB_REMOTE,
    EVENT_BUS
};

enum class LogLevelEx : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4,
    CRITICAL = 5,
    NONE = 6  // Disable logging
};

struct LogEntry {
    unsigned long timestamp;
    LogLevelEx level;
    String category;
    String message;
    String source;

    LogEntry() noexcept : timestamp(0), level(LogLevelEx::INFO) {}
};

class LogManager : public Service {
public:
    LogManager() noexcept;
    ~LogManager() noexcept;

    bool Initialize() noexcept override;
    void Update() noexcept override;
    bool Stop() noexcept override;

    // Enable/disable outputs
    void EnableOutput(LogOutput output, bool enable) noexcept;
    bool IsOutputEnabled(LogOutput output) const noexcept;

    // Set minimum level per output
    void SetLevel(LogOutput output, LogLevelEx minLevel) noexcept;
    LogLevelEx GetLevel(LogOutput output) const noexcept;

    // Write a log entry
    void Write(LogLevelEx level, const String& category, const String& message) noexcept;

    // Convenience wrappers matching existing Logger style
    void Trace(const String& category, const String& msg) noexcept;
    void Debug(const String& category, const String& msg) noexcept;
    void Info(const String& category, const String& msg) noexcept;
    void Warning(const String& category, const String& msg) noexcept;
    void Error(const String& category, const String& msg) noexcept;
    void Critical(const String& category, const String& msg) noexcept;

    // Query
    std::vector<LogEntry> Search(const String& query, LogLevelEx minLevel = LogLevelEx::TRACE,
                                  size_t maxResults = 50) const noexcept;
    std::vector<LogEntry> GetRecent(size_t count = 50) const noexcept;
    size_t GetEntryCount() const noexcept;

    // Rotation
    bool RotateLog() noexcept;
    size_t GetLogSize() const noexcept;

    String LevelToString(LogLevelEx level) const noexcept;
    LogLevelEx StringToLevel(const String& str) const noexcept;

    // Bridge from existing Logger static calls to LogManager
    static void LoggerBridge(LogLevel level, const char* category, const char* message);

    static constexpr const char* kStaticName = "LogManager";

private:
    struct OutputConfig {
        bool enabled;
        LogLevelEx minLevel;

        OutputConfig() noexcept : enabled(false), minLevel(LogLevelEx::DEBUG) {}
    };

    void WriteToSerial(LogLevelEx level, const String& category, const String& message) noexcept;
    void WriteToSD(const LogEntry& entry) noexcept;
    void WriteToEventBus(const LogEntry& entry) noexcept;

    void PruneBuffer() noexcept;

    static constexpr const char* kLogCategory = "LogManager";
    static constexpr size_t kMaxBufferedEntries = 200;
    static constexpr size_t kMaxLogFileSize = 1024 * 1024; // 1MB before rotation
    static constexpr const char* kLogFilePath = "/system/log.txt";
    static constexpr const char* kLogFilePathOld = "/system/log.old.txt";

    OutputConfig m_outputs[4]; // SERIAL_PORT, SD, WEB, EVENTBUS
    std::vector<LogEntry> m_buffer;
    bool m_initialized;
    size_t m_logFileSize;
};

extern LogManager logManager;

#endif