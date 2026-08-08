#include "led_ring.h"
#include "event_bus.h"
#include "audio_manager.h"

LedRing ledRing;

namespace {

void HandleVoiceEvent(const Event& e);

constexpr const char* kLogCategory = "LedRing";

// React to live voice activity: light the ring green (LISTENING) the moment a
// speech threshold is crossed, feed the VU level, and return to idle on silence.
void HandleVoiceEvent(const Event& e) {
    switch (e.type) {
        case EventType::VOICE_DETECTED:
            ledRing.setMood(AuraMood::LISTENING);
            ledRing.setVoiceLevel(255U);   // full VU floor -> bright green
            break;
        case EventType::VOICE_ENDED:
            ledRing.setVoiceLevel(0U);
            // Only drop back to idle when this VAD instance lit the ring; a
            // live conversation recording still owns the listening mood.
            if (ledRing.getMood() == AuraMood::LISTENING && !audioManager.isRecording()) {
                ledRing.setMood(AuraMood::IDLE);
            }
            break;
        default:
            break;
    }
}

constexpr uint32_t kFrameIntervalMs = 20UL;
constexpr uint8_t  kCrossFade = 140U;           // crisp, near-instant mood cross-fades (0..255)

// ---- boot ----
constexpr uint32_t kBootFillMs = 1600UL;        // time for the ring to fill
constexpr uint32_t kBootStepMs = 40UL;

// ---- idle ----
constexpr uint32_t kIdleBreathPeriodMs = 6000UL;
constexpr uint32_t kSparkleIntervalMinMs = 9000UL;
constexpr uint32_t kSparkleIntervalMaxMs = 15000UL;
constexpr uint32_t kSparkleStepMs = 18UL;
constexpr uint8_t  kSparkleLen = 8;

// ---- manual device-control session ---------------------------------------
// A manual test session auto-expires so the ring can never be left glowing
// forever after a user stops interacting with the device-control page.
constexpr uint32_t kManualControlTimeoutMs = 60000UL;

// ---- listening ----
constexpr uint32_t kListenStepMs = 42UL;        // full lap ~ 16*42 = 672ms

// ---- thinking / processing ----
constexpr uint32_t kThinkFlowClock = 120UL;

// ---- speaking ----
constexpr uint32_t kSpeakClock = 22UL;

// ---- reminder ----
constexpr uint32_t kReminderStepMs = 55UL;

// ---- warning ----
constexpr uint32_t kWarningBeatMs = 1400UL;

// ---- critical ----
constexpr uint32_t kCriticalPeriodMs = 2200UL;

// ---- ota ----
constexpr uint32_t kOtaStepMs = 34UL;

// ---- wifi ----
constexpr uint32_t kWifiStepMs = 90UL;

// ---- transients (these return to IDLE) ----
constexpr uint32_t kSuccessDurationMs = 1700UL;
constexpr uint32_t kWakeDurationMs = 1100UL;
constexpr uint32_t kWifiConnectedDurationMs = 1300UL;

// ---- palette (default, tinted by theme via moodColor) ----
const CRGB kIdleColor(42, 84, 168);           // dim blue (passive idle)
const CRGB kListeningColor(46, 216, 92);      // green (voice detected, VAD)
const CRGB kRecordingColor(0, 208, 198);      // cyan (mic capture)
const CRGB kThinkingColor(242, 212, 60);      // yellow (processing)
const CRGB kProcessingColor(212, 232, 92);    // yellow-green processing
const CRGB kSpeakingColor(238, 238, 242);     // white (speaking)
const CRGB kHappyColor(255, 198, 108);       // warm gold
const CRGB kSuccessColor(92, 224, 130);      // green
const CRGB kReminderColor(245, 205, 84);     // golden yellow
const CRGB kWarningColor(255, 150, 40);      // orange
const CRGB kErrorColor(224, 58, 50);         // red
const CRGB kPrivacyColor(216, 40, 44);       // solid red (privacy)
const CRGB kCriticalColor(150, 26, 22);      // dark red
const CRGB kOtaColor(164, 86, 230);          // purple
const CRGB kOfflineColor(120, 132, 152);     // dim white
const CRGB kSleepColor(150, 160, 182);       // white (rendered near-off)
const CRGB kWakeColor(40, 110, 240);         // blue
const CRGB kBootColor(206, 214, 236);        // clean white
const CRGB kWifiConnectingColor(200, 212, 230);
const CRGB kWifiConnectedColor(96, 214, 150);
const CRGB kSetupColor(164, 86, 230);           // purple (setup/provisioning)

// ---- disco mode ----------------------------------------------------------
// Smooth, LOW-resolution colour animations that intentionally avoid fast
// per-frame strobing (friendly, non-seizure-risk) while still reading as
// clearly "party". Rotated every 15-20 s. Rendered at the disco brightness.
constexpr uint8_t  kDiscoAnimCount = 10;
constexpr uint32_t kDiscoAnimMinMs = 15000UL;
constexpr uint32_t kDiscoAnimMaxMs = 20000UL;
constexpr uint8_t  kDiscoCrossFade = 110U;      // soft blend between animations

}  // namespace

