#include "knowledge_graph_manager.h"
#include "json_helpers.h"

KnowledgeGraphManager knowledgeGraphManager;

namespace {

String extractStr(const String& json, const char* key) noexcept {
    String search = String("\"") + key + "\":\"";
    int start = json.indexOf(search);
    if (start < 0) return "";
    start += search.length();
    int end = json.indexOf('"', start);
    return (end < 0) ? "" : json.substring(start, end);
}

float extractFloat(const String& json, const char* key, float def) noexcept {
    String search = String("\"") + key + "\":";
    int start = json.indexOf(search);
    if (start < 0) return def;
    start += search.length();
    int end = start;
    while (end < (int)json.length() && json[end] != ',' && json[end] != '}' && json[end] != ' ') end++;
    if (end == start) return def;
    return json.substring(start, end).toFloat();
}

int extractInt(const String& json, const char* key, int def) noexcept {
    String search = String("\"") + key + "\":";
    int start = json.indexOf(search);
    if (start < 0) return def;
    start += search.length();
    int end = start;
    while (end < (int)json.length() && json[end] >= '0' && json[end] <= '9') end++;
    if (end == start) return def;
    return json.substring(start, end).toInt();
}

bool extractBool(const String& json, const char* key, bool def) noexcept {
    String search = String("\"") + key + "\":";
    int start = json.indexOf(search);
    if (start < 0) return def;
    start += search.length();
    return json.substring(start, start + 4) == "true";
}

} // namespace

KnowledgeGraphManager::KnowledgeGraphManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastIdCounter(0) {
    m_nodes.reserve(kMaxNodes);
    m_edges.reserve(kMaxEdges);
}

KnowledgeGraphManager::~KnowledgeGraphManager() noexcept {
    if (m_dirty) save();
}

bool KnowledgeGraphManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u nodes, %u edges)", m_nodes.size(), m_edges.size());
    return true;
}

void KnowledgeGraphManager::update() noexcept {
    if (!m_initialized) return;
    static unsigned long lastSave = 0;
    unsigned long now = millis();
    if (m_dirty && (now - lastSave > 5000)) { lastSave = now; if (save()) m_dirty = false; }
}

String KnowledgeGraphManager::createNode(NodeType type, const String& name, const String& value,
                                           const String& tags, uint8_t importance) noexcept {
    if (!m_initialized || m_nodes.size() >= kMaxNodes || name.isEmpty()) return "";
    GraphNode n;
    n.id = generateId();
    n.type = type;
    n.name = name;
    n.value = value;
    n.tags = tags;
    n.importance = importance;
    n.createdAt = millis();
    n.updatedAt = n.createdAt;
    m_nodes.push_back(n);
    m_dirty = true;
    Logger::info(kLogCategory, "Node '%s' created (%s)", name.c_str(), nodeTypeToString(type));
    return n.id;
}

bool KnowledgeGraphManager::deleteNode(const String& nodeId) noexcept {
    size_t idx = findNode(nodeId);
    if (idx == SIZE_MAX) return false;
    for (auto it = m_edges.begin(); it != m_edges.end(); ) {
        if (it->sourceId == nodeId || it->targetId == nodeId) it = m_edges.erase(it);
        else ++it;
    }
    m_nodes.erase(m_nodes.begin() + static_cast<ptrdiff_t>(idx));
    m_dirty = true;
    return true;
}

bool KnowledgeGraphManager::mergeNodes(const String& targetId, const String& sourceId) noexcept {
    size_t tIdx = findNode(targetId);
    size_t sIdx = findNode(sourceId);
    if (tIdx == SIZE_MAX || sIdx == SIZE_MAX) return false;

    GraphNode& target = m_nodes[tIdx];
    const GraphNode& source = m_nodes[sIdx];

    if (source.importance > target.importance) target.importance = source.importance;
    if (target.tags.isEmpty() && !source.tags.isEmpty()) target.tags = source.tags;
    else if (!source.tags.isEmpty()) target.tags += "," + source.tags;
    target.updatedAt = millis();

    for (auto& e : m_edges) {
        if (e.sourceId == sourceId) e.sourceId = targetId;
        if (e.targetId == sourceId) e.targetId = targetId;
    }

    deleteNode(sourceId);
    m_dirty = true;
    Logger::info(kLogCategory, "Merged node '%s' into '%s'", source.name.c_str(), target.name.c_str());
    return true;
}

