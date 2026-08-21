#ifndef AURA_HEALTH_MANAGER_H
#define AURA_HEALTH_MANAGER_H

#include <Arduino.h>
#include <cstdint>
#include "config.h"

enum class SubsystemId : uint8_t {
    WIFI = 0,
    WEBSOCKET,
    MICROPHONE,
    STT,
    AI,
    TTS,
    SPEAKER,
    OLED,
    SD,
    NVS,
    OTA,
    TASKS,
    COUNT
};

enum class SubsystemHealth : uint8_t {
    HEALTHY = 0,
    DEGRADED,
    FAILED,
    RECOVERING,
    OFFLINE
};

struct RecoveryEvent {
    unsigned long timestampMs;
    SubsystemId subsystem;
    const char* reason;
    uint8_t attempt;
    bool success;
    uint32_t freeHeap;
    uint32_t maxBlock;

    RecoveryEvent() noexcept
        : timestampMs(0), subsystem(SubsystemId::COUNT), reason(""), attempt(0),
          success(false), freeHeap(0), maxBlock(0) {}
};

class HealthManager {
public:
    void initialize() noexcept;
    void update() noexcept;

    void registerRecovery(SubsystemId id, void (*action)(void)) noexcept;
    void reportFailure(SubsystemId id, const char* reason) noexcept;
    void reportHealthy(SubsystemId id) noexcept;
    void markDisabled(SubsystemId id) noexcept;

    [[nodiscard]] SubsystemHealth getHealth(SubsystemId id) const noexcept;
    [[nodiscard]] const RecoveryEvent* lastEvent(SubsystemId id) const noexcept;
    [[nodiscard]] uint32_t getRecoveryCount() const noexcept;
    [[nodiscard]] uint8_t getAttempts(SubsystemId id) const noexcept;

    static const char* subsystemName(SubsystemId id) noexcept;
    static const char* healthName(SubsystemHealth h) noexcept;

private:
    struct SubsystemState {
        SubsystemHealth health{SubsystemHealth::HEALTHY};
        uint8_t attempts{0};
        unsigned long lastFailureMs{0};
        unsigned long lastRecoveryMs{0};
        unsigned long cooldownUntilMs{0};
        unsigned long lastFailedMs{0};
        void (*recover)(void){nullptr};
        RecoveryEvent last;
    };

    void runRecovery(SubsystemState& st, SubsystemId id) noexcept;
    void logRecovery(const RecoveryEvent& ev, bool ok) noexcept;

    static constexpr uint8_t kMaxAttempts = AURA_HEALTH_MAX_ATTEMPTS;
    static constexpr unsigned long kCooldownMs = AURA_HEALTH_COOLDOWN_MS;
    static constexpr unsigned long kFailedToRebootMs = AURA_HEALTH_FAILED_TO_REBOOT_MS;
    static constexpr unsigned long kRebootCooldownMs = AURA_HEALTH_REBOOT_COOLDOWN_MS;
    static constexpr unsigned long kMinUptimeMs = AURA_HEALTH_MIN_UPTIME_MS;

    SubsystemState m_states[static_cast<size_t>(SubsystemId::COUNT)];
    uint32_t m_recoveryCount{0};
    unsigned long m_lastRebootMs{0};
    unsigned long m_bootTime{0};
    bool m_initialized{false};
};

extern HealthManager healthManager;

#endif // AURA_HEALTH_MANAGER_H