#include "audio_asset_manager.h"
#include "storage_manager.h"
#include "audio_manager.h"
#include "logger.h"
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

static const char* kTag = "AudioAsset";

AudioAssetManager& AudioAssetManager::instance() {
    static AudioAssetManager inst;
    return inst;
}

bool AudioAssetManager::begin() {
    m_mutex = xSemaphoreCreateMutex();
    if (!m_mutex) {
        LOG_ERROR(kTag, "Failed to create mutex");
        return false;
    }
    m_initialized = true;

    storageManager.createDirectory(AUDIO_CACHE_PATH, StorageType::SD_CARD);

    if (!loadManifest("default")) {
        LOG_WARNING(kTag, "No manifest found, assets unavailable");
    }

    return true;
}

bool AudioAssetManager::loadManifest(const String& theme) {
    if (!m_initialized) return false;

    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_currentTheme = theme;
    m_manifest.clear();
    m_manifestLoaded = false;
    xSemaphoreGive(m_mutex);

    String path;
    if (theme.isEmpty() || theme == "default") {
        path = AUDIO_MANIFEST_PATH;
    } else {
        path = String(AUDIO_THEMES_PATH) + "/" + theme + ".json";
    }

    return loadManifestFromPath(path);
}

bool AudioAssetManager::loadManifestFromPath(const String& path) {
    String content;
    StorageStatus status;

    status = storageManager.readFile(path.c_str(), content, StorageType::SD_CARD);
    if (status != StorageStatus::SUCCESS) {
        status = storageManager.readFile(path.c_str(), content, StorageType::SPIFFS);
        if (status != StorageStatus::SUCCESS) {
            return false;
        }
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, content);
    if (err) {
        LOG_ERROR(kTag, "Manifest parse error: %s", err.c_str());
        return false;
    }

    JsonObject assets = doc["assets"];
    if (assets.isNull()) {
        LOG_ERROR(kTag, "Manifest missing 'assets' key");
        return false;
    }

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    for (JsonPair kv : assets) {
        AssetInfo info;
        info.filename = kv.value()["file"].as<String>();
        info.priority = kv.value()["priority"] | 5;
        info.loop = kv.value()["loop"] | false;
        m_manifest[kv.key().c_str()] = info;
    }

    m_manifestPath = path;
    m_manifestLoaded = true;

    xSemaphoreGive(m_mutex);

    LOG_INFO(kTag, "Loaded %d assets from %s", m_manifest.size(), path.c_str());
    return true;
}

bool AudioAssetManager::assetExists(const String& name) const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    bool found = m_manifest.find(name) != m_manifest.end();
    xSemaphoreGive(m_mutex);
    return found;
}

bool AudioAssetManager::playAsset(const String& name, int priority) {
    if (!m_initialized || !m_manifestLoaded) return false;

    (void)priority;

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    auto it = m_manifest.find(name);
    if (it == m_manifest.end()) {
        xSemaphoreGive(m_mutex);
        return false;
    }

    auto cacheIt = m_cache.find(name);
    if (cacheIt != m_cache.end()) {
        touchLRU(name);
        CachedAsset cached = cacheIt->second;
        xSemaphoreGive(m_mutex);

        m_activeAsset = name;
        return streamPlayback(cached.data);
    }

    xSemaphoreGive(m_mutex);

    if (!cacheAsset(name)) return false;

    xSemaphoreTake(m_mutex, portMAX_DELAY);
    cacheIt = m_cache.find(name);
    if (cacheIt == m_cache.end()) {
        xSemaphoreGive(m_mutex);
        return false;
    }
    CachedAsset cached = cacheIt->second;
    xSemaphoreGive(m_mutex);

    m_activeAsset = name;
    return streamPlayback(cached.data);
}

bool AudioAssetManager::streamPlayback(const std::vector<int16_t>& pcmData) {
    if (pcmData.empty()) return false;

    const uint8_t* rawData = reinterpret_cast<const uint8_t*>(pcmData.data());
    size_t remainingBytes = pcmData.size() * sizeof(int16_t);
    size_t offset = 0;

    audioManager.stopPlayback();
    if (!audioManager.startPlayback()) {
        LOG_ERROR(kTag, "Failed to start playback");
        return false;
    }

    while (remainingBytes > 0) {
        size_t chunkSize = (remainingBytes > 512) ? 512 : remainingBytes;
        size_t written = 0;
        if (!audioManager.play(rawData + offset, chunkSize, written)) {
            audioManager.stopPlayback();
            LOG_WARNING(kTag, "Playback write failed at offset %u", offset);
            return false;
        }
        offset += written;
        remainingBytes -= written;
        if (esp_task_wdt_status(nullptr) == ESP_OK) esp_task_wdt_reset();
        if (written == 0) {
            vTaskDelay(1);
        }
    }

    audioManager.stopPlayback();
    return true;
}

bool AudioAssetManager::stopAsset(const String& name) {
    if (m_activeAsset == name) {
        audioManager.stopPlayback();
        m_activeAsset = "";
        return true;
    }
    return false;
}

void AudioAssetManager::stopAll() {
    audioManager.stopPlayback();
    m_activeAsset = "";
}

