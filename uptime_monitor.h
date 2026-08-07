#ifndef AURA_UPTIME_MONITOR_H
#define AURA_UPTIME_MONITOR_H

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>

#include "logger.h"

/**
 * @file uptime_monitor.h
 * @brief Tracks session and lifetime uptime for the AURA device.
 *
 * Responsibilities:
 *   - Session uptime with safe millis() rollover handling (uint64 accumulation).
 *   - Persisted lifetime uptime + boot counter + longest session (NVS).
 *   - Boot reset-reason reporting.
 *   - Milestone events logged once as the device stays alive.
 *   - JSON payload for the /api/uptime REST endpoint.
 */
class UptimeMonitor {
public:
    UptimeMonitor() noexcept = default;
    ~UptimeMonitor() = default;

    UptimeMonitor(const UptimeMonitor&) = delete;
    UptimeMonitor& operator=(const UptimeMonitor&) = delete;

    /**
     * @brief Loads persisted counters, records this boot, starts the clock.
     */
    void begin() noexcept;

    /**
     * @brief Accumulates uptime and periodically persists totals. Call from loop.
     */
    void update() noexcept;

    /**
     * @brief Persists lifetime totals immediately (e.g. before shutdown).
     */
    void persist() noexcept;

    /** @brief Session uptime in seconds. */
    [[nodiscard]] uint32_t sessionUptimeSeconds() const noexcept {
        return static_cast<uint32_t>(m_sessionMillis / 1000U);
    }

    /** @brief Lifetime uptime across all boots, seconds. */
    [[nodiscard]] uint32_t lifetimeUptimeSeconds() const noexcept {
        return m_lifetimeUptimeSeconds;
    }

    /** @brief Total number of boots recorded. */
    [[nodiscard]] uint32_t bootCount() const noexcept {
        return m_bootCount;
    }

    /** @brief Longest recorded single session, seconds. */
    [[nodiscard]] uint32_t longestSessionSeconds() const noexcept {
        return m_longestSessionSeconds;
    }

    /** @brief Reset reason of the current boot. */
    [[nodiscard]] esp_reset_reason_t resetReason() const noexcept {
        return m_resetReason;
    }

    /** @brief Human-readable reset reason. */
    [[nodiscard]] const char* resetReasonString() const noexcept;

    /** @brief "Xd HH:MM:SS" style session uptime string. */
    [[nodiscard]] String formatUptime() const noexcept;

    /** @brief "Xh Ym Zs" compact session uptime string. */
    [[nodiscard]] String formatUptimeShort() const noexcept;

    /**
     * @brief Builds the JSON payload for /api/uptime.
     * @return Heap-allocated JSON string (caller owns; use String copy).
     */
    [[nodiscard]] String toJson() const noexcept;

private:
    static void formatDuration(uint32_t seconds, char* out, size_t len) noexcept;

    Preferences m_prefs;

    unsigned long m_lastMillis = 0;      ///< Previous update() timestamp.
    uint64_t m_sessionMillis = 0;        ///< Accumulated session uptime.
    unsigned long m_lastPersistAt = 0;   ///< Last NVS write timestamp.
    uint32_t m_lastMilestoneSec = 0;     ///< Last logged milestone.

    uint32_t m_lifetimeUptimeSeconds = 0;
    uint32_t m_bootCount = 0;
    uint32_t m_longestSessionSeconds = 0;
    esp_reset_reason_t m_resetReason = ESP_RST_UNKNOWN;
    bool m_initialized = false;
};

/**
 * @brief Global uptime monitor instance.
 */
extern UptimeMonitor uptimeMonitor;

#endif  // AURA_UPTIME_MONITOR_H
