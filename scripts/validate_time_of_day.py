# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.1: solar position and time of day (docs/ASOBO_EXTENSIONS_PHASE_6.md).

The scene-level clock (date, local time, UTC offset, latitude, longitude, heading) gives the Sun's position (NOAA
solar calculator equations) and the Moon's (Schlyter low-precision elements); with time_of_day_mode on the A3.1 sun
follows it. Conventions: header/imgui/TimeOfDay.h. A compass bearing B maps to the A3.1 azimuth (around +Y from +Z
towards +X) as  azimuth = heading - B  (MSFS aircraft: nose on glTF +Z, left side on +X).

Reference data (frozen below, no dependency at run time):
  * SOLAR_TABLE row 0 is the NREL SPA test case published in Reda & Andreas, "Solar Position Algorithm for Solar
    Radiation Applications" (NREL/TP-560-34302, 2003/2008), 2003-10-17 12:30:30 local, UTC-7, 39.742476 N,
    105.1786 W: topocentric azimuth 194.340241, elevation 39.872046 (zenith 50.111622 with refraction at 820 hPa,
    11 C: elevation 39.888378). The SPA elevation includes the Sun's parallax (0.002 degrees), ours is geocentric.
  * The other rows (high latitude, southern hemisphere, equator, polar day and night, years 1999 and 2050) are
    NOT from a published table: they were generated once with pyerfa 2.0.1.5 (IAU 2006/2000A apparent place from
    epv00 + atco13, no refraction), an implementation independent of the NOAA equations used here. The generator
    is --regenerate-references (needs pyerfa; it re-derives every row except the published SPA one).
  * SUN_EVENTS: sunrise and sunset (geometric elevation -0.8333 degrees) from the same pyerfa positions.
  * Moon: Meeus, "Astronomical Algorithms" 2nd ed., example 47.a (1992-04-12 0h TD: lambda 133.162655, beta
    -3.229126, distance 368409.7 km, RA 134.688470, Dec +13.768368) and example 48.a (illuminated fraction 0.6786);
    the principal phases of 2024 as published by USNO/NASA (new moon 2024-04-08 18:21 UTC, first quarter
    2024-04-15 19:13, full moon 2024-04-23 23:49 and 2024-01-25 17:54; the Moon-Sun ecliptic longitude difference
    is 0, 90, 180, 180 degrees there); topocentric azimuth/elevation from pyerfa moon98 (same caveat as above).

Checks:
  * mode off (the default) and clock settings with the mode off: byte-identical to the baseline binary
    (--baseline-exe): HelloCube, two E195 reference views (ASOBO), and the E195 exterior with the manual sun on;
  * Sun azimuth/elevation against SOLAR_TABLE within SUN_TOLERANCE_DEG (great-circle); sunrise/sunset within
    EVENT_TOLERANCE_S; presets (dawn/dusk at +3 degrees, noon at transit, night at solar midnight, morning and
    afternoon halfway, following date and place, polar days);
  * Moon against Meeus/pyerfa within MOON_TOLERANCE_DEG, phase cycle and illuminated fraction within PHASE_TOLERANCE;
  * heading: +90 degrees of heading turns the sun's A3.1 azimuth by +90 (state and the lit Lambert sphere over the
    control pipe); the E195 file confirms the nose is on +Z and the left side on +X;
  * below-horizon fade: the lit pixel luminance rises monotonically across -1..+1 degrees, 0 below, full above;
  * Automatic day/night flips at the threshold (state) and renders like forced Day / Night (ASOBO emissive quad);
  * registry/CLI: keys listed, invalid values rejected, CLI flags equal the pipe, play advances and rolls the date;
  * zero GL errors everywhere.

Run: python scripts/validate_time_of_day.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
import validate_asobo_emissive as emissive  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same, views, render_view  # noqa: E402
from validate_asobo_sss import write_hdr  # noqa: E402
from validate_sun import (COMMON, VIEW, INTENSITY, Session, gl_errors_zero, lambert, sun_vector,  # noqa: E402
                          angle_between)

OUT = REPO / "build" / "timeofday"
SUN_TOLERANCE_DEG = 0.05
EVENT_TOLERANCE_S = 60.0
MOON_TOLERANCE_DEG = 1.0
PHASE_TOLERANCE = 0.05
PROBE_REL, PROBE_ABS = 0.02, 2e-3
E195_DIR = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model")

