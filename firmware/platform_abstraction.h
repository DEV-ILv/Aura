#ifndef AURA_PLATFORM_ABSTRACTION_H
#define AURA_PLATFORM_ABSTRACTION_H

#include <Arduino.h>
#include "config.h"
#include "logger.h"
#include "service.h"
#include "capability_manager.h"

enum class PlatformType : uint8_t {
    ESP32,
    ESP32_S3,
    RASPBERRY_PI,
    LINUX,
    WINDOWS,
    ANDROID,
    IOS,
    WEB,
    UNKNOWN
};

struct PlatformInfo {
    PlatformType type;
    String name;
    String chipModel;
    String chipRevision;
    uint32_t cpuFreqMHz;
    size_t totalHeap;
    size_t totalPSRAM;
    size_t flashSize;
    uint8_t coreCount;
    bool hasBluetooth;
    bool hasWiFi;
    bool hasEthernet;
    bool hasUSBOTG;
    bool hasCamera;
    bool hasTouch;
    bool hasHallSensor;
    bool hasDAC;
    bool hasADC;
};

class PlatformAbstraction : public Service {
public:
    PlatformAbstraction() noexcept;
    ~PlatformAbstraction() noexcept;

    bool Initialize() noexcept override;
    void Update() noexcept override;

    const PlatformInfo& GetInfo() const noexcept;
    PlatformType GetType() const noexcept;
    String GetName() const noexcept;

    // Hardware access wrappers
    [[nodiscard]] uint32_t GetFreeHeap() const noexcept;
    [[nodiscard]] uint32_t GetMinFreeHeap() const noexcept;
    [[nodiscard]] uint32_t GetMaxAllocHeap() const noexcept;
    [[nodiscard]] float GetHeapFragmentation() const noexcept;

    [[nodiscard]] uint32_t GetFreePSRAM() const noexcept;
    [[nodiscard]] uint32_t GetFreeFlash() const noexcept;

    [[nodiscard]] float GetCPUFrequency() const noexcept;
    [[nodiscard]] int GetCPUCoreCount() const noexcept;
    [[nodiscard]] float GetCPUUsage() const noexcept;

    [[nodiscard]] String GetMACAddress() const noexcept;
    [[nodiscard]] String GetChipID() const noexcept;

    // Power management
    void DeepSleep(uint64_t micros) noexcept;
    void LightSleep(uint64_t micros) noexcept;
    bool Restart() noexcept override;
    [[nodiscard]] uint64_t GetUptimeMs() const noexcept;
    [[nodiscard]] uint32_t GetUptimeSec() const noexcept;

    [[nodiscard]] float GetTemperature() const noexcept;
    [[nodiscard]] float GetVCC() const noexcept;

    static constexpr const char* kStaticName = "PlatformAbstraction";

private:
    void DetectPlatform() noexcept;
    void RegisterCapabilities() noexcept;
    uint32_t EstimateFlashSize() const noexcept;

    static constexpr const char* kLogCategory = "Platform";
    PlatformInfo m_info;
    bool m_initialized;
};

extern PlatformAbstraction platform;

#endif