#ifndef AURA_ESP_NOW_MANAGER_H
#define AURA_ESP_NOW_MANAGER_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include "config.h"
#include "logger.h"
#include "event_bus.h"

enum class EspNowNodeType : uint8_t {
    AURA_MASTER = 0,
    AURA_SATELLITE,
    SENSOR_NODE,
    REMOTE_MIC,
    REMOTE_SPEAKER,
    UNKNOWN
};

enum class EspNowNodeState : uint8_t {
    DISCOVERED,
    PAIRING,
    PAIRED,
    DISCONNECTED
};

struct EspNowNode {
    uint8_t mac[6];
    EspNowNodeType type;
    EspNowNodeState state;
    char name[24];
    uint8_t rssi;
    unsigned long lastSeen;
    bool encrypted;

    EspNowNode() noexcept : type(EspNowNodeType::UNKNOWN), state(EspNowNodeState::DISCOVERED), rssi(0), lastSeen(0), encrypted(false) {
        memset(mac, 0, sizeof(mac));
        memset(name, 0, sizeof(name));
    }
};

struct EspNowMessage {
    uint8_t type;
    uint8_t srcMac[6];
    uint8_t dstMac[6];
    uint16_t seq;
    uint8_t payload[224];
    uint16_t payloadLen;
    unsigned long timestamp;

    EspNowMessage() noexcept : type(0), seq(0), payloadLen(0), timestamp(0) {
        memset(srcMac, 0, sizeof(srcMac));
        memset(dstMac, 0, sizeof(dstMac));
        memset(payload, 0, sizeof(payload));
    }
};

enum class EspNowMessageType : uint8_t {
    HEARTBEAT = 0,
    DISCOVERY,
    PAIR_REQUEST,
    PAIR_ACCEPT,
    PAIR_REJECT,
    TEXT,
    COMMAND,
    OTA_REQUEST,
    OTA_CHUNK,
    OTA_COMPLETE,
    AUDIO_STREAM
};

class EspNowManager {
public:
    EspNowManager() noexcept;
    ~EspNowManager() noexcept;

    EspNowManager(const EspNowManager&) = delete;
    EspNowManager& operator=(const EspNowManager&) = delete;
    EspNowManager(EspNowManager&&) = delete;
    EspNowManager& operator=(EspNowManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;
    void shutdown() noexcept;

    [[nodiscard]] bool sendMessage(const EspNowMessage& msg, const uint8_t* dstMac = nullptr) noexcept;
    [[nodiscard]] bool broadcastMessage(const EspNowMessage& msg) noexcept;

    [[nodiscard]] bool pairNode(const uint8_t* mac, EspNowNodeType type) noexcept;
    [[nodiscard]] bool pairNodeLocked(const uint8_t* mac, EspNowNodeType type) noexcept;
    void unpairNode(const uint8_t* mac) noexcept;
    [[nodiscard]] bool isPaired(const uint8_t* mac) noexcept;

    [[nodiscard]] const std::vector<EspNowNode>& getNodes() noexcept;
    [[nodiscard]] size_t nodeCount() noexcept;

    [[nodiscard]] bool startDiscovery() noexcept;
    void stopDiscovery() noexcept;
    [[nodiscard]] bool isDiscovering() const noexcept;

    void setEncryptionKey(const uint8_t* key) noexcept;

    [[nodiscard]] bool requestOTA(const uint8_t* nodeMac) noexcept;
    [[nodiscard]] bool sendOTAChunk(const uint8_t* nodeMac, const uint8_t* data, size_t len, uint16_t chunkIndex) noexcept;
    [[nodiscard]] bool sendOTAComplete(const uint8_t* nodeMac) noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] String getNodesJson() noexcept;

    static EspNowManager* instance() noexcept { return s_instance; }

    void handleReceivedMessage(const uint8_t* srcMac, const uint8_t* data, int len);

private:
    void handleHeartbeat(const uint8_t* srcMac, const EspNowMessage& msg);
    void handleDiscovery(const uint8_t* srcMac, const EspNowMessage& msg);
    void handlePairRequest(const uint8_t* srcMac, const EspNowMessage& msg);
    void handlePairAccept(const uint8_t* srcMac, const EspNowMessage& msg);
    void handleTextMessage(const uint8_t* srcMac, const EspNowMessage& msg);
    void handleOTAChunk(const uint8_t* srcMac, const EspNowMessage& msg);
    void sendHeartbeat() noexcept;
    void sendDiscoveryBeacon() noexcept;
    void checkTimeouts() noexcept;
    EspNowNode* findNode(const uint8_t* mac) noexcept;
    const EspNowNode* findNode(const uint8_t* mac) const noexcept;
    bool addOrUpdateNode(const uint8_t* mac, EspNowNodeType type, const char* name = nullptr) noexcept;
    void removeNode(const uint8_t* mac) noexcept;
    void generateEncryptionKey(uint8_t* key) noexcept;

    void lock() noexcept { portENTER_CRITICAL(&m_mutex); }
    void unlock() noexcept { portEXIT_CRITICAL(&m_mutex); }

    bool m_initialized;
    bool m_discovering;
    EspNowNodeType m_selfType;
    uint8_t m_encryptionKey[ESPNOW_ENCRYPT_KEY_SIZE];
    portMUX_TYPE m_mutex;

    std::vector<EspNowNode> m_nodes;
    uint16_t m_seqCounter;
    unsigned long m_lastHeartbeat;
    unsigned long m_discoveryStartTime;

    static EspNowManager* s_instance;

    static constexpr const char* kLogCategory = "EspNowManager";
    static constexpr unsigned long kDiscoveryDurationMs = 30000UL;
    static constexpr unsigned long kNodeTimeoutMs = 60000UL;
    static constexpr size_t kMaxPayloadSize = 224;
    static constexpr uint8_t kBroadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
};

extern EspNowManager espNowManager;

#endif // AURA_ESP_NOW_MANAGER_H