# (label, local date, local time, UTC offset hours, latitude, longitude, true azimuth, geometric elevation)
SOLAR_TABLE = [
    ("NREL SPA test case (published), Golden CO", "2003-10-17", "12:30:30", -7.0, 39.742476, -105.1786, 194.340241, 39.872046),
    ("Sydney, summer afternoon", "2026-01-15", "14:00:00", 11.0, -33.8688, 151.2093, 312.4907, 72.3177),
    ("Ushuaia, southern winter", "2026-07-10", "10:00:00", -3.0, -54.8019, -68.303, 49.0505, 0.0092),
    ("Quito, equator near sunrise", "2026-03-20", "06:30:00", -5.0, -0.1807, -78.4678, 90.0469, 2.1718),
    ("Tromso, midnight sun", "2026-06-21", "23:30:00", 2.0, 69.6492, 18.9553, 342.5555, 4.0863),
    ("Tromso, polar night noon", "2026-12-21", "12:00:00", 1.0, 69.6492, 18.9553, 184.0831, -3.1410),
    ("Longyearbyen, spring equinox morning", "2026-03-20", "08:15:00", 1.0, 78.2232, 15.6267, 121.9747, 6.1746),
    ("McMurdo, Antarctic midnight sun", "2026-12-21", "00:30:00", 13.0, -77.8419, 166.6863, 198.9002, 11.9673),
    ("Reykjavik, evening", "2026-09-05", "19:45:00", 0.0, 64.1466, -21.9426, 277.0574, 3.8924),
    ("Honolulu, near the zenith", "2026-05-25", "12:30:00", -10.0, 21.3069, -157.8583, 238.4593, 89.5672),
    ("Seattle (the default place) in 2050", "2050-04-15", "09:30:00", -7.0, 47.45, -122.31, 110.4976, 30.7917),
    ("Cape Town, 1999-12-31", "1999-12-31", "23:50:00", 2.0, -33.9249, 18.4241, 195.9890, -31.2773),
    ("Tokyo, autumn dusk", "2026-10-28", "16:40:00", 9.0, 35.6762, 139.6503, 252.7698, 1.2760),
]
# (label, date, UTC offset, latitude, longitude, sunrise, sunset) local clock "HH:MM:SS"
SUN_EVENTS = [
    ("Golden CO, 2003-10-17", "2003-10-17", -7.0, 39.742476, -105.1786, "06:12:43", "17:18:52"),
    ("Sydney, 2026-01-15", "2026-01-15", 11.0, -33.8688, 151.2093, "05:59:29", "20:08:59"),
    ("Reykjavik, 2026-09-05", "2026-09-05", 0.0, 64.1466, -21.9426, "06:21:43", "20:29:16"),
]
# Moon topocentric (pyerfa moon98): date, UTC time, latitude, longitude, azimuth, elevation
MOON_TOPOCENTRIC = [
    ("2026-04-15", "20:00:00", 47.45, -122.31, 206.8641, 42.1629),
    ("2026-01-15", "03:00:00", -33.8688, 151.2093, 260.2448, 38.5967),
    ("2026-10-28", "12:00:00", 35.6762, 139.6503, 79.0665, 31.6842),
    ("2026-10-28", "21:00:00", 35.6762, 139.6503, 279.0290, 36.5735),
]

BLACK = None


def parse_clock(text):
    parts = [float(p) for p in text.split(":")]
    return parts[0] + parts[1] / 60.0 + (parts[2] / 3600.0 if len(parts) > 2 else 0.0)


def separation(az1, el1, az2, el2):
    """Great-circle angle in degrees between two (azimuth, elevation) directions."""
    a1, e1, a2, e2 = map(math.radians, (az1, el1, az2, el2))
    c = math.sin(e1) * math.sin(e2) + math.cos(e1) * math.cos(e2) * math.cos(a1 - a2)
    return math.degrees(math.acos(max(-1.0, min(1.0, c))))


def wrap_signed(degrees):
    return (degrees + 180.0) % 360.0 - 180.0


def setup(session, date, time, offset, latitude, longitude, heading=None):
    for key, value in (("date", date), ("utc_offset", offset), ("latitude", latitude), ("longitude", longitude),
                       ("time", time)):
        session.call("set_environment", key=key, value=value)
    if heading is not None:
        session.call("set_environment", key="heading", value=heading)


def tod(session):
    return session.call("get_state")["timeOfDay"]


def lambert_model():
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "lambert.gltf"
    path.write_text(json.dumps(lambert()), encoding="utf-8")
    return path


