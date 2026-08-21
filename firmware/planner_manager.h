#ifndef AURA_PLANNER_MANAGER_H
#define AURA_PLANNER_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

struct PlannedTask {
    String id;
    String goalId;
    String title;
    String description;
    uint8_t priority;
    unsigned long estimatedDurationMs;
    unsigned long scheduledTime;
    unsigned long deadline;
    bool completed;

    PlannedTask() noexcept : priority(0), estimatedDurationMs(0), scheduledTime(0),
                             deadline(0), completed(false) {}
};

class PlannerManager {
public:
    PlannerManager() noexcept;
    ~PlannerManager() noexcept;

    PlannerManager(const PlannerManager&) = delete;
    PlannerManager& operator=(const PlannerManager&) = delete;
    PlannerManager(PlannerManager&&) = delete;
    PlannerManager& operator=(PlannerManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    [[nodiscard]] String addTask(const String& goalId, const String& title,
                                   const String& description = "", uint8_t priority = 0,
                                   unsigned long durationMs = 0, unsigned long deadline = 0) noexcept;
    [[nodiscard]] bool removeTask(const String& taskId) noexcept;
    [[nodiscard]] bool completeTask(const String& taskId) noexcept;
    [[nodiscard]] bool rescheduleTask(const String& taskId, unsigned long newTime) noexcept;

    [[nodiscard]] std::vector<PlannedTask> getTasksForGoal(const String& goalId) const noexcept;
    [[nodiscard]] std::vector<PlannedTask> getTodaysTasks() const noexcept;
    [[nodiscard]] std::vector<PlannedTask> getUpcomingTasks() const noexcept;
    [[nodiscard]] const std::vector<PlannedTask>& getAllTasks() const noexcept;

    [[nodiscard]] String suggestNextAction() const noexcept;
    [[nodiscard]] size_t taskCount() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;

private:
    static constexpr const char* kLogCategory = "PlannerManager";
    static constexpr size_t kMaxTasks = PLANNER_MAX_TASKS;

    String generateId() noexcept;
    size_t findTask(const String& id) const noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<PlannedTask> m_tasks;
    unsigned long m_lastIdCounter;
};

extern PlannerManager plannerManager;

#endif
