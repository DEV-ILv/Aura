#ifndef AURA_STORAGE_MANAGER_H
#define AURA_STORAGE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <cstdint>
#include "config.h"
#include "logger.h"

/**
 * @enum StorageType
 * @brief Enumeration of available storage media
 */
enum class StorageType : uint8_t {
    SPIFFS,      ///< Internal flash filesystem
    SD_CARD,     ///< External SD card
    UNKNOWN      ///< Unknown storage type
};

/**
 * @enum StorageStatus
 * @brief Status of storage operations
 */
enum class StorageStatus : uint8_t {
    SUCCESS,           ///< Operation successful
    ERROR_NOT_MOUNTED, ///< Storage not mounted
    ERROR_NOT_FOUND,   ///< File or directory not found
    ERROR_INVALID,     ///< Invalid argument
    ERROR_NO_SPACE,    ///< Insufficient space
    ERROR_IO,          ///< I/O error
    ERROR_PERMISSION,  ///< Permission denied
    ERROR_EXISTS,      ///< File or directory exists
    ERROR_UNKNOWN      ///< Unknown error
};

/**
 * @struct FileInfo
 * @brief Information about a file
 */
struct FileInfo {
    String name;
    size_t size;
    bool isDirectory;
    unsigned long modified;
};

/**
 * @struct StorageStatistics
 * @brief Storage usage statistics.
 */
struct StorageStatistics {
    size_t totalSpace;
    size_t freeSpace;
    size_t usedSpace;
    uint32_t fileCount;
};

/**
 * @class StorageManager
 * @brief Single authority for all persistent storage operations
 *
 * Manages SPIFFS, SD card, file operations, and directory operations.
 * All modules must access storage exclusively through this class.
 *
 * Features:
 * - Unified interface for SPIFFS and SD card
 * - File and directory management
 * - Conversation and audio storage
 * - Settings backup and restore
 * - Space management
 * - Non-blocking operations where practical
 * - Production-quality error handling
 */
class StorageManager {
public:
    /**
     * @brief Constructor
     */
    StorageManager() noexcept;

    /**
     * @brief Destructor
     */
    ~StorageManager() noexcept;

    // Delete copy semantics
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;

    // Delete move semantics
    StorageManager(StorageManager&&) = delete;
    StorageManager& operator=(StorageManager&&) = delete;

    /**
     * @brief Initialize the storage manager
     * @return true if initialization successful, false otherwise
     * @note Should be called once during setup()
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Update storage manager state
     * @note Should be called regularly from loop()
     */
    void update() noexcept;

    /**
     * @brief Scheduler-compatible alias for update()
     */
    void run() noexcept;

    // ========================================================================
    // Mount/Unmount Operations
    // ========================================================================

    /**
     * @brief Mount SPIFFS filesystem
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus mountSPIFFS() noexcept;

    /**
     * @brief Unmount SPIFFS filesystem
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus unmountSPIFFS() noexcept;

    /**
     * @brief Mount SD card
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus mountSD() noexcept;

    /**
     * @brief Unmount SD card
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus unmountSD() noexcept;

    /**
     * @brief Check if SPIFFS is mounted
     * @return true if SPIFFS is mounted, false otherwise
     */
    [[nodiscard]] bool isSPIFFSMounted() const noexcept;

    /**
     * @brief Check if SD card is mounted
     * @return true if SD card is mounted, false otherwise
     */
    [[nodiscard]] bool isSDMounted() const noexcept;
    /**
     * @brief Check whether the storage subsystem is initialized.
     * @return true if initialized.
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Check whether the storage subsystem is healthy.
     * @return true if healthy.
     */
    [[nodiscard]] bool isHealthy() const noexcept;

    /**
     * @brief Get the last storage operation status.
     * @return Last StorageStatus.
     */
    [[nodiscard]] StorageStatus getLastStatus() const noexcept;

    // ========================================================================
    // Directory Operations
    // ========================================================================

