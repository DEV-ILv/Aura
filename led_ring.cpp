#include "led_ring.h"

LedRing ledRing;

namespace {

constexpr const char* kLogCategory = "LedRing";

constexpr uint32_t kFrameIntervalMs = 16U;

constexpr uint32_t kBootSweepFrameMs = 70U;
constexpr uint32_t kBootSweepDurationMs = 1120U;
constexpr uint32_t kBootBreathPeriodMs = 1800U;

constexpr uint32_t kReadyBreathPeriodMs = 4000U;
constexpr uint32_t kListeningPulsePeriodMs = 900U;
constexpr uint32_t kThinkingStepIntervalMs = 100U;
constexpr uint32_t kSpeakingStepIntervalMs = 60U;
constexpr uint32_t kWifiStepIntervalMs = 110U;
constexpr uint32_t kWifiConnectedDurationMs = 900U;
constexpr uint32_t kReminderBreathPeriodMs = 1800U;
constexpr uint32_t kErrorFlashPeriodMs = 180U;
constexpr uint32_t kSleepBreathPeriodMs = 5000U;

constexpr uint8_t kBootSweepLength = 4U;
constexpr uint8_t kThinkingDotCount = 3U;
constexpr uint8_t kThinkingDotSpacing = LED_COUNT / kThinkingDotCount;
constexpr uint8_t kWifiSegmentLength = 3U;
constexpr uint8_t kMinimumSleepBrightness = 2U;
constexpr uint8_t kMaximumSleepBrightness = 12U;

const CRGB kBootColor(LED_BOOT);
const CRGB kReadyColor(LED_READY);
const CRGB kListeningColor(LED_LISTENING);
const CRGB kThinkingColor(CRGB::White);
const CRGB kSpeakingColor(LED_SPEAKING);
const CRGB kWifiConnectingColor(LED_PROCESSING);
const CRGB kWifiConnectedColor(LED_READY);
const CRGB kReminderColor(CRGB::Orange);
const CRGB kOtaColor(LED_LISTENING);
const CRGB kErrorColor(LED_ERROR);
const CRGB kSleepColor(CRGB::White);

}  // namespace

LedRing::LedRing() noexcept
    : m_currentState(LedState::BOOT),
      m_brightness(LED_BRIGHTNESS),
      m_isEnabled(true),
      m_otaProgress(0U),
      m_animationFrame(0U),
      m_animationTimer(0U),
      m_leds{},
      m_themeColor(CRGB::Black)
{
}

void LedRing::initialize() noexcept
{
    FastLED.addLeds<WS2812B, LED_RING_PIN, GRB>(m_leds, LED_COUNT);
    FastLED.setBrightness(m_brightness);

    clearRing();
    resetAnimation();

    Logger::info(
        kLogCategory,
        "Initialized %u WS2812B LEDs on GPIO %u",
        static_cast<unsigned int>(LED_COUNT),
        static_cast<unsigned int>(LED_RING_PIN));
}

void LedRing::update() noexcept
{
    if (!m_isEnabled) {
        return;
    }

    const unsigned long now = millis();
    if ((now - m_animationTimer) < kFrameIntervalMs) {
        return;
    }

    switch (m_currentState) {
        case LedState::BOOT:
            playBootAnimation();
            break;

        case LedState::READY:
            playReadyAnimation();
            break;

        case LedState::LISTENING:
            playListeningAnimation();
            break;

        case LedState::THINKING:
            playThinkingAnimation();
            break;

        case LedState::SPEAKING:
            playSpeakingAnimation();
            break;

        case LedState::WIFI_CONNECTING:
            playWifiConnectingAnimation();
            break;

        case LedState::WIFI_CONNECTED:
            playWifiConnectedAnimation();
            break;

        case LedState::REMINDER:
            playReminderAnimation();
            break;

        case LedState::OTA_UPDATE:
            playOtaUpdateAnimation();
            break;

        case LedState::ERROR:
            playErrorAnimation();
            break;

        case LedState::SLEEP:
            playSleepAnimation();
            break;

        default:
            Logger::warning(kLogCategory, "Unknown LED state");
            clearRing();
            return;
    }

    FastLED.show();
}

void LedRing::setState(const LedState newState) noexcept
{
    if (m_currentState == newState) {
        return;
    }

    m_currentState = newState;
    resetAnimation();

    Logger::info(
        kLogCategory,
        "LED state changed to %u",
        static_cast<unsigned int>(newState));
}

LedState LedRing::getState() const noexcept
{
    return m_currentState;
}

