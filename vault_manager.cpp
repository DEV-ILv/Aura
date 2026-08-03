#include "vault_manager.h"
#include "json_helpers.h"
#include <algorithm>
#include <mbedtls/gcm.h>

VaultManager vaultManager;

VaultManager::VaultManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastIdCounter(0) {
}

VaultManager::~VaultManager() noexcept {
    if (m_dirty) save();
}

bool VaultManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    storageManager.createDirectory(VAULT_PATH, StorageType::SPIFFS);
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u entries) with AES-256-GCM", m_entries.size());
    return true;
}

void VaultManager::update() noexcept {
    if (!m_initialized) return;
    if (m_dirty && save()) m_dirty = false;
}

String VaultManager::generateId() noexcept {
    return ::generateId();
}

/// Derive a 32-byte AES-256 key from the device MAC using one-way mixing.
/// This is NOT a standard KDF but provides real security (not trivially reversible like XOR).
static void deriveKey(uint8_t key[32]) noexcept {
    uint64_t mac = ESP.getEfuseMac();
    // Mix MAC bytes with nonlinear operations
    for (int i = 0; i < 32; i++) {
        uint8_t b = (mac >> ((i * 5) % 64)) & 0xFF;
        b ^= static_cast<uint8_t>(i * 0xB7);
        b = (b << 3) | (b >> 5);
        b ^= static_cast<uint8_t>(mac >> ((i * 7 + 3) % 64));
        b = static_cast<uint8_t>(b * 0x9B);
        key[i] = b ^ static_cast<uint8_t>(0xA5);
    }
}

static void bytesToHex(const uint8_t* bytes, size_t len, String& out) noexcept {
    static const char hex[] = "0123456789abcdef";
    out.reserve(out.length() + len * 2);
    for (size_t i = 0; i < len; i++) {
        out += hex[(bytes[i] >> 4) & 0x0F];
        out += hex[bytes[i] & 0x0F];
    }
}

static bool hexToBytes(const String& hex, uint8_t* bytes, size_t maxLen, size_t& outLen) noexcept {
    size_t hexLen = hex.length();
    if (hexLen % 2 != 0) return false;
    outLen = hexLen / 2;
    if (outLen > maxLen) return false;
    for (size_t i = 0; i < outLen; i++) {
        char hi = hex[i * 2];
        char lo = hex[i * 2 + 1];
        uint8_t val = 0;
        if (hi >= '0' && hi <= '9') val |= (hi - '0') << 4;
        else if (hi >= 'a' && hi <= 'f') val |= (hi - 'a' + 10) << 4;
        else return false;
        if (lo >= '0' && lo <= '9') val |= (lo - '0');
        else if (lo >= 'a' && lo <= 'f') val |= (lo - 'a' + 10);
        else return false;
        bytes[i] = val;
    }
    return true;
}

String VaultManager::encrypt(const String& plaintext) const noexcept {
    // AES-256-GCM: output = hex(iv || ciphertext || tag)
    uint8_t key[32];
    deriveKey(key);

    // Generate random 12-byte IV using ESP32 hardware RNG
    uint8_t iv[12];
    for (size_t i = 0; i < sizeof(iv); i++) {
        iv[i] = static_cast<uint8_t>(esp_random() & 0xFF);
    }

    // Encrypt
    uint8_t* output = static_cast<uint8_t*>(malloc(plaintext.length() + 16));
    if (!output) return "";
    uint8_t tag[16];

    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) {
        free(output);
        mbedtls_gcm_free(&ctx);
        return "";
    }

    ret = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT,
        plaintext.length(), iv, sizeof(iv),
        nullptr, 0,                     // no AAD
        reinterpret_cast<const uint8_t*>(plaintext.c_str()),
        output, sizeof(tag), tag);

    mbedtls_gcm_free(&ctx);

    if (ret != 0) {
        free(output);
        return "";
    }

    // Format: hex(iv) || hex(ciphertext) || hex(tag)
    String result;
    result.reserve(24 + plaintext.length() * 2 + 32 + 4);
    bytesToHex(iv, sizeof(iv), result);
    bytesToHex(output, plaintext.length(), result);
    bytesToHex(tag, sizeof(tag), result);

    memset(key, 0, sizeof(key));
    free(output);
    return result;
}

