#ifndef AURA_TASK_SCHEDULER_H
#define AURA_TASK_SCHEDULER_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "service.h"

enum class TaskPriority : uint8_t {
    CRITICAL        = 0,  // Must run now (watchdog, safety)
    INTERACTIVE     = 1,  // User-facing (conversation, display)
    PRIORITY_HIGH   = 2,  // Important background (memory save)
    NORMAL          = 3,  // Standard background (analytics)
    PRIORITY_LOW    = 4,  // Deferrable (search indexing)
    IDLE            = 5,  // Only when nothing else to do
    MAINTENANCE     = 6   // Cleanup, compaction (runs rarely)
};

enum class TaskState : uint8_t {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED,
    DEFERRED
};

enum class TaskCategory : uint8_t {
    ONE_SHOT,       // Run once
    INTERVAL,       // Run every interval
    DAILY,          // Run once per day
    HOURLY,         // Run once per hour
    DEFERRED,       // Run when resources permit
    TRIGGERED,      // Run when triggered by event
    MAINTENANCE     // Run during maintenance windows
};

struct TaskHandle {
    size_t id;
    TaskState state;
    TaskPriority priority;
    TaskCategory category;
    String name;
    unsigned long lastRun;
    unsigned long intervalMs;
    unsigned long maxExecutionMs;
    unsigned long totalRuns;
    unsigned long totalFailures;
    float avgExecutionMs;

    TaskHandle() noexcept
        : id(0), state(TaskState::PENDING), priority(TaskPriority::NORMAL),
          category(TaskCategory::ONE_SHOT), lastRun(0), intervalMs(0),
          maxExecutionMs(500), totalRuns(0), totalFailures(0), avgExecutionMs(0) {}
};

using TaskFunction = std::function<bool()>;

class TaskScheduler : public Service {
public:
    TaskScheduler() noexcept;
    ~TaskScheduler() noexcept;

    bool Initialize() noexcept override;
    void Update() noexcept override;
    bool Stop() noexcept override;
    bool Suspend() noexcept override;
    bool Resume() noexcept override;

    // Schedule a task
    size_t Schedule(TaskFunction func, const String& name,
                    TaskPriority priority = TaskPriority::NORMAL,
                    TaskCategory category = TaskCategory::ONE_SHOT,
                    unsigned long intervalMs = 0,
                    unsigned long maxExecutionMs = 500) noexcept;

    // Cancel
    bool Cancel(size_t taskId) noexcept;
    void CancelAll() noexcept;

    // Query
    TaskHandle GetInfo(size_t taskId) const noexcept;
    std::vector<TaskHandle> GetPending() const noexcept;
    std::vector<TaskHandle> GetCompleted() const noexcept;
    size_t PendingCount() const noexcept;
    size_t CompletedCount() const noexcept;
    bool IsBusy() const noexcept;

    // Stats
    unsigned long GetTotalTasksRun() const noexcept;
    float GetAverageQueueTimeMs() const noexcept;
    String GetStatsJSON() const noexcept;

    void HandleEvent(const String& eventType, const String& eventData) noexcept override;

    static constexpr const char* kStaticName = "TaskScheduler";

private:
    struct Task {
        TaskFunction func;
        TaskHandle handle;
    };

    int FindTask(size_t id) const noexcept;
    size_t GetNextId() noexcept;
    void ExecuteTask(Task& task) noexcept;
    bool ShouldRun(const Task& task) const noexcept;
    void CleanupCompleted() noexcept;

    static constexpr const char* kLogCategory = "TaskScheduler";
    static constexpr size_t kMaxTasks = 64;
    static constexpr size_t kMaxCompletedLog = 20;
    static constexpr unsigned long kIdleCheckMs = 50;

    std::vector<Task> m_tasks;
    std::vector<TaskHandle> m_completedLog;
    size_t m_nextId;
    unsigned long m_lastUpdate;
    unsigned long m_totalTasksRun;
    unsigned long m_totalQueueTimeMs;
    bool m_suspended;
};

extern TaskScheduler taskScheduler;

#endif