LedRing::LedRing() noexcept
    : m_mood(AuraMood::BOOT)
    , m_manualMood(AuraMood::IDLE)
    , m_controlMode(RingControlMode::kAutomatic)
    , m_manualSince(0U)
    , m_brightness(0)
    , m_isEnabled(true)
    , m_ledsBlack(true)
    , m_subscribed(false)
    , m_voiceLevel(0)
    , m_otaProgress(0U)
    , m_animationFrame(0U)
    , m_animationTimer(0U)
    , m_moodSince(0U)
    , m_leds{}
    , m_frame{}
    , m_themeColor(CRGB::Black)
    , m_sparkleNext(0U)
    , m_sparkleUntil(0U)
    , m_sparkleActive(false)
    , m_sparkleHead(0U)
    , m_discoEnabled(false)
    , m_discoBrightness(100)
    , m_discoAnim(0U)
    , m_discoAnimSince(0U)
    , m_discoAnimDuration(kDiscoAnimMinMs) {
}

void LedRing::initialize() noexcept {
    FastLED.addLeds<WS2812B, LED_RING_PIN, GRB>(m_leds, LED_COUNT);
    FastLED.setBrightness(0);                 // fade in from off
    setMood(AuraMood::BOOT);
    m_animationTimer = millis();
    Logger::info(kLogCategory, "Initialized %u aura LEDs (WS2812B) on GPIO %u",
                 static_cast<unsigned int>(LED_COUNT),
                 static_cast<unsigned int>(LED_RING_PIN));
}

void LedRing::update() noexcept {
    const unsigned long now = millis();

    // Late-bind the event-bus subscription so init order never matters.
    if (!m_subscribed && eventBus.isInitialized()) {
        subscribeToEvents();
    }

    // Manual control sessions auto-expire back to automatic behaviour so the
    // ring can never be left lit forever after a device-control test.
    if (m_controlMode == RingControlMode::kManual &&
        (now - m_manualSince) >= kManualControlTimeoutMs) {
        endManualControl();
    }

    // Disco Mode is an app-only override. Emergency moods (Error / OTA /
    // Critical) always outrank it and pause it; when the emergency clears the
    // next update() resumes Disco Mode automatically because m_discoEnabled is
    // still set.
    if (m_discoEnabled && !isEmergencyMood(m_mood)) {
        renderDisco();
        return;
    }

    // During a manual session the ring shows the user's chosen animation;
    // otherwise it follows the system AuraMood.
    const AuraMood render = (m_controlMode == RingControlMode::kManual)
        ? m_manualMood
        : m_mood;

    if (!m_isEnabled) {
        if (!m_ledsBlack) clearRing();
        return;
    }

    // Idle policy: SLEEP/OFFLINE stay fully OFF (power save); normal IDLE shows
    // a dim blue passive-listening glow per the final interaction spec.
    if (isQuietMood(render)) {
        if (!m_ledsBlack) clearRing();
        return;
    }
    m_ledsBlack = false;

    if ((now - m_animationTimer) < kFrameIntervalMs) {
        return;
    }
    m_animationTimer = now;
    ++m_animationFrame;

    easeBrightness();

    switch (render) {
        case AuraMood::BOOT:            playBoot();            break;
        case AuraMood::IDLE:            playIdle();            break;
        case AuraMood::LISTENING:       playListening();       break;
        case AuraMood::RECORDING:       playRecording();       break;
        case AuraMood::THINKING:        playThinking();        break;
        case AuraMood::PROCESSING:      playProcessing();      break;
        case AuraMood::SPEAKING:        playSpeaking();        break;
        case AuraMood::HAPPY:           playHappy();           break;
        case AuraMood::SUCCESS:         playSuccess();         break;
        case AuraMood::REMINDER:        playReminder();        break;
        case AuraMood::WARNING:         playWarning();         break;
        case AuraMood::ERROR:           playError();           break;
        case AuraMood::PRIVACY:         playPrivacy();         break;
        case AuraMood::CRITICAL:        playCritical();        break;
        case AuraMood::OTA:             playOta();             break;
        case AuraMood::OFFLINE:         playOffline();         break;
        case AuraMood::SLEEP:           playSleep();           break;
        case AuraMood::WAKE:            playWake();            break;
        case AuraMood::WIFI_CONNECTING: playWifiConnecting();  break;
        case AuraMood::WIFI_CONNECTED:  playWifiConnected();   break;
        case AuraMood::SETUP:           playSetup();           break;
        default:
            clearRing();
            return;
    }

    // Soft cross-fade into the freshly computed frame for smooth transitions.
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        m_leds[i] = blend(m_leds[i], m_frame[i], kCrossFade);
    }
    FastLED.show();
}

