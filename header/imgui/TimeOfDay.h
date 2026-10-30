// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "imgui/SolarPosition.h"

#include <array>
#include <bit>
#include <cstdint>
#include <string>

namespace ux3d::imgui {

// A6.1 time of day (docs/ASOBO_EXTENSIONS_PHASE_6.md), scene-level like the sun: part of SceneEnvironmentState.
//
// Conventions:
//  * The clock is local civil time at the location: date + hours (fractional, [0, 24)) + the UTC offset in
//    hours, so UTC = hours - utcOffset on that date. Latitude is positive north, longitude positive east.
//  * heading is the true compass direction the model's nose points to, in degrees (0 = north, 90 = east). For
//    MSFS-exported aircraft the nose points to glTF +Z and the aircraft's left side to glTF +X (checked on the
//    E195 exterior: nose gear at z = +9.7, rudder at z = -22.7, left elevator at x = +2.7). Seen from above
//    (+Y toward the viewer) +Z is the nose and +X the left wing, so a compass bearing B maps to the A3.1
//    azimuth (around +Y, from +Z towards +X)   azimuth = heading - B   (mod 360).
//  * Time of day mode off (the default) changes nothing; on, the sun is on and its direction comes from the
//    solar position (colour and intensity stay the Manual sun's), faded to 0 across the apparent elevations
//    -1..+1 degrees.
enum class TimePreset : int { Custom = 0, Dawn, Morning, Noon, Afternoon, Dusk, Night, Count };

inline constexpr std::array<const char*, static_cast<size_t>(TimePreset::Count)> kTimePresetNames = {
    "custom", "dawn", "morning", "noon", "afternoon", "dusk", "night"};

// Day/night switch of ASOBO_material_emissive (SceneEnvironmentState::timeOfDay).
enum class DayNightMode : int { Day = 0, Night = 1, Automatic = 2 };

struct TimeOfDayState {
    bool enabled = false;          // Sun source: false = Manual, true = Time of day
    int year = 2026;               // local date at the location (default: mid April)
    int month = 4;
    int day = 15;
    double hours = 12.0;           // local clock time; ignored while a preset other than Custom is selected
    float utcOffsetHours = -7.0f;  // default: Pacific daylight time
    float latitude = 47.45f;       // default: Seattle-Tacoma airport (KSEA)
    float longitude = -122.31f;
    float heading = 0.0f;          // true degrees the model's nose points to
    int preset = 0;                // TimePreset; a preset follows the date and the location
    bool refraction = true;        // the lit sun's elevation includes atmospheric refraction
    float nightThreshold = -6.0f;  // Automatic day/night: night below this apparent sun elevation (civil twilight)
    bool playing = false;          // session state: advance the clock by timeScale x real time
    float timeScale = 600.0f;      // simulated seconds per real second while playing

