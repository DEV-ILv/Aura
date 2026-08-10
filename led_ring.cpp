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

// ---- manual device-control session ---------------------------------------
// A manual test session auto-expires so the ring can never be left glowing
// forever after a user stops interacting with the device-control page.
constexpr uint32_t kManualControlTimeoutMs = 60000UL;

// ---- transients (these return to IDLE) ----
constexpr uint32_t kSuccessDurationMs = 1700UL;
constexpr uint32_t kWakeDurationMs = 1100UL;
constexpr uint32_t kWifiConnectedDurationMs = 1300UL;

// ---- normal status palette (FINAL SOLID-COLOUR SYSTEM) --------------------
// The 16-LED ring is ONE indicator: every AURA state paints ALL 16 LEDs in a
// single, distinct SOLID colour. There is NO per-LED movement, chase, comet,
// tail, pulse, breathe, or rotation in the normal status path. Colours follow
// the FINAL spec exactly and each status has its own recognisable hue.
const CRGB kIdleColor(0x00, 0x80, 0xFF);          // Blue   #0080FF  IDLE/READY
const CRGB kListeningColor(0x00, 0xFF, 0xFF);     // Cyan   #00FFFF  LISTENING
const CRGB kRecordingColor(0x00, 0xFF, 0x40);     // Green  #00FF40  RECORDING
const CRGB kThinkingColor(0xFF, 0xFF, 0x00);      // Yellow #FFFF00  THINKING
const CRGB kProcessingColor(0xFF, 0xFF, 0x00);    // Yellow #FFFF00  PROCESSING
const CRGB kSpeakingColor(0xFF, 0xFF, 0xFF);      // White  #FFFFFF  SPEAKING
const CRGB kSetupColor(0x80, 0x00, 0xFF);         // Purple #8000FF  SETUP
const CRGB kPrivacyColor(0xFF, 0x00, 0xAA);       // Magenta#FF00AA  PRIVACY/MUTED
const CRGB kErrorColor(0xFF, 0x00, 0x00);         // Red    #FF0000  ERROR
const CRGB kOtaColor(0xFF, 0x80, 0x00);           // Orange #FF8000  OTA

// Non-status moods keep their own distinct SOLID colour so they never collide
// with the status table, and are likewise rendered as an all-LED constant.
const CRGB kIdlePastel(0x00, 0x80, 0xFF);         // reserved/blue idle variant
const CRGB kBootColor(0x00, 0x80, 0xFF);          // Blue (boot sweep base)
const CRGB kHappyColor(0xFF, 0xD7, 0x00);         // warm gold        HAPPY
const CRGB kSuccessColor(0x00, 0xFF, 0xAA);       // mint green       SUCCESS
const CRGB kReminderColor(0xFF, 0xAA, 0x00);      // amber            REMINDER
const CRGB kWarningColor(0xFF, 0x40, 0x00);       // red-orange       WARNING
const CRGB kCriticalColor(0x8A, 0x00, 0x00);      // dark red         CRITICAL
const CRGB kOfflineColor(0x60, 0x70, 0x85);       // slate            OFFLINE
const CRGB kSleepColor(0x40, 0x4A, 0x60);         // dim navy         SLEEP
const CRGB kWakeColor(0x00, 0xAF, 0xFF);          // azure            WAKE
const CRGB kWifiConnectingColor(0x40, 0x80, 0xE0);// steel blue       WIFI_CONNECTING
const CRGB kWifiConnectedColor(0x00, 0xFF, 0x80); // spring green     WIFI_CONNECTED

// ---- synchronised brightness flicker (status system only) ------------------
// The entire ring shares ONE brightness level; individual LEDs NEVER change
// independently. The level performs a small random walk (not a sine) so the
// ring reads as a subtle electronic energy flicker rather than a flashing
// warning or a breathing pulse.
constexpr uint8_t kFlickerMacroIntervalMinMs = 50UL;  // typical recompute spacing
constexpr uint8_t kFlickerMacroIntervalMaxMs = 150UL;
constexpr uint8_t kFlickerNormalLoPercent  = 86U;   // ~85-100% for normal states
constexpr uint8_t kFlickerNormalHiPercent  = 100U;
constexpr uint8_t kFlickerSubtleLoPercent  = 92U;   // IDLE / PRIVACY / quiet states
constexpr uint8_t kFlickerSubtleHiPercent  = 100U;
constexpr uint8_t kFlickerErrorLoPercent   = 55U;   // ERROR: deeper, quicker flicker
constexpr uint8_t kFlickerErrorHiPercent   = 100U;
constexpr uint8_t kFlickerErrorIntervalMinMs = 35UL;
constexpr uint8_t kFlickerErrorIntervalMaxMs = 90UL;
constexpr uint8_t kFlickerErrorStepMax       = 12U;
constexpr uint8_t kFlickerNormalStepMax      = 6;   // small random-walk step

