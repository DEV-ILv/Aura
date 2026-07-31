#include "capability_manager.h"
#include <WiFi.h>
#include <Wire.h>

CapabilityManager capabilityManager;

CapabilityManager::CapabilityManager() noexcept
    : Service(kStaticName, BootPriority::CRITICAL) {
    for (auto& c : m_capabilities) {
        c.cap = static_cast<Capability>(&c - m_capabilities);
    }
}

CapabilityManager::~CapabilityManager() noexcept = default;

bool CapabilityManager::Initialize() noexcept {
    if (GetState() != ServiceState::UNINITIALIZED) return true;
    SetState(ServiceState::INITIALIZING);
    DetectAll();
    EmitCapabilityEvents();
    SetState(ServiceState::INITIALIZED);
    LOG_INFO(kLogCategory, "%d capabilities detected (%d available)",
             TotalCount(), AvailableCount());
    return true;
}

void CapabilityManager::Update() noexcept {
    // Runtime capability changes (e.g., SD card hotplug) can be detected here
}

bool CapabilityManager::Has(Capability cap) const noexcept {
    return m_capabilities[static_cast<size_t>(cap)].available;
}

bool CapabilityManager::HasAll(std::initializer_list<Capability> caps) const noexcept {
    for (auto cap : caps) {
        if (!m_capabilities[static_cast<size_t>(cap)].available) return false;
    }
    return true;
}

bool CapabilityManager::HasAny(std::initializer_list<Capability> caps) const noexcept {
    for (auto cap : caps) {
        if (m_capabilities[static_cast<size_t>(cap)].available) return true;
    }
    return false;
}

const CapabilityInfo& CapabilityManager::GetInfo(Capability cap) const noexcept {
    return m_capabilities[static_cast<size_t>(cap)];
}

String CapabilityManager::GetName(Capability cap) const noexcept {
    switch (cap) {
        case Capability::DISPLAY_OLED:       return "OLED Display";
        case Capability::DISPLAY_LCD:        return "LCD Display";
        case Capability::DISPLAY_TOUCH:      return "Touchscreen";
        case Capability::DISPLAY_COLOR:      return "Color Display";
        case Capability::DISPLAY_MONOCHROME: return "Monochrome Display";
        case Capability::AUDIO_INPUT:        return "Audio Input";
        case Capability::AUDIO_OUTPUT:       return "Audio Output";
        case Capability::AUDIO_I2S:          return "I2S Audio";
        case Capability::WIFI_STATION:       return "WiFi Station";
        case Capability::WIFI_ACCESS_POINT:  return "WiFi Access Point";
        case Capability::BLUETOOTH_CLASSIC:  return "Bluetooth Classic";
        case Capability::BLUETOOTH_BLE:      return "Bluetooth BLE";
        case Capability::ESPNOW:             return "ESP-NOW";
        case Capability::ETHERNET:           return "Ethernet";
        case Capability::STORAGE_SD:         return "SD Card";
        case Capability::STORAGE_SPIFFS:     return "SPIFFS";
        case Capability::STORAGE_LITTLEFS:   return "LittleFS";
        case Capability::STORAGE_NVS:        return "NVS";
        case Capability::SENSOR_TOUCH:       return "Touch Sensor";
        case Capability::SENSOR_TEMPERATURE: return "Temperature Sensor";
        case Capability::SENSOR_HUMIDITY:    return "Humidity Sensor";
        case Capability::SENSOR_MOTION:      return "Motion Sensor";
        case Capability::SENSOR_LIGHT:       return "Light Sensor";
        case Capability::INPUT_BUTTON:       return "Button Input";
        case Capability::INPUT_ROTARY_ENCODER: return "Rotary Encoder";
        case Capability::INPUT_KEYBOARD:     return "Keyboard";
        case Capability::INPUT_MOUSE:        return "Mouse";
        case Capability::INPUT_VOICE:        return "Voice Input";
        case Capability::INPUT_GESTURE:      return "Gesture Input";
        case Capability::AI_GEMINI:          return "Gemini AI";
        case Capability::AI_OPENAI:          return "OpenAI";
        case Capability::AI_CLAUDE:          return "Claude AI";
        case Capability::AI_LOCAL_LLM:       return "Local LLM";
        case Capability::AI_TINY:            return "Tiny AI";
        case Capability::PLATFORM_ESP32:     return "ESP32";
        case Capability::PLATFORM_ESP32_S3:  return "ESP32-S3";
        case Capability::PLATFORM_RASPBERRY_PI: return "Raspberry Pi";
        case Capability::PLATFORM_LINUX:     return "Linux";
        case Capability::PLATFORM_WINDOWS:   return "Windows";
        case Capability::PLATFORM_ANDROID:   return "Android";
        case Capability::PLATFORM_IOS:       return "iOS";
        case Capability::PLATFORM_WEB:       return "Web";
        case Capability::POWER_BATTERY:      return "Battery";
        case Capability::POWER_USB:          return "USB Power";
        case Capability::POWER_SOLAR:        return "Solar Power";
        case Capability::CAMERA:             return "Camera";
        case Capability::GPS:                return "GPS";
        case Capability::VIBRATOR:           return "Vibrator";
        case Capability::NFC:                return "NFC";
        case Capability::RADIO:              return "Radio";
        default:                             return "Unknown";
    }
}

std::vector<Capability> CapabilityManager::GetAvailable() const noexcept {
    std::vector<Capability> result;
    for (const auto& c : m_capabilities) {
        if (c.available) result.push_back(c.cap);
    }
    return result;
}

