#include "performance_manager.h"
#include <WiFi.h>
#include "memory_manager.h"
#include "conversation_manager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

PerformanceManager performanceManager;

PerformanceManager::PerformanceManager() noexcept
    : m_initialized(false), m_lastSampleTime(0) {
}

PerformanceManager::~PerformanceManager() noexcept {
}

bool PerformanceManager::initialize() noexcept {
    if (m_initialized) {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    takeSnapshot();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized");
    return true;
}

void PerformanceManager::update() noexcept {
    if (!m_initialized) return;

    unsigned long now = millis();
    if (now - m_lastSampleTime >= kSampleIntervalMs) {
        m_lastSampleTime = now;
        takeSnapshot();
    }
}

String PerformanceManager::getMetricsJson() const noexcept {
    String json;
    json.reserve(512);
    json += "{";
    json += "\"free_heap\":" + String(m_current.freeHeap) + ",";
    json += "\"min_heap\":" + String(m_current.minHeap) + ",";
    json += "\"max_alloc\":" + String(m_current.maxAllocHeap) + ",";
    json += "\"free_sketch\":" + String(m_current.freeSketchSpace) + ",";
    json += "\"sketch_size\":" + String(m_current.sketchSize) + ",";
    json += "\"wifi_rssi\":" + String(m_current.wifiRSSI) + ",";
    json += "\"cpu_mhz\":" + String(m_current.cpuFreqMHz, 1) + ",";
    json += "\"cpu_usage\":" + String(m_current.cpuUsageEstimate) + ",";
    json += "\"api_latency_ms\":" + String(m_current.apiLatencyMs) + ",";
    json += "\"memories\":" + String(m_current.memoryCount) + ",";
    json += "\"conversations\":" + String(m_current.conversationCount);
    json += "}";
    return json;
}

const PerformanceSnapshot& PerformanceManager::getCurrentSnapshot() const noexcept {
    return m_current;
}

void PerformanceManager::recordApiLatency(uint32_t latencyMs) noexcept {
    m_current.apiLatencyMs = latencyMs;
}

uint32_t PerformanceManager::getFreeRam() const noexcept {
    return ESP.getFreeHeap();
}

String PerformanceManager::getDeveloperMetricsJson() const noexcept {
    String json;
    json.reserve(1024);
    json += "{";
    json += "\"free_heap\":" + String(m_current.freeHeap) + ",";
    json += "\"min_heap\":" + String(m_current.minHeap) + ",";
    json += "\"max_alloc\":" + String(m_current.maxAllocHeap) + ",";
    json += "\"stack_hwm\":" + String(m_current.stackHighWater) + ",";
    json += "\"task_count\":" + String(m_current.taskCount) + ",";
    json += "\"queue_count\":" + String(m_current.queueCount) + ",";
    json += "\"frag_pct\":" + String(m_current.fragPercent) + ",";
    json += "\"wdt_resets\":" + String(m_current.wdtResetCount) + ",";
    json += "\"api_avg\":" + String(m_current.apiLatencyAvg) + ",";
    json += "\"api_max\":" + String(m_current.apiLatencyMax) + ",";
    json += "\"api_min\":" + String(m_current.apiLatencyMin) + ",";
    json += "\"api_calls\":" + String(m_current.apiCallCount) + ",";
    json += "\"cpu_usage\":" + String(m_current.cpuUsageEstimate) + ",";
    json += "\"cpu_mhz\":" + String(m_current.cpuFreqMHz, 1) + ",";
    json += "\"wifi_rssi\":" + String(m_current.wifiRSSI);
    json += "}";
    return json;
}

String PerformanceManager::exportDiagnostics() const noexcept {
    String diag;
    diag.reserve(2048);
    diag += "AURA Diagnostics Report\n";
    diag += "======================\n";
    diag += "Timestamp: " + String(millis()) + " ms\n";
    diag += "Free Heap: " + String(m_current.freeHeap) + " bytes\n";
    diag += "Min Heap: " + String(m_current.minHeap) + " bytes\n";
    diag += "Max Alloc: " + String(m_current.maxAllocHeap) + " bytes\n";
    diag += "Fragmentation: " + String(m_current.fragPercent) + "%\n";
    diag += "Stack HWM: " + String(m_current.stackHighWater) + " bytes\n";
    diag += "Tasks: " + String(m_current.taskCount) + "\n";
    diag += "Queues: " + String(m_current.queueCount) + "\n";
    diag += "CPU Freq: " + String(m_current.cpuFreqMHz, 1) + " MHz\n";
    diag += "CPU Load: " + String(m_current.cpuUsageEstimate) + "%\n";
    diag += "WiFi RSSI: " + String(m_current.wifiRSSI) + " dBm\n";
    diag += "API Calls: " + String(m_current.apiCallCount) + "\n";
    diag += "API Latency Avg/Max/Min: " + String(m_current.apiLatencyAvg) + "/"
            + String(m_current.apiLatencyMax) + "/" + String(m_current.apiLatencyMin) + " ms\n";
    diag += "WDT Resets: " + String(m_current.wdtResetCount) + "\n";
    diag += "Free Sketch: " + String(m_current.freeSketchSpace) + " bytes\n";
    diag += "Sketch Size: " + String(m_current.sketchSize) + " bytes\n";
    diag += "Memories: " + String(m_current.memoryCount) + "\n";
    diag += "Conversations: " + String(m_current.conversationCount) + "\n";
    return diag;
}

uint32_t PerformanceManager::getStackHighWater() const noexcept {
    return m_current.stackHighWater;
}

uint8_t PerformanceManager::getTaskCount() const noexcept {
    return m_current.taskCount;
}

uint32_t PerformanceManager::getFragPercent() const noexcept {
    return m_current.fragPercent;
}

bool PerformanceManager::isInitialized() const noexcept {
    return m_initialized;
}

void PerformanceManager::takeSnapshot() noexcept {
    m_current.timestamp = millis();
    m_current.freeHeap = ESP.getFreeHeap();
    m_current.minHeap = ESP.getMinFreeHeap();
    m_current.maxAllocHeap = ESP.getMaxAllocHeap();
    m_current.freeSketchSpace = ESP.getFreeSketchSpace();
    m_current.sketchSize = ESP.getSketchSize();
    m_current.wifiRSSI = WiFi.RSSI();
    m_current.cpuFreqMHz = static_cast<float>(ESP.getCpuFreqMHz());

    // Developer metrics
    m_current.stackHighWater = uxTaskGetStackHighWaterMark(nullptr);
    m_current.taskCount = static_cast<uint8_t>(uxTaskGetNumberOfTasks());
    // Estimate queue count (simplified)
    m_current.queueCount = 4; // rough estimate
    m_current.fragPercent = (m_current.maxAllocHeap > 0) ?
        (100 - (m_current.maxAllocHeap * 100 / (m_current.freeHeap + m_current.maxAllocHeap))) : 0;
    m_current.wdtResetCount = 0; // Cleared on boot

    // CPU usage estimate
    static unsigned long lastCheck = 0;
    static uint32_t lastIdle = 0;
    unsigned long now = m_current.timestamp;
    if (lastCheck > 0 && now > lastCheck) {
        unsigned long elapsed = now - lastCheck;
        if (elapsed > 0) {
            unsigned long busy = ESP.getCycleCount() - lastIdle;
            m_current.cpuUsageEstimate = static_cast<uint8_t>(
                constrain(map(static_cast<unsigned long>(busy >> 10), 0UL,
                           static_cast<unsigned long>(elapsed * 240), 0, 100), 0, 100));
        }
    }
    lastCheck = now;
    lastIdle = ESP.getCycleCount();

    m_current.memoryCount = memoryManager.isInitialized() ? memoryManager.memoryCount() : 0;
    m_current.conversationCount = 0;
}