void LedRing::setThemeColor(const CRGB color) noexcept
{
    m_themeColor = color;
    Logger::info(kLogCategory, "Theme color set to #%02X%02X%02X",
        static_cast<unsigned int>(color.r),
        static_cast<unsigned int>(color.g),
        static_cast<unsigned int>(color.b));
}

CRGB LedRing::getThemeColor() const noexcept
{
    return m_themeColor;
}

void LedRing::setBrightness(const uint8_t brightness) noexcept
{
    m_brightness = brightness;
    FastLED.setBrightness(m_brightness);

    if (m_isEnabled) {
        FastLED.show();
    }

    Logger::info(
        kLogCategory,
        "Brightness set to %u",
        static_cast<unsigned int>(m_brightness));
}

void LedRing::setOtaProgress(const uint8_t percentage) noexcept
{
    m_otaProgress = percentage > 100U ? 100U : percentage;

    if (m_currentState == LedState::OTA_UPDATE) {
        m_animationTimer = 0U;
    }
}

void LedRing::turnOff() noexcept
{
    if (!m_isEnabled) {
        return;
    }

    m_isEnabled = false;
    clearRing();

    Logger::info(kLogCategory, "LED ring turned off");
}

void LedRing::turnOn() noexcept
{
    if (m_isEnabled) {
        return;
    }

    m_isEnabled = true;
    resetAnimation();

    Logger::info(kLogCategory, "LED ring turned on");
}

bool LedRing::isEnabled() const noexcept
{
    return m_isEnabled;
}

void LedRing::playBootAnimation() noexcept
{
    const CRGB color = resolveColor(kBootColor);
    const unsigned long elapsed = millis() - m_animationTimer;

    if (elapsed < kBootSweepDurationMs) {
        const uint8_t position = static_cast<uint8_t>(
            (elapsed / kBootSweepFrameMs) % LED_COUNT);

        fill_solid(m_leds, LED_COUNT, CRGB::Black);

        for (uint8_t offset = 0U; offset < kBootSweepLength; ++offset) {
            const uint8_t index =
                static_cast<uint8_t>((position + LED_COUNT - offset) % LED_COUNT);
            const uint8_t brightness =
                static_cast<uint8_t>(
                    255U - ((offset * 255U) / kBootSweepLength));

            m_leds[index] = color;
            m_leds[index].nscale8_video(brightness);
        }

        ++m_animationFrame;
        return;
    }

    const uint8_t brightness = beatsin8(
        static_cast<uint8_t>(60000UL / kBootBreathPeriodMs),
        40U,
        180U);

    fill_solid(m_leds, LED_COUNT, color);
    for (uint8_t index = 0U; index < LED_COUNT; ++index) {
        m_leds[index].nscale8_video(brightness);
    }
}

void LedRing::playReadyAnimation() noexcept
{
    const CRGB color = resolveColor(kReadyColor);
    const uint8_t brightness = beatsin8(
        static_cast<uint8_t>(60000UL / kReadyBreathPeriodMs),
        20U,
        130U);

    fill_solid(m_leds, LED_COUNT, color);

    for (uint8_t index = 0U; index < LED_COUNT; ++index) {
        m_leds[index].nscale8_video(brightness);
    }
}

void LedRing::playListeningAnimation() noexcept
{
    const CRGB color = resolveColor(kListeningColor);
    const uint8_t pulse = beatsin8(
        static_cast<uint8_t>(60000UL / kListeningPulsePeriodMs),
        35U,
        255U);
    const uint8_t headPosition = static_cast<uint8_t>(
        (millis() / kListeningPulsePeriodMs) % LED_COUNT);

    fill_solid(m_leds, LED_COUNT, color);

    for (uint8_t index = 0U; index < LED_COUNT; ++index) {
        const uint8_t distance = static_cast<uint8_t>(
            (index + LED_COUNT - headPosition) % LED_COUNT);
        const uint8_t attenuation =
            distance < 5U ? static_cast<uint8_t>(255U - (distance * 42U)) : 45U;
        const uint8_t brightness = scale8(pulse, attenuation);

        m_leds[index].nscale8_video(brightness);
    }
}

void LedRing::playThinkingAnimation() noexcept
{
    const CRGB color = resolveColor(kThinkingColor);
    const uint8_t position = static_cast<uint8_t>(
        (millis() / kThinkingStepIntervalMs) % LED_COUNT);

    fill_solid(m_leds, LED_COUNT, CRGB::Black);

    for (uint8_t dot = 0U; dot < kThinkingDotCount; ++dot) {
        const uint8_t index = static_cast<uint8_t>(
            (position + (dot * kThinkingDotSpacing)) % LED_COUNT);
        const uint8_t brightness =
            dot == 0U ? 255U : (dot == 1U ? 150U : 75U);

        m_leds[index] = color;
        m_leds[index].nscale8_video(brightness);
    }

    ++m_animationFrame;
}

