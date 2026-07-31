#ifndef AURA_KNOWLEDGE_GRAPH_MANAGER_H
#define AURA_KNOWLEDGE_GRAPH_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

enum class NodeType : uint8_t {
    USER, PROJECT, DEVICE, PERSON, SUBJECT, GOAL, PREFERENCE, SKILL, CUSTOM
};

struct GraphNode {
    String id;
    NodeType type;
    String name;
    String value;
    String tags;
    unsigned long createdAt;
    unsigned long updatedAt;
    uint8_t importance;

    GraphNode() noexcept : type(NodeType::CUSTOM), createdAt(0), updatedAt(0), importance(0) {}
};

struct GraphEdge {
    String id;
    String sourceId;
    String targetId;
    String relationship;
    float strength;
    float confidence;
    bool bidirectional;
    unsigned long createdAt;
    unsigned long updatedAt;

    GraphEdge() noexcept : strength(1.0f), confidence(1.0f), bidirectional(false), createdAt(0), updatedAt(0) {}
};

class KnowledgeGraphManager {
public:
    KnowledgeGraphManager() noexcept;
    ~KnowledgeGraphManager() noexcept;

    KnowledgeGraphManager(const KnowledgeGraphManager&) = delete;
    KnowledgeGraphManager& operator=(const KnowledgeGraphManager&) = delete;
    KnowledgeGraphManager(KnowledgeGraphManager&&) = delete;
    KnowledgeGraphManager& operator=(KnowledgeGraphManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    [[nodiscard]] String createNode(NodeType type, const String& name, const String& value = "",
                                      const String& tags = "", uint8_t importance = 0) noexcept;
    [[nodiscard]] bool deleteNode(const String& nodeId) noexcept;
    [[nodiscard]] bool mergeNodes(const String& targetId, const String& sourceId) noexcept;
    [[nodiscard]] GraphNode getNode(const String& nodeId) const noexcept;
    [[nodiscard]] std::vector<GraphNode> searchNodes(const String& query) const noexcept;
    [[nodiscard]] std::vector<GraphNode> getNodesByType(NodeType type) const noexcept;
    [[nodiscard]] const std::vector<GraphNode>& getAllNodes() const noexcept;

    [[nodiscard]] String createEdge(const String& sourceId, const String& targetId,
                                      const String& relationship, float strength = 1.0f,
                                      bool bidirectional = false) noexcept;
    [[nodiscard]] bool deleteEdge(const String& edgeId) noexcept;
    [[nodiscard]] std::vector<GraphEdge> getEdgesForNode(const String& nodeId) const noexcept;
    [[nodiscard]] std::vector<GraphNode> getRelated(const String& nodeId, float minStrength = 0.0f) const noexcept;
    [[nodiscard]] std::vector<GraphEdge> getRelationshipsByType(const String& relationship) const noexcept;
    [[nodiscard]] std::vector<GraphEdge> getEdgesBetween(const String& nodeA, const String& nodeB) const noexcept;
    [[nodiscard]] float getRelationshipStrength(const String& sourceId, const String& targetId) const noexcept;
    [[nodiscard]] bool updateRelationshipStrength(const String& edgeId, float newStrength) noexcept;

    [[nodiscard]] std::vector<GraphNode> traverse(const String& startId, const String& relationship = "",
                                                     uint8_t maxDepth = 3) const noexcept;

    [[nodiscard]] size_t autoLink() noexcept;
    [[nodiscard]] size_t autoGenerateRelationships() noexcept;
    [[nodiscard]] size_t nodeCount() const noexcept;
    [[nodiscard]] size_t edgeCount() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;

private:
    static constexpr const char* kLogCategory = "KnowledgeGraph";
    static constexpr size_t kMaxNodes = KG_MAX_NODES;
    static constexpr size_t kMaxEdges = KG_MAX_EDGES;
    static constexpr float kAutoLinkThresh = KG_AUTO_LINK_THRESHOLD;

    String generateId() noexcept;
    size_t findNode(const String& id) const noexcept;
    size_t findEdge(const String& id) const noexcept;
    float computeTagSimilarity(const String& a, const String& b) const noexcept;
    const char* nodeTypeToString(NodeType t) const noexcept;
    NodeType stringToNodeType(const String& s) const noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<GraphNode> m_nodes;
    std::vector<GraphEdge> m_edges;
    unsigned long m_lastIdCounter;
};

extern KnowledgeGraphManager knowledgeGraphManager;

#endif
