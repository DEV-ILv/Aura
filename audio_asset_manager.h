#ifndef AUDIO_ASSET_MANAGER_H
#define AUDIO_ASSET_MANAGER_H

#include <Arduino.h>
#include <map>
#include <vector>
#include <list>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class AudioAssetManager {
public:
    static AudioAssetManager& instance();

    bool begin();
    bool loadManifest(const String& theme = "");
    bool playAsset(const String& name, int priority = 5);
    bool stopAsset(const String& name);
    void stopAll();
    bool isPlaying() const;
    bool assetExists(const String& name) const;
    String getCurrentTheme() const { return m_currentTheme; }
    bool setTheme(const String& theme);
    void setVolume(uint8_t vol);
    uint8_t getVolume() const { return m_volume; }
    void clearCache();
    void setCacheMax(size_t entries);

private:
    AudioAssetManager() = default;
    ~AudioAssetManager() = default;
    AudioAssetManager(const AudioAssetManager&) = delete;
    AudioAssetManager& operator=(const AudioAssetManager&) = delete;

    struct AssetInfo {
        String filename;
        int priority = 5;
        bool loop = false;
        uint32_t size = 0;
    };

    struct CachedAsset {
        std::vector<int16_t> data;
        uint32_t lastAccess;
    };

    bool loadManifestFromPath(const String& path);
    bool cacheAsset(const String& name);
    bool readAssetData(const String& filename, std::vector<int16_t>& outData);
    bool streamPlayback(const std::vector<int16_t>& pcmData);
    void evictLRU();
    void touchLRU(const String& name);
    String resolvePath(const String& filename) const;

    String m_currentTheme = "default";
    String m_manifestPath;
    std::map<String, AssetInfo> m_manifest;
    std::map<String, CachedAsset> m_cache;
    std::list<String> m_lruOrder;
    size_t m_maxCacheEntries = 8;
    SemaphoreHandle_t m_mutex = nullptr;
    bool m_initialized = false;
    bool m_manifestLoaded = false;
    uint8_t m_volume = 100;
    String m_activeAsset;
};

#endif
