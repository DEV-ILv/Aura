#include "platform_abstraction.h"
#include <WiFi.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_spi_flash.h>
#include <soc/soc.h>
#include <soc/rtc.h>

#ifndef CONFIG_ESP32_PSRAM
#define CONFIG_ESP32_PSRAM 0
#endif

PlatformAbstraction platform;

PlatformAbstraction::PlatformAbstraction() noexcept
    : Service(kStaticName, BootPriority::CRITICAL)
    , m_initialized(false) {
    m_info.type = PlatformType::UNKNOWN;
    m_info.coreCount = 0;
    m_info.cpuFreqMHz = 0;
    m_info.totalHeap = 0;
    m_info.totalPSRAM = 0;
    m_info.flashSize = 0;
}

PlatformAbstraction::~PlatformAbstraction() noexcept = default;

bool PlatformAbstraction::Initialize() noexcept {
    if (GetState() != ServiceState::UNINITIALIZED) return true;
    SetState(ServiceState::INITIALIZING);
    DetectPlatform();
    RegisterCapabilities();
    SetState(ServiceState::INITIALIZED);
    LOG_INFO(kLogCategory, "Platform: %s | CPU: %d MHz x%d | Heap: %u KB | Flash: %u MB",
             m_info.name.c_str(), m_info.cpuFreqMHz, m_info.coreCount,
             m_info.totalHeap / 1024, m_info.flashSize / (1024 * 1024));
    m_initialized = true;
    return true;
}

void PlatformAbstraction::Update() noexcept {}

const PlatformInfo& PlatformAbstraction::GetInfo() const noexcept { return m_info; }

PlatformType PlatformAbstraction::GetType() const noexcept { return m_info.type; }

String PlatformAbstraction::GetName() const noexcept { return m_info.name; }

uint32_t PlatformAbstraction::GetFreeHeap() const noexcept {
    return ESP.getFreeHeap();
}

uint32_t PlatformAbstraction::GetMinFreeHeap() const noexcept {
    return ESP.getMinFreeHeap();
}

uint32_t PlatformAbstraction::GetMaxAllocHeap() const noexcept {
    return ESP.getMaxAllocHeap();
}

float PlatformAbstraction::GetHeapFragmentation() const noexcept {
    uint32_t free = ESP.getFreeHeap();
    uint32_t max = ESP.getMaxAllocHeap();
    if (free == 0) return 100.0f;
    return 100.0f * (1.0f - (float)max / (float)free);
}

uint32_t PlatformAbstraction::GetFreePSRAM() const noexcept {
#if CONFIG_ESP32_PSRAM
    return ESP.getFreePsram();
#else
    return 0;
#endif
}

uint32_t PlatformAbstraction::GetFreeFlash() const noexcept {
    // Approximate: SPIFFS total - used
    return 0; // TODO: query SPIFFS
}

float PlatformAbstraction::GetCPUFrequency() const noexcept {
    return static_cast<float>(ESP.getCpuFreqMHz());
}

int PlatformAbstraction::GetCPUCoreCount() const noexcept {
    return static_cast<int>(m_info.coreCount);
}

float PlatformAbstraction::GetCPUUsage() const noexcept {
    // ESP32 has no built-in CPU usage API
    // Approximate from FreeRTOS task stats
    return 0.0f;
}