void LedRing::playSpeakingAnimation() noexcept
{
    const CRGB color = resolveColor(kSpeakingColor);
    const unsigned long frame = millis() / kSpeakingStepIntervalMs;

    for (uint8_t index = 0U; index < LED_COUNT; ++index) {
        const uint8_t phase = static_cast<uint8_t>(
            (frame * 19U) + (index * (256U / LED_COUNT)));
        const uint8_t brightness = sin8(phase);

        m_leds[index] = color;
        m_leds[index].nscale8_video(
            static_cast<uint8_t>(50U + (brightness / 2U)));
    }

    ++m_animationFrame;
}

void LedRing::playWifiConnectingAnimation() noexcept
{
    const uint8_t position = static_cast<uint8_t>(
        (millis() / kWifiStepIntervalMs) % LED_COUNT);

    fill_solid(m_leds, LED_COUNT, CRGB::Black);

    for (uint8_t offset = 0U; offset < kWifiSegmentLength; ++offset) {
        const uint8_t index = static_cast<uint8_t>(
            (position + offset) % LED_COUNT);
        const uint8_t brightness =
            static_cast<uint8_t>(255U - (offset * 65U));

        m_leds[index] = kWifiConnectingColor;
        m_leds[index].nscale8_video(brightness);
    }

    ++m_animationFrame;
}

void LedRing::playWifiConnectedAnimation() noexcept
{
    const unsigned long elapsed = millis() - m_animationTimer;

    if (elapsed >= kWifiConnectedDurationMs) {
        setState(LedState::READY);
        playReadyAnimation();
        return;
    }

    const uint8_t brightness = beatsin8(80U, 80U, 255U);

    fill_solid(m_leds, LED_COUNT, kWifiConnectedColor);
    for (uint8_t index = 0U; index < LED_COUNT; ++index) {
        m_leds[index].nscale8_video(brightness);
    }
}

void LedRing::playReminderAnimation() noexcept
{
    const uint8_t brightness = beatsin8(
        static_cast<uint8_t>(60000UL / kReminderBreathPeriodMs),
        45U,
        220U);

    fill_solid(m_leds, LED_COUNT, kReminderColor);

    for (uint8_t index = 0U; index < LED_COUNT; ++index) {
        m_leds[index].nscale8_video(brightness);
    }
}

void LedRing::playOtaUpdateAnimation() noexcept
{
    const uint8_t completedLeds = static_cast<uint8_t>(
        (static_cast<uint16_t>(m_otaProgress) * LED_COUNT) / 100U);

    fill_solid(m_leds, LED_COUNT, CRGB::Black);

    for (uint8_t index = 0U; index < completedLeds; ++index) {
        m_leds[index] = kOtaColor;
    }

    if (completedLeds < LED_COUNT) {
        const uint8_t pulse = beatsin8(40U, 40U, 180U);
        m_leds[completedLeds] = kOtaColor;
        m_leds[completedLeds].nscale8_video(pulse);
    }
}

void LedRing::playErrorAnimation() noexcept
{
    const unsigned long elapsed = millis() - m_animationTimer;
    const bool isOn =
        ((elapsed / kErrorFlashPeriodMs) % 2U) == 0U;

    fill_solid(m_leds, LED_COUNT, isOn ? kErrorColor : CRGB::Black);

    if (isOn) {
        const uint8_t brightness = beatsin8(100U, 110U, 255U);
        for (uint8_t index = 0U; index < LED_COUNT; ++index) {
            m_leds[index].nscale8_video(brightness);
        }
    }
}

void LedRing::playSleepAnimation() noexcept
{
    const uint8_t brightness = beatsin8(
        static_cast<uint8_t>(60000UL / kSleepBreathPeriodMs),
        kMinimumSleepBrightness,
        kMaximumSleepBrightness);

    fill_solid(m_leds, LED_COUNT, kSleepColor);

    for (uint8_t index = 0U; index < LED_COUNT; ++index) {
        m_leds[index].nscale8_video(brightness);
    }
}

CRGB LedRing::resolveColor(const CRGB defaultColor) const noexcept
{
    if (m_themeColor.r != 0 || m_themeColor.g != 0 || m_themeColor.b != 0) {
        return m_themeColor;
    }
    return defaultColor;
}

void LedRing::clearRing() noexcept
{
    fill_solid(m_leds, LED_COUNT, CRGB::Black);
    FastLED.show();
}

void LedRing::resetAnimation() noexcept
{
    m_animationFrame = 0U;
    m_animationTimer = millis();
}