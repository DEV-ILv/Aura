#include "task_scheduler.h"
#include <algorithm>

TaskScheduler taskScheduler;

TaskScheduler::TaskScheduler() noexcept
    : Service(kStaticName, BootPriority::CRITICAL)
    , m_nextId(1)
    , m_lastUpdate(0)
    , m_totalTasksRun(0)
    , m_totalQueueTimeMs(0)
    , m_suspended(false) {
}

TaskScheduler::~TaskScheduler() noexcept = default;

bool TaskScheduler::Initialize() noexcept {
    if (GetState() != ServiceState::UNINITIALIZED) return true;
    SetState(ServiceState::INITIALIZING);
    SetState(ServiceState::INITIALIZED);
    LOG_INFO(kLogCategory, "TaskScheduler initialized");
    return true;
}

void TaskScheduler::Update() noexcept {
    if (m_suspended || GetState() == ServiceState::SUSPENDED) return;

    unsigned long now = millis();

    // Sort by priority so higher-priority tasks run first
    std::sort(m_tasks.begin(), m_tasks.end(),
        [](const Task& a, const Task& b) {
            return static_cast<uint8_t>(a.handle.priority) <
                   static_cast<uint8_t>(b.handle.priority);
        });

    for (auto& task : m_tasks) {
        if (task.handle.state == TaskState::CANCELLED ||
            task.handle.state == TaskState::COMPLETED) continue;

        if (ShouldRun(task)) {
            task.handle.state = TaskState::RUNNING;
            unsigned long start = micros();
            ExecuteTask(task);
            unsigned long elapsed = micros() - start;

            task.handle.totalRuns++;
            task.handle.lastRun = now;
            task.handle.avgExecutionMs = (task.handle.avgExecutionMs * (task.handle.totalRuns - 1) + elapsed / 1000.0f) / task.handle.totalRuns;
            m_totalTasksRun++;
        }
    }

    CleanupCompleted();
}

bool TaskScheduler::Stop() noexcept {
    CancelAll();
    SetState(ServiceState::STOPPED);
    return true;
}

bool TaskScheduler::Suspend() noexcept {
    m_suspended = true;
    return true;
}

bool TaskScheduler::Resume() noexcept {
    m_suspended = false;
    return true;
}

size_t TaskScheduler::Schedule(TaskFunction func, const String& name,
                                TaskPriority priority, TaskCategory category,
                                unsigned long intervalMs, unsigned long maxExecutionMs) noexcept {
    if (m_tasks.size() >= kMaxTasks || !func) return 0;

    Task task;
    task.func = std::move(func);
    task.handle.id = GetNextId();
    task.handle.name = name;
    task.handle.priority = priority;
    task.handle.category = category;
    task.handle.intervalMs = intervalMs;
    task.handle.maxExecutionMs = maxExecutionMs;
    task.handle.state = TaskState::PENDING;
    task.handle.lastRun = 0;

    m_tasks.push_back(std::move(task));
    LOG_DEBUG(kLogCategory, "Scheduled task #%zu: %s (priority=%d, interval=%lu)",
              task.handle.id, name.c_str(), static_cast<int>(priority), intervalMs);
    return task.handle.id;
}

bool TaskScheduler::Cancel(size_t taskId) noexcept {
    int idx = FindTask(taskId);
    if (idx < 0) return false;
    m_tasks[idx].handle.state = TaskState::CANCELLED;
    return true;
}

void TaskScheduler::CancelAll() noexcept {
    for (auto& t : m_tasks) {
        t.handle.state = TaskState::CANCELLED;
    }
}

TaskHandle TaskScheduler::GetInfo(size_t taskId) const noexcept {
    for (const auto& t : m_tasks) {
        if (t.handle.id == taskId) return t.handle;
    }
    return TaskHandle();
}

std::vector<TaskHandle> TaskScheduler::GetPending() const noexcept {
    std::vector<TaskHandle> result;
    for (const auto& t : m_tasks) {
        if (t.handle.state == TaskState::PENDING ||
            t.handle.state == TaskState::RUNNING ||
            t.handle.category == TaskCategory::INTERVAL) {
            result.push_back(t.handle);
        }
    }
    return result;
}

