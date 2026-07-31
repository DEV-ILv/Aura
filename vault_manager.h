#ifndef AURA_VAULT_MANAGER_H
#define AURA_VAULT_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

struct VaultEntry {
    String id;
    String key;                 // Human-readable identifier (e.g. "gemini_api_key")
    String encryptedValue;      // XOR-encrypted value
    String category;            // "api_key", "wifi", "token", "secret", "password"
    unsigned long createdAt;
    unsigned long updatedAt;
    
    VaultEntry() noexcept : createdAt(0), updatedAt(0) {}
};

class VaultManager {
public:
    VaultManager() noexcept;
    ~VaultManager() noexcept;
    
    VaultManager(const VaultManager&) = delete;
    VaultManager& operator=(const VaultManager&) = delete;
    VaultManager(VaultManager&&) = delete;
    VaultManager& operator=(VaultManager&&) = delete;
    
    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;
    
    // Core API
    [[nodiscard]] bool setSecret(const String& key, const String& value, const String& category = "secret") noexcept;
    [[nodiscard]] String getSecret(const String& key) const noexcept;
    [[nodiscard]] bool deleteSecret(const String& key) noexcept;
    [[nodiscard]] bool hasSecret(const String& key) const noexcept;
    
    // Convenience
    [[nodiscard]] bool setApiKey(const String& service, const String& key) noexcept;
    [[nodiscard]] String getApiKey(const String& service) const noexcept;
    [[nodiscard]] bool setWiFiCredential(const String& ssid, const String& password) noexcept;
    [[nodiscard]] bool getWiFiCredential(const String& ssid, String& password) const noexcept;
    
    // Encryption (AES-256-GCM with device-derived key)
    [[nodiscard]] String encrypt(const String& plaintext) const noexcept;
    [[nodiscard]] String decrypt(const String& ciphertext) const noexcept;
    
    // Query
    [[nodiscard]] std::vector<VaultEntry> getAllEntries() const noexcept;
    [[nodiscard]] std::vector<VaultEntry> getByCategory(const String& category) const noexcept;
    [[nodiscard]] String getVaultJson() const noexcept;
    
    // Backup
    [[nodiscard]] bool exportBackup(const String& path = VAULT_BACKUP_PATH) noexcept;
    [[nodiscard]] bool importBackup(const String& path = VAULT_BACKUP_PATH) noexcept;
    
    // Persistence
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;
    
    [[nodiscard]] bool isInitialized() const noexcept;
    
private:
    static constexpr const char* kLogCategory = "VaultMgr";
    static constexpr size_t kMaxEntries = VAULT_MAX_ENTRIES;
    
    String generateId() noexcept;
    void trimToMax() noexcept;
    String serializeEntry(const VaultEntry& e) const noexcept;
    VaultEntry deserializeEntry(const String& json) const noexcept;
    
    bool m_initialized;
    bool m_dirty;
    std::vector<VaultEntry> m_entries;
    unsigned long m_lastIdCounter;
};

extern VaultManager vaultManager;

#endif
