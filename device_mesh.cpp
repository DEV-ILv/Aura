#include "device_mesh.h"
#include "esp_now_manager.h"

DeviceMesh deviceMesh;

DeviceMesh::DeviceMesh() noexcept
    : m_initialized(false)
    , m_enabled(false)
    , m_localCapabilities(0)
    , m_lastHeartbeatTime(0)
    , m_lastDiscoveryTime(0) {}

DeviceMesh::~DeviceMesh() noexcept = default;

bool DeviceMesh::initialize() noexcept {
    if (m_initialized) return true;
    generateDeviceId();
    m_friendlyName = settingsManager.isInitialized()
        ? String(settingsManager.getDeviceName()) : "AURA-1";
    m_localCapabilities = static_cast<uint32_t>(DeviceCapability::AUDIO_INPUT)
        | static_cast<uint32_t>(DeviceCapability::AUDIO_OUTPUT)
        | static_cast<uint32_t>(DeviceCapability::HAS_DISPLAY)
        | static_cast<uint32_t>(DeviceCapability::LED_RING)
        | static_cast<uint32_t>(DeviceCapability::TOUCH_SENSOR)
        | static_cast<uint32_t>(DeviceCapability::STORAGE)
        | static_cast<uint32_t>(DeviceCapability::REMINDERS)
        | static_cast<uint32_t>(DeviceCapability::MEMORY_SHARING)
        | static_cast<uint32_t>(DeviceCapability::CONVERSATION)
        | static_cast<uint32_t>(DeviceCapability::HANDOVER)
        | static_cast<uint32_t>(DeviceCapability::WEBSERVER);
    m_initialized = true;
    m_lastHeartbeatTime = millis();
    m_lastDiscoveryTime = millis();
    LOG_INFO(kLogCategory, "DeviceMesh initialized [%s]", m_localDeviceId.c_str());
    return true;
}

void DeviceMesh::update() noexcept {
    if (!m_initialized || !m_enabled) return;
    unsigned long now = millis();

    if (now - m_lastHeartbeatTime >= kHeartbeatIntervalMs) {
        m_lastHeartbeatTime = now;
        sendHeartbeat();
    }

    if (now - m_lastDiscoveryTime >= kHeartbeatIntervalMs * 4) {
        m_lastDiscoveryTime = now;
        discoverDevices();
    }

    checkTimeouts();
}

const std::vector<MeshDevice>& DeviceMesh::getPairedDevices() const noexcept {
    return m_pairedDevices;
}

const std::vector<MeshDevice>& DeviceMesh::getDiscoveredDevices() const noexcept {
    return m_discoveredDevices;
}

size_t DeviceMesh::deviceCount() const noexcept {
    return m_pairedDevices.size() + m_discoveredDevices.size();
}

bool DeviceMesh::pairDevice(const String& deviceId) noexcept {
    for (auto it = m_discoveredDevices.begin(); it != m_discoveredDevices.end(); ++it) {
        if (it->deviceId == deviceId) {
            it->paired = true;
            m_pairedDevices.push_back(*it);
            m_discoveredDevices.erase(it);
            if (eventBus.isInitialized()) {
                eventBus.publish(EventType::DEVICE_PAIRED, "DeviceMesh",
                                 "{\"deviceId\":\"" + deviceId + "\"}");
            }
            LOG_INFO(kLogCategory, "Paired device: %s", deviceId.c_str());
            return true;
        }
    }
    return false;
}

bool DeviceMesh::unpairDevice(const String& deviceId) noexcept {
    for (auto it = m_pairedDevices.begin(); it != m_pairedDevices.end(); ++it) {
        if (it->deviceId == deviceId) {
            m_pairedDevices.erase(it);
            if (eventBus.isInitialized()) {
                eventBus.publish(EventType::DEVICE_UNPAIRED, "DeviceMesh",
                                 "{\"deviceId\":\"" + deviceId + "\"}");
            }
            LOG_INFO(kLogCategory, "Unpaired device: %s", deviceId.c_str());
            return true;
        }
    }
    return false;
}

bool DeviceMesh::sendReminder(const String& targetDeviceId, const SharedReminder& reminder) noexcept {
    if (!m_enabled) return false;
    if (eventBus.isInitialized()) {
        String data = "{\"target\":\"" + targetDeviceId + "\",\"title\":\"" + reminder.title + "\"}";
        eventBus.publish(EventType::DEVICE_SHARED_REMINDER, "DeviceMesh", data);
    }
    return true;
}

bool DeviceMesh::sendMemory(const String& targetDeviceId, const SharedMemory& memory) noexcept {
    if (!m_enabled) return false;
    if (eventBus.isInitialized()) {
        String data = "{\"target\":\"" + targetDeviceId + "\",\"key\":\"" + memory.key + "\"}";
        eventBus.publish(EventType::DEVICE_SHARED_MEMORY, "DeviceMesh", data);
    }
    return true;
}

bool DeviceMesh::requestHandover(const String& targetDeviceId) noexcept {
    if (!m_enabled) return false;
    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::DEVICE_HANDOVER_STARTED, "DeviceMesh",
                         "{\"target\":\"" + targetDeviceId + "\"}");
    }
    LOG_INFO(kLogCategory, "Handover requested to: %s", targetDeviceId.c_str());
    return true;
}

String DeviceMesh::getLocalDeviceId() const noexcept {
    return m_localDeviceId;
}

void DeviceMesh::setFriendlyName(const String& name) noexcept {
    m_friendlyName = name;
}

String DeviceMesh::getFriendlyName() const noexcept {
    return m_friendlyName;
}

uint32_t DeviceMesh::getLocalCapabilities() const noexcept {
    return m_localCapabilities;
}

bool DeviceMesh::isInitialized() const noexcept {
    return m_initialized;
}

bool DeviceMesh::isMultiDeviceEnabled() const noexcept {
    return m_enabled;
}

void DeviceMesh::setMultiDeviceEnabled(bool enabled) noexcept {
    m_enabled = enabled;
    if (!enabled) {
        m_pairedDevices.clear();
        m_discoveredDevices.clear();
    }
}

void DeviceMesh::sendHeartbeat() noexcept {
    if (eventBus.isInitialized()) {
        String data = "{\"deviceId\":\"" + m_localDeviceId
            + "\",\"name\":\"" + m_friendlyName
            + "\",\"caps\":" + String(m_localCapabilities) + "}";
        eventBus.publish(EventType::DEVICE_HEARTBEAT, "DeviceMesh", data);
    }
}

void DeviceMesh::checkTimeouts() noexcept {
    unsigned long now = millis();
    for (auto it = m_pairedDevices.begin(); it != m_pairedDevices.end(); ) {
        if (now - it->lastHeartbeat > kDeviceTimeoutMs) {
            it->connected = false;
            if (eventBus.isInitialized()) {
                eventBus.publish(EventType::ESPNOW_NODE_DISCONNECTED, "DeviceMesh",
                                 "{\"deviceId\":\"" + it->deviceId + "\"}");
            }
            ++it;
        } else {
            ++it;
        }
    }
}

void DeviceMesh::discoverDevices() noexcept {
    if (espNowManager.isInitialized()) {
        LOG_DEBUG(kLogCategory, "Discovery scan initiated via ESP-NOW");
    }
}

void DeviceMesh::generateDeviceId() noexcept {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    m_localDeviceId = "AURA-" + macToString(mac);
}

String DeviceMesh::macToString(const uint8_t mac[6]) const noexcept {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}
