#include "workflow_engine.h"
#include "timeline_manager.h"
#include "analytics_manager.h"
#include "reflection_manager.h"
#include "display_manager.h"
#include "knowledge_graph_manager.h"
#include "memory_manager.h"
#include "executive_assistant.h"

WorkflowEngine workflowEngine;

WorkflowEngine::WorkflowEngine() noexcept
    : Service(kStaticName, BootPriority::PRIORITY_LOW) {
}

WorkflowEngine::~WorkflowEngine() noexcept = default;

bool WorkflowEngine::Initialize() noexcept {
    if (GetState() != ServiceState::UNINITIALIZED) return true;
    SetState(ServiceState::INITIALIZING);
    RegisterBuiltins();
    SetState(ServiceState::INITIALIZED);
    LOG_INFO(kLogCategory, "WorkflowEngine initialized (%zu workflows)", m_workflows.size());
    return true;
}

void WorkflowEngine::Update() noexcept {
    for (auto it = m_active.begin(); it != m_active.end();) {
        if (!it->active) {
            it = m_active.erase(it);
            continue;
        }

        // Check if it's time for the next step
        if (it->stepStartTime > 0 && millis() - it->stepStartTime >=
            it->workflow.steps[it->currentStep].delayMs) {
            AdvanceWorkflow(*it);
        }

        ++it;
    }
}

bool WorkflowEngine::RegisterWorkflow(const Workflow& workflow) noexcept {
    if (m_workflows.size() >= kMaxWorkflows) return false;
    if (FindWorkflow(workflow.id) >= 0) return false;
    m_workflows.push_back(workflow);
    LOG_DEBUG(kLogCategory, "Registered workflow: %s (%zu steps)",
              workflow.id.c_str(), workflow.steps.size());
    return true;
}

bool WorkflowEngine::UnregisterWorkflow(const String& id) noexcept {
    int idx = FindWorkflow(id);
    if (idx < 0) return false;
    m_workflows.erase(m_workflows.begin() + idx);
    return true;
}

bool WorkflowEngine::EnableWorkflow(const String& id) noexcept {
    int idx = FindWorkflow(id);
    if (idx < 0) return false;
    m_workflows[idx].enabled = true;
    return true;
}

bool WorkflowEngine::DisableWorkflow(const String& id) noexcept {
    int idx = FindWorkflow(id);
    if (idx < 0) return false;
    m_workflows[idx].enabled = false;
    return true;
}

bool WorkflowEngine::TriggerWorkflow(const String& id, const String& data) noexcept {
    int idx = FindWorkflow(id);
    if (idx < 0 || !m_workflows[idx].enabled) return false;

    if (m_workflows[idx].singleInstance) {
        for (const auto& inst : m_active) {
            if (inst.workflow.id == id) return false; // Already running
        }
    }

    if (m_active.size() >= kMaxActiveInstances) return false;

    WorkflowInstance instance;
    instance.workflow = m_workflows[idx];
    instance.data = data;
    instance.currentStep = 0;
    instance.stepStartTime = millis();
    instance.active = true;
    m_active.push_back(instance);

    LOG_INFO(kLogCategory, "Workflow triggered: %s", id.c_str());
    return true;
}

Workflow WorkflowEngine::GetWorkflow(const String& id) const noexcept {
    int idx = FindWorkflow(id);
    if (idx < 0) return Workflow();
    return m_workflows[idx];
}

std::vector<Workflow> WorkflowEngine::GetAllWorkflows() const noexcept {
    return m_workflows;
}

bool WorkflowEngine::IsWorkflowActive(const String& id) const noexcept {
    for (const auto& inst : m_active) {
        if (inst.workflow.id == id && inst.active) return true;
    }
    return false;
}

size_t WorkflowEngine::GetActiveCount() const noexcept {
    size_t count = 0;
    for (const auto& inst : m_active) {
        if (inst.active) count++;
    }
    return count;
}