GraphNode KnowledgeGraphManager::getNode(const String& nodeId) const noexcept {
    size_t idx = findNode(nodeId);
    return (idx != SIZE_MAX) ? m_nodes[idx] : GraphNode();
}

std::vector<GraphNode> KnowledgeGraphManager::searchNodes(const String& query) const noexcept {
    std::vector<GraphNode> results;
    if (query.isEmpty()) return results;
    String lq = query; lq.toLowerCase();
    for (const auto& n : m_nodes) {
        String ln = n.name; ln.toLowerCase();
        String lv = n.value; lv.toLowerCase();
        String lt = n.tags; lt.toLowerCase();
        if (ln.indexOf(lq) >= 0 || lv.indexOf(lq) >= 0 || lt.indexOf(lq) >= 0) results.push_back(n);
    }
    return results;
}

std::vector<GraphNode> KnowledgeGraphManager::getNodesByType(NodeType type) const noexcept {
    std::vector<GraphNode> results;
    for (const auto& n : m_nodes) { if (n.type == type) results.push_back(n); }
    return results;
}

const std::vector<GraphNode>& KnowledgeGraphManager::getAllNodes() const noexcept {
    return m_nodes;
}

String KnowledgeGraphManager::createEdge(const String& sourceId, const String& targetId,
                                           const String& relationship, float strength,
                                           bool bidirectional) noexcept {
    if (!m_initialized || m_edges.size() >= kMaxEdges || sourceId.isEmpty() || targetId.isEmpty()) return "";
    if (findNode(sourceId) == SIZE_MAX || findNode(targetId) == SIZE_MAX) return "";
    GraphEdge e;
    e.id = generateId();
    e.sourceId = sourceId;
    e.targetId = targetId;
    e.relationship = relationship;
    e.strength = strength;
    e.bidirectional = bidirectional;
    e.createdAt = millis();
    m_edges.push_back(e);
    m_dirty = true;
    return e.id;
}

bool KnowledgeGraphManager::deleteEdge(const String& edgeId) noexcept {
    size_t idx = findEdge(edgeId);
    if (idx == SIZE_MAX) return false;
    m_edges.erase(m_edges.begin() + static_cast<ptrdiff_t>(idx));
    m_dirty = true;
    return true;
}

std::vector<GraphEdge> KnowledgeGraphManager::getEdgesForNode(const String& nodeId) const noexcept {
    std::vector<GraphEdge> results;
    for (const auto& e : m_edges) {
        if (e.sourceId == nodeId || (e.bidirectional && e.targetId == nodeId)) results.push_back(e);
    }
    return results;
}

std::vector<GraphNode> KnowledgeGraphManager::getRelated(const String& nodeId, float minStrength) const noexcept {
    std::vector<GraphNode> results;
    for (const auto& e : m_edges) {
        if (e.strength < minStrength) continue;
        if (e.sourceId == nodeId) {
            size_t idx = findNode(e.targetId);
            if (idx != SIZE_MAX) results.push_back(m_nodes[idx]);
        }
        if (e.bidirectional && e.targetId == nodeId) {
            size_t idx = findNode(e.sourceId);
            if (idx != SIZE_MAX) results.push_back(m_nodes[idx]);
        }
    }
    return results;
}

std::vector<GraphEdge> KnowledgeGraphManager::getRelationshipsByType(const String& relationship) const noexcept {
    std::vector<GraphEdge> results;
    for (const auto& e : m_edges) {
        if (e.relationship == relationship) results.push_back(e);
    }
    return results;
}