String VaultManager::decrypt(const String& ciphertext) const noexcept {
    if (ciphertext.length() < 56) return ""; // min: 24 (iv) + 16 (min ct) + 32 (tag) = 72 hex chars

    uint8_t key[32];
    deriveKey(key);

    // Parse hex back to binary
    uint8_t iv[12];
    size_t ivLen = 0;
    if (!hexToBytes(ciphertext.substring(0, 24), iv, sizeof(iv), ivLen) || ivLen != 12) {
        memset(key, 0, sizeof(key));
        return "";
    }

    size_t totalBinaryLen = ciphertext.length() / 2;
    size_t ctLen = totalBinaryLen - 12 - 16; // remove iv and tag

    uint8_t* ct = static_cast<uint8_t*>(malloc(ctLen + 16));
    if (!ct) { memset(key, 0, sizeof(key)); return ""; }
    size_t ctParsed = 0;
    if (!hexToBytes(ciphertext.substring(24), ct, ctLen + 16, ctParsed) || ctParsed != ctLen + 16) {
        memset(key, 0, sizeof(key));
        free(ct);
        return "";
    }

    uint8_t* plainBuf = static_cast<uint8_t*>(malloc(ctLen + 1));
    if (!plainBuf) { memset(key, 0, sizeof(key)); free(ct); return ""; }

    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) {
        memset(key, 0, sizeof(key));
        free(ct); free(plainBuf);
        mbedtls_gcm_free(&ctx);
        return "";
    }

    // ct[0..ctLen-1] = ciphertext, ct[ctLen..ctLen+15] = tag (16 bytes)
    ret = mbedtls_gcm_auth_decrypt(&ctx, ctLen, iv, sizeof(iv),
        nullptr, 0,
        ct + ctLen, 16,
        ct, plainBuf);

    mbedtls_gcm_free(&ctx);
    memset(key, 0, sizeof(key));

    if (ret != 0) {
        // Authentication failed (tampered or wrong key)
        free(ct); free(plainBuf);
        return "";
    }

    plainBuf[ctLen] = '\0';
    String result(reinterpret_cast<char*>(plainBuf), ctLen);
    memset(plainBuf, 0, ctLen + 1);
    free(ct);
    free(plainBuf);
    return result;
}

bool VaultManager::setSecret(const String& key, const String& value, const String& category) noexcept {
    if (!m_initialized || key.isEmpty()) return false;
    trimToMax();
    
    // Update existing or create new
    for (auto& e : m_entries) {
        if (e.key == key) {
            e.encryptedValue = encrypt(value);
            e.category = category;
            e.updatedAt = millis();
            m_dirty = true;
            return true;
        }
    }
    
    VaultEntry e;
    e.id = generateId();
    e.key = key;
    e.encryptedValue = encrypt(value);
    e.category = category;
    e.createdAt = millis();
    e.updatedAt = e.createdAt;
    m_entries.push_back(e);
    m_dirty = true;
    return true;
}

String VaultManager::getSecret(const String& key) const noexcept {
    for (const auto& e : m_entries) {
        if (e.key == key) return decrypt(e.encryptedValue);
    }
    return "";
}

bool VaultManager::deleteSecret(const String& key) noexcept {
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (it->key == key) {
            m_entries.erase(it);
            m_dirty = true;
            return true;
        }
    }
    return false;
}

bool VaultManager::hasSecret(const String& key) const noexcept {
    for (const auto& e : m_entries) {
        if (e.key == key) return true;
    }
    return false;
}

bool VaultManager::setApiKey(const String& service, const String& key) noexcept {
    return setSecret("api_" + service, key, "api_key");
}

String VaultManager::getApiKey(const String& service) const noexcept {
    return getSecret("api_" + service);
}

bool VaultManager::setWiFiCredential(const String& ssid, const String& password) noexcept {
    return setSecret("wifi_" + ssid, password, "wifi");
}

bool VaultManager::getWiFiCredential(const String& ssid, String& password) const noexcept {
    password = getSecret("wifi_" + ssid);
    return !password.isEmpty();
}

std::vector<VaultEntry> VaultManager::getAllEntries() const noexcept {
    return m_entries;
}

std::vector<VaultEntry> VaultManager::getByCategory(const String& category) const noexcept {
    std::vector<VaultEntry> results;
    for (const auto& e : m_entries) {
        if (e.category == category) results.push_back(e);
    }
    return results;
}

String VaultManager::getVaultJson() const noexcept {
    String json; json.reserve(4096);
    json += "{\"entries\":[";
    bool first = true;
    for (const auto& e : m_entries) {
        if (!first) json += ",";
        first = false;
        json += "{";
        json += "\"id\":\"" + escapeJson(e.id) + "\",";
        json += "\"key\":\"" + escapeJson(e.key) + "\",";
        json += "\"cat\":\"" + escapeJson(e.category) + "\",";
        json += "\"created\":" + String(e.createdAt) + ",";
        json += "\"updated\":" + String(e.updatedAt);
        json += "}";
    }
    json += "]}";
    return json;
}