void LedRing::setMood(const AuraMood mood) noexcept {
    if (static_cast<uint8_t>(mood) >= static_cast<uint8_t>(AuraMood::MAX)) {
        return;
    }
    const bool changed = (mood != m_mood);
    m_mood = mood;
    m_moodSince = millis();
    if (changed) {
        Logger::info(kLogCategory, "Aura mood -> %s", auraMoodName(mood));
    }
}

AuraMood LedRing::getMood() const noexcept {
    return m_mood;
}

void LedRing::setVoiceLevel(const uint8_t level) noexcept {
    m_voiceLevel = level;
}

void LedRing::subscribeToEvents() noexcept {
    if (m_subscribed || !eventBus.isInitialized()) return;
    eventBus.subscribe(EventType::VOICE_DETECTED, HandleVoiceEvent, EventPriority::NORMAL_PRIORITY, "LedRing-VAD");
    eventBus.subscribe(EventType::VOICE_ENDED, HandleVoiceEvent, EventPriority::NORMAL_PRIORITY, "LedRing-VAD");
    m_subscribed = true;
    Logger::info(kLogCategory, "Subscribed to VAD events (instant voice LED)");
}

void LedRing::setOtaProgress(const uint8_t percentage) noexcept {
    m_otaProgress = percentage > 100U ? 100U : percentage;
}

void LedRing::setBrightness(const uint8_t brightness) noexcept {
    m_brightness = brightness;
    FastLED.setBrightness(m_brightness);
}

void LedRing::setThemeColor(const CRGB color) noexcept {
    m_themeColor = color;
}

CRGB LedRing::getThemeColor() const noexcept {
    return m_themeColor;
}

void LedRing::turnOff() noexcept {
    if (!m_isEnabled) return;
    m_isEnabled = false;
    clearRing();
    Logger::info(kLogCategory, "Aura ring turned off");
}

void LedRing::turnOn() noexcept {
    if (m_isEnabled) return;
    m_isEnabled = true;
    m_animationTimer = millis();
    Logger::info(kLogCategory, "Aura ring turned on");
}

bool LedRing::isEnabled() const noexcept {
    return m_isEnabled;
}

// ============================================================================
// Rendering helpers
// ============================================================================

CRGB LedRing::moodColor(const CRGB base) const noexcept {
    if (m_themeColor.r == 0 && m_themeColor.g == 0 && m_themeColor.b == 0) {
        return base;
    }
    // Blend ~40% toward the personality theme tint while keeping the base hue.
    return blend(base, m_themeColor, 100U);
}

uint8_t LedRing::effectiveBrightness() const noexcept {
    if (!m_isEnabled) return 0U;
    const AuraMood render = (m_controlMode == RingControlMode::kManual)
        ? m_manualMood
        : m_mood;
    if (isQuietMood(render)) return 0U;
    return auraMoodBrightness[static_cast<uint8_t>(render)];
}

void LedRing::easeBrightness() noexcept {
    const uint8_t target = effectiveBrightness();
    const int16_t delta = static_cast<int16_t>(target) - static_cast<int16_t>(m_brightness);
    if (delta != 0) {
        // Large step so brightness reaches its target almost instantly
        // (<50 ms at a 20 ms frame interval) without hard-stepping flicker.
        const int16_t step = (delta > 0) ? 8 : -8;
        m_brightness = static_cast<uint8_t>(
            static_cast<int16_t>(m_brightness) + step);
        FastLED.setBrightness(m_brightness);
    }
}

bool LedRing::isQuietMood(const AuraMood mood) const noexcept {
    // Only real sleep/offline stay fully OFF; IDLE shows a dim blue presence
    // (passive-listening idle per the final interaction spec).
    return mood == AuraMood::SLEEP || mood == AuraMood::OFFLINE;
}

bool LedRing::isEmergencyMood(const AuraMood mood) const noexcept {
    // Priority moods that always outrank Disco Mode (final interaction spec):
    // ERROR / CRITICAL / OTA / SETUP / PRIVACY.
    return mood == AuraMood::ERROR || mood == AuraMood::CRITICAL || mood == AuraMood::OTA ||
           mood == AuraMood::SETUP || mood == AuraMood::PRIVACY;
}

