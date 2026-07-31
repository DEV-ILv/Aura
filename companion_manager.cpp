#include "companion_manager.h"
#include "json_helpers.h"

CompanionManager companionManager;

CompanionManager::CompanionManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastRetryTime(0), m_lastIdCounter(0) {
    m_devices.reserve(kMaxDevices);
    m_messages.reserve(kMaxMessages);
}

CompanionManager::~CompanionManager() noexcept {
    if (m_dirty) save();
}

bool CompanionManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u devices, %u messages)", m_devices.size(), m_messages.size());
    return true;
}

void CompanionManager::update() noexcept {
    if (!m_initialized) return;
    unsigned long now = millis();

    static unsigned long lastSave = 0;
    if (m_dirty && (now - lastSave > 5000)) {
        lastSave = now;
        if (save()) m_dirty = false;
    }

    if (now - m_lastRetryTime >= kRetryIntervalMs) {
        m_lastRetryTime = now;
        retryFailedMessages();
    }
}

bool CompanionManager::pairDevice(const String& name, const String& deviceType, const String& ip, uint16_t port) noexcept {
    if (!m_initialized || m_devices.size() >= kMaxDevices || name.isEmpty()) return false;
    CompanionDevice d;
    d.id = generateId();
    d.name = name;
    d.deviceType = deviceType;
    d.ipAddress = ip;
    d.port = port;
    d.status = "paired";
    d.pairedAt = millis();
    d.lastSeen = millis();
    m_devices.push_back(d);
    m_dirty = true;
    Logger::info(kLogCategory, "Device '%s' paired (%s)", name.c_str(), deviceType.c_str());
    return true;
}

bool CompanionManager::unpairDevice(const String& deviceId) noexcept {
    size_t idx = findDevice(deviceId);
    if (idx == SIZE_MAX) return false;
    m_devices.erase(m_devices.begin() + static_cast<ptrdiff_t>(idx));
    m_dirty = true;
    Logger::info(kLogCategory, "Device '%s' unpaired", deviceId.c_str());
    return true;
}

bool CompanionManager::setDeviceStatus(const String& deviceId, const String& status) noexcept {
    size_t idx = findDevice(deviceId);
    if (idx == SIZE_MAX) return false;
    m_devices[idx].status = status;
    m_devices[idx].lastSeen = millis();
    m_dirty = true;
    return true;
}

CompanionDevice CompanionManager::getDevice(const String& deviceId) const noexcept {
    size_t idx = findDevice(deviceId);
    return (idx != SIZE_MAX) ? m_devices[idx] : CompanionDevice();
}

std::vector<CompanionDevice> CompanionManager::getAllDevices() const noexcept {
    return m_devices;
}

std::vector<CompanionDevice> CompanionManager::getConnectedDevices() const noexcept {
    std::vector<CompanionDevice> result;
    for (const auto& d : m_devices) {
        if (d.status == "connected") result.push_back(d);
    }
    return result;
}

String CompanionManager::sendMessage(const String& deviceId, const String& type, const String& payload) noexcept {
    if (!m_initialized || findDevice(deviceId) == SIZE_MAX) return "";
    CompanionMessage msg;
    msg.id = generateId();
    msg.deviceId = deviceId;
    msg.type = type;
    msg.payload = payload;
    msg.timestamp = millis();
    msg.delivered = false;
    msg.retryCount = 0;
    m_messages.push_back(msg);
    if (m_messages.size() > kMaxMessages) {
        m_messages.erase(m_messages.begin());
    }
    m_dirty = true;
    Logger::info(kLogCategory, "Message '%s' sent to '%s' (type: %s)", msg.id.c_str(), deviceId.c_str(), type.c_str());
    return msg.id;
}

bool CompanionManager::markDelivered(const String& messageId) noexcept {
    size_t idx = findMessage(messageId);
    if (idx == SIZE_MAX) return false;
    m_messages[idx].delivered = true;
    m_dirty = true;
    return true;
}