std::vector<TaskHandle> TaskScheduler::GetCompleted() const noexcept {
    return m_completedLog;
}

size_t TaskScheduler::PendingCount() const noexcept {
    size_t count = 0;
    for (const auto& t : m_tasks) {
        if (t.handle.state == TaskState::PENDING ||
            t.handle.state == TaskState::RUNNING) count++;
    }
    return count;
}

size_t TaskScheduler::CompletedCount() const noexcept {
    return m_completedLog.size();
}

bool TaskScheduler::IsBusy() const noexcept {
    return PendingCount() > 0;
}

unsigned long TaskScheduler::GetTotalTasksRun() const noexcept {
    return m_totalTasksRun;
}

float TaskScheduler::GetAverageQueueTimeMs() const noexcept {
    if (m_totalTasksRun == 0) return 0;
    return static_cast<float>(m_totalQueueTimeMs) / static_cast<float>(m_totalTasksRun);
}

String TaskScheduler::GetStatsJSON() const noexcept {
    String json = "{\"totalRun\":" + String(m_totalTasksRun);
    json += ",\"pending\":" + String(PendingCount());
    json += ",\"completed\":" + String(m_completedLog.size());
    json += ",\"avgQueueMs\":" + String(GetAverageQueueTimeMs());
    json += "}";
    return json;
}

void TaskScheduler::HandleEvent(const String& eventType, const String& eventData) noexcept {
    Service::HandleEvent(eventType, eventData);
}

int TaskScheduler::FindTask(size_t id) const noexcept {
    for (size_t i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].handle.id == id) return static_cast<int>(i);
    }
    return -1;
}

size_t TaskScheduler::GetNextId() noexcept {
    return m_nextId++;
}

void TaskScheduler::ExecuteTask(Task& task) noexcept {
    unsigned long startMs = millis();
    bool success = false;

    try {
        success = task.func();
    } catch (...) {
        success = false;
    }

    unsigned long elapsed = millis() - startMs;

    if (!success) {
        task.handle.totalFailures++;
        task.handle.state = TaskState::FAILED;
        LOG_WARNING(kLogCategory, "Task #%zu (%s) failed after %lu ms",
                    task.handle.id, task.handle.name.c_str(), elapsed);
    } else {
        task.handle.state = TaskState::COMPLETED;
    }

    // Update completed log
    if (m_completedLog.size() >= kMaxCompletedLog) {
        m_completedLog.erase(m_completedLog.begin());
    }
    m_completedLog.push_back(task.handle);

    // Re-schedule interval tasks
    if (task.handle.category == TaskCategory::INTERVAL && success) {
        task.handle.state = TaskState::PENDING;
    }
}

bool TaskScheduler::ShouldRun(const Task& task) const noexcept {
    unsigned long now = millis();

    switch (task.handle.category) {
        case TaskCategory::ONE_SHOT:
            return task.handle.state == TaskState::PENDING;

        case TaskCategory::INTERVAL:
            return (now - task.handle.lastRun >= task.handle.intervalMs);

        case TaskCategory::DEFERRED:
            return task.handle.state == TaskState::PENDING &&
                   PendingCount() <= 2; // Only run deferred when quiet

        case TaskCategory::MAINTENANCE:
            return task.handle.state == TaskState::PENDING &&
                   PendingCount() == 0; // Only in total idle

        case TaskCategory::TRIGGERED:
            return false; // Triggered externally via HandleEvent

        default:
            return false;
    }
}

void TaskScheduler::CleanupCompleted() noexcept {
    m_tasks.erase(
        std::remove_if(m_tasks.begin(), m_tasks.end(),
            [](const Task& t) {
                return t.handle.state == TaskState::COMPLETED &&
                       t.handle.category == TaskCategory::ONE_SHOT;
            }),
        m_tasks.end());
}