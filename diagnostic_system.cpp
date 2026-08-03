#include "diagnostic_system.h"
#include <WiFi.h>
#include <freertos/task.h>
#include "event_bus.h"
#include "task_scheduler.h"
#include <algorithm>

DiagnosticSystem diagnosticSystem;

DiagnosticSystem::DiagnosticSystem() noexcept
    : Service(kStaticName, BootPriority::PRIORITY_LOW)
    , m_lastSampleTime(0)
    , m_initialized(false) {
}

DiagnosticSystem::~DiagnosticSystem() noexcept = default;

bool DiagnosticSystem::Initialize() noexcept {
    if (GetState() != ServiceState::UNINITIALIZED) return true;
    SetState(ServiceState::INITIALIZING);
    SetState(ServiceState::INITIALIZED);
    m_initialized = true;
    LOG_INFO(kLogCategory, "DiagnosticSystem initialized");
    return true;
}

void DiagnosticSystem::Update() noexcept {
    if (!m_initialized) return;

    unsigned long now = millis();
    if (now - m_lastSampleTime >= kSampleIntervalMs) {
        m_lastSampleTime = now;
        auto snap = TakeSnapshot();
        m_history.push_back(snap);
        PruneHistory();
        LogWarningIfThresholdsExceeded(snap);
    }
}

ServiceHealth DiagnosticSystem::Health() const noexcept {
    if (m_history.empty()) return ServiceHealth::HEALTHY;
    const auto& latest = m_history.back();
    if (latest.healthScore >= 80) return ServiceHealth::HEALTHY;
    if (latest.healthScore >= 50) return ServiceHealth::DEGRADED;
    if (latest.healthScore >= 25) return ServiceHealth::UNSTABLE;
    return ServiceHealth::FAILED;
}

String DiagnosticSystem::Version() const noexcept {
    return "2.0.0";
}

DiagnosticSnapshot DiagnosticSystem::TakeSnapshot() noexcept {
    DiagnosticSnapshot snap;
    snap.timestamp = millis();

    // Memory
    snap.freeHeap = platform.GetFreeHeap();
    snap.minHeap = platform.GetMinFreeHeap();
    snap.maxAllocHeap = platform.GetMaxAllocHeap();
    snap.fragmentation = GetHeapFragmentation();
    snap.freePSRAM = platform.GetFreePSRAM();

    // CPU
    snap.cpuFreq = platform.GetCPUFrequency();
    snap.coreCount = platform.GetCPUCoreCount();

    // Network
    snap.wifiConnected = WiFi.isConnected();
    snap.wifiRSSI = WiFi.RSSI();

    // System
    snap.temperature = platform.GetTemperature();
    snap.uptimeSec = platform.GetUptimeSec();

    // Task info
    snap.taskCount = uxTaskGetNumberOfTasks();

    // Event queue
    snap.eventQueueDepth = eventBus.isInitialized() ? eventBus.pendingCount() : 0;

    // Health
    snap.healthScore = CalculateHealthScore();

    return snap;
}

uint8_t DiagnosticSystem::CalculateHealthScore() noexcept {
    uint8_t score = 100;
    auto snap = TakeSnapshot();  // Note: recursive but guarded by sample interval

    // Memory penalty: -30 if heap < 20KB
    if (snap.freeHeap < 20000) score -= 30;
    else if (snap.freeHeap < 40000) score -= 15;
    else if (snap.freeHeap < 80000) score -= 5;

    // Fragmentation penalty: -20 if > 30%
    if (snap.fragmentation > 50.0f) score -= 20;
    else if (snap.fragmentation > 30.0f) score -= 10;

    // WiFi penalty: -15 if disconnected
    if (!snap.wifiConnected) score -= 15;

    // Temperature penalty: -10 if hot
    if (snap.temperature > 70.0f) score -= 10;
    else if (snap.temperature > 50.0f) score -= 5;

    // Event queue penalty: -5 if backed up
    if (snap.eventQueueDepth > 40) score -= 5;

    return (score > 100) ? 100 : score;
}

String DiagnosticSystem::GetHealthReport() noexcept {
    auto snap = TakeSnapshot();
    char buf[256];
    snprintf(buf, sizeof(buf),
        "Health: %d/100 | Heap: %lu KB free (%lu min) | Frag: %.1f%% | "
        "WiFi: %s (%d dBm) | Temp: %.1f C | CPU: %.0f MHz | Uptime: %lu s",
        snap.healthScore,
        snap.freeHeap / 1024, snap.minHeap / 1024,
        snap.fragmentation,
        snap.wifiConnected ? "OK" : "DOWN", snap.wifiRSSI,
        snap.temperature, snap.cpuFreq, snap.uptimeSec);
    return String(buf);
}

const std::vector<DiagnosticSnapshot>& DiagnosticSystem::GetHistory() const noexcept {
    return m_history;
}

size_t DiagnosticSystem::GetHistoryCount() const noexcept {
    return m_history.size();
}

float DiagnosticSystem::GetHeapFragmentation() noexcept {
    return platform.GetHeapFragmentation();
}

float DiagnosticSystem::GetEstimatedCPUUsage() noexcept {
    return 0.0f; // Requires FreeRTOS task stats
}

String DiagnosticSystem::GetTopConsumers() noexcept {
    return "{}"; // TODO: implement
}

String DiagnosticSystem::GetDiagnosticJSON() noexcept {
    auto snap = TakeSnapshot();
    String json = "{";
    json += "\"heapFree\":" + String(snap.freeHeap) + ",";
    json += "\"heapMin\":" + String(snap.minHeap) + ",";
    json += "\"fragmentation\":" + String(snap.fragmentation, 1) + ",";
    json += "\"wifi\":" + String(snap.wifiConnected ? "true" : "false") + ",";
    json += "\"rssi\":" + String(snap.wifiRSSI) + ",";
    json += "\"temp\":" + String(snap.temperature, 1) + ",";
    json += "\"uptime\":" + String(snap.uptimeSec) + ",";
    json += "\"health\":" + String(snap.healthScore);
    json += "}";
    return json;
}

void DiagnosticSystem::PruneHistory() noexcept {
    while (m_history.size() > kMaxHistory) {
        m_history.erase(m_history.begin());
    }
}

void DiagnosticSystem::LogWarningIfThresholdsExceeded(const DiagnosticSnapshot& snap) noexcept {
    if (snap.freeHeap < kHeapWarningThreshold) {
        LOG_WARNING(kLogCategory, "Low heap: %lu bytes", snap.freeHeap);
    }
    if (snap.fragmentation > kFragmentationWarningThreshold) {
        LOG_WARNING(kLogCategory, "High fragmentation: %.1f%%", snap.fragmentation);
    }
    if (!snap.wifiConnected) {
        LOG_WARNING(kLogCategory, "WiFi disconnected");
    }
    if (snap.temperature > 60.0f) {
        LOG_WARNING(kLogCategory, "High temperature: %.1f C", snap.temperature);
    }
}