    /**
     * @brief Create a directory
     * @param path Directory path
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus createDirectory(
        const char* path,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief Remove a directory
     * @param path Directory path
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus removeDirectory(
        const char* path,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief List directory contents
     * @param path Directory path
     * @param entries Output vector of FileInfo structures
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus listDirectory(
        const char* path,
        std::vector<FileInfo>& entries,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    // ========================================================================
    // File Query Operations
    // ========================================================================

    /**
     * @brief Check if file or directory exists
     * @param path File or directory path
     * @param storageType Storage media to use
     * @return true if exists, false otherwise
     */
    [[nodiscard]] bool fileExists(
        const char* path,
        StorageType storageType = StorageType::SPIFFS) const noexcept;

    /**
     * @brief Get file size
     * @param path File path
     * @param storageType Storage media to use
     * @return File size in bytes, or 0 if not found
     */
    [[nodiscard]] size_t getFileSize(
        const char* path,
        StorageType storageType = StorageType::SPIFFS) const noexcept;

    /**
     * @brief Get total space available
     * @param storageType Storage media to query
     * @return Total space in bytes
     */
    [[nodiscard]] size_t getTotalSpace(
        StorageType storageType = StorageType::SPIFFS) const noexcept;

    /**
     * @brief Get free space available
     * @param storageType Storage media to query
     * @return Free space in bytes
     */
    [[nodiscard]] size_t getFreeSpace(
        StorageType storageType = StorageType::SPIFFS) const noexcept;

    /**
     * @brief Get storage statistics.
     */
    [[nodiscard]] StorageStatus getStatistics(
        StorageType storageType,
        size_t& totalBytes,
        size_t& usedBytes,
        size_t& freeBytes) noexcept;    

    // ========================================================================
    // File Operations
    // ========================================================================

    /**
     * @brief Create a new file
     * @param path File path
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus createFile(
        const char* path,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief Delete a file
     * @param path File path
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus deleteFile(
        const char* path,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief Read file contents into a buffer
     * @param path File path
     * @param buffer Output buffer
     * @param bufferSize Size of output buffer
     * @param bytesRead Output parameter for actual bytes read
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus readFile(
        const char* path,
        uint8_t* buffer,
        size_t bufferSize,
        size_t& bytesRead,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief Read file contents into a String
     * @param path File path
     * @param content Output String reference
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus readFile(
        const char* path,
        String& content,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief Write data to file (overwrites existing content)
     * @param path File path
     * @param data Data buffer to write
     * @param dataSize Size of data in bytes
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus writeFile(
        const char* path,
        const uint8_t* data,
        size_t dataSize,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief Write String to file (overwrites existing content)
     * @param path File path
     * @param content String content to write
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus writeFile(
        const char* path,
        const String& content,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief Append data to file
     * @param path File path
     * @param data Data buffer to append
     * @param dataSize Size of data in bytes
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus appendFile(
        const char* path,
        const uint8_t* data,
        size_t dataSize,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief Append String to file
     * @param path File path
     * @param content String content to append
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus appendFile(
        const char* path,
        const String& content,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief Rename a file
     * @param oldPath Current file path
     * @param newPath New file path
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus renameFile(
        const char* oldPath,
        const char* newPath,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief Copy a file
     * @param sourcePath Source file path
     * @param destPath Destination file path
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus copyFile(
        const char* sourcePath,
        const char* destPath,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    /**
     * @brief Move a file (rename across directories)
     * @param sourcePath Source file path
     * @param destPath Destination file path
     * @param storageType Storage media to use
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus moveFile(
        const char* sourcePath,
        const char* destPath,
        StorageType storageType = StorageType::SPIFFS) noexcept;

    // ========================================================================
    // Format Operations
    // ========================================================================

    /**
     * @brief Format SPIFFS filesystem (destructive)
     * @return StorageStatus indicating success or error
     * @warning This operation erases all SPIFFS data
     */
    [[nodiscard]] StorageStatus formatSPIFFS() noexcept;

    /**
     * @brief Format SD card (destructive)
     * @return StorageStatus indicating success or error
     * @warning This operation erases all SD card data
     */
    [[nodiscard]] StorageStatus formatSD() noexcept;

    // ========================================================================
    // Application-Specific Operations
    // ========================================================================

