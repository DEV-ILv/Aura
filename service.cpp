#include "service.h"

Service::Service(const char* name, BootPriority priority) noexcept
    : m_name(name)
    , m_priority(priority)
    , m_state(ServiceState::UNINITIALIZED)
    , m_health(ServiceHealth::HEALTHY)
    , m_lastErrorTime(0)
    , m_startTime(0)
    , m_restartCount(0)
    , m_errorState(false)
    , m_lastHealthLogMs(0) {}

Service::~Service() noexcept = default;

bool Service::Initialize() noexcept { return true; }
bool Service::Start() noexcept { return true; }
bool Service::Stop() noexcept { return true; }
bool Service::Suspend() noexcept { return true; }
bool Service::Resume() noexcept { return true; }
bool Service::Restart() noexcept {
    Stop();
    return Start();
}

ServiceHealth Service::Health() const noexcept { return m_health; }
std::vector<ServiceDependency> Service::Dependencies() const noexcept { return {}; }
String Service::Version() const noexcept { return "1.0.0"; }
std::vector<ServiceCapability> Service::Capabilities() const noexcept { return {}; }
size_t Service::MemoryUsage() const noexcept { return 0; }

void Service::Update() noexcept {}
void Service::HandleEvent(const String& eventType, const String& eventData) noexcept {
    (void)eventType;
    (void)eventData;
}

bool Service::Recover() noexcept {
    LOG_WARNING(kLogCategory, "%s attempting automatic recovery", m_name.c_str());
    SetState(ServiceState::RECOVERING);
    bool ok = Restart();
    if (ok) {
        ClearError();
        m_health = ServiceHealth::HEALTHY;
        LOG_INFO(kLogCategory, "%s recovered successfully", m_name.c_str());
    } else {
        m_health = ServiceHealth::FAILED;
        LOG_ERROR(kLogCategory, "%s recovery failed", m_name.c_str());
    }
    return ok;
}

const String& Service::GetName() const noexcept { return m_name; }
ServiceState Service::GetState() const noexcept { return m_state; }
BootPriority Service::GetPriority() const noexcept { return m_priority; }

bool Service::IsRunning() const noexcept {
    return m_state == ServiceState::RUNNING;
}

bool Service::IsHealthy() const noexcept {
    return m_health == ServiceHealth::HEALTHY;
}

unsigned long Service::GetUptime() const noexcept {
    if (m_state == ServiceState::RUNNING && m_startTime > 0)
        return (millis() - m_startTime) / 1000;
    return 0;
}

unsigned long Service::GetLastErrorTime() const noexcept {
    return m_lastErrorTime;
}

String Service::GetLastError() const noexcept {
    return m_lastError;
}

uint32_t Service::GetRestartCount() const noexcept {
    return m_restartCount;
}

void Service::OnStateChange(ServiceState oldState, ServiceState newState) noexcept {
    LOG_DEBUG(kLogCategory, "%s: %s -> %s",
              m_name.c_str(), StateToString(oldState), StateToString(newState));
}

void Service::SetState(ServiceState state) noexcept {
    if (m_state == state) return;
    ServiceState old = m_state;
    m_state = state;

    if (state == ServiceState::RUNNING && m_startTime == 0)
        m_startTime = millis();

    OnStateChange(old, state);
}

void Service::SetError(const String& error) noexcept {
    m_lastError = error;
    m_lastErrorTime = millis();
    m_errorState = true;
    m_health = ServiceHealth::FAILED;
}

void Service::ClearError() noexcept {
    m_lastError = "";
    m_lastErrorTime = 0;
    m_errorState = false;
}

const char* Service::StateToString(ServiceState state) const noexcept {
    switch (state) {
        case ServiceState::UNINITIALIZED: return "UNINITIALIZED";
        case ServiceState::INITIALIZING:  return "INITIALIZING";
        case ServiceState::INITIALIZED:   return "INITIALIZED";
        case ServiceState::STARTING:      return "STARTING";
        case ServiceState::RUNNING:       return "RUNNING";
        case ServiceState::SUSPENDED:     return "SUSPENDED";
        case ServiceState::STOPPING:      return "STOPPING";
        case ServiceState::STOPPED:       return "STOPPED";
        case ServiceState::ERROR:         return "ERROR";
        case ServiceState::RECOVERING:    return "RECOVERING";
        default:                          return "UNKNOWN";
    }
}