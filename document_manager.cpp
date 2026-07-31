#include "document_manager.h"
#include "json_helpers.h"
#include <algorithm>

DocumentManager documentManager;

DocumentManager::DocumentManager() noexcept
    : m_initialized(false), m_dirty(false), m_lastIdCounter(0) {
    m_documents.reserve(kMaxDocs);
}

DocumentManager::~DocumentManager() noexcept {
    if (m_dirty) save();
}

bool DocumentManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    storageManager.createDirectory(DOCUMENTS_PATH, StorageType::SPIFFS);
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u docs)", m_documents.size());
    return true;
}

void DocumentManager::update() noexcept {
    if (!m_initialized) return;
    if (m_dirty && save()) m_dirty = false;
}

String DocumentManager::generateId() noexcept {
    unsigned long now = millis(); m_lastIdCounter++;
    uint32_t mix = static_cast<uint32_t>(now) ^ (m_lastIdCounter << 16) ^ (ESP.getEfuseMac() & 0xFFFFFFFF);
    String id; id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) { id += hex[val & 0x0F]; val = (val >> 2) ^ (val << 3) ^ (m_lastIdCounter + i); }
    return id;
}

bool DocumentManager::isValidExtension(const String& ext) const noexcept {
    return (ext == "txt" || ext == "md" || ext == "json" || ext == "csv" || ext == "pdf");
}

String DocumentManager::getContentPath(const String& id) const noexcept {
    return String(DOCUMENTS_PATH) + "/" + id + ".dat";
}

bool DocumentManager::storeDocument(const String& filename, const String& content,
                                     const String& title, const String& description,
                                     const String& tags) noexcept {
    if (!m_initialized || content.isEmpty()) return false;
    if (content.length() > kMaxSize) {
        Logger::warning(kLogCategory, "Document too large: %u bytes", content.length());
        return false;
    }
    int dotPos = filename.lastIndexOf('.');
    String ext = (dotPos >= 0) ? filename.substring(dotPos + 1) : "";
    ext.toLowerCase();
    if (!isValidExtension(ext)) {
        Logger::warning(kLogCategory, "Unsupported extension: %s", ext.c_str());
        return false;
    }
    trimToMax();
    DocumentEntry doc;
    doc.id = generateId();
    doc.filename = filename;
    doc.extension = ext;
    doc.title = title.isEmpty() ? filename : title;
    doc.description = description;
    doc.timestamp = millis();
    doc.fileSize = content.length();
    doc.tags = tags;
    doc.indexed = true;
    String path = getContentPath(doc.id);
    StorageStatus st = storageManager.writeFile(path.c_str(), content, StorageType::SPIFFS);
    if (st != StorageStatus::SUCCESS) {
        Logger::error(kLogCategory, "Failed to write document content");
        return false;
    }
    m_documents.push_back(doc);
    m_dirty = true;
    Logger::info(kLogCategory, "Stored: %s (%u bytes)", filename.c_str(), content.length());
    return true;
}

bool DocumentManager::deleteDocument(const String& id) noexcept {
    for (auto it = m_documents.begin(); it != m_documents.end(); ++it) {
        if (it->id != id) continue;
        String path = getContentPath(id);
        storageManager.deleteFile(path.c_str(), StorageType::SPIFFS);
        m_documents.erase(it);
        m_dirty = true;
        return true;
    }
    return false;
}

bool DocumentManager::getDocumentContent(const String& id, String& content) const noexcept {
    for (const auto& d : m_documents) {
        if (d.id != id) continue;
        String path = getContentPath(id);
        return (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) == StorageStatus::SUCCESS);
    }
    return false;
}

void DocumentManager::reindexAll() noexcept {
    for (auto& d : m_documents) {
        d.indexed = true;
    }
    Logger::info(kLogCategory, "Reindexed %u documents", m_documents.size());
}

std::vector<DocumentEntry> DocumentManager::search(const String& query) const noexcept {
    std::vector<DocumentEntry> results;
    if (query.isEmpty()) return results;
    String lq = query; lq.toLowerCase();
    for (const auto& d : m_documents) {
        String lt = d.title; lt.toLowerCase();
        String ldesc = d.description; ldesc.toLowerCase();
        String ltags = d.tags; ltags.toLowerCase();
        if (lt.indexOf(lq) >= 0 || ldesc.indexOf(lq) >= 0 || ltags.indexOf(lq) >= 0) {
            results.push_back(d);
        }
    }
    return results;
}

std::vector<DocumentEntry> DocumentManager::getAllDocuments() const noexcept {
    return m_documents;
}

std::vector<DocumentEntry> DocumentManager::getByExtension(const String& ext) const noexcept {
    std::vector<DocumentEntry> results;
    for (const auto& d : m_documents) {
        if (d.extension == ext) results.push_back(d);
    }
    return results;
}

bool DocumentManager::linkToGraph(const String& docId, const String& graphNodeId) noexcept {
    for (auto& d : m_documents) {
        if (d.id == docId) { d.graphNodeId = graphNodeId; m_dirty = true; return true; }
    }
    return false;
}

String DocumentManager::summarize(const String& content, size_t maxLength) const noexcept {
    if (content.length() <= maxLength) return content;
    String result;
    int pos = 0;
    while (result.length() < (int)maxLength && pos < (int)content.length()) {
        int nextPeriod = content.indexOf('.', pos);
        if (nextPeriod < 0) { nextPeriod = content.length(); }
        String sentence = content.substring(pos, nextPeriod + 1);
        if (result.length() + sentence.length() > (int)maxLength) break;
        result += sentence;
        pos = nextPeriod + 1;
        while (pos < (int)content.length() && content[pos] == ' ') pos++;
    }
    if (result.isEmpty()) {
        result = content.substring(0, maxLength) + "...";
    }
    return result;
}

