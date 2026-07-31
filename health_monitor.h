#ifndef AURA_HEALTH_MONITOR_H
#define AURA_HEALTH_MONITOR_H

#include <Arduino.h>
#include <vector>
#include <esp_system.h>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "settings_manager.h"

struct HealthSnapshot {
    uint32_t freeHeap;
    uint32_t minHeap;
    uint32_t largestBlock;
    float fragmentation;
    int32_t wifiRSSI;
    float cpuLoad;
    unsigned long uptime;
    uint32_t flashUsed;
    uint32_t flashTotal;
    bool sdMounted;
    float temperature;
    esp_reset_reason_t resetReason;
};

enum class HealthAlertType : uint8_t {
    HEAP_LOW,
    WIFI_WEAK,
    SD_MISSING,
    FRAGMENTATION_HIGH,
    CPU_HIGH,
    TEMP_HIGH,
    FLASH_LOW,
    NONE
};

struct HealthAlert {
    HealthAlertType type;
    String message;
    float value;
    float threshold;
    unsigned long timestamp;
};

class HealthMonitor {
public:
    HealthMonitor() noexcept;
    ~HealthMonitor() noexcept;

    HealthMonitor(const HealthMonitor&) = delete;
    HealthMonitor& operator=(const HealthMonitor&) = delete;
    HealthMonitor(HealthMonitor&&) = delete;
    HealthMonitor& operator=(HealthMonitor&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    [[nodiscard]] HealthSnapshot getSnapshot() const noexcept;
    [[nodiscard]] const std::vector<HealthAlert>& getActiveAlerts() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;

    void setHeapWarningThreshold(uint32_t bytes) noexcept;
    void setWifiRssiThreshold(int32_t rssi) noexcept;
    void setFragmentationThreshold(float percent) noexcept;

private:
    static constexpr const char* kLogCategory = "HealthMonitor";
    static constexpr unsigned long kCheckIntervalMs = 30000;
    static constexpr unsigned long kAlertCooldownMs = 300000;
    static constexpr uint32_t kDefaultHeapWarning = 32768;
    static constexpr int32_t kDefaultWifiRssiWarning = -80;
    static constexpr float kDefaultFragmentationWarning = 30.0f;

    void checkHeap() noexcept;
    void checkWiFi() noexcept;
    void checkSD() noexcept;
    void checkFragmentation() noexcept;
    void checkCPU() noexcept;
    void checkFlash() noexcept;
    void publishAlert(HealthAlertType type, const String& message, float value, float threshold) noexcept;

    bool m_initialized;
    unsigned long m_lastCheckTime;
    static constexpr size_t kAlertTypeCount = static_cast<size_t>(HealthAlertType::NONE);
    unsigned long m_lastAlertTimes[kAlertTypeCount];
    HealthSnapshot m_snapshot;
    std::vector<HealthAlert> m_activeAlerts;
    uint32_t m_heapWarningThreshold;
    int32_t m_wifiRssiThreshold;
    float m_fragmentationThreshold;
};

extern HealthMonitor healthMonitor;

#endif // AURA_HEALTH_MONITOR_H
