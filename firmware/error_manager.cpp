#include "error_manager.h"
#include "json_helpers.h"
#include <esp_system.h>

ErrorManager errorManager;

namespace {

const char* resetReasonName(esp_reset_reason_t reason) noexcept {
    switch (reason) {
        case ESP_RST_POWERON:   return "POWER_ON";
        case ESP_RST_SW:        return "SOFTWARE";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEP_SLEEP";
        case ESP_RST_BROWNOUT:  return "BROWN_OUT";
        case ESP_RST_SDIO:      return "SDIO";
        case ESP_RST_EXT:       return "EXT";
        default:                return "UNKNOWN";
    }
}

} // namespace

const char* auraErrorSeverityName(AuraErrorSeverity severity) noexcept {
    switch (severity) {
        case AuraErrorSeverity::INFO:     return "INFO";
        case AuraErrorSeverity::WARNING:  return "WARNING";
        case AuraErrorSeverity::ERROR:    return "ERROR";
        case AuraErrorSeverity::CRITICAL: return "CRITICAL";
        default:                          return "UNKNOWN";
    }
}

ErrorManager::ErrorManager() noexcept
    : m_initialized(false), m_dirty(false), m_bootId(0), m_seq(0), m_lastSaveTime(0) {
}

ErrorManager::~ErrorManager() noexcept {
    if (m_dirty) save();
}

bool ErrorManager::initialize() noexcept {
    if (m_initialized) {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    m_bootId = static_cast<uint32_t>(millis());

    if (storageManager.isHealthy()) {
        load();
    } else {
        Logger::warning(kLogCategory, "StorageManager unhealthy - RAM-only error history");
    }

    seedBootInfo();

    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u events, boot %lu)",
                 static_cast<unsigned>(m_events.size()),
                 static_cast<unsigned long>(m_bootId));
    return true;
}

void ErrorManager::update() noexcept {
    unsigned long now = millis();
    if (m_dirty && (now - m_lastSaveTime > 5000)) {
        m_lastSaveTime = now;
        if (save()) m_dirty = false;
    }
}

bool ErrorManager::report(AuraErrorSeverity severity, const String& component,
                          const String& code, const String& title,
                          const String& message) noexcept {
    String comp = component;
    String ccode = code;
    String ctitle = title;
    String cmsg = message;
    sanitize(comp, 32);
    sanitize(ccode, 40);
    sanitize(ctitle, kMaxTitleLen);
    sanitize(cmsg, kMaxMessageLen);
    if (comp.isEmpty() || ccode.isEmpty()) {
        Logger::warning(kLogCategory, "report() ignored: empty component/code");
        return false;
    }

    unsigned long now = millis();
    size_t idx = findActive(comp, ccode);
    if (idx != SIZE_MAX) {
        AuraError& ev = m_events[idx];
        bool escalated = severity > ev.severity;
        bool freshEnough = (now - ev.lastSeenMs) >= ERROR_REPORT_THROTTLE_MS;

        if (!freshEnough && !escalated) {
            return false; // dedup throttle
        }
        if (escalated) ev.severity = severity;
        if (freshEnough) {
            ev.occurrenceCount++;
            ev.lastSeenMs = now;
        }
        ev.message = cmsg;
        m_dirty = true;

        if (escalated && static_cast<uint8_t>(ev.severity) >= ERROR_WS_PUSH_SEVERITY &&
            m_pendingBroadcast.isEmpty()) {
            m_pendingBroadcast = "{\"type\":\"aura_error\",\"event\":" + eventToJson(ev) + "}";
        }
        return true;
    }

    AuraError ev;
    ev.id = generateId();
    ev.component = comp;
    ev.code = ccode;
    ev.title = ctitle;
    ev.message = cmsg;
    ev.severity = severity;
    ev.firstSeenMs = now;
    ev.lastSeenMs = now;
    ev.uptimeSec = static_cast<uint32_t>(millis() / 1000);
    ev.bootId = m_bootId;
    ev.occurrenceCount = 1;
    ev.active = true;
    ev.acknowledged = false;

    m_events.push_back(ev);
    evictIfNeeded();
    m_dirty = true;

    Logger::info(kLogCategory, "Reported: [%s] %s/%s (%s)",
                 auraErrorSeverityName(ev.severity),
                 ev.component.c_str(), ev.code.c_str(), ev.title.c_str());

    if (static_cast<uint8_t>(ev.severity) >= ERROR_WS_PUSH_SEVERITY) {
        m_pendingBroadcast = "{\"type\":\"aura_error\",\"event\":" + eventToJson(ev) + "}";
    }
    return true;
}

