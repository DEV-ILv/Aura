#include "scene_engine.h"
#include "display_manager.h"
#include "settings_manager.h"

SceneEngine sceneEngine;

SceneEngine::SceneEngine() noexcept
    : Service(kStaticName, BootPriority::NORMAL)
    , m_activeScene(SceneID::SCENE_DEFAULT)
    , m_previousSaved(false)
    , m_initialized(false) {
    m_scenes.reserve(kMaxScenes);
}

SceneEngine::~SceneEngine() noexcept = default;

bool SceneEngine::Initialize() noexcept {
    if (GetState() != ServiceState::UNINITIALIZED) return true;
    SetState(ServiceState::INITIALIZING);

    // Register default scenes
    SceneConfig studyCfg;
    studyCfg.screen = ScreenID::DASHBOARD;
    studyCfg.primaryWidget = WidgetID::STUDY_PROGRESS;
    studyCfg.brightness = 100;
    studyCfg.silentMode = true;
    studyCfg.autoSleep = false;
    RegisterScene(SceneID::STUDY, studyCfg, "Study");

    SceneConfig codingCfg;
    codingCfg.screen = ScreenID::DASHBOARD;
    codingCfg.primaryWidget = WidgetID::PROJECTS;
    codingCfg.brightness = 120;
    RegisterScene(SceneID::CODING, codingCfg, "Coding");

    SceneConfig meetingCfg;
    meetingCfg.screen = ScreenID::ASSISTANT_CHAT;
    meetingCfg.silentMode = true;
    meetingCfg.showNotifications = false;
    RegisterScene(SceneID::MEETING, meetingCfg, "Meeting");

    SceneConfig nightCfg;
    nightCfg.brightness = 20;
    nightCfg.silentMode = true;
    nightCfg.autoSleep = true;
    nightCfg.showNotifications = false;
    RegisterScene(SceneID::NIGHT, nightCfg, "Night");

    SceneConfig relaxCfg;
    relaxCfg.screen = ScreenID::DASHBOARD;
    relaxCfg.brightness = 80;
    relaxCfg.showSuggestions = true;
    RegisterScene(SceneID::RELAX, relaxCfg, "Relax");

    SceneConfig focusCfg;
    focusCfg.screen = ScreenID::DASHBOARD;
    focusCfg.primaryWidget = WidgetID::STUDY_PROGRESS;
    focusCfg.silentMode = true;
    focusCfg.showNotifications = false;
    focusCfg.showSuggestions = false;
    RegisterScene(SceneID::FOCUS, focusCfg, "Focus");

    SetState(ServiceState::INITIALIZED);
    m_initialized = true;
    LOG_INFO(kLogCategory, "SceneEngine initialized (%zu scenes)", m_scenes.size());
    return true;
}

void SceneEngine::Update() noexcept {}

bool SceneEngine::ActivateScene(SceneID id) noexcept {
    int idx = FindSceneIndex(id);
    if (idx < 0) {
        LOG_WARNING(kLogCategory, "Unknown scene: %d", static_cast<int>(id));
        return false;
    }

    // Save current state before switching
    if (!m_previousSaved) {
        m_previousConfig = m_defaultConfig;
        m_previousSaved = true;
    }

    m_activeScene = id;
    m_activeCustomName = "";
    ApplyScene(m_scenes[idx].config);

    LOG_INFO(kLogCategory, "Scene activated: %s", m_scenes[idx].name.c_str());

    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::CONTEXT_SWITCHED, "SceneEngine",
                         "{\"scene\":\"" + m_scenes[idx].name + "\"}");
    }

    return true;
}

bool SceneEngine::ActivateScene(const String& name) noexcept {
    int idx = FindSceneIndex(name);
    if (idx >= 0) {
        m_activeScene = m_scenes[idx].id;
        m_activeCustomName = "";
        ApplyScene(m_scenes[idx].config);
        return true;
    }

    // Check custom scenes by name
    for (size_t i = 0; i < m_scenes.size(); ++i) {
        if (m_scenes[i].id == SceneID::CUSTOM && m_scenes[i].name == name) {
            m_activeScene = SceneID::CUSTOM;
            m_activeCustomName = name;
            ApplyScene(m_scenes[i].config);
            return true;
        }
    }

    return false;
}

