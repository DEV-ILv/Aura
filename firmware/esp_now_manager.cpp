#include "esp_now_manager.h"
#include "wifi_manager.h"
#include "error_manager.h"
#include <esp_random.h>

EspNowManager* EspNowManager::s_instance = nullptr;
EspNowManager espNowManager;

extern "C" void espNowSendCb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);
extern "C" void espNowRecvCb(const esp_now_recv_info_t* recvInfo, const uint8_t* data, int len);

namespace {
// ESP-NOW must bind its peers to whatever interface WifiManager has active.
// Normal operation is STA; setup mode runs the SoftAP. Using WIFI_IF_AP
// unconditionally (the old code) silently fails when the device is STA-only.
wifi_interface_t currentEspNowInterface() noexcept {
    if (wifiManager.isAccessPointMode()) {
        return WIFI_IF_AP;
    }
    return WIFI_IF_STA;
}
}

EspNowManager::EspNowManager() noexcept
    : m_initialized(false)
    , m_discovering(false)
    , m_selfType(EspNowNodeType::AURA_MASTER)
    , m_mutex()
    , m_seqCounter(0)
    , m_lastHeartbeat(0)
    , m_discoveryStartTime(0) {
    m_mutex = portMUX_INITIALIZER_UNLOCKED;
    memset(m_encryptionKey, 0, ESPNOW_ENCRYPT_KEY_SIZE);
    s_instance = this;
}

EspNowManager::~EspNowManager() noexcept {
    shutdown();
    if (s_instance == this) s_instance = nullptr;
}

bool EspNowManager::initialize() noexcept {
    if (m_initialized) return true;

    // ESP-NOW does NOT own the Wi-Fi radio. WifiManager is the single
    // authority for WiFi.mode()/WiFi.channel(); the radio is already in the
    // mode selected by WifiManager (STA, or STA for normal operation). ESP-NOW
    // simply runs on the current radio and follows the channel WifiManager has
    // established (see pairNode: peer channel 0 = "use current channel").
    // We no longer force WIFI_AP_STA or ESPNOW_CHANNEL here — that was the
    // conflicting-ownership defect that re-initialized the radio mid-boot.

    esp_err_t result = esp_now_init();
    if (result != ESP_OK) {
        Logger::error(kLogCategory, "ESP-NOW init failed: %d", static_cast<int>(result));
        errorManager.report(AuraErrorSeverity::WARNING, "EspNowManager", "ESPNOW_INIT_FAIL",
                            "ESP-NOW failed to initialize",
                            "Mesh discovery/pairing disabled for this boot");
        return false;
    }

    esp_now_register_send_cb(espNowSendCb);
    esp_now_register_recv_cb(espNowRecvCb);

    // Set the strong shared primary master key. ESP-NOW derives the per-peer
    // default LMK from this key, so all nodes running the same firmware can
    // exchange encrypted frames. A zeroed LMK tells the driver to derive it.
    esp_now_set_pmk(const_cast<uint8_t*>(kPrimaryMasterKey));

    // Pre-size the node table so push_back in the recv task never allocates
    // while the portMUX critical section is held.
    m_nodes.reserve(ESPNOW_MAX_NODES);

    m_initialized = true;
    Logger::info(kLogCategory, "ESP-NOW initialized (follows WifiManager channel %u)",
                 static_cast<unsigned>(wifiManager.getChannel()));
    return true;
}

void EspNowManager::update() noexcept {
    if (!m_initialized) return;

    unsigned long now = millis();

    sendHeartbeat();
    checkTimeouts();

    if (m_discovering && (now - m_discoveryStartTime >= kDiscoveryDurationMs)) {
        stopDiscovery();
    }
}

