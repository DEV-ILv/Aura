#ifndef AURA_AI_PIPELINE_H
#define AURA_AI_PIPELINE_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "service.h"
#include "task_scheduler.h"
#include "ai_provider.h"

enum class PipelineStage : uint8_t {
    IDLE,
    WAITING_FOR_INPUT,
    AUDIO_CAPTURE,
    SPEECH_TO_TEXT,
    AI_PROCESSING,
    TEXT_TO_SPEECH,
    PLAYBACK,
    COMPLETED,
    ERROR
};

struct PipelineMetrics {
    unsigned long totalPipelines;
    unsigned long failedPipelines;
    unsigned long avgSttMs;
    unsigned long avgAiMs;
    unsigned long avgTtsMs;
    unsigned long totalAudioMs;

    PipelineMetrics() noexcept
        : totalPipelines(0), failedPipelines(0),
          avgSttMs(0), avgAiMs(0), avgTtsMs(0), totalAudioMs(0) {}
};

class AIPipeline : public Service {
public:
    AIPipeline() noexcept;
    ~AIPipeline() noexcept;

    bool Initialize() noexcept override;
    void Update() noexcept override;
    bool Stop() noexcept override;
    bool Suspend() noexcept override;
    bool Resume() noexcept override;
    ServiceHealth Health() const noexcept override;

    // Pipeline control
    bool StartVoicePipeline() noexcept;
    bool StartTextPipeline(const String& text) noexcept;
    bool CancelPipeline() noexcept;

    // Provider management
    bool RegisterProvider(AIProvider* provider) noexcept;
    bool SetActiveProvider(AIProviderType type) noexcept;
    AIProvider* GetActiveProvider() noexcept;
    AIProvider* GetProvider(AIProviderType type) noexcept;

    // Queries
    PipelineStage GetCurrentStage() const noexcept;
    String GetCurrentStageName() const noexcept;
    const PipelineMetrics& GetMetrics() const noexcept;
    bool IsPipelineRunning() const noexcept;

    void HandleEvent(const String& eventType, const String& eventData) noexcept override;

    static constexpr const char* kStaticName = "AIPipeline";

private:
    void AdvanceStage(PipelineStage stage) noexcept;
    void FailPipeline(const String& reason) noexcept;
    void ResetPipeline() noexcept;

    void BeginAudioCapture() noexcept;
    void BeginSTT(const String& audioData) noexcept;
    void BeginAIProcessing(const String& text) noexcept;
    void BeginTTS(const String& responseText) noexcept;
    void BeginPlayback(const String& audioData) noexcept;

    void OnSTTComplete(const AIResponse& response) noexcept;
    void OnAIComplete(const AIResponse& response) noexcept;
    void OnTTSComplete(const AIResponse& response) noexcept;

    void EmitStageEvent(PipelineStage stage) noexcept;
    void UpdateMetrics(PipelineStage stage, unsigned long elapsedMs) noexcept;

    bool FindAndExecuteStageTask() noexcept;

    static constexpr const char* kLogCategory = "AIPipeline";

    PipelineStage m_currentStage;
    PipelineMetrics m_metrics;
    std::vector<AIProvider*> m_providers;
    AIProvider* m_activeProvider;

    String m_currentInput;
    String m_currentSTTResult;
    String m_currentAIResult;
    String m_currentTTSResult;

    unsigned long m_stageStartTime;
    unsigned long m_pipelineStartTime;
    bool m_pipelineRunning;
    bool m_suspended;
    size_t m_currentTaskId;

    // Stage timing
    unsigned long m_sttStartMs;
    unsigned long m_aiStartMs;
    unsigned long m_ttsStartMs;
};

extern AIPipeline aiPipeline;

#endif