def regenerate_references():
    """Prints SOLAR_TABLE, SUN_EVENTS and MOON_TOPOCENTRIC values from pyerfa (IAU 2006/2000A apparent places);
    the run-time checks never import it. Sun: -pvh of epv00 through atco13 without refraction. Moon: moon98 minus
    the observer's GCRS position through atco13 (topocentric). Row 0 of SOLAR_TABLE keeps the published SPA values."""
    import warnings
    from datetime import date as Date, timedelta
    import erfa
    import numpy as np
    warnings.filterwarnings("ignore")

    def utc_parts(day, local, offset):
        hours = parse_clock(local) - offset
        moment = Date.fromisoformat(day)
        while hours < 0.0:
            hours += 24.0
            moment -= timedelta(days=1)
        while hours >= 24.0:
            hours -= 24.0
            moment += timedelta(days=1)
        h = int(hours)
        m = int((hours - h) * 60)
        return moment.year, moment.month, moment.day, h, m, ((hours - h) * 60 - m) * 60

    def times(y, mo, d, h, mi, sec):
        u1, u2 = erfa.dtf2d("UTC", y, mo, d, h, mi, sec)
        t1, t2 = erfa.utctai(u1, u2)
        return u1, u2, *erfa.taitt(t1, t2)

    def observe(rc, dc, u1, u2, lat, lon):
        aob, zob, *_ = erfa.atco13(rc, dc, 0, 0, 0, 0, u1, u2, 0.0, math.radians(lon), math.radians(lat), 0.0, 0, 0, 0, 0, 0, 0.55)
        return math.degrees(aob), 90.0 - math.degrees(zob)

    def sun_horizontal(y, mo, d, h, mi, sec, lat, lon):
        u1, u2, t1, t2 = times(y, mo, d, h, mi, sec)
        v = -erfa.epv00(t1, t2)[0][0]
        return observe(math.atan2(v[1], v[0]), math.atan2(v[2], math.hypot(v[0], v[1])), u1, u2, lat, lon)

    def moon_horizontal(y, mo, d, h, mi, sec, lat, lon):
        u1, u2, t1, t2 = times(y, mo, d, h, mi, sec)
        observer = np.array(erfa.gd2gc(1, math.radians(lon), math.radians(lat), 0.0)) / 149597870700.0
        v = np.array(erfa.moon98(t1, t2)[0]) - erfa.c2t06a(t1, t2, u1, u2, 0.0, 0.0).T @ observer
        return observe(math.atan2(v[1], v[0]), math.atan2(v[2], math.hypot(v[0], v[1])), u1, u2, lat, lon)

    def crossing(day, offset, lat, lon, rising):
        previous = None
        for k in range(24 * 60 + 1):
            value = sun_horizontal(*utc_parts(day, f"{k // 60}:{k % 60:02d}:00", offset), lat, lon)[1] + 0.8333
            if previous is not None and ((rising and previous < 0 <= value) or (not rising and previous >= 0 > value)):
                lo, hi = (k - 1) / 60.0, k / 60.0
                for _ in range(30):
                    mid = 0.5 * (lo + hi)
                    seconds = int(round(mid * 3600))
                    v = sun_horizontal(*utc_parts(day, f"{seconds // 3600}:{(seconds // 60) % 60:02d}:{seconds % 60:02d}", offset), lat, lon)[1] + 0.8333
                    lo, hi = (mid, hi) if (v < 0) == (previous < 0) else (lo, mid)
                seconds = int(round(0.5 * (lo + hi) * 3600))
                return f"{seconds // 3600:02d}:{(seconds // 60) % 60:02d}:{seconds % 60:02d}"
            previous = value
        return None

    print("SOLAR_TABLE rows 1..:")
    for label, day, local, offset, lat, lon, _, _ in SOLAR_TABLE[1:]:
        az, el = sun_horizontal(*utc_parts(day, local, offset), lat, lon)
        print(f'    ({label!r}, "{day}", "{local}", {offset}, {lat}, {lon}, {az:.4f}, {el:.4f}),')
    print("SUN_EVENTS:")
    for label, day, offset, lat, lon, _, _ in SUN_EVENTS:
        print(f'    ({label!r}, "{day}", {offset}, {lat}, {lon}, "{crossing(day, offset, lat, lon, True)}", "{crossing(day, offset, lat, lon, False)}"),')
    print("MOON_TOPOCENTRIC:")
    for day, local, lat, lon, _, _ in MOON_TOPOCENTRIC:
        az, el = moon_horizontal(*utc_parts(day, local, 0.0), lat, lon)
        print(f'    ("{day}", "{local}", {lat}, {lon}, {az:.4f}, {el:.4f}),')


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_solar_table(session):
    print("[Sun azimuth/elevation against the reference table]")
    worst = 0.0
    for label, date, time, offset, latitude, longitude, azimuth, elevation in SOLAR_TABLE:
        setup(session, date, time, offset, latitude, longitude)
        sun = tod(session)["sun"]
        error = separation(sun["azimuth"], sun["elevation"], azimuth, elevation)
        worst = max(worst, error)
        check(error <= SUN_TOLERANCE_DEG,
              f"{label}: {date} {time} -> az {sun['azimuth']:.4f} el {sun['elevation']:.4f} (reference "
              f"{azimuth:.4f} / {elevation:.4f}), {error:.4f} deg off (<= {SUN_TOLERANCE_DEG})")
    print(f"  INFO worst error {worst:.4f} deg over {len(SOLAR_TABLE)} cases")
    # The SPA row also gives the refracted elevation at 820 hPa / 11 C; ours is the standard atmosphere
    # (about 1010 hPa), so only the order of magnitude of the refraction is comparable.
    setup(session, *SOLAR_TABLE[0][1:6])
    state = tod(session)
    sun = state["sun"]
    refraction = sun["apparentElevation"] - sun["elevation"]
    check(abs(sun["apparentElevation"] - 39.888378) <= 0.01,
          f"SPA case: apparent elevation {sun['apparentElevation']:.4f} against the published 39.888378 (820 hPa, 11 C; "
          f"refraction {refraction:.4f} deg here, standard atmosphere)")
    check(state["utc"] == {"date": "2003-10-17", "time": "19:30:30"}, f"UTC = local - offset: {state['utc']}")


def check_sun_events(session):
    print("[Sunrise and sunset]")
    for label, date, offset, latitude, longitude, sunrise, sunset in SUN_EVENTS:
        setup(session, date, "12:00:00", offset, latitude, longitude)
        events = tod(session)["events"]
        for name, expected in (("sunrise", sunrise), ("sunset", sunset)):
            found = events[name]
            seconds = abs(found["hours"] - parse_clock(expected)) * 3600.0 if found else 1e9
            check(seconds <= EVENT_TOLERANCE_S,
                  f"{label}: {name} {found['time'] if found else None} (reference {expected}), {seconds:.1f} s off "
                  f"(<= {EVENT_TOLERANCE_S:.0f})")
    # Polar day and night, no crossing.
    setup(session, "2026-06-21", "12:00:00", 2.0, 69.6492, 18.9553)
    events = tod(session)["events"]
    check(events["polarDay"] and events["sunrise"] is None and events["sunset"] is None,
          "Tromso 2026-06-21: polar day, no sunrise or sunset")
    setup(session, "2026-12-21", "12:00:00", 1.0, 69.6492, 18.9553)
    events = tod(session)["events"]
    check(events["polarNight"] and events["sunrise"] is None, "Tromso 2026-12-21: polar night, no sunrise")


