#ifndef AURA_WORKFLOW_ENGINE_H
#define AURA_WORKFLOW_ENGINE_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "service.h"

struct WorkflowStep {
    String name;
    EventType triggerEvent;     // Event that triggers this step
    String sourceFilter;        // Empty = any source, or specific module name
    unsigned long delayMs;      // Delay before executing (for sequencing)
    bool required;              // If true, workflow fails when this step fails

    WorkflowStep() noexcept
        : triggerEvent(EventType::COUNT)
        , delayMs(0)
        , required(true) {}
};

struct Workflow {
    String id;
    String name;
    String description;
    std::vector<WorkflowStep> steps;
    bool enabled;
    bool singleInstance;    // Only one instance at a time

    Workflow() noexcept : enabled(true), singleInstance(true) {}
};

class WorkflowEngine : public Service {
public:
    WorkflowEngine() noexcept;
    ~WorkflowEngine() noexcept;

    bool Initialize() noexcept override;
    void Update() noexcept override;

    // Workflow lifecycle
    bool RegisterWorkflow(const Workflow& workflow) noexcept;
    bool UnregisterWorkflow(const String& id) noexcept;
    bool EnableWorkflow(const String& id) noexcept;
    bool DisableWorkflow(const String& id) noexcept;

    // Trigger a workflow manually
    bool TriggerWorkflow(const String& id, const String& data = "") noexcept;

    // Query
    Workflow GetWorkflow(const String& id) const noexcept;
    std::vector<Workflow> GetAllWorkflows() const noexcept;
    bool IsWorkflowActive(const String& id) const noexcept;
    size_t GetActiveCount() const noexcept;

    void HandleEvent(const String& eventType, const String& eventData) noexcept override;

    // Register built-in workflows
    void RegisterBuiltins() noexcept;

    static constexpr const char* kStaticName = "WorkflowEngine";

private:
    struct WorkflowInstance {
        Workflow workflow;
        String data;
        size_t currentStep;
        unsigned long stepStartTime;
        bool active;

        WorkflowInstance() noexcept : currentStep(0), stepStartTime(0), active(false) {}
    };

    int FindWorkflow(const String& id) const noexcept;
    void AdvanceWorkflow(WorkflowInstance& instance) noexcept;
    void CompleteWorkflow(WorkflowInstance& instance) noexcept;
    void FailWorkflow(WorkflowInstance& instance, const String& reason) noexcept;

    static constexpr const char* kLogCategory = "WorkflowEngine";
    static constexpr size_t kMaxWorkflows = 32;
    static constexpr size_t kMaxActiveInstances = 8;

    std::vector<Workflow> m_workflows;
    std::vector<WorkflowInstance> m_active;
};

extern WorkflowEngine workflowEngine;

#endif