#ifndef AURA_SCENE_ENGINE_H
#define AURA_SCENE_ENGINE_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "service.h"
#include "ui_event_types.h"

enum class SceneID : uint8_t {
    SCENE_DEFAULT,
    STUDY,
    CODING,
    MEETING,
    MAINTENANCE,
    TRAVEL,
    NIGHT,
    PRESENTATION,
    RELAX,
    FOCUS,
    CUSTOM
};

struct SceneConfig {
    ScreenID screen;
    WidgetID primaryWidget;
    uint8_t brightness;
    bool silentMode;
    bool autoSleep;
    bool showNotifications;
    bool showSuggestions;
    String dashboardLayout; // JSON describing widget layout

    SceneConfig() noexcept
        : screen(ScreenID::DASHBOARD)
        , primaryWidget(WidgetID::NONE)
        , brightness(128)
        , silentMode(false)
        , autoSleep(true)
        , showNotifications(true)
        , showSuggestions(true)
        , dashboardLayout("default") {}
};

class SceneEngine : public Service {
public:
    SceneEngine() noexcept;
    ~SceneEngine() noexcept;

    bool Initialize() noexcept override;
    void Update() noexcept override;

    // Scene management
    bool ActivateScene(SceneID id) noexcept;
    bool ActivateScene(const String& name) noexcept;
    bool DeactivateScene() noexcept;
    bool RestorePreviousScene() noexcept;

    // Registration
    bool RegisterScene(SceneID id, const SceneConfig& config, const String& name) noexcept;
    bool RegisterCustomScene(const String& name, const SceneConfig& config) noexcept;

    // Query
    SceneID GetActiveScene() const noexcept;
    String GetActiveSceneName() const noexcept;
    SceneConfig GetSceneConfig(SceneID id) const noexcept;
    bool IsSceneActive() const noexcept;
    std::vector<String> GetAvailableScenes() const noexcept;

    void HandleEvent(const String& eventType, const String& eventData) noexcept override;

    static constexpr const char* kStaticName = "SceneEngine";

private:
    void ApplyScene(const SceneConfig& config) noexcept;
    void RestoreDefaults() noexcept;
    int FindSceneIndex(SceneID id) const noexcept;
    int FindSceneIndex(const String& name) const noexcept;

    struct SceneEntry {
        SceneID id;
        SceneConfig config;
        String name;
    };

    static constexpr const char* kLogCategory = "SceneEngine";
    static constexpr size_t kMaxScenes = 32;

    std::vector<SceneEntry> m_scenes;
    SceneID m_activeScene;
    SceneConfig m_defaultConfig;
    SceneConfig m_previousConfig;
    bool m_previousSaved;
    String m_activeCustomName;
    bool m_initialized;
};

extern SceneEngine sceneEngine;

#endif