// Maximum per-frame brightness easing rate (0..255 level units).
constexpr uint8_t kFlickerEaseStep = 10U;

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
    , m_flickerLevel(255U)
    , m_flickerTarget(255U)
    , m_flickerNext(0U)
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

    // A short cross-fade eases between two SOLID colours on state change.
    // Every LED is still a constant colour; the blend only applies while a
    // transition is in progress so there is no chasing or movement at all.
    const uint8_t fade = kCrossFade;
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        m_leds[i] = blend(m_leds[i], m_frame[i], fade);
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
    // Reset the shared flicker so a fresh SOLID colour starts near full
    // brightness and its own random-walk path (no stale state from old mood).
    m_flickerLevel = 255U;
    m_flickerTarget = 255U;
    m_flickerNext = 0U;
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
// Normal SOLID status system (independent from Disco Mode)
//
// The 16-LED ring is ONE indicator. Each AuraMood fills ALL 16 LEDs with a
// single distinct SOLID colour; there is NO per-LED movement, chase, comet,
// tail, pulse, breathe, or rotation in this path. The only variation allowed
// is a subtle, fully-synchronised brightness flicker over the whole ring.
// ============================================================================

bool LedRing::isSequentialAnimation(const AuraMood mood) const noexcept {
    // Retained as a marker only: the normal status path no longer performs any
    // sequential/moving animation (all states are solid). Every mood reports
    // false so the caller always uses the shared solid cross-fade.
    (void)mood;
    return false;
}

void LedRing::renderSolidStatus(const CRGB color, const uint8_t loPercent,
                                const uint8_t hiPercent, const uint8_t stepMax,
                                const uint8_t intervalMinMs,
                                const uint8_t intervalMaxMs) noexcept {
    const unsigned long now = millis();

    // Pick a new random-walk target on a slow cadence. The walk stays inside
    // [loPercent, hiPercent] and never jumps by more than `stepMax` points so
    // the ring shimmers like subtle energy instead of flashing.
    if (m_flickerNext == 0UL || now >= m_flickerNext) {
        if (m_flickerNext != 0UL) {
            const uint8_t lo = static_cast<uint8_t>((static_cast<uint16_t>(loPercent) * 255U) / 100U);
            const uint8_t hi = static_cast<uint8_t>((static_cast<uint16_t>(hiPercent) * 255U) / 100U);
            int16_t target = static_cast<int16_t>(m_flickerTarget)
                + static_cast<int16_t>(random8(stepMax * 2U + 1U)) - static_cast<int16_t>(stepMax);
            if (target < static_cast<int16_t>(lo)) target = static_cast<int16_t>(lo);
            if (target > static_cast<int16_t>(hi)) target = static_cast<int16_t>(hi);
            m_flickerTarget = static_cast<uint8_t>(target);
        }
        m_flickerNext = now + intervalMinMs
            + static_cast<unsigned long>(random16(intervalMaxMs - intervalMinMs + 1UL));
    }

    // Ease the shared level toward the target so the whole ring changes
    // together, in tiny synchronized steps (never per-LED).
    if (m_flickerLevel < m_flickerTarget) {
        const uint8_t next = static_cast<uint8_t>(static_cast<uint16_t>(m_flickerLevel) + kFlickerEaseStep);
        m_flickerLevel = (next > m_flickerTarget) ? m_flickerTarget : next;
    } else if (m_flickerLevel > m_flickerTarget) {
        const uint8_t next = static_cast<uint8_t>(static_cast<uint16_t>(m_flickerLevel) - kFlickerEaseStep);
        m_flickerLevel = (next < m_flickerTarget) ? m_flickerTarget : next;
    }

    CRGB solid = color;
    solid.nscale8_video(m_flickerLevel);
    fill_solid(m_frame, LED_COUNT, solid);
}

// ---- status moods (SOLID, exact spec colours) -----------------------------