void WorkflowEngine::HandleEvent(const String& eventType, const String& eventData) noexcept {
    Service::HandleEvent(eventType, eventData);

    // Check if any workflow step is triggered by this event
    for (auto& instance : m_active) {
        if (!instance.active) continue;
        if (instance.currentStep >= instance.workflow.steps.size()) continue;

        auto& step = instance.workflow.steps[instance.currentStep];

        // Convert string eventType to EventType enum for matching
        // Workflow steps use EventType directly, but HandleEvent receives strings
        // This is a simplified check; production would map string -> EventType
        (void)eventType;
        (void)eventData;
    }
}

void WorkflowEngine::RegisterBuiltins() noexcept {
    // Study session completion workflow
    {
        Workflow w;
        w.id = "study_completion";
        w.name = "Study Session Completion";
        w.description = "Updates timeline, analytics, reflection, dashboard, knowledge, and executive assistant when a study session completes";

        WorkflowStep step1;
        step1.name = "Record Timeline Entry";
        step1.triggerEvent = EventType::STUDY_SESSION_COMPLETED;
        step1.required = false;
        w.steps.push_back(step1);

        WorkflowStep step2;
        step2.name = "Update Analytics";
        step2.triggerEvent = EventType::STUDY_SESSION_COMPLETED;
        step2.delayMs = 100;
        step2.required = false;
        w.steps.push_back(step2);

        WorkflowStep step3;
        step3.name = "Knowledge Integration";
        step3.triggerEvent = EventType::STUDY_SESSION_COMPLETED;
        step3.delayMs = 200;
        step3.required = false;
        w.steps.push_back(step3);

        WorkflowStep step4;
        step4.name = "Executive Review";
        step4.triggerEvent = EventType::STUDY_SESSION_COMPLETED;
        step4.delayMs = 300;
        step4.required = false;
        w.steps.push_back(step4);

        RegisterWorkflow(w);
    }

    // Morning routine workflow
    {
        Workflow w;
        w.id = "morning_routine";
        w.name = "Morning Routine";
        w.description = "Daily briefing, timeline check, and suggestion generation";

        WorkflowStep s;
        s.name = "Briefing Generation";
        s.triggerEvent = EventType::SYSTEM_STARTUP;
        s.required = false;
        w.steps.push_back(s);

        RegisterWorkflow(w);
    }
}

int WorkflowEngine::FindWorkflow(const String& id) const noexcept {
    for (size_t i = 0; i < m_workflows.size(); ++i) {
        if (m_workflows[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

void WorkflowEngine::AdvanceWorkflow(WorkflowInstance& instance) noexcept {
    if (instance.currentStep >= instance.workflow.steps.size()) {
        CompleteWorkflow(instance);
        return;
    }

    auto& step = instance.workflow.steps[instance.currentStep];

    // Execute the step
    LOG_DEBUG(kLogCategory, "Workflow %s: executing step %s",
              instance.workflow.id.c_str(), step.name.c_str());

    // The actual execution is driven by EventBus handlers registered by target managers
    if (eventBus.isInitialized()) {
        eventBus.publish(step.triggerEvent, "WorkflowEngine",
                         "{\"workflow\":\"" + instance.workflow.id +
                         "\",\"step\":\"" + step.name + "\",\"data\":" + instance.data + "}");
    }

    instance.currentStep++;
    instance.stepStartTime = millis();

    if (instance.currentStep >= instance.workflow.steps.size()) {
        CompleteWorkflow(instance);
    }
}

void WorkflowEngine::CompleteWorkflow(WorkflowInstance& instance) noexcept {
    instance.active = false;
    LOG_INFO(kLogCategory, "Workflow completed: %s", instance.workflow.id.c_str());

    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::AUTOMATION_COMPLETED, "WorkflowEngine",
                         "{\"workflow\":\"" + instance.workflow.id + "\"}");
    }
}

void WorkflowEngine::FailWorkflow(WorkflowInstance& instance, const String& reason) noexcept {
    instance.active = false;
    LOG_WARNING(kLogCategory, "Workflow failed: %s (%s)",
                instance.workflow.id.c_str(), reason.c_str());

    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::AUTOMATION_COMPLETED, "WorkflowEngine",
                         "{\"workflow\":\"" + instance.workflow.id + "\",\"error\":\"" + reason + "\"}");
    }
}