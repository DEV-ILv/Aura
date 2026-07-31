#ifndef AURA_CAPABILITY_MANAGER_H
#define AURA_CAPABILITY_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "service.h"

enum class Capability : uint8_t {
    // Display
    DISPLAY_OLED,
    DISPLAY_LCD,
    DISPLAY_TOUCH,
    DISPLAY_COLOR,
    DISPLAY_MONOCHROME,

    // Audio
    AUDIO_INPUT,
    AUDIO_OUTPUT,
    AUDIO_I2S,

    // Connectivity
    WIFI_STATION,
    WIFI_ACCESS_POINT,
    BLUETOOTH_CLASSIC,
    BLUETOOTH_BLE,
    ESPNOW,
    ETHERNET,

    // Storage
    STORAGE_SD,
    STORAGE_SPIFFS,
    STORAGE_LITTLEFS,
    STORAGE_NVS,

    // Sensors
    SENSOR_TOUCH,
    SENSOR_TEMPERATURE,
    SENSOR_HUMIDITY,
    SENSOR_MOTION,
    SENSOR_LIGHT,

    // Input
    INPUT_BUTTON,
    INPUT_ROTARY_ENCODER,
    INPUT_KEYBOARD,
    INPUT_MOUSE,
    INPUT_VOICE,
    INPUT_GESTURE,

    // AI
    AI_GEMINI,
    AI_OPENAI,
    AI_CLAUDE,
    AI_LOCAL_LLM,
    AI_TINY,

    // Platform
    PLATFORM_ESP32,
    PLATFORM_ESP32_S3,
    PLATFORM_RASPBERRY_PI,
    PLATFORM_LINUX,
    PLATFORM_WINDOWS,
    PLATFORM_ANDROID,
    PLATFORM_IOS,
    PLATFORM_WEB,

    // Power
    POWER_BATTERY,
    POWER_USB,
    POWER_SOLAR,

    // Advanced
    CAMERA,
    GPS,
    VIBRATOR,
    NFC,
    RADIO,

    COUNT
};

struct CapabilityInfo {
    Capability cap;
    bool available;
    String description;
    String version;
    uint8_t confidence;  // 0-100

    CapabilityInfo() noexcept : cap(Capability::COUNT), available(false), confidence(0) {}
};

class CapabilityManager : public Service {
public:
    CapabilityManager() noexcept;
    ~CapabilityManager() noexcept;

    bool Initialize() noexcept override;
    void Update() noexcept override;

    // Query
    bool Has(Capability cap) const noexcept;
    bool HasAll(std::initializer_list<Capability> caps) const noexcept;
    bool HasAny(std::initializer_list<Capability> caps) const noexcept;

    const CapabilityInfo& GetInfo(Capability cap) const noexcept;
    String GetName(Capability cap) const noexcept;
    std::vector<Capability> GetAvailable() const noexcept;
    std::vector<Capability> GetUnavailable() const noexcept;
    size_t AvailableCount() const noexcept;
    size_t TotalCount() const noexcept;

    // Registration (for platform detection or runtime discovery)
    void Register(Capability cap, bool available, const String& description = "",
                  const String& version = "", uint8_t confidence = 100) noexcept;

    // Events
    void HandleEvent(const String& eventType, const String& eventData) noexcept override;

    static constexpr const char* kStaticName = "CapabilityManager";

private:
    void DetectAll() noexcept;
    void DetectDisplay() noexcept;
    void DetectAudio() noexcept;
    void DetectConnectivity() noexcept;
    void DetectStorage() noexcept;
    void DetectSensors() noexcept;
    void DetectAI() noexcept;
    void DetectPower() noexcept;
    void EmitCapabilityEvents() noexcept;

    static constexpr const char* kLogCategory = "CapabilityManager";
    CapabilityInfo m_capabilities[static_cast<size_t>(Capability::COUNT)];
};

extern CapabilityManager capabilityManager;

#endif