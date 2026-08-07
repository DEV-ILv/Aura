#include "uptime_monitor.h"

UptimeMonitor uptimeMonitor;

namespace {

constexpr const char* kLogCategory = "Uptime";

constexpr const char* kUptimeNvsNamespace = "uptime";
constexpr const char* kLifetimeKey = "lifetime_s";
constexpr const char* kLifetimeBootsKey = "boots";
constexpr const char* kLongestSessionKey = "longest_s";

constexpr uint32_t kPersistIntervalMs = 60000UL;   // NVS write every 60s
constexpr unsigned long kMilestoneTick = 1000UL;

}  // namespace

void UptimeMonitor::begin() noexcept {
    m_resetReason = esp_reset_reason();
    m_lastMillis = millis();
    m_lastPersistAt = m_lastMillis;

    // Load persisted lifetime stats.
    if (m_prefs.begin(kUptimeNvsNamespace, true)) {
        m_lifetimeUptimeSeconds = m_prefs.getUInt(kLifetimeKey, 0);
        m_bootCount = m_prefs.getUInt(kLifetimeBootsKey, 0);
        m_longestSessionSeconds = m_prefs.getUInt(kLongestSessionKey, 0);
        m_prefs.end();
    }

    // Record one more boot; carry over the last session if it ended uncleanly.
    if (m_prefs.begin(kUptimeNvsNamespace, false)) {
        m_bootCount++;
        m_prefs.putUInt(kLifetimeBootsKey, m_bootCount);
        m_prefs.end();
    }

    m_initialized = true;
    m_lastMilestoneSec = 0;

    Logger::info(kLogCategory, "Uptime monitor started (reset reason: %s, boots: %u)",
                 resetReasonString(), static_cast<unsigned int>(m_bootCount));
}

void UptimeMonitor::update() noexcept {
    if (!m_initialized) return;

    const unsigned long now = millis();
    // Unsigned subtraction is rollover-safe for uint32 millis counters.
    const unsigned long dt = now - m_lastMillis;
    m_lastMillis = now;
    m_sessionMillis += dt;

    const uint32_t sessionSec = sessionUptimeSeconds();

    // Milestones (heart-beat logs so the monitor is auditable).
    if (sessionSec >= m_lastMilestoneSec + kMilestoneTick) {
        m_lastMilestoneSec = sessionSec;
        switch (sessionSec) {
            case 10UL:  Logger::info(kLogCategory, "Uptime: 10s");            break;
            case 60UL:  Logger::info(kLogCategory, "Uptime: 1 minute");        break;
            case 300UL: Logger::info(kLogCategory, "Uptime: 5 minutes");       break;
            case 3600UL: Logger::info(kLogCategory, "Uptime: 1 hour");         break;
            case 21600UL: Logger::info(kLogCategory, "Uptime: 6 hours");       break;
            case 43200UL: Logger::info(kLogCategory, "Uptime: 12 hours");      break;
            case 86400UL: Logger::info(kLogCategory, "Uptime: 1 day");         break;
            default: break;
        }
    }

    // Track longest session.
    if (sessionSec > m_longestSessionSeconds) {
        m_longestSessionSeconds = sessionSec;
    }

    // Persist lifetime totals periodically (avoid NVS write wear).
    if ((now - m_lastPersistAt) >= kPersistIntervalMs) {
        persist();
        m_lastPersistAt = now;
    }
}

void UptimeMonitor::persist() noexcept {
    // Fold the current session into the lifetime total.
    m_lifetimeUptimeSeconds = m_lifetimeUptimeSeconds + sessionUptimeSeconds();
    m_sessionMillis = 0;

    if (!m_prefs.begin(kUptimeNvsNamespace, false)) return;
    m_prefs.putUInt(kLifetimeKey, m_lifetimeUptimeSeconds);
    m_prefs.putUInt(kLifetimeBootsKey, m_bootCount);
    m_prefs.putUInt(kLongestSessionKey, m_longestSessionSeconds);
    m_prefs.end();
}

const char* UptimeMonitor::resetReasonString() const noexcept {
    switch (m_resetReason) {
        case ESP_RST_POWERON:     return "power-on";
        case ESP_RST_SW:          return "software-restart";
        case ESP_RST_PANIC:       return "panic";
        case ESP_RST_INT_WDT:     return "interrupt-watchdog";
        case ESP_RST_TASK_WDT:    return "task-watchdog";
        case ESP_RST_WDT:         return "other-watchdog";
        case ESP_RST_DEEPSLEEP:   return "deep-sleep-wake";
        case ESP_RST_BROWNOUT:    return "brownout";
        case ESP_RST_SDIO:        return "sdio";
        case ESP_RST_USB:         return "usb";
        case ESP_RST_UNKNOWN:
        default:                  return "unknown";
    }
}

// ============================================================================
// Formatting
// ============================================================================

void UptimeMonitor::formatDuration(
    const uint32_t seconds, char* out, const size_t len) noexcept {
    const uint32_t d = seconds / 86400UL;
    const uint32_t h = (seconds % 86400UL) / 3600UL;
    const uint32_t m = (seconds % 3600UL) / 60UL;
    const uint32_t s = seconds % 60UL;

    if (d > 0UL) {
        snprintf(out, len, "%lu%s%02lu:%02lu:%02lu",
                 static_cast<unsigned long>(d),
                 (d == 1UL) ? " day " : " days ",
                 static_cast<unsigned long>(h),
                 static_cast<unsigned long>(m),
                 static_cast<unsigned long>(s));
    } else {
        snprintf(out, len, "%02lu:%02lu:%02lu",
                 static_cast<unsigned long>(h),
                 static_cast<unsigned long>(m),
                 static_cast<unsigned long>(s));
    }
}

String UptimeMonitor::formatUptime() const noexcept {
    char buf[40];
    formatDuration(sessionUptimeSeconds(), buf, sizeof(buf));
    return String(buf);
}

String UptimeMonitor::formatUptimeShort() const noexcept {
    const uint32_t sec = sessionUptimeSeconds();
    const uint32_t h = sec / 3600UL;
    const uint32_t m = (sec % 3600UL) / 60UL;
    const uint32_t s = sec % 60UL;
    String out;
    if (h) out += String(h) + "h ";
    if (m || h) out += String(m) + "m ";
    out += String(s) + "s";
    return out;
}

String UptimeMonitor::toJson() const noexcept {
    char durBuf[40];
    formatDuration(sessionUptimeSeconds(), durBuf, sizeof(durBuf));

    char longestBuf[40];
    formatDuration(m_longestSessionSeconds, longestBuf, sizeof(longestBuf));

    String json;
    json.reserve(256);
    json = "{\"uptimeSeconds\":";
    json += String(static_cast<uint64_t>(sessionUptimeSeconds()));
    json += ",\"uptime\":\"";
    json += durBuf;
    json += "\",\"uptimeShort\":\"";
    json += formatUptimeShort();
    json += "\",\"resetReason\":\"";
    json += resetReasonString();
    json += "\",\"bootCount\":";
    json += String(bootCount());
    json += ",\"lifetimeSeconds\":";
    json += String(static_cast<uint64_t>(m_lifetimeUptimeSeconds));
    json += ",\"longestSession\":\"";
    json += longestBuf;
    json += "\",\"longestSeconds\":";
    json += String(static_cast<uint64_t>(m_longestSessionSeconds));
    json += "}";
    return json;
}