std::vector<CompanionMessage> CompanionManager::getPendingMessages(const String& deviceId) const noexcept {
    std::vector<CompanionMessage> result;
    for (const auto& m : m_messages) {
        if (m.deviceId == deviceId && !m.delivered) result.push_back(m);
    }
    return result;
}

std::vector<CompanionMessage> CompanionManager::getMessageHistory(const String& deviceId, size_t maxMessages) const noexcept {
    std::vector<CompanionMessage> result;
    for (const auto& m : m_messages) {
        if (m.deviceId == deviceId) {
            result.push_back(m);
            if (result.size() >= maxMessages) break;
        }
    }
    return result;
}

bool CompanionManager::requestSync(const String& deviceId) noexcept {
    if (findDevice(deviceId) == SIZE_MAX) return false;
    String msgId = sendMessage(deviceId, "sync", "{\"action\":\"sync_request\"}");
    return !msgId.isEmpty();
}

bool CompanionManager::handleSyncResponse(const String& deviceId, const String& data) noexcept {
    if (findDevice(deviceId) == SIZE_MAX) return false;
    Logger::info(kLogCategory, "Sync response received from '%s'", deviceId.c_str());
    size_t idx = findDevice(deviceId);
    if (idx != SIZE_MAX) {
        m_devices[idx].lastSeen = millis();
        m_dirty = true;
    }
    return true;
}

bool CompanionManager::isInitialized() const noexcept { return m_initialized; }
size_t CompanionManager::pairedDeviceCount() const noexcept { return m_devices.size(); }

size_t CompanionManager::pendingMessageCount() const noexcept {
    size_t count = 0;
    for (const auto& m : m_messages) { if (!m.delivered) count++; }
    return count;
}

void CompanionManager::retryFailedMessages() noexcept {
    unsigned long now = millis();
    for (auto& m : m_messages) {
        if (!m.delivered && m.retryCount < kMaxMessageRetries) {
            m.retryCount++;
            Logger::info(kLogCategory, "Retrying message '%s' (attempt %u)", m.id.c_str(), m.retryCount);
        }
    }
    m_dirty = true;
}

bool CompanionManager::save() noexcept {
    String json; json.reserve(2048);
    json += "{\"items\":[";
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (i > 0) json += ",";
        const auto& d = m_devices[i];
        json += "{\"id\":\"" + escapeJson(d.id) + "\",\"name\":\"" + escapeJson(d.name) + "\",";
        json += "\"type\":\"" + escapeJson(d.deviceType) + "\",\"ip\":\"" + escapeJson(d.ipAddress) + "\",";
        json += "\"port\":" + String(d.port) + ",\"status\":\"" + escapeJson(d.status) + "\",";
        json += "\"paired\":" + String(d.pairedAt) + ",\"seen\":" + String(d.lastSeen) + ",";
        json += "\"key\":\"" + escapeJson(d.publicKey) + "\",\"retry\":" + String(d.retryCount) + "}";
    }
    json += "]}";
    if (storageManager.writeFile(kDevicesPath, json, StorageType::SPIFFS) != StorageStatus::SUCCESS) return false;

    json = "{\"items\":[";
    for (size_t i = 0; i < m_messages.size(); ++i) {
        if (i > 0) json += ",";
        const auto& m = m_messages[i];
        json += "{\"id\":\"" + escapeJson(m.id) + "\",\"dev\":\"" + escapeJson(m.deviceId) + "\",";
        json += "\"type\":\"" + escapeJson(m.type) + "\",\"payload\":\"" + escapeJson(m.payload) + "\",";
        json += "\"ts\":" + String(m.timestamp) + ",\"del\":" + String(m.delivered ? "true" : "false") + ",";
        json += "\"retry\":" + String(m.retryCount) + "}";
    }
    json += "]}";
    if (storageManager.writeFile(kMessagesPath, json, StorageType::SPIFFS) != StorageStatus::SUCCESS) return false;

    m_dirty = false;
    return true;
}

