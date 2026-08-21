#ifndef AURA_DECISION_MANAGER_H
#define AURA_DECISION_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

struct DecisionOption {
    String id;
    String name;
    String description;
    float priorityScore;       // 0-1
    float riskScore;           // 0-1 (higher = riskier)
    float deadlineUrgency;     // 0-1
    float confidenceScore;     // 0-1
    unsigned long timestamp;
    
    DecisionOption() noexcept : priorityScore(0), riskScore(0), deadlineUrgency(0), confidenceScore(0), timestamp(0) {}
};

struct DecisionRecord {
    String id;
    String question;
    unsigned long timestamp;
    std::vector<DecisionOption> options;
    String chosenOptionId;
    String reasoning;
    String alternatives;
    String confidence;
    bool explained;
    String decisionType;       // "study", "project", "time_management", "reminder_priority", "task_selection"
    float overallConfidence;
    bool resolved;
    
    DecisionRecord() noexcept : timestamp(0), overallConfidence(0), resolved(false), alternatives("[]"), confidence("medium"), explained(false) {}
};

class DecisionManager {
public:
    DecisionManager() noexcept;
    ~DecisionManager() noexcept;
    
    DecisionManager(const DecisionManager&) = delete;
    DecisionManager& operator=(const DecisionManager&) = delete;
    DecisionManager(DecisionManager&&) = delete;
    DecisionManager& operator=(DecisionManager&&) = delete;
    
    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;
    
    // Core decision API
    [[nodiscard]] String addOption(const String& name, const String& description,
                                    float priority = 0.5f, float risk = 0.5f,
                                    float deadlineUrgency = 0.0f) noexcept;
    
    [[nodiscard]] String makeDecision(const String& question, const std::vector<String>& optionIds,
                                       const String& decisionType = "general") noexcept;
    
    [[nodiscard]] float computeConfidence(const DecisionOption& option, const String& decisionType) const noexcept;
    [[nodiscard]] String explainDecision(const String& decisionId) const noexcept;
    [[nodiscard]] float assessRisk(const String& optionId) const noexcept;
    [[nodiscard]] std::vector<DecisionOption> rankByPriority(const std::vector<String>& optionIds) const noexcept;
    
    // Query
    [[nodiscard]] const DecisionRecord* getDecision(const String& id) const noexcept;
    [[nodiscard]] std::vector<DecisionRecord> getDecisionsByType(const String& decisionType) const noexcept;
    [[nodiscard]] std::vector<DecisionRecord> getDecisionsByConfidence(const String& confidenceLevel) const noexcept;
    [[nodiscard]] std::vector<DecisionRecord> getRecentDecisions(size_t count = 10) const noexcept;
    [[nodiscard]] String getDecisionsJson(const String& type = "") const noexcept;
    
    // Persistence
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;
    
    [[nodiscard]] bool isInitialized() const noexcept;
    
private:
    static constexpr const char* kLogCategory = "DecisionMgr";
    static constexpr size_t kMaxHistory = DECISION_MAX_HISTORY;
    
    String generateId() noexcept;
    void trimHistory() noexcept;
    String serializeOption(const DecisionOption& opt) const noexcept;
    String serializeRecord(const DecisionRecord& rec) const noexcept;
    DecisionOption deserializeOption(const String& json, int& pos) const noexcept;
    DecisionRecord deserializeRecord(const String& json) const noexcept;
    
    bool m_initialized;
    bool m_dirty;
    std::vector<DecisionOption> m_pendingOptions;
    std::vector<DecisionRecord> m_history;
    unsigned long m_lastIdCounter;
};

extern DecisionManager decisionManager;

#endif