void LedRing::clearRing() noexcept {
    fill_solid(m_leds, LED_COUNT, CRGB::Black);
    FastLED.show();
    m_ledsBlack = true;
}

// ============================================================================
// Manual device-control session
// ============================================================================

void LedRing::beginManualControl() noexcept {
    const bool first = (m_controlMode != RingControlMode::kManual);
    m_controlMode = RingControlMode::kManual;
    m_manualSince = millis();
    if (first) {
        // Start from the current system mood so a colour/brightness change is
        // immediately visible, and record nothing else: automatic behaviour
        // resumes from the live system mood when the session ends.
        m_manualMood = m_mood;
        Logger::info(kLogCategory, "Manual LED control active");
    }
}

void LedRing::endManualControl() noexcept {
    if (m_controlMode != RingControlMode::kManual) return;
    m_controlMode = RingControlMode::kAutomatic;
    m_manualSince = 0U;
    Logger::info(kLogCategory,
                 "Manual LED control ended (automatic idle restored)");
}

void LedRing::setManualMood(const AuraMood mood) noexcept {
    if (static_cast<uint8_t>(mood) >= static_cast<uint8_t>(AuraMood::MAX)) {
        return;
    }
    m_manualMood = mood;
    m_manualSince = millis();  // each command refreshes the session
    Logger::info(kLogCategory, "Manual LED mood -> %s", auraMoodName(mood));
}

AuraMood LedRing::getManualMood() const noexcept {
    return m_manualMood;
}

bool LedRing::isManualControl() const noexcept {
    return m_controlMode == RingControlMode::kManual;
}

// ============================================================================
// Disco Mode
// ============================================================================

bool LedRing::setDiscoEnabled(const bool enabled) noexcept {
    if (m_discoEnabled == enabled) return false;
    m_discoEnabled = enabled;
    if (enabled) {
        m_discoAnim = static_cast<uint8_t>(random16() % kDiscoAnimCount);
        m_discoAnimSince = millis();
        m_discoAnimDuration = kDiscoAnimMinMs
            + (random16() % (kDiscoAnimMaxMs - kDiscoAnimMinMs + 1U));
        Logger::info(kLogCategory, "Disco Mode ON (brightness %u%%)", m_discoBrightness);
    } else {
        Logger::info(kLogCategory, "Disco Mode OFF (normal animations restored)");
    }
    m_animationTimer = millis();
    return true;
}

bool LedRing::isDiscoEnabled() const noexcept {
    return m_discoEnabled;
}

bool LedRing::isDiscoActive() const noexcept {
    return m_discoEnabled && !isEmergencyMood(m_mood);
}

void LedRing::setDiscoBrightness(const uint8_t percent) noexcept {
    m_discoBrightness = (percent < 10U) ? 10U : (percent > 100U) ? 100U : percent;
}

uint8_t LedRing::getDiscoBrightness() const noexcept {
    return m_discoBrightness;
}

void LedRing::renderDisco() noexcept {
    const unsigned long now = millis();

    if (!m_isEnabled) {
        if (!m_ledsBlack) clearRing();
        return;
    }
    m_ledsBlack = false;

    if ((now - m_animationTimer) < kFrameIntervalMs) {
        return;
    }
    m_animationTimer = now;
    ++m_animationFrame;

    // Rotate to a new animation every 15-20 s.
    if ((now - m_discoAnimSince) >= m_discoAnimDuration) {
        m_discoAnim = static_cast<uint8_t>((m_discoAnim + 1U) % kDiscoAnimCount);
        m_discoAnimSince = now;
        m_discoAnimDuration = kDiscoAnimMinMs
            + (random16() % (kDiscoAnimMaxMs - kDiscoAnimMinMs + 1U));
    }

    // Drive the user's disco brightness to a target (percent -> 0..255).
    const uint8_t target = static_cast<uint8_t>(
        (static_cast<uint16_t>(m_discoBrightness) * 255U) / 100U);
    const int16_t delta = static_cast<int16_t>(target) - static_cast<int16_t>(m_brightness);
    if (delta != 0) {
        const int16_t step = (delta > 0) ? 8 : -8;
        m_brightness = static_cast<uint8_t>(static_cast<int16_t>(m_brightness) + step);
        FastLED.setBrightness(m_brightness);
    }

    switch (m_discoAnim) {
        case 0:  playDiscoRainbowRotate(); break;
        case 1:  playDiscoRainbowSpiral(); break;
        case 2:  playDiscoColorWipe();     break;
        case 3:  playDiscoTheaterChase();  break;
        case 4:  playDiscoRainbowBreath(); break;
        case 5:  playDiscoFire();          break;
        case 6:  playDiscoOcean();         break;
        case 7:  playDiscoAurora();        break;
        case 8:  playDiscoSparkle();       break;
        default: playDiscoPulse();         break;
    }

    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        m_leds[i] = blend(m_leds[i], m_frame[i], kDiscoCrossFade);
    }
    FastLED.show();
}

