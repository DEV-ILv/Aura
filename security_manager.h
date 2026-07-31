#ifndef AURA_SECURITY_MANAGER_H
#define AURA_SECURITY_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "service.h"
#include "event_bus.h"

enum class Permission : uint8_t {
    NONE        = 0,
    READ        = 1,
    WRITE       = 2,
    EXECUTE     = 3,
    ADMIN       = 4,
    SYSTEM      = 5
};

enum class AuditEventType : uint8_t {
    LOGIN,
    LOGOUT,
    COMMAND_EXECUTED,
    SETTINGS_CHANGED,
    OTA_STARTED,
    OTA_COMPLETED,
    FACTORY_RESET,
    MEMORY_ACCESS,
    MEMORY_MODIFY,
    PERMISSION_CHANGE,
    UNAUTHORIZED_ACCESS,
    ENCRYPTION_OPERATION,
    SECURITY_VIOLATION
};

struct AuditEntry {
    unsigned long timestamp;
    AuditEventType type;
    String source;
    String description;
    bool success;

    AuditEntry() noexcept : timestamp(0), type(AuditEventType::LOGIN), success(true) {}
};

class SecurityManager : public Service {
public:
    SecurityManager() noexcept;
    ~SecurityManager() noexcept;

    bool Initialize() noexcept override;
    void Update() noexcept override;

    // Authentication
    bool Authenticate(const String& token) noexcept;
    void Deauthenticate() noexcept;
    bool IsAuthenticated() const noexcept;
    Permission GetCurrentPermission() const noexcept;

    // Permission checks
    bool HasPermission(Permission required) const noexcept;
    bool CheckAccess(const String& resource, Permission required) noexcept;

    // Audit
    void LogAudit(AuditEventType type, const String& source,
                   const String& description, bool success) noexcept;
    std::vector<AuditEntry> GetAuditLog(size_t maxResults = 50) const noexcept;

    // Session
    String CreateSession(const String& source, Permission level) noexcept;
    bool ValidateSession(const String& sessionId) noexcept;
    void RevokeSession(const String& sessionId) noexcept;

    // Encryption helpers (delegates to VaultManager)
    String Encrypt(const String& plaintext) noexcept;
    String Decrypt(const String& ciphertext) noexcept;

    // Security status
    String GetSecurityReport() noexcept;
    bool IsSecure() const noexcept;
    uint8_t GetSecurityScore() const noexcept; // 0-100

    void HandleEvent(const String& eventType, const String& eventData) noexcept override;

    static constexpr const char* kStaticName = "SecurityManager";

private:
    struct Session {
        String id;
        String source;
        Permission permissionLevel;
        unsigned long createdAt;
        unsigned long lastUsed;
        bool valid;

        Session() noexcept : permissionLevel(Permission::NONE), createdAt(0), lastUsed(0), valid(false) {}
    };

    String GenerateSessionId() const noexcept;
    int FindSession(const String& id) const noexcept;
    void PruneSessions() noexcept;
    void PruneAuditLog() noexcept;

    static constexpr const char* kLogCategory = "Security";
    static constexpr size_t kMaxSessions = 16;
    static constexpr size_t kMaxAuditEntries = 200;
    static constexpr unsigned long kSessionTimeoutMs = 3600000UL; // 1 hour
    static constexpr unsigned long kCleanupIntervalMs = 60000UL;  // 1 minute

    std::vector<Session> m_sessions;
    std::vector<AuditEntry> m_auditLog;
    unsigned long m_lastCleanup;
    bool m_authenticated;
    Permission m_currentPermission;
    bool m_initialized;
};

extern SecurityManager securityManager;

#endif