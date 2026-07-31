#include "tiny_ai_manager.h"

TinyAIManager tinyAIManager;

TinyAIManager::TinyAIManager() noexcept
    : m_initialized(false), m_enabled(TINYAI_ENABLED_DEFAULT), m_inferenceCount(0), m_lastInferenceTimeMs(0) {}

TinyAIManager::~TinyAIManager() noexcept {}

bool TinyAIManager::initialize() noexcept {
    if (m_initialized) {
        return true;
    }

    m_enabled = true;
    m_inferenceCount = 0;
    m_lastInferenceTimeMs = 0;
    m_initialized = true;

    LOG_INFO(kLogCategory, "Tiny AI engine initialized (offline fallback ready)");
    return true;
}

void TinyAIManager::update() noexcept {
}

bool TinyAIManager::process(const String& userText, String& responseText) noexcept {
    if (!m_initialized || !m_enabled) {
        return false;
    }

    if (userText.isEmpty()) {
        return false;
    }

    unsigned long start = millis();

    IntentResult intent = m_classifier.classify(userText);

    if (intent.type == IntentType::UNKNOWN && intent.confidence < 0.2f) {
        m_lastInferenceTimeMs = millis() - start;
        return false;
    }

    responseText = m_generator.generate(intent);

    m_lastInferenceTimeMs = millis() - start;
    m_inferenceCount++;

    if (responseText.isEmpty()) {
        return false;
    }

    LOG_INFO(kLogCategory, "Intent: %d confidence: %.2f response: %s",
        static_cast<int>(intent.type), intent.confidence, responseText.c_str());
    return true;
}

bool TinyAIManager::isInitialized() const noexcept {
    return m_initialized;
}

bool TinyAIManager::isEnabled() const noexcept {
    return m_enabled;
}

void TinyAIManager::setEnabled(bool enabled) noexcept {
    m_enabled = enabled;
    LOG_INFO(kLogCategory, "Offline AI %s", enabled ? "enabled" : "disabled");
}

size_t TinyAIManager::getInferenceCount() const noexcept {
    return m_inferenceCount;
}

unsigned long TinyAIManager::getLastInferenceTimeMs() const noexcept {
    return m_lastInferenceTimeMs;
}

String TinyAIManager::getStatusJSON() const noexcept {
    String json;
    json.reserve(256);
    json += "{";
    json += "\"initialized\":" + String(m_initialized ? "true" : "false") + ",";
    json += "\"enabled\":" + String(m_enabled ? "true" : "false") + ",";
    json += "\"inferenceCount\":" + String(m_inferenceCount) + ",";
    json += "\"lastInferenceTimeMs\":" + String(m_lastInferenceTimeMs);
    json += "}";
    return json;
}