// ============================================================================
// Mood renderers
// ============================================================================

void LedRing::playBoot() noexcept {
    const unsigned long t = millis() - m_moodSince;
    const CRGB col = moodColor(kBootColor);

    fill_solid(m_frame, LED_COUNT, CRGB::Black);

    if (t < kBootFillMs) {
        // A single LED appears, then the ring slowly fills.
        const uint8_t head = static_cast<uint8_t>((t / kBootStepMs) % LED_COUNT);
        const uint8_t filled = static_cast<uint8_t>(1 + (t * LED_COUNT) / kBootFillMs);
        for (uint8_t i = 0; i < LED_COUNT; ++i) {
            if (i < filled) {
                const uint8_t dist = (i + LED_COUNT - head) % LED_COUNT;
                const uint8_t amp = (dist < 4U)
                    ? static_cast<uint8_t>(255U - dist * 60U)
                    : 40U;
                m_frame[i] = col;
                m_frame[i].nscale8_video(amp);
            }
        }
        return;
    }

    // Hold a calm breathing glow until the system advances to READY/IDLE.
    const uint8_t br = beatsin8(static_cast<uint8_t>(60000UL / 2400UL), 40U, 180U);
    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(br);
}

void LedRing::playIdle() noexcept {
    const CRGB col = moodColor(kIdleColor);
    const unsigned long now = millis();

    // Very slow breathing, 5-7s.
    const uint8_t br = beatsin8(
        static_cast<uint8_t>(60000UL / kIdleBreathPeriodMs), 55U, 170U);

    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(br);

    // Occasional tiny travelling sparkle (only one LED), very subtle.
    if (now >= m_sparkleNext) {
        m_sparkleNext = now + kSparkleIntervalMinMs
            + (random16() % (kSparkleIntervalMaxMs - kSparkleIntervalMinMs));
        m_sparkleActive = true;
        m_sparkleHead = 0;
        m_sparkleUntil = now + kSparkleLen * kSparkleStepMs;
    }
    if (m_sparkleActive) {
        if (now >= m_sparkleUntil) {
            m_sparkleActive = false;
        } else {
            const uint8_t head = static_cast<uint8_t>(
                ((now - (m_sparkleUntil - kSparkleLen * kSparkleStepMs)) / kSparkleStepMs) % LED_COUNT);
            for (uint8_t i = 0; i < LED_COUNT; ++i) {
                const uint8_t d = (i + LED_COUNT - head) % LED_COUNT;
                if (d < kSparkleLen) {
                    const uint8_t a = static_cast<uint8_t>(kSparkleLen - d);
                    m_frame[i] = col;
                    m_frame[i].nscale8_video(static_cast<uint8_t>(a * 12U));
                }
            }
        }
    }
}

void LedRing::playListening() noexcept {
    const CRGB col = moodColor(kListeningColor);
    const uint8_t voice = m_voiceLevel;
    const uint8_t head = static_cast<uint8_t>((millis() / kListenStepMs) % LED_COUNT);

    fill_solid(m_frame, LED_COUNT, col);

    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t d = (i + LED_COUNT - head) % LED_COUNT;
        uint8_t amp;
        if (d == 0) {
            amp = 210U + scale8(voice, 45U);
        } else if (d < 6U) {
            amp = static_cast<uint8_t>(150U - d * 22U + scale8(voice, 30U));
        } else {
            amp = static_cast<uint8_t>(46U + scale8(voice, 70U));  // VU floor follows voice
        }
        m_frame[i].nscale8_video(amp);
    }
}

void LedRing::playRecording() noexcept {
    const CRGB col = moodColor(kRecordingColor);

    // Smooth breathing cyan — mic capture in progress.
    const uint8_t br = beatsin8(static_cast<uint8_t>(60000UL / 1600UL), 70U, 235U);
    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(br);
}

void LedRing::playPrivacy() noexcept {
    const CRGB col = moodColor(kPrivacyColor);

    // Solid red, no motion — microphone muted.
    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(255U);
}

void LedRing::playSetup() noexcept {
    const CRGB col = moodColor(kSetupColor);
    const unsigned long t = millis() - m_moodSince;

    // Purple breathing with a slow rotating highlight — provisioning mode.
    const uint8_t br = beatsin8(static_cast<uint8_t>(60000UL / 2200UL), 55U, 150U);
    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(br);

    const uint8_t head = static_cast<uint8_t>((t / 260UL) % LED_COUNT);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t d = (i + LED_COUNT - head) % LED_COUNT;
        if (d < 6U) {
            m_frame[i].nscale8_video(static_cast<uint8_t>(210U - d * 40U));
        }
    }
}