void LedRing::playIdle() noexcept {
    // ALL 16 LEDs: SOLID BLUE (very subtle, slow flicker).
    renderSolidStatus(kIdleColor, kFlickerSubtleLoPercent, kFlickerSubtleHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playListening() noexcept {
    // ALL 16 LEDs: SOLID CYAN.
    renderSolidStatus(kListeningColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playRecording() noexcept {
    // ALL 16 LEDs: SOLID GREEN.
    renderSolidStatus(kRecordingColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playThinking() noexcept {
    // ALL 16 LEDs: SOLID YELLOW.
    renderSolidStatus(kThinkingColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playProcessing() noexcept {
    // ALL 16 LEDs: SOLID YELLOW.
    renderSolidStatus(kProcessingColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playSpeaking() noexcept {
    // ALL 16 LEDs: SOLID WHITE.
    renderSolidStatus(kSpeakingColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playSetup() noexcept {
    // ALL 16 LEDs: SOLID PURPLE.
    renderSolidStatus(kSetupColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playPrivacy() noexcept {
    // ALL 16 LEDs: SOLID MAGENTA (very subtle).
    renderSolidStatus(kPrivacyColor, kFlickerSubtleLoPercent, kFlickerSubtleHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playError() noexcept {
    // ALL 16 LEDs: SOLID RED with a slightly deeper, faster flicker so errors
    // are noticed, while the whole ring still stays perfectly in sync.
    renderSolidStatus(kErrorColor, kFlickerErrorLoPercent, kFlickerErrorHiPercent,
                      kFlickerErrorStepMax, kFlickerErrorIntervalMinMs, kFlickerErrorIntervalMaxMs);
}

void LedRing::playOta() noexcept {
    // ALL 16 LEDs: SOLID ORANGE.
    renderSolidStatus(kOtaColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

// ---- non-status moods (also SOLID, kept distinct from the status table) ---

void LedRing::playBoot() noexcept {
    // SOLID blue during the boot window (matches the IDLE identity).
    renderSolidStatus(kBootColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playHappy() noexcept {
    renderSolidStatus(kHappyColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playSuccess() noexcept {
    const unsigned long t = millis() - m_moodSince;
    if (t >= kSuccessDurationMs) {
        setMood(AuraMood::IDLE);
        fill_solid(m_frame, LED_COUNT, CRGB::Black);
        clearRing();
        return;
    }
    renderSolidStatus(kSuccessColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playReminder() noexcept {
    renderSolidStatus(kReminderColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playWarning() noexcept {
    renderSolidStatus(kWarningColor, kFlickerErrorLoPercent, kFlickerErrorHiPercent,
                      kFlickerErrorStepMax, kFlickerErrorIntervalMinMs, kFlickerErrorIntervalMaxMs);
}

void LedRing::playCritical() noexcept {
    renderSolidStatus(kCriticalColor, kFlickerErrorLoPercent, kFlickerErrorHiPercent,
                      kFlickerErrorStepMax, kFlickerErrorIntervalMinMs, kFlickerErrorIntervalMaxMs);
}

void LedRing::playOffline() noexcept {
    renderSolidStatus(kOfflineColor, kFlickerSubtleLoPercent, kFlickerSubtleHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playSleep() noexcept {
    renderSolidStatus(kSleepColor, kFlickerSubtleLoPercent, kFlickerSubtleHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playWake() noexcept {
    const unsigned long t = millis() - m_moodSince;
    if (t >= kWakeDurationMs) {
        setMood(AuraMood::IDLE);
        fill_solid(m_frame, LED_COUNT, CRGB::Black);
        clearRing();
        return;
    }
    renderSolidStatus(kWakeColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playWifiConnecting() noexcept {
    renderSolidStatus(kWifiConnectingColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
}

void LedRing::playWifiConnected() noexcept {
    const unsigned long t = millis() - m_moodSince;
    if (t >= kWifiConnectedDurationMs) {
        setMood(AuraMood::IDLE);
        fill_solid(m_frame, LED_COUNT, CRGB::Black);
        clearRing();
        return;
    }
    renderSolidStatus(kWifiConnectedColor, kFlickerNormalLoPercent, kFlickerNormalHiPercent,
                      kFlickerNormalStepMax, kFlickerMacroIntervalMinMs, kFlickerMacroIntervalMaxMs);
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