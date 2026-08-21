#ifndef AURA_RESILIENCE_MANAGER_H
#define AURA_RESILIENCE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "logger.h"
#include "service.h"
#include "event_bus.h"

enum class FailureType : uint8_t {
    WIFI_LOSS,
    CLOUD_TIMEOUT,
    AI_FAILURE,
    SD_FAILURE,
    RENDERER_CRASH,
    MEMORY_CORRUPTION,
    WATCHDOG_RESET,
    UNEXPECTED_REBOOT,
    MODULE_HANG,
    EVENT_QUEUE_OVERFLOW,
    HEAP_EXHAUSTION,
    STORAGE_FULL
};

struct FailureRecoveryPlan {
    FailureType type;
    String description;
    uint8_t maxRetries;
    bool restartModule;
    bool rebootSystem;
    bool safeModeOnFailure;
    unsigned long cooldownMs;

    FailureRecoveryPlan() noexcept
        : type(FailureType::WIFI_LOSS), maxRetries(3), restartModule(false),
          rebootSystem(false), safeModeOnFailure(false), cooldownMs(30000) {}
};

class ResilienceManager : public Service {
public:
    ResilienceManager() noexcept;
    ~ResilienceManager() noexcept;

    bool Initialize() noexcept override;
    void Update() noexcept override;

    // Register recovery plan for a failure type
    void RegisterPlan(FailureType type, const FailureRecoveryPlan& plan) noexcept;
    FailureRecoveryPlan GetPlan(FailureType type) const noexcept;

    // Report a failure (triggers recovery)
    void ReportFailure(FailureType type, const String& source,
                        const String& details = "") noexcept;

    // Recovery
    bool AttemptRecovery(FailureType type) noexcept;
    bool RecoverWiFi() noexcept;
    bool RecoverSD() noexcept;
    bool RecoverRenderer() noexcept;
    bool RecoverAI() noexcept;
    bool RecoverMemory() noexcept;

    // Health checks
    bool CheckAllSystems() noexcept;
    String GetResilienceReport() noexcept;

    // Stats
    uint32_t GetTotalFailures() const noexcept;
    uint32_t GetTotalRecoveries() const noexcept;
    uint32_t GetConsecutiveFailures(FailureType type) const noexcept;
    float GetRecoveryRate() const noexcept;

    void HandleEvent(const String& eventType, const String& eventData) noexcept override;

    static constexpr const char* kStaticName = "ResilienceManager";

private:
    struct FailureRecord {
        FailureType type;
        unsigned long timestamp;
        String source;
        String details;
        uint32_t attemptCount;
        bool recovered;

        FailureRecord() noexcept : timestamp(0), attemptCount(0), recovered(false) {}
    };

    FailureRecoveryPlan GetDefaultPlan(FailureType type) const noexcept;
    void PruneRecords() noexcept;
    void FinalizeWifiRecovery(bool recovered) noexcept;

    static constexpr const char* kLogCategory = "Resilience";
    static constexpr size_t kMaxRecords = 100;
    static constexpr size_t kMaxFailureCount = 10;
    static constexpr unsigned long kCleanupIntervalMs = 60000UL;
    static constexpr unsigned long kRecoveryWindowMs = 10000UL;

    FailureRecoveryPlan m_plans[12];
    std::vector<FailureRecord> m_records;
    uint32_t m_totalFailures;
    uint32_t m_totalRecoveries;
    unsigned long m_lastCleanup;
    bool m_initialized;
    bool m_wifiRecoverPending;
    unsigned long m_wifiRecoverStarted;
};

extern ResilienceManager resilienceManager;

#endif