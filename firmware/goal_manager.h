#ifndef AURA_GOAL_MANAGER_H
#define AURA_GOAL_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

enum class GoalType : uint8_t { DAILY, WEEKLY, LONG_TERM };

struct Milestone {
    String name;
    bool completed;
    unsigned long completedAt;

    Milestone() noexcept : completed(false), completedAt(0) {}
};

struct GoalEntry {
    String id;
    String title;
    String description;
    GoalType type;
    uint8_t priority;
    unsigned long deadline;
    uint8_t progress;
    std::vector<Milestone> milestones;
    std::vector<String> linkedReminders;
    std::vector<String> linkedMemories;
    std::vector<String> linkedConversations;
    unsigned long createdAt;
    unsigned long completedAt;
    bool completed;

    GoalEntry() noexcept : type(GoalType::DAILY), priority(0), deadline(0), progress(0),
                           createdAt(0), completedAt(0), completed(false) {}
};

class GoalManager {
public:
    GoalManager() noexcept;
    ~GoalManager() noexcept;

    GoalManager(const GoalManager&) = delete;
    GoalManager& operator=(const GoalManager&) = delete;
    GoalManager(GoalManager&&) = delete;
    GoalManager& operator=(GoalManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    [[nodiscard]] String createGoal(const String& title, GoalType type,
                                      const String& description = "", uint8_t priority = 0,
                                      unsigned long deadline = 0) noexcept;
    [[nodiscard]] bool updateGoal(const String& id, const String& title, const String& description,
                                    uint8_t priority, uint8_t progress) noexcept;
    [[nodiscard]] bool deleteGoal(const String& id) noexcept;
    [[nodiscard]] bool completeGoal(const String& id) noexcept;
    [[nodiscard]] String addMilestone(const String& goalId, const String& name) noexcept;
    [[nodiscard]] bool completeMilestone(const String& goalId, const String& milestoneName) noexcept;

    [[nodiscard]] bool linkReminder(const String& goalId, const String& reminderId) noexcept;
    [[nodiscard]] bool linkMemory(const String& goalId, const String& memoryId) noexcept;
    [[nodiscard]] bool linkConversation(const String& goalId, const String& conversationId) noexcept;

    [[nodiscard]] GoalEntry getGoal(const String& id) const noexcept;
    [[nodiscard]] std::vector<GoalEntry> getActiveGoals() const noexcept;
    [[nodiscard]] std::vector<GoalEntry> getCompletedGoals() const noexcept;
    [[nodiscard]] std::vector<GoalEntry> getGoalsByType(GoalType type) const noexcept;
    [[nodiscard]] const std::vector<GoalEntry>& getAllGoals() const noexcept;
    [[nodiscard]] size_t goalCount() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;

private:
    static constexpr const char* kLogCategory = "GoalManager";
    static constexpr size_t kMaxGoals = GOAL_MAX_COUNT;
    static constexpr uint8_t kMaxMilestones = GOAL_MAX_MILESTONES;

    String generateId() noexcept;
    size_t findGoal(const String& id) const noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<GoalEntry> m_goals;
    unsigned long m_lastIdCounter;
};

extern GoalManager goalManager;

#endif