bool AudioAssetManager::isPlaying() const {
    return audioManager.isPlaying();
}

bool AudioAssetManager::setTheme(const String& theme) {
    return loadManifest(theme);
}

void AudioAssetManager::setVolume(uint8_t vol) {
    m_volume = (vol > 100) ? 100 : vol;
    audioManager.setVolume(m_volume);
}

void AudioAssetManager::clearCache() {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_cache.clear();
    m_lruOrder.clear();
    xSemaphoreGive(m_mutex);
}

void AudioAssetManager::setCacheMax(size_t entries) {
    m_maxCacheEntries = (entries == 0) ? 1 : entries;
}

bool AudioAssetManager::cacheAsset(const String& name) {
    auto it = m_manifest.find(name);
    if (it == m_manifest.end()) return false;

    std::vector<int16_t> pcmData;
    if (!readAssetData(it->second.filename, pcmData)) return false;

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    while (m_cache.size() >= m_maxCacheEntries) {
        evictLRU();
    }

    CachedAsset& cached = m_cache[name];
    cached.data = std::move(pcmData);
    cached.lastAccess = millis();
    touchLRU(name);

    xSemaphoreGive(m_mutex);
    return true;
}

bool AudioAssetManager::readAssetData(const String& filename, std::vector<int16_t>& outData) {
    String fullPath = resolvePath(filename);

    size_t fileSize = storageManager.getFileSize(fullPath.c_str(), StorageType::SD_CARD);
    if (fileSize == 0) {
        LOG_WARNING(kTag, "Asset file not found: %s", fullPath.c_str());
        return false;
    }
    if (fileSize > AUDIO_MAX_ASSET_SIZE) {
        LOG_WARNING(kTag, "Asset %s too large (%u bytes)", filename.c_str(), fileSize);
        return false;
    }

    std::vector<uint8_t> rawData(fileSize);
    size_t bytesRead = 0;
    StorageStatus status = storageManager.readFile(
        fullPath.c_str(), rawData.data(), fileSize, bytesRead, StorageType::SD_CARD);

    if (status != StorageStatus::SUCCESS || bytesRead != fileSize) {
        LOG_ERROR(kTag, "Failed to read %s", fullPath.c_str());
        return false;
    }

    if (rawData.size() < 44) {
        LOG_ERROR(kTag, "%s too small for WAV header", filename.c_str());
        return false;
    }

    if (rawData[0] != 'R' || rawData[1] != 'I' || rawData[2] != 'F' || rawData[3] != 'F' ||
        rawData[8] != 'W' || rawData[9] != 'A' || rawData[10] != 'V' || rawData[11] != 'E') {
        LOG_ERROR(kTag, "%s not a valid WAV file", filename.c_str());
        return false;
    }

    uint32_t offset = 12;
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;

    while (offset + 8 <= rawData.size()) {
        uint32_t chunkSize = rawData[offset + 4] |
                            (rawData[offset + 5] << 8) |
                            (rawData[offset + 6] << 16) |
                            (rawData[offset + 7] << 24);

        if (rawData[offset] == 'd' && rawData[offset + 1] == 'a' &&
            rawData[offset + 2] == 't' && rawData[offset + 3] == 'a') {
            dataOffset = offset + 8;
            dataSize = chunkSize;
            break;
        }

        offset += 8 + chunkSize;
        if (chunkSize % 2 != 0) offset++;
    }

    if (dataOffset == 0 || dataSize == 0) {
        LOG_ERROR(kTag, "%s has no data chunk", filename.c_str());
        return false;
    }

    // The data chunk size comes from the file header and may lie about the
    // actual file length (truncated/corrupt WAV on the user-writable SD card).
    // Clamp to the bytes actually read so memcpy below cannot read past the
    // heap buffer and crash the device.
    const uint32_t available = static_cast<uint32_t>(rawData.size()) - dataOffset;
    if (dataSize > available) {
        LOG_WARNING(kTag, "%s data chunk (%u B) exceeds file size (%u B); truncating",
            filename.c_str(), static_cast<unsigned>(dataSize), static_cast<unsigned>(available));
        dataSize = available;
    }

    uint32_t sampleCount = dataSize / 2;
    outData.resize(sampleCount);
    memcpy(outData.data(), rawData.data() + dataOffset, dataSize);

    LOG_DEBUG(kTag, "Loaded %s (%d samples)", filename.c_str(), sampleCount);
    return true;
}

void AudioAssetManager::evictLRU() {
    if (m_lruOrder.empty() || m_cache.empty()) return;
    String oldest = m_lruOrder.back();
    m_lruOrder.pop_back();
    m_cache.erase(oldest);
}

void AudioAssetManager::touchLRU(const String& name) {
    m_lruOrder.remove(name);
    m_lruOrder.push_front(name);
    auto it = m_cache.find(name);
    if (it != m_cache.end()) {
        it->second.lastAccess = millis();
    }
}

String AudioAssetManager::resolvePath(const String& filename) const {
    if (filename.startsWith("/")) return filename;
    return String(AUDIO_FOLDER) + "/" + filename;
}
