#ifndef AURA_DEVICE_MESH_H
#define AURA_DEVICE_MESH_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "settings_manager.h"
#include "reminder_manager.h"
#include "memory_manager.h"

struct MeshDevice {
    uint8_t mac[6];
    String deviceId;
    String friendlyName;
    String firmwareVersion;
    uint32_t capabilities;
    bool paired;
    bool connected;
    unsigned long lastHeartbeat;
    int8_t rssi;

    bool operator==(const MeshDevice& other) const {
        return deviceId == other.deviceId;
    }
};

enum class DeviceCapability : uint32_t {
    NONE            = 0,
    AUDIO_INPUT     = 1 << 0,
    AUDIO_OUTPUT    = 1 << 1,
    HAS_DISPLAY     = 1 << 2,
    LED_RING        = 1 << 3,
    TOUCH_SENSOR    = 1 << 4,
    STORAGE         = 1 << 5,
    REMINDERS       = 1 << 6,
    MEMORY_SHARING  = 1 << 7,
    CONVERSATION    = 1 << 8,
    HANDOVER        = 1 << 9,
    WEBSERVER       = 1 << 10
};

struct SharedReminder {
    String reminderId;
    String sourceDeviceId;
    String title;
    String message;
    unsigned long triggerTime;
};

struct SharedMemory {
    String memoryId;
    String sourceDeviceId;
    String key;
    String value;
    String category;
};

class DeviceMesh {
public:
    DeviceMesh() noexcept;
    ~DeviceMesh() noexcept;

    DeviceMesh(const DeviceMesh&) = delete;
    DeviceMesh& operator=(const DeviceMesh&) = delete;
    DeviceMesh(DeviceMesh&&) = delete;
    DeviceMesh& operator=(DeviceMesh&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    [[nodiscard]] const std::vector<MeshDevice>& getPairedDevices() const noexcept;
    [[nodiscard]] const std::vector<MeshDevice>& getDiscoveredDevices() const noexcept;
    [[nodiscard]] size_t deviceCount() const noexcept;

    [[nodiscard]] bool pairDevice(const String& deviceId) noexcept;
    [[nodiscard]] bool unpairDevice(const String& deviceId) noexcept;
    [[nodiscard]] bool sendReminder(const String& targetDeviceId, const SharedReminder& reminder) noexcept;
    [[nodiscard]] bool sendMemory(const String& targetDeviceId, const SharedMemory& memory) noexcept;
    [[nodiscard]] bool requestHandover(const String& targetDeviceId) noexcept;

    [[nodiscard]] String getLocalDeviceId() const noexcept;
    void setFriendlyName(const String& name) noexcept;
    [[nodiscard]] String getFriendlyName() const noexcept;
    [[nodiscard]] uint32_t getLocalCapabilities() const noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool isMultiDeviceEnabled() const noexcept;
    void setMultiDeviceEnabled(bool enabled) noexcept;

private:
    static constexpr const char* kLogCategory = "DeviceMesh";
    static constexpr unsigned long kHeartbeatIntervalMs = 30000;
    static constexpr unsigned long kDeviceTimeoutMs = 180000;

    void sendHeartbeat() noexcept;
    void checkTimeouts() noexcept;
    void discoverDevices() noexcept;
    String macToString(const uint8_t mac[6]) const noexcept;
    void generateDeviceId() noexcept;

    bool m_initialized;
    bool m_enabled;
    String m_localDeviceId;
    String m_friendlyName;
    uint32_t m_localCapabilities;
    std::vector<MeshDevice> m_pairedDevices;
    std::vector<MeshDevice> m_discoveredDevices;
    unsigned long m_lastHeartbeatTime;
    unsigned long m_lastDiscoveryTime;
};

extern DeviceMesh deviceMesh;

#endif // AURA_DEVICE_MESH_H