void EspNowManager::shutdown() noexcept {
    if (!m_initialized) return;

    // Collect the MACs of PAIRED nodes under the spinlock, then call
    // esp_now_del_peer after unlock: the driver call can block on the WiFi
    // task and must never run while the portMUX critical section is held.
    uint8_t pairedMacs[ESPNOW_MAX_NODES][6];
    size_t pairedCount = 0;
    lock();
    for (auto& node : m_nodes) {
        if (node.state == EspNowNodeState::PAIRED && pairedCount < ESPNOW_MAX_NODES) {
            memcpy(pairedMacs[pairedCount], node.mac, sizeof(node.mac));
            pairedCount++;
        }
    }
    m_nodes.clear();
    unlock();

    for (size_t i = 0; i < pairedCount; ++i) {
        esp_now_del_peer(pairedMacs[i]);
    }
    esp_now_deinit();
    m_initialized = false;
    Logger::info(kLogCategory, "ESP-NOW shutdown");
}

bool EspNowManager::sendMessage(const EspNowMessage& msg, const uint8_t* dstMac) noexcept {
    if (!m_initialized) return false;

    uint8_t buf[sizeof(EspNowMessage)];
    EspNowMessage out = msg;

    if (dstMac) {
        memcpy(out.dstMac, dstMac, 6);
    }

    esp_err_t result = esp_now_send(dstMac ? dstMac : kBroadcastMac, reinterpret_cast<const uint8_t*>(&out), sizeof(EspNowMessage));
    return result == ESP_OK;
}

bool EspNowManager::broadcastMessage(const EspNowMessage& msg) noexcept {
    return sendMessage(msg, kBroadcastMac);
}

bool EspNowManager::pairNode(const uint8_t* mac, EspNowNodeType type) noexcept {
    // NOTE: caller must hold m_mutex lock when calling this method.
    // The ESP-NOW driver registration (esp_now_add_peer) is performed by
    // pairNodeLocked() OUTSIDE the spinlock; this method only updates the
    // node bookkeeping so blocking driver calls never run under the lock.
    if (!m_initialized) return false;

    addOrUpdateNode(mac, type);
    auto* node = findNode(mac);
    if (node) {
        node->state = EspNowNodeState::PAIRED;
        node->lastSeen = millis();
        node->encrypted = true;
    }

    return true;
}

bool EspNowManager::pairNodeLocked(const uint8_t* mac, EspNowNodeType type) noexcept {
    if (!m_initialized) return false;

    // esp_now_add_peer can take FreeRTOS mutexes / block, so it must never run
    // under the spinlock (consistent with the deferred-action convention used
    // for the receive callback). Register with the driver first, then update
    // the node bookkeeping under the lock.
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, mac, 6);
    // Channel 0 tells the ESP-NOW driver to use the channel WifiManager has
    // established for the active interface (STA/AP), so ESP-NOW never fights
    // the router channel and never forces ESPNOW_CHANNEL=1.
    peerInfo.channel = 0;
    peerInfo.encrypt = true;   // encrypted link; LMK derived from PMK
    peerInfo.ifidx = currentEspNowInterface();

    esp_err_t result = esp_now_add_peer(&peerInfo);
    if (result != ESP_OK && result != ESP_ERR_ESPNOW_EXIST) {
        Logger::warning(kLogCategory, "Failed to add peer: %d", static_cast<int>(result));
        return false;
    }

    lock();
    const bool ok = pairNode(mac, type);
    unlock();
    return ok;
}

void EspNowManager::unpairNode(const uint8_t* mac) noexcept {
    // esp_now_del_peer can take FreeRTOS mutexes / block; run it outside the
    // spinlock. Only the node bookkeeping runs under the lock.
    esp_now_del_peer(mac);
    lock();
    removeNode(mac);
    unlock();
}

bool EspNowManager::isPaired(const uint8_t* mac) noexcept {
    lock();
    const auto* node = findNode(mac);
    bool result = node && node->state == EspNowNodeState::PAIRED;
    unlock();
    return result;
}

const std::vector<EspNowNode>& EspNowManager::getNodes() noexcept {
    return m_nodes;
}

size_t EspNowManager::nodeCount() noexcept {
    lock();
    size_t count = m_nodes.size();
    unlock();
    return count;
}

bool EspNowManager::startDiscovery() noexcept {
    if (!m_initialized || m_discovering) return false;
    m_discovering = true;
    m_discoveryStartTime = millis();
    sendDiscoveryBeacon();
    Logger::info(kLogCategory, "Discovery started");
    return true;
}