std::vector<GraphEdge> KnowledgeGraphManager::getEdgesBetween(const String& nodeA, const String& nodeB) const noexcept {
    std::vector<GraphEdge> results;
    for (const auto& e : m_edges) {
        if ((e.sourceId == nodeA && e.targetId == nodeB) || (e.bidirectional && e.sourceId == nodeB && e.targetId == nodeA)) {
            results.push_back(e);
        }
    }
    return results;
}

float KnowledgeGraphManager::getRelationshipStrength(const String& sourceId, const String& targetId) const noexcept {
    for (const auto& e : m_edges) {
        if ((e.sourceId == sourceId && e.targetId == targetId) || (e.bidirectional && e.sourceId == targetId && e.targetId == sourceId)) {
            return e.strength;
        }
    }
    return 0.0f;
}

bool KnowledgeGraphManager::updateRelationshipStrength(const String& edgeId, float newStrength) noexcept {
    for (auto& e : m_edges) {
        if (e.id == edgeId) {
            e.strength = newStrength;
            e.updatedAt = millis();
            m_dirty = true;
            return true;
        }
    }
    return false;
}

size_t KnowledgeGraphManager::autoGenerateRelationships() noexcept {
    if (!m_initialized) return 0;
    size_t generated = 0;

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        for (size_t j = i + 1; j < m_nodes.size(); ++j) {
            bool exists = false;
            for (const auto& e : m_edges) {
                if ((e.sourceId == m_nodes[i].id && e.targetId == m_nodes[j].id) ||
                    (e.sourceId == m_nodes[j].id && e.targetId == m_nodes[i].id)) { exists = true; break; }
            }
            if (exists) continue;

            float sim = computeTagSimilarity(m_nodes[i].tags, m_nodes[j].tags);
            if (sim >= REL_AUTO_LINK_MIN_STRENGTH) {
                createEdge(m_nodes[i].id, m_nodes[j].id, "related_to", sim, true);
                generated++;
            }

            if (m_nodes[i].type == NodeType::PROJECT && m_nodes[j].type == NodeType::GOAL) {
                createEdge(m_nodes[i].id, m_nodes[j].id, "contains", 0.8f, false);
                generated++;
            }
            if (m_nodes[i].type == NodeType::GOAL && m_nodes[j].type == NodeType::PROJECT) {
                createEdge(m_nodes[i].id, m_nodes[j].id, "part_of", 0.8f, false);
                generated++;
            }
        }
    }

    if (generated > 0) Logger::info(kLogCategory, "Auto-generated %u relationships", generated);
    return generated;
}

std::vector<GraphNode> KnowledgeGraphManager::traverse(const String& startId, const String& relationship,
                                                         uint8_t maxDepth) const noexcept {
    std::vector<GraphNode> visited;
    if (findNode(startId) == SIZE_MAX) return visited;

    std::vector<String> frontier, next;
    frontier.push_back(startId);

    for (uint8_t depth = 0; depth < maxDepth && !frontier.empty(); ++depth) {
        next.clear();
        for (const auto& current : frontier) {
            auto edges = getEdgesForNode(current);
            for (const auto& e : edges) {
                if (!relationship.isEmpty() && e.relationship != relationship) continue;
                String neighbor = (e.sourceId == current) ? e.targetId : e.sourceId;
                size_t idx = findNode(neighbor);
                if (idx == SIZE_MAX) continue;
                bool alreadyVisited = false;
                for (const auto& v : visited) { if (v.id == neighbor) { alreadyVisited = true; break; } }
                if (!alreadyVisited) {
                    visited.push_back(m_nodes[idx]);
                    next.push_back(neighbor);
                }
            }
        }
        frontier.swap(next);
    }

    return visited;
}

