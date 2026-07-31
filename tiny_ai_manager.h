#ifndef AURA_TINY_AI_MANAGER_H
#define AURA_TINY_AI_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "logger.h"
#include "intent_classifier.h"
#include "offline_response_generator.h"

class TinyAIManager {
public:
    TinyAIManager() noexcept;
    ~TinyAIManager() noexcept;

    TinyAIManager(const TinyAIManager&) = delete;
    TinyAIManager& operator=(const TinyAIManager&) = delete;

    bool initialize() noexcept;
    void update() noexcept;

    bool process(const String& userText, String& responseText) noexcept;

    bool isInitialized() const noexcept;
    bool isEnabled() const noexcept;
    void setEnabled(bool enabled) noexcept;

    size_t getInferenceCount() const noexcept;
    unsigned long getLastInferenceTimeMs() const noexcept;
    String getStatusJSON() const noexcept;

private:
    bool m_initialized;
    bool m_enabled;
    size_t m_inferenceCount;
    unsigned long m_lastInferenceTimeMs;

    IntentClassifier m_classifier;
    OfflineResponseGenerator m_generator;

    static constexpr const char* kLogCategory = "TinyAI";
};

extern TinyAIManager tinyAIManager;

#endif
