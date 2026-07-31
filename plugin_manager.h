#ifndef AURA_PLUGIN_MANAGER_H
#define AURA_PLUGIN_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

/**
 * @struct PluginMetadata
 * @brief Metadata for a single plugin
 */
struct PluginMetadata {
    String id;                  ///< Plugin directory name / unique ID
    String name;                ///< Display name
    String version;             ///< Semantic version string
    String author;              ///< Plugin author
    String description;         ///< Brief description
    String category;            ///< "tool", "game", "utility", "integration", "theme"
    String tags;                ///< Comma-separated tags
    String dependencies;        ///< Comma-separated plugin IDs
    String icon;                ///< Icon filename (relative to plugin assets/)
    String requiredFirmware;    ///< Minimum firmware version required
    String path;                ///< Full path on SD card
    String sourceUrl;           ///< Marketplace URL or local path
    String checksum;            ///< For integrity verification
    bool enabled;               ///< Whether plugin is currently enabled
    unsigned long installDate;  ///< Install timestamp
    uint32_t downloadCount;     ///< Download count
    uint8_t rating;             ///< Rating 0-5
    unsigned long lastUpdated;  ///< Last update timestamp

    PluginMetadata() noexcept
        : enabled(false), installDate(0), downloadCount(0), rating(0), lastUpdated(0) {}
};

/**
 * @struct PluginConfig
 * @brief Runtime configuration for a plugin
 */
struct PluginConfig {
    String pluginId;            ///< Associated plugin ID
    String settings;            ///< Plugin-specific config JSON string

    PluginConfig() noexcept {}
};

/**
 * @class PluginManager
 * @brief Manages plugin discovery, loading, and lifecycle
 *
 * Plugins are configuration/content modules stored on SD card.
 * No executable code support - metadata and assets only.
 *
 * Directory structure:
 *   /plugins/<plugin_id>/
 *     plugin.json
 *     assets/
 */
class PluginManager {
public:
    PluginManager() noexcept;
    ~PluginManager() noexcept;

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    PluginManager(PluginManager&&) = delete;
    PluginManager& operator=(PluginManager&&) = delete;

    /**
     * @brief Initialize plugin manager
     * @return true if initialized successfully
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Update plugin manager (periodic refresh)
     */
    void update() noexcept;

    /**
     * @brief Discover all plugins on SD card
     * @return Number of plugins discovered
     */
    [[nodiscard]] size_t discoverPlugins() noexcept;

    /**
     * @brief Load metadata from a plugin's plugin.json
     * @param pluginId Plugin ID (directory name)
     * @return true if loaded successfully
     */
    [[nodiscard]] bool loadPlugin(const String& pluginId) noexcept;

    /**
     * @brief Enable a plugin
     * @param pluginId Plugin ID
     * @return true if enabled
     */
    [[nodiscard]] bool enablePlugin(const String& pluginId) noexcept;

    /**
     * @brief Disable a plugin
     * @param pluginId Plugin ID
     * @return true if disabled
     */
    [[nodiscard]] bool disablePlugin(const String& pluginId) noexcept;

    /**
     * @brief Get metadata for a specific plugin
     * @param pluginId Plugin ID
     * @return PluginMetadata (empty id if not found)
     */
    [[nodiscard]] PluginMetadata getPlugin(const String& pluginId) const noexcept;

    /**
     * @brief Get all discovered plugins
     * @return Vector of plugin metadata
     */
    [[nodiscard]] const std::vector<PluginMetadata>& getAllPlugins() const noexcept;

    /**
     * @brief Get enabled plugin count
     * @return Count of enabled plugins
     */
    [[nodiscard]] size_t enabledCount() const noexcept;

    /**
     * @brief Check if plugin manager is initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    // Marketplace
    [[nodiscard]] bool registerPlugin(const PluginMetadata& plugin) noexcept;
    [[nodiscard]] bool unregisterPlugin(const String& pluginId) noexcept;
    [[nodiscard]] bool updatePlugin(const String& pluginId, const PluginMetadata& updates) noexcept;
    [[nodiscard]] PluginMetadata getPluginInfo(const String& pluginId) const noexcept;
    [[nodiscard]] std::vector<PluginMetadata> searchPlugins(const String& query) const noexcept;
    [[nodiscard]] std::vector<PluginMetadata> getPluginsByCategory(const String& category) const noexcept;
    [[nodiscard]] bool isPluginEnabled(const String& pluginId) const noexcept;

    /**
     * @brief Save enabled/disabled state
     * @return true if saved
     */
    [[nodiscard]] bool saveState() noexcept;

    /**
     * @brief Load enabled/disabled state
     * @return true if loaded
     */
    [[nodiscard]] bool loadState() noexcept;

private:
    static constexpr const char* kLogCategory = "PluginManager";
    static constexpr const char* kStatePath = "/plugins_state.json";
    static constexpr const char* kMarketplacePath = "/plugin_marketplace.json";
    static constexpr size_t kMaxPlugins = PLUGIN_MAX_COUNT;

    bool parsePluginJson(const String& json, PluginMetadata& meta) noexcept;
    bool scanDirectory() noexcept;
    size_t findPlugin(const String& id) const noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<PluginMetadata> m_plugins;
};

extern PluginManager pluginManager;

#endif // AURA_PLUGIN_MANAGER_H
