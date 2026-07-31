#ifndef AURA_PREDICTION_MANAGER_H
#define AURA_PREDICTION_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

enum class PredictionType : uint8_t {
    GOAL_COMPLETION,
    MISSED_HABIT,
    LATE_REMINDER,
    STORAGE_EXHAUSTION,
    WIFI_FAILURE,
    LOW_BATTERY,
    LONG_RESPONSE_TIME,
    PRODUCTIVITY_TREND,
    CUSTOM
};

struct Prediction {
    String id;
    unsigned long timestamp;
    PredictionType type;
    String targetId;            // ID of the predicted entity
    String targetName;          // Human-readable name
    float probability;          // 0-1
    float confidence;           // 0-1
    unsigned long predictedTime;
    String reasoning;
    bool verified;
    
    Prediction() noexcept : timestamp(0), probability(0), confidence(0), predictedTime(0), verified(false) {}
};

class PredictionManager {
public:
    PredictionManager() noexcept;
    ~PredictionManager() noexcept;
    
    PredictionManager(const PredictionManager&) = delete;
    PredictionManager& operator=(const PredictionManager&) = delete;
    PredictionManager(PredictionManager&&) = delete;
    PredictionManager& operator=(PredictionManager&&) = delete;
    
    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;
    
    // Generate predictions
    void runAllPredictions() noexcept;
    [[nodiscard]] float predictGoalCompletion(const String& goalId, float currentProgress, unsigned long deadline) noexcept;
    [[nodiscard]] float predictMissedHabit(const String& habitId, float streak, float successRate) noexcept;
    [[nodiscard]] float predictLateReminder(const String& reminderId, unsigned long triggerTime) noexcept;
    [[nodiscard]] float predictStorageExhaustion(size_t totalBytes, size_t freeBytes, float dailyUsageBytes) noexcept;
    [[nodiscard]] float predictWiFiFailure(int rssi, int disconnects) noexcept;
    [[nodiscard]] float predictProductivityTrend(const std::vector<float>& history) noexcept;
    
    // Add/store prediction
    [[nodiscard]] String addPrediction(PredictionType type, const String& targetId,
                                        const String& targetName, float probability,
                                        float confidence = 0.5f, unsigned long predictedTime = 0,
                                        const String& reasoning = "") noexcept;
    
    // Query
    [[nodiscard]] std::vector<Prediction> getActivePredictions(float minProbability = 0.3f) const noexcept;
    [[nodiscard]] std::vector<Prediction> getByType(PredictionType type) const noexcept;
    [[nodiscard]] const Prediction* getPrediction(const String& id) const noexcept;
    [[nodiscard]] String getPredictionsJson(float minProbability = 0.0f) const noexcept;
    
    // Persistence
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;
    
    [[nodiscard]] bool isInitialized() const noexcept;
    
private:
    static constexpr const char* kLogCategory = "PredictionMgr";
    static constexpr size_t kMaxHistory = PREDICTION_MAX_HISTORY;
    static constexpr unsigned long kIntervalMs = PREDICTION_INTERVAL_MS;
    static constexpr float kDefaultConfidence = PREDICTION_DEFAULT_CONFIDENCE;
    
    String generateId() noexcept;
    void trimHistory() noexcept;
    const char* typeToString(PredictionType t) const noexcept;
    PredictionType stringToType(const String& s) const noexcept;
    String serializePrediction(const Prediction& p) const noexcept;
    Prediction deserializePrediction(const String& json) const noexcept;
    
    bool m_initialized;
    bool m_dirty;
    std::vector<Prediction> m_predictions;
    unsigned long m_lastIdCounter;
    unsigned long m_lastRunTime;
};

extern PredictionManager predictionManager;

#endif