void EspNowManager::stopDiscovery() noexcept {
    if (!m_discovering) return;
    m_discovering = false;
    lock();
    size_t count = m_nodes.size();
    unlock();
    Logger::info(kLogCategory, "Discovery stopped (%d nodes found)", static_cast<int>(count));
}

bool EspNowManager::isDiscovering() const noexcept {
    return m_discovering;
}

void EspNowManager::setEncryptionKey(const uint8_t* key) noexcept {
    if (key) {
        memcpy(m_encryptionKey, key, ESPNOW_ENCRYPT_KEY_SIZE);
    }
}

bool EspNowManager::requestOTA(const uint8_t* nodeMac) noexcept {
    EspNowMessage msg;
    msg.type = static_cast<uint8_t>(EspNowMessageType::OTA_REQUEST);
    msg.seq = m_seqCounter++;
    return sendMessage(msg, nodeMac);
}

bool EspNowManager::sendOTAChunk(const uint8_t* nodeMac, const uint8_t* data, size_t len, uint16_t chunkIndex) noexcept {
    EspNowMessage msg;
    msg.type = static_cast<uint8_t>(EspNowMessageType::OTA_CHUNK);
    msg.seq = m_seqCounter++;
    msg.payloadLen = (len < kMaxPayloadSize) ? static_cast<uint16_t>(len) : kMaxPayloadSize;
    memcpy(msg.payload, data, msg.payloadLen);
    msg.payload[0] = static_cast<uint8_t>(chunkIndex & 0xFF);
    msg.payload[1] = static_cast<uint8_t>((chunkIndex >> 8) & 0xFF);
    return sendMessage(msg, nodeMac);
}

bool EspNowManager::sendOTAComplete(const uint8_t* nodeMac) noexcept {
    EspNowMessage msg;
    msg.type = static_cast<uint8_t>(EspNowMessageType::OTA_COMPLETE);
    msg.seq = m_seqCounter++;
    return sendMessage(msg, nodeMac);
}

bool EspNowManager::isInitialized() const noexcept {
    return m_initialized;
}

String EspNowManager::getNodesJson() noexcept {
    // Self-locking: safe to call from the web API handlers without external
    // locking. The node table is snapshotted into a fixed-size stack array
    // under the spinlock and the JSON String is built after it is released,
    // so no allocation ever runs while the critical section is held.
    EspNowNode snapshot[ESPNOW_MAX_NODES];
    size_t count = 0;
    lock();
    for (const auto& n : m_nodes) {
        if (count < ESPNOW_MAX_NODES) {
            snapshot[count++] = n;
        }
    }
    unlock();

    String json = "{\"count\":" + String(count) + ",\"nodes\":[";
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) json += ",";
        const auto& n = snapshot[i];
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            n.mac[0], n.mac[1], n.mac[2], n.mac[3], n.mac[4], n.mac[5]);
        json += "{\"mac\":\"" + String(macStr) + "\"";
        json += ",\"type\":" + String(static_cast<int>(n.type));
        json += ",\"state\":" + String(static_cast<int>(n.state));
        json += ",\"name\":\"" + String(n.name) + "\"";
        json += ",\"rssi\":" + String(n.rssi);
        json += ",\"lastSeen\":" + String(n.lastSeen);
        json += "}";
    }
    json += "]}";
    return json;
}

extern "C" void espNowSendCb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    (void)tx_info;
    if (status != ESP_NOW_SEND_SUCCESS) {
        Logger::debug("EspNowManager", "ESP-NOW send failed");
    }
}

extern "C" void espNowRecvCb(const esp_now_recv_info_t* recvInfo, const uint8_t* data, int len) {
    EspNowManager* mgr = EspNowManager::instance();
    if (!mgr || !data || len < static_cast<int>(sizeof(EspNowMessage))) return;
    mgr->handleReceivedMessage(recvInfo->src_addr, data, len);
}