bool SceneEngine::DeactivateScene() noexcept {
    if (!m_previousSaved) return false;
    RestoreDefaults();
    m_previousSaved = false;
    m_activeScene = SceneID::SCENE_DEFAULT;
    return true;
}

bool SceneEngine::RestorePreviousScene() noexcept {
    if (!m_previousSaved) return false;
    RestoreDefaults();
    m_previousSaved = false;
    return true;
}

bool SceneEngine::RegisterScene(SceneID id, const SceneConfig& config, const String& name) noexcept {
    if (m_scenes.size() >= kMaxScenes) return false;
    if (FindSceneIndex(id) >= 0) return false;

    SceneEntry entry;
    entry.id = id;
    entry.config = config;
    entry.name = name;
    m_scenes.push_back(entry);
    return true;
}

bool SceneEngine::RegisterCustomScene(const String& name, const SceneConfig& config) noexcept {
    if (m_scenes.size() >= kMaxScenes) return false;
    if (FindSceneIndex(SceneID::CUSTOM) >= 0) {
        // Update existing custom scene
        for (auto& s : m_scenes) {
            if (s.id == SceneID::CUSTOM && s.name == name) {
                s.config = config;
                return true;
            }
        }
    }
    return RegisterScene(SceneID::CUSTOM, config, name);
}

SceneID SceneEngine::GetActiveScene() const noexcept { return m_activeScene; }

String SceneEngine::GetActiveSceneName() const noexcept {
    int idx = FindSceneIndex(m_activeScene);
    if (idx >= 0) return m_scenes[idx].name;
    return m_activeCustomName;
}

SceneConfig SceneEngine::GetSceneConfig(SceneID id) const noexcept {
    int idx = FindSceneIndex(id);
    if (idx < 0) return SceneConfig();
    return m_scenes[idx].config;
}

bool SceneEngine::IsSceneActive() const noexcept {
    return m_previousSaved;
}

std::vector<String> SceneEngine::GetAvailableScenes() const noexcept {
    std::vector<String> names;
    for (const auto& s : m_scenes) {
        names.push_back(s.name);
    }
    return names;
}

void SceneEngine::HandleEvent(const String& eventType, const String& eventData) noexcept {
    Service::HandleEvent(eventType, eventData);

    if (eventType == "CONTEXT_SWITCHED") {
        // Auto-activate scene based on context
        if (eventData.indexOf("study") >= 0 || eventData.indexOf("learning") >= 0) {
            ActivateScene(SceneID::STUDY);
        } else if (eventData.indexOf("meeting") >= 0) {
            ActivateScene(SceneID::MEETING);
        } else if (eventData.indexOf("night") >= 0 || eventData.indexOf("sleep") >= 0) {
            ActivateScene(SceneID::NIGHT);
        }
    }
}

void SceneEngine::ApplyScene(const SceneConfig& config) noexcept {
    if (displayManager.isInitialized()) {
        // Apply brightness
        if (config.brightness < 255) {
            displayManager.setBrightness(config.brightness);
        }
    }

    if (settingsManager.isInitialized()) {
        settingsManager.setSilentMode(config.silentMode);
    }

    // Notify via EventBus
    if (eventBus.isInitialized()) {
        String payload = "{\"scene\":\"" + GetActiveSceneName() +
                         "\",\"silent\":" + String(config.silentMode ? "true" : "false") +
                         ",\"brightness\":" + String(config.brightness) + "}";
        eventBus.publish(EventType::CONTEXT_SWITCHED, "SceneEngine", payload);
    }
}

void SceneEngine::RestoreDefaults() noexcept {
    ApplyScene(m_previousConfig);

    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::CONTEXT_SWITCHED, "SceneEngine",
                         "{\"scene\":\"default\",\"action\":\"restored\"}");
    }
}

int SceneEngine::FindSceneIndex(SceneID id) const noexcept {
    for (size_t i = 0; i < m_scenes.size(); ++i) {
        if (m_scenes[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

int SceneEngine::FindSceneIndex(const String& name) const noexcept {
    for (size_t i = 0; i < m_scenes.size(); ++i) {
        if (m_scenes[i].name == name) return static_cast<int>(i);
    }
    return -1;
}