String PlatformAbstraction::GetMACAddress() const noexcept {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

String PlatformAbstraction::GetChipID() const noexcept {
    uint64_t chipId = ESP.getEfuseMac();
    char buf[16];
    snprintf(buf, sizeof(buf), "%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
    return String(buf);
}

void PlatformAbstraction::DeepSleep(uint64_t micros) noexcept {
    esp_deep_sleep(micros);
}

void PlatformAbstraction::LightSleep(uint64_t micros) noexcept {
    esp_light_sleep_start();
    delayMicroseconds(micros);
}

bool PlatformAbstraction::Restart() noexcept {
    ESP.restart();
    return true;
}

uint64_t PlatformAbstraction::GetUptimeMs() const noexcept {
    return millis();
}

uint32_t PlatformAbstraction::GetUptimeSec() const noexcept {
    return millis() / 1000;
}

float PlatformAbstraction::GetTemperature() const noexcept {
    return temperatureRead();
}

float PlatformAbstraction::GetVCC() const noexcept {
    return static_cast<float>(analogRead(36)) * 3.3f / 4095.0f;
}

void PlatformAbstraction::DetectPlatform() noexcept {
    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);

    m_info.coreCount = static_cast<uint8_t>(chipInfo.cores);
    m_info.cpuFreqMHz = static_cast<uint32_t>(ESP.getCpuFreqMHz());

    // Detect chip model
    switch (chipInfo.model) {
        case CHIP_ESP32:
            m_info.type = PlatformType::ESP32;
            m_info.name = "ESP32";
            m_info.chipModel = "ESP32-D0WDQ6";
            m_info.hasWiFi = true;
            m_info.hasBluetooth = true;
            break;
        case CHIP_ESP32S2:
            m_info.type = PlatformType::ESP32_S3;
            m_info.name = "ESP32-S2";
            m_info.chipModel = "ESP32-S2";
            m_info.hasWiFi = true;
            m_info.hasBluetooth = false;
            break;
        case CHIP_ESP32S3:
            m_info.type = PlatformType::ESP32_S3;
            m_info.name = "ESP32-S3";
            m_info.chipModel = "ESP32-S3";
            m_info.hasWiFi = true;
            m_info.hasBluetooth = true;
            break;
        default:
            m_info.type = PlatformType::UNKNOWN;
            m_info.name = "Unknown ESP32 Variant";
            break;
    }

    // Chip revision
    m_info.chipRevision = String(chipInfo.revision);

    // Features — determined by chip model (not exposed as esp_chip_info feature flags)
    m_info.hasUSBOTG = (chipInfo.model == CHIP_ESP32S2 || chipInfo.model == CHIP_ESP32S3);
    m_info.hasCamera = (chipInfo.model == CHIP_ESP32);  // ESP32 has camera (DVP) interface
    m_info.hasTouch = (chipInfo.model == CHIP_ESP32 || chipInfo.model == CHIP_ESP32S2 || chipInfo.model == CHIP_ESP32S3);
    m_info.hasHallSensor = (chipInfo.model == CHIP_ESP32);  // Only original ESP32 has hall sensor
    m_info.hasDAC = (chipInfo.model == CHIP_ESP32 || chipInfo.model == CHIP_ESP32S2);  // ESP32-S3 lacks DAC
    m_info.hasADC = true;  // All ESP32 variants have SAR ADCs

    // Memory
    m_info.totalHeap = ESP.getHeapSize();
#if CONFIG_ESP32_PSRAM
    m_info.totalPSRAM = ESP.getPsramSize();
#endif
    m_info.flashSize = EstimateFlashSize();
}

void PlatformAbstraction::RegisterCapabilities() noexcept {
    if (capabilityManager.GetState() < ServiceState::INITIALIZED) return;

    capabilityManager.Register(Capability::PLATFORM_ESP32,
        m_info.type == PlatformType::ESP32, m_info.chipModel);
    capabilityManager.Register(Capability::PLATFORM_ESP32_S3,
        m_info.type == PlatformType::ESP32_S3, m_info.chipModel);
    capabilityManager.Register(Capability::SENSOR_TOUCH, m_info.hasTouch);
    capabilityManager.Register(Capability::CAMERA, m_info.hasCamera);
}

uint32_t PlatformAbstraction::EstimateFlashSize() const noexcept {
    uint32_t size = ESP.getFlashChipSize();
    return (size > 0) ? size : 4 * 1024 * 1024; // default 4MB
}