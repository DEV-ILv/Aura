#include "ai_pipeline.h"

AIPipeline aiPipeline;

AIPipeline::AIPipeline() noexcept
    : Service(kStaticName, BootPriority::NORMAL)
    , m_currentStage(PipelineStage::IDLE)
    , m_activeProvider(nullptr)
    , m_stageStartTime(0)
    , m_pipelineStartTime(0)
    , m_pipelineRunning(false)
    , m_suspended(false)
    , m_currentTaskId(0)
    , m_sttStartMs(0)
    , m_aiStartMs(0)
    , m_ttsStartMs(0) {
    m_providers.reserve(4);
}

AIPipeline::~AIPipeline() noexcept = default;

bool AIPipeline::Initialize() noexcept {
    if (GetState() != ServiceState::UNINITIALIZED) return true;
    SetState(ServiceState::INITIALIZING);

    // Subscribe to relevant events
    if (eventBus.isInitialized()) {
        // Future: wire up EventBus handlers for pipeline stages
    }

    SetState(ServiceState::INITIALIZED);
    LOG_INFO(kLogCategory, "AIPipeline initialized");
    return true;
}

void AIPipeline::Update() noexcept {
    if (m_suspended || !m_pipelineRunning) return;
    // Stage transitions are event-driven, not polled
}

bool AIPipeline::Stop() noexcept {
    CancelPipeline();
    SetState(ServiceState::STOPPED);
    return true;
}

bool AIPipeline::Suspend() noexcept {
    m_suspended = true;
    return true;
}

bool AIPipeline::Resume() noexcept {
    m_suspended = false;
    return true;
}

ServiceHealth AIPipeline::Health() const noexcept {
    if (!m_activeProvider) return ServiceHealth::DEGRADED;
    return m_activeProvider->IsAvailable() ? ServiceHealth::HEALTHY : ServiceHealth::DEGRADED;
}

bool AIPipeline::StartVoicePipeline() noexcept {
    if (m_pipelineRunning) return false;

    ResetPipeline();
    m_pipelineRunning = true;
    m_pipelineStartTime = millis();

    m_metrics.totalPipelines++;

    BeginAudioCapture();
    return true;
}

bool AIPipeline::StartTextPipeline(const String& text) noexcept {
    if (m_pipelineRunning) return false;

    ResetPipeline();
    m_currentInput = text;
    m_pipelineRunning = true;
    m_pipelineStartTime = millis();

    m_metrics.totalPipelines++;

    // Schedule AI processing directly (skip audio capture + STT)
    m_currentTaskId = taskScheduler.Schedule(
        [this]() -> bool {
            m_aiStartMs = millis();
            AdvanceStage(PipelineStage::AI_PROCESSING);
            BeginAIProcessing(m_currentInput);
            return true;
        },
        "ai_pipeline_text",
        TaskPriority::INTERACTIVE,
        TaskCategory::ONE_SHOT,
        0, 10000
    );

    return true;
}

bool AIPipeline::CancelPipeline() noexcept {
    if (!m_pipelineRunning) return true;

    if (m_activeProvider) m_activeProvider->Cancel();
    taskScheduler.Cancel(m_currentTaskId);

    m_pipelineRunning = false;
    m_currentStage = PipelineStage::IDLE;

    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::CONVERSATION_ENDED, "AIPipeline", "cancelled");
    }

    LOG_INFO(kLogCategory, "Pipeline cancelled");
    return true;
}

bool AIPipeline::RegisterProvider(AIProvider* provider) noexcept {
    if (!provider) return false;

    // Check for duplicate
    for (auto* p : m_providers) {
        if (p->GetProviderType() == provider->GetProviderType()) return false;
    }

    m_providers.push_back(provider);

    // Auto-select first registered provider
    if (!m_activeProvider) {
        m_activeProvider = provider;
    }

    LOG_INFO(kLogCategory, "Registered AI provider: %s", provider->GetName().c_str());
    return true;
}

