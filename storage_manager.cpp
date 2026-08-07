#include "storage_manager.h"
#include <SPIFFS.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/// Global StorageManager instance
StorageManager storageManager;

// ============================================================================
// Constants
// ============================================================================

namespace {

constexpr const char* kLogCategory = "StorageManager";

fs::FS& getFileSystem(StorageType type) noexcept {
    if (type == StorageType::SPIFFS) {
        return SPIFFS;
    }
    return SD;
}

const char* sdCardTypeName(const uint8_t type) noexcept {
    switch (type) {
        case 0U: return "NONE";
        case 1U: return "MMC";
        case 2U: return "SD";
        case 3U: return "SDHC";
        case 4U: return "UNKNOWN";
        default: return "INVALID";
    }
}


constexpr size_t kChunkSize = 4096U;
constexpr const char* kConversationPath = "/conversations";
constexpr const char* kAudioPath = "/audio";
constexpr const char* kReminderPath = "/reminders";
constexpr const char* kBackupPath = "/backups";
constexpr const char* kLogPath = "/logs";
constexpr unsigned long kMinCleanupIntervalMs = 3600000UL;

}  // namespace

// ============================================================================
// Constructor / Destructor
// ============================================================================

StorageManager::StorageManager() noexcept
    : m_spiffsMounted(false),
      m_sdMounted(false),
      m_initialized(false),
      m_lastCleanup(0),
      m_storageHealthy(false),
      m_lastStatus(StorageStatus::SUCCESS)
{
}

