#include "service_manager.h"
#include <algorithm>

ServiceManager serviceManager;

ServiceManager::ServiceManager() noexcept
    : m_initialized(false)
    , m_lastHealthCheck(0)
    , m_healthCheckRuns(0)
    , m_healthLogsEmitted(0)
    , m_lastHealthBasisMs(0)
    , m_lastDiagnosticLogMs(0) {
}

ServiceManager::~ServiceManager() noexcept {
    if (m_initialized) Shutdown();
}

bool ServiceManager::Initialize() noexcept {
    if (m_initialized) return true;
    m_initialized = true;
    LOG_INFO(kLogCategory, "ServiceManager initialized");
    return true;
}

void ServiceManager::Update() noexcept {
    if (!m_initialized) return;

    for (auto* service : m_services) {
        if (service->GetState() == ServiceState::RUNNING ||
            service->GetState() == ServiceState::SUSPENDED) {
            service->Update();
        }
    }

    unsigned long now = millis();
    if (now - m_lastHealthCheck >= kHealthIntervalMs) {
        m_lastHealthBasisMs = now - m_lastHealthCheck;
        m_lastHealthCheck = now;
        ReportHealth();
        RecoverFailingServices();
    }

    if (now - m_lastDiagnosticLogMs >= kHealthDiagnosticIntervalMs) {
        LOG_INFO(kLogCategory,
                 "health diag: runs=%lu logs=%lu lastRunIntervalMs=%lu",
                 m_healthCheckRuns, m_healthLogsEmitted, m_lastHealthBasisMs);
        m_healthCheckRuns = 0;
        m_healthLogsEmitted = 0;
        m_lastDiagnosticLogMs = now;
    }
}

void ServiceManager::Shutdown() noexcept {
    if (!m_initialized) return;
    StopAll();
    m_services.clear();
    m_initialized = false;
    LOG_INFO(kLogCategory, "ServiceManager shut down");
}

bool ServiceManager::Register(Service* service) noexcept {
    if (!service || m_services.size() >= kMaxServices) return false;
    if (FindService(service->GetName()) >= 0) return false;

    m_services.push_back(service);
    SortByPriority();
    LOG_DEBUG(kLogCategory, "Registered service: %s (priority=%d)",
              service->GetName().c_str(), static_cast<int>(service->GetPriority()));
    return true;
}

bool ServiceManager::Unregister(const String& name) noexcept {
    int idx = FindService(name);
    if (idx < 0) return false;
    m_services[idx]->Stop();
    m_services.erase(m_services.begin() + idx);
    return true;
}

Service* ServiceManager::GetService(const String& name) noexcept {
    int idx = FindService(name);
    return (idx >= 0) ? m_services[idx] : nullptr;
}

bool ServiceManager::StartService(const String& name) noexcept {
    auto* svc = GetService(name);
    if (!svc) return false;

    if (svc->GetState() == ServiceState::UNINITIALIZED) {
        svc->SetState(ServiceState::INITIALIZING);
        if (!svc->Initialize()) {
            svc->SetState(ServiceState::ERROR);
            svc->SetError("Initialization failed");
            return false;
        }
        svc->SetState(ServiceState::INITIALIZED);
    }

    svc->SetState(ServiceState::STARTING);
    if (!svc->Start()) {
        svc->SetState(ServiceState::ERROR);
        svc->SetError("Start failed");
        return false;
    }
    svc->SetState(ServiceState::RUNNING);
    return true;
}

bool ServiceManager::StopService(const String& name) noexcept {
    auto* svc = GetService(name);
    if (!svc) return false;
    svc->SetState(ServiceState::STOPPING);
    bool ok = svc->Stop();
    svc->SetState(ServiceState::STOPPED);
    return ok;
}

bool ServiceManager::SuspendService(const String& name) noexcept {
    auto* svc = GetService(name);
    if (!svc || svc->GetState() != ServiceState::RUNNING) return false;
    return svc->Suspend();
}

bool ServiceManager::ResumeService(const String& name) noexcept {
    auto* svc = GetService(name);
    if (!svc || svc->GetState() != ServiceState::SUSPENDED) return false;
    return svc->Resume();
}

bool ServiceManager::RestartService(const String& name) noexcept {
    auto* svc = GetService(name);
    if (!svc) return false;
    return svc->Restart();
}

bool ServiceManager::StartAll(BootPriority minPriority) noexcept {
    for (auto* svc : m_services) {
        if (static_cast<uint8_t>(svc->GetPriority()) <= static_cast<uint8_t>(minPriority)) {
            StartService(svc->GetName());
        }
    }
    return true;
}

bool ServiceManager::StopAll() noexcept {
    for (auto* svc : m_services) {
        StopService(svc->GetName());
    }
    return true;
}

bool ServiceManager::SuspendAll() noexcept {
    for (auto* svc : m_services) {
        if (svc->GetState() == ServiceState::RUNNING) svc->Suspend();
    }
    return true;
}

bool ServiceManager::ResumeAll() noexcept {
    for (auto* svc : m_services) {
        if (svc->GetState() == ServiceState::SUSPENDED) svc->Resume();
    }
    return true;
}

void ServiceManager::ReportHealth() noexcept {
    m_healthCheckRuns++;

    const unsigned long now = millis();
    for (auto* svc : m_services) {
        const ServiceHealth health = svc->Health();
        if (health == ServiceHealth::HEALTHY) {
            continue;
        }
        if ((now - svc->m_lastHealthLogMs) >= kHealthIntervalMs) {
            m_healthLogsEmitted++;
            LOG_WARNING(kLogCategory, "%s health: %d",
                        svc->GetName().c_str(), static_cast<int>(health));
            svc->m_lastHealthLogMs = now;
        }
    }
}

bool ServiceManager::RecoverFailingServices() noexcept {
    bool anyRecovered = false;
    for (auto* svc : m_services) {
        if (svc->GetState() == ServiceState::ERROR ||
            svc->GetState() == ServiceState::RECOVERING) {
            LOG_WARNING(kLogCategory, "%s in error state, attempting recovery",
                        svc->GetName().c_str());
            if (svc->Recover()) anyRecovered = true;
            else svc->SetState(ServiceState::ERROR);
        }
    }
    return anyRecovered;
}

std::vector<Service*> ServiceManager::GetServicesByState(ServiceState state) const noexcept {
    std::vector<Service*> result;
    for (auto* svc : m_services) {
        if (svc->GetState() == state) result.push_back(svc);
    }
    return result;
}

std::vector<Service*> ServiceManager::GetServicesByPriority(BootPriority priority) const noexcept {
    std::vector<Service*> result;
    for (auto* svc : m_services) {
        if (svc->GetPriority() == priority) result.push_back(svc);
    }
    return result;
}

size_t ServiceManager::GetServiceCount() const noexcept { return m_services.size(); }
bool ServiceManager::IsInitialized() const noexcept { return m_initialized; }

void ServiceManager::SortByPriority() noexcept {
    std::sort(m_services.begin(), m_services.end(),
        [](const Service* a, const Service* b) {
            return static_cast<uint8_t>(a->GetPriority()) < static_cast<uint8_t>(b->GetPriority());
        });
}

int ServiceManager::FindService(const String& name) const noexcept {
    for (size_t i = 0; i < m_services.size(); ++i) {
        if (m_services[i]->GetName() == name) return static_cast<int>(i);
    }
    return -1;
}