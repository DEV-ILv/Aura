#include "health_monitor.h"
#include <esp_heap_caps.h>
#include <WiFi.h>
#include "storage_manager.h"

HealthMonitor healthMonitor;

HealthMonitor::HealthMonitor() noexcept
    : m_initialized(false)
    , m_lastCheckTime(0)
    , m_heapWarningThreshold(kDefaultHeapWarning)
    , m_wifiRssiThreshold(kDefaultWifiRssiWarning)
    , m_fragmentationThreshold(kDefaultFragmentationWarning) {
    for (auto& t : m_lastAlertTimes) t = 0;
    m_snapshot = {};
}

HealthMonitor::~HealthMonitor() noexcept = default;

bool HealthMonitor::initialize() noexcept {
    if (m_initialized) return true;
    m_initialized = true;
    m_lastCheckTime = millis();
    LOG_INFO(kLogCategory, "HealthMonitor initialized");
    return true;
}

void HealthMonitor::update() noexcept {
    if (!m_initialized) return;
    unsigned long now = millis();
    if (now - m_lastCheckTime < kCheckIntervalMs) return;
    m_lastCheckTime = now;

    m_snapshot.freeHeap = ESP.getFreeHeap();
    m_snapshot.minHeap = ESP.getMinFreeHeap();
    m_snapshot.largestBlock = ESP.getMaxAllocHeap();
    m_snapshot.uptime = millis() / 1000;
    m_snapshot.resetReason = esp_reset_reason();
    m_snapshot.wifiRSSI = WiFi.RSSI();

    multi_heap_info_t heapInfo;
    heap_caps_get_info(&heapInfo, MALLOC_CAP_8BIT);
    size_t totalHeap = heapInfo.total_free_bytes + heapInfo.total_allocated_bytes;
    m_snapshot.fragmentation = totalHeap > 0
        ? (1.0f - (float)heapInfo.largest_free_block / (float)totalHeap) * 100.0f
        : 0.0f;

    m_snapshot.flashUsed = ESP.getSketchSize();
    m_snapshot.flashTotal = ESP.getFlashChipSize();

    m_snapshot.sdMounted = storageManager.isInitialized() && storageManager.isSDMounted();
    if (m_snapshot.sdMounted) {
        size_t totalBytes = 0, usedBytes = 0, freeBytes = 0;
        storageManager.getStatistics(StorageType::SD_CARD, totalBytes, usedBytes, freeBytes);
        m_snapshot.flashUsed = usedBytes;
        m_snapshot.flashTotal = totalBytes + usedBytes;
    }

    checkHeap();
    checkWiFi();
    checkSD();
    checkFragmentation();
    checkCPU();
    checkFlash();
}

HealthSnapshot HealthMonitor::getSnapshot() const noexcept {
    return m_snapshot;
}

const std::vector<HealthAlert>& HealthMonitor::getActiveAlerts() const noexcept {
    return m_activeAlerts;
}

bool HealthMonitor::isInitialized() const noexcept {
    return m_initialized;
}

void HealthMonitor::setHeapWarningThreshold(uint32_t bytes) noexcept {
    m_heapWarningThreshold = bytes;
}

void HealthMonitor::setWifiRssiThreshold(int32_t rssi) noexcept {
    m_wifiRssiThreshold = rssi;
}

void HealthMonitor::setFragmentationThreshold(float percent) noexcept {
    m_fragmentationThreshold = percent;
}

void HealthMonitor::checkHeap() noexcept {
    if (m_snapshot.freeHeap < m_heapWarningThreshold && m_snapshot.freeHeap > 0) {
        unsigned long now = millis();
        if (now - m_lastAlertTimes[0] >= kAlertCooldownMs) {
            m_lastAlertTimes[0] = now;
            char buf[48];
            snprintf(buf, sizeof(buf), "Low heap: %u bytes free", m_snapshot.freeHeap);
            publishAlert(HealthAlertType::HEAP_LOW, buf, (float)m_snapshot.freeHeap, (float)m_heapWarningThreshold);
        }
    }
}

void HealthMonitor::checkWiFi() noexcept {
    if (m_snapshot.wifiRSSI < m_wifiRssiThreshold && m_snapshot.wifiRSSI != 0) {
        unsigned long now = millis();
        if (now - m_lastAlertTimes[1] >= kAlertCooldownMs) {
            m_lastAlertTimes[1] = now;
            char buf[48];
            snprintf(buf, sizeof(buf), "Weak WiFi: RSSI %ld dBm", (long)m_snapshot.wifiRSSI);
            publishAlert(HealthAlertType::WIFI_WEAK, buf, (float)m_snapshot.wifiRSSI, (float)m_wifiRssiThreshold);
        }
    }
}

