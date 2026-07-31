#include "logger.h"

#include <cstdarg>
#include <cstdio>

namespace {

constexpr std::size_t kLogBufferSize = 256U;
constexpr uint32_t kSerialReadyTimeoutMs = 500U;
constexpr const char* kUnknownCategory = "Unknown";
constexpr const char* kFormattingError = "<formatting error>";

StaticSemaphore_t gLogMutexStorage;
portMUX_TYPE gInitializationLock = portMUX_INITIALIZER_UNLOCKED;
bool gLoggerInitialized = false;

using LevelToStringFunction = const char* (*)(LogLevel);

SemaphoreHandle_t getMutexSnapshot(SemaphoreHandle_t* const mutex)
{
    if (mutex == nullptr) {
        return nullptr;
    }

    portENTER_CRITICAL(&gInitializationLock);
    const SemaphoreHandle_t mutexSnapshot = *mutex;
    portEXIT_CRITICAL(&gInitializationLock);

    return mutexSnapshot;
}

void writeFormattedLog(
    const SemaphoreHandle_t mutex,
    const LogLevel level,
    const char* const category,
    const char* const format,
    va_list arguments,
    const LevelToStringFunction levelToString)
{
    if (mutex == nullptr || format == nullptr ||
        static_cast<int>(level) < static_cast<int>(CURRENT_LOG_LEVEL)) {
        return;
    }

    char message[kLogBufferSize] = {};
    const int formatResult =
        vsnprintf(message, sizeof(message), format, arguments);

    if (formatResult < 0) {
        snprintf(message, sizeof(message), "%s", kFormattingError);
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    if (gLoggerInitialized && Serial) {
        const char* const safeCategory =
            (category != nullptr) ? category : kUnknownCategory;

        Serial.printf(
            "[%08lu ms] [%s] [%s] %s\n",
            static_cast<unsigned long>(millis()),
            levelToString(level),
            safeCategory,
            message);
    }

    xSemaphoreGive(mutex);
}

}  // namespace

SemaphoreHandle_t Logger::xLogMutex = nullptr;

bool Logger::initialize()
{
    portENTER_CRITICAL(&gInitializationLock);

    if (xLogMutex == nullptr) {
        xLogMutex = xSemaphoreCreateMutexStatic(&gLogMutexStorage);
    }

    const SemaphoreHandle_t mutex = xLogMutex;

    portEXIT_CRITICAL(&gInitializationLock);

    if (mutex == nullptr || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    if (!gLoggerInitialized) {
        Serial.begin(SERIAL_BAUD_RATE);

        const uint32_t startTime = millis();
        while (!Serial &&
               (millis() - startTime) < kSerialReadyTimeoutMs) {
            delay(1);
        }

        gLoggerInitialized = true;
    }

    const bool initialized = gLoggerInitialized;
    xSemaphoreGive(mutex);

    return initialized;
}

void Logger::log(
    LogLevel level,
    const char* category,
    const char* format,
    ...)
{
    va_list arguments;
    va_start(arguments, format);

    writeFormattedLog(
        getMutexSnapshot(&xLogMutex),
        level,
        category,
        format,
        arguments,
        &Logger::levelToString);

    va_end(arguments);
}

void Logger::debug(
    const char* category,
    const char* format,
    ...)
{
    va_list arguments;
    va_start(arguments, format);

    writeFormattedLog(
        getMutexSnapshot(&xLogMutex),
        LOG_DEBUG,
        category,
        format,
        arguments,
        &Logger::levelToString);

    va_end(arguments);
}

void Logger::info(
    const char* category,
    const char* format,
    ...)
{
    va_list arguments;
    va_start(arguments, format);

    writeFormattedLog(
        getMutexSnapshot(&xLogMutex),
        LOG_INFO,
        category,
        format,
        arguments,
        &Logger::levelToString);

    va_end(arguments);
}

void Logger::warning(
    const char* category,
    const char* format,
    ...)
{
    va_list arguments;
    va_start(arguments, format);

    writeFormattedLog(
        getMutexSnapshot(&xLogMutex),
        LOG_WARNING,
        category,
        format,
        arguments,
        &Logger::levelToString);

    va_end(arguments);
}

void Logger::error(
    const char* category,
    const char* format,
    ...)
{
    va_list arguments;
    va_start(arguments, format);

    writeFormattedLog(
        getMutexSnapshot(&xLogMutex),
        LOG_ERROR,
        category,
        format,
        arguments,
        &Logger::levelToString);

    va_end(arguments);
}

const char* Logger::levelToString(LogLevel level)
{
    switch (level) {
        case LOG_DEBUG:
            return "DEBUG";

        case LOG_INFO:
            return "INFO";

        case LOG_WARNING:
            return "WARNING";

        case LOG_ERROR:
            return "ERROR";

        default:
            return "UNKNOWN";
    }
}