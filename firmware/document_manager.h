#ifndef AURA_DOCUMENT_MANAGER_H
#define AURA_DOCUMENT_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

struct DocumentEntry {
    String id;
    String filename;
    String extension;
    String title;
    String description;
    unsigned long timestamp;
    size_t fileSize;
    String tags;
    String graphNodeId;
    bool indexed;

    DocumentEntry() noexcept : timestamp(0), fileSize(0), indexed(false) {}
};

class DocumentManager {
public:
    DocumentManager() noexcept;
    ~DocumentManager() noexcept;

    DocumentManager(const DocumentManager&) = delete;
    DocumentManager& operator=(const DocumentManager&) = delete;
    DocumentManager(DocumentManager&&) = delete;
    DocumentManager& operator=(DocumentManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    // CRUD
    [[nodiscard]] bool storeDocument(const String& filename, const String& content,
                                      const String& title = "", const String& description = "",
                                      const String& tags = "") noexcept;
    [[nodiscard]] bool deleteDocument(const String& id) noexcept;
    [[nodiscard]] bool getDocumentContent(const String& id, String& content) const noexcept;

    // Indexing & search
    void reindexAll() noexcept;
    [[nodiscard]] std::vector<DocumentEntry> search(const String& query) const noexcept;
    [[nodiscard]] std::vector<DocumentEntry> getAllDocuments() const noexcept;
    [[nodiscard]] std::vector<DocumentEntry> getByExtension(const String& ext) const noexcept;

    // KG linking
    [[nodiscard]] bool linkToGraph(const String& docId, const String& graphNodeId) noexcept;

    // Summarization
    [[nodiscard]] String summarize(const String& content, size_t maxLength = 500) const noexcept;

    // Query
    [[nodiscard]] String getDocumentsJson(const String& filter = "") const noexcept;
    [[nodiscard]] size_t documentCount() const noexcept;

    // Persistence
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;

private:
    static constexpr const char* kLogCategory = "DocMgr";
    static constexpr size_t kMaxDocs = DOCUMENT_MAX_COUNT;
    static constexpr size_t kMaxSize = DOCUMENT_MAX_SIZE;

    String generateId() noexcept;
    bool isValidExtension(const String& ext) const noexcept;
    String getContentPath(const String& id) const noexcept;
    void trimToMax() noexcept;

    String serializeDoc(const DocumentEntry& d) const noexcept;
    DocumentEntry deserializeDoc(const String& json) const noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<DocumentEntry> m_documents;
    unsigned long m_lastIdCounter;
};

extern DocumentManager documentManager;

#endif
