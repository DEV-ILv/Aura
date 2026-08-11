#ifndef AURA_SERVICE_H
#define AURA_SERVICE_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"

enum class ServiceState : uint8_t {
    UNINITIALIZED,
    INITIALIZING,
    INITIALIZED,
    STARTING,
    RUNNING,
    SUSPENDED,
    STOPPING,
    STOPPED,
    ERROR,
    RECOVERING
};

enum class BootPriority : uint8_t {
    CRITICAL        = 0,   // Storage, EventBus
    PRIORITY_HIGH   = 1,   // Display, WiFi, Audio
    NORMAL          = 2,   // Memory, Conversation, Knowledge
    PRIORITY_LOW    = 3,   // Analytics, Search, Diagnostics
    BACKGROUND  = 4,   // Recommendations, Predictions
    MAINTENANCE = 5    // Cleanup, Compaction, Backups
};

enum class ServiceHealth : uint8_t {
    HEALTHY,
    DEGRADED,
    UNSTABLE,
    FAILED,
    NOT_RESPONDING
};

struct ServiceDependency {
    String serviceName;
    bool required;     // true = system cannot operate without this
    bool autoRecover;  // true = attempt restart on failure
};

struct ServiceCapability {
    String name;
    String version;
    bool available;
};

class Service {
public:
    Service(const char* name, BootPriority priority) noexcept;
    virtual ~Service() noexcept;

    // Lifecycle — all virtual with default no-op returns
    virtual bool Initialize() noexcept;
    virtual bool Start() noexcept;
    virtual bool Stop() noexcept;
    virtual bool Suspend() noexcept;
    virtual bool Resume() noexcept;
    virtual bool Restart() noexcept;

    // Introspection
    virtual ServiceHealth Health() const noexcept;
    virtual std::vector<ServiceDependency> Dependencies() const noexcept;
    virtual String Version() const noexcept;
    virtual std::vector<ServiceCapability> Capabilities() const noexcept;
    virtual size_t MemoryUsage() const noexcept;

    // Core
    virtual void Update() noexcept;
    virtual void HandleEvent(const String& eventType, const String& eventData) noexcept;

    // Recovery
    virtual bool Recover() noexcept;

    // Accessors
    const String& GetName() const noexcept;
    ServiceState GetState() const noexcept;
    BootPriority GetPriority() const noexcept;
    bool IsRunning() const noexcept;
    bool IsHealthy() const noexcept;
    unsigned long GetUptime() const noexcept;
    unsigned long GetLastErrorTime() const noexcept;
    String GetLastError() const noexcept;
    uint32_t GetRestartCount() const noexcept;

    // Event hooks (called by ServiceManager)
    void OnStateChange(ServiceState oldState, ServiceState newState) noexcept;

    friend class ServiceManager;

protected:
    static constexpr const char* kLogCategory = "Service";

    void SetState(ServiceState state) noexcept;
    void SetError(const String& error) noexcept;
    void ClearError() noexcept;

    const char* StateToString(ServiceState state) const noexcept;

    String m_name;
    BootPriority m_priority;
    ServiceState m_state;
    ServiceHealth m_health;
    String m_lastError;
    unsigned long m_lastErrorTime;
    unsigned long m_startTime;
    uint32_t m_restartCount;
    bool m_errorState;

    unsigned long m_lastHealthLogMs;
};

#endif