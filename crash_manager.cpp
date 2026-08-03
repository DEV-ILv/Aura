#include "crash_manager.h"
#include "json_helpers.h"
#include <esp_system.h>

CrashManager crashManager;

namespace {

/**
 * @brief Convert ESP32 reset reason to readable string
 */
const char* getResetReasonString(esp_reset_reason_t reason) noexcept {
    switch (reason) {
        case ESP_RST_POWERON:    return "POWER_ON";
        case ESP_RST_SW:         return "SOFTWARE";
        case ESP_RST_PANIC:      return "PANIC";
        case ESP_RST_INT_WDT:    return "INT_WDT";
        case ESP_RST_TASK_WDT:   return "TASK_WDT";
        case ESP_RST_WDT:        return "WDT";
        case ESP_RST_DEEPSLEEP:  return "DEEP_SLEEP";
        case ESP_RST_BROWNOUT:   return "BROWN_OUT";
        case ESP_RST_SDIO:       return "SDIO";
        case ESP_RST_EXT:        return "EXT";
        default:                 return "UNKNOWN";
    }
}

} // namespace

CrashManager::CrashManager() noexcept
    : m_initialized(false), m_dirty(false), m_bootCounter(0), m_lastIdCounter(0) {
}

CrashManager::~CrashManager() noexcept {
    if (m_dirty) save();
}

bool CrashManager::initialize() noexcept {
    if (m_initialized) {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    if (!storageManager.isHealthy()) {
        Logger::error(kLogCategory, "StorageManager not healthy");
        return false;
    }

    load();
    checkCrashOnBoot();

    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u crashes logged)", m_crashes.size());
    return true;
}

void CrashManager::update() noexcept {
    if (!m_initialized) return;

    static unsigned long lastSave = 0;
    unsigned long now = millis();

    if (m_dirty && (now - lastSave > 5000)) {
        lastSave = now;
        if (save()) m_dirty = false;
    }
}

void CrashManager::logCrash(const String& exception, const String& lastModule,
                             const String& stackInfo) noexcept {
    CrashLog crash;
    crash.id = generateId();
    crash.timestamp = millis();
    crash.exception = exception;
    crash.lastModule = lastModule;
    crash.stackInfo = stackInfo;
    crash.freeHeap = ESP.getFreeHeap();
    crash.freeSketch = ESP.getFreeSketchSpace();

    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_PANIC:    crash.resetReason = "PANIC"; break;
        case ESP_RST_INT_WDT:  crash.resetReason = "INT_WDT"; break;
        case ESP_RST_TASK_WDT: crash.resetReason = "TASK_WDT"; break;
        case ESP_RST_WDT:      crash.resetReason = "WDT"; break;
        case ESP_RST_BROWNOUT: crash.resetReason = "BROWN_OUT"; break;
        default:               crash.resetReason = String("RST_") + String(static_cast<int>(reason)); break;
    }

    m_crashes.push_back(crash);

    // Enforce max limit
    while (m_crashes.size() > kMaxCrashLogs) {
        m_crashes.erase(m_crashes.begin());
    }

    m_dirty = true;
    Logger::error(kLogCategory, "Crash logged: %s (module: %s, heap: %u)",
        exception.c_str(), lastModule.c_str(), crash.freeHeap);
}

void CrashManager::checkCrashOnBoot() noexcept {
    esp_reset_reason_t reason = esp_reset_reason();

    bool crashReset = (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT ||
                       reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT ||
                       reason == ESP_RST_BROWNOUT);

    if (crashReset) {
        logCrash("Unexpected reset detected on boot", "SystemStartup");
        Logger::warning(kLogCategory, "Previous boot was caused by %s",
            getResetReasonString(reason));

        m_bootCounter = readBootCounter() + 1;
        writeBootCounter(m_bootCounter);
        Logger::warning(kLogCategory, "Boot counter: %d/%d",
            m_bootCounter, BOOT_LOOP_THRESHOLD);
    } else {
        // Clean boot reset (POWERON, SW, DEEP_SLEEP, EXT) — reset counter
        m_bootCounter = 0;
        writeBootCounter(0);
    }
}

bool CrashManager::isBootLoopDetected() noexcept {
    if (!m_initialized) {
        m_bootCounter = readBootCounter();
    }
    return m_bootCounter >= BOOT_LOOP_THRESHOLD;
}

void CrashManager::clearBootLoopCounter() noexcept {
    m_bootCounter = 0;
    writeBootCounter(0);
}

bool CrashManager::logCrashBeforeRestart(const String& reason) noexcept {
    logCrash(reason, "SystemRestart");
    return save();
}

