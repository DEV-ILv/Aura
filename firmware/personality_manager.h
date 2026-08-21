#ifndef AURA_PERSONALITY_MANAGER_H
#define AURA_PERSONALITY_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

/**
 * @struct PersonalityProfile
 * @brief Defines an AI personality profile
 */
struct PersonalityProfile {
    String id;                  ///< Profile identifier
    String name;                ///< Display name (e.g. "Jarvis")
    String systemPrompt;        ///< System prompt for Gemini API
    String voice;               ///< TTS voice name
    uint32_t ledTheme;          ///< LED theme color (RGB)
    String startupSound;        ///< Startup sound filename
    String responseStyle;       ///< Response style description
    bool enabled;               ///< Whether this profile is selectable

    PersonalityProfile() noexcept
        : ledTheme(0x00FF00), enabled(true) {}
};

/**
 * @class PersonalityManager
 * @brief Manages AI personality profiles
 *
 * Profiles define system prompts, voice settings, LED themes,
 * startup sounds, and response styles. Built-in profiles:
 * Jarvis, Professional, Teacher, Programmer, Friendly, Minimal.
 */
class PersonalityManager {
public:
    PersonalityManager() noexcept;
    ~PersonalityManager() noexcept;

    PersonalityManager(const PersonalityManager&) = delete;
    PersonalityManager& operator=(const PersonalityManager&) = delete;
    PersonalityManager(PersonalityManager&&) = delete;
    PersonalityManager& operator=(PersonalityManager&&) = delete;

    /**
     * @brief Initialize personality manager
     * @return true if initialized
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Update personality manager
     */
    void update() noexcept;

    /**
     * @brief Activate a personality profile
     * @param profileId Profile ID
     * @return true if activated
     */
    [[nodiscard]] bool activateProfile(const String& profileId) noexcept;

    /**
     * @brief Get active profile
     * @return Const reference to active profile
     */
    [[nodiscard]] const PersonalityProfile& getActiveProfile() const noexcept;

    /**
     * @brief Get a profile by ID
     * @param profileId Profile ID
     * @return PersonalityProfile (empty id if not found)
     */
    [[nodiscard]] PersonalityProfile getProfile(const String& profileId) const noexcept;

    /**
     * @brief Get all available profiles
     * @return Vector of all profiles
     */
    [[nodiscard]] const std::vector<PersonalityProfile>& getAllProfiles() const noexcept;

    /**
     * @brief Add a custom profile
     * @param profile Profile to add
     * @return true if added
     */
    [[nodiscard]] bool addProfile(const PersonalityProfile& profile) noexcept;

    /**
     * @brief Remove a custom profile
     * @param profileId Profile ID
     * @return true if removed
     */
    [[nodiscard]] bool removeProfile(const String& profileId) noexcept;

    /**
     * @brief Get active profile ID
     * @return Profile ID string
     */
    [[nodiscard]] String getActiveProfileId() const noexcept;

    /**
     * @brief Check if initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Persist all profiles to storage.
     * @return true if saved successfully.
     */
    [[nodiscard]] bool save() noexcept;

    // Voice Personality Extensions
    [[nodiscard]] bool setVoiceTone(const String& toneId) noexcept;
    [[nodiscard]] String getVoiceTone() const noexcept;
    [[nodiscard]] bool setSpeechRate(const String& rate) noexcept;  // "slow", "normal", "fast"
    [[nodiscard]] String getSpeechRate() const noexcept;
    [[nodiscard]] bool setVoiceGender(const String& gender) noexcept;  // "male", "female", "neutral"
    [[nodiscard]] String getVoiceGender() const noexcept;
    [[nodiscard]] String applyVoiceModulation(const String& text) const noexcept;
    [[nodiscard]] std::vector<String> getAvailableTones() const noexcept;

private:
    static constexpr const char* kLogCategory = "PersonalityManager";
    static constexpr const char* kStoragePath = "/personality.json";
    static constexpr size_t kMaxProfiles = PERSONALITY_MAX_COUNT;
    static constexpr size_t kMaxTones = 10;

    void loadBuiltinProfiles() noexcept;
    bool saveProfileState() noexcept;
    bool loadProfileState() noexcept;
    size_t findProfile(const String& id) const noexcept;

    String applyTone(const String& text) const noexcept;
    String applyRate(const String& text) const noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<PersonalityProfile> m_profiles;
    size_t m_activeProfileIndex;
    String m_voiceTone;
    String m_speechRate;
    String m_voiceGender;
};

extern PersonalityManager personalityManager;

#endif // AURA_PERSONALITY_MANAGER_H
