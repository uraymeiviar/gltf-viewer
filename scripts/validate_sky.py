# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.2 procedural sky and A6.3 dynamic environment lighting (docs/ASOBO_EXTENSIONS_PHASE_6.md).

Model (header/imgui/SkyAtmosphere.h): Hillaire 2020 (transmittance, multiple-scattering and sky-view LUTs) with
Bruneton 2017's Earth atmosphere; units: 1 environment unit = 12,800 lux, the top-of-atmosphere sun = 10 units.

Reference values (frozen below, no dependency at run time): scripts/sky_reference.py, an independent float64 numpy
implementation of the same model (direct transmittance integration along every ray, 2000-step single scattering
per direction, Psi_ms tabulated at 64 x 64 from 256 directions; no LUT shared with the GPU). --regenerate-references
prints them. They check the GPU implementation, its LUT parameterisations and resolution against the model itself
(the model's own physical validity is Hillaire's / Bruneton's).

Checks:
  * the HDR source (default) is byte-identical to the baseline binary (--baseline-exe): HelloCube default and with
    every sky setting changed while the source stays HDR, and two E195 reference views (ASOBO) with the sky settings;
  * sky radiance (background, probed over the control pipe) for a high sun (60 degrees) and a low sun (5 degrees) at
    SKY_DIRECTIONS against the reference: luminance within SKY_LUMINANCE_TOLERANCE, every channel within
    SKY_CHANNEL_TOLERANCE (relative);
  * noon zenith bluer than the noon horizon; the horizon towards the sun redder at sunset than at noon; the zenith,
    two horizon directions and the down-facing irradiance fall monotonically for sun elevations 10 .. -6 degrees;
  * the sun (get_state sky / sun): transmittance against the reference within SUN_TOLERANCE, luminance falling and
    red/blue rising as the elevation drops; the A3.1 sun the renderer uses equals it;
  * dynamic IBL: a white Lambert sphere's diffuse IBL (probe at normals the sun does not reach) against the
    reference irradiance (cosine-weighted mean radiance) within IRRADIANCE_TOLERANCE at both suns, i.e. the lighting
    follows the sky; the converged time-sliced result equals a full synchronous filter (sky_time_slicing off) within
    CONVERGED_TOLERANCE; while time plays, every time-sliced frame's extra GPU time (profile report) stays under the
    stated 4 ms budget, the reduced cubes are swapped in repeatedly and only complete jobs are swapped
    (incompleteSwaps 0); once time stops the cubes refine to the configured size;
  * ground albedo (A3.6 plane, override), ozone and Mie respond; registry keys listed, CLI = pipe (byte-identical),
    invalid values refused;
  * time of day + sky + shadows + ground plane together: finite probes, zero GL errors.

Run: python scripts/validate_sky.py [--baseline-exe build/baselines/<sha>.exe] [--only a,b] [--regenerate-references]
"""

import argparse
import json
import math
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same, views, render_view, REAL_ASSETS  # noqa: E402
from validate_sun import lambert  # noqa: E402

OUT = REPO / "build" / "sky"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
SKY = ["--environment", "sky", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0"]
LUMA = (0.2126, 0.7152, 0.0722)

SKY_LUMINANCE_TOLERANCE = 0.04   # relative, background radiance against the reference
SKY_CHANNEL_TOLERANCE = 0.05     # relative, per channel
SUN_TOLERANCE = 0.005            # relative, sun transmittance per channel
IRRADIANCE_TOLERANCE = 0.10      # relative, Lambert sphere diffuse IBL against the reference cosine mean
CONVERGED_TOLERANCE = 1.0e-4     # relative, converged time-sliced cubes against a full synchronous filter
BUDGET_MS = 4.0                  # stated per-frame limit of the time-sliced work (SkyEnvironment::kStatedBudgetMs)

SUN_AZIMUTH = 0.0
# (label, sun elevation): the two suns of the reference.
SUNS = [("high", 60.0), ("low", 5.0)]
# (azimuth, elevation) view directions, A3.1 convention (the sun is at azimuth 0).
SKY_DIRECTIONS = [(0.0, 85.0), (180.0, 45.0), (90.0, 30.0), (180.0, 10.0), (90.0, 5.0), (270.0, 15.0),
                  (40.0, 12.0), (180.0, 2.0), (180.0, -20.0)]
# Lambert sphere normals the sun never reaches at either elevation: horizontal away from the sun, and down.
NORMALS = [("away", (180.0, 0.0)), ("down", (0.0, -89.0))]
SUN_ELEVATIONS = [60.0, 30.0, 10.0, 5.0, 2.0, 0.5]
SUNSET_ELEVATIONS = [10.0, 8.0, 6.0, 4.0, 2.0, 0.0, -2.0, -4.0, -6.0]

# --- frozen reference values (scripts/sky_reference.py; --regenerate-references) ---
REFERENCE_SKY = {
    'high': {(0.0, 85.0): (0.0850507, 0.167144, 0.369762), (180.0, 45.0): (0.070488, 0.153641, 0.359252), (90.0, 30.0): (0.104675, 0.222967, 0.496734), (180.0, 10.0): (0.253474, 0.492895, 0.894194), (90.0, 5.0): (0.392769, 0.683229, 1.02098), (270.0, 15.0): (0.176605, 0.359434, 0.72102), (40.0, 12.0): (0.245357, 0.480746, 0.898612), (180.0, 2.0): (0.651108, 0.956952, 1.14696), (180.0, -20.0): (0.770687, 0.70316, 0.608041)},
    'low': {(0.0, 85.0): (0.0244308, 0.0389952, 0.0742563), (180.0, 45.0): (0.0440877, 0.0679752, 0.11884), (90.0, 30.0): (0.045897, 0.0707673, 0.122564), (180.0, 10.0): (0.199708, 0.255424, 0.283417), (90.0, 5.0): (0.183235, 0.213414, 0.189585), (270.0, 15.0): (0.0828465, 0.119474, 0.17423), (40.0, 12.0): (0.159856, 0.2094, 0.258606), (180.0, 2.0): (0.444489, 0.370456, 0.185768), (180.0, -20.0): (0.0458229, 0.0214602, 0.00600294)},
}
REFERENCE_IRRADIANCE = {
    'high': {'away': (0.478807, 0.524236, 0.616513), 'down': (0.770653, 0.702781, 0.60671)},
    'low': {'away': (0.0826949, 0.0833945, 0.0891428), 'down': (0.0456595, 0.0211739, 0.00570477)},
}
REFERENCE_SUN = {
    60.0: (0.9320353, 0.8497195, 0.7326387),
    30.0: (0.8856783, 0.755201, 0.5844539),
    10.0: (0.7149685, 0.4616657, 0.2239445),
    5.0: (0.5465486, 0.2517645, 0.064859),
    2.0: (0.3344124, 0.08681248, 0.006385298),
    0.5: (0.1709038, 0.02258761, 0.0003220167),
}

gl_logs = []


# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------

def direction(azimuth, elevation):
    az, el = math.radians(azimuth), math.radians(elevation)
    return (math.cos(el) * math.sin(az), math.sin(el), math.cos(el) * math.cos(az))


def luminance(rgb):
    return sum(c * w for c, w in zip(rgb, LUMA))


def rel(a, b):
    return abs(a - b) / max(abs(b), 1e-9)


def gl_errors_zero(log):
    lines = [line for line in log.splitlines() if line.startswith("GL automation errors")]
    return lines == ["GL automation errors: 0"] and "GL error 0x" not in log


def empty_model():
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "empty.gltf"
    path.write_text(json.dumps({"asset": {"version": "2.0", "generator": "A6.2 sky fixture"}, "scene": 0,
                                "scenes": [{"nodes": [0]}], "nodes": [{"name": "Empty"}]}), encoding="utf-8")
    return path


def lambert_model():
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "lambert.gltf"
    path.write_text(json.dumps(lambert()), encoding="utf-8")
    return path


def run(args, timeout=900):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(model, name, flags=(), exe=EXE, frame=24, common=COMMON):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    result = run([exe] + ([model] if model else []) + list(common) + list(flags) +
                 ["--capture-viewport", png, "--capture-frame", str(frame), "--profile-report", report, "--exit-after-profile"])
    log = result.stdout + result.stderr
    if exe == EXE:
        gl_logs.append((name, log))
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    return (png if png.exists() else None), data


class Session:
    def __init__(self, model, flags=()):
        args = [str(EXE), str(model)] + COMMON + list(flags) + ["--control-pipe"]
        self.process = subprocess.Popen(args, cwd=REPO, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                        stderr=subprocess.DEVNULL)
        deadline = time.time() + 120
        while not os.path.exists(glider_ctl.pipe_name(self.process.pid)):
            if self.process.poll() is not None or time.time() > deadline:
                raise RuntimeError(f"Glider did not open its pipe (exit {self.process.poll()})")
            time.sleep(0.1)
        while True:
            try:
                self.pipe = glider_ctl.GliderPipe(self.process.pid)
                break
            except glider_ctl.GliderError:
                if self.process.poll() is not None or time.time() > deadline:
                    self.process.kill()
                    raise
                time.sleep(0.2)
        self.size = None

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def env(self, key, value):
        return self.call("set_environment", key=key, value=value)

    def settle(self, frames=4):
        self.call("capture", path=str(OUT / "_settle.png"), settle_frames=frames, overlays=False)

    def converge(self, limit_frames=6000, step=16):
        """Renders until the procedural environment's cubes are converged; returns the frames it took (None: never)."""
        frames = 0
        while frames <= limit_frames:
            state = self.call("get_state")["sky"]["refine"]
            if state["state"] == "converged":
                return frames
            self.settle(step)
            frames += step
        return None

    def viewport_size(self):
        if self.size is None:
            vp = self.call("get_state")["viewports"][0]
            self.size = (vp["width"], vp["height"])
        return self.size

    def place(self, eye, target, fov_y=20.0):
        """set_camera with the eye at `eye` looking at `target`; the camera it reports is checked against the wanted one."""
        self.call("set_camera", eye=list(eye), target=list(target), fov_y=fov_y)
        camera = self.call("get_camera")
        got = [camera["target"][i] - camera["eye"][i] for i in range(3)]
        want = [target[i] - eye[i] for i in range(3)]
        cosine = sum(a * b for a, b in zip(got, want)) / max(math.sqrt(sum(a * a for a in got) * sum(b * b for b in want)), 1e-12)
        if cosine < math.cos(math.radians(0.05)):
            raise RuntimeError(f"set_camera looks along {got}, wanted {want}")

    def look(self, view):
        # Any eye works for the background (it is centred on the camera); looking outwards from 5 units along the
        # direction keeps a model at the origin behind the camera.
        self.place([5.0 * c for c in view], [6.0 * c for c in view])

    def centre_probe(self, frames=3):
        self.settle(frames)
        width, height = self.viewport_size()
        return self.call("probe", x=width // 2, y=height // 2)

    def sky(self, view):
        self.look(view)
        return self.centre_probe()["hdr"][:3]

    def sphere(self, normal):
        """Probe of the unit sphere at (about) `normal`: the camera looks at the origin from 4 units along it."""
        offset = (0.013, 0.0, 0.017) if abs(normal[1]) > 0.9 else (0.0, 0.0, 0.0)
        eye = [4.0 * normal[i] + offset[i] for i in range(3)]
        self.place(eye, [0.0, 0.0, 0.0])
        return self.centre_probe()

    def close(self):
        errors = None
        try:
            errors = self.call("get_state")["glErrors"]
            self.call("quit")
        finally:
            self.pipe.close()
            try:
                self.process.wait(60)
            except Exception:  # noqa: BLE001
                self.process.kill()
        return errors


def set_sun(session, elevation, azimuth=SUN_AZIMUTH):
    session.env("sun_dir", [azimuth, elevation])


# ---------------------------------------------------------------------------------------------
# References
# ---------------------------------------------------------------------------------------------

def regenerate_references():
    import sky_reference as ref
    atmosphere = ref.Atmosphere()
    atmosphere.build_psi()
    print("REFERENCE_SKY = {")
    for label, elevation in SUNS:
        sun = ref.direction(SUN_AZIMUTH, elevation)
        rows = []
        for az, el in SKY_DIRECTIONS:
            L = atmosphere.radiance(ref.direction(az, el), sun)
            rows.append(f"({az}, {el}): ({L[0]:.6g}, {L[1]:.6g}, {L[2]:.6g})")
        print(f"    {label!r}: {{" + ", ".join(rows) + "},")
    print("}")
    print("REFERENCE_IRRADIANCE = {")
    for label, elevation in SUNS:
        sun = ref.direction(SUN_AZIMUTH, elevation)
        rows = []
        for name, (az, el) in NORMALS:
            E = atmosphere.cosine_mean(ref.direction(az, el), sun)
            rows.append(f"{name!r}: ({E[0]:.6g}, {E[1]:.6g}, {E[2]:.6g})")
        print(f"    {label!r}: {{" + ", ".join(rows) + "},")
    print("}")
    print("REFERENCE_SUN = {")
    for elevation in SUN_ELEVATIONS:
        T = atmosphere.sun_transmittance(ref.direction(SUN_AZIMUTH, elevation))
        print(f"    {elevation}: ({T[0]:.7g}, {T[1]:.7g}, {T[2]:.7g}),")
    print("}")


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_baseline(baseline):
    print("[HDR source: byte-identical to the baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    print(f"  INFO baseline {baseline.name}")
    sky_flags = ["--environment", "hdr", "--sky-mie", "3", "--sky-ozone", "off", "--sky-ground-albedo", "0.6",
                 "--sky-time-slicing", "off", "--sky-budget-ms", "1"]
    for name, flags in (("default", []), ("sky_settings_hdr", sky_flags)):
        now, _ = render(None, f"base_now_hellocube_{name}", flags)
        before, _ = render(None, f"base_ref_hellocube_{name}", [], exe=baseline)
        check(same(now, before), f"HelloCube ({name}): byte-identical to {baseline.stem}")
    for name in ("exterior_fuselage_nose", "cockpit_main_displays"):
        try:
            entry = next(v for v in views() if v["name"] == name)
        except StopIteration:
            print(f"  SKIP reference view {name} not found")
            continue
        now = render_view(EXE, entry, f"base_now_{name}", sky_flags, folder=OUT)
        before = render_view(baseline, entry, f"base_ref_{name}", folder=OUT)
        if now is None and before is None:
            print(f"  SKIP {name}: E195 asset not available")
            continue
        check(same(now, before), f"E195 {name} (ASOBO), sky settings with the HDR source: byte-identical to {baseline.stem}")


def check_sky_reference(session):
    print("[Sky radiance against the reference (scripts/sky_reference.py)]")
    worst_l, worst_c = 0.0, 0.0
    for label, elevation in SUNS:
        set_sun(session, elevation)
        for (az, el), expected in REFERENCE_SKY[label].items():
            got = session.sky(direction(az, el))
            el_err = rel(luminance(got), luminance(expected))
            ch_err = max(rel(g, e) for g, e in zip(got, expected))
            worst_l, worst_c = max(worst_l, el_err), max(worst_c, ch_err)
            check(el_err <= SKY_LUMINANCE_TOLERANCE and ch_err <= SKY_CHANNEL_TOLERANCE,
                  f"{label} sun (el {elevation:g}), view az {az:g} el {el:g}: luminance {luminance(got):.4f} vs "
                  f"{luminance(expected):.4f} ({el_err * 100:+.1f}%), worst channel {ch_err * 100:.1f}%")
    print(f"  INFO worst luminance error {worst_l * 100:.2f}%, worst channel error {worst_c * 100:.2f}%")


def check_sky_behaviour(session):
    print("[Sky colour through the day and sunset]")
    set_sun(session, 60.0)
    zenith = session.sky(direction(0.0, 85.0))
    horizon = session.sky(direction(180.0, 2.0))
    blue = lambda c: c[2] / max(sum(c), 1e-9)  # noqa: E731
    check(blue(zenith) > blue(horizon), f"noon: zenith bluer than the horizon (blue share {blue(zenith):.3f} vs {blue(horizon):.3f})")
    noon_sunward = session.sky(direction(0.0, 3.0))
    set_sun(session, 2.0)
    sunset_sunward = session.sky(direction(0.0, 3.0))
    red = lambda c: c[0] / max(c[2], 1e-9)  # noqa: E731
    check(red(sunset_sunward) > 1.5 * red(noon_sunward),
          f"horizon towards the sun: red/blue {red(noon_sunward):.3f} at el 60 -> {red(sunset_sunward):.3f} at el 2")
    series = {"zenith": [], "horizon away": [], "horizon side": [], "lambert down": []}
    for elevation in SUNSET_ELEVATIONS:
        set_sun(session, elevation)
        series["zenith"].append(luminance(session.sky(direction(0.0, 85.0))))
        series["horizon away"].append(luminance(session.sky(direction(180.0, 5.0))))
        series["horizon side"].append(luminance(session.sky(direction(90.0, 5.0))))
    for name, values in series.items():
        if not values:
            continue
        falling = all(b < a for a, b in zip(values, values[1:]))
        check(falling and all(math.isfinite(v) and v >= 0.0 for v in values),
              f"{name} luminance falls monotonically for sun el {SUNSET_ELEVATIONS[0]:g} .. {SUNSET_ELEVATIONS[-1]:g}: "
              + ", ".join(f"{v:.3g}" for v in values))


def check_sun(session):
    print("[Sun colour and illuminance from the transmittance]")
    previous = None
    for elevation in SUN_ELEVATIONS:
        set_sun(session, elevation)
        state = session.call("get_state")
        sky, sun = state["sky"], state["sun"]
        transmittance = sky["sunTransmittance"]
        expected = REFERENCE_SUN[elevation]
        err = max(rel(t, e) for t, e in zip(transmittance, expected))
        check(err <= SUN_TOLERANCE, f"el {elevation:g}: transmittance {[round(t, 5) for t in transmittance]} vs reference "
                                    f"{[round(e, 5) for e in expected]} ({err * 100:.3f}%)")
        same_sun = sun["active"] and all(abs(a - 10.0 * b) <= 1e-4 * max(1.0, a) for a, b in zip(sun["illuminance"], transmittance))
        check(same_sun and sun["mode"] == "sky", f"el {elevation:g}: the renderer's A3.1 sun = 10 x transmittance "
                                                  f"({[round(c, 4) for c in sun['illuminance']]}, mode {sun['mode']})")
        lum, ratio = luminance(transmittance), transmittance[0] / max(transmittance[2], 1e-9)
        if previous is not None:
            check(lum < previous[0] and ratio > previous[1],
                  f"el {elevation:g}: dimmer ({lum:.4f} < {previous[0]:.4f}) and redder (r/b {ratio:.3f} > {previous[1]:.3f})")
        previous = (lum, ratio)
    set_sun(session, -3.0)
    sun = session.call("get_state")["sun"]
    check(not sun["active"] and luminance(sun["illuminance"]) == 0.0, "el -3: the sun is below the horizon and off")


def check_irradiance(session):
    print("[Dynamic IBL: Lambert sphere against the reference irradiance]")
    probes = {}
    for label, elevation in SUNS:
        set_sun(session, elevation)
        frames = session.converge()
        check(frames is not None, f"{label} sun: the time-sliced cubes converge ({frames} frames)")
        state = session.call("get_state")["sky"]["refine"]
        for name, (az, el) in NORMALS:
            normal = direction(az, el)
            probe = session.sphere(normal)
            got = probe["hdr"][:3]
            world = probe.get("world")
            hit = world is not None and sum(w * n for w, n in zip(world, normal)) > 0.995
            expected = REFERENCE_IRRADIANCE[label][name]
            err = rel(luminance(got), luminance(expected))
            check(hit and err <= IRRADIANCE_TOLERANCE,
                  f"{label} sun, normal {name}: {luminance(got):.4f} vs reference {luminance(expected):.4f} "
                  f"({err * 100:+.1f}%; cubes {state['frontSize']} px)")
            probes[(label, name)] = got
    return probes


def check_converged_equals_full(session, sliced):
    print("[Converged time-sliced cubes = full synchronous filter]")
    session.call("set_viewport_setting", key="sky_time_slicing", value=False)
    worst = 0.0
    for label, elevation in SUNS:
        set_sun(session, elevation + 1.0)  # a different sky first, so the synchronous path re-filters
        session.settle(2)
        set_sun(session, elevation)
        session.settle(2)
        state = session.call("get_state")["sky"]
        check(state["refine"]["state"] == "converged" and not state["timeSlicing"],
              f"{label} sun: time slicing off filters every change before the frame ({state['refine']['state']})")
        for name, (az, el) in NORMALS:
            full = session.sphere(direction(az, el))["hdr"][:3]
            err = max(rel(a, b) for a, b in zip(sliced[(label, name)], full))
            worst = max(worst, err)
            check(err <= CONVERGED_TOLERANCE, f"{label} sun, normal {name}: converged time-sliced vs full filter {err:.2e}")
    session.call("set_viewport_setting", key="sky_time_slicing", value=True)
    print(f"  INFO worst relative difference {worst:.2e}")


def check_budget(model):
    print("[Time-sliced updates while time plays: GPU budget, complete swaps, refinement]")
    flags = SKY + ["--time-of-day-mode", "on", "--time-preset", "afternoon", "--time-play", "on", "--time-scale", "1200"]
    png, data = render(model, "budget_play", flags, frame=240)
    if data is None:
        check(False, "time-play run produced a profile report")
        return
    sky = data["sky"]
    stats = sky["stats"]
    check(stats["slicedFrames"] >= 200 and stats["gpuMsMax"] <= BUDGET_MS and stats["overBudgetFrames"] == 0,
          f"{stats['slicedFrames']} time-sliced frames: max extra GPU {stats['gpuMsMax']:.3f} ms, mean {stats['gpuMsMean']:.3f} ms "
          f"(budget {BUDGET_MS} ms, aim {sky['budgetMs']} ms)")
    check(stats["swaps"] >= 40 and stats["fastJobs"] >= 40 and stats["incompleteSwaps"] == 0,
          f"{stats['swaps']} swaps of complete reduced-size jobs ({stats['fastJobs']} fast jobs, incomplete swaps "
          f"{stats['incompleteSwaps']}); front {sky['refine']['frontSize']} px while moving")
    check(sky["refine"]["frontSize"] < sky["refine"]["targetSize"] and stats["refineJobs"] == 0,
          f"no refinement while moving (front {sky['refine']['frontSize']} px, target {sky['refine']['targetSize']} px)")
    session = Session(model, SKY + ["--time-of-day-mode", "on", "--time-preset", "afternoon", "--time-scale", "1200"])
    try:
        session.converge()
        session.env("time_playing", True)
        session.settle(60)
        moving = session.call("get_state")["sky"]
        session.env("time_playing", False)
        frames = session.converge()
        done = session.call("get_state")["sky"]
        check(moving["refine"]["frontSize"] == 128 and frames is not None and done["refine"]["frontSize"] == done["refine"]["targetSize"]
              and done["stats"]["gpuMsMax"] <= BUDGET_MS and done["stats"]["refineJobs"] >= 1,
              f"play then stop: {moving['refine']['frontSize']} px while moving, refined to {done['refine']['frontSize']} px "
              f"{frames} frames after the stop; max extra GPU {done['stats']['gpuMsMax']:.3f} ms over "
              f"{done['stats']['slicedFrames']} frames (refinement included)")
        check(done["stats"]["incompleteSwaps"] == 0, f"only complete jobs swapped ({done['stats']['swaps']} swaps)")
        errors = session.close()
        check(errors == 0, f"budget session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"budget session failed: {error}")
        session.process.kill()


def check_parameters(session):
    print("[Atmosphere parameters and ground albedo]")
    set_sun(session, 30.0)
    session.converge()
    down = luminance(session.sphere(direction(0.0, -89.0))["hdr"][:3])
    zenith = session.sky(direction(0.0, 85.0))
    session.env("sky_ground_albedo", 0.6)
    session.converge()
    down_bright = luminance(session.sphere(direction(0.0, -89.0))["hdr"][:3])
    state = session.call("get_state")["sky"]
    check(state["groundAlbedoSource"] == "override" and down_bright > 1.6 * down,
          f"sky_ground_albedo 0.6: down-facing irradiance {down:.4f} -> {down_bright:.4f} (neutral 0.3)")
    session.env("sky_ground_albedo", -1.0)
    session.call("set_viewport_setting", key="ground", value="plane")
    session.call("set_viewport_setting", key="ground_preset", value="grass")
    session.settle(4)
    state = session.call("get_state")
    plane_albedo = state["ground"]["albedo"]
    check(state["sky"]["groundAlbedoSource"] == "plane" and
          all(abs(a - b) <= 1e-5 for a, b in zip(state["sky"]["groundAlbedo"], plane_albedo)),
          f"automatic ground albedo follows the A3.6 plane: {state['sky']['groundAlbedo']} = {plane_albedo}")
    session.call("set_viewport_setting", key="ground", value="grid")
    session.settle(2)
    session.env("sky_mie", 4.0)
    hazy = session.sky(direction(0.0, 85.0))
    check(luminance(hazy) > luminance(zenith) and hazy[2] / sum(hazy) < zenith[2] / sum(zenith),
          f"sky_mie 4: zenith brighter and whiter ({luminance(zenith):.4f} -> {luminance(hazy):.4f})")
    session.env("sky_mie", 1.0)
    set_sun(session, -4.0)
    with_ozone = session.sky(direction(0.0, 85.0))
    session.env("sky_ozone", False)
    without_ozone = session.sky(direction(0.0, 85.0))
    session.env("sky_ozone", True)
    check(with_ozone[2] / with_ozone[0] > without_ozone[2] / without_ozone[0],
          f"ozone keeps the twilight zenith blue: b/r {with_ozone[2] / with_ozone[0]:.3f} with, "
          f"{without_ozone[2] / without_ozone[0]:.3f} without")


def check_registry(model):
    print("[Registry, CLI and pipe]")
    session = Session(model, ["--hide-grid", "--sun-dir", "200,25"])
    try:
        environment = {s["key"] for s in session.call("list_settings", scope="environment")["settings"]}
        renderer = {s["key"] for s in session.call("list_settings", scope="renderer")["settings"]}
        keys = {"environment", "sky_mie", "sky_ozone", "sky_ground_albedo"}
        check(keys <= environment and {"sky_time_slicing", "sky_budget_ms"} <= renderer,
              "environment, sky_mie, sky_ozone, sky_ground_albedo, sky_time_slicing, sky_budget_ms are registered")
        refused = []
        for key, value in (("environment", "foo"), ("sky_mie", -1), ("sky_ground_albedo", 2)):
            try:
                session.env(key, value)
                refused.append(False)
            except glider_ctl.GliderError:
                refused.append(True)
        check(all(refused), "invalid environment / sky_mie / sky_ground_albedo values are refused")
        session.env("sky_mie", 2.0)
        session.env("environment", "sky")
        session.call("capture", path=str(OUT / "pipe_sky.png"), settle_frames=24, overlays=False)
        errors = session.close()
        check(errors == 0, f"registry session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"registry session failed: {error}")
        session.process.kill()
        return
    cli, _ = render(model, "cli_sky", ["--hide-grid", "--sun-dir", "200,25", "--sky-mie", "2", "--environment", "sky"])
    check(same(cli, OUT / "pipe_sky.png"), "set_environment environment sky = --environment sky (byte-identical capture)")
    bad = run([EXE, model, "--isolated", "--environment", "foo"], timeout=120)
    check(bad.returncode != 0, f"--environment foo is refused (exit {bad.returncode})")


def check_together():
    print("[Time of day + sky + shadows + ground plane]")
    exterior = REAL_ASSETS.get("Exterior")
    model = exterior if exterior and Path(exterior).exists() else lambert_model()
    flags = SKY + ["--time-of-day-mode", "on", "--time-preset", "dusk", "--ground", "plane", "--shadows", "on",
                   "--camera-yaw", "35", "--camera-pitch", "12", "--report-pick", "640,600"]
    png, data = render(model, "together", flags, frame=32)
    log = gl_logs[-1][1] if gl_logs else ""
    ok = data is not None and png is not None and gl_errors_zero(log)
    finite = False
    if ok:
        from PIL import Image
        import numpy as np
        pixels = np.asarray(Image.open(png).convert("RGB"), dtype=np.float64)
        finite = bool(np.isfinite(pixels).all() and pixels.mean() > 1.0)
        sun = data["sun"]
        finite = finite and all(math.isfinite(c) for c in sun["illuminance"])
    check(ok and finite, f"{Path(model).name}: dusk, procedural sky, shadows, ground plane: image and sun finite, zero GL errors")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=None)
    parser.add_argument("--regenerate-references", action="store_true", help="print the reference values (numpy, slow) and exit")
    parser.add_argument("--only", default=None, help="comma-separated: baseline,reference,behaviour,sun,ibl,budget,params,registry,together")
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
    if want("baseline"):
        check_baseline(baseline)
    empty = empty_model()
    if wanted is None or wanted & {"reference", "behaviour", "sun"}:
        session = Session(empty, SKY)
        try:
            if want("reference"):
                check_sky_reference(session)
            if want("behaviour"):
                check_sky_behaviour(session)
            if want("sun"):
                check_sun(session)
            errors = session.close()
            check(errors == 0, f"sky session: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"sky session failed: {error}")
            session.process.kill()
    sphere = lambert_model()
    if wanted is None or wanted & {"ibl", "params"}:
        session = Session(sphere, SKY)
        try:
            if want("ibl"):
                sliced = check_irradiance(session)
                check_converged_equals_full(session, sliced)
            if want("params"):
                check_parameters(session)
            errors = session.close()
            check(errors == 0, f"IBL session: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"IBL session failed: {error}")
            session.process.kill()
    if want("budget"):
        check_budget(sphere)
    if want("registry"):
        check_registry(sphere)
    if want("together"):
        check_together()
    for name, log in gl_logs:
        if not gl_errors_zero(log):
            check(False, f"{name}: GL errors in the log")
    return finish()


if __name__ == "__main__":
    main()