String DocumentManager::getDocumentsJson(const String& filter) const noexcept {
    String json; json.reserve(4096);
    json += "{\"documents\":[";
    bool first = true;
    for (const auto& d : m_documents) {
        if (!filter.isEmpty() && d.extension != filter) continue;
        if (!first) json += ",";
        first = false;
        json += "{";
        json += "\"id\":\"" + escapeJson(d.id) + "\",";
        json += "\"name\":\"" + escapeJson(d.filename) + "\",";
        json += "\"ext\":\"" + escapeJson(d.extension) + "\",";
        json += "\"title\":\"" + escapeJson(d.title) + "\",";
        json += "\"desc\":\"" + escapeJson(d.description) + "\",";
        json += "\"ts\":" + String(d.timestamp) + ",";
        json += "\"size\":" + String(d.fileSize) + ",";
        json += "\"tags\":\"" + escapeJson(d.tags) + "\",";
        json += "\"graph\":\"" + escapeJson(d.graphNodeId) + "\",";
        json += "\"indexed\":" + String(d.indexed ? "true" : "false");
        json += "}";
    }
    json += "]}";
    return json;
}

size_t DocumentManager::documentCount() const noexcept { return m_documents.size(); }

bool DocumentManager::isInitialized() const noexcept { return m_initialized; }

bool DocumentManager::save() noexcept {
    String path = String(DOCUMENTS_PATH) + "/index.json";
    String j; j.reserve(8192);
    j += "{\"version\":1,\"documents\":[";
    for (size_t i = 0; i < m_documents.size(); ++i) {
        if (i > 0) j += ",";
        j += serializeDoc(m_documents[i]);
    }
    j += "]}";
    StorageStatus st = storageManager.writeFile(path.c_str(), j, StorageType::SPIFFS);
    if (st == StorageStatus::SUCCESS) { m_dirty = false; return true; }
    return false;
}

bool DocumentManager::load() noexcept {
    String path = String(DOCUMENTS_PATH) + "/index.json";
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS)) return false;
    String content;
    if (storageManager.readFile(path.c_str(), content, StorageType::SPIFFS) != StorageStatus::SUCCESS || content.isEmpty()) return false;
    m_documents.clear();
    int pos = content.indexOf("\"documents\":[");
    if (pos < 0) return false;
    pos = content.indexOf('[', pos) + 1;
    while (pos < (int)content.length()) {
        int braceStart = content.indexOf('{', pos);
        if (braceStart < 0) break;
        int braceEnd = content.indexOf('}', braceStart);
        if (braceEnd < 0) break;
        String obj = content.substring(braceStart, braceEnd + 1);
        DocumentEntry d = deserializeDoc(obj);
        if (!d.id.isEmpty()) m_documents.push_back(d);
        pos = braceEnd + 1;
    }
    return true;
}

void DocumentManager::trimToMax() noexcept {
    while (m_documents.size() >= kMaxDocs) {
        m_documents.erase(m_documents.begin());
        m_dirty = true;
    }
}

String DocumentManager::serializeDoc(const DocumentEntry& d) const noexcept {
    String j; j.reserve(256);
    j += "{";
    j += "\"id\":\"" + escapeJson(d.id) + "\",";
    j += "\"name\":\"" + escapeJson(d.filename) + "\",";
    j += "\"ext\":\"" + escapeJson(d.extension) + "\",";
    j += "\"title\":\"" + escapeJson(d.title) + "\",";
    j += "\"desc\":\"" + escapeJson(d.description) + "\",";
    j += "\"ts\":" + String(d.timestamp) + ",";
    j += "\"size\":" + String(d.fileSize) + ",";
    j += "\"tags\":\"" + escapeJson(d.tags) + "\",";
    j += "\"graph\":\"" + escapeJson(d.graphNodeId) + "\",";
    j += "\"idx\":" + String(d.indexed ? "true" : "false");
    j += "}";
    return j;
}

DocumentEntry DocumentManager::deserializeDoc(const String& json) const noexcept {
    DocumentEntry d;
    auto eS = [&](const char* key) -> String {
        String s = String("\"") + key + "\":\"";
        int start = json.indexOf(s);
        if (start < 0) return "";
        start += s.length();
        int end = json.indexOf('"', start);
        return (end < 0) ? "" : json.substring(start, end);
    };
    auto eI = [&](const char* key, int def) -> int {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s);
        if (start < 0) return def;
        start += s.length();
        int end = start;
        while (end < (int)json.length() && json[end] >= '0' && json[end] <= '9') end++;
        return (end == start) ? def : json.substring(start, end).toInt();
    };
    auto eB = [&](const char* key, bool def) -> bool {
        String s = String("\"") + key + "\":";
        int start = json.indexOf(s);
        if (start < 0) return def;
        start += s.length();
        return json.substring(start).startsWith("true");
    };
    d.id = eS("id");
    d.filename = eS("name");
    d.extension = eS("ext");
    d.title = eS("title");
    d.description = eS("desc");
    d.timestamp = static_cast<unsigned long>(eI("ts", 0));
    d.fileSize = static_cast<size_t>(eI("size", 0));
    d.tags = eS("tags");
    d.graphNodeId = eS("graph");
    d.indexed = eB("idx", true);
    return d;
}