std::vector<Capability> CapabilityManager::GetUnavailable() const noexcept {
    std::vector<Capability> result;
    for (const auto& c : m_capabilities) {
        if (!c.available) result.push_back(c.cap);
    }
    return result;
}

size_t CapabilityManager::AvailableCount() const noexcept {
    size_t count = 0;
    for (const auto& c : m_capabilities) {
        if (c.available) ++count;
    }
    return count;
}

size_t CapabilityManager::TotalCount() const noexcept {
    return static_cast<size_t>(Capability::COUNT);
}

void CapabilityManager::Register(Capability cap, bool available, const String& description,
                                  const String& version, uint8_t confidence) noexcept {
    auto idx = static_cast<size_t>(cap);
    m_capabilities[idx].available = available;
    if (!description.isEmpty()) m_capabilities[idx].description = description;
    if (!version.isEmpty()) m_capabilities[idx].version = version;
    m_capabilities[idx].confidence = confidence;
}

void CapabilityManager::HandleEvent(const String& eventType, const String& eventData) noexcept {
    Service::HandleEvent(eventType, eventData);
}

void CapabilityManager::DetectAll() noexcept {
    DetectDisplay();
    DetectAudio();
    DetectConnectivity();
    DetectStorage();
    DetectSensors();
    DetectAI();
    DetectPower();
}

void CapabilityManager::DetectDisplay() noexcept {
    // I2C scan for OLED
    Wire.beginTransmission(OLED_ADDRESS);
    bool oledPresent = (Wire.endTransmission() == 0);
    // SPI scan for LCD
    bool lcdPresent = false; // TODO: SPI detect
    bool touchPresent = false; // TODO: Touch controller detect

    Register(Capability::DISPLAY_OLED, oledPresent);
    Register(Capability::DISPLAY_LCD, lcdPresent);
    Register(Capability::DISPLAY_TOUCH, touchPresent);
    Register(Capability::DISPLAY_COLOR, lcdPresent || false);
    Register(Capability::DISPLAY_MONOCHROME, oledPresent);
}

void CapabilityManager::DetectAudio() noexcept {
    Register(Capability::AUDIO_I2S, true, "MAX98357 + INMP441");
    Register(Capability::AUDIO_INPUT, true, "INMP441 I2S Mic");
    Register(Capability::AUDIO_OUTPUT, true, "MAX98357 I2S Speaker");
}

void CapabilityManager::DetectConnectivity() noexcept {
    Register(Capability::WIFI_STATION, true, "ESP32 WiFi");
    Register(Capability::WIFI_ACCESS_POINT, true, "ESP32 SoftAP");
    Register(Capability::BLUETOOTH_CLASSIC, true, "ESP32 Bluetooth");
    Register(Capability::BLUETOOTH_BLE, true, "ESP32 BLE");

    // ESP-NOW: available on all ESP32
    Register(Capability::ESPNOW, true, "ESP-NOW Mesh");
    Register(Capability::ETHERNET, false);
}

void CapabilityManager::DetectStorage() noexcept {
    // SPIFFS available on ESP32
    Register(Capability::STORAGE_SPIFFS, true);

    // SD card: check if CS pin reads correctly
    pinMode(SD_CS_PIN, INPUT_PULLUP);
    bool sdDetected = (digitalRead(SD_CS_PIN) == LOW);
    Register(Capability::STORAGE_SD, sdDetected);

    Register(Capability::STORAGE_NVS, true, "ESP32 Preferences");
    Register(Capability::STORAGE_LITTLEFS, false);
}

void CapabilityManager::DetectSensors() noexcept {
    // Touch sensor is available on ESP32
    Register(Capability::SENSOR_TOUCH, true, "ESP32 Touch", "", 80);
    Register(Capability::SENSOR_TEMPERATURE, false);
    Register(Capability::SENSOR_HUMIDITY, false);
    Register(Capability::SENSOR_MOTION, false);
    Register(Capability::SENSOR_LIGHT, false);

    Register(Capability::INPUT_BUTTON, true, "Touch pin acts as button");
    Register(Capability::INPUT_ROTARY_ENCODER, false);
    Register(Capability::INPUT_KEYBOARD, false);
    Register(Capability::INPUT_MOUSE, false);
    Register(Capability::INPUT_VOICE, true, "INMP441 Mic + Google STT");
    Register(Capability::INPUT_GESTURE, false);
}

void CapabilityManager::DetectAI() noexcept {
    Register(Capability::AI_GEMINI, true, "Gemini 2.5 Flash");
    Register(Capability::AI_TINY, true, "Offline Tiny AI");
    Register(Capability::AI_OPENAI, false);
    Register(Capability::AI_CLAUDE, false);
    Register(Capability::AI_LOCAL_LLM, false);
}

void CapabilityManager::DetectPower() noexcept {
    Register(Capability::POWER_USB, true, "USB Powered");
    Register(Capability::POWER_BATTERY, false);
    Register(Capability::POWER_SOLAR, false);
}

void CapabilityManager::EmitCapabilityEvents() noexcept {
    if (!eventBus.isInitialized()) return;

    String avail = "[";
    auto available = GetAvailable();
    for (size_t i = 0; i < available.size(); ++i) {
        if (i > 0) avail += ",";
        avail += String(static_cast<int>(available[i]));
    }
    avail += "]";

    eventBus.publish(EventType::SYSTEM_STARTUP, "CapabilityManager",
                     "{\"available\":" + avail + ",\"total\":" + String(TotalCount()) + "}");
}