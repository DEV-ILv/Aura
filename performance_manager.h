#ifndef AURA_PERFORMANCE_MANAGER_H
#define AURA_PERFORMANCE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "logger.h"

/**
 * @struct PerformanceSnapshot
 * @brief Single performance data point
 */
struct PerformanceSnapshot {
    unsigned long timestamp;
    uint32_t freeHeap;
    uint32_t minHeap;
    uint32_t maxAllocHeap;
    uint32_t freeSketchSpace;
    uint32_t sketchSize;
    int32_t wifiRSSI;
    float cpuFreqMHz;
    uint8_t cpuUsageEstimate;   ///< 0-100 estimated CPU load
    uint32_t apiLatencyMs;      ///< Last API call latency
    size_t memoryCount;         ///< Memory manager entries
    size_t conversationCount;   ///< Conversation history entries
    // Developer extension fields
    uint32_t stackHighWater;    ///< Minimum stack bytes remaining
    uint8_t taskCount;          ///< Active FreeRTOS tasks
    uint16_t queueCount;        ///< Number of queues
    uint32_t fragPercent;       ///< Heap fragmentation percentage
    uint32_t wdtResetCount;     ///< Watchdog reset count
    uint32_t apiLatencyAvg;     ///< Average API latency
    uint32_t apiLatencyMax;     ///< Max API latency
    uint32_t apiLatencyMin;     ///< Min API latency
    uint32_t apiCallCount;      ///< API call count

    PerformanceSnapshot() noexcept
        : timestamp(0), freeHeap(0), minHeap(0), maxAllocHeap(0),
          freeSketchSpace(0), sketchSize(0), wifiRSSI(0), cpuFreqMHz(0),
          cpuUsageEstimate(0), apiLatencyMs(0), memoryCount(0), conversationCount(0),
          stackHighWater(0), taskCount(0), queueCount(0), fragPercent(0),
          wdtResetCount(0), apiLatencyAvg(0), apiLatencyMax(0), apiLatencyMin(0), apiCallCount(0) {}
};

/**
 * @class PerformanceManager
 * @brief System performance monitoring and metrics
 *
 * Tracks heap, CPU, WiFi, flash usage, API latency, and memory statistics.
 * Exposes data via Web Portal and REST API.
 */
class PerformanceManager {
public:
    PerformanceManager() noexcept;
    ~PerformanceManager() noexcept;

    PerformanceManager(const PerformanceManager&) = delete;
    PerformanceManager& operator=(const PerformanceManager&) = delete;
    PerformanceManager(PerformanceManager&&) = delete;
    PerformanceManager& operator=(PerformanceManager&&) = delete;

    /**
     * @brief Initialize performance manager
     * @return true if initialized
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Update performance metrics
     */
    void update() noexcept;

    /**
     * @brief Get current performance snapshot as JSON
     * @return JSON string
     */
    [[nodiscard]] String getMetricsJson() const noexcept;

    /**
     * @brief Get current snapshot
     * @return Const reference to current snapshot
     */
    [[nodiscard]] const PerformanceSnapshot& getCurrentSnapshot() const noexcept;

    /**
     * @brief Record API latency
     * @param latencyMs Latency in milliseconds
     */
    void recordApiLatency(uint32_t latencyMs) noexcept;

    /**
     * @brief Get estimated free RAM
     * @return Free RAM in bytes
     */
    [[nodiscard]] uint32_t getFreeRam() const noexcept;

    // Developer extension methods
    [[nodiscard]] String getDeveloperMetricsJson() const noexcept;
    [[nodiscard]] String exportDiagnostics() const noexcept;
    [[nodiscard]] uint32_t getStackHighWater() const noexcept;
    [[nodiscard]] uint8_t getTaskCount() const noexcept;
    [[nodiscard]] uint32_t getFragPercent() const noexcept;

    /**
     * @brief Check if initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

private:
    static constexpr const char* kLogCategory = "Performance";
    static constexpr unsigned long kSampleIntervalMs = PERF_SAMPLE_INTERVAL_MS;

    void takeSnapshot() noexcept;

    bool m_initialized;
    PerformanceSnapshot m_current;
    unsigned long m_lastSampleTime;
};

extern PerformanceManager performanceManager;

#endif // AURA_PERFORMANCE_MANAGER_H