StorageManager::~StorageManager() noexcept
{
    unmountSPIFFS();
    unmountSD();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool StorageManager::initialize() noexcept
{
    if (m_initialized)
    {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    Logger::info(kLogCategory, "Initializing storage manager");

    // Mount SPIFFS
    if (mountSPIFFS() != StorageStatus::SUCCESS)
    {
        Logger::error(kLogCategory, "Failed to mount SPIFFS");
        m_storageHealthy = false;
        m_lastStatus = StorageStatus::ERROR_IO;
        return false;
    }

    // Create necessary directories
    createDirectory(kConversationPath, StorageType::SPIFFS);
    createDirectory(kAudioPath, StorageType::SPIFFS);
    createDirectory(kReminderPath, StorageType::SPIFFS);
    createDirectory(kBackupPath, StorageType::SPIFFS);
    createDirectory(kLogPath, StorageType::SPIFFS);

    // Recover any orphaned .tmp files from interrupted atomic writes
    recoverOrphanedTmps();

    // Mount the optional MicroSD card asynchronously on a background task so
    // a slow/detecting card can never stall the WDT-registered loop task during
    // boot. Absent cards fail gracefully and keep SPIFFS as the active store.
    static bool sdMountArmed = false;
    if (!sdMountArmed) {
        sdMountArmed = true;
        StorageManager* self = this;
        xTaskCreatePinnedToCore(
            [](void* param) {
                auto* sm = static_cast<StorageManager*>(param);
                if (sm->mountSD() != StorageStatus::SUCCESS) {
                    Logger::warning("StorageManager", "SD card unavailable (optional)");
                }
                vTaskDelete(nullptr);
            },
            "aura_sd", 4096, self, 1, nullptr, tskNO_AFFINITY);
    }

    m_initialized = true;
    m_storageHealthy = true;
    m_lastStatus = StorageStatus::SUCCESS;
    m_lastCleanup = millis();

    Logger::info(kLogCategory, "Storage manager initialized");
    return true;
}

void StorageManager::update() noexcept
{
    if (!m_initialized)
    {
        return;
    }

    // Periodic cleanup every hour
    unsigned long now = millis();
    if ((now - m_lastCleanup) > kMinCleanupIntervalMs)
    {
        m_lastCleanup = now;
        cleanupOldConversations(86400);
        cleanupOldAudio(604800);
    }
}

void StorageManager::run() noexcept
{
    update();
}

// ============================================================================
// Mount/Unmount Operations
// ============================================================================

StorageStatus StorageManager::mountSPIFFS() noexcept
{
    if (m_spiffsMounted)
    {
        return StorageStatus::SUCCESS;
    }

    if (!SPIFFS.begin(true))
    {
        Logger::error(kLogCategory, "SPIFFS mount failed");
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    m_spiffsMounted = true;
    m_lastStatus = StorageStatus::SUCCESS;
    Logger::info(kLogCategory, "SPIFFS mounted");
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::unmountSPIFFS() noexcept
{
    if (!m_spiffsMounted)
    {
        return StorageStatus::SUCCESS;
    }

    SPIFFS.end();
    m_spiffsMounted = false;
    m_lastStatus = StorageStatus::SUCCESS;
    Logger::info(kLogCategory, "SPIFFS unmounted");
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::mountSD() noexcept
{
    if (m_sdMounted)
    {
        return StorageStatus::SUCCESS;
    }

    // Explicitly bind the SPI bus to the configured SD pins before mounting;
    // ensures the card is probed on the correct bus rather than relying on the
    // default VSPI mapping at the call site.
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    Logger::info(kLogCategory,
        "[SD] SPI initialized (SCK=GPIO%d MISO=GPIO%d MOSI=GPIO%d)",
        (int)SD_SCK_PIN, (int)SD_MISO_PIN, (int)SD_MOSI_PIN);
    Logger::info(kLogCategory, "[SD] CS pin = GPIO%d", (int)SD_CS_PIN);

    // Passive presence hint: with the module's CS held low (module pull-down),
    // the line reads LOW; a floating/no-connection reads HIGH via the internal
    // pull-up. Informational only - SD.begin() below is the authoritative test.
    pinMode(SD_CS_PIN, INPUT_PULLUP);
    const int csIdle = digitalRead(SD_CS_PIN);
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    Logger::info(kLogCategory, "[SD] CS line = %s",
        (csIdle == LOW) ? "LOW (module pull present)" :
        (csIdle == HIGH) ? "HIGH (floating/no module)" : "UNKNOWN");

    // Bounded, non-blocking-for-boot retry loop (runs on the background task).
    // A present card sometimes needs a short warm-up; a missing card fails
    // fast on every attempt. The main boot path is never blocked.
    bool ok = false;
    uint8_t attempts = 0;
    constexpr uint8_t kMaxAttempts = 3;
    for (; attempts < kMaxAttempts; ++attempts) {
        const uint32_t sdT0 = millis();
        ok = SD.begin(SD_CS_PIN, SPI, SD_SPI_FREQUENCY_HZ);
        Logger::info(kLogCategory,
            "[SD] begin() attempt %u: %s in %u ms",
            (unsigned)(attempts + 1), ok ? "OK" : "fail",
            (unsigned)(millis() - sdT0));
        if (ok) break;
        if (attempts + 1 < kMaxAttempts) vTaskDelay(120 / portTICK_PERIOD_MS);
    }

    if (!ok)
    {
        const uint8_t type = SD.cardType();
        if (type == 0U) {
            m_sdLastError = "Card not detected on SPI bus (CS=GPIO" +
                String(SD_CS_PIN) + ", check wiring/power)";
        } else {
            m_sdLastError = "SD.begin() failed after " + String(attempts) +
                " attempts (cardType=" + String(type) + ")";
        }
        Logger::warning(kLogCategory, "[SD] ERROR: %s", m_sdLastError.c_str());
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    const uint8_t type = SD.cardType();
    const uint64_t capacity = SD.cardSize();
    Logger::info(kLogCategory, "[SD] Card detected");
    Logger::info(kLogCategory, "[SD] Card type = %s",
        sdCardTypeName(type));
    Logger::info(kLogCategory, "[SD] Capacity = %llu MB",
        (unsigned long long)(capacity / (1024ULL * 1024ULL)));
    Logger::info(kLogCategory, "[SD] FAT32 mounted");
    Logger::info(kLogCategory, "[SD] Filesystem OK");

    m_sdMounted = true;
    m_sdLastError = "OK";
    m_lastStatus = StorageStatus::SUCCESS;
    Logger::info(kLogCategory, "SD card mounted");
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::unmountSD() noexcept
{
    if (!m_sdMounted)
    {
        return StorageStatus::SUCCESS;
    }

    SD.end();
    m_sdMounted = false;
    m_lastStatus = StorageStatus::SUCCESS;
    Logger::info(kLogCategory, "SD card unmounted");
    return StorageStatus::SUCCESS;
}

bool StorageManager::isSPIFFSMounted() const noexcept
{
    return m_spiffsMounted;
}

bool StorageManager::isSDMounted() const noexcept
{
    return m_sdMounted;
}

// ============================================================================
// Directory Operations
// ============================================================================

StorageStatus StorageManager::createDirectory(
    const char* path,
    StorageType storageType) noexcept
{
    if (!path || path[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    // Verify filesystem is mounted
    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    StorageStatus status = validatePath(path);
    if (status != StorageStatus::SUCCESS)
    {
        m_lastStatus = status;
        return status;
    }

    fs::FS& fs = getFileSystem(storageType);

    if (!fs.exists(path))
    {
        if (!fs.mkdir(path))
        {
            Logger::warning(kLogCategory, "Failed to create directory: %s", path);
            m_lastStatus = StorageStatus::ERROR_IO;
            return StorageStatus::ERROR_IO;
        }
    }

    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::removeDirectory(
    const char* path,
    StorageType storageType) noexcept
{
    if (!path || path[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    // Verify filesystem is mounted
    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    fs::FS& fs = getFileSystem(storageType);

    if (!fs.exists(path))
    {
        m_lastStatus = StorageStatus::ERROR_NOT_FOUND;
        return StorageStatus::ERROR_NOT_FOUND;
    }

    if (!fs.rmdir(path))
    {
        Logger::warning(kLogCategory, "Failed to remove directory: %s", path);
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::listDirectory(
    const char* path,
    std::vector<FileInfo>& entries,
    StorageType storageType) noexcept
{
    if (!path || path[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    // Verify filesystem is mounted
    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    fs::FS& fs = getFileSystem(storageType);

    if (!fs.exists(path))
    {
        m_lastStatus = StorageStatus::ERROR_NOT_FOUND;
        return StorageStatus::ERROR_NOT_FOUND;
    }

    File dir = fs.open(path);
    if (!dir)
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    entries.clear();

    File file = dir.openNextFile();
    while (file)
    {
        FileInfo info;
        info.name = String(file.name());
        info.size = file.size();
        info.isDirectory = file.isDirectory();
        info.modified = 0;

        entries.push_back(info);
        file = dir.openNextFile();
    }

    dir.close();
    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

// ============================================================================
// File Query Operations
// ============================================================================

bool StorageManager::fileExists(
    const char* path,
    StorageType storageType) const noexcept
{
    if (!path || path[0] == '\0')
    {
        return false;
    }

    fs::FS& fs = getFileSystem(storageType);
    return fs.exists(path);
}

size_t StorageManager::getFileSize(
    const char* path,
    StorageType storageType) const noexcept
{
    if (!path || path[0] == '\0')
    {
        return 0;
    }

    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        return 0;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        return 0;
    }

    fs::FS& fs = getFileSystem(storageType);


    File f = fs.open(path);
    if (!f) return 0;
    size_t size = f.size();
    f.close();
    return size;
}

size_t StorageManager::getTotalSpace(StorageType storageType) const noexcept
{
    if (storageType == StorageType::SPIFFS && m_spiffsMounted)
    {
        return SPIFFS.totalBytes();
    }
    else if (storageType == StorageType::SD_CARD && m_sdMounted)
    {
        return SD.totalBytes();
    }
    return 0;
}

size_t StorageManager::getFreeSpace(StorageType storageType) const noexcept
{
    if (storageType == StorageType::SPIFFS && m_spiffsMounted)
    {
        return SPIFFS.totalBytes() - SPIFFS.usedBytes();
    }
    else if (storageType == StorageType::SD_CARD && m_sdMounted)
    {
        return SD.totalBytes() - SD.usedBytes();
    }
    return 0;
}

// ============================================================================
// File Operations
// ============================================================================

StorageStatus StorageManager::createFile(
    const char* path,
    StorageType storageType) noexcept
{
    if (!path || path[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    // Verify filesystem is mounted
    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    fs::FS& fs = getFileSystem(storageType);

    if (fs.exists(path))
    {
        m_lastStatus = StorageStatus::ERROR_EXISTS;
        return StorageStatus::ERROR_EXISTS;
    }

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    file.close();
    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::deleteFile(
    const char* path,
    StorageType storageType) noexcept
{
    return deleteFileInternal(path, storageType);
}

StorageStatus StorageManager::readFile(
    const char* path,
    uint8_t* buffer,
    size_t bufferSize,
    size_t& bytesRead,
    StorageType storageType) noexcept
{
    bytesRead = 0;

    if (!path || path[0] == '\0' || !buffer || bufferSize == 0)
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    // Verify filesystem is mounted
    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    fs::FS& fs = getFileSystem(storageType);

    if (!fs.exists(path))
    {
        m_lastStatus = StorageStatus::ERROR_NOT_FOUND;
        return StorageStatus::ERROR_NOT_FOUND;
    }

    File file = fs.open(path, FILE_READ);
    if (!file)
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    size_t toRead = (file.size() < bufferSize) ? file.size() : bufferSize;
    bytesRead = file.read(buffer, toRead);
    file.close();

    if (bytesRead != toRead)
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::readFile(
    const char* path,
    String& content,
    StorageType storageType) noexcept
{
    if (!path || path[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    // Verify filesystem is mounted
    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    fs::FS& fs = getFileSystem(storageType);

    if (!fs.exists(path))
    {
        m_lastStatus = StorageStatus::ERROR_NOT_FOUND;
        return StorageStatus::ERROR_NOT_FOUND;
    }

    File file = fs.open(path, FILE_READ);
    if (!file)
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    content.clear();
    uint8_t buffer[kChunkSize];

    while (file.available())
    {
        size_t bytesRead = file.read(buffer, sizeof(buffer));
        for (size_t i = 0; i < bytesRead; ++i)
{
    content += static_cast<char>(buffer[i]);
}
    }

    file.close();
    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::writeFile(
    const char* path,
    const uint8_t* data,
    size_t dataSize,
    StorageType storageType) noexcept
{
    if (!path || path[0] == '\0' || !data || dataSize == 0)
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    // Verify filesystem is mounted
    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    fs::FS& fs = getFileSystem(storageType);

    // Account for existing file that still exists during atomic swap
    size_t existingSize = 0;
    if (fs.exists(path))
    {
        File f = fs.open(path, FILE_READ);
        if (f)
        {
            existingSize = f.size();
            f.close();
        }
    }

    StorageStatus status = checkSpace(dataSize + existingSize, storageType);
    if (status != StorageStatus::SUCCESS)
    {
        m_lastStatus = status;
        return status;
    }

    // Write to temporary file for atomicity
    String tmpPath = String(path) + ".tmp";
    if (fs.exists(tmpPath.c_str()))
    {
        fs.remove(tmpPath.c_str());
    }

    File file = fs.open(tmpPath.c_str(), FILE_WRITE);
    if (!file)
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    size_t offset = 0;
    while (offset < dataSize)
    {
        size_t chunkSize = (dataSize - offset < kChunkSize) ? (dataSize - offset) : kChunkSize;
        size_t written = file.write(&data[offset], chunkSize);

        if (written != chunkSize)
        {
            file.close();
            fs.remove(tmpPath.c_str());
            m_lastStatus = StorageStatus::ERROR_IO;
            return StorageStatus::ERROR_IO;
        }

        offset += chunkSize;
    }

    file.close();

    // Atomically replace the original file
    if (fs.exists(path) && !fs.remove(path))
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    if (!fs.rename(tmpPath.c_str(), path))
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::writeFile(
    const char* path,
    const String& content,
    StorageType storageType) noexcept
{
    return writeFile(path, reinterpret_cast<const uint8_t*>(content.c_str()), content.length(), storageType);
}

StorageStatus StorageManager::appendFile(
    const char* path,
    const uint8_t* data,
    size_t dataSize,
    StorageType storageType) noexcept
{
    if (!path || path[0] == '\0' || !data || dataSize == 0)
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    // Verify filesystem is mounted
    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    StorageStatus status = checkSpace(dataSize, storageType);
    if (status != StorageStatus::SUCCESS)
    {
        m_lastStatus = status;
        return status;
    }

    fs::FS& fs = getFileSystem(storageType);

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    size_t bytesWritten = 0;
    size_t offset = 0;

    while (offset < dataSize)
    {
        size_t chunkSize = (dataSize - offset < kChunkSize) ? (dataSize - offset) : kChunkSize;
        size_t written = file.write(&data[offset], chunkSize);

        if (written != chunkSize)
        {
            file.close();
            m_lastStatus = StorageStatus::ERROR_IO;
            return StorageStatus::ERROR_IO;
        }

        bytesWritten += written;
        offset += chunkSize;
    }

    file.close();
    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::appendFile(
    const char* path,
    const String& content,
    StorageType storageType) noexcept
{
    return appendFile(path, reinterpret_cast<const uint8_t*>(content.c_str()), content.length(), storageType);
}

StorageStatus StorageManager::renameFile(
    const char* oldPath,
    const char* newPath,
    StorageType storageType) noexcept
{
    if (!oldPath || oldPath[0] == '\0' || !newPath || newPath[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    // Verify filesystem is mounted
    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    fs::FS& fs = getFileSystem(storageType);

    if (!fs.exists(oldPath))
    {
        m_lastStatus = StorageStatus::ERROR_NOT_FOUND;
        return StorageStatus::ERROR_NOT_FOUND;
    }

    if (fs.exists(newPath))
    {
        m_lastStatus = StorageStatus::ERROR_EXISTS;
        return StorageStatus::ERROR_EXISTS;
    }

    if (!fs.rename(oldPath, newPath))
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::copyFile(
    const char* sourcePath,
    const char* destPath,
    StorageType storageType) noexcept
{
    return copyFileInternal(sourcePath, destPath, storageType);
}

StorageStatus StorageManager::moveFile(
    const char* sourcePath,
    const char* destPath,
    StorageType storageType) noexcept
{
    StorageStatus status = copyFileInternal(sourcePath, destPath, storageType);
    if (status != StorageStatus::SUCCESS)
    {
        return status;
    }

    return deleteFileInternal(sourcePath, storageType);
}

// ============================================================================
// Format Operations
// ============================================================================

StorageStatus StorageManager::formatSPIFFS() noexcept
{
    Logger::warning(kLogCategory, "Formatting SPIFFS (destructive)");

    unmountSPIFFS();

    if (!SPIFFS.format())
    {
        Logger::error(kLogCategory, "SPIFFS format failed");
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    if (mountSPIFFS() != StorageStatus::SUCCESS)
    {
        Logger::error(kLogCategory, "SPIFFS remount after format failed");
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    Logger::info(kLogCategory, "SPIFFS formatted successfully");
    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::formatSD() noexcept
{
    Logger::warning(kLogCategory, "Formatting SD card (destructive)");

    unmountSD();

#ifdef SD_HAS_FORMAT

    if (!SD.format()) {
        Logger::warning(kLogCategory, "SD format failed");
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    if (mountSD() != StorageStatus::SUCCESS) {
        Logger::error(kLogCategory, "SD card remount after format failed");
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    Logger::info(kLogCategory, "SD card formatted successfully");
    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;

#else

    Logger::warning(kLogCategory,
        "SD formatting is not supported on this ESP32 core.");
    m_lastStatus = StorageStatus::ERROR_IO;
    return StorageStatus::ERROR_IO;

#endif
}

// ============================================================================
// Application-Specific Operations
// ============================================================================

StorageStatus StorageManager::saveConversation(
    const char* conversationId,
    const String& content) noexcept
{
    if (!conversationId || conversationId[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    String path = String(kConversationPath) + "/" + String(conversationId) + ".json";
    return writeFile(path.c_str(), content, StorageType::SPIFFS);
}

StorageStatus StorageManager::loadConversation(
    const char* conversationId,
    String& content) noexcept
{
    if (!conversationId || conversationId[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    String path = String(kConversationPath) + "/" + String(conversationId) + ".json";
    return readFile(path.c_str(), content, StorageType::SPIFFS);
}

StorageStatus StorageManager::saveAudio(
    const char* recordingId,
    const uint8_t* audioData,
    size_t audioSize) noexcept
{
    if (!recordingId || recordingId[0] == '\0' || !audioData || audioSize == 0)
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    String path = String(kAudioPath) + "/" + String(recordingId) + ".wav";
    return writeFile(path.c_str(), audioData, audioSize, StorageType::SD_CARD);
}

StorageStatus StorageManager::loadAudio(
    const char* recordingId,
    uint8_t* audioData,
    size_t bufferSize,
    size_t& bytesRead) noexcept
{
    if (!recordingId || recordingId[0] == '\0' || !audioData || bufferSize == 0)
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    String path = String(kAudioPath) + "/" + String(recordingId) + ".wav";
    return readFile(path.c_str(), audioData, bufferSize, bytesRead, StorageType::SD_CARD);
}

StorageStatus StorageManager::saveReminder(
    const char* reminderId,
    const String& content) noexcept
{
    if (!reminderId || reminderId[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    String path = String(kReminderPath) + "/" + String(reminderId) + ".json";
    return writeFile(path.c_str(), content, StorageType::SPIFFS);
}

StorageStatus StorageManager::loadReminder(
    const char* reminderId,
    String& content) noexcept
{
    if (!reminderId || reminderId[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    String path = String(kReminderPath) + "/" + String(reminderId) + ".json";
    return readFile(path.c_str(), content, StorageType::SPIFFS);
}

StorageStatus StorageManager::backupSettings(
    const char* backupId,
    const uint8_t* settingsData,
    size_t settingsSize) noexcept
{
    if (!backupId || backupId[0] == '\0' || !settingsData || settingsSize == 0)
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    String path = String(kBackupPath) + "/" + String(backupId) + ".bak";
    return writeFile(path.c_str(), settingsData, settingsSize, StorageType::SPIFFS);
}

StorageStatus StorageManager::restoreSettings(
    const char* backupId,
    uint8_t* settingsData,
    size_t bufferSize,
    size_t& bytesRead) noexcept
{
    if (!backupId || backupId[0] == '\0' || !settingsData || bufferSize == 0)
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    String path = String(kBackupPath) + "/" + String(backupId) + ".bak";
    return readFile(path.c_str(), settingsData, bufferSize, bytesRead, StorageType::SPIFFS);
}

StorageStatus StorageManager::saveLog(
    const String& log) noexcept
{
    if (log.length() == 0)
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    String path = String(kLogPath) + "/log_entry.log";
    return appendFile(path.c_str(), log, StorageType::SPIFFS);
}

StorageStatus StorageManager::clearLogs() noexcept
{
    if (!m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    std::vector<FileInfo> entries;
    StorageStatus status = listDirectory(kLogPath, entries, StorageType::SPIFFS);

    if (status != StorageStatus::SUCCESS)
    {
        return status;
    }

    for (const auto& entry : entries)
    {
        if (!entry.isDirectory)
        {
            String path = String(kLogPath) + "/" + entry.name;
            deleteFileInternal(path.c_str(), StorageType::SPIFFS);
        }
    }

    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

bool StorageManager::backupExists(
    const char* backupId) const noexcept
{
    if (!backupId || backupId[0] == '\0')
    {
        return false;
    }

    String path = String(kBackupPath) + "/" + String(backupId) + ".bak";
    return fileExists(path.c_str(), StorageType::SPIFFS);
}

StorageStatus StorageManager::cleanupOldConversations(
    unsigned long maxAge) noexcept
{
    if (!m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    std::vector<FileInfo> entries;
    StorageStatus status = listDirectory(kConversationPath, entries, StorageType::SPIFFS);

    if (status != StorageStatus::SUCCESS)
    {
        return status;
    }

    // Safe cleanup: only delete if we have valid timestamps
    for (const auto& entry : entries)
    {
        if (!entry.isDirectory && entry.modified > 0)
        {
            unsigned long now = time(nullptr);
            if ((now - entry.modified) > maxAge)
            {
                String path = String(kConversationPath) + "/" + entry.name;
                deleteFileInternal(path.c_str(), StorageType::SPIFFS);
            }
        }
    }

    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::cleanupOldAudio(
    unsigned long maxAge) noexcept
{
    if (!m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    std::vector<FileInfo> entries;
    StorageStatus status = listDirectory(kAudioPath, entries, StorageType::SD_CARD);

    if (status != StorageStatus::SUCCESS)
    {
        return status;
    }

    // Safe cleanup: only delete if we have valid timestamps
    for (const auto& entry : entries)
    {
        if (!entry.isDirectory && entry.modified > 0)
        {
            unsigned long now = time(nullptr);
            if ((now - entry.modified) > maxAge)
            {
                String path = String(kAudioPath) + "/" + entry.name;
                deleteFileInternal(path.c_str(), StorageType::SD_CARD);
            }
        }
    }

    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

bool StorageManager::isInitialized() const noexcept
{
    return m_initialized;
}

bool StorageManager::isHealthy() const noexcept
{
    return m_storageHealthy;
}

StorageStatus StorageManager::getLastStatus() const noexcept
{
    return m_lastStatus;
}

StorageStatus StorageManager::getStatistics(
    StorageType storageType,
    size_t& totalBytes,
    size_t& usedBytes,
    size_t& freeBytes) noexcept
{
    totalBytes = getTotalSpace(storageType);
    freeBytes = getFreeSpace(storageType);
    usedBytes = totalBytes - freeBytes;

    if (totalBytes == 0)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

// ============================================================================
// SD Card Diagnostics
// ============================================================================

std::uint8_t StorageManager::getSDCardType() const noexcept
{
    if (!m_sdMounted) return 0U;
    return static_cast<std::uint8_t>(SD.cardType());
}

std::uint64_t StorageManager::getSDCardSize() const noexcept
{
    if (!m_sdMounted) return 0U;
    return static_cast<std::uint64_t>(SD.cardSize());
}

const char* StorageManager::getSDCardTypeName() const noexcept
{
    return sdCardTypeName(getSDCardType());
}

const char* StorageManager::getSDLastError() const noexcept
{
    return m_sdLastError.c_str();
}

std::uint32_t StorageManager::getSDSpiFrequencyHz() const noexcept
{
    return static_cast<std::uint32_t>(SD_SPI_FREQUENCY_HZ);
}

StorageStatus StorageManager::runSDSpeedTest(
    float& readBytesPerSec,
    float& writeBytesPerSec) noexcept
{
    readBytesPerSec = 0.0f;
    writeBytesPerSec = 0.0f;

    if (!m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    // Modest 64 KiB probe - enough to be meaningful, small enough to keep the
    // request snappy and the card's flash wear negligible.
    constexpr size_t kProbeBytes = 64U * 1024U;
    constexpr const char* kProbePath = "/.sd_diag_probe.bin";
    std::vector<uint8_t> pattern(kProbeBytes, 0xA5);
    for (size_t i = 0; i < pattern.size(); ++i) {
        pattern[i] = static_cast<uint8_t>(i * 7U + 3U);
    }

    // --- write pass ---
    const uint32_t w0 = millis();
    StorageStatus st = writeFile(kProbePath, pattern.data(), pattern.size(),
        StorageType::SD_CARD);
    const uint32_t wElapsed = millis() - w0;
    if (st != StorageStatus::SUCCESS)
    {
        m_sdLastError = "Speed test write failed";
        m_lastStatus = st;
        return st;
    }
    if (wElapsed > 0U)
    {
        writeBytesPerSec = static_cast<float>(kProbeBytes) *
            (1000.0f / static_cast<float>(wElapsed));
    }

    // --- read pass ---
    std::vector<uint8_t> readback(kProbeBytes);
    size_t bytesRead = 0;
    const uint32_t r0 = millis();
    st = readFile(kProbePath, readback.data(), readback.size(), bytesRead,
        StorageType::SD_CARD);
    const uint32_t rElapsed = millis() - r0;
    deleteFile(kProbePath, StorageType::SD_CARD);

    if (st != StorageStatus::SUCCESS || bytesRead != kProbeBytes)
    {
        m_sdLastError = "Speed test read failed";
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }
    if (rElapsed > 0U)
    {
        readBytesPerSec = static_cast<float>(kProbeBytes) *
            (1000.0f / static_cast<float>(rElapsed));
    }

    m_sdLastError = "OK";
    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

// ============================================================================
// Private Helpers
// ============================================================================

StorageStatus StorageManager::validatePath(const char* path) const noexcept
{
    if (!path || path[0] != '/')
    {
        return StorageStatus::ERROR_INVALID;
    }

    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::checkSpace(
    size_t requiredBytes,
    StorageType storageType) noexcept
{
    size_t freeSpace = getFreeSpace(storageType);

    if (freeSpace < requiredBytes)
    {
        Logger::warning(kLogCategory, "Insufficient space: required %u, available %u",
            static_cast<unsigned int>(requiredBytes),
            static_cast<unsigned int>(freeSpace));
        return StorageStatus::ERROR_NO_SPACE;
    }

    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::recoverFromTemp(
    const char* path,
    StorageType storageType) noexcept
{
    if (!path || path[0] == '\0')
    {
        return StorageStatus::ERROR_INVALID;
    }

    fs::FS& fs = getFileSystem(storageType);
    String tmpPath = String(path) + ".tmp";

    bool origExists = fs.exists(path);
    bool tmpExists = fs.exists(tmpPath.c_str());

    if (!tmpExists)
    {
        return StorageStatus::SUCCESS;
    }

    if (origExists)
    {
        fs.remove(path);
    }

    if (!fs.rename(tmpPath.c_str(), path))
    {
        Logger::error(kLogCategory, "Failed to recover '%s' from temp", path);
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    Logger::warning(kLogCategory, "Recovered '%s' from temp file", path);
    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::recoverOrphanedTmps() noexcept
{
    if (!m_spiffsMounted)
    {
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    // Scan known directories for orphaned .tmp files
    const char* scanDirs[] = {
        "/",
        kConversationPath,
        kReminderPath,
        kBackupPath
    };

    for (auto dirPath : scanDirs)
    {
        std::vector<FileInfo> entries;
        if (listDirectory(dirPath, entries, StorageType::SPIFFS) != StorageStatus::SUCCESS)
        {
            continue;
        }

        for (const auto& entry : entries)
        {
            if (!entry.isDirectory && entry.name.endsWith(".tmp"))
            {
                // Derive original path: strip .tmp extension, reconstruct full path
                String nameNoExt = entry.name.substring(0, entry.name.length() - 4);
                String origPath = String(dirPath) + "/" + nameNoExt;
                if (String(dirPath) == "/")
                {
                    origPath = "/" + nameNoExt;
                }

                Logger::warning(kLogCategory, "Found orphaned temp file: %s", entry.name.c_str());
                recoverFromTemp(origPath.c_str(), StorageType::SPIFFS);
            }
        }
    }

    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

String StorageManager::getStoragePath(
    const char* filename,
    StorageType storageType) noexcept
{
    (void)storageType;
    return String(filename);
}

StorageStatus StorageManager::copyFileInternal(
    const char* sourcePath,
    const char* destPath,
    StorageType storageType) noexcept
{
    if (!sourcePath || sourcePath[0] == '\0' || !destPath || destPath[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    // Verify filesystem is mounted
    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    fs::FS& fs = getFileSystem(storageType);

    if (!fs.exists(sourcePath))
    {
        m_lastStatus = StorageStatus::ERROR_NOT_FOUND;
        return StorageStatus::ERROR_NOT_FOUND;
    }

    if (fs.exists(destPath))
    {
        m_lastStatus = StorageStatus::ERROR_EXISTS;
        return StorageStatus::ERROR_EXISTS;
    }

    File source = fs.open(sourcePath, FILE_READ);
    if (!source)
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    File dest = fs.open(destPath, FILE_WRITE);
    if (!dest)
    {
        source.close();
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    uint8_t buffer[kChunkSize];
    size_t bytesRead = 0;

    while ((bytesRead = source.read(buffer, sizeof(buffer))) > 0)
    {
        if (dest.write(buffer, bytesRead) != bytesRead)
        {
            source.close();
            dest.close();
            m_lastStatus = StorageStatus::ERROR_IO;
            return StorageStatus::ERROR_IO;
        }
    }

    source.close();
    dest.close();
    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}

StorageStatus StorageManager::deleteFileInternal(
    const char* path,
    StorageType storageType) noexcept
{
    if (!path || path[0] == '\0')
    {
        m_lastStatus = StorageStatus::ERROR_INVALID;
        return StorageStatus::ERROR_INVALID;
    }

    // Verify filesystem is mounted
    if (storageType == StorageType::SPIFFS && !m_spiffsMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    if (storageType == StorageType::SD_CARD && !m_sdMounted)
    {
        m_lastStatus = StorageStatus::ERROR_NOT_MOUNTED;
        return StorageStatus::ERROR_NOT_MOUNTED;
    }

    fs::FS& fs = getFileSystem(storageType);

    if (!fs.exists(path))
    {
        m_lastStatus = StorageStatus::ERROR_NOT_FOUND;
        return StorageStatus::ERROR_NOT_FOUND;
    }

    if (!fs.remove(path))
    {
        m_lastStatus = StorageStatus::ERROR_IO;
        return StorageStatus::ERROR_IO;
    }

    m_lastStatus = StorageStatus::SUCCESS;
    return StorageStatus::SUCCESS;
}