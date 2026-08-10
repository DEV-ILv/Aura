#ifndef AURA_ERROR_MANAGER_H
#define AURA_ERROR_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

/**
 * @enum AuraErrorSeverity
 * @brief Severity levels for structured diagnostic events.
 *
 * Mirrors the LogLevel ordering in logger.h (TRACE..CRITICAL) so a numeric
 * comparison works: CRITICAL is the most severe.
 */
enum class AuraErrorSeverity : uint8_t {
    INFO = 0,       ///< Informational diagnostic event
    WARNING = 1,    ///< Minor anomaly, service still functioning
    ERROR = 2,      ///< Notable failure, recovery may be automatic
    CRITICAL = 3    ///< Major failure, manual intervention likely required
};

/// Human-readable severity name ("INFO", "WARNING", "ERROR", "CRITICAL").
const char* auraErrorSeverityName(AuraErrorSeverity severity) noexcept;

/**
 * @struct AuraError
 * @brief A single structured device diagnostic event.
 *
 * Events are deduplicated by (component, code): repeating faults increment
 * occurrenceCount instead of flooding the history. The ring is bounded to
 * ERROR_LOG_MAX entries and persisted to SPIFFS.
 */
struct AuraError {
    String id;                       ///< Stable event id ("ERR-XXXXXX")
    String component;                ///< Owning module ("WiFi", "STT", "Memory" ...)
    String code;                     ///< Machine code ("WIFI_CONN_TIMEOUT")
    String title;                    ///< Short human title
    String message;                  ///< Detail (sanitized, never contains credentials)
    AuraErrorSeverity severity;      ///< Worst severity seen for this fault
    unsigned long firstSeenMs;       ///< First observation (millis)
    unsigned long lastSeenMs;        ///< Most recent observation (millis)
    uint32_t uptimeSec;              ///< Uptime at first observation
    uint32_t bootId;                 ///< Boot session id at first observation
    uint32_t occurrenceCount;        ///< Total observations (dedup within throttle)
    bool active;                     ///< true while fault is unresolved
    bool acknowledged;               ///< true once user dismissed it

    AuraError() noexcept
        : severity(AuraErrorSeverity::INFO), firstSeenMs(0), lastSeenMs(0),
          uptimeSec(0), bootId(0), occurrenceCount(0), active(true), acknowledged(false) {}
};

/**
 * @class ErrorManager
 * @brief Structured, on-device diagnostic event reporter.
 *
 * Collects AURA_ERROR events (severity, component, code, active/resolved,
 * acknowledged, occurrence dedup), keeps a bounded in-RAM history, persists
 * it to SPIFFS through StorageManager, and hands the WebPortal one-shot
 * payloads to push over WebSocket when a new ERROR/CRITICAL event fires.
 */
class ErrorManager {
public:
    ErrorManager() noexcept;
    ~ErrorManager() noexcept;

    ErrorManager(const ErrorManager&) = delete;
    ErrorManager& operator=(const ErrorManager&) = delete;
    ErrorManager(ErrorManager&&) = delete;
    ErrorManager& operator=(ErrorManager&&) = delete;

    /**
     * @brief Initialize: load persisted history and report abnormal boot.
     * @return true when the manager can operate (RAM-only mode still works if
     *         storage is unavailable).
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Periodic update: throttle-persist dirty history.
     */
    void update() noexcept;

    /**
     * @brief Report a diagnostic event.
     *
     * Dedups on (component, code): an already-active event gets its counters
     * bumped (subject to ERROR_REPORT_THROTTLE_MS) and severity raised, a new
     * event is appended with a fresh id. A one-shot WebSocket payload is queued
     * only when a brand-new event (or a severity escalation to) reaches at
     * least ERROR_WS_PUSH_SEVERITY.
     *
     * @return true if the history changed, false if throttled/duplicate.
     */
    bool report(AuraErrorSeverity severity, const String& component,
                const String& code, const String& title,
                const String& message = "") noexcept;

    /**
     * @brief Mark an active fault resolved (by component + code).
     * @return true when an event transitioned to resolved.
     */
    bool resolve(const String& component, const String& code) noexcept;

    /**
     * @brief Acknowledge a specific event by id.
     * @return true when acknowledged.
     */
    bool acknowledge(const String& id) noexcept;

    /**
     * @brief Drop all history. Persists immediately.
     */
    void clearAll() noexcept;

    /// Total events in history.
    size_t count() const noexcept;

    /// Events currently active (unresolved).
    size_t activeCount() const noexcept;

    /// Events of a given severity that are currently active.
    size_t activeCountBySeverity(AuraErrorSeverity sev) const noexcept;

    /// Full history.
    const std::vector<AuraError>& getAll() const noexcept;

    /// Event by id (empty id when not found).
    AuraError getById(const String& id) const noexcept;

    /// Serialize a single event by id; empty string when not found.
    String getEventJsonById(const String& id) const noexcept;

    /**
     * @brief Aggregate device health from active events.
     * @return "CRITICAL" | "ERROR" | "WARNING" | "HEALTHY"
     */
    const char* getHealth() const noexcept;

    /// Wait: authoritative health used by dashboard/companion.
    /// (getHealth is defined in the .cpp; helper kept here for API clarity.)

    /// true while a WS payload is queued for the WebPortal.
    bool hasPendingBroadcast() const noexcept;

    /**
     * @brief Consume the queued WS payload.
     * @return JSON like {"type":"aura_error","event":{...}} or empty string.
     */
    String takeBroadcastJson() noexcept;

    /**
     * @brief Serialize events.
     * @param activeOnly true to include only unresolved events.
     * @return JSON object {"errors":[...],"meta":{...}} (all escaped).
     */
    String buildJson(bool activeOnly) const noexcept;

    /**
     * @brief Serialize only counts + health (small payload).
     * @return JSON object {"total":N,"active":M,"warning":..,"error":..,"critical":..,"health":".."}.
     */
    String buildSummaryJson() const noexcept;

    /// true when initialize() has run.
    [[nodiscard]] bool isInitialized() const noexcept;

    /// Persist to storage (best-effort).
    [[nodiscard]] bool save() noexcept;

    /// Load persisted history (best-effort).
    [[nodiscard]] bool load() noexcept;

private:
    static constexpr const char* kLogCategory = "ErrorManager";
    static constexpr const char* kStoragePath = ERROR_EVENT_PATH;
    static constexpr size_t kMaxEvents = ERROR_LOG_MAX;
    static constexpr size_t kMaxTitleLen = 64;
    static constexpr size_t kMaxMessageLen = 200;

    String generateId() noexcept;
    size_t findActive(const String& component, const String& code) noexcept;
    size_t findById(const String& id) const noexcept;
    void evictIfNeeded() noexcept;
    void seedBootInfo() noexcept;
    void sanitize(String& value, size_t maxLen) noexcept;
    String eventToJson(const AuraError& ev) const noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<AuraError> m_events;
    String m_pendingBroadcast;
    uint32_t m_bootId;
    unsigned long m_seq;          ///< monotonic sequence for deterministic ids
    unsigned long m_lastSaveTime;
};

/**
 * @brief Global diagnostic event manager.
 */
extern ErrorManager errorManager;

#endif // AURA_ERROR_MANAGER_H