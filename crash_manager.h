#ifndef AURA_CRASH_MANAGER_H
#define AURA_CRASH_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <Preferences.h>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

/**
 * @struct CrashLog
 * @brief Stores crash information
 */
struct CrashLog {
    String id;                  ///< Unique crash ID
    unsigned long timestamp;    ///< Crash timestamp
    String exception;           ///< Exception description
    String lastModule;          ///< Last active module
    String stackInfo;           ///< Stack trace (where available)
    uint32_t freeHeap;          ///< Free heap at crash
    uint32_t freeSketch;        ///< Free sketch space
    String resetReason;         ///< ESP32 reset reason
    bool acknowledged;          ///< Whether user has seen this

    CrashLog() noexcept
        : timestamp(0), freeHeap(0), freeSketch(0), acknowledged(false) {}
};

/**
 * @class CrashManager
 * @brief Captures and stores crash information
 *
 * On system crash, stores timestamp, exception, last module,
 * stack info, heap, and free RAM to SD card.
 * Crash logs are viewable in Web Portal.
 */
class CrashManager {
public:
    CrashManager() noexcept;
    ~CrashManager() noexcept;

    CrashManager(const CrashManager&) = delete;
    CrashManager& operator=(const CrashManager&) = delete;
    CrashManager(CrashManager&&) = delete;
    CrashManager& operator=(CrashManager&&) = delete;

    /**
     * @brief Initialize crash manager
     * @return true if initialized
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Update crash manager
     */
    void update() noexcept;

    /**
     * @brief Log a crash event
     * @param exception Exception description
     * @param lastModule Last active module name
     * @param stackInfo Stack trace information
     */
    void logCrash(const String& exception, const String& lastModule,
                  const String& stackInfo = "") noexcept;

    /**
     * @brief Check for crash on boot (from stored reset reason)
     */
    void checkCrashOnBoot() noexcept;

    /**
     * @brief Get all crash logs
     * @return Vector of crash logs
     */
    [[nodiscard]] const std::vector<CrashLog>& getAllCrashes() const noexcept;

    /**
     * @brief Get crash log by ID
     * @param crashId Crash log ID
     * @return CrashLog (empty id if not found)
     */
    [[nodiscard]] CrashLog getCrash(const String& crashId) const noexcept;

    /**
     * @brief Acknowledge a crash log
     * @param crashId Crash log ID
     * @return true if acknowledged
     */
    [[nodiscard]] bool acknowledgeCrash(const String& crashId) noexcept;

    /**
     * @brief Clear all crash logs
     */
    void clearCrashes() noexcept;

    /**
     * @brief Get total crash count
     * @return Crash count
     */
    [[nodiscard]] size_t crashCount() const noexcept;

    /**
     * @brief Check if initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Detect boot loops from persistent counter
     * @return true if boot loop threshold exceeded
     */
    [[nodiscard]] bool isBootLoopDetected() noexcept;

    /**
     * @brief Clear boot loop counter (successful boot)
     */
    void clearBootLoopCounter() noexcept;

    /**
     * @brief Log crash immediately before restart
     * @param reason Crash reason description
     * @return true if saved
     */
    [[nodiscard]] bool logCrashBeforeRestart(const String& reason) noexcept;

    /**
     * @brief Save crash logs to storage
     * @return true if saved
     */
    [[nodiscard]] bool save() noexcept;

    /**
     * @brief Load crash logs from storage
     * @return true if loaded
     */
    [[nodiscard]] bool load() noexcept;

private:
    static constexpr const char* kLogCategory = "CrashManager";
    static constexpr const char* kStoragePath = "/crash_logs.json";
    static constexpr size_t kMaxCrashLogs = CRASH_LOG_MAX;

    String generateId() noexcept;
    size_t findCrash(const String& id) const noexcept;

    int readBootCounter() noexcept;
    void writeBootCounter(int count) noexcept;

    bool m_initialized;
    bool m_dirty;
    int m_bootCounter;
    std::vector<CrashLog> m_crashes;
    unsigned long m_lastIdCounter;
    Preferences m_prefs;
};

extern CrashManager crashManager;

#endif // AURA_CRASH_MANAGER_H
