#ifndef AURA_WORKSPACE_MANAGER_H
#define AURA_WORKSPACE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

struct WorkspaceMember {
    String entityType;          // "project", "goal", "memory", "timeline", "document", "conversation", "graph", "planner"
    String entityId;
    
    WorkspaceMember() noexcept {}
    WorkspaceMember(const String& type, const String& id) noexcept : entityType(type), entityId(id) {}
};

struct Workspace {
    String id;
    String name;
    String description;
    unsigned long createdAt;
    unsigned long updatedAt;
    bool active;
    std::vector<WorkspaceMember> members;
    
    Workspace() noexcept : createdAt(0), updatedAt(0), active(false) {}
};

class WorkspaceManager {
public:
    WorkspaceManager() noexcept;
    ~WorkspaceManager() noexcept;
    
    WorkspaceManager(const WorkspaceManager&) = delete;
    WorkspaceManager& operator=(const WorkspaceManager&) = delete;
    WorkspaceManager(WorkspaceManager&&) = delete;
    WorkspaceManager& operator=(WorkspaceManager&&) = delete;
    
    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;
    
    // CRUD
    [[nodiscard]] String createWorkspace(const String& name, const String& description = "") noexcept;
    [[nodiscard]] bool deleteWorkspace(const String& id) noexcept;
    [[nodiscard]] bool updateWorkspace(const String& id, const String& name, const String& description) noexcept;
    
    // Members
    [[nodiscard]] bool addMember(const String& workspaceId, const String& entityType, const String& entityId) noexcept;
    [[nodiscard]] bool removeMember(const String& workspaceId, const String& entityId) noexcept;
    [[nodiscard]] std::vector<WorkspaceMember> getMembers(const String& workspaceId) const noexcept;
    
    // Activation
    [[nodiscard]] bool activateWorkspace(const String& id) noexcept;
    [[nodiscard]] bool deactivateWorkspace(const String& id) noexcept;
    [[nodiscard]] const Workspace* getActiveWorkspace() const noexcept;
    
    // Query
    [[nodiscard]] const Workspace* getWorkspace(const String& id) const noexcept;
    [[nodiscard]] std::vector<Workspace> getAllWorkspaces() const noexcept;
    [[nodiscard]] std::vector<Workspace> findWorkspacesByEntity(const String& entityType, const String& entityId) const noexcept;
    [[nodiscard]] String getWorkspacesJson() const noexcept;
    
    // Persistence
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;
    
    [[nodiscard]] bool isInitialized() const noexcept;
    
private:
    static constexpr const char* kLogCategory = "WorkspaceMgr";
    static constexpr size_t kMaxWorkspaces = WORKSPACE_MAX_COUNT;
    static constexpr size_t kMaxMembers = WORKSPACE_MAX_MEMBERS;
    
    String generateId() noexcept;
    void trimToMax() noexcept;
    String serializeMember(const WorkspaceMember& m) const noexcept;
    String serializeWorkspace(const Workspace& w) const noexcept;
    WorkspaceMember deserializeMember(const String& json, int& pos) const noexcept;
    Workspace deserializeWorkspace(const String& json) const noexcept;
    
    bool m_initialized;
    bool m_dirty;
    std::vector<Workspace> m_workspaces;
    unsigned long m_lastIdCounter;
};

extern WorkspaceManager workspaceManager;

#endif
