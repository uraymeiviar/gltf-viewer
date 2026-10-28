// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>

// A6.1: solar and lunar position (docs/ASOBO_EXTENSIONS_PHASE_6.md). Pure functions of time and place, no
// application state, so they can be checked against published tables.
//
// Sun: the NOAA Global Monitoring Laboratory solar calculator equations (Meeus, "Astronomical Algorithms",
// ch. 25 with the low-precision corrections: geometric mean longitude and anomaly, equation of centre,
// apparent longitude with the nutation/aberration term, obliquity with its nutation term), then the hour angle
// from the apparent sidereal time of Meeus ch. 12. Against ERFA's (IAU 2006/2000A) apparent Sun the error is
// below 0.012 degrees over 2000-2060 (scripts/validate_time_of_day.py repeats the comparison).
// Refraction: the NOAA piecewise fit (about 1010 hPa, 10 C).
//
// Moon: Paul Schlyter's low-precision lunar elements ("How to compute planetary positions": orbital elements
// of the Moon plus the twelve largest longitude, five latitude and two distance perturbations), converted to
// equatorial coordinates, then reduced to the observer (diurnal parallax up to 1 degree). Against ERFA's moon98
// the geocentric error is below 0.09 degrees over 2000-2060; Meeus example 47.a is reproduced within 0.04
// degrees. Phase: elongation from the Sun's apparent longitude; illuminated fraction from the phase angle.
//
// Angle conventions: degrees; azimuth is measured from true north, clockwise (east = 90); latitude is positive
// north, longitude positive east. Times are Julian days in UT (the difference to TT, about 69 s in 2026, moves
// the Sun by 0.001 degrees and the Moon by 0.01 degrees and is ignored).