def check_presets(session):
    print("[Presets]")
    setup(session, "2026-04-15", "12:00:00", -7.0, 47.45, -122.31)
    times = {}
    for name in ("dawn", "morning", "noon", "afternoon", "dusk", "night"):
        session.call("set_environment", key="time_preset", value=name)
        state = tod(session)
        times[name] = state
    check(all(times[n]["preset"] == n for n in times), "the preset names read back")
    hours = {n: times[n]["hours"] for n in times}
    check(hours["dawn"] < hours["morning"] < hours["noon"] < hours["afternoon"] < hours["dusk"],
          "dawn < morning < noon < afternoon < dusk: " + ", ".join(f"{n} {times[n]['time']}" for n in times))
    check(abs(times["dawn"]["sun"]["elevation"] - 3.0) <= 0.01 and abs(times["dusk"]["sun"]["elevation"] - 3.0) <= 0.01,
          f"dawn/dusk at +3 degrees: {times['dawn']['sun']['elevation']:.3f}, {times['dusk']['sun']['elevation']:.3f}")
    check(abs(times["noon"]["sun"]["hourAngle"]) <= 0.01 and times["noon"]["sun"]["elevation"] > 50.0,
          f"noon at the transit: hour angle {times['noon']['sun']['hourAngle']:.4f} deg, elevation "
          f"{times['noon']['sun']['elevation']:.2f}")
    check(abs(hours["morning"] - 0.5 * (hours["dawn"] + hours["noon"])) < 1e-3 and
          abs(hours["afternoon"] - 0.5 * (hours["noon"] + hours["dusk"])) < 1e-3, "morning/afternoon halfway")
    lowest = times["night"]["events"]["lowestElevation"]
    check(abs(times["night"]["sun"]["elevation"] - lowest) <= 0.2 and times["night"]["sun"]["elevation"] < -20.0,
          f"night at solar midnight: elevation {times['night']['sun']['elevation']:.2f} (lowest of the day {lowest:.2f})")
    # A preset follows the date and the place; a time edit drops it; custom keeps the time.
    session.call("set_environment", key="time_preset", value="noon")
    before = tod(session)["hours"]
    session.call("set_environment", key="date", value="2026-11-03")
    after = tod(session)
    check(after["preset"] == "noon" and abs(after["sun"]["hourAngle"]) <= 0.01 and abs(after["hours"] - before) > 0.01,
          f"noon follows the date: {before:.3f} h in April, {after['hours']:.3f} h in November")
    session.call("set_environment", key="time_preset", value="custom")
    kept = tod(session)
    check(kept["preset"] == "custom" and abs(kept["hours"] - after["hours"]) < 1e-6, "custom keeps the preset's time")
    session.call("set_environment", key="time_preset", value="dusk")
    session.call("set_environment", key="time", value="09:15:30")
    edited = tod(session)
    check(edited["preset"] == "custom" and edited["time"] == "09:15:30", f"a time edit selects custom ({edited['time']})")
    # Polar: presets stay finite and in range.
    setup(session, "2026-06-21", "12:00:00", 2.0, 69.6492, 18.9553)
    ok = True
    for name in ("dawn", "morning", "noon", "afternoon", "dusk", "night"):
        session.call("set_environment", key="time_preset", value=name)
        state = tod(session)
        ok = ok and math.isfinite(state["sun"]["elevation"]) and 0.0 <= state["hours"] < 24.0
    check(ok, "presets under the midnight sun stay finite and inside the day")
    session.call("set_environment", key="time_preset", value="custom")


def check_moon(session):
    print("[Moon]")
    # Meeus 47.a: 1992-04-12 0h TD = 23:59:01 UT on 04-11 (Delta T 59 s).
    setup(session, "1992-04-11", "23:59:01", 0.0, 0.0, 0.0)
    state = tod(session)
    moon, sun = state["moon"], state["sun"]
    sep = separation(moon["rightAscension"], moon["declination"], 134.688470, 13.768368)
    check(sep <= MOON_TOLERANCE_DEG, f"Meeus 47.a: RA {moon['rightAscension']:.4f} Dec {moon['declination']:.4f} "
                                     f"(published 134.6885 / 13.7684), {sep:.3f} deg off (<= {MOON_TOLERANCE_DEG})")
    check(abs(wrap_signed(moon["eclipticLongitude"] - 133.162655)) <= MOON_TOLERANCE_DEG and
          abs(moon["eclipticLatitude"] + 3.229126) <= MOON_TOLERANCE_DEG,
          f"Meeus 47.a: ecliptic {moon['eclipticLongitude']:.4f} / {moon['eclipticLatitude']:.4f} (published 133.1627 / -3.2291)")
    check(abs(moon["distanceKm"] / 368409.7 - 1.0) <= 0.01, f"Meeus 47.a: distance {moon['distanceKm']:.0f} km (published 368409.7)")
    check(abs(moon["illuminatedFraction"] - 0.6786) <= PHASE_TOLERANCE,
          f"Meeus 48.a: illuminated fraction {moon['illuminatedFraction']:.4f} (published 0.6786)")
    phases = [("new moon 2024-04-08 18:21 UTC", "2024-04-08", "18:21:00", 0.0, 0.0, 0.0),
              ("first quarter 2024-04-15 19:13 UTC", "2024-04-15", "19:13:00", 90.0, 0.25, 0.5),
              ("full moon 2024-04-23 23:49 UTC", "2024-04-23", "23:49:00", 180.0, 0.5, 1.0),
              ("full moon 2024-01-25 17:54 UTC", "2024-01-25", "17:54:00", 180.0, 0.5, 1.0)]
    for label, date, time, difference, cycle, lit in phases:
        setup(session, date, time, 0.0, 0.0, 0.0)
        state = tod(session)
        moon, sun = state["moon"], state["sun"]
        diff = wrap_signed(moon["eclipticLongitude"] - sun["eclipticLongitude"] - difference)
        phase_error = abs(wrap_signed((moon["phase"] - cycle) * 360.0)) / 360.0
        check(abs(diff) <= MOON_TOLERANCE_DEG and phase_error <= PHASE_TOLERANCE and abs(moon["illuminatedFraction"] - lit) <= PHASE_TOLERANCE,
              f"{label}: longitude difference to the Sun {wrap_signed(moon['eclipticLongitude'] - sun['eclipticLongitude']):.3f} deg "
              f"(published {difference:.0f}), phase {moon['phase']:.4f}, lit {moon['illuminatedFraction']:.3f}")
    for date, time, latitude, longitude, azimuth, elevation in MOON_TOPOCENTRIC:
        setup(session, date, time, 0.0, latitude, longitude)
        moon = tod(session)["moon"]
        sep = separation(moon["azimuth"], moon["elevation"], azimuth, elevation)
        check(sep <= MOON_TOLERANCE_DEG, f"topocentric {date} {time} UTC at {latitude:.2f},{longitude:.2f}: az "
                                         f"{moon['azimuth']:.2f} el {moon['elevation']:.2f} (pyerfa {azimuth:.2f} / {elevation:.2f}), "
                                         f"{sep:.3f} deg off (<= {MOON_TOLERANCE_DEG})")
    check(state["moon"].get("rendered") is False, "the Moon is exposed in the state but not rendered (A6.6)")