void EspNowManager::handleReceivedMessage(const uint8_t* srcMac, const uint8_t* data, int len) {
    if (len < static_cast<int>(sizeof(EspNowMessage))) return;

    EspNowPendingAction action;
    bool rejectedUnpaired = false;
    uint8_t rejectedType = 0;

    lock();
    EspNowMessage msg;
    memcpy(&msg, data, sizeof(EspNowMessage));

    // Privileged message types (text, commands, OTA) are only accepted from
    // paired nodes. Discovery/pairing handshakes remain open so new devices
    // can join, but they cannot inject commands or firmware updates.
    bool isPaired = false;
    const auto* node = findNode(srcMac);
    if (node && node->state == EspNowNodeState::PAIRED) isPaired = true;

    switch (static_cast<EspNowMessageType>(msg.type)) {
        case EspNowMessageType::HEARTBEAT:
            handleHeartbeat(srcMac, msg, action);
            break;
        case EspNowMessageType::DISCOVERY:
            handleDiscovery(srcMac, msg, action);
            break;
        case EspNowMessageType::PAIR_REQUEST:
            handlePairRequest(srcMac, msg, action);
            break;
        case EspNowMessageType::PAIR_ACCEPT:
            handlePairAccept(srcMac, msg, action);
            break;
        case EspNowMessageType::TEXT:
        case EspNowMessageType::COMMAND:
        case EspNowMessageType::OTA_REQUEST:
        case EspNowMessageType::OTA_CHUNK:
        case EspNowMessageType::OTA_COMPLETE:
        case EspNowMessageType::AUDIO_STREAM:
            if (isPaired) {
                handleTextMessage(srcMac, msg, action);
            } else {
                rejectedUnpaired = true;
                rejectedType = static_cast<uint8_t>(msg.type);
            }
            break;
        default:
            break;
    }
    unlock();

    // Apply deferred work outside the critical section: esp_now_add_peer,
    // esp_now_send, String/JSON construction and event publishing all allocate
    // or take FreeRTOS mutexes, so they must never run under the spinlock.
    if (action.addPeer) {
        esp_now_add_peer(&action.peerInfo);
    }
    if (action.sendOutbound) {
        sendMessage(action.outbound, action.outboundToSrc ? srcMac : nullptr);
    }
    if (action.hasEvent && eventBus.isInitialized()) {
        if (action.eventWithNodes) {
            eventBus.publish(action.eventType, kLogCategory, getNodesJson());
        } else if (action.hasTextPayload) {
            String payload(action.textPayload, action.textPayloadLen);
            eventBus.publish(action.eventType, kLogCategory, payload);
        } else {
            eventBus.publish(action.eventType, kLogCategory);
        }
    }

    // Log outside the spinlock: Logger takes a FreeRTOS mutex, which must not
    // be taken while the portMUX critical section is held.
    if (rejectedUnpaired) {
        Logger::warning(kLogCategory, "Rejected message type %d from unpaired node", rejectedType);
    }
}

void EspNowManager::handleHeartbeat(const uint8_t* srcMac, const EspNowMessage& msg, EspNowPendingAction& action) {
    auto* node = findNode(srcMac);
    if (node) {
        node->lastSeen = millis();
        node->rssi = WiFi.RSSI();
        if (node->state == EspNowNodeState::DISCOVERED && msg.payloadLen > 0) {
            EspNowNodeType type = static_cast<EspNowNodeType>(msg.payload[0]);
            if (type <= EspNowNodeType::REMOTE_SPEAKER) {
                node->type = type;
            }
        }
    }
    action.hasEvent = true;
    action.eventType = EventType::ESPNOW_NODE_HEARTBEAT;
}

void EspNowManager::handleDiscovery(const uint8_t* srcMac, const EspNowMessage& msg, EspNowPendingAction& action) {
    EspNowNodeType type = EspNowNodeType::UNKNOWN;
    if (msg.payloadLen > 0 && msg.payload[0] <= static_cast<uint8_t>(EspNowNodeType::REMOTE_SPEAKER)) {
        type = static_cast<EspNowNodeType>(msg.payload[0]);
    }
    const char* name = (msg.payloadLen > 1) ? reinterpret_cast<const char*>(msg.payload + 1) : nullptr;

    addOrUpdateNode(srcMac, type, name);
    auto* node = findNode(srcMac);
    if (node) {
        node->lastSeen = millis();
        node->rssi = WiFi.RSSI();
    }

    action.hasEvent = true;
    action.eventType = EventType::ESPNOW_NODE_DISCOVERED;
    action.eventWithNodes = true;

    // Auto-respond with our discovery beacon (sent after the lock is released)
    buildDiscoveryBeacon(action.outbound);
    action.sendOutbound = true;
    action.outboundToSrc = false;
}