void HealthMonitor::checkSD() noexcept {
    if (!m_snapshot.sdMounted) {
        unsigned long now = millis();
        if (now - m_lastAlertTimes[2] >= kAlertCooldownMs) {
            m_lastAlertTimes[2] = now;
            publishAlert(HealthAlertType::SD_MISSING, "SD card not mounted", 0, 0);
        }
    }
}

void HealthMonitor::checkFragmentation() noexcept {
    if (m_snapshot.fragmentation > m_fragmentationThreshold) {
        unsigned long now = millis();
        if (now - m_lastAlertTimes[3] >= kAlertCooldownMs) {
            m_lastAlertTimes[3] = now;
            char buf[48];
            snprintf(buf, sizeof(buf), "High fragmentation: %.1f%%", m_snapshot.fragmentation);
            publishAlert(HealthAlertType::FRAGMENTATION_HIGH, buf, m_snapshot.fragmentation, m_fragmentationThreshold);
        }
    }
}

void HealthMonitor::checkCPU() noexcept {
    // CPU load estimation using call-interval jitter as a proxy
    // Accurate per-task runtime stats require FreeRTOS config flags
    // that may not be enabled in all builds, so we use a lightweight
    // heuristic based on HealthMonitor's own scheduling consistency.
    unsigned long now = millis();
    static unsigned long lastCheckMs = 0;
    static unsigned long lastSampleMs = 0;

    if (lastCheckMs == 0) {
        lastCheckMs = now;
        lastSampleMs = now;
        m_snapshot.cpuLoad = 0.0f;
        return;
    }

    unsigned long elapsed = now - lastSampleMs;
    if (elapsed >= 10000) {
        unsigned long callDelta = now - lastCheckMs;
        unsigned long expectedDelta = kCheckIntervalMs;
        float load = 0.0f;
        // If the call interval exceeds the expected cadence significantly
        // the system may be busy. Use a simple saturation model.
        if (expectedDelta > 0 && callDelta > expectedDelta * 2) {
            load = 50.0f + ((float)(callDelta - expectedDelta * 2) / (float)expectedDelta) * 50.0f;
            if (load > 95.0f) load = 95.0f;
        }
        m_snapshot.cpuLoad = load;
        lastSampleMs = now;
    }
    lastCheckMs = now;
}

void HealthMonitor::checkFlash() noexcept {
    if (m_snapshot.flashTotal > 0) {
        float usedPct = 100.0f * (float)m_snapshot.flashUsed / (float)m_snapshot.flashTotal;
        if (usedPct > 90.0f) {
            unsigned long now = millis();
            if (now - m_lastAlertTimes[5] >= kAlertCooldownMs) {
                m_lastAlertTimes[5] = now;
                char buf[48];
                snprintf(buf, sizeof(buf), "Flash nearly full: %.0f%%", usedPct);
                publishAlert(HealthAlertType::FLASH_LOW, buf, usedPct, 90.0f);
            }
        }
    }
}

void HealthMonitor::publishAlert(HealthAlertType type, const String& message, float value, float threshold) noexcept {
    HealthAlert alert;
    alert.type = type;
    alert.message = message;
    alert.value = value;
    alert.threshold = threshold;
    alert.timestamp = millis();
    m_activeAlerts.push_back(alert);
    if (m_activeAlerts.size() > 10) {
        m_activeAlerts.erase(m_activeAlerts.begin());
    }

    if (eventBus.isInitialized()) {
        EventType et = EventType::HEALTH_STATUS;
        switch (type) {
            case HealthAlertType::HEAP_LOW: et = EventType::HEALTH_HEAP_LOW; break;
            case HealthAlertType::WIFI_WEAK: et = EventType::HEALTH_WIFI_WEAK; break;
            case HealthAlertType::SD_MISSING: et = EventType::HEALTH_SD_MISSING; break;
            case HealthAlertType::FRAGMENTATION_HIGH: et = EventType::HEALTH_FRAGMENTATION_HIGH; break;
            case HealthAlertType::CPU_HIGH: et = EventType::HEALTH_CPU_HIGH; break;
            case HealthAlertType::FLASH_LOW: et = EventType::HEALTH_FLASH_LOW; break;
            default: break;
        }
        String data = "{\"message\":\"" + message + "\",\"value\":" + String(value) + ",\"threshold\":" + String(threshold) + "}";
        eventBus.publish(et, "HealthMonitor", data);
    }

    LOG_WARN(kLogCategory, "%s", message.c_str());
}