void LedRing::playThinking() noexcept {
    const CRGB col = moodColor(kThinkingColor);
    const unsigned long t = millis() - m_moodSince;

    // Fast rotating yellow "thinking" spinner (~450 ms lap).
    const uint8_t head = static_cast<uint8_t>((t / 28UL) % LED_COUNT);
    fill_solid(m_frame, LED_COUNT, CRGB::Black);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t d = (i + LED_COUNT - head) % LED_COUNT;
        if (d < 5U) {
            m_frame[i] = col;
            m_frame[i].nscale8_video(static_cast<uint8_t>(255U - d * 45U));
        }
    }
}

void LedRing::playProcessing() noexcept {
    const CRGB col = moodColor(kProcessingColor);
    const unsigned long t = millis() - m_moodSince;

    // Even faster rotating yellow-green spinner with a faint ring glow.
    const uint8_t head = static_cast<uint8_t>((t / 20UL) % LED_COUNT);
    fill_solid(m_frame, LED_COUNT, CRGB::Black);
    const uint8_t glow = beatsin8(static_cast<uint8_t>(60000UL / 900UL), 40U, 120U);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t d = (i + LED_COUNT - head) % LED_COUNT;
        if (d < 7U) {
            m_frame[i] = col;
            m_frame[i].nscale8_video(static_cast<uint8_t>(255U - d * 30U));
        } else {
            m_frame[i] = col;
            m_frame[i].nscale8_video(glow);
        }
    }
}

void LedRing::playSpeaking() noexcept {
    const CRGB col = moodColor(kSpeakingColor);
    const unsigned long t = millis() - m_moodSince;
    const uint8_t voice = m_voiceLevel;

    // Audio-reactive white: the ring brightness follows the (speaker) voice
    // level, layered with a fast speech pulse so it reads as "talking".
    const uint8_t beat = beatsin8(static_cast<uint8_t>(60000UL / 110UL), 55U, 255U);
    const uint8_t base = static_cast<uint8_t>(80U + scale8(voice, 140U));

    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        // Expanding speech waves add motion at the mic level.
        const uint8_t w = sin8(static_cast<uint8_t>((t / kSpeakClock) + i * 32U));
        uint16_t e = static_cast<uint16_t>(scale8(beat, base)) + scale8(w, 70U);
        if (e > 255U) e = 255U;
        m_frame[i] = col;
        m_frame[i].nscale8_video(static_cast<uint8_t>(e));
    }
}

void LedRing::playHappy() noexcept {
    const CRGB col = moodColor(kHappyColor);
    const unsigned long t = millis();

    // Warm gold, soft expanding pulse.
    const uint8_t br = beatsin8(static_cast<uint8_t>(60000UL / 2200UL), 70U, 230U);
    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(br);

    // Expanding ripple — brightness grows then decays around two heads.
    const uint8_t h = static_cast<uint8_t>((t / 70U) % LED_COUNT);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t d = (i + LED_COUNT - h) % LED_COUNT;
        if (d < 6U) {
            m_frame[i].nscale8_video(static_cast<uint8_t>(scale8(220U - d * 30U, 200U)));
        }
    }
}

void LedRing::playSuccess() noexcept {
    const unsigned long t = millis() - m_moodSince;
    if (t >= kSuccessDurationMs) {
        // Event finished: return the ring to its automatic idle state (OFF).
        setMood(AuraMood::IDLE);
        fill_solid(m_frame, LED_COUNT, CRGB::Black);
        clearRing();
        return;
    }

    const CRGB col = moodColor(kSuccessColor);
    // A green wave expands around the ring then returns.
    const uint8_t head = static_cast<uint8_t>((t / 35U) % LED_COUNT);
    fill_solid(m_frame, LED_COUNT, CRGB::Black);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t d = (i + LED_COUNT - head) % LED_COUNT;
        if (d < 12U) {
            const uint8_t a = static_cast<uint8_t>((12U - d) * 20U);
            m_frame[i] = col;
            m_frame[i].nscale8_video(a);
        }
    }
}

void LedRing::playReminder() noexcept {
    const CRGB col = moodColor(kReminderColor);
    const unsigned long t = millis() - m_moodSince;

    // Golden-yellow circular ripple, repeating slowly.
    const uint8_t pos = static_cast<uint8_t>((t / kReminderStepMs) % LED_COUNT);
    fill_solid(m_frame, LED_COUNT, CRGB::Black);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t d = (i + LED_COUNT - pos) % LED_COUNT;
        if (d < 6U) {
            const uint8_t a = static_cast<uint8_t>(scale8(255U - d * 38U, 210U) * (d == 0U ? 255U : 120U) / 255U);
            m_frame[i] = col;
            m_frame[i].nscale8_video(a);
        } else if (d >= 8U && d < 12U) {
            m_frame[i] = col;
            m_frame[i].nscale8_video(30U);
        }
    }
}