void EspNowManager::handlePairRequest(const uint8_t* srcMac, const EspNowMessage& msg, EspNowPendingAction& action) {
    EspNowNodeType reqType = EspNowNodeType::UNKNOWN;
    if (msg.payloadLen > 0 && msg.payload[0] <= static_cast<uint8_t>(EspNowNodeType::REMOTE_SPEAKER)) {
        reqType = static_cast<EspNowNodeType>(msg.payload[0]);
    }

    if (m_nodes.size() >= ESPNOW_MAX_NODES) {
        action.outbound.type = static_cast<uint8_t>(EspNowMessageType::PAIR_REJECT);
        action.sendOutbound = true;
        action.outboundToSrc = true;
        return;
    }

    addOrUpdateNode(srcMac, reqType);
    auto* node = findNode(srcMac);
    if (node) {
        node->state = EspNowNodeState::PAIRED;
        node->lastSeen = millis();
        node->encrypted = true;
    }

    buildPeerInfo(&action.peerInfo, srcMac);
    action.addPeer = true;

    action.outbound.type = static_cast<uint8_t>(EspNowMessageType::PAIR_ACCEPT);
    action.outbound.payload[0] = static_cast<uint8_t>(m_selfType);
    action.sendOutbound = true;
    action.outboundToSrc = true;

    action.hasEvent = true;
    action.eventType = EventType::ESPNOW_NODE_PAIRED;
    action.eventWithNodes = true;
}

void EspNowManager::handlePairAccept(const uint8_t* srcMac, const EspNowMessage& msg, EspNowPendingAction& action) {
    EspNowNodeType acceptedType = EspNowNodeType::UNKNOWN;
    if (msg.payloadLen > 0 && msg.payload[0] <= static_cast<uint8_t>(EspNowNodeType::REMOTE_SPEAKER)) {
        acceptedType = static_cast<EspNowNodeType>(msg.payload[0]);
    }

    auto* node = findNode(srcMac);
    if (node) {
        node->state = EspNowNodeState::PAIRED;
        node->type = acceptedType;
        node->lastSeen = millis();
        node->encrypted = true;
    }

    buildPeerInfo(&action.peerInfo, srcMac);
    action.addPeer = true;

    action.hasEvent = true;
    action.eventType = EventType::ESPNOW_NODE_PAIRED;
    action.eventWithNodes = true;
}

void EspNowManager::handleTextMessage(const uint8_t* srcMac, const EspNowMessage& msg, EspNowPendingAction& action) {
    (void)srcMac;
    action.hasEvent = true;
    action.eventType = EventType::ESPNOW_MESSAGE_RECEIVED;
    if (msg.payloadLen > 0) {
        const uint16_t n = (msg.payloadLen <= kMaxPayloadSize) ? msg.payloadLen : kMaxPayloadSize;
        memcpy(action.textPayload, msg.payload, n);
        action.textPayloadLen = n;
        action.hasTextPayload = true;
    }
}

void EspNowManager::sendHeartbeat() noexcept {
    unsigned long now = millis();
    if (now - m_lastHeartbeat < ESPNOW_HEARTBEAT_INTERVAL_MS) return;
    m_lastHeartbeat = now;

    EspNowMessage msg;
    msg.type = static_cast<uint8_t>(EspNowMessageType::HEARTBEAT);
    msg.seq = m_seqCounter++;
    msg.payload[0] = static_cast<uint8_t>(m_selfType);
    msg.payloadLen = 1;
    sendMessage(msg, kBroadcastMac);
}

void EspNowManager::buildDiscoveryBeacon(EspNowMessage& msg) noexcept {
    memset(&msg, 0, sizeof(msg));
    msg.type = static_cast<uint8_t>(EspNowMessageType::DISCOVERY);
    msg.seq = m_seqCounter++;
    msg.payload[0] = static_cast<uint8_t>(m_selfType);
    const char* selfName = "AURA";
    size_t nameLen = strlen(selfName) + 1;
    if (nameLen > kMaxPayloadSize - 1) nameLen = kMaxPayloadSize - 1;
    memcpy(msg.payload + 1, selfName, nameLen);
    msg.payloadLen = static_cast<uint16_t>(1 + nameLen);
}

