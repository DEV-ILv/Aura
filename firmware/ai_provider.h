#ifndef AURA_AI_PROVIDER_H
#define AURA_AI_PROVIDER_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "logger.h"
#include "service.h"

enum class AIProviderType : uint8_t {
    GEMINI,
    OPENAI,
    CLAUDE,
    LOCAL_LLM,
    TINY_AI,
    CUSTOM
};

enum class AIModelCapability : uint8_t {
    TEXT_GENERATION,
    CHAT,
    CODE,
    IMAGE_ANALYSIS,
    AUDIO_TRANSCRIPTION,
    TEXT_TO_SPEECH,
    EMBEDDINGS,
    FUNCTION_CALLING,
    STREAMING,
    VISION
};

struct AIProviderInfo {
    AIProviderType type;
    String name;
    String model;
    String version;
    float maxTokens;
    bool available;
    bool streaming;
    std::vector<AIModelCapability> capabilities;

    AIProviderInfo() noexcept
        : type(AIProviderType::CUSTOM), maxTokens(0), available(false), streaming(false) {}
};

struct AIRequest {
    String prompt;
    String systemPrompt;
    float temperature;
    float topP;
    int maxTokens;
    bool stream;
    std::vector<String> tools; // Function names

    AIRequest() noexcept
        : temperature(0.7f), topP(0.95f), maxTokens(2048), stream(false) {}
};

struct AIResponse {
    String text;
    String finishReason;
    float confidence;
    unsigned long latencyMs;
    bool success;
    int promptTokens;
    int completionTokens;

    AIResponse() noexcept
        : confidence(1.0f), latencyMs(0), success(false),
          promptTokens(0), completionTokens(0) {}
};

using AIStreamCallback = std::function<void(const String& chunk)>;
using AICallback = std::function<void(const AIResponse& response)>;

class AIProvider : public Service {
public:
    AIProvider(const char* name, AIProviderType type) noexcept;
    ~AIProvider() noexcept override;

    virtual bool SendRequest(const AIRequest& request, AICallback callback) noexcept = 0;
    virtual bool SendRequestStream(const AIRequest& request, AIStreamCallback streamCallback,
                                    AICallback completionCallback) noexcept = 0;
    virtual bool Cancel() noexcept = 0;

    virtual AIProviderInfo GetProviderInfo() const noexcept = 0;
    virtual bool IsAvailable() const noexcept = 0;
    virtual bool TestConnection() noexcept = 0;

    AIProviderType GetProviderType() const noexcept;

protected:
    AIProviderType m_type;
};

#endif