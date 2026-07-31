#ifndef AURA_COMPANION_MANAGER_H
#define AURA_COMPANION_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

struct CompanionDevice {
    String id;
    String name;
    String deviceType;       // "windows", "android", "web"
    String ipAddress;
    uint16_t port;
    String status;           // "paired", "connected", "disconnected"
    unsigned long pairedAt;
    unsigned long lastSeen;
    String publicKey;        // For secure communication
    uint8_t retryCount;

    CompanionDevice() noexcept : port(0), pairedAt(0), lastSeen(0), retryCount(0) {}
};

struct CompanionMessage {
    String id;
    String deviceId;
    String type;             // "sync", "command", "notification", "data"
    String payload;
    unsigned long timestamp;
    bool delivered;
    uint8_t retryCount;

    CompanionMessage() noexcept : timestamp(0), delivered(false), retryCount(0) {}
};

class CompanionManager {
public:
    CompanionManager() noexcept;
    ~CompanionManager() noexcept;

    CompanionManager(const CompanionManager&) = delete;
    CompanionManager& operator=(const CompanionManager&) = delete;
    CompanionManager(CompanionManager&&) = delete;
    CompanionManager& operator=(CompanionManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    // Device management
    [[nodiscard]] bool pairDevice(const String& name, const String& deviceType, const String& ip, uint16_t port) noexcept;
    [[nodiscard]] bool unpairDevice(const String& deviceId) noexcept;
    [[nodiscard]] bool setDeviceStatus(const String& deviceId, const String& status) noexcept;
    [[nodiscard]] CompanionDevice getDevice(const String& deviceId) const noexcept;
    [[nodiscard]] std::vector<CompanionDevice> getAllDevices() const noexcept;
    [[nodiscard]] std::vector<CompanionDevice> getConnectedDevices() const noexcept;

    // Messaging
    [[nodiscard]] String sendMessage(const String& deviceId, const String& type, const String& payload) noexcept;
    [[nodiscard]] bool markDelivered(const String& messageId) noexcept;
    [[nodiscard]] std::vector<CompanionMessage> getPendingMessages(const String& deviceId) const noexcept;
    [[nodiscard]] std::vector<CompanionMessage> getMessageHistory(const String& deviceId, size_t maxMessages = 20) const noexcept;

    // Sync
    [[nodiscard]] bool requestSync(const String& deviceId) noexcept;
    [[nodiscard]] bool handleSyncResponse(const String& deviceId, const String& data) noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] size_t pairedDeviceCount() const noexcept;
    [[nodiscard]] size_t pendingMessageCount() const noexcept;
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;

private:
    static constexpr const char* kLogCategory = "CompanionManager";
    static constexpr const char* kDevicesPath = "/companion_devices.json";
    static constexpr const char* kMessagesPath = "/companion_messages.json";
    static constexpr size_t kMaxDevices = 8;
    static constexpr size_t kMaxMessages = 256;
    static constexpr size_t kMaxMessageRetries = 3;
    static constexpr unsigned long kRetryIntervalMs = 30000;

    void retryFailedMessages() noexcept;
    String generateId() noexcept;
    size_t findDevice(const String& id) const noexcept;
    size_t findMessage(const String& id) const noexcept;
    bool m_initialized;
    bool m_dirty;
    std::vector<CompanionDevice> m_devices;
    std::vector<CompanionMessage> m_messages;
    unsigned long m_lastRetryTime;
    unsigned long m_lastIdCounter;
};

extern CompanionManager companionManager;

#endif // AURA_COMPANION_MANAGER_H