bool ErrorManager::resolve(const String& component, const String& code) noexcept {
    for (auto& ev : m_events) {
        if (ev.active && ev.component == component && ev.code == code) {
            ev.active = false;
            m_dirty = true;
            Logger::info(kLogCategory, "Resolved: %s/%s", component.c_str(), code.c_str());
            return true;
        }
    }
    return false;
}

bool ErrorManager::acknowledge(const String& id) noexcept {
    size_t idx = findById(id);
    if (idx == SIZE_MAX) return false;
    if (!m_events[idx].acknowledged) {
        m_events[idx].acknowledged = true;
        m_dirty = true;
    }
    return true;
}

void ErrorManager::clearAll() noexcept {
    m_events.clear();
    m_dirty = true;
    save();
    Logger::info(kLogCategory, "History cleared");
}

size_t ErrorManager::count() const noexcept {
    return m_events.size();
}

size_t ErrorManager::activeCount() const noexcept {
    size_t n = 0;
    for (const auto& ev : m_events) {
        if (ev.active) n++;
    }
    return n;
}

size_t ErrorManager::activeCountBySeverity(AuraErrorSeverity sev) const noexcept {
    size_t n = 0;
    for (const auto& ev : m_events) {
        if (ev.active && ev.severity == sev) n++;
    }
    return n;
}

const std::vector<AuraError>& ErrorManager::getAll() const noexcept {
    return m_events;
}

AuraError ErrorManager::getById(const String& id) const noexcept {
    size_t idx = findById(id);
    if (idx == SIZE_MAX) return AuraError();
    return m_events[idx];
}

String ErrorManager::getEventJsonById(const String& id) const noexcept {
    for (const auto& ev : m_events) {
        if (ev.id == id) return eventToJson(ev);
    }
    return "";
}

const char* ErrorManager::getHealth() const noexcept {
    if (activeCountBySeverity(AuraErrorSeverity::CRITICAL) > 0) return "CRITICAL";
    if (activeCountBySeverity(AuraErrorSeverity::ERROR) > 0) return "ERROR";
    if (activeCountBySeverity(AuraErrorSeverity::WARNING) > 0) return "WARNING";
    return "HEALTHY";
}

bool ErrorManager::hasPendingBroadcast() const noexcept {
    return !m_pendingBroadcast.isEmpty();
}

String ErrorManager::takeBroadcastJson() noexcept {
    String payload = m_pendingBroadcast;
    m_pendingBroadcast.clear();
    return payload;
}

String ErrorManager::eventToJson(const AuraError& ev) const noexcept {
    String json;
    json.reserve(256);
    json += "{";
    json += "\"id\":\"" + escapeJson(ev.id) + "\",";
    json += "\"severity\":\"" + String(auraErrorSeverityName(ev.severity)) + "\",";
    json += "\"component\":\"" + escapeJson(ev.component) + "\",";
    json += "\"code\":\"" + escapeJson(ev.code) + "\",";
    json += "\"title\":\"" + escapeJson(ev.title) + "\",";
    json += "\"message\":\"" + escapeJson(ev.message) + "\",";
    json += "\"first_seen_ms\":" + String(ev.firstSeenMs) + ",";
    json += "\"last_seen_ms\":" + String(ev.lastSeenMs) + ",";
    json += "\"uptime_sec\":" + String(ev.uptimeSec) + ",";
    json += "\"boot_id\":" + String(ev.bootId) + ",";
    json += "\"occurrences\":" + String(ev.occurrenceCount) + ",";
    json += "\"active\":" + String(ev.active ? "true" : "false") + ",";
    json += "\"acknowledged\":" + String(ev.acknowledged ? "true" : "false");
    json += "}";
    return json;
}