def e195_orientation():
    """World positions of E195 nodes: the nose points to +Z, the left side to +X."""
    path = E195_DIR / "E195-Exterior-LOD00.gltf"
    if not path.exists():
        return None
    doc = json.loads(path.read_text(encoding="utf-8"))
    nodes = doc["nodes"]
    parent = {c: i for i, n in enumerate(nodes) for c in n.get("children", [])}

    def local(node):
        if "matrix" in node:
            m = node["matrix"]
            return [[m[r + 4 * c] for c in range(4)] for r in range(4)]
        t = node.get("translation", [0, 0, 0])
        x, y, z, w = node.get("rotation", [0, 0, 0, 1])
        s = node.get("scale", [1, 1, 1])
        r = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
             [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
             [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
        return [[r[i][j] * s[j] for j in range(3)] + [t[i]] for i in range(3)] + [[0, 0, 0, 1]]

    def mul(a, b):
        return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]

    def position(index):
        m = local(nodes[index])
        while index in parent:
            index = parent[index]
            m = mul(local(nodes[index]), m)
        return [m[0][3], m[1][3], m[2][3]]

    by_name = {n.get("name"): i for i, n in enumerate(nodes)}
    wanted = ("BONE_LGEAR_F", "BONE_RUDDER", "E195_ELEV_L", "E195_ELEV_R")
    if any(name not in by_name for name in wanted):
        return None
    return {name: position(by_name[name]) for name in wanted}


def check_heading(session):
    print("[Heading]")
    positions = e195_orientation()
    if positions is None:
        print("  SKIP E195 exterior not available")
    else:
        nose, tail = positions["BONE_LGEAR_F"], positions["BONE_RUDDER"]
        left, right = positions["E195_ELEV_L"], positions["E195_ELEV_R"]
        check(nose[2] > tail[2] + 20.0, f"E195: nose gear at z = {nose[2]:.2f}, rudder at z = {tail[2]:.2f}: the nose points to +Z")
        check(left[0] > 0.0 > right[0], f"E195: left elevator at x = {left[0]:.2f}, right at x = {right[0]:.2f}: the left side is +X")
    setup(session, "2026-04-15", "13:09:12", -7.0, 47.45, -122.31)
    session.call("set_environment", key="time_of_day_mode", value=True)
    previous = None
    for heading in (0.0, 90.0, 180.0, 270.0, 37.5, 359.0):
        session.call("set_environment", key="heading", value=heading)
        state = session.call("get_state")
        sun, effective = state["timeOfDay"]["sun"], state["sun"]
        expected = (heading - sun["azimuth"]) % 360.0
        check(abs(wrap_signed(sun["gliderAzimuth"] - expected)) <= 1e-3 and abs(wrap_signed(effective["azimuth"] - expected)) <= 1e-3,
              f"heading {heading:.1f}: bearing {sun['azimuth']:.2f} -> A3.1 azimuth {sun['gliderAzimuth']:.3f} "
              f"(heading - bearing = {expected:.3f}), lit sun {effective['azimuth']:.3f}")
        if heading == 90.0 and previous is not None:
            turned = wrap_signed(effective["azimuth"] - previous)
            check(abs(turned - 90.0) <= 1e-3, f"heading +90 turns the A3.1 sun azimuth by {turned:.4f} degrees")
        if heading == 0.0:
            previous = effective["azimuth"]
    # A bearing straight ahead is +Z, to the right is -X (the aircraft's left is +X).
    session.call("set_environment", key="heading", value=0.0)
    sun = tod(session)["sun"]
    session.call("set_environment", key="heading", value=(sun["azimuth"] + 90.0) % 360.0)
    ahead = session.call("get_state")["sun"]["direction"]
    check(abs(ahead[0] - math.cos(math.radians(sun["lightElevation"]))) <= 1e-3,
          f"nose 90 degrees to the right of the sun: the sun lies on the aircraft's left, +X: x = {ahead[0]:.3f} "
          f"(cos el = {math.cos(math.radians(sun['lightElevation'])):.3f})")


def check_lit_sphere(model, flags):
    print("[The sun follows the clock: Lambert sphere over the control pipe]")
    session = Session(model, flags)
    try:
        session.call("set_environment", key="sun_intensity", value=INTENSITY)
        setup(session, "2026-04-15", "13:09:12", -7.0, 47.45, -122.31)
        session.call("set_environment", key="time_of_day_mode", value=True)
        results = {}
        for heading in (0.0, 90.0):
            session.call("set_environment", key="heading", value=heading)
            session.call("capture", path=str(OUT / f"lit_heading_{int(heading)}.png"))
            state = session.call("get_state")
            sun = state["timeOfDay"]["sun"]
            direction = sun_vector(sun["gliderAzimuth"], sun["lightElevation"])
            width, height = state["viewports"][0]["width"], state["viewports"][0]["height"]
            worst, count = 0.0, 0
            for gy in range(-3, 4):
                for gx in range(-3, 4):
                    result = session.call("probe", x=width // 2 + gx * 30, y=height // 2 + gy * 30)
                    world = result.get("world")
                    if not world:
                        continue
                    n = [c / math.sqrt(sum(w * w for w in world)) for c in world]
                    expected = INTENSITY * sun["fade"] * max(0.0, sum(n[i] * direction[i] for i in range(3))) / math.pi
                    worst = max(worst, abs(result["hdrLuminance"] - expected) - PROBE_REL * expected)
                    count += 1
            results[heading] = direction
            check(count >= 40 and worst <= PROBE_ABS,
                  f"heading {heading:.0f}: {count} probes = intensity x N.L / pi for the sun at A3.1 azimuth "
                  f"{sun['gliderAzimuth']:.2f}, elevation {sun['lightElevation']:.2f} (worst excess {worst:.2e})")
        turned = angle_between(results[0.0], results[90.0])
        elevation = math.asin(results[0.0][1])
        expected_turn = math.degrees(math.acos(math.sin(elevation) ** 2))  # two directions 90 degrees apart in azimuth
        check(abs(turned - expected_turn) <= 0.5,
              f"+90 degrees of heading moves the lit sun by {turned:.2f} degrees on the sphere (90 degrees of azimuth at "
              f"elevation {math.degrees(elevation):.1f} = {expected_turn:.2f})")
        errors = session.close()
        check(errors == 0, f"lit sphere session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"lit sphere session failed: {error}")
        session.process.kill()


def check_fade(model, flags):
    print("[Below-horizon fade]")
    session = Session(model, flags)
    try:
        session.call("set_environment", key="sun_intensity", value=INTENSITY)
        # Equator at the March equinox: the sun rises due east and climbs 15 degrees per hour, so one minute is a
        # quarter of a degree; the nose points east (heading 90), putting the sun straight behind the camera (+Z).
        setup(session, "2026-03-20", "06:00:00", 0.0, 0.0, 0.0, heading=90.0)
        session.call("set_environment", key="time_of_day_mode", value=True)
        session.call("set_environment", key="sun_refraction", value=True)
        e0 = tod(session)["sun"]["lightElevation"]
        start_minutes = 6 * 60 + (-1.6 - e0) / 0.25
        samples = []
        for k in range(0, 27):
            seconds = int(round((start_minutes + k * 0.5) * 60.0))
            session.call("set_environment", key="time", value=f"{seconds // 3600:02d}:{(seconds // 60) % 60:02d}:{seconds % 60:02d}")
            session.call("capture", path=str(OUT / "fade.png"))
            state = session.call("get_state")
            sun = state["timeOfDay"]["sun"]
            width, height = state["viewports"][0]["width"], state["viewports"][0]["height"]
            value = session.call("probe", x=width // 2, y=height // 2)["hdrLuminance"]
            samples.append((sun["lightElevation"], sun["fade"], value))
        errors = session.close()
    except Exception as error:  # noqa: BLE001
        check(False, f"fade session failed: {error}")
        session.process.kill()
        return
    unit = INTENSITY / math.pi
    (OUT / "fade.json").write_text(json.dumps(samples), encoding="utf-8")
    check(samples[0][0] < -1.4 and samples[-1][0] > 1.4, f"sweep covers {samples[0][0]:.2f} .. {samples[-1][0]:.2f} degrees in {len(samples)} steps")
    # Normalised by the cosine of the elevation (N.L of the sphere's centre for a sun straight behind the camera)
    # the lit value is the fade itself: it rises monotonically, is 0 at and below -1 degree and 1 from +1 degree up,
    # and cannot step faster than the smoothstep's slope (0.75 per degree of lighting elevation).
    normalised = [v / (unit * math.cos(math.radians(e))) for e, _, v in samples]
    check(all(b >= a - 1e-3 for a, b in zip(normalised, normalised[1:])),
          "the lit value never decreases while the sun rises: " + ", ".join(f"{e:+.2f}:{n:.3f}" for (e, _, _), n in list(zip(samples, normalised))[::3]))
    check(all(abs(n - f) <= 2e-3 for (_, f, _), n in zip(samples, normalised)),
          "the rendered value equals the reported fade at every sample (max difference "
          f"{max(abs(n - f) for (_, f, _), n in zip(samples, normalised)):.4f})")
    below = [n for (e, _, _), n in zip(samples, normalised) if e <= -1.0]
    above = [n for (e, _, _), n in zip(samples, normalised) if e >= 1.0]
    check(below and max(below) <= 1e-3, f"zero at or below -1 degree ({len(below)} samples, max {max(below) if below else None})")
    check(above and all(abs(n - 1.0) <= 0.02 for n in above), f"full intensity from +1 degree up ({len(above)} samples within 2%)")
    slopes = [abs(b - a) / abs(e2 - e1) for (e1, _, _), (e2, _, _), a, b in zip(samples, samples[1:], normalised, normalised[1:]) if abs(e2 - e1) > 1e-6]
    check(max(slopes) <= 0.75 * 1.05 + 0.02,
          f"no discontinuity: the steepest rise between neighbouring samples is {max(slopes):.3f} per degree "
          f"(smoothstep bound 0.75; the NOAA refraction fit makes the lighting elevation move up to 1.7x faster than the geometric one near -0.6 degrees)")
    check(errors == 0, f"fade session: {errors} GL errors")


def check_day_night(session):
    print("[Automatic day/night]")
    # Equator at the equinox again: the sun sets at 15 degrees per hour; scan the evening in 30 s steps.
    setup(session, "2026-03-20", "17:00:00", 0.0, 0.0, 0.0)
    session.call("set_environment", key="time_of_day", value="auto")
    for threshold in (-6.0, -12.0):
        session.call("set_environment", key="night_threshold", value=threshold)
        flips = []
        previous = None
        for step in range(0, 241):
            seconds = 17 * 3600 + 20 * 60 + step * 30
            session.call("set_environment", key="time", value=f"{seconds // 3600:02d}:{(seconds // 60) % 60:02d}:{seconds % 60:02d}")
            state = tod(session)
            now = (state["dayNight"]["effective"], state["sun"]["lightElevation"])
            if previous is not None and previous[0] != now[0]:
                flips.append((previous, now))
            previous = now
        ok = len(flips) == 1 and flips[0][0][0] == "day" and flips[0][1][0] == "night" and \
            flips[0][0][1] >= threshold > flips[0][1][1]
        check(ok, f"threshold {threshold:+.0f}: one flip day -> night between elevations "
                  f"{flips[0][0][1]:+.3f} and {flips[0][1][1]:+.3f}" if flips else f"threshold {threshold:+.0f}: no flip")
    session.call("set_environment", key="night_threshold", value=-6.0)
    # Manual modes keep working whatever the sun does.
    session.call("set_environment", key="time_preset", value="night")
    session.call("set_environment", key="time_of_day", value="day")
    check(tod(session)["dayNight"]["effective"] == "day", "Day stays day at night")
    session.call("set_environment", key="time_of_day", value="night")
    session.call("set_environment", key="time_preset", value="noon")
    check(tod(session)["dayNight"]["effective"] == "night", "Night stays night at noon")
    session.call("set_environment", key="time_of_day", value="auto")
    check(tod(session)["dayNight"]["effective"] == "day", "Automatic is day at noon")
    session.call("set_environment", key="time_of_day", value="day")


def check_day_night_render():
    print("[Automatic day/night: the render path]")
    emissive.EMIS_DIR.mkdir(parents=True, exist_ok=True)
    model = emissive.make_quad_fixture("tod_daynight", [3000.0, 3000.0, 3000.0], is_asobo=True, day_mult=1.0, night_mult=0.1)
    shots = {}
    for label, extra in (("day", ["--time-of-day", "day"]), ("night", ["--time-of-day", "night"]),
                         ("auto_noon", ["--time-of-day", "auto", "--time-preset", "noon"]),
                         ("auto_night", ["--time-of-day", "auto", "--time-preset", "night"]),
                         ("auto_noon_threshold", ["--time-of-day", "auto", "--time-preset", "noon", "--night-threshold", "60"])):
        shots[label], _ = emissive.run_capture(f"tod_{label}", model, extra_args=["--aa", "none", "--bloom", "off"] + extra)
    check(not same(shots["day"], shots["night"]), "the quad renders differently by day and by night")
    check(same(shots["auto_noon"], shots["day"]), "Automatic at noon is byte-identical to Day")
    check(same(shots["auto_night"], shots["night"]), "Automatic at solar midnight is byte-identical to Night")
    check(same(shots["auto_noon_threshold"], shots["night"]), "Automatic with the threshold at +60 degrees flips noon to Night")


def check_registry(session):
    print("[Registry and CLI]")
    listed = {s["key"]: s for s in session.call("list_settings", scope="environment")["settings"]}
    wanted = ["time_of_day", "time_of_day_mode", "date", "time", "utc_offset", "latitude", "longitude", "heading",
              "time_preset", "sun_refraction", "night_threshold", "time_playing", "time_scale"]
    check(all(k in listed for k in wanted), f"list_settings lists the clock settings ({[k for k in wanted if k not in listed] or 'all'})")
    check(listed["time_of_day"]["values"] == ["day", "night", "auto"], f"time_of_day values {listed['time_of_day']['values']}")
    check(listed["time_of_day_mode"]["default"] is False and listed["time_preset"]["default"] == "custom" and
          listed["date"]["type"] == "string", "defaults: mode off, preset custom; date is a string setting")
    for key, value in (("date", "2026-02-30"), ("date", "26-1-1"), ("date", "2200-01-01"), ("time", "25:00"), ("time", "12:61"),
                       ("time", "noon"), ("time_preset", "midday"), ("latitude", 91), ("longitude", -181), ("utc_offset", 15),
                       ("time_scale", -1)):
        try:
            session.call("set_environment", key=key, value=value)
            check(False, f"{key}={value!r} is rejected")
        except glider_ctl.GliderError:
            check(True, f"{key}={value!r} is rejected")
    session.call("set_environment", key="time", value="7.5")
    check(session.call("get_environment", key="time")["value"] == "07:30:00", "time accepts decimal hours")
    session.call("set_environment", key="heading", value=-90)
    check(abs(session.call("get_environment", key="heading")["value"] - 270.0) < 1e-4, "heading wraps into [0, 360)")
    # Play: 3600x advances the clock by three minutes per 0.05 s frame; past midnight the date rolls.
    setup(session, "2026-04-15", "23:57:00", -7.0, 47.45, -122.31)
    session.call("set_environment", key="time_scale", value=3600)
    session.call("set_environment", key="time_playing", value=True)
    seen = []
    for _ in range(6):
        session.call("capture", path=str(OUT / "play.png"))
        state = tod(session)
        seen.append((state["date"], state["hours"]))
    session.call("set_environment", key="time_playing", value=False)
    rolled = any(d == "2026-04-16" for d, _ in seen)
    check(rolled and seen[-1][0] == "2026-04-16", f"playing at 3600x rolls the date past midnight: {seen[0]} .. {seen[-1]}")
    after = tod(session)
    session.call("capture", path=str(OUT / "play.png"))
    check(tod(session)["hours"] == after["hours"], "stopping the play freezes the clock")
    session.call("set_environment", key="time_preset", value="noon")
    session.call("set_environment", key="time_playing", value=True)
    check(tod(session)["preset"] == "custom", "playing from a preset turns it into plain time")
    session.call("set_environment", key="time_playing", value=False)


def check_cli(model, flags):
    print("[CLI flags equal the registry]")
    label, date, time, offset, latitude, longitude, azimuth, elevation = SOLAR_TABLE[1]
    png, report = OUT / "cli.png", OUT / "cli.json"
    report.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), str(model)] + COMMON + VIEW +
                            ["--time-of-day-mode", "on", "--date", date, "--time", time, "--utc-offset", str(offset),
                             "--latitude", str(latitude), "--longitude", str(longitude), "--heading", "135",
                             "--capture-viewport", str(png), "--capture-frame", "8", "--profile-report", str(report),
                             "--exit-after-profile"], cwd=REPO, capture_output=True, text=True, timeout=600)
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    check(data is not None and gl_errors_zero(result.stdout + result.stderr), "CLI run reports and has zero GL errors")
    if data is None:
        return
    state = data["timeOfDay"]
    error = separation(state["sun"]["azimuth"], state["sun"]["elevation"], azimuth, elevation)
    check(state["enabled"] and error <= SUN_TOLERANCE_DEG and state["date"] == date and state["time"] == time,
          f"--time-of-day-mode on --date --time --utc-offset --latitude --longitude: sun {error:.4f} deg from {label}")
    check(abs(wrap_signed(state["sun"]["gliderAzimuth"] - (135.0 - state["sun"]["azimuth"]))) <= 1e-3 and data["sun"]["mode"] == "time_of_day",
          f"--heading 135: A3.1 azimuth {state['sun']['gliderAzimuth']:.3f}; the profile report sun mode is {data['sun']['mode']}")
    result = subprocess.run([str(EXE), str(model)] + COMMON + VIEW +
                            ["--time-preset", "dusk", "--time-of-day", "auto", "--capture-viewport", str(png), "--capture-frame", "8",
                             "--profile-report", str(report), "--exit-after-profile"], cwd=REPO, capture_output=True, text=True, timeout=600)
    data = json.loads(report.read_text(encoding="utf-8"))["timeOfDay"]
    check(data["preset"] == "dusk" and abs(data["sun"]["elevation"] - 3.0) <= 0.01 and data["dayNight"]["mode"] == "auto" and
          not data["enabled"], f"--time-preset dusk --time-of-day auto (mode off): preset {data['preset']}, elevation "
                               f"{data['sun']['elevation']:.3f}, day/night {data['dayNight']['effective']}")
    bad = subprocess.run([str(EXE), str(model), "--isolated", "--date", "2026-13-01"], cwd=REPO, capture_output=True, text=True, timeout=120)
    check(bad.returncode != 0, f"--date 2026-13-01 is refused (exit {bad.returncode})")


def check_baseline(baseline):
    print("[Mode off: byte-identical to the baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    print(f"  INFO baseline {baseline.name}")
    clock_flags = ["--date", "2026-06-21", "--time", "03:00", "--latitude", "0", "--longitude", "10", "--heading", "123",
                   "--utc-offset", "2", "--time-preset", "night", "--time-of-day-mode", "off", "--night-threshold", "-3"]

    def shot(exe, model, name, flags):
        png = OUT / f"{name}.png"
        png.unlink(missing_ok=True)
        result = subprocess.run([str(exe)] + ([str(model)] if model else []) + COMMON + list(flags) +
                                ["--capture-viewport", str(png), "--capture-frame", "24", "--exit-after-profile",
                                 "--profile-report", str(OUT / f"{name}.json")], cwd=REPO, capture_output=True, text=True, timeout=900)
        if exe == EXE:
            check(gl_errors_zero(result.stdout + result.stderr), f"{name}: zero GL errors")
        return png if png.exists() else None

    for name, flags in (("default", []), ("clock_flags_mode_off", clock_flags)):
        now = shot(EXE, None, f"base_now_hellocube_{name}", flags)
        before = shot(baseline, None, f"base_ref_hellocube_{name}", [])
        check(same(now, before), f"HelloCube ({name}): byte-identical to {baseline.stem}")
    for name in ("exterior_fuselage_nose", "cockpit_main_displays"):
        try:
            entry = next(v for v in views() if v["name"] == name)
        except StopIteration:
            print(f"  SKIP reference view {name} not found")
            continue
        now = render_view(EXE, entry, f"base_now_{name}", clock_flags, folder=OUT)
        before = render_view(baseline, entry, f"base_ref_{name}", folder=OUT)
        if now is None and before is None:
            print(f"  SKIP {name}: E195 asset not available")
            continue
        check(same(now, before), f"E195 {name} (ASOBO), clock settings with the mode off: byte-identical to {baseline.stem}")
    try:
        entry = next(v for v in views() if v["name"] == "exterior_fuselage_nose")
        sun = ["--sun", "on", "--sun-dir", "45,40", "--sun-intensity", "8"]
        now = render_view(EXE, entry, "base_now_exterior_sun", clock_flags + sun, folder=OUT)
        before = render_view(baseline, entry, "base_ref_exterior_sun", sun, folder=OUT)
        if now is not None:
            check(same(now, before), f"E195 exterior with the Manual sun and shadows, mode off: byte-identical to {baseline.stem}")
    except StopIteration:
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=None)
    parser.add_argument("--regenerate-references", action="store_true", help="print the pyerfa reference rows (needs pyerfa) and exit")
    parser.add_argument("--only", default=None, help="comma-separated: baseline,solar,moon,heading,lit,fade,daynight,registry,cli")
    args = parser.parse_args()
    if args.regenerate_references:
        regenerate_references()
        return 0
    baseline = Path(args.baseline_exe) if args.baseline_exe else None
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None
    wanted = set(args.only.split(",")) if args.only else None

    def want(name):
        return wanted is None or name in wanted

    OUT.mkdir(parents=True, exist_ok=True)
    model = lambert_model()
    black = ["--env", str(write_hdr(OUT / "black.hdr", 64, 32, lambda x, y: (0.0, 0.0, 0.0)))]
    if want("baseline"):
        check_baseline(baseline)
    if wanted is None or wanted & {"solar", "moon", "heading", "registry", "daynight"}:
        session = Session(model, black)
        try:
            if want("solar"):
                check_solar_table(session)
                check_sun_events(session)
                check_presets(session)
            if want("moon"):
                check_moon(session)
            if want("heading"):
                check_heading(session)
            if want("daynight"):
                check_day_night(session)
            if want("registry"):
                check_registry(session)
            errors = session.close()
            check(errors == 0, f"state session: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"state session failed: {error}")
            session.process.kill()
    if want("lit"):
        check_lit_sphere(model, black)
    if want("fade"):
        check_fade(model, black)
    if want("daynight"):
        check_day_night_render()
    if want("cli"):
        check_cli(model, black)
    return finish()


if __name__ == "__main__":
    main()
