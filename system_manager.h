#ifndef AURA_SYSTEM_MANAGER_H
#define AURA_SYSTEM_MANAGER_H

#include <Arduino.h>

/**
 * @enum SystemState
 * @brief System manager operational states
 */
enum class SystemState : uint8_t {
    BOOTING,        ///< System booting
    INITIALIZING,   ///< Initializing modules
    READY,          ///< All systems operational
    BUSY,           ///< Active processing
    LOW_POWER,      ///< Low power mode
    UPDATING,       ///< OTA update in progress
    ERROR,          ///< System error
    SHUTDOWN        ///< Graceful shutdown
};

/**
 * @enum SystemError
 * @brief System manager error codes
 */
enum class SystemError : uint8_t {
    NONE,           ///< No error
    INIT_FAILED,    ///< Module initialization failed
    MEMORY,         ///< Memory error
    WIFI,           ///< WiFi error
    OTA,            ///< OTA error
    STORAGE,        ///< Storage error
    NETWORK,        ///< Network error
    UNKNOWN         ///< Unspecified error
};

/**
 * @struct SystemInfo
 * @brief System information and health metrics
 */
struct SystemInfo {
    String firmwareVersion;        ///< Firmware version
    String deviceName;             ///< Device name
    uint32_t freeHeap;             ///< Current free heap (bytes)
    uint32_t minimumHeap;          ///< Minimum free heap (bytes)
    uint32_t uptime;               ///< System uptime (seconds)
    bool wifiConnected;            ///< WiFi connected
    bool otaRunning;               ///< OTA update running
    bool conversationRunning;      ///< Conversation active
    bool reminderRunning;          ///< Reminder processing

    SystemInfo() noexcept
        : firmwareVersion(""), deviceName(""),
          freeHeap(0), minimumHeap(0), uptime(0),
          wifiConnected(false), otaRunning(false),
          conversationRunning(false), reminderRunning(false) {}
};

/**
 * @class SystemManager
 * @brief Single authority for system coordination and health monitoring
 *
 * Responsibilities:
 * - Initialize all modules in correct dependency order
 * - Monitor system health (memory, WiFi, modules)
 * - Coordinate state transitions
 * - Handle errors with safe rollback
 * - Provide system information
 * - Manage low power and shutdown
 *
 * Thread-safe for main loop access. ESP32-optimized.
 */
class SystemManager {
public:
    /**
     * @brief Constructor
     */
    SystemManager() noexcept;

    /**
     * @brief Destructor
     */
    ~SystemManager() noexcept;

    // Delete copy semantics
    SystemManager(const SystemManager&) = delete;
    SystemManager& operator=(const SystemManager&) = delete;

    // Delete move semantics
    SystemManager(SystemManager&&) = delete;
    SystemManager& operator=(SystemManager&&) = delete;

    /**
     * @brief Initialize system manager and all modules
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
     * @brief Update system manager state machine
     * @note Should be called regularly from loop()
     */
    void update() noexcept;

    /**
     * @brief Graceful shutdown
     */
    void shutdown() noexcept;

    /**
     * @brief Restart device
     */
    void restart() noexcept;

    /**
     * @brief Check if system is in safe mode
     * @return true if running in safe mode
     */
    [[nodiscard]] bool isSafeMode() const noexcept;

    /**
     * @brief Factory reset (erase all settings)
     */
    void factoryReset() noexcept;

    /**
     * @brief Enter low power mode
     */
    void enterLowPower() noexcept;

    /**
     * @brief Exit low power mode
     */
    void exitLowPower() noexcept;

    /**
     * @brief Check system health
     * @return true if healthy, false otherwise
     */
    [[nodiscard]] bool checkHealth() noexcept;

    /**
     * @brief Get system information
     * @return Const reference to SystemInfo
     */
    [[nodiscard]] const SystemInfo& getSystemInfo() const noexcept;

    /**
     * @brief Get current system state
     * @return Current SystemState
     */
    [[nodiscard]] SystemState getState() const noexcept;

    /**
     * @brief Get last error code
     * @return Current SystemError
     */
    [[nodiscard]] SystemError getError() const noexcept;

    /**
     * @brief Check if initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Check if system is busy
     * @return true if not READY or LOW_POWER
     */
    [[nodiscard]] bool isBusy() const noexcept;

private:
    // Constants
    static constexpr unsigned long kHealthCheckIntervalMs = 5000UL;
    static constexpr unsigned long kModuleInitTimeoutMs = 10000UL;
    static constexpr uint32_t kMinimumFreeHeap = 20000U;
    static constexpr const char* kLogCategory = "SystemManager";

    // Private methods
    void changeState(SystemState newState) noexcept;
    void setError(SystemError error) noexcept;
    bool initializeModules() noexcept;
    void updateModules() noexcept;
    void monitorMemory() noexcept;
    void monitorTasks() noexcept;
    void monitorWiFi() noexcept;
    void monitorOTA() noexcept;
    void monitorReminders() noexcept;
    void monitorConversation() noexcept;
    void rollbackInitialization() noexcept;
    void validateCredentials() noexcept;

    // Member variables
    bool m_initialized;
    SystemState m_currentState;
    SystemError m_lastError;
    SystemInfo m_info;
    unsigned long m_bootTime;
    unsigned long m_lastHealthCheck;
    unsigned long m_moduleInitStartTime;
    uint8_t m_initModuleIndex;
    bool m_safeMode;
};

/**
 * @brief Global system manager instance
 */
extern SystemManager systemManager;

#endif // AURA_SYSTEM_MANAGER_H