String ErrorManager::buildJson(bool activeOnly) const noexcept {
    String json;
    // Reserve proportional to the actual event count (avg ~380B/event) so the
    // payload reaches its final size with few reallocations. reserve() is a
    // hint; failure degrades gracefully to the previous grow-on-demand path.
    json.reserve(m_events.size() * 380U + 192U);
    json += "{\"errors\":[";
    size_t written = 0;
    for (const auto& ev : m_events) {
        if (activeOnly && !ev.active) continue;
        if (written > 0) json += ",";
        json += eventToJson(ev);
        written++;
    }
    json += "],\"meta\":{";
    json += "\"total\":" + String(static_cast<unsigned>(m_events.size())) + ",";
    json += "\"active\":" + String(static_cast<unsigned>(activeCount())) + ",";
    json += "\"critical\":" + String(static_cast<unsigned>(activeCountBySeverity(AuraErrorSeverity::CRITICAL))) + ",";
    json += "\"error\": " + String(static_cast<unsigned>(activeCountBySeverity(AuraErrorSeverity::ERROR))) + ",";
    json += "\"warning\":" + String(static_cast<unsigned>(activeCountBySeverity(AuraErrorSeverity::WARNING))) + ",";
    json += "\"health\":\"" + String(getHealth()) + "\"";
    json += "}}";
    return json;
}

String ErrorManager::buildSummaryJson() const noexcept {
    String json;
    json.reserve(192);
    json += "{";
    json += "\"total\":" + String(static_cast<unsigned>(m_events.size())) + ",";
    json += "\"active\":" + String(static_cast<unsigned>(activeCount())) + ",";
    json += "\"critical\":" + String(static_cast<unsigned>(activeCountBySeverity(AuraErrorSeverity::CRITICAL))) + ",";
    json += "\"error\":" + String(static_cast<unsigned>(activeCountBySeverity(AuraErrorSeverity::ERROR))) + ",";
    json += "\"warning\":" + String(static_cast<unsigned>(activeCountBySeverity(AuraErrorSeverity::WARNING))) + ",";
    json += "\"health\":\"" + String(getHealth()) + "\"";
    json += "}";
    return json;
}

bool ErrorManager::isInitialized() const noexcept {
    return m_initialized;
}

bool ErrorManager::save() noexcept {
    if (!m_initialized || !storageManager.isHealthy()) return false;

    String json;
    json.reserve(m_events.size() * 380U + 64U);
    json += "{\"errors\":[";
    for (size_t i = 0; i < m_events.size(); ++i) {
        if (i > 0) json += ",";
        json += eventToJson(m_events[i]);
    }
    json += "]}";

    if (storageManager.writeFile(kStoragePath, json, StorageType::SPIFFS) == StorageStatus::SUCCESS) {
        m_dirty = false;
        return true;
    }
    Logger::warning(kLogCategory, "Failed to persist error history");
    return false;
}