size_t KnowledgeGraphManager::autoLink() noexcept {
    if (!m_initialized) return 0;
    size_t linked = 0;
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        for (size_t j = i + 1; j < m_nodes.size(); ++j) {
            if (m_nodes[i].tags.isEmpty() || m_nodes[j].tags.isEmpty()) continue;
            float sim = computeTagSimilarity(m_nodes[i].tags, m_nodes[j].tags);
            if (sim >= kAutoLinkThresh) {
                bool exists = false;
                for (const auto& e : m_edges) {
                    if ((e.sourceId == m_nodes[i].id && e.targetId == m_nodes[j].id) ||
                        (e.sourceId == m_nodes[j].id && e.targetId == m_nodes[i].id)) { exists = true; break; }
                }
                if (!exists) {
                    createEdge(m_nodes[i].id, m_nodes[j].id, "related", sim, true);
                    linked++;
                }
            }
        }
    }
    if (linked > 0) Logger::info(kLogCategory, "Auto-linked %u node pairs", linked);
    return linked;
}

size_t KnowledgeGraphManager::nodeCount() const noexcept { return m_nodes.size(); }
size_t KnowledgeGraphManager::edgeCount() const noexcept { return m_edges.size(); }
bool KnowledgeGraphManager::isInitialized() const noexcept { return m_initialized; }

bool KnowledgeGraphManager::save() noexcept {
    String json; json.reserve(8192);
    json += "{\"nodes\":[";
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (i > 0) json += ",";
        const auto& n = m_nodes[i];
        json += "{";
        json += "\"id\":\"" + escapeJson(n.id) + "\",";
        json += "\"type\":\"" + String(nodeTypeToString(n.type)) + "\",";
        json += "\"name\":\"" + escapeJson(n.name) + "\",";
        json += "\"value\":\"" + escapeJson(n.value) + "\",";
        json += "\"tags\":\"" + escapeJson(n.tags) + "\",";
        json += "\"created\":" + String(n.createdAt) + ",";
        json += "\"updated\":" + String(n.updatedAt) + ",";
        json += "\"importance\":" + String(n.importance);
        json += "}";
    }
    json += "],\"edges\":[";
    for (size_t i = 0; i < m_edges.size(); ++i) {
        if (i > 0) json += ",";
        const auto& e = m_edges[i];
        json += "{";
        json += "\"id\":\"" + escapeJson(e.id) + "\",";
        json += "\"source\":\"" + escapeJson(e.sourceId) + "\",";
        json += "\"target\":\"" + escapeJson(e.targetId) + "\",";
        json += "\"rel\":\"" + escapeJson(e.relationship) + "\",";
        json += "\"strength\":" + String(e.strength, 2) + ",";
        json += "\"conf\":" + String(e.confidence, 2) + ",";
        json += "\"bidir\":" + String(e.bidirectional ? "true" : "false") + ",";
        json += "\"created\":" + String(e.createdAt) + ",";
        json += "\"updated\":" + String(e.updatedAt);
        json += "}";
    }
    json += "]}";
    StorageStatus st = storageManager.writeFile(KNOWLEDGE_GRAPH_PATH, json, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_dirty = false; return true; }
    return false;
}

