#ifndef AURA_SERVICE_STATUS_MANAGER_H
#define AURA_SERVICE_STATUS_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "logger.h"

/**
 * @enum ServiceStatus
 * @brief Lifecycle status of a firmware service/module.
 * @note Values are prefixed to avoid collisions with ESP32 macros
 *       (e.g. Arduino.h defines DISPLAY, ERROR, etc.).
 */
enum class ServiceStatus : uint8_t {
    SS_UNKNOWN   = 0,   ///< Not yet evaluated
    SS_ONLINE    = 1,   ///< Running and healthy
    SS_OFFLINE   = 2,   ///< Present but not available (e.g. no SD card)
    SS_DISABLED  = 3,   ///< Intentionally disabled (headless mode)
    SS_ERROR     = 4    ///< Initialization or runtime failure
};

/**
 * @enum HeadlessMode
 * @brief How headless mode was activated.
 */
enum class HeadlessMode : uint8_t {
    HM_NORMAL  = 0,     ///< Not headless
    HM_AUTO    = 1,     ///< Auto-detected (no display present)
    HM_FORCED  = 2      ///< Forced via HEADLESS_MODE_FORCE
};

/**
 * @enum ServiceId
 * @brief Stable identifiers for every tracked service.
 */
enum class ServiceId : uint8_t {
    SVC_SYSTEM       = 0,
    SVC_DISPLAY      = 1,
    SVC_LED_RING     = 2,
    SVC_MICROPHONE   = 3,
    SVC_SPEAKER      = 4,
    SVC_TOUCH        = 5,
    SVC_SD_CARD      = 6,
    SVC_STORAGE      = 7,
    SVC_WIFI         = 8,
    SVC_WEB_PORTAL   = 9,
    SVC_WEBSOCKET    = 10,
    SVC_REST         = 11,
    SVC_GEMINI       = 12,
    SVC_LOCAL_AI     = 13,
    SVC_MEMORY       = 14,
    SVC_PLANNER      = 15,
    SVC_GOALS        = 16,
    SVC_HABITS       = 17,
    SVC_KNOWLEDGE_GRAPH = 18,
    SVC_REMINDERS    = 19,
    SVC_WORKSPACES   = 20,
    SVC_OTA          = 21,
    SVC_SETTINGS     = 22,
    SVC_COMPANION    = 23,
    SVC_SENSORS      = 24,
    SVC_COUNT        = 25
};

/**
 * @class ServiceStatusManager
 * @brief Central registry of module/service runtime status.
 *
 * Tracks the ONLINE/OFFLINE/DISABLED/ERROR state of every firmware
 * service, records whether the device is running in Headless Mode,
 * and produces JSON payloads for the REST API and WebSocket clients.
 *
 * Singleton accessed via the global `serviceStatusManager`.
 */
class ServiceStatusManager {
public:
    ServiceStatusManager() noexcept;
    ~ServiceStatusManager() noexcept;

    ServiceStatusManager(const ServiceStatusManager&) = delete;
    ServiceStatusManager& operator=(const ServiceStatusManager&) = delete;

    [[nodiscard]] bool initialize() noexcept;

    // ---- Headless mode --------------------------------------------------
    void setHeadless(bool enabled, HeadlessMode mode) noexcept;
    [[nodiscard]] bool isHeadless() const noexcept;
    [[nodiscard]] HeadlessMode getHeadlessMode() const noexcept;
    [[nodiscard]] const char* getHeadlessModeString() const noexcept;

    // ---- Service status -------------------------------------------------
    void setStatus(ServiceId id, ServiceStatus status) noexcept;
    [[nodiscard]] ServiceStatus getStatus(ServiceId id) const noexcept;
    [[nodiscard]] const char* getServiceName(ServiceId id) const noexcept;
    [[nodiscard]] static const char* statusToString(ServiceStatus status) noexcept;
    [[nodiscard]] bool isOnline(ServiceId id) const noexcept;
    [[nodiscard]] bool isEnabled(ServiceId id) const noexcept;

    // ---- JSON serialization ---------------------------------------------
    /**
     * @brief Full status payload for GET /api/status.
     * @param requestCount Number of API requests received so far.
     * @return JSON string including firmware version, headless flag,
     *         per-module status, connected/disabled lists, memory/CPU,
     *         Wi-Fi, uptime, heap and flash usage.
     */
    [[nodiscard]] String getStatusJson(uint32_t requestCount = 0) const noexcept;

    /**
     * @brief Object of all module name -> status strings.
     */
    [[nodiscard]] String getModulesJson() const noexcept;

    /**
     * @brief Array of module names currently ONLINE.
     */
    [[nodiscard]] String getConnectedJson() const noexcept;

    /**
     * @brief Array of module names DISABLED or OFFLINE (unavailable).
     */
    [[nodiscard]] String getDisabledJson() const noexcept;

    // ---- Change tracking (for WebSocket broadcasts) ----------------------
    /**
     * @brief Whether any status changed since last takePendingChangesJson().
     */
    [[nodiscard]] bool hasPendingChanges() const noexcept;

    /**
     * @brief JSON object of modules that changed since last call, and clears.
     * @return e.g. {"type":"module_status","modules":{"display":"DISABLED"}}
     */
    [[nodiscard]] String takePendingChangesJson() noexcept;

    /**
     * @brief Mark all services as "changed" so the next broadcast is full.
     */
    void markAllChanged() noexcept;

private:
    struct ServiceEntry {
        const char* name;
        ServiceStatus status;
        bool dirty;
    };

    static constexpr size_t kServiceCount =
        static_cast<size_t>(ServiceId::SVC_COUNT);

    ServiceEntry m_services[kServiceCount];

    bool m_initialized;
    bool m_headless;
    HeadlessMode m_headlessMode;
    uint32_t m_changeSeq;
};

/// Global instance (defined in service_status_manager.cpp)
extern ServiceStatusManager serviceStatusManager;

#endif // AURA_SERVICE_STATUS_MANAGER_H