int CrashManager::readBootCounter() noexcept {
    m_prefs.begin(CRASH_COUNTER_NVS_NAMESPACE, true);
    int val = m_prefs.getInt(CRASH_COUNTER_KEY, 0);
    m_prefs.end();
    return val;
}

void CrashManager::writeBootCounter(int count) noexcept {
    m_prefs.begin(CRASH_COUNTER_NVS_NAMESPACE, false);
    m_prefs.putInt(CRASH_COUNTER_KEY, count);
    m_prefs.end();
}

const std::vector<CrashLog>& CrashManager::getAllCrashes() const noexcept {
    return m_crashes;
}

CrashLog CrashManager::getCrash(const String& crashId) const noexcept {
    size_t idx = findCrash(crashId);
    if (idx == SIZE_MAX) return CrashLog();
    return m_crashes[idx];
}

bool CrashManager::acknowledgeCrash(const String& crashId) noexcept {
    size_t idx = findCrash(crashId);
    if (idx == SIZE_MAX) return false;
    m_crashes[idx].acknowledged = true;
    m_dirty = true;
    return true;
}

void CrashManager::clearCrashes() noexcept {
    m_crashes.clear();
    m_dirty = true;
    Logger::info(kLogCategory, "All crash logs cleared");
}

size_t CrashManager::crashCount() const noexcept {
    return m_crashes.size();
}

bool CrashManager::isInitialized() const noexcept {
    return m_initialized;
}

bool CrashManager::save() noexcept {
    String json;
    json.reserve(2048);
    json += "{\"crashes\":[";
    for (size_t i = 0; i < m_crashes.size(); ++i) {
        if (i > 0) json += ",";
        const CrashLog& c = m_crashes[i];
        json += "{";
        json += "\"id\":\"" + escapeJson(c.id) + "\",";
        json += "\"timestamp\":" + String(c.timestamp) + ",";
        json += "\"exception\":\"" + escapeJson(c.exception) + "\",";
        json += "\"last_module\":\"" + escapeJson(c.lastModule) + "\",";
        json += "\"stack_info\":\"" + escapeJson(c.stackInfo) + "\",";
        json += "\"free_heap\":" + String(c.freeHeap) + ",";
        json += "\"free_sketch\":" + String(c.freeSketch) + ",";
        json += "\"reset_reason\":\"" + escapeJson(c.resetReason) + "\",";
        json += "\"acknowledged\":" + String(c.acknowledged ? "true" : "false");
        json += "}";
    }
    json += "]}";

    StorageStatus status = storageManager.writeFile(kStoragePath, json, StorageType::SPIFFS);
    if (status == StorageStatus::SUCCESS) {
        m_dirty = false;
        return true;
    }
    return false;
}

bool CrashManager::load() noexcept {
    if (!storageManager.fileExists(kStoragePath, StorageType::SPIFFS)) return false;

    String content;
    StorageStatus status = storageManager.readFile(kStoragePath, content, StorageType::SPIFFS);
    if (status != StorageStatus::SUCCESS || content.isEmpty()) return false;

    m_crashes.clear();

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
        CrashLog crash;

        auto extract = [&](const char* key) -> String {
            String search = String("\"") + key + "\":\"";
            int s = obj.indexOf(search);
            if (s >= 0) {
                s += search.length();
                int e = obj.indexOf('"', s);
                return (e < 0) ? "" : obj.substring(s, e);
            }
            search = String("\"") + key + "\":";
            s = obj.indexOf(search);
            if (s >= 0) {
                s += search.length();
                int e = s;
                while (e < (int)obj.length() && obj[e] != ',' && obj[e] != '}') e++;
                return obj.substring(s, e);
            }
            return "";
        };

        crash.id = extract("id");
        crash.exception = extract("exception");
        crash.lastModule = extract("last_module");
        crash.resetReason = extract("reset_reason");

        if (!crash.id.isEmpty()) {
            m_crashes.push_back(crash);
        }

        pos = end + 1;
    }

    return true;
}

String CrashManager::generateId() noexcept {
    unsigned long now = millis();
    m_lastIdCounter++;
    uint32_t mix = static_cast<uint32_t>(now) ^
                   static_cast<uint32_t>(m_lastIdCounter << 16) ^
                   static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFFF);
    String id;
    id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) {
        id += hex[val & 0x0F];
        val = (val >> 2) ^ (val << 3) ^ (m_lastIdCounter + i);
    }
    return id;
}

size_t CrashManager::findCrash(const String& id) const noexcept {
    for (size_t i = 0; i < m_crashes.size(); ++i) {
        if (m_crashes[i].id == id) return i;
    }
    return SIZE_MAX;
}