void EspNowManager::sendDiscoveryBeacon() noexcept {
    EspNowMessage msg;
    buildDiscoveryBeacon(msg);
    sendMessage(msg, kBroadcastMac);
}

void EspNowManager::buildPeerInfo(esp_now_peer_info_t* peerInfo, const uint8_t* mac) noexcept {
    memset(peerInfo, 0, sizeof(*peerInfo));
    memcpy(peerInfo->peer_addr, mac, 6);
    // Channel 0 tells the ESP-NOW driver to use the channel WifiManager has
    // established for the active interface (STA/AP), so ESP-NOW never fights
    // the router channel and never forces ESPNOW_CHANNEL=1.
    peerInfo->channel = 0;
    peerInfo->encrypt = true;   // encrypted link; LMK derived from PMK
    peerInfo->ifidx = currentEspNowInterface();
}

void EspNowManager::checkTimeouts() noexcept {
    unsigned long now = millis();
    uint8_t timedOutMacs[ESPNOW_MAX_NODES][6];
    uint8_t timedOutCount = 0;
    lock();
    for (auto& node : m_nodes) {
        if (node.state == EspNowNodeState::PAIRED && (now - node.lastSeen >= kNodeTimeoutMs)) {
            node.state = EspNowNodeState::DISCONNECTED;
            if (timedOutCount < ESPNOW_MAX_NODES) {
                memcpy(timedOutMacs[timedOutCount++], node.mac, 6);
            }
        }
    }
    unlock();

    // Publish outside the spinlock: publish() allocates an event copy.
    for (uint8_t i = 0; i < timedOutCount; ++i) {
        (void)timedOutMacs[i];
        if (eventBus.isInitialized()) {
            eventBus.publish(EventType::ESPNOW_NODE_DISCONNECTED, kLogCategory);
        }
    }
}

EspNowNode* EspNowManager::findNode(const uint8_t* mac) noexcept {
    for (auto& n : m_nodes) {
        if (memcmp(n.mac, mac, 6) == 0) return &n;
    }
    return nullptr;
}

const EspNowNode* EspNowManager::findNode(const uint8_t* mac) const noexcept {
    for (const auto& n : m_nodes) {
        if (memcmp(n.mac, mac, 6) == 0) return &n;
    }
    return nullptr;
}

bool EspNowManager::addOrUpdateNode(const uint8_t* mac, EspNowNodeType type, const char* name) noexcept {
    auto* node = findNode(mac);
    if (node) {
        node->lastSeen = millis();
        node->rssi = WiFi.RSSI();
        if (type != EspNowNodeType::UNKNOWN) node->type = type;
        if (name) {
            strncpy(node->name, name, sizeof(node->name) - 1);
            node->name[sizeof(node->name) - 1] = '\0';
        }
        return false;
    }
    if (m_nodes.size() >= ESPNOW_MAX_NODES) return false;
    EspNowNode n;
    memcpy(n.mac, mac, 6);
    n.type = (type != EspNowNodeType::UNKNOWN) ? type : EspNowNodeType::UNKNOWN;
    n.state = EspNowNodeState::DISCOVERED;
    n.lastSeen = millis();
    n.rssi = WiFi.RSSI();
    if (name) {
        strncpy(n.name, name, sizeof(n.name) - 1);
        n.name[sizeof(n.name) - 1] = '\0';
    }
    m_nodes.push_back(n);
    return true;
}

void EspNowManager::removeNode(const uint8_t* mac) noexcept {
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (memcmp(it->mac, mac, 6) == 0) {
            m_nodes.erase(it);
            break;
        }
    }
}

void EspNowManager::generateEncryptionKey(uint8_t* key) noexcept {
    for (size_t i = 0; i < ESPNOW_ENCRYPT_KEY_SIZE; ++i) {
        key[i] = static_cast<uint8_t>(esp_random() & 0xFF);
    }
}
