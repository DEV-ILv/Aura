#ifndef AURA_DIAGNOSTIC_SYSTEM_H
#define AURA_DIAGNOSTIC_SYSTEM_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "service.h"
#include "platform_abstraction.h"

struct DiagnosticSnapshot {
    unsigned long timestamp;
    uint32_t freeHeap;
    uint32_t minHeap;
    uint32_t maxAllocHeap;
    float fragmentation;
    float cpuFreq;
    int coreCount;
    bool wifiConnected;
    int wifiRSSI;
    float temperature;
    size_t freePSRAM;
    uint32_t uptimeSec;
    uint8_t healthScore;       // 0-100
    uint32_t taskCount;
    uint32_t eventQueueDepth;

    DiagnosticSnapshot() noexcept
        : timestamp(0), freeHeap(0), minHeap(0), maxAllocHeap(0),
          fragmentation(0), cpuFreq(0), coreCount(0), wifiConnected(false),
          wifiRSSI(0), temperature(0), freePSRAM(0), uptimeSec(0),
          healthScore(100), taskCount(0), eventQueueDepth(0) {}
};

class DiagnosticSystem : public Service {
public:
    DiagnosticSystem() noexcept;
    ~DiagnosticSystem() noexcept;

    bool Initialize() noexcept override;
    void Update() noexcept override;
    ServiceHealth Health() const noexcept override;
    String Version() const noexcept override;

    // Take a snapshot
    DiagnosticSnapshot TakeSnapshot() noexcept;

    // Health score (0-100)
    uint8_t CalculateHealthScore() noexcept;
    String GetHealthReport() noexcept;

    // Historical data
    const std::vector<DiagnosticSnapshot>& GetHistory() const noexcept;
    size_t GetHistoryCount() const noexcept;

    // Specific monitors
    float GetHeapFragmentation() noexcept;
    float GetEstimatedCPUUsage() noexcept;
    String GetTopConsumers() noexcept;
    String GetDiagnosticJSON() noexcept;

    static constexpr const char* kStaticName = "DiagnosticSystem";

private:
    void PruneHistory() noexcept;
    void LogWarningIfThresholdsExceeded(const DiagnosticSnapshot& snap) noexcept;

    static constexpr const char* kLogCategory = "Diagnostics";
    static constexpr size_t kMaxHistory = 120;       // 10 min at 5s intervals
    static constexpr unsigned long kSampleIntervalMs = 5000;
    static constexpr uint32_t kHeapWarningThreshold = 30000;
    static constexpr float kFragmentationWarningThreshold = 30.0f;

    std::vector<DiagnosticSnapshot> m_history;
    unsigned long m_lastSampleTime;
    bool m_initialized;
};

extern DiagnosticSystem diagnosticSystem;

#endif