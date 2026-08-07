#ifndef AURA_FACE_H
#define AURA_FACE_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

/**
 * @enum FaceExpression
 * @brief High-level AURA presence states rendered by AuraFace.
 *
 * Movement-first: the face communicates state through eye shape, pupil
 * motion and micro-animations before any text is considered.
 */
enum class FaceExpression : uint8_t {
    IDLE = 0,        ///< Relaxed eyes, gentle breathing
    LISTENING,       ///< Attentive eyes, subtle pulse
    THINKING,        ///< Eyes scan, tiny orbiting dots
    SPEAKING,        ///< Mouth animates, eyes engaged
    HAPPY,           ///< Softer arched eyes
    CONCERNED,       ///< Narrowed eyes
    CRITICAL,        ///< Calm flat eyes + warning mark
    OFFLINE,         ///< Closed/dim eyes
    MAX
};

/**
 * @enum FaceEvent
 * @brief Short-lived micro-animations triggered by system events.
 *
 * These are subtle glyphs/gestures drawn in the corners so they never
 * disturb the face itself.
 */
enum class FaceEvent : uint8_t {
    NONE = 0,
    WIFI_CONNECTED,  ///< Small wifi glyph + gentle arc
    TOUCH,           ///< Quick acknowledgement pulse/blink
    STORAGE,         ///< Storage glyph flash
    OTA,             ///< Small progress dots
    WAKE             ///< Gentle wake pulse
};

/**
 * @class AuraFace
 * @brief Non-blocking, lightweight OLED "presence" renderer.
 *
 * Renders the JARVIS-inspired AURA face into an Adafruit_SH1106G buffer.
 * All motion is deterministic per timestamp and cheap (no allocations,
 * fixed-point geometry, sinf/cosf only). Random ambient behaviour uses an
 * internal xorshift PRNG so the idle never looks scripted.
 */
class AuraFace {
public:
    AuraFace() noexcept;

    void setDisplay(Adafruit_SH1106G* display) noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return m_display != nullptr; }

    /**
     * @brief Request a new expression. Transitions ease smoothly.
     */
    void setExpression(FaceExpression expr) noexcept;
    [[nodiscard]] FaceExpression getExpression() const noexcept { return m_targetExpression; }

    /**
     * @brief Trigger a short-lived event micro-animation.
     */
    void notify(FaceEvent event) noexcept;

    /**
     * @brief Optionally modulate eye openness with mic/attention level.
     * @param level 0..255
     */
    void setAttention(uint8_t level) noexcept;

    /**
     * @brief Reset all state (e.g. before a fresh boot sequence).
     */
    void reset() noexcept;

    /**
     * @brief Draw the current frame into the display buffer.
     */
    void render(unsigned long now) noexcept;

private:
    // ---- drawing helpers -------------------------------------------------
    void drawEye(int16_t cx, int16_t cy, float openness,
                 int16_t pupilDx, int16_t pupilDy) noexcept;
    void drawArc(int16_t cx, int16_t cy, int16_t r,
                 float a0, float a1, uint8_t thickness) noexcept;
    void drawMouth(float amount, unsigned long now) noexcept;
    void drawThinkingDots(unsigned long now) noexcept;
    void drawListeningPulse(unsigned long now) noexcept;
    void drawBrows(float tilt) noexcept;
    void drawEventGlyph(unsigned long now) noexcept;
    void drawClosedEyes(bool narrow) noexcept;

    // ---- motion helpers --------------------------------------------------
    static uint32_t nextRandom() noexcept;         // xorshift32
    static float randUnit() noexcept;
    static float clamp01(float v) noexcept;
    static float easeOutCubic(float t) noexcept;

    // ---- ambient behaviour ------------------------------------------------
    void updateBlink(unsigned long now) noexcept;
    void updatePupil(unsigned long now) noexcept;

    // ---- state ------------------------------------------------------------
    Adafruit_SH1106G* m_display;

    FaceExpression m_targetExpression;

    float m_openness;          // eased eye openness 0..1
    float m_attention;         // 0..1 from setAttention

    int16_t m_pupilX;
    int16_t m_pupilY;
    int16_t m_pupilTargetX;
    int16_t m_pupilTargetY;
    uint32_t m_nextGlanceAt;

    uint32_t m_nextBlinkAt;
    uint32_t m_blinkStart;
    bool     m_blinking;

    uint32_t m_nextPulseAt;    // listening pulse timer
    uint32_t m_pulseStart;

    uint32_t m_eventStart;
    uint8_t  m_event;          // FaceEvent

    uint32_t m_lastRender;

    // ---- geometry constants ----------------------------------------------
    static constexpr int16_t  kFaceCx = 64;
    static constexpr int16_t  kFaceCy = 30;
    static constexpr int16_t  kEyeSep = 22;      // eye cx offset from centre
    static constexpr int16_t  kEyeRx  = 8;
    static constexpr int16_t  kEyeRy  = 9;
    static constexpr int16_t  kPupilMaxX = 4;
    static constexpr int16_t  kPupilMaxY = 3;
    static constexpr int16_t  kMouthCy  = 46;
    static constexpr int16_t  kMouthHalfW = 10;

    // ---- timing constants -------------------------------------------------
    static constexpr uint32_t kBlinkIntervalMinMs = 2600;
    static constexpr uint32_t kBlinkIntervalMaxMs = 5400;
    static constexpr uint32_t kBlinkDurationMs    = 150;
    static constexpr uint32_t kGlanceIntervalMs   = 2200;
    static constexpr uint32_t kEventDurationMs    = 950;
    static constexpr uint32_t kPulseIntervalMs    = 2400;
};

extern AuraFace auraFace;

#endif // AURA_FACE_H