bool ErrorManager::load() noexcept {
    if (!storageManager.fileExists(kStoragePath, StorageType::SPIFFS)) return false;

    String content;
    if (storageManager.readFile(kStoragePath, content, StorageType::SPIFFS) != StorageStatus::SUCCESS ||
        content.isEmpty()) {
        return false;
    }

    m_events.clear();

    int pos = 0;
    while (true) {
        int start = content.indexOf('{', pos);
        if (start < 0) break;

        int braceCount = 0;
        int end = start;
        for (; end < (int)content.length(); ++end) {
            if (content[end] == '{') braceCount++;
            else if (content[end] == '}') { braceCount--; if (braceCount == 0) break; }
        }
        if (end >= (int)content.length()) break;

        String obj = content.substring(start, end + 1);
        if (obj.indexOf("\"id\"") < 0 || obj.indexOf("\"component\"") < 0) {
            pos = end + 1;
            continue;
        }

        AuraError ev;
        ev.component = json_helpers_extractString(obj, "component");
        ev.code = json_helpers_extractString(obj, "code");
        ev.title = json_helpers_extractString(obj, "title");
        ev.message = json_helpers_extractString(obj, "message");
        ev.id = json_helpers_extractString(obj, "id");

        String sev = json_helpers_extractString(obj, "severity");
        if (sev == "CRITICAL") ev.severity = AuraErrorSeverity::CRITICAL;
        else if (sev == "ERROR") ev.severity = AuraErrorSeverity::ERROR;
        else if (sev == "WARNING") ev.severity = AuraErrorSeverity::WARNING;
        else ev.severity = AuraErrorSeverity::INFO;

        String firstSeen = json_helpers_extractRaw(obj, "first_seen_ms");
        String lastSeen = json_helpers_extractRaw(obj, "last_seen_ms");
        String uptime = json_helpers_extractRaw(obj, "uptime_sec");
        String bootId = json_helpers_extractRaw(obj, "boot_id");
        String occ = json_helpers_extractRaw(obj, "occurrences");
        ev.firstSeenMs = (unsigned long)firstSeen.toInt();
        ev.lastSeenMs = (unsigned long)lastSeen.toInt();
        ev.uptimeSec = (uint32_t)uptime.toInt();
        ev.bootId = (uint32_t)bootId.toInt();
        ev.occurrenceCount = (uint32_t)occ.toInt();

        ev.active = json_helpers_extractBool(obj, "active");
        ev.acknowledged = json_helpers_extractBool(obj, "acknowledged");

        if (!ev.id.isEmpty()) m_events.push_back(ev);
        pos = end + 1;
    }

    evictIfNeeded();
    Logger::info(kLogCategory, "Loaded %u events", static_cast<unsigned>(m_events.size()));
    return true;
}

String ErrorManager::generateId() noexcept {
    m_seq++;
    unsigned long now = millis();
    uint32_t mix = static_cast<uint32_t>(now) ^
                   static_cast<uint32_t>(m_seq << 16) ^
                   static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFFF);
    m_bootId = (m_bootId == 0) ? mix : m_bootId;
    String id;
    id.reserve(10);
    id += "ERR-";
    static const char hex[] = "0123456789ABCDEF";
    uint32_t val = mix;
    for (size_t i = 0; i < 6; ++i) {
        id += hex[val & 0x0F];
        val = (val >> 2) ^ (val << 3) ^ (m_seq + i);
    }
    return id;
}

size_t ErrorManager::findActive(const String& component, const String& code) noexcept {
    for (size_t i = 0; i < m_events.size(); ++i) {
        if (m_events[i].active && m_events[i].component == component &&
            m_events[i].code == code) {
            return i;
        }
    }
    return SIZE_MAX;
}

size_t ErrorManager::findById(const String& id) const noexcept {
    for (size_t i = 0; i < m_events.size(); ++i) {
        if (m_events[i].id == id) return i;
    }
    return SIZE_MAX;
}

void ErrorManager::evictIfNeeded() noexcept {
    if (m_events.size() <= kMaxEvents) return;

    // Prefer dropping acknowledged + resolved entries first, oldest first.
    size_t overflow = m_events.size() - kMaxEvents;
    for (size_t pass = 0; pass < 2 && overflow > 0; ++pass) {
        for (size_t i = 0; i < m_events.size() && overflow > 0;) {
            bool stale = (pass == 0)
                ? (!m_events[i].active && m_events[i].acknowledged)
                : (!m_events[i].active);
            if (stale) {
                m_events.erase(m_events.begin() + i);
                overflow--;
            } else {
                i++;
            }
        }
    }
    // Final safety net: drop oldest regardless.
    while (m_events.size() > kMaxEvents) {
        m_events.erase(m_events.begin());
    }
}

void ErrorManager::seedBootInfo() noexcept {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_BROWNOUT:
            report(AuraErrorSeverity::CRITICAL, "System", "ABNORMAL_RESET",
                   "Unexpected reset detected",
                   String("Previous boot ended with a ") + resetReasonName(reason) +
                   " reset. See Crash Logs for details.");
            break;
        default:
            break;
    }
}

void ErrorManager::sanitize(String& value, size_t maxLen) noexcept {
    if (value.length() > (int)maxLen) value = value.substring(0, (int)maxLen);
    // Strip control characters so stored JSON stays clean even if a caller
    // passes a raw string that bypasses escapeJson on load (defensive only).
    for (int i = 0; i < (int)value.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c < 0x20) value.setCharAt(i, ' ');
    }
    value.trim();
}