namespace ux3d::imgui::solar {

inline constexpr double kMeanSolarDaySeconds = 86400.0;
inline constexpr double kEarthRadiusKm = 6378.14;

// Proleptic Gregorian calendar helpers (Howard Hinnant's civil-days algorithms).
inline long long daysFromCivil(int year, int month, int day); // days since 1970-01-01
inline void civilFromDays(long long days, int& year, int& month, int& day);
inline int daysInMonth(int year, int month);
inline bool validDate(int year, int month, int day);

// Julian day (UT) of a calendar date at `hoursUt`, which may lie outside [0, 24) (it then rolls the date).
inline double julianDay(int year, int month, int day, double hoursUt);

inline double normalizeDegrees(double degrees);       // [0, 360)
inline double normalizeSignedDegrees(double degrees); // [-180, 180)

struct SunCoordinates {
    double rightAscension = 0.0;    // degrees, apparent, of date
    double declination = 0.0;       // degrees
    double eclipticLongitude = 0.0; // degrees, apparent
    double distanceAu = 1.0;
    double equationOfTimeMinutes = 0.0;
};
inline SunCoordinates sunCoordinates(double julianDayUt);

struct Horizontal {
    double azimuth = 0.0;           // from true north, clockwise
    double elevation = 0.0;         // geometric, degrees above the horizon
    double apparentElevation = 0.0; // elevation + refraction
    double hourAngle = 0.0;         // degrees west of the meridian, [-180, 180)
};

// Atmospheric refraction in degrees for a geometric elevation (NOAA fit; 0 above 85 degrees).
inline double refractionDegrees(double geometricElevation);

// Local sidereal angle minus right ascension, and the horizontal coordinates from equatorial ones.
inline Horizontal horizontalFromEquatorial(double rightAscension, double declination, double julianDayUt, double latitude,
                                    double longitude);
inline Horizontal sunHorizontal(double julianDayUt, double latitude, double longitude);

struct MoonState {
    double eclipticLongitude = 0.0; // degrees, of date
    double eclipticLatitude = 0.0;
    double distanceKm = 384400.0;   // geocentric
    double rightAscension = 0.0;    // degrees, geocentric
    double declination = 0.0;
    Horizontal horizontal;          // topocentric (diurnal parallax applied); elevation is geometric
    double elongation = 0.0;        // degrees, Moon-Sun angle at the geocentre
    double phaseAngle = 0.0;        // degrees, Sun-Moon-Earth angle
    double illuminatedFraction = 0.0; // 0 new .. 1 full
    double phase = 0.0;             // synodic cycle fraction: 0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter
    double ageDays = 0.0;           // phase x 29.530589
};
inline MoonState moonState(double julianDayUt, double latitude, double longitude);

// The Sun's events in the 24-hour window [windowStart, windowStart + 1) (Julian days UT). Sunrise and sunset are
// the crossings of geometric elevation -0.833 degrees (refraction and the Sun's radius); transit is the hour
// angle crossing 0, solar midnight the crossing of +-180. Crossings are bisected to a tenth of a second.
struct SunDay {
    bool hasSunrise = false;
    bool hasSunset = false;
    bool hasTransit = false;
    bool hasMidnight = false;
    double sunrise = 0.0;  // Julian day UT
    double sunset = 0.0;
    double transit = 0.0;
    double midnight = 0.0;
    double peakElevation = 0.0;   // highest and lowest geometric elevation sampled in the window
    double lowestElevation = 0.0;
    bool alwaysUp = false;        // polar day: no crossing and the Sun stays above -0.833
    bool alwaysDown = false;      // polar night
};
inline SunDay sunDay(double windowStart, double latitude, double longitude);

// First time in the window at which the Sun's geometric elevation crosses `elevation` rising or setting.
inline bool findElevationCrossing(double windowStart, double latitude, double longitude, double elevation, bool rising,
                           double& julianDayOut);

// ---- definitions (header-only: the project's CMake source list is fixed) ----


namespace detail {

constexpr double kPi = 3.14159265358979323846;
constexpr double kAstronomicalUnitKm = 149597870.7;
constexpr double kSynodicMonthDays = 29.530588861;
constexpr double kSunriseElevation = -0.833; // refraction 34' + semi-diameter 16'

inline double rad(double degrees) { return degrees * kPi / 180.0; }
inline double deg(double radians) { return radians * 180.0 / kPi; }
inline double sinDeg(double degrees) { return std::sin(rad(degrees)); }
inline double cosDeg(double degrees) { return std::cos(rad(degrees)); }

// Greenwich mean sidereal time in degrees (Meeus 12.4).
inline double greenwichSiderealDegrees(double jd)
{
    const double t = (jd - 2451545.0) / 36525.0;
    return normalizeDegrees(280.46061837 + 360.98564736629 * (jd - 2451545.0) + 0.000387933 * t * t - t * t * t / 38710000.0);
}

// Mean obliquity of the ecliptic in degrees (IAU 1976 linear term).
inline double meanObliquity(double t)
{
    return 23.439291 - 0.0130042 * t;
}

} // namespace detail

using namespace detail;

inline long long daysFromCivil(int year, int month, int day)
{
    year -= month <= 2 ? 1 : 0;
    const long long era = (year >= 0 ? year : year - 399) / 400;
    const long long yearOfEra = year - era * 400;
    const long long dayOfYear = (153LL * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const long long dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

inline void civilFromDays(long long days, int& year, int& month, int& day)
{
    days += 719468;
    const long long era = (days >= 0 ? days : days - 146096) / 146097;
    const long long dayOfEra = days - era * 146097;
    const long long yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const long long y = yearOfEra + era * 400;
    const long long dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const long long mp = (5 * dayOfYear + 2) / 153;
    day = static_cast<int>(dayOfYear - (153 * mp + 2) / 5 + 1);
    month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    year = static_cast<int>(y + (month <= 2 ? 1 : 0));
}

inline int daysInMonth(int year, int month)
{
    static const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return lengths[month - 1] + (month == 2 && leap ? 1 : 0);
}

inline bool validDate(int year, int month, int day)
{
    return month >= 1 && month <= 12 && day >= 1 && day <= daysInMonth(year, month);
}

inline double julianDay(int year, int month, int day, double hoursUt)
{
    return static_cast<double>(daysFromCivil(year, month, day)) + 2440587.5 + hoursUt / 24.0;
}

inline double normalizeDegrees(double degrees)
{
    double v = std::fmod(degrees, 360.0);
    if (v < 0.0) v += 360.0;
    return v;
}

inline double normalizeSignedDegrees(double degrees)
{
    double v = normalizeDegrees(degrees + 180.0) - 180.0;
    return v;
}

inline SunCoordinates sunCoordinates(double jd)
{
    const double t = (jd - 2451545.0) / 36525.0;
    const double meanLongitude = normalizeDegrees(280.46646 + t * (36000.76983 + t * 0.0003032));
    const double meanAnomaly = normalizeDegrees(357.52911 + t * (35999.05029 - 0.0001537 * t));
    const double eccentricity = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    const double centre = sinDeg(meanAnomaly) * (1.914602 - t * (0.004817 + 0.000014 * t)) +
                          sinDeg(2.0 * meanAnomaly) * (0.019993 - 0.000101 * t) + sinDeg(3.0 * meanAnomaly) * 0.000289;
    const double trueLongitude = meanLongitude + centre;
    const double trueAnomaly = meanAnomaly + centre;
    const double omega = 125.04 - 1934.136 * t;
    const double apparentLongitude = trueLongitude - 0.00569 - 0.00478 * sinDeg(omega);
    const double meanObliquityNoaa =
        23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0;
    const double obliquity = meanObliquityNoaa + 0.00256 * cosDeg(omega);

    SunCoordinates sun;
    sun.eclipticLongitude = normalizeDegrees(apparentLongitude);
    sun.rightAscension = normalizeDegrees(deg(std::atan2(cosDeg(obliquity) * sinDeg(apparentLongitude), cosDeg(apparentLongitude))));
    sun.declination = deg(std::asin(sinDeg(obliquity) * sinDeg(apparentLongitude)));
    sun.distanceAu = 1.000001018 * (1.0 - eccentricity * eccentricity) / (1.0 + eccentricity * cosDeg(trueAnomaly));
    const double y = std::tan(rad(obliquity) / 2.0) * std::tan(rad(obliquity) / 2.0);
    sun.equationOfTimeMinutes =
        4.0 * deg(y * sinDeg(2.0 * meanLongitude) - 2.0 * eccentricity * sinDeg(meanAnomaly) +
                  4.0 * eccentricity * y * sinDeg(meanAnomaly) * cosDeg(2.0 * meanLongitude) -
                  0.5 * y * y * sinDeg(4.0 * meanLongitude) - 1.25 * eccentricity * eccentricity * sinDeg(2.0 * meanAnomaly));
    return sun;
}

inline double refractionDegrees(double h)
{
    if (h > 85.0) return 0.0;
    double arcseconds;
    const double tanH = std::tan(rad(h));
    if (h > 5.0) {
        arcseconds = 58.1 / tanH - 0.07 / (tanH * tanH * tanH) + 0.000086 / (tanH * tanH * tanH * tanH * tanH);
    } else if (h > -0.575) {
        arcseconds = 1735.0 + h * (-518.2 + h * (103.4 + h * (-12.79 + h * 0.711)));
    } else {
        arcseconds = -20.774 / tanH;
    }
    return arcseconds / 3600.0;
}

inline Horizontal horizontalFromEquatorial(double rightAscension, double declination, double jd, double latitude, double longitude)
{
    Horizontal out;
    out.hourAngle = normalizeSignedDegrees(greenwichSiderealDegrees(jd) + longitude - rightAscension);
    const double sinElevation = sinDeg(latitude) * sinDeg(declination) +
                                cosDeg(latitude) * cosDeg(declination) * cosDeg(out.hourAngle);
    out.elevation = deg(std::asin(std::clamp(sinElevation, -1.0, 1.0)));
    out.azimuth = normalizeDegrees(deg(std::atan2(-sinDeg(out.hourAngle) * cosDeg(declination),
                                                  sinDeg(declination) * cosDeg(latitude) -
                                                      cosDeg(declination) * sinDeg(latitude) * cosDeg(out.hourAngle))));
    out.apparentElevation = out.elevation + refractionDegrees(out.elevation);
    return out;
}

inline Horizontal sunHorizontal(double jd, double latitude, double longitude)
{
    const SunCoordinates sun = sunCoordinates(jd);
    return horizontalFromEquatorial(sun.rightAscension, sun.declination, jd, latitude, longitude);
}

inline MoonState moonState(double jd, double latitude, double longitude)
{
    // Schlyter's elements; d counts days from 1999 December 31, 0h UT.
    const double d = jd - 2451543.5;
    const double node = normalizeDegrees(125.1228 - 0.0529538083 * d);
    const double inclination = 5.1454;
    const double perigeeArgument = normalizeDegrees(318.0634 + 0.1643573223 * d);
    const double semiMajorAxis = 60.2666; // Earth radii
    const double eccentricity = 0.054900;
    const double meanAnomaly = normalizeDegrees(115.3654 + 13.0649929509 * d);
    const double sunAnomaly = normalizeDegrees(356.0470 + 0.9856002585 * d);
    const double sunPerihelion = 282.9404 + 4.70935e-5 * d;
    const double sunMeanLongitude = normalizeDegrees(sunAnomaly + sunPerihelion);
    const double moonMeanLongitude = normalizeDegrees(meanAnomaly + perigeeArgument + node);
    const double D = normalizeDegrees(moonMeanLongitude - sunMeanLongitude);
    const double F = normalizeDegrees(moonMeanLongitude - node);
    const double M = meanAnomaly;
    const double Ms = sunAnomaly;

    double eccentricAnomaly = rad(M);
    for (int i = 0; i < 12; ++i) {
        eccentricAnomaly -= (eccentricAnomaly - eccentricity * std::sin(eccentricAnomaly) - rad(M)) /
                            (1.0 - eccentricity * std::cos(eccentricAnomaly));
    }
    const double xv = semiMajorAxis * (std::cos(eccentricAnomaly) - eccentricity);
    const double yv = semiMajorAxis * std::sqrt(1.0 - eccentricity * eccentricity) * std::sin(eccentricAnomaly);
    const double trueAnomaly = std::atan2(yv, xv);
    double distance = std::hypot(xv, yv);
    const double argument = rad(perigeeArgument) + trueAnomaly;
    const double xh = distance * (cosDeg(node) * std::cos(argument) - sinDeg(node) * std::sin(argument) * cosDeg(inclination));
    const double yh = distance * (sinDeg(node) * std::cos(argument) + cosDeg(node) * std::sin(argument) * cosDeg(inclination));
    const double zh = distance * std::sin(argument) * sinDeg(inclination);
    double longitudeEcl = deg(std::atan2(yh, xh));
    double latitudeEcl = deg(std::atan2(zh, std::hypot(xh, yh)));

    longitudeEcl += -1.274 * sinDeg(M - 2.0 * D) + 0.658 * sinDeg(2.0 * D) - 0.186 * sinDeg(Ms) -
                    0.059 * sinDeg(2.0 * M - 2.0 * D) - 0.057 * sinDeg(M - 2.0 * D + Ms) + 0.053 * sinDeg(M + 2.0 * D) +
                    0.046 * sinDeg(2.0 * D - Ms) + 0.041 * sinDeg(M - Ms) - 0.035 * sinDeg(D) - 0.031 * sinDeg(M + Ms) -
                    0.015 * sinDeg(2.0 * F - 2.0 * D) + 0.011 * sinDeg(M - 4.0 * D);
    latitudeEcl += -0.173 * sinDeg(F - 2.0 * D) - 0.055 * sinDeg(M - F - 2.0 * D) - 0.046 * sinDeg(M + F - 2.0 * D) +
                   0.033 * sinDeg(F + 2.0 * D) + 0.017 * sinDeg(2.0 * M + F);
    distance += -0.58 * cosDeg(M - 2.0 * D) - 0.46 * cosDeg(2.0 * D);

    MoonState moon;
    moon.eclipticLongitude = normalizeDegrees(longitudeEcl);
    moon.eclipticLatitude = latitudeEcl;
    moon.distanceKm = distance * kEarthRadiusKm;

    // Ecliptic of date to equatorial.
    const double t = (jd - 2451545.0) / 36525.0;
    const double obliquity = meanObliquity(t);
    const double x = cosDeg(latitudeEcl) * cosDeg(longitudeEcl);
    const double y = cosDeg(latitudeEcl) * sinDeg(longitudeEcl);
    const double z = sinDeg(latitudeEcl);
    const double ye = y * cosDeg(obliquity) - z * sinDeg(obliquity);
    const double ze = y * sinDeg(obliquity) + z * cosDeg(obliquity);
    moon.rightAscension = normalizeDegrees(deg(std::atan2(ye, x)));
    moon.declination = deg(std::asin(std::clamp(ze, -1.0, 1.0)));

    // Geocentric horizontal direction, then the observer's displacement (a unit Earth radius along the zenith).
    const Horizontal geocentric = horizontalFromEquatorial(moon.rightAscension, moon.declination, jd, latitude, longitude);
    const double ce = cosDeg(geocentric.elevation);
    const double north = distance * ce * cosDeg(geocentric.azimuth);
    const double east = distance * ce * sinDeg(geocentric.azimuth);
    const double up = distance * sinDeg(geocentric.elevation) - 1.0;
    moon.horizontal = geocentric;
    moon.horizontal.azimuth = normalizeDegrees(deg(std::atan2(east, north)));
    moon.horizontal.elevation = deg(std::atan2(up, std::hypot(north, east)));
    moon.horizontal.apparentElevation = moon.horizontal.elevation + refractionDegrees(moon.horizontal.elevation);

    // Phase from the angle to the Sun (ecliptic latitude 0).
    const SunCoordinates sun = sunCoordinates(jd);
    const double cosElongation = std::clamp(cosDeg(latitudeEcl) * cosDeg(longitudeEcl - sun.eclipticLongitude), -1.0, 1.0);
    moon.elongation = deg(std::acos(cosElongation));
    const double sunDistanceKm = sun.distanceAu * kAstronomicalUnitKm;
    moon.phaseAngle = deg(std::atan2(sunDistanceKm * std::sin(rad(moon.elongation)),
                                     moon.distanceKm - sunDistanceKm * cosElongation));
    moon.illuminatedFraction = 0.5 * (1.0 + cosDeg(moon.phaseAngle));
    moon.phase = normalizeDegrees(longitudeEcl - sun.eclipticLongitude) / 360.0;
    moon.ageDays = moon.phase * kSynodicMonthDays;
    return moon;
}

namespace detail {

constexpr double kStepDays = 10.0 / 1440.0; // 10-minute scan
constexpr int kStepCount = 144;

inline double geometricElevation(double jd, double latitude, double longitude)
{
    return sunHorizontal(jd, latitude, longitude).elevation;
}

// The hour angle shifted so that the crossing of interest sits at 0.
inline double shiftedHourAngle(double jd, double latitude, double longitude, double shift)
{
    (void)latitude;
    const SunCoordinates sun = sunCoordinates(jd);
    return normalizeSignedDegrees(greenwichSiderealDegrees(jd) + longitude - sun.rightAscension + shift);
}

template <typename Function>
inline double bisect(double lo, double hi, double valueLo, Function function)
{
    // `function` is zero at the answer; valueLo has the sign of function(lo).
    for (int i = 0; i < 40; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double value = function(mid);
        if ((value < 0.0) == (valueLo < 0.0)) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

} // namespace detail

inline bool findElevationCrossing(double windowStart, double latitude, double longitude, double elevation, bool rising,
                           double& julianDayOut)
{
    double previous = geometricElevation(windowStart, latitude, longitude) - elevation;
    for (int i = 1; i <= kStepCount; ++i) {
        const double jd = windowStart + i * kStepDays;
        const double current = geometricElevation(jd, latitude, longitude) - elevation;
        const bool crossesUp = previous < 0.0 && current >= 0.0;
        const bool crossesDown = previous >= 0.0 && current < 0.0;
        if ((rising && crossesUp) || (!rising && crossesDown)) {
            julianDayOut = bisect(jd - kStepDays, jd, previous, [&](double t) {
                return geometricElevation(t, latitude, longitude) - elevation;
            });
            return true;
        }
        previous = current;
    }
    return false;
}

inline SunDay sunDay(double windowStart, double latitude, double longitude)
{
    SunDay day;
    day.hasSunrise = findElevationCrossing(windowStart, latitude, longitude, kSunriseElevation, true, day.sunrise);
    day.hasSunset = findElevationCrossing(windowStart, latitude, longitude, kSunriseElevation, false, day.sunset);

    double peak = -90.0, lowest = 90.0;
    for (int i = 0; i <= kStepCount; ++i) {
        const double e = geometricElevation(windowStart + i * kStepDays, latitude, longitude);
        peak = std::max(peak, e);
        lowest = std::min(lowest, e);
    }
    day.peakElevation = peak;
    day.lowestElevation = lowest;
    day.alwaysUp = !day.hasSunrise && !day.hasSunset && lowest > kSunriseElevation;
    day.alwaysDown = !day.hasSunrise && !day.hasSunset && peak <= kSunriseElevation;

    // Transit: the hour angle rises through 0 (|H| < 90 excludes the wrap at 180). Midnight: the same for H + 180.
    for (int pass = 0; pass < 2; ++pass) {
        const double shift = pass == 0 ? 0.0 : 180.0;
        double previous = shiftedHourAngle(windowStart, latitude, longitude, shift);
        for (int i = 1; i <= kStepCount; ++i) {
            const double jd = windowStart + i * kStepDays;
            const double current = shiftedHourAngle(jd, latitude, longitude, shift);
            if (previous < 0.0 && current >= 0.0 && previous > -90.0 && current < 90.0) {
                const double found = bisect(jd - kStepDays, jd, previous, [&](double t) {
                    return shiftedHourAngle(t, latitude, longitude, shift);
                });
                if (pass == 0) {
                    day.hasTransit = true;
                    day.transit = found;
                } else {
                    day.hasMidnight = true;
                    day.midnight = found;
                }
                break;
            }
            previous = current;
        }
    }
    return day;
}


} // namespace ux3d::imgui::solar
