#ifndef AURA_HABIT_MANAGER_H
#define AURA_HABIT_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

enum class HabitSchedule : uint8_t { DAILY, WEEKLY, MONTHLY, CUSTOM };

struct HabitEntry {
    String id;
    String name;
    String description;
    HabitSchedule schedule;
    uint8_t customDays;         ///< Bitmask for CUSTOM schedule (bit 0=Mon..6=Sun)
    uint16_t streak;
    uint16_t longestStreak;
    uint32_t totalCompletions;
    float successRate;
    bool reminderEnabled;
    unsigned long createdAt;
    unsigned long lastCompletedDate;

    HabitEntry() noexcept : schedule(HabitSchedule::DAILY), customDays(0), streak(0),
        longestStreak(0), totalCompletions(0), successRate(0.0f),
        reminderEnabled(false), createdAt(0), lastCompletedDate(0) {}
};

class HabitManager {
public:
    HabitManager() noexcept;
    ~HabitManager() noexcept;

    HabitManager(const HabitManager&) = delete;
    HabitManager& operator=(const HabitManager&) = delete;
    HabitManager(HabitManager&&) = delete;
    HabitManager& operator=(HabitManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    [[nodiscard]] String createHabit(const String& name, HabitSchedule schedule,
                                       const String& description = "", bool reminderEnabled = false) noexcept;
    [[nodiscard]] bool updateHabit(const String& id, const String& name, const String& description) noexcept;
    [[nodiscard]] bool deleteHabit(const String& id) noexcept;
    [[nodiscard]] bool completeHabit(const String& id) noexcept;

    [[nodiscard]] HabitEntry getHabit(const String& id) const noexcept;
    [[nodiscard]] const std::vector<HabitEntry>& getAllHabits() const noexcept;
    [[nodiscard]] std::vector<HabitEntry> getDueHabits() const noexcept;
    [[nodiscard]] size_t habitCount() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;

private:
    static constexpr const char* kLogCategory = "HabitManager";
    static constexpr size_t kMaxHabits = HABIT_MAX_COUNT;

    String generateId() noexcept;
    size_t findHabit(const String& id) const noexcept;
    bool isDue(const HabitEntry& h) const noexcept;

    bool m_initialized;
    bool m_dirty;
    std::vector<HabitEntry> m_habits;
    unsigned long m_lastIdCounter;
};

extern HabitManager habitManager;

#endif
