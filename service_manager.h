#ifndef AURA_SERVICE_MANAGER_H
#define AURA_SERVICE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "service.h"

class ServiceManager {
public:
    ServiceManager() noexcept;
    ~ServiceManager() noexcept;

    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;

    bool Initialize() noexcept;
    void Update() noexcept;
    void Shutdown() noexcept;

    bool Register(Service* service) noexcept;
    bool Unregister(const String& name) noexcept;

    Service* GetService(const String& name) noexcept;
    template<typename T> T* GetServiceAs(const String& name) noexcept {
        return static_cast<T*>(GetService(name));
    }

    bool StartService(const String& name) noexcept;
    bool StopService(const String& name) noexcept;
    bool SuspendService(const String& name) noexcept;
    bool ResumeService(const String& name) noexcept;
    bool RestartService(const String& name) noexcept;

    bool StartAll(BootPriority minPriority = BootPriority::BACKGROUND) noexcept;
    bool StopAll() noexcept;
    bool SuspendAll() noexcept;
    bool ResumeAll() noexcept;

    void ReportHealth() noexcept;
    bool RecoverFailingServices() noexcept;

    std::vector<Service*> GetServicesByState(ServiceState state) const noexcept;
    std::vector<Service*> GetServicesByPriority(BootPriority priority) const noexcept;
    size_t GetServiceCount() const noexcept;
    bool IsInitialized() const noexcept;

    // Sort by boot priority for correct init order
    void SortByPriority() noexcept;

private:
    static constexpr const char* kLogCategory = "ServiceManager";
    static constexpr size_t kMaxServices = 64;
    static constexpr unsigned long kHealthIntervalMs = 10000UL;

    int FindService(const String& name) const noexcept;

    bool m_initialized;
    std::vector<Service*> m_services;
    unsigned long m_lastHealthCheck;
};

extern ServiceManager serviceManager;

#endif