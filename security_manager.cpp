#include "security_manager.h"
#include "settings_manager.h"
#include "vault_manager.h"

SecurityManager securityManager;

SecurityManager::SecurityManager() noexcept
    : Service(kStaticName, BootPriority::NORMAL)
    , m_lastCleanup(0)
    , m_authenticated(false)
    , m_currentPermission(Permission::NONE)
    , m_initialized(false) {
}

SecurityManager::~SecurityManager() noexcept = default;

bool SecurityManager::Initialize() noexcept {
    if (GetState() != ServiceState::UNINITIALIZED) return true;
    SetState(ServiceState::INITIALIZING);

    // No implicit trust: the device starts unauthenticated. The web portal
    // must successfully validate credentials and hand the session token to
    // Authenticate() before any privileged operation is permitted.
    m_currentPermission = Permission::NONE;
    m_authenticated = false;
    m_expectedToken = "";

    SetState(ServiceState::INITIALIZED);
    m_initialized = true;
    LOG_INFO(kLogCategory, "SecurityManager initialized (unauthenticated)");
    return true;
}

void SecurityManager::Update() noexcept {
    unsigned long now = millis();
    if (now - m_lastCleanup >= kCleanupIntervalMs) {
        m_lastCleanup = now;
        PruneSessions();
    }
}

bool SecurityManager::Authenticate(const String& token) noexcept {
    if (token.isEmpty()) {
        m_authenticated = false;
        m_currentPermission = Permission::NONE;
        LogAudit(AuditEventType::LOGIN, "security",
                 "Authentication failed: empty token", false);
        return false;
    }
    m_expectedToken = token;
    m_authenticated = true;
    m_currentPermission = Permission::ADMIN;
    LogAudit(AuditEventType::LOGIN, "security", "User authenticated", true);
    return true;
}

bool SecurityManager::CheckToken(const String& token) const noexcept {
    if (token.isEmpty() || m_expectedToken.isEmpty()) return false;
    if (token.length() != m_expectedToken.length()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < token.length(); ++i) {
        diff |= static_cast<uint8_t>(token[i]) ^ static_cast<uint8_t>(m_expectedToken[i]);
    }
    return diff == 0;
}

void SecurityManager::Deauthenticate() noexcept {
    m_authenticated = false;
    m_currentPermission = Permission::NONE;
    m_expectedToken = "";
    LogAudit(AuditEventType::LOGOUT, "security", "User deauthenticated", true);
}

bool SecurityManager::IsAuthenticated() const noexcept {
    return m_authenticated;
}

Permission SecurityManager::GetCurrentPermission() const noexcept {
    return m_currentPermission;
}

bool SecurityManager::HasPermission(Permission required) const noexcept {
    return static_cast<uint8_t>(m_currentPermission) >= static_cast<uint8_t>(required);
}

bool SecurityManager::CheckAccess(const String& resource, Permission required) noexcept {
    bool granted = HasPermission(required);
    if (!granted) {
        LogAudit(AuditEventType::UNAUTHORIZED_ACCESS, resource,
                 "Access denied", false);
    }
    return granted;
}

void SecurityManager::LogAudit(AuditEventType type, const String& source,
                                const String& description, bool success) noexcept {
    AuditEntry entry;
    entry.timestamp = millis();
    entry.type = type;
    entry.source = source;
    entry.description = description;
    entry.success = success;

    m_auditLog.push_back(entry);
    PruneAuditLog();

    LOG_DEBUG(kLogCategory, "Audit: %s from %s (%s)",
              description.c_str(), source.c_str(),
              success ? "OK" : "FAIL");
}

std::vector<AuditEntry> SecurityManager::GetAuditLog(size_t maxResults) const noexcept {
    if (maxResults >= m_auditLog.size()) return m_auditLog;
    std::vector<AuditEntry> recent;
    recent.reserve(maxResults);
    for (size_t i = m_auditLog.size() - maxResults; i < m_auditLog.size(); ++i) {
        recent.push_back(m_auditLog[i]);
    }
    return recent;
}

String SecurityManager::CreateSession(const String& source, Permission level) noexcept {
    if (m_sessions.size() >= kMaxSessions) return "";

    Session session;
    session.id = GenerateSessionId();
    session.source = source;
    session.permissionLevel = level;
    session.createdAt = millis();
    session.lastUsed = millis();
    session.valid = true;

    m_sessions.push_back(session);
    LogAudit(AuditEventType::LOGIN, source, "Session created: " + session.id, true);
    return session.id;
}

bool SecurityManager::ValidateSession(const String& sessionId) noexcept {
    int idx = FindSession(sessionId);
    if (idx < 0) return false;

    auto& session = m_sessions[idx];
    if (!session.valid) return false;

    // Check timeout
    if (millis() - session.lastUsed > kSessionTimeoutMs) {
        session.valid = false;
        return false;
    }

    session.lastUsed = millis();
    return true;
}

void SecurityManager::RevokeSession(const String& sessionId) noexcept {
    int idx = FindSession(sessionId);
    if (idx >= 0) {
        m_sessions[idx].valid = false;
    }
}

String SecurityManager::Encrypt(const String& plaintext) noexcept {
    if (vaultManager.isInitialized()) {
        return vaultManager.encrypt(plaintext);
    }
    LogAudit(AuditEventType::ENCRYPTION_OPERATION, "security",
             "Encrypt requested but VaultManager unavailable", false);
    return plaintext;
}

String SecurityManager::Decrypt(const String& ciphertext) noexcept {
    if (vaultManager.isInitialized()) {
        return vaultManager.decrypt(ciphertext);
    }
    return ciphertext;
}

String SecurityManager::GetSecurityReport() noexcept {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "Auth: %s | Permission: %d | Sessions: %zu | Audit: %zu entries",
        m_authenticated ? "Yes" : "No",
        static_cast<int>(m_currentPermission),
        m_sessions.size(), m_auditLog.size());
    return String(buf);
}

bool SecurityManager::IsSecure() const noexcept {
    return m_authenticated;
}

uint8_t SecurityManager::GetSecurityScore() const noexcept {
    uint8_t score = 100;
    if (!m_authenticated) score -= 50;
    return score;
}

void SecurityManager::HandleEvent(const String& eventType, const String& eventData) noexcept {
    Service::HandleEvent(eventType, eventData);

    if (eventType == "COMMAND_EXECUTED") {
        LogAudit(AuditEventType::COMMAND_EXECUTED, "palette", eventData, true);
    }
}

String SecurityManager::GenerateSessionId() const noexcept {
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    char buf[24];
    snprintf(buf, sizeof(buf), "%08x%08x%04x", r1, r2, (uint16_t)(millis() & 0xFFFF));
    return String(buf);
}

int SecurityManager::FindSession(const String& id) const noexcept {
    for (size_t i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

void SecurityManager::PruneSessions() noexcept {
    auto now = millis();
    m_sessions.erase(
        std::remove_if(m_sessions.begin(), m_sessions.end(),
            [now](const Session& s) {
                return !s.valid || (now - s.lastUsed > kSessionTimeoutMs);
            }),
        m_sessions.end());
}

void SecurityManager::PruneAuditLog() noexcept {
    while (m_auditLog.size() > kMaxAuditEntries) {
        m_auditLog.erase(m_auditLog.begin());
    }
}