    bool operator==(const TimeOfDayState&) const = default;
};

// What the state means at one instant.
struct TimeOfDayEvaluation {
    int year = 2026;
    int month = 4;
    int day = 15;
    double localHours = 12.0;       // preset resolved
    double julianDayUt = 0.0;
    solar::SunCoordinates sunCoordinates;
    solar::Horizontal sun;          // true azimuth; elevation geometric, apparentElevation refracted
    double lightElevation = 0.0;    // the elevation that drives lighting (refraction per the state)
    float fade = 0.0f;              // 0 below -1 degrees, 1 above +1, smoothstep between
    float gliderAzimuth = 0.0f;     // A3.1 convention after the heading
    float gliderElevation = 0.0f;
    bool night = false;             // lightElevation below the threshold
    solar::MoonState moon;
    float moonGliderAzimuth = 0.0f;
};

// Heading to A3.1: azimuth around +Y from +Z towards +X for a compass bearing.
inline double gliderAzimuthFromBearing(double bearing, double heading);
inline double bearingFromGliderAzimuth(double azimuth, double heading);

// Smoothstep from -1 to +1 degrees of lighting elevation.
inline float sunFade(double lightElevation);

inline double timeOfDayJulianDay(const TimeOfDayState& state, double localHours);
// Local hours of a preset for the state's date and place (Custom returns state.hours).
inline double presetLocalHours(const TimeOfDayState& state, TimePreset preset);
// state.hours, or the selected preset's hours.
inline double effectiveLocalHours(const TimeOfDayState& state);
// Turns a selected preset into plain Custom hours (before the clock is edited or played).
inline void materializePreset(TimeOfDayState& state);
// Rolls hours outside [0, 24) into the date.
inline void normalizeClock(TimeOfDayState& state);
inline void advanceClock(TimeOfDayState& state, double simulatedSeconds);

inline TimeOfDayEvaluation evaluateTimeOfDay(const TimeOfDayState& state);
// The sun events of the state's local date; windowStart receives the Julian day of local midnight.
inline solar::SunDay timeOfDaySunDay(const TimeOfDayState& state, double& windowStart);
// Local clock hours of a Julian day relative to windowStart (0..24 inside the window).
inline double hoursSince(double julianDay, double windowStart) { return (julianDay - windowStart) * 24.0; }

// "YYYY-MM-DD" and "HH:MM[:SS[.sss]]" (or decimal hours); on failure the error says what was expected.
inline bool parseDate(const std::string& text, int& year, int& month, int& day, std::string& error);
inline bool parseTime(const std::string& text, double& hours, std::string& error);
inline std::string formatDate(int year, int month, int day);
inline std::string formatTime(double hours); // "HH:MM:SS"

// ---- definitions (header-only: the project's CMake source list is fixed) ----


namespace detail {

constexpr double kPresetElevation = 3.0; // Dawn and Dusk: the sun this high above the horizon

inline double wrapDegrees(double degrees)
{
    return solar::normalizeDegrees(degrees);
}

struct PresetKey {
    int year = 0, month = 0, day = 0;
    float utcOffsetHours = 0.0f, latitude = 0.0f, longitude = 0.0f;
    bool operator==(const PresetKey&) const = default;
};

struct PresetTimes {
    double hours[static_cast<size_t>(TimePreset::Count)] = {};
};

inline PresetKey keyOf(const TimeOfDayState& state)
{
    return {state.year, state.month, state.day, state.utcOffsetHours, state.latitude, state.longitude};
}

// The preset hours of a date and place cost a few hundred sun evaluations; one slot is enough because the state
// changes rarely and every consumer of a frame asks for the same one.
inline const PresetTimes& presetTimes(const TimeOfDayState& state)
{
    static PresetKey cachedKey;
    static PresetTimes cached;
    static bool valid = false;
    const PresetKey key = keyOf(state);
    if (valid && cachedKey == key) return cached;

    double windowStart = 0.0;
    const solar::SunDay events = timeOfDaySunDay(state, windowStart);
    const double noon = events.hasTransit ? hoursSince(events.transit, windowStart) : 12.0;
    double crossing = 0.0;
    double dawn = noon - 5.0;
    if (solar::findElevationCrossing(windowStart, state.latitude, state.longitude, kPresetElevation, true, crossing)) {
        dawn = hoursSince(crossing, windowStart);
    }
    double dusk = noon + 5.0;
    if (solar::findElevationCrossing(windowStart, state.latitude, state.longitude, kPresetElevation, false, crossing)) {
        dusk = hoursSince(crossing, windowStart);
    }
    if (dawn >= noon) dawn = noon - 5.0;
    if (dusk <= noon) dusk = noon + 5.0;
    PresetTimes times;
    times.hours[static_cast<size_t>(TimePreset::Dawn)] = dawn;
    times.hours[static_cast<size_t>(TimePreset::Morning)] = 0.5 * (dawn + noon);
    times.hours[static_cast<size_t>(TimePreset::Noon)] = noon;
    times.hours[static_cast<size_t>(TimePreset::Afternoon)] = 0.5 * (noon + dusk);
    times.hours[static_cast<size_t>(TimePreset::Dusk)] = dusk;
    times.hours[static_cast<size_t>(TimePreset::Night)] = events.hasMidnight ? hoursSince(events.midnight, windowStart) : 0.0;
    cached = times;
    cachedKey = key;
    valid = true;
    return cached;
}

} // namespace detail

inline double gliderAzimuthFromBearing(double bearing, double heading)
{
    return detail::wrapDegrees(heading - bearing);
}

inline double bearingFromGliderAzimuth(double azimuth, double heading)
{
    return detail::wrapDegrees(heading - azimuth);
}

inline float sunFade(double lightElevation)
{
    const double x = std::clamp((lightElevation + 1.0) / 2.0, 0.0, 1.0);
    return static_cast<float>(x * x * (3.0 - 2.0 * x));
}

inline double timeOfDayJulianDay(const TimeOfDayState& state, double localHours)
{
    return solar::julianDay(state.year, state.month, state.day, localHours - static_cast<double>(state.utcOffsetHours));
}

inline double presetLocalHours(const TimeOfDayState& state, TimePreset preset)
{
    if (preset == TimePreset::Custom || static_cast<size_t>(preset) >= static_cast<size_t>(TimePreset::Count)) {
        return state.hours;
    }
    return detail::presetTimes(state).hours[static_cast<size_t>(preset)];
}

inline double effectiveLocalHours(const TimeOfDayState& state)
{
    const int preset = std::clamp(state.preset, 0, static_cast<int>(TimePreset::Count) - 1);
    return presetLocalHours(state, static_cast<TimePreset>(preset));
}

inline void normalizeClock(TimeOfDayState& state)
{
    const double shift = std::floor(state.hours / 24.0);
    if (shift != 0.0) {
        state.hours -= shift * 24.0;
        int y = 0, m = 0, d = 0;
        solar::civilFromDays(solar::daysFromCivil(state.year, state.month, state.day) + static_cast<long long>(shift), y, m, d);
        state.year = y;
        state.month = m;
        state.day = d;
    }
    if (state.hours < 0.0 || state.hours >= 24.0) state.hours = 0.0;
}

inline void materializePreset(TimeOfDayState& state)
{
    if (state.preset == static_cast<int>(TimePreset::Custom)) return;
    state.hours = effectiveLocalHours(state);
    state.preset = static_cast<int>(TimePreset::Custom);
    normalizeClock(state);
}

inline void advanceClock(TimeOfDayState& state, double simulatedSeconds)
{
    materializePreset(state);
    state.hours += simulatedSeconds / 3600.0;
    normalizeClock(state);
}

inline solar::SunDay timeOfDaySunDay(const TimeOfDayState& state, double& windowStart)
{
    windowStart = timeOfDayJulianDay(state, 0.0);
    return solar::sunDay(windowStart, state.latitude, state.longitude);
}

inline TimeOfDayEvaluation evaluateTimeOfDayUncached(const TimeOfDayState& state)
{
    TimeOfDayEvaluation out;
    TimeOfDayState clock = state;
    clock.hours = effectiveLocalHours(state);
    clock.preset = static_cast<int>(TimePreset::Custom);
    normalizeClock(clock);
    out.year = clock.year;
    out.month = clock.month;
    out.day = clock.day;
    out.localHours = clock.hours;
    out.julianDayUt = timeOfDayJulianDay(clock, clock.hours);
    out.sunCoordinates = solar::sunCoordinates(out.julianDayUt);
    out.sun = solar::horizontalFromEquatorial(out.sunCoordinates.rightAscension, out.sunCoordinates.declination,
                                              out.julianDayUt, state.latitude, state.longitude);
    out.lightElevation = state.refraction ? out.sun.apparentElevation : out.sun.elevation;
    out.fade = sunFade(out.lightElevation);
    out.gliderAzimuth = static_cast<float>(gliderAzimuthFromBearing(out.sun.azimuth, state.heading));
    out.gliderElevation = static_cast<float>(out.lightElevation);
    out.night = out.lightElevation < static_cast<double>(state.nightThreshold);
    out.moon = solar::moonState(out.julianDayUt, state.latitude, state.longitude);
    out.moonGliderAzimuth = static_cast<float>(gliderAzimuthFromBearing(out.moon.horizontal.azimuth, state.heading));
    return out;
}

// C7 (round 11): the sun, the moon, the exposure plan and the night blend each evaluate the same clock several times
// per frame (with a preset that is a sun-event search of ~0.1 ms per call). The evaluation is pure, so a cache keyed
// on the exact bits of every state field returns bit-identical results. Per thread, 4 entries, round-robin.
inline TimeOfDayEvaluation evaluateTimeOfDay(const TimeOfDayState& state)
{
    // A new TimeOfDayState field must join the key; the size check catches most additions.
    static_assert(sizeof(TimeOfDayState) == 64, "TimeOfDayState changed: extend evaluateTimeOfDay's cache key");
    using Key = std::array<uint64_t, 14>;
    const Key key = {state.enabled ? 1u : 0u,
                     static_cast<uint64_t>(static_cast<uint32_t>(state.year)),
                     static_cast<uint64_t>(static_cast<uint32_t>(state.month)),
                     static_cast<uint64_t>(static_cast<uint32_t>(state.day)),
                     std::bit_cast<uint64_t>(state.hours),
                     std::bit_cast<uint32_t>(state.utcOffsetHours),
                     std::bit_cast<uint32_t>(state.latitude),
                     std::bit_cast<uint32_t>(state.longitude),
                     std::bit_cast<uint32_t>(state.heading),
                     static_cast<uint64_t>(static_cast<uint32_t>(state.preset)),
                     state.refraction ? 1u : 0u,
                     std::bit_cast<uint32_t>(state.nightThreshold),
                     state.playing ? 1u : 0u,
                     std::bit_cast<uint32_t>(state.timeScale)};
    struct Entry {
        Key key{};
        TimeOfDayEvaluation value;
        bool valid = false;
    };
    thread_local std::array<Entry, 4> cache{};
    thread_local size_t next = 0;
    for (const Entry& entry : cache) {
        if (entry.valid && entry.key == key) return entry.value;
    }
    Entry& slot = cache[next];
    next = (next + 1) % cache.size();
    slot.value = evaluateTimeOfDayUncached(state);
    slot.key = key;
    slot.valid = true;
    return slot.value;
}

inline bool parseDate(const std::string& text, int& year, int& month, int& day, std::string& error)
{
    int y = 0, m = 0, d = 0, consumed = 0;
    if (std::sscanf(text.c_str(), "%d-%d-%d%n", &y, &m, &d, &consumed) != 3 || static_cast<size_t>(consumed) != text.size()) {
        error = "date expects YYYY-MM-DD";
        return false;
    }
    if (y < 1900 || y > 2100) {
        error = "date year must be in 1900..2100 (the solar equations are fitted to that range)";
        return false;
    }
    if (!solar::validDate(y, m, d)) {
        error = "date " + text + " is not a calendar date";
        return false;
    }
    year = y;
    month = m;
    day = d;
    return true;
}

inline bool parseTime(const std::string& text, double& hours, std::string& error)
{
    double value = 0.0;
    if (text.find(':') != std::string::npos) {
        int h = 0, m = 0, consumed = 0;
        double s = 0.0;
        int count = std::sscanf(text.c_str(), "%d:%d:%lf%n", &h, &m, &s, &consumed);
        if (count != 3) {
            s = 0.0;
            count = std::sscanf(text.c_str(), "%d:%d%n", &h, &m, &consumed);
            if (count != 2) {
                error = "time expects HH:MM[:SS] (or decimal hours)";
                return false;
            }
        }
        if (static_cast<size_t>(consumed) != text.size() || h < 0 || h > 23 || m < 0 || m > 59 || s < 0.0 || s >= 60.0) {
            error = "time expects HH:MM[:SS] with hours 0..23, minutes 0..59 and seconds 0..59";
            return false;
        }
        value = h + m / 60.0 + s / 3600.0;
    } else {
        char* end = nullptr;
        value = std::strtod(text.c_str(), &end);
        if (text.empty() || end == nullptr || *end != '\0' || !std::isfinite(value)) {
            error = "time expects HH:MM[:SS] (or decimal hours)";
            return false;
        }
        if (value < 0.0 || value >= 24.0) {
            error = "time must be in [0, 24) hours";
            return false;
        }
    }
    hours = value;
    return true;
}

inline std::string formatDate(int year, int month, int day)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
    return buffer;
}

inline std::string formatTime(double hours)
{
    long long seconds = std::llround(hours * 3600.0);
    seconds %= 86400;
    if (seconds < 0) seconds += 86400;
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld", seconds / 3600, (seconds / 60) % 60, seconds % 60);
    return buffer;
}


} // namespace ux3d::imgui