bool KnowledgeGraphManager::load() noexcept {
    if (!storageManager.fileExists(KNOWLEDGE_GRAPH_PATH, StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(KNOWLEDGE_GRAPH_PATH, content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_nodes.clear(); m_edges.clear();

    // Parse nodes array
    int nodesStart = content.indexOf("\"nodes\":[");
    if (nodesStart >= 0) {
        int pos = content.indexOf('[', nodesStart) + 1;
        while (pos < (int)content.length()) {
            int braceStart = content.indexOf('{', pos);
            if (braceStart < 0) break;
            int braceEnd = content.indexOf('}', braceStart);
            if (braceEnd < 0) break;
            String obj = content.substring(braceStart, braceEnd + 1);
            GraphNode n;
            n.id = extractStr(obj, "id");
            n.name = extractStr(obj, "name");
            n.value = extractStr(obj, "value");
            n.tags = extractStr(obj, "tags");
            n.type = stringToNodeType(extractStr(obj, "type"));
            n.importance = static_cast<uint8_t>(extractInt(obj, "importance", 0));
            if (!n.id.isEmpty()) m_nodes.push_back(n);
            pos = braceEnd + 1;
        }
    }

    // Parse edges array
    int edgesStart = content.indexOf("\"edges\":[");
    if (edgesStart >= 0) {
        int pos = content.indexOf('[', edgesStart) + 1;
        while (pos < (int)content.length()) {
            int braceStart = content.indexOf('{', pos);
            if (braceStart < 0) break;
            int braceEnd = content.indexOf('}', braceStart);
            if (braceEnd < 0) break;
            String obj = content.substring(braceStart, braceEnd + 1);
            GraphEdge e;
            e.id = extractStr(obj, "id");
            e.sourceId = extractStr(obj, "source");
            e.targetId = extractStr(obj, "target");
            e.relationship = extractStr(obj, "rel");
            e.strength = extractFloat(obj, "strength", 1.0f);
            e.confidence = extractFloat(obj, "conf", 1.0f);
            e.bidirectional = extractBool(obj, "bidir", false);
            if (!e.id.isEmpty()) m_edges.push_back(e);
            pos = braceEnd + 1;
        }
    }

    return true;
}

String KnowledgeGraphManager::generateId() noexcept {
    unsigned long now = millis(); m_lastIdCounter++;
    uint32_t mix = static_cast<uint32_t>(now) ^ (m_lastIdCounter << 16) ^ (ESP.getEfuseMac() & 0xFFFFFFFF);
    String id; id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) { id += hex[val & 0x0F]; val = (val >> 2) ^ (val << 3) ^ (m_lastIdCounter + i); }
    return id;
}

size_t KnowledgeGraphManager::findNode(const String& id) const noexcept {
    for (size_t i = 0; i < m_nodes.size(); ++i) { if (m_nodes[i].id == id) return i; }
    return SIZE_MAX;
}

size_t KnowledgeGraphManager::findEdge(const String& id) const noexcept {
    for (size_t i = 0; i < m_edges.size(); ++i) { if (m_edges[i].id == id) return i; }
    return SIZE_MAX;
}

float KnowledgeGraphManager::computeTagSimilarity(const String& a, const String& b) const noexcept {
    if (a.isEmpty() || b.isEmpty()) return 0.0f;
    std::vector<String> ta, tb;
    int s = 0;
    while (s < (int)a.length()) { int c = a.indexOf(',', s); if (c < 0) { ta.push_back(a.substring(s)); break; } ta.push_back(a.substring(s, c)); s = c + 1; }
    s = 0;
    while (s < (int)b.length()) { int c = b.indexOf(',', s); if (c < 0) { tb.push_back(b.substring(s)); break; } tb.push_back(b.substring(s, c)); s = c + 1; }
    size_t matches = 0;
    for (const auto& t1 : ta) { String l1 = t1; l1.toLowerCase(); l1.trim(); for (const auto& t2 : tb) { String l2 = t2; l2.toLowerCase(); l2.trim(); if (l1 == l2) matches++; } }
    size_t total = ta.size() + tb.size();
    return (total > 0) ? (2.0f * matches / total) : 0.0f;
}

const char* KnowledgeGraphManager::nodeTypeToString(NodeType t) const noexcept {
    switch (t) {
        case NodeType::USER: return "USER"; case NodeType::PROJECT: return "PROJECT";
        case NodeType::DEVICE: return "DEVICE"; case NodeType::PERSON: return "PERSON";
        case NodeType::SUBJECT: return "SUBJECT"; case NodeType::GOAL: return "GOAL";
        case NodeType::PREFERENCE: return "PREFERENCE"; case NodeType::SKILL: return "SKILL";
        default: return "CUSTOM";
    }
}

NodeType KnowledgeGraphManager::stringToNodeType(const String& s) const noexcept {
    if (s == "USER") return NodeType::USER; if (s == "PROJECT") return NodeType::PROJECT;
    if (s == "DEVICE") return NodeType::DEVICE; if (s == "PERSON") return NodeType::PERSON;
    if (s == "SUBJECT") return NodeType::SUBJECT; if (s == "GOAL") return NodeType::GOAL;
    if (s == "PREFERENCE") return NodeType::PREFERENCE; if (s == "SKILL") return NodeType::SKILL;
    return NodeType::CUSTOM;
}