bool AIPipeline::SetActiveProvider(AIProviderType type) noexcept {
    for (auto* p : m_providers) {
        if (p->GetProviderType() == type) {
            m_activeProvider = p;
            LOG_INFO(kLogCategory, "Active provider set to: %s", p->GetName().c_str());
            return true;
        }
    }
    return false;
}

AIProvider* AIPipeline::GetActiveProvider() noexcept {
    return m_activeProvider;
}

AIProvider* AIPipeline::GetProvider(AIProviderType type) noexcept {
    for (auto* p : m_providers) {
        if (p->GetProviderType() == type) return p;
    }
    return nullptr;
}

PipelineStage AIPipeline::GetCurrentStage() const noexcept {
    return m_currentStage;
}

String AIPipeline::GetCurrentStageName() const noexcept {
    switch (m_currentStage) {
        case PipelineStage::IDLE:             return "Idle";
        case PipelineStage::WAITING_FOR_INPUT: return "Waiting for input";
        case PipelineStage::AUDIO_CAPTURE:    return "Audio capture";
        case PipelineStage::SPEECH_TO_TEXT:   return "Speech to text";
        case PipelineStage::AI_PROCESSING:    return "AI processing";
        case PipelineStage::TEXT_TO_SPEECH:   return "Text to speech";
        case PipelineStage::PLAYBACK:         return "Playback";
        case PipelineStage::COMPLETED:        return "Completed";
        case PipelineStage::ERROR:            return "Error";
        default:                              return "Unknown";
    }
}

const PipelineMetrics& AIPipeline::GetMetrics() const noexcept {
    return m_metrics;
}

bool AIPipeline::IsPipelineRunning() const noexcept {
    return m_pipelineRunning;
}

void AIPipeline::HandleEvent(const String& eventType, const String& eventData) noexcept {
    Service::HandleEvent(eventType, eventData);
}

void AIPipeline::AdvanceStage(PipelineStage stage) noexcept {
    m_currentStage = stage;
    m_stageStartTime = millis();
    EmitStageEvent(stage);
}

void AIPipeline::FailPipeline(const String& reason) noexcept {
    LOG_ERROR(kLogCategory, "Pipeline failed: %s", reason.c_str());
    m_metrics.failedPipelines++;
    m_pipelineRunning = false;
    m_currentStage = PipelineStage::ERROR;

    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::CONVERSATION_ENDED, "AIPipeline",
                         "{\"error\":\"" + reason + "\"}");
    }
}

void AIPipeline::ResetPipeline() noexcept {
    m_currentInput = "";
    m_currentSTTResult = "";
    m_currentAIResult = "";
    m_currentTTSResult = "";
    m_currentStage = PipelineStage::IDLE;
    m_stageStartTime = 0;
    m_sttStartMs = 0;
    m_aiStartMs = 0;
    m_ttsStartMs = 0;
}

void AIPipeline::BeginAudioCapture() noexcept {
    AdvanceStage(PipelineStage::AUDIO_CAPTURE);
    // Voice pipeline not yet implemented in this phase
    // Delegates to existing audioManager/speechToText via EventBus
    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::CONVERSATION_STARTED, "AIPipeline", "voice");
    }
    // For now, immediately transition to AI processing with a placeholder
    // This matches existing behavior - STT runs separately
    AdvanceStage(PipelineStage::AI_PROCESSING);
}

void AIPipeline::BeginSTT(const String& audioData) noexcept {
    (void)audioData;
    AdvanceStage(PipelineStage::SPEECH_TO_TEXT);
    m_sttStartMs = millis();
    // Delegates to speechToText service
}