void LedRing::playWarning() noexcept {
    const CRGB col = moodColor(kWarningColor);
    const uint32_t tt = millis() % kWarningBeatMs;

    // Orange heartbeat — two soft thumps per beat, no flashing.
    uint8_t e = 0;
    if (tt < 240U) {
        e = static_cast<uint8_t>(255U - (tt * 255U / 240U));
    } else if (tt > (kWarningBeatMs - 400U)) {
        const uint32_t k = tt - (kWarningBeatMs - 400U);
        e = static_cast<uint8_t>(200U - (k * 200U / 400U));
    }
    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(e);
}

void LedRing::playError() noexcept {
    const CRGB col = moodColor(kErrorColor);
    // Fast red flashing, 5 Hz (~200 ms period).
    const bool on = (millis() % 200UL) < 100UL;
    const uint8_t amp = on ? 255U : 14U;
    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(amp);
}

void LedRing::playCritical() noexcept {
    const CRGB col = moodColor(kCriticalColor);
    // Dark red, very slow pulse (<= 2 Hz).
    const uint8_t br = beatsin8(static_cast<uint8_t>(60000UL / kCriticalPeriodMs), 40U, 130U);
    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(br);
}

void LedRing::playOta() noexcept {
    const CRGB col = moodColor(kOtaColor);
    const unsigned long t = millis() - m_moodSince;

    const uint8_t arc = static_cast<uint8_t>(
        (static_cast<uint16_t>(m_otaProgress) * LED_COUNT) / 100U);
    fill_solid(m_frame, LED_COUNT, CRGB::Black);

    // Purple energy rotates; the arc length tracks progress.
    const uint8_t head = static_cast<uint8_t>((t / kOtaStepMs) % LED_COUNT);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t d = (i + LED_COUNT - head) % LED_COUNT;
        if (d < arc) {
            m_frame[i] = col;
            m_frame[i].nscale8_video(d == 0U ? 255U : 150U);
        }
    }
    if (arc < LED_COUNT) {
        // A bright pulse leads the arc's end.
        const uint8_t lead = static_cast<uint8_t>((head + LED_COUNT + arc) % LED_COUNT);
        m_frame[lead] = col;
        m_frame[lead].nscale8_video(beatsin8(80U, 120U, 255U));
    }
}

void LedRing::playOffline() noexcept {
    const CRGB col = moodColor(kOfflineColor);
    // Dim white with a tiny heartbeat — alive, not broken.
    const uint8_t br = beatsin8(static_cast<uint8_t>(60000UL / 2600UL), 30U, 110U);
    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(br);
}

void LedRing::playSleep() noexcept {
    const CRGB col = moodColor(kSleepColor);
    // Near-off: a faint heartbeat every ~10s.
    const uint32_t tt = millis() % 10000UL;
    uint8_t e = 0;
    if (tt < 500UL) {
        const uint8_t k = static_cast<uint8_t>(255U - (tt * 255U / 500UL));
        e = static_cast<uint8_t>(scale8(k, 90U));
    }
    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(e);
}

void LedRing::playWake() noexcept {
    const unsigned long t = millis() - m_moodSince;
    if (t >= kWakeDurationMs) {
        // Event finished: return the ring to its automatic idle state (OFF).
        setMood(AuraMood::IDLE);
        fill_solid(m_frame, LED_COUNT, CRGB::Black);
        clearRing();
        return;
    }

    const CRGB col = moodColor(kWakeColor);
    // Small expanding blue pulse from a single origin.
    const uint8_t rad = static_cast<uint8_t>((t / 55UL) % LED_COUNT);
    fill_solid(m_frame, LED_COUNT, CRGB::Black);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t d = (i + LED_COUNT - rad) % LED_COUNT;
        if (d < 6U) {
            m_frame[i] = col;
            m_frame[i].nscale8_video(static_cast<uint8_t>(scale8(255U - d * 40U, 230U)));
        }
    }
}

void LedRing::playWifiConnecting() noexcept {
    const CRGB col = moodColor(kWifiConnectingColor);
    // Soft white segment sweeping, pulsing gently.
    const uint8_t head = static_cast<uint8_t>((millis() / kWifiStepMs) % LED_COUNT);
    fill_solid(m_frame, LED_COUNT, CRGB::Black);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t d = (i + LED_COUNT - head) % LED_COUNT;
        if (d < 5U) {
            m_frame[i] = col;
            m_frame[i].nscale8_video(static_cast<uint8_t>(140U - d * 24U));
        }
    }
}