    /**
     * @brief Save conversation history
     * @param conversationId Unique conversation identifier
     * @param content Conversation content (typically JSON)
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus saveConversation(
        const char* conversationId,
        const String& content) noexcept;

    /**
     * @brief Load conversation history
     * @param conversationId Unique conversation identifier
     * @param content Output String containing conversation content
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus loadConversation(
        const char* conversationId,
        String& content) noexcept;

    /**
     * @brief Save audio recording
     * @param recordingId Unique recording identifier
     * @param audioData Audio data buffer (PCM16 or compressed)
     * @param audioSize Size of audio data in bytes
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus saveAudio(
        const char* recordingId,
        const uint8_t* audioData,
        size_t audioSize) noexcept;

    /**
     * @brief Load audio recording
     * @param recordingId Unique recording identifier
     * @param audioData Output buffer for audio data
     * @param bufferSize Size of output buffer
     * @param bytesRead Output parameter for actual bytes read
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus loadAudio(
        const char* recordingId,
        uint8_t* audioData,
        size_t bufferSize,
        size_t& bytesRead) noexcept;

    /**
     * @brief Save reminder entry
     * @param reminderId Unique reminder identifier
     * @param content Reminder content (typically JSON)
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus saveReminder(
        const char* reminderId,
        const String& content) noexcept;

    /**
     * @brief Load reminder entry
     * @param reminderId Unique reminder identifier
     * @param content Output String containing reminder content
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus loadReminder(
        const char* reminderId,
        String& content) noexcept;

    /**
     * @brief Backup device settings
     * @param backupId Unique backup identifier
     * @param settingsData Settings data buffer
     * @param settingsSize Size of settings data in bytes
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus backupSettings(
        const char* backupId,
        const uint8_t* settingsData,
        size_t settingsSize) noexcept;

    /**
     * @brief Restore device settings from backup
     * @param backupId Unique backup identifier
     * @param settingsData Output buffer for settings data
     * @param bufferSize Size of output buffer
     * @param bytesRead Output parameter for actual bytes read
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus restoreSettings(
        const char* backupId,
        uint8_t* settingsData,
        size_t bufferSize,
        size_t& bytesRead) noexcept;

    /**
     * @brief Check whether a backup exists.
     */
    [[nodiscard]] bool backupExists(
        const char* backupId) const noexcept;

    /**
     * @brief Delete old conversation archives
     * @param maxAge Maximum age in seconds
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus cleanupOldConversations(
        unsigned long maxAge) noexcept;

    /**
     * @brief Delete old audio recordings
     * @param maxAge Maximum age in seconds
     * @return StorageStatus indicating success or error
     */
    [[nodiscard]] StorageStatus cleanupOldAudio(
        unsigned long maxAge) noexcept;

    /**
     * @brief Save a log entry.
     */
    [[nodiscard]] StorageStatus saveLog(
        const String& log) noexcept;

    /**
     * @brief Delete all stored logs.
     */
    [[nodiscard]] StorageStatus clearLogs() noexcept;

private:
    // Private helper methods
    StorageStatus validatePath(const char* path) const noexcept;
    StorageStatus checkSpace(size_t requiredBytes, StorageType storageType) noexcept;
    StorageStatus recoverFromTemp(const char* path, StorageType storageType) noexcept;
    StorageStatus recoverOrphanedTmps() noexcept;
    String getStoragePath(const char* filename, StorageType storageType) noexcept;
    StorageStatus copyFileInternal(
        const char* sourcePath,
        const char* destPath,
        StorageType storageType) noexcept;
    StorageStatus deleteFileInternal(
        const char* path,
        StorageType storageType) noexcept;

    // Private member variables
    // Default directories
static constexpr const char* CONVERSATION_DIR = "/conversations";
static constexpr const char* AUDIO_DIR        = "/audio";
static constexpr const char* REMINDER_DIR     = "/reminders";
static constexpr const char* BACKUP_DIR       = "/backups";
static constexpr const char* LOG_DIR          = "/logs";
    bool m_spiffsMounted;              ///< SPIFFS mount state
    bool m_sdMounted;                  ///< SD card mount state
    bool m_initialized;                ///< Initialization state
    StorageStatus m_lastStatus;
    bool m_storageHealthy;
    unsigned long m_lastCleanup;       ///< Timestamp of last cleanup operation
    static constexpr unsigned long m_cleanupIntervalMs{3600000}; ///< 1 hour between cleanups
};

/**
 * @brief Global storage manager instance
 */
extern StorageManager storageManager;

#endif // AURA_STORAGE_MANAGER_H