void AIPipeline::BeginAIProcessing(const String& text) noexcept {
    if (!m_activeProvider || !m_activeProvider->IsAvailable()) {
        FailPipeline("No AI provider available");
        return;
    }

    AdvanceStage(PipelineStage::AI_PROCESSING);
    m_aiStartMs = millis();

    AIRequest request;
    request.prompt = text;
    request.temperature = 0.7f;
    request.maxTokens = 2048;

    m_activeProvider->SendRequest(request, [this](const AIResponse& response) {
        OnAIComplete(response);
    });
}

void AIPipeline::BeginTTS(const String& responseText) noexcept {
    AdvanceStage(PipelineStage::TEXT_TO_SPEECH);
    m_ttsStartMs = millis();
    // Delegates to textToSpeech service via EventBus
    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::MESSAGE_SENT, "AIPipeline", responseText);
    }
    OnTTSComplete(AIResponse()); // Mark complete
}

void AIPipeline::BeginPlayback(const String& audioData) noexcept {
    (void)audioData;
    AdvanceStage(PipelineStage::PLAYBACK);
    // Delegates to existing audioManager
}

void AIPipeline::OnSTTComplete(const AIResponse& response) noexcept {
    m_currentSTTResult = response.text;
    UpdateMetrics(PipelineStage::SPEECH_TO_TEXT, millis() - m_sttStartMs);

    if (!response.success) {
        FailPipeline("STT failed");
        return;
    }

    BeginAIProcessing(m_currentSTTResult);
}

void AIPipeline::OnAIComplete(const AIResponse& response) noexcept {
    m_currentAIResult = response.text;
    UpdateMetrics(PipelineStage::AI_PROCESSING, millis() - m_aiStartMs);

    if (!response.success) {
        FailPipeline("AI processing failed");
        return;
    }

    BeginTTS(m_currentAIResult);
}

void AIPipeline::OnTTSComplete(const AIResponse& response) noexcept {
    m_currentTTSResult = response.text;
    UpdateMetrics(PipelineStage::TEXT_TO_SPEECH, millis() - m_ttsStartMs);

    m_pipelineRunning = false;
    AdvanceStage(PipelineStage::COMPLETED);

    unsigned long totalTime = millis() - m_pipelineStartTime;
    LOG_INFO(kLogCategory, "Pipeline completed in %lu ms", totalTime);

    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::CONVERSATION_ENDED, "AIPipeline",
                         "{\"totalMs\":" + String(totalTime) + "}");
    }
}

void AIPipeline::EmitStageEvent(PipelineStage stage) noexcept {
    if (!eventBus.isInitialized()) return;

    switch (stage) {
        case PipelineStage::AUDIO_CAPTURE:
            eventBus.publish(EventType::CONVERSATION_STARTED, "AIPipeline", "listening");
            break;
        case PipelineStage::SPEECH_TO_TEXT:
            break;
        case PipelineStage::AI_PROCESSING:
            break;
        case PipelineStage::TEXT_TO_SPEECH:
            break;
        case PipelineStage::COMPLETED:
            eventBus.publish(EventType::CONVERSATION_ENDED, "AIPipeline", "completed");
            break;
        default:
            break;
    }
}

void AIPipeline::UpdateMetrics(PipelineStage stage, unsigned long elapsedMs) noexcept {
    switch (stage) {
        case PipelineStage::SPEECH_TO_TEXT:
            m_metrics.avgSttMs = (m_metrics.avgSttMs * (m_metrics.totalPipelines - 1) + elapsedMs) / m_metrics.totalPipelines;
            break;
        case PipelineStage::AI_PROCESSING:
            m_metrics.avgAiMs = (m_metrics.avgAiMs * (m_metrics.totalPipelines - 1) + elapsedMs) / m_metrics.totalPipelines;
            break;
        case PipelineStage::TEXT_TO_SPEECH:
            m_metrics.avgTtsMs = (m_metrics.avgTtsMs * (m_metrics.totalPipelines - 1) + elapsedMs) / m_metrics.totalPipelines;
            break;
        default:
            break;
    }
}