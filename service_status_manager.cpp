#include "service_status_manager.h"

#include <WiFi.h>
#include <esp_system.h>
#include "performance_manager.h"

/// Global ServiceStatusManager instance
ServiceStatusManager serviceStatusManager;

namespace {
const char* kHeadlessModeNames[] = { "normal", "auto", "forced" };

const char* const kServiceNames[] = {
    "system",
    "display",
    "led_ring",
    "microphone",
    "speaker",
    "touch",
    "sd_card",
    "storage",
    "wifi",
    "web_portal",
    "websocket",
    "rest",
    "gemini",
    "local_ai",
    "memory",
    "planner",
    "goals",
    "habits",
    "knowledge_graph",
    "reminders",
    "workspaces",
    "ota",
    "settings",
    "companion",
    "sensors"
};

static_assert(sizeof(kServiceNames) / sizeof(kServiceNames[0]) ==
                  static_cast<size_t>(ServiceId::SVC_COUNT),
              "service name table must match ServiceId");
} // namespace

ServiceStatusManager::ServiceStatusManager() noexcept
    : m_initialized(false),
      m_headless(false),
      m_headlessMode(HeadlessMode::HM_NORMAL),
      m_changeSeq(0) {
    for (size_t i = 0; i < kServiceCount; ++i) {
        m_services[i].name = (i < sizeof(kServiceNames) / sizeof(kServiceNames[0]))
                                 ? kServiceNames[i]
                                 : "unknown";
        m_services[i].status = ServiceStatus::SS_UNKNOWN;
        m_services[i].dirty = true;
    }
}

ServiceStatusManager::~ServiceStatusManager() noexcept {}

bool ServiceStatusManager::initialize() noexcept {
    if (m_initialized) return true;
    m_initialized = true;
    for (size_t i = 0; i < kServiceCount; ++i) {
        m_services[i].status = ServiceStatus::SS_UNKNOWN;
        m_services[i].dirty = true;
    }
    m_headless = false;
    m_headlessMode = HeadlessMode::HM_NORMAL;
    Logger::info("ServiceStatus", "Initialized");
    return true;
}

// ---- Headless mode --------------------------------------------------------

void ServiceStatusManager::setHeadless(bool enabled, HeadlessMode mode) noexcept {
    if (m_headless == enabled && m_headlessMode == mode) return;
    m_headless = enabled;
    m_headlessMode = mode;
    m_changeSeq++;
    // Reflect headless state in the system service
    m_services[static_cast<size_t>(ServiceId::SVC_SYSTEM)].dirty = true;
    Logger::info("ServiceStatus", "Headless mode: %s (%s)",
        m_headless ? "ENABLED" : "disabled", getHeadlessModeString());
}

bool ServiceStatusManager::isHeadless() const noexcept { return m_headless; }

HeadlessMode ServiceStatusManager::getHeadlessMode() const noexcept {
    return m_headlessMode;
}

const char* ServiceStatusManager::getHeadlessModeString() const noexcept {
    uint8_t idx = static_cast<uint8_t>(m_headlessMode);
    if (idx > 2) idx = 0;
    return kHeadlessModeNames[idx];
}

// ---- Service status --------------------------------------------------------

void ServiceStatusManager::setStatus(ServiceId id, ServiceStatus status) noexcept {
    size_t index = static_cast<size_t>(id);
    if (index >= kServiceCount) return;
    if (m_services[index].status == status) return;
    m_services[index].status = status;
    m_services[index].dirty = true;
    m_changeSeq++;
}

ServiceStatus ServiceStatusManager::getStatus(ServiceId id) const noexcept {
    size_t index = static_cast<size_t>(id);
    if (index >= kServiceCount) return ServiceStatus::SS_UNKNOWN;
    return m_services[index].status;
}

const char* ServiceStatusManager::getServiceName(ServiceId id) const noexcept {
    size_t index = static_cast<size_t>(id);
    if (index >= kServiceCount) return "unknown";
    return m_services[index].name;
}

const char* ServiceStatusManager::statusToString(ServiceStatus status) noexcept {
    switch (status) {
        case ServiceStatus::SS_ONLINE:   return "ONLINE";
        case ServiceStatus::SS_OFFLINE:  return "OFFLINE";
        case ServiceStatus::SS_DISABLED: return "DISABLED";
        case ServiceStatus::SS_ERROR:    return "ERROR";
        default:                         return "UNKNOWN";
    }
}

bool ServiceStatusManager::isOnline(ServiceId id) const noexcept {
    return getStatus(id) == ServiceStatus::SS_ONLINE;
}

bool ServiceStatusManager::isEnabled(ServiceId id) const noexcept {
    ServiceStatus s = getStatus(id);
    return s == ServiceStatus::SS_ONLINE || s == ServiceStatus::SS_OFFLINE ||
           s == ServiceStatus::SS_ERROR;
}

// ---- JSON serialization ----------------------------------------------------

String ServiceStatusManager::getStatusJson(uint32_t requestCount) const noexcept {
    String json;
    json.reserve(2048);

    // Total heap available on ESP32 (SPI RAM excluded)
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t usedHeap = (freeHeap <= totalHeap) ? (totalHeap - freeHeap) : 0;
    uint8_t memPct = (totalHeap > 0) ? static_cast<uint8_t>((usedHeap * 100ULL) / totalHeap) : 0;

    uint8_t cpuPct = 0;
    if (performanceManager.isInitialized()) {
        cpuPct = performanceManager.getCurrentSnapshot().cpuUsageEstimate;
    }

    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    String wifiIp = wifiConnected ? WiFi.localIP().toString() : String("");
    String wifiSsid = wifiConnected ? WiFi.SSID() : String("");

    uint32_t flashTotal = ESP.getFlashChipSize();
    uint32_t flashUsed = ESP.getSketchSize();

    json += "{";
    json += "\"running\":";
    json += "true";
    json += ",\"uptime\":";
    json += String(millis() / 1000);
    json += ",\"heap_free\":";
    json += String(freeHeap);
    json += ",\"wifi_connected\":";
    json += wifiConnected ? "true" : "false";
    json += ",\"version\":\"";
    json += aura::identity::kVersion;
    json += "\"";
    json += ",\"headless\":";
    json += m_headless ? "true" : "false";
    json += ",\"mode\":\"";
    json += getHeadlessModeString();
    json += "\"";
    json += ",\"modules\":";
    json += getModulesJson();
    json += ",\"connected_modules\":";
    json += getConnectedJson();
    json += ",\"disabled_modules\":";
    json += getDisabledJson();
    json += ",\"memory_usage\":";
    json += String(memPct);
    json += ",\"cpu_usage\":";
    json += String(cpuPct);
    json += ",\"wifi\":{";
    json += "\"connected\":";
    json += wifiConnected ? "true" : "false";
    json += ",\"ssid\":\"";
    json += wifiSsid;
    json += "\",\"ip\":\"";
    json += wifiIp;
    json += "\",\"rssi\":";
    json += String(WiFi.RSSI());
    json += "}";
    json += ",\"flash_used\":";
    json += String(flashUsed);
    json += ",\"flash_total\":";
    json += String(flashTotal);
    json += ",\"requests\":";
    json += String(requestCount);
    json += "}";
    return json;
}

String ServiceStatusManager::getModulesJson() const noexcept {
    String json;
    json.reserve(1024);
    json += "{";
    for (size_t i = 0; i < kServiceCount; ++i) {
        if (i > 0) json += ",";
        json += "\"";
        json += m_services[i].name;
        json += "\":\"";
        json += statusToString(m_services[i].status);
        json += "\"";
    }
    json += "}";
    return json;
}

String ServiceStatusManager::getConnectedJson() const noexcept {
    String json;
    json.reserve(512);
    json += "[";
    bool first = true;
    for (size_t i = 0; i < kServiceCount; ++i) {
        if (m_services[i].status == ServiceStatus::SS_ONLINE) {
            if (!first) json += ",";
            first = false;
            json += "\"";
            json += m_services[i].name;
            json += "\"";
        }
    }
    json += "]";
    return json;
}

String ServiceStatusManager::getDisabledJson() const noexcept {
    String json;
    json.reserve(512);
    json += "[";
    bool first = true;
    for (size_t i = 0; i < kServiceCount; ++i) {
        ServiceStatus s = m_services[i].status;
        if (s == ServiceStatus::SS_DISABLED || s == ServiceStatus::SS_OFFLINE ||
            s == ServiceStatus::SS_ERROR) {
            if (!first) json += ",";
            first = false;
            json += "\"";
            json += m_services[i].name;
            json += "\"";
        }
    }
    json += "]";
    return json;
}

// ---- Change tracking --------------------------------------------------------

bool ServiceStatusManager::hasPendingChanges() const noexcept {
    for (size_t i = 0; i < kServiceCount; ++i) {
        if (m_services[i].dirty) return true;
    }
    return false;
}

String ServiceStatusManager::takePendingChangesJson() noexcept {
    String json;
    json.reserve(512);
    json += "{\"type\":\"module_status\",\"modules\":{";
    bool first = true;
    for (size_t i = 0; i < kServiceCount; ++i) {
        if (!m_services[i].dirty) continue;
        if (!first) json += ",";
        first = false;
        json += "\"";
        json += m_services[i].name;
        json += "\":\"";
        json += statusToString(m_services[i].status);
        json += "\"";
        m_services[i].dirty = false;
    }
    json += "}}";
    return json;
}

void ServiceStatusManager::markAllChanged() noexcept {
    for (size_t i = 0; i < kServiceCount; ++i) {
        m_services[i].dirty = true;
    }
}