bool CompanionManager::load() noexcept {
    String content;

    auto parseDevice = [&](const String& obj, CompanionDevice& d) {
        auto ext = [&](const char* k) -> String {
            String q = String("\"") + k + "\":\"";
            int st = obj.indexOf(q);
            if (st < 0) {
                q = String("\"") + k + "\":";
                st = obj.indexOf(q);
                if (st < 0) return "";
                st += q.length();
                int en = st;
                while (en < (int)obj.length() && obj[en] != ',' && obj[en] != '}') en++;
                return obj.substring(st, en);
            }
            st += q.length();
            int en = obj.indexOf('"', st);
            return (en < 0) ? "" : obj.substring(st, en);
        };
        d.id = ext("id"); d.name = ext("name"); d.deviceType = ext("type"); d.ipAddress = ext("ip");
        d.port = static_cast<uint16_t>(ext("port").toInt());
        d.status = ext("status");
        d.pairedAt = ext("paired").toInt();
        d.lastSeen = ext("seen").toInt();
        d.publicKey = ext("key");
        d.retryCount = ext("retry").toInt();
    };

    if (storageManager.fileExists(kDevicesPath, StorageType::SPIFFS)) {
        content = "";
        if (storageManager.readFile(kDevicesPath, content, StorageType::SPIFFS) == StorageStatus::SUCCESS && !content.isEmpty()) {
            m_devices.clear();
            int pos = 0;
            while (true) {
                int s = content.indexOf('{', pos); if (s < 0) break;
                int e = content.indexOf('}', s); if (e < 0) break;
                String obj = content.substring(s, e + 1);
                CompanionDevice d; parseDevice(obj, d);
                if (!d.id.isEmpty()) m_devices.push_back(d);
                pos = e + 1;
            }
        }
    }

    auto parseMessage = [&](const String& obj, CompanionMessage& m) {
        auto ext = [&](const char* k) -> String {
            String q = String("\"") + k + "\":\"";
            int st = obj.indexOf(q);
            if (st < 0) {
                q = String("\"") + k + "\":";
                st = obj.indexOf(q);
                if (st < 0) return "";
                st += q.length();
                int en = st;
                while (en < (int)obj.length() && obj[en] != ',' && obj[en] != '}') en++;
                return obj.substring(st, en);
            }
            st += q.length();
            int en = obj.indexOf('"', st);
            return (en < 0) ? "" : obj.substring(st, en);
        };
        m.id = ext("id"); m.deviceId = ext("dev"); m.type = ext("type"); m.payload = ext("payload");
        m.timestamp = ext("ts").toInt();
        m.delivered = ext("del") == "true";
        m.retryCount = ext("retry").toInt();
    };

    if (storageManager.fileExists(kMessagesPath, StorageType::SPIFFS)) {
        content = "";
        if (storageManager.readFile(kMessagesPath, content, StorageType::SPIFFS) == StorageStatus::SUCCESS && !content.isEmpty()) {
            m_messages.clear();
            int pos = 0;
            while (true) {
                int s = content.indexOf('{', pos); if (s < 0) break;
                int e = content.indexOf('}', s); if (e < 0) break;
                String obj = content.substring(s, e + 1);
                CompanionMessage msg; parseMessage(obj, msg);
                if (!msg.id.isEmpty()) m_messages.push_back(msg);
                pos = e + 1;
            }
        }
    }

    return true;
}

String CompanionManager::generateId() noexcept {
    unsigned long now = millis(); m_lastIdCounter++;
    uint32_t mix = static_cast<uint32_t>(now) ^ (m_lastIdCounter << 16) ^ (ESP.getEfuseMac() & 0xFFFFFFFF);
    String id; id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) { id += hex[val & 0x0F]; val = (val >> 2) ^ (val << 3) ^ (m_lastIdCounter + i); }
    return id;
}

size_t CompanionManager::findDevice(const String& id) const noexcept {
    for (size_t i = 0; i < m_devices.size(); ++i) { if (m_devices[i].id == id) return i; }
    return SIZE_MAX;
}

size_t CompanionManager::findMessage(const String& id) const noexcept {
    for (size_t i = 0; i < m_messages.size(); ++i) { if (m_messages[i].id == id) return i; }
    return SIZE_MAX;
}