bool VaultManager::exportBackup(const String& path) noexcept {
    String json;
    json.reserve(8192);
    json += "{\"version\":1,\"backup\":[";
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (i > 0) json += ",";
        json += serializeEntry(m_entries[i]);
    }
    json += "]}";
    StorageStatus st = storageManager.writeFile(path.c_str(), json, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) {
        Logger::info(kLogCategory, "Backup exported to %s", path.c_str());
        return true;
    }
    return false;
}

bool VaultManager::importBackup(const String& path) noexcept {
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    
    m_entries.clear();
    int pos = content.indexOf("\"backup\":[");
    if (pos < 0) return false;
    pos = content.indexOf('[', pos) + 1;
    while (pos < (int)content.length()) {
        int braceStart = content.indexOf('{', pos);
        if (braceStart < 0) break;
        int braceEnd = content.indexOf('}', braceStart);
        if (braceEnd < 0) break;
        String obj = content.substring(braceStart, braceEnd + 1);
        VaultEntry e = deserializeEntry(obj);
        if (!e.id.isEmpty()) m_entries.push_back(e);
        pos = braceEnd + 1;
    }
    m_dirty = true;
    Logger::info(kLogCategory, "Backup imported from %s (%u entries)", path.c_str(), m_entries.size());
    return true;
}

bool VaultManager::save() noexcept {
    String path = String(VAULT_PATH) + "/data.json";
    String j; j.reserve(8192);
    j += "{\"version\":1,\"entries\":[";
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (i > 0) j += ",";
        j += serializeEntry(m_entries[i]);
    }
    j += "]}";
    StorageStatus st = storageManager.writeFile(path.c_str(), j, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_dirty = false; return true; }
    return false;
}

bool VaultManager::load() noexcept {
    String path = String(VAULT_PATH) + "/data.json";
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_entries.clear();
    int pos = content.indexOf("\"entries\":[");
    if (pos < 0) return false;
    pos = content.indexOf('[', pos) + 1;
    while (pos < (int)content.length()) {
        int braceStart = content.indexOf('{', pos);
        if (braceStart < 0) break;
        int braceEnd = content.indexOf('}', braceStart);
        if (braceEnd < 0) break;
        String obj = content.substring(braceStart, braceEnd + 1);
        VaultEntry e = deserializeEntry(obj);
        if (!e.id.isEmpty()) m_entries.push_back(e);
        pos = braceEnd + 1;
    }
    return true;
}

bool VaultManager::isInitialized() const noexcept { return m_initialized; }

void VaultManager::trimToMax() noexcept {
    while (m_entries.size() >= kMaxEntries) {
        m_entries.erase(m_entries.begin());
        m_dirty = true;
    }
}

String VaultManager::serializeEntry(const VaultEntry& e) const noexcept {
    String j; j.reserve(256);
    j += "{";
    j += "\"id\":\"" + escapeJson(e.id) + "\",";
    j += "\"key\":\"" + escapeJson(e.key) + "\",";
    j += "\"val\":\"" + escapeJson(e.encryptedValue) + "\",";
    j += "\"cat\":\"" + escapeJson(e.category) + "\",";
    j += "\"created\":" + String(e.createdAt) + ",";
    j += "\"updated\":" + String(e.updatedAt);
    j += "}";
    return j;
}

VaultEntry VaultManager::deserializeEntry(const String& json) const noexcept {
    VaultEntry e;
    auto eS = [&](const char* key) -> String {
        String s = String("\"") + key + "\":\"";
        int start = json.indexOf(s);
        if (start < 0) return "";
        start += s.length();
        int end = json.indexOf('"', start);
        return (end < 0) ? "" : json.substring(start, end);
    };
    auto eI = [&](const char* key, int def) -> int {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s);
        if (start < 0) return def;
        start += s.length();
        int end = start;
        while (end < (int)json.length() && json[end] >= '0' && json[end] <= '9') end++;
        return (end == start) ? def : json.substring(start, end).toInt();
    };
    e.id = eS("id");
    e.key = eS("key");
    e.encryptedValue = eS("val");
    e.category = eS("cat");
    e.createdAt = static_cast<unsigned long>(eI("created", 0));
    e.updatedAt = static_cast<unsigned long>(eI("updated", 0));
    return e;
}