void LedRing::playWifiConnected() noexcept {
    const unsigned long t = millis() - m_moodSince;
    if (t >= kWifiConnectedDurationMs) {
        // Brief status animation finished: back to automatic idle (OFF).
        setMood(AuraMood::IDLE);
        fill_solid(m_frame, LED_COUNT, CRGB::Black);
        clearRing();
        return;
    }

    const CRGB col = moodColor(kWifiConnectedColor);
    const uint8_t br = beatsin8(120U, 120U, 255U);
    fill_solid(m_frame, LED_COUNT, col);
    for (uint8_t i = 0; i < LED_COUNT; ++i) m_frame[i].nscale8_video(br);
}

// ============================================================================
// Disco Mode animations
//
// Each renderer writes a fresh target into m_frame[]. They are non-blocking,
// derive their state purely from the running clock, use no delay(), and are
// throttled to the shared 50 FPS gate by renderDisco(). Brightness is applied
// globally in renderDisco().
// ============================================================================

void LedRing::playDiscoRainbowRotate() noexcept {
    const unsigned long t = millis() / 4U;
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        m_frame[i] = CHSV(static_cast<uint8_t>(t + i * 16U), 255U, 255U);
    }
}

void LedRing::playDiscoRainbowSpiral() noexcept {
    const unsigned long t = millis();
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t hue = static_cast<uint8_t>(i * 24U + t / 6U);
        const uint8_t val = sin8(static_cast<uint8_t>(i * 32U + t / 3U));
        m_frame[i] = CHSV(hue, 255U, val);
    }
}

void LedRing::playDiscoColorWipe() noexcept {
    const unsigned long t = millis();
    const uint8_t head = static_cast<uint8_t>((t / 18U) % LED_COUNT);
    const uint8_t hue = static_cast<uint8_t>(t / 140U);
    fill_solid(m_frame, LED_COUNT, CRGB::Black);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t d = (i + LED_COUNT - head) % LED_COUNT;
        if (d <= 3U) {
            m_frame[i] = CHSV(hue, 255U, static_cast<uint8_t>(255U - d * 55U));
        }
    }
}

void LedRing::playDiscoTheaterChase() noexcept {
    const unsigned long t = millis() / 18U;
    const uint8_t hue = static_cast<uint8_t>(millis() / 4U);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const bool lit = (((i + (t % 3U)) % 3U) == 0U);
        const CRGB c = lit ? static_cast<CRGB>(CHSV(hue + static_cast<uint8_t>(i * 16U), 255U, 235U)) : CRGB::Black;
        m_frame[i] = c;
    }
}

void LedRing::playDiscoRainbowBreath() noexcept {
    const uint8_t br = beatsin8(4U, 45U, 255U);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        m_frame[i] = CHSV(static_cast<uint8_t>(i * 16U + millis() / 8U), 255U, br);
    }
}

void LedRing::playDiscoFire() noexcept {
    const unsigned long t = millis() / 7U;
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t flicker = static_cast<uint8_t>(40U + (random16() & 0xFFU) / 3U);
        const uint8_t v = qadd8(sin8(static_cast<uint8_t>(t + i * 53U)), flicker);
        m_frame[i] = CHSV(static_cast<uint8_t>(16U - (i * 3U)), 220U, v);
    }
}

void LedRing::playDiscoOcean() noexcept {
    const unsigned long t = millis();
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t v = sin8(static_cast<uint8_t>(i * 40U + t / 3U));
        const uint8_t h2 = static_cast<uint8_t>(t / 30U + 120U);
        m_frame[i] = CHSV(h2, 220U, v);
    }
}

void LedRing::playDiscoAurora() noexcept {
    const unsigned long t = millis();
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t a = qadd8(sin8(static_cast<uint8_t>(i * 40U + t / 2U)),
                                sin8(static_cast<uint8_t>(i * 18U + t / 5U)));
        const uint8_t v = static_cast<uint8_t>(a / 2U);
        m_frame[i] = CHSV(static_cast<uint8_t>(60U + (t / 40U)), 230U, v);
    }
}

void LedRing::playDiscoSparkle() noexcept {
    fill_solid(m_frame, LED_COUNT, CRGB::Black);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        if (random8() < 38U) {
            m_frame[i] = CHSV(static_cast<uint8_t>(random8()), 200U, 255U);
        }
    }
}

void LedRing::playDiscoPulse() noexcept {
    const uint8_t br = beatsin8(6U, 40U, 255U);
    const unsigned long t = millis() / 16U;
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        m_frame[i] = CHSV(static_cast<uint8_t>(t + i * 28U), 255U, br);
    }
}