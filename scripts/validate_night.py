# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.4 exposure across the day, A6.5 emissive day/night blend, A6.6 night sky (docs/ASOBO_EXTENSIONS_PHASE_6.md;
header/imgui/NightSky.h).

Checks:
  * defaults (HDR source, exposure mode default = Manual, clock off), also with every new setting changed, and the HDR
    with the clock at night: byte-identical to the baseline (HelloCube, two E195 ASOBO views);
  * exposure: E195 exterior on the A3.6 plane with the procedural sky and the clock, in Auto (the sky's default) and
    Curve: the mean displayed luminance (sRGB-encoded luma of the viewport image) at noon, sunset, dusk, night (moonless)
    and a full-moon night lies inside READABLE_BAND; over an evening sweep (TIME_STEP_MIN steps) the image changes
    smoothly (no step between consecutive captures above SWEEP_STEP_LIMIT after the adaptation settles) and in Curve
    mode it darkens monotonically (rises at most SWEEP_RISE_TOLERANCE);
  * moon: the full moon's disk centre in the image within MOON_CENTRE_TOLERANCE_PX of the projected A6.1 moon direction;
    lit fraction of the disk in the image within PHASE_TOLERANCE of A6.1's illuminatedFraction (gibbous, near-quarter);
    the bright limb towards the sun (the lit part's centroid offset against the projected sun direction from the moon,
    within LIMB_ANGLE_TOLERANCE degrees); full-moon centre radiance = the reported K x 1/2 (Lommel-Seeliger at full);
    full-moon illuminance above the atmosphere 0.2-0.35 lux;
  * moonlight: a white Lambert sphere at a full-moon night: the fitted light direction within MOONLIGHT_ANGLE_TOLERANCE
    of the moon direction and its strength = albedo x E / pi within MOONLIGHT_TOLERANCE; the shadows switch from the
    sun (noon) to the moon (night) in get_state, and a sphere above the ground plane shades the ground on the side away
    from the moon;
  * stars: detected in a moonless night image, none at noon; two times 1 h apart: the star field turns about the
    reported celestial pole by the sidereal angle (15.04 deg) within ROTATION_TOLERANCE, westwards;
  * emissive: Automatic day/night blends the ASOBO_material_emissive multipliers as a smoothstep from threshold + 3 to
    threshold - 3 degrees (radiance of an emissive quad = day x (1 - w) + night x w, w the reported nightBlend), exactly
    Day / Night outside;
  * registry keys and CLI flags, invalid values refused, pipe = CLI (byte-identical), zero GL errors;
  * A6.4b interiors (Auto with the highlight meter, the procedural sky's default): E195 cockpit main displays at night
    and the E195 windshield view at noon, the DA62 SDK cockpit at night (if installed, read only): the display screens
    (found by their nodes' screen rectangles, --report-node) are not clipped (99th percentile of the displayed luma in
    each rectangle < SCREEN_CLIP_LIMIT) and stand out from their bezels (95th - 5th percentile >= SCREEN_CONTRAST);
    the rest of the panel is not black (its lit legends and buttons: PANEL_LIT_FRACTION of it above PANEL_LIT_LUMA) at
    night, readable at noon (median >= NOON_PANEL_MEDIAN) with at most WINDOW_CLIP_FRACTION of the image clipped; the
    DA62's stars through the windows are counted (and checked with --exposure-metering average); a star field and the
    moon change the Auto EV by at most POINT_LIGHT_EV; Auto through the pipe ends in the same image as on the command
    line (byte-identical).

Run: python scripts/validate_night.py [--baseline-exe build/baselines/<sha>.exe] [--only a,b]
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
import validate_asobo_emissive as emissive  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO, check, finish, same, views, render_view  # noqa: E402
from validate_ground import without_ground_equipment  # noqa: E402
from validate_sun import lambert, Camera  # noqa: E402

OUT = REPO / "build" / "night"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
SKY = ["--environment", "sky", "--time-of-day-mode", "on", "--hide-grid"]
E195_CAMERA = ["--camera-yaw", "35", "--camera-pitch", "12"]
LUMA = (0.2126, 0.7152, 0.0722)

READABLE_BAND = (0.05, 0.60)     # mean sRGB-encoded luma of the viewport image
SWEEP_STEP_LIMIT = 0.03          # largest change of the mean between captures TIME_STEP_MIN apart
SWEEP_RISE_TOLERANCE = 0.01      # Curve mode: largest rise while the evening darkens (night plateau)
TIME_STEP_MIN = 5
MOON_CENTRE_TOLERANCE_PX = 1.0
PHASE_TOLERANCE = 0.1
LIMB_ANGLE_TOLERANCE = 15.0
MOONLIGHT_ANGLE_TOLERANCE = 3.0  # degrees
MOONLIGHT_TOLERANCE = 0.10
ROTATION_TOLERANCE = 1.0         # degrees
SIDEREAL_DEG_PER_HOUR = 15.041
EMISSIVE_TOLERANCE = 0.01
SCREEN_CLIP_LIMIT = 0.95         # A6.4b: 99th percentile of the displayed luma in a screen rectangle
SCREEN_CONTRAST = 0.3            # 95th - 5th percentile in a screen rectangle (screen against its bezel)
PANEL_LIT_LUMA = 0.1             # night: lit legends / buttons outside the screens
PANEL_LIT_FRACTION = 0.005
NOON_PANEL_MEDIAN = 0.04
WINDOW_CLIP_FRACTION = 0.02      # noon cockpit: pixels with a channel >= 250 / 255 outside the screens
POINT_LIGHT_EV = 0.25

E195_SCREENS = ["SCREEN_EICAS", "LCD_MFD_SCREEN_L", "LCD_MFD_SCREEN_R", "LCD_PFD_SCREEN_L", "LCD_PFD_SCREEN_R"]
DA62 = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
DA62_COCKPIT = DA62 / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo/Part_Interior_Cockpit/model/Cockpit_LOD00.gltf"
DA62_CAMERA = ["--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05", "--camera-pitch", "-20", "--camera-fov", "55",
               "--camera-yaw", "220", "--search-path", str(DA62 / "Sources/Art/Max/textures")]
DA62_SCREENS = ["COCKPIT_Attitude_Screen", "COCKPIT_Speed_Screen"]
NIGHT_EMISSIVE = ["--time-preset", "night", "--time-of-day", "auto"]

# Dates (Seattle, UTC-7): the 2026-05-01 full moon (17:23 UTC) at 23:30 local is 15 degrees up; 2026-04-26 22:00 a
# waxing gibbous; 2026-04-22 21:30 a crescent before the first quarter; the default 2026-04-15 is two days before the
# new moon (no moon at night).
FULL_MOON = ["--date", "2026-05-01", "--time", "23:30"]

gl_logs = []


# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------

def luminance(rgb):
    return sum(c * w for c, w in zip(rgb, LUMA))


def rel(a, b):
    return abs(a - b) / max(abs(b), 1e-12)


def angle_between(a, b):
    dot = sum(x * y for x, y in zip(a, b)) / max(math.sqrt(sum(x * x for x in a) * sum(y * y for y in b)), 1e-30)
    return math.degrees(math.acos(max(-1.0, min(1.0, dot))))


def gl_errors_zero(log):
    lines = [line for line in log.splitlines() if line.startswith("GL automation errors")]
    return lines == ["GL automation errors: 0"] and "GL error 0x" not in log


def write(name, doc):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def empty_model():
    return write("empty", {"asset": {"version": "2.0", "generator": "A6.6 night fixture"}, "scene": 0,
                           "scenes": [{"nodes": [0]}], "nodes": [{"name": "Empty"}]})


def run(args, timeout=900):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(model, name, flags=(), exe=EXE, frame=24, common=None):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    result = run([exe] + ([model] if model else []) + list(COMMON if common is None else common) + list(flags) +
                 ["--capture-viewport", png, "--capture-frame", str(frame), "--profile-report", report, "--exit-after-profile"])
    if exe == EXE:
        gl_logs.append((name, result.stdout + result.stderr))
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    return (png if png.exists() else None), data


def load(png):
    import numpy as np
    from PIL import Image
    return np.asarray(Image.open(png).convert("RGB"), dtype=np.float64) / 255.0


def mean_luma(png):
    a = load(png)
    return float((a[..., 0] * LUMA[0] + a[..., 1] * LUMA[1] + a[..., 2] * LUMA[2]).mean())


class Session:
    def __init__(self, model, flags=(), common=None):
        args = [str(EXE), str(model)] + list(COMMON if common is None else common) + list(flags) + ["--control-pipe"]
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

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def env(self, key, value):
        return self.call("set_environment", key=key, value=value)

    def capture(self, name, frames=4):
        path = OUT / f"{name}.png"
        self.call("capture", path=str(path), settle_frames=frames, overlays=False)
        return path

    def place(self, eye, target, fov_y):
        """set_camera with the eye at `eye` looking at `target` (set_camera mirrors the requested offset's x and y, also
        in the baseline: validate_sky.py), checked against get_camera; returns a Camera of the viewport."""
        offset = [eye[i] - target[i] for i in range(3)]
        request = [target[0] - offset[0], target[1] - offset[1], target[2] + offset[2]]
        self.call("set_camera", eye=request, target=list(target), fov_y=fov_y)
        return self.camera(want=[target[i] - eye[i] for i in range(3)])

    def look(self, view, fov_y):
        return self.place([0.0, 0.0, 0.0], list(view), fov_y)

    def camera(self, want=None):
        cam = self.call("get_camera")
        vp = self.call("get_state")["viewports"][0]
        got = [cam["target"][i] - cam["eye"][i] for i in range(3)]
        if want is not None and angle_between(got, want) > 0.02:
            raise RuntimeError(f"set_camera looks along {got}, wanted {want}")
        return Camera({"camera": {"eye": cam["eye"], "target": cam["target"], "up": cam["up"], "fovY": cam["fovY"],
                                  "viewport": [vp["width"], vp["height"]]}})

    def probe(self, x, y):
        return self.call("probe", x=int(round(x)), y=int(round(y)))["hdr"][:3]

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


def session_check(name, function):
    """Runs function(session_factory) and turns an exception into a failed check."""
    try:
        function()
    except Exception as error:  # noqa: BLE001
        check(False, f"{name} failed: {error}")


def e195_model():
    exterior = REAL_ASSETS.get("Exterior")
    if not exterior or not Path(exterior).exists():
        return None
    OUT.mkdir(parents=True, exist_ok=True)
    return without_ground_equipment(exterior, OUT / "e195_no_gse.gltf")


def sunset_times():
    """The default date's sunset (local clock, geometric -0.83 deg) and dusk (sunset + 35 min, about -6 deg)."""
    _, data = render(empty_model(), "_events", SKY + ["--time-preset", "noon"], frame=2)
    sunset = data["timeOfDay"]["events"]["sunset"]["hours"]
    return sunset, sunset + 35.0 / 60.0


def hhmm(hours):
    hours %= 24.0
    return f"{int(hours):02d}:{int(hours * 60) % 60:02d}:{int(round(hours * 3600)) % 60:02d}"


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_baseline(baseline):
    print("[Defaults: byte-identical to the baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    print(f"  INFO baseline {baseline.name}")
    # Every new setting changed while the HDR source keeps them inactive (exposure mode default = Manual).
    night_flags = ["--moon", "off", "--stars", "off", "--moonlight-scale", "3", "--star-density", "2", "--sky-airglow", "4",
                   "--night-vision", "0.2", "--exposure-min-ev100", "-8", "--exposure-max-ev100", "16"]
    for name, flags in (("default", []), ("night_settings", night_flags),
                        ("explicit_manual", ["--exposure-mode", "manual", "--tone-mapping", "hill"])):
        now, _ = render(None, f"base_now_hellocube_{name}", flags)
        before, _ = render(None, f"base_ref_hellocube_{name}", [], exe=baseline)
        check(same(now, before), f"HelloCube ({name}): byte-identical to {baseline.stem}")
    # The baseline has the A6.1 clock: HDR source with the clock at night and Automatic day/night.
    clock = ["--time-of-day-mode", "on", "--time-preset", "night", "--time-of-day", "auto"]
    now, _ = render(None, "base_now_hellocube_clock_night", clock + night_flags)
    before, _ = render(None, "base_ref_hellocube_clock_night", clock, exe=baseline)
    check(same(now, before), f"HelloCube, HDR with the clock at night (moon/stars flags set): byte-identical to {baseline.stem}")
    for name in ("exterior_fuselage_nose", "cockpit_main_displays"):
        try:
            entry = next(v for v in views() if v["name"] == name)
        except StopIteration:
            print(f"  SKIP reference view {name} not found")
            continue
        now = render_view(EXE, entry, f"base_now_{name}", night_flags, folder=OUT)
        before = render_view(baseline, entry, f"base_ref_{name}", folder=OUT)
        if now is None and before is None:
            print(f"  SKIP {name}: E195 asset not available")
            continue
        check(same(now, before), f"E195 {name} (ASOBO), night settings with the HDR source: byte-identical to {baseline.stem}")


def check_exposure():
    print("[A6.4 exposure across the day: E195 exterior, procedural sky, clock]")
    model = e195_model()
    if model is None:
        print("  SKIP E195 exterior not available")
        return
    sunset, dusk = sunset_times()
    moments = (("noon", ["--time-preset", "noon"]), ("sunset", ["--time", hhmm(sunset)]), ("dusk", ["--time", hhmm(dusk)]),
               ("night", ["--time-preset", "night"]), ("full_moon", FULL_MOON))
    for mode in ("auto", "curve"):
        for name, when in moments:
            flags = SKY + ["--ground", "plane"] + E195_CAMERA + when + ["--exposure-mode", mode]
            png, data = render(model, f"exposure_{mode}_{name}", flags, frame=24)
            if png is None or data is None:
                check(False, f"{mode} {name}: no capture")
                continue
            value = mean_luma(png)
            night = data["night"]
            vp = night["exposure"]["viewports"][0]
            check(READABLE_BAND[0] <= value <= READABLE_BAND[1] and night["exposure"]["physical"],
                  f"{mode:5s} {name:9s}: mean displayed luma {value:.3f} in {READABLE_BAND} (EV {vp['exposureEv']:.2f}, curve "
                  f"EV100 {night['exposure']['curveEv100']:.2f} = {night['exposure']['curveIlluminanceLux']:.4g} lux, "
                  f"sun el {data['timeOfDay']['sun']['elevation']:.1f})")
    # Default mode with the sky is Auto, with the HDR Manual.
    _, data = render(model, "exposure_default_mode", SKY + ["--ground", "plane"] + E195_CAMERA, frame=2)
    check(data["night"]["exposure"]["mode"] == "default" and data["night"]["exposure"]["effectiveMode"] == "auto" and
          data["night"]["exposure"]["metering"] == "highlight",
          f"default exposure mode with the procedural sky resolves to auto with highlight metering (A6.4b; "
          f"{data['night']['exposure']['effectiveMode']}, {data['night']['exposure'].get('metering')})")

    # Evening sweep: sunset - 30 min .. sunset + 2.5 h.
    for mode in ("curve", "auto"):
        session = Session(model, SKY + ["--ground", "plane", "--window-size", "640x360"] + E195_CAMERA +
                          ["--exposure-mode", mode, "--time", hhmm(sunset - 0.5)])
        try:
            values = []
            steps = int(180 / TIME_STEP_MIN)
            # Step -1 is a warm-up: the startup frame has the synchronously filtered full-size IBL, every later step the
            # time-sliced one (A6.3), which differs by about 0.01 in the mean.
            for i in range(-1, steps + 1):
                session.env("time", hhmm(sunset - 0.5 + i * TIME_STEP_MIN / 60.0))
                # 24 frames: the time-sliced IBL (A6.3) has caught up with the sky, and Auto has adapted.
                png = session.capture(f"sweep_{mode}", frames=24)
                if i >= 0:
                    values.append(mean_luma(png))
            jumps = [abs(b - a) for a, b in zip(values, values[1:])]
            rises = [b - a for a, b in zip(values, values[1:])]
            check(max(jumps) <= SWEEP_STEP_LIMIT,
                  f"{mode} sweep ({steps} steps of {TIME_STEP_MIN} min): largest step {max(jumps):.4f} <= {SWEEP_STEP_LIMIT} "
                  f"(range {min(values):.3f} .. {max(values):.3f})")
            check(all(READABLE_BAND[0] <= v <= READABLE_BAND[1] for v in values),
                  f"{mode} sweep: every frame readable ({min(values):.3f} .. {max(values):.3f})")
            if mode == "curve":
                check(max(rises) <= SWEEP_RISE_TOLERANCE,
                      f"curve sweep darkens monotonically (largest rise {max(rises):.4f} <= {SWEEP_RISE_TOLERANCE})")
            (OUT / f"sweep_{mode}.json").write_text(json.dumps(values), encoding="utf-8")
            errors = session.close()
            check(errors == 0, f"{mode} sweep session: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"{mode} sweep failed: {error}")
            session.process.kill()


def moon_image(date, when, name):
    """Looks at the moon (empty scene, 1.5 deg field) and returns the image, the projected centre, radius (px), the
    projected direction of the sun from the moon, and the state."""
    session = Session(empty_model(), SKY + ["--date", date, "--time", when, "--bloom", "off"])
    try:
        state = session.call("get_state")
        moon = state["night"]["moon"]
        camera = session.look(moon["direction"], 1.5)
        png = session.capture(name, frames=8)
        state = session.call("get_state")
        moon = state["night"]["moon"]
        d = moon["direction"]
        cx, cy = camera.project([camera.eye[i] + d[i] for i in range(3)])
        radius = math.radians(moon["angularRadius"])
        side = [camera.r[i] - sum(camera.r[j] * d[j] for j in range(3)) * d[i] for i in range(3)]
        n = math.sqrt(sum(c * c for c in side))
        edge = [math.cos(radius) * d[i] + math.sin(radius) * side[i] / n for i in range(3)]
        ex, ey = camera.project([camera.eye[i] + edge[i] for i in range(3)])
        r_px = math.hypot(ex - cx, ey - cy)
        s = moon["sunFromMoon"]
        toward = [d[i] + 0.001 * s[i] for i in range(3)]
        sx, sy = camera.project([camera.eye[i] + toward[i] for i in range(3)])
        centre_hdr = session.probe(cx, cy)
        errors = session.close()
        return {"png": png, "centre": (cx, cy), "radius": r_px, "sun": (sx - cx, sy - cy), "state": state,
                "centreHdr": centre_hdr, "errors": errors}
    except Exception:
        session.process.kill()
        raise


def check_moon():
    print("[A6.6 moon: position, phase, bright limb, radiance]")
    import numpy as np
    for date, when, label in (("2026-05-01", "23:30", "full"), ("2026-04-26", "22:00", "gibbous"),
                              ("2026-04-22", "21:30", "crescent")):
        try:
            m = moon_image(date, when, f"moon_{label}")
        except Exception as error:  # noqa: BLE001
            check(False, f"moon {label} session failed: {error}")
            continue
        state = m["state"]
        moon, tod_moon = state["night"]["moon"], state["timeOfDay"]["moon"]
        a = load(m["png"])
        luma = a[..., 0] * LUMA[0] + a[..., 1] * LUMA[1] + a[..., 2] * LUMA[2]
        cx, cy = m["centre"]
        r = m["radius"]
        h, w = luma.shape
        ys, xs = np.mgrid[0:h, 0:w]
        inside = (xs - cx) ** 2 + (ys - cy) ** 2 <= (r + 2.0) ** 2
        background = float(np.median(luma[~inside]))
        lit = inside & (luma > background + 0.5 * (luma[inside].max() - background))
        fraction = float(lit.sum()) / (math.pi * r * r)
        expected = tod_moon["illuminatedFraction"]
        check(abs(fraction - expected) <= PHASE_TOLERANCE and abs(moon["illuminatedFraction"] - expected) < 1e-9,
              f"{label} ({date} {when}, el {moon['elevation']:.1f}): lit fraction in the image {fraction:.3f} vs A6.1 "
              f"{expected:.3f} (disk radius {r:.1f} px)")
        if label == "full":
            weights = np.clip(luma - background, 0.0, None) * inside
            mx = float((weights * xs).sum() / weights.sum())
            my = float((weights * ys).sum() / weights.sum())
            error = math.hypot(mx - cx, my - cy)
            check(error <= MOON_CENTRE_TOLERANCE_PX,
                  f"full moon disk centre {error:.2f} px from the projected A6.1 moon direction ({cx:.1f}, {cy:.1f})")
            k = moon["diskScale"]
            got = m["centreHdr"]
            worst = max(rel(g, 0.5 * kk) for g, kk in zip(got, k))
            check(worst <= 0.02, f"full moon centre radiance {[round(g, 4) for g in got]} = K / 2 {[round(0.5 * kk, 4) for kk in k]} "
                                 f"({worst * 100:.2f}%; Lommel-Seeliger at full phase)")
            top_lux = moon["illuminanceTop"][1] * 12800.0
            check(0.2 <= top_lux <= 0.35, f"full moon illuminance above the atmosphere {top_lux:.3f} lux "
                                          f"(phase angle {moon['phaseAngle']:.1f} deg; about 0.25 lux expected)")
        else:
            ly, lx = np.nonzero(lit)
            offset = (float(lx.mean()) - cx, float(ly.mean()) - cy)
            diff = abs((math.degrees(math.atan2(offset[1], offset[0]) - math.atan2(m["sun"][1], m["sun"][0])) + 180.0) % 360.0 - 180.0)
            check(diff <= LIMB_ANGLE_TOLERANCE,
                  f"{label}: the lit side faces the sun (centroid direction {diff:.1f} deg from the projected sun direction)")
        check(m["errors"] == 0, f"moon {label} session: {m['errors']} GL errors")


def check_moonlight():
    print("[A6.6 moonlight: white Lambert sphere, shadows from the moon]")
    model = write("lambert", lambert())
    session = Session(model, SKY + FULL_MOON + ["--bloom", "off", "--exposure-mode", "curve"])
    try:
        state = session.call("get_state")
        moon = state["night"]["moon"]
        light = state["night"]["directionalLight"]
        check(light["source"] == "moon" and angle_between(light["direction"], moon["direction"]) < 1e-3 and
              moon["lighting"],
              f"night: the directional light is the moon (el {moon['elevation']:.1f}, {moon['illuminanceLux']:.3f} lux)")
        d = moon["direction"]
        # Camera on the moon's side, 30 deg off, looking at the sphere.
        side = [d[2], 0.0, -d[0]]
        eye = [6.0 * (0.87 * d[i] + 0.5 * side[i] / math.hypot(side[0], side[2])) for i in range(3)]
        camera = session.place(eye, [0.0, 0.0, 0.0], 30.0)
        session.capture("moonlight_sphere", frames=6)
        rows = []
        for py in range(int(camera.height * 0.2), int(camera.height * 0.8), 9):
            for px in range(int(camera.width * 0.2), int(camera.width * 0.8), 9):
                p = camera.hit_unit_sphere(px, py)
                if p is None:
                    continue
                cosine = sum(p[i] * d[i] for i in range(3))
                if cosine < 0.25:
                    continue
                rows.append((p, luminance(session.probe(px, py))))
        import numpy as np
        A = np.array([[1.0, p[0], p[1], p[2]] for p, _ in rows])
        b = np.array([v for _, v in rows])
        coef, *_ = np.linalg.lstsq(A, b, rcond=None)
        c = coef[1:]
        expected = luminance(moon["illuminance"]) / math.pi
        angle = angle_between(c.tolist(), d)
        check(len(rows) > 100 and angle <= MOONLIGHT_ANGLE_TOLERANCE,
              f"lit sphere: fitted light direction {angle:.2f} deg from the moon ({len(rows)} samples)")
        check(rel(float(np.linalg.norm(c)), expected) <= MOONLIGHT_TOLERANCE,
              f"moonlight strength {float(np.linalg.norm(c)):.4g} vs albedo x E / pi {expected:.4g} "
              f"({rel(float(np.linalg.norm(c)), expected) * 100:.1f}%)")
        shadow = state["sun"]["shadows"]["viewports"][0]
        state = session.call("get_state")
        shadow = state["sun"]["shadows"]["viewports"][0]
        check(shadow["active"] and shadow.get("light") == "moon" and angle_between(shadow["lightDirection"], d) < 1e-3,
              f"night: the A3.2 shadow map is rendered for the moon ({shadow.get('light')})")
        session.env("time_preset", "noon")
        session.capture("_noon", frames=4)
        state = session.call("get_state")
        shadow = state["sun"]["shadows"]["viewports"][0]
        check(shadow["active"] and shadow.get("light") == "sun" and state["night"]["directionalLight"]["source"] == "sun",
              f"noon: the shadows and the directional light are the sun's ({shadow.get('light')})")
        errors = session.close()
        check(errors == 0, f"moonlight session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"moonlight session failed: {error}")
        session.process.kill()

    # A sphere 1 unit above the plane: its moon shadow lies on the side away from the moon.
    session = Session(model, SKY + FULL_MOON + ["--bloom", "off", "--exposure-mode", "curve", "--ground", "plane",
                                                "--ground-height", "-2"])
    try:
        moon = session.call("get_state")["night"]["moon"]
        d = moon["direction"]
        t = 1.0 / d[1]
        shadow_point = [-d[0] * t, -1.0 - 1.0, -d[2] * t]
        shadow_point = [-d[0] * (1.0 / d[1]) * 1.0, -2.0, -d[2] * (1.0 / d[1]) * 1.0]
        lit_point = [d[0] * (1.0 / d[1]), -2.0, d[2] * (1.0 / d[1])]
        camera = session.place([0.0, 14.0, 0.01], [0.0, -2.0, 0.0], 50.0)
        session.capture("moon_shadow", frames=6)
        sx, sy = camera.project(shadow_point)
        lx, ly = camera.project(lit_point)
        in_shadow = luminance(session.probe(sx, sy))
        lit = luminance(session.probe(lx, ly))
        check(in_shadow < 0.5 * lit, f"ground plane: the sphere's moon shadow at the anti-moon point {in_shadow:.3g} vs "
                                     f"{lit:.3g} on the moon's side")
        errors = session.close()
        check(errors == 0, f"moon shadow session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"moon shadow session failed: {error}")
        session.process.kill()


def detect_stars(png, camera, background_margin=0.06):
    """Local maxima clearly above the local background; returns world directions."""
    import numpy as np
    a = load(png)
    luma = a[..., 0] * LUMA[0] + a[..., 1] * LUMA[1] + a[..., 2] * LUMA[2]
    h, w = luma.shape
    from scipy import ndimage  # noqa: F401  (available in the Anaconda used by the harnesses)
    local_max = ndimage.maximum_filter(luma, size=5) == luma
    background = ndimage.median_filter(luma, size=15)
    peaks = local_max & (luma > background + background_margin)
    peaks[:3, :] = peaks[-3:, :] = False
    peaks[:, :3] = peaks[:, -3:] = False
    ys, xs = np.nonzero(peaks)
    out = []
    for x, y in zip(xs, ys):
        # Sub-pixel centroid over 3 x 3.
        win = np.clip(luma[y - 1:y + 2, x - 1:x + 2] - background[y, x], 0.0, None)
        if win.sum() <= 0:
            continue
        gy, gx = np.mgrid[-1:2, -1:2]
        fx = x + float((win * gx).sum() / win.sum())
        fy = y + float((win * gy).sum() / win.sum())
        out.append(camera.ray(fx - 0.5 + 0.5, fy - 0.5 + 0.5))
    return out


def rotate(v, axis, degrees):
    a = math.radians(degrees)
    k = axis
    c, s = math.cos(a), math.sin(a)
    dot = sum(v[i] * k[i] for i in range(3))
    cross = [k[1] * v[2] - k[2] * v[1], k[2] * v[0] - k[0] * v[2], k[0] * v[1] - k[1] * v[0]]
    return [v[i] * c + cross[i] * s + k[i] * dot * (1 - c) for i in range(3)]


def check_stars():
    print("[A6.6 stars: present at night, absent at noon, turning with the sidereal time]")
    model = empty_model()
    # Looking north-east, 40 deg up (heading 0: north is glTF +Z, east is A3.1 azimuth -90 = 270).
    az = math.radians(315.0)
    el = math.radians(40.0)
    view = [math.cos(el) * math.sin(az), math.sin(el), math.cos(el) * math.cos(az)]
    session = Session(model, SKY + ["--date", "2026-04-16", "--time", "00:30", "--bloom", "off", "--exposure-mode", "curve"])
    try:
        camera = session.look(view, 60.0)
        first = session.capture("stars_0030", frames=6)
        state1 = session.call("get_state")["night"]["stars"]
        stars1 = detect_stars(first, camera)
        session.env("time", "01:30")
        second = session.capture("stars_0130", frames=6)
        state2 = session.call("get_state")["night"]["stars"]
        stars2 = detect_stars(second, camera)
        check(len(stars1) >= 50 and len(stars2) >= 50 and state1["fade"] == 1.0,
              f"moonless night: {len(stars1)} / {len(stars2)} stars detected in a 60 deg view (fade {state1['fade']})")
        pole = state1["celestialPole"]
        sidereal = (state2["localSiderealDegrees"] - state1["localSiderealDegrees"]) % 360.0
        check(abs(sidereal - SIDEREAL_DEG_PER_HOUR) < 0.01, f"reported local sidereal time advances {sidereal:.3f} deg in 1 h")

        def matches(angle):
            count = 0
            for s in stars1:
                r = rotate(s, pole, angle)
                best = min(angle_between(r, t) for t in stars2)
                count += best < 0.12
            return count

        best_angle, best_count = 0.0, -1
        for tenth in range(-300, 301, 5):
            c = matches(tenth / 10.0)
            if c > best_count:
                best_angle, best_count = tenth / 10.0, c
        for tenth in range(int(best_angle * 10) - 6, int(best_angle * 10) + 7):
            c = matches(tenth / 10.0)
            if c > best_count:
                best_angle, best_count = tenth / 10.0, c
        expected = -sidereal  # the sky turns westwards: clockwise about the north pole axis
        check(abs(best_angle - expected) <= ROTATION_TOLERANCE and best_count >= 0.5 * min(len(stars1), len(stars2)),
              f"the star field turns {best_angle:.1f} deg about the celestial pole in 1 h (expected {expected:.2f}; "
              f"{best_count} of {len(stars1)} stars matched within 0.12 deg)")
        session.env("time_preset", "noon")
        noon = session.capture("stars_noon", frames=6)
        found = detect_stars(noon, camera)
        state = session.call("get_state")["night"]["stars"]
        check(len(found) == 0 and state["fade"] == 0.0, f"noon: {len(found)} stars (fade {state['fade']})")
        errors = session.close()
        check(errors == 0, f"stars session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"stars session failed: {error}")
        session.process.kill()


def check_emissive():
    print("[A6.5 Automatic day/night blends the ASOBO_material_emissive multipliers over twilight]")
    emissive.EMIS_DIR.mkdir(parents=True, exist_ok=True)
    model = emissive.make_quad_fixture("night_blend", [3000.0, 3000.0, 3000.0], is_asobo=True, day_mult=1.0, night_mult=0.1)
    black = OUT / "black.hdr"
    if not black.exists():
        from validate_asobo_sss import write_hdr
        OUT.mkdir(parents=True, exist_ok=True)
        write_hdr(black, 64, 32, lambda x, y: (0.0, 0.0, 0.0))
    session = Session(model, ["--env", str(black), "--time-of-day-mode", "on", "--time-of-day", "auto", "--bloom", "off",
                              "--auto-exposure", "off", "--exposure", "0", "--camera-projection", "ortho", "--camera-yaw", "0",
                              "--camera-pitch", "0", "--camera-distance", "2", "--sun", "off"])
    try:
        sunset, _ = sunset_times_from(session)
        threshold = -6.0
        samples = []
        hours = sunset + 6.0 / 60.0  # the sun's light (and its specular on the quad) has faded out
        for i in range(90):
            session.env("time", hhmm(hours))
            session.capture("_emissive", frames=2)
            state = session.call("get_state")
            vp = state["viewports"][0]
            value = luminance(session.probe(vp["width"] / 2, vp["height"] / 2))
            tod = state["timeOfDay"]
            samples.append((tod["sun"]["lightElevation"], tod["dayNight"]["nightBlend"], tod["dayNight"]["effective"], value))
            hours += 1.0 / 60.0
            if tod["sun"]["lightElevation"] < threshold - 4.0:
                break
        day = next(v for e, w, _, v in samples if w == 0.0)
        worst_formula, worst_render, flips = 0.0, 0.0, []
        for e, w, eff, v in samples:
            x = min(max((threshold + 3.0 - e) / 6.0, 0.0), 1.0)
            worst_formula = max(worst_formula, abs(w - x * x * (3.0 - 2.0 * x)))
            worst_render = max(worst_render, abs(v - day * ((1.0 - w) * 1.0 + w * 0.1)) / day)
            flips.append((e, eff))
        blended = [s for s in samples if 0.0 < s[1] < 1.0]
        check(worst_formula < 1e-4 and len(blended) >= 10,
              f"night weight = smoothstep from {threshold + 3:g} to {threshold - 3:g} deg ({len(blended)} blended samples, "
              f"worst {worst_formula:.2e})")
        check(worst_render <= EMISSIVE_TOLERANCE,
              f"quad radiance = day x (1 - w) + night x w over the sweep (worst {worst_render * 100:.2f}% of day)")
        changes = [(a, b) for a, b in zip(flips, flips[1:]) if a[1] != b[1]]
        check(len(changes) == 1 and changes[0][0][0] >= threshold > changes[0][1][0],
              "the reported day/night state flips once, at the threshold")
        errors = session.close()
        check(errors == 0, f"emissive session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"emissive session failed: {error}")
        session.process.kill()


def sunset_times_from(session):
    session.env("time_preset", "noon")
    events = session.call("get_state")["timeOfDay"]["events"]
    sunset = events["sunset"]["hours"]
    return sunset, sunset + 35.0 / 60.0


def check_registry():
    print("[Registry, CLI, pipe = CLI]")
    model = empty_model()
    session = Session(model, SKY)
    try:
        listed = {s["key"]: s for s in session.call("list_settings", scope="environment")["settings"]}
        wanted = ["exposure_mode", "exposure_min_ev100", "exposure_max_ev100", "moon", "moonlight_scale", "stars",
                  "star_density", "sky_airglow", "night_vision", "tone_mapping", "auto_exposure", "exposure_metering"]
        missing = [k for k in wanted if k not in listed]
        check(not missing, f"registry keys listed ({', '.join(wanted)}); missing {missing}")
        check(listed["exposure_mode"]["values"] == ["default", "manual", "auto", "curve"],
              f"exposure_mode values {listed['exposure_mode'].get('values')}")
        refused = 0
        for key, value in (("exposure_mode", "sometimes"), ("moonlight_scale", -1), ("star_density", 50),
                           ("tone_mapping", "filmic"), ("night_vision", 2), ("exposure_metering", "spot")):
            try:
                session.env(key, value)
            except glider_ctl.GliderError:
                refused += 1
        check(refused == 6, f"invalid values refused ({refused} of 6)")
        session.env("exposure_mode", "manual")
        check(session.call("get_environment", key="auto_exposure")["value"] is False, "exposure_mode manual: auto_exposure off")
        session.env("auto_exposure", True)
        check(session.call("get_environment", key="exposure_mode")["value"] == "auto", "auto_exposure on: exposure_mode auto")
        errors = session.close()
        check(errors == 0, f"registry session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"registry session failed: {error}")
        session.process.kill()
    for flag, value in (("--exposure-mode", "sometimes"), ("--moon", "maybe"), ("--stars", "maybe"),
                        ("--moonlight-scale", "-1"), ("--exposure-metering", "spot")):
        result = run([EXE, model, "--isolated", flag, value], timeout=120)
        check(result.returncode != 0, f"{flag} {value} is refused (exit {result.returncode})")
    # Pipe = CLI.
    flags = ["--date", "2026-04-26", "--time", "22:00", "--exposure-mode", "curve", "--moonlight-scale", "2",
             "--star-density", "1.5", "--night-vision", "0.5"]
    # Synchronous IBL filtering, so both paths end with the same cubes.
    cli, _ = render(model, "pipe_cli_reference", SKY + ["--sky-time-slicing", "off"] + flags, frame=12)
    session = Session(model, SKY + ["--sky-time-slicing", "off"])
    try:
        for key, value in (("date", "2026-04-26"), ("time", "22:00"), ("exposure_mode", "curve"), ("moonlight_scale", 2),
                           ("star_density", 1.5), ("night_vision", 0.5)):
            session.env(key, value)
        piped = session.capture("pipe_cli_piped", frames=12)
        errors = session.close()
        check(same(cli, piped), "pipe set_environment = CLI flags (byte-identical capture)")
        check(errors == 0, f"pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session failed: {error}")
        session.process.kill()


def luma_image(png):
    a = load(png)
    return a, a[..., 0] * LUMA[0] + a[..., 1] * LUMA[1] + a[..., 2] * LUMA[2]


def screen_rects(data, shape):
    """Clipped [x0, y0, x1, y1] of every reported node on screen (capture-image coordinates)."""
    rects = {}
    for node in (data or {}).get("nodes", []):
        if not node.get("onScreen"):
            continue
        x0, y0, x1, y1 = [int(round(v)) for v in node["screenRect"]]
        x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, shape[1]), min(y1, shape[0])
        if x1 - x0 >= 4 and y1 - y0 >= 4:
            rects[node["name"]] = (x0, y0, x1, y1)
    return rects


def interior(name, model, flags, screens, search_paths=()):
    """Captures an interior and the screens' rectangles; returns (rgb, luma, rects, outside-screens mask, report)."""
    import numpy as np
    extra = [a for p in search_paths for a in ("--search-path", p)] + [a for n in screens for a in ("--report-node", n)]
    png, data = render(model, name, SKY + extra + list(flags), frame=24)
    if png is None or data is None:
        return None
    rgb, luma = luma_image(png)
    rects = screen_rects(data, luma.shape)
    outside = np.ones(luma.shape, dtype=bool)
    for x0, y0, x1, y1 in rects.values():
        outside[y0:y1, x0:x1] = False
    return rgb, luma, rects, outside, data


def check_screens(label, luma, rects, expected):
    import numpy as np
    check(len(rects) >= expected, f"{label}: {len(rects)} screen rectangles in the image (>= {expected})")
    for node, (x0, y0, x1, y1) in sorted(rects.items()):
        region = luma[y0:y1, x0:x1]
        p99 = float(np.percentile(region, 99))
        contrast = float(np.percentile(region, 95) - np.percentile(region, 5))
        check(p99 < SCREEN_CLIP_LIMIT and contrast >= SCREEN_CONTRAST,
              f"{label} {node}: not clipped (99th percentile {p99:.3f} < {SCREEN_CLIP_LIMIT}), against its bezel "
              f"{contrast:.3f} >= {SCREEN_CONTRAST}")


def exposure_of(data):
    vp = data["night"]["exposure"]["viewports"][0]
    return vp["exposureEv"], vp.get("meter") or {}


def describe(label, data):
    ev, meter = exposure_of(data)
    print(f"  INFO {label}: EV {ev:.2f}; average meter {meter.get('averageEv', 0):.2f}, highlight meter "
          f"{meter.get('highlightEv', 0):.2f} ({meter.get('highlightLuminance', 0):.3g} units, "
          f"{meter.get('surfaceFraction', 0) * 100:.0f}% surfaces); limited by {meter.get('limitedBy')}")
    return ev, meter


def stars_in(png, top=0.6):
    """Star-like local maxima clearly above the local background in the upper part of the image (the DA62 view's
    windows; its lit keypad is below), detect_stars without a camera."""
    import numpy as np
    from scipy import ndimage
    _, luma = luma_image(png)
    local_max = ndimage.maximum_filter(luma, size=5) == luma
    background = ndimage.median_filter(luma, size=15)
    peaks = local_max & (luma > background + 0.06)
    peaks[:3, :] = peaks[-3:, :] = False
    peaks[:, :3] = peaks[:, -3:] = False
    peaks[int(top * peaks.shape[0]):, :] = False
    return list(zip(*np.nonzero(peaks)))


def check_interiors():
    print("[A6.4b interiors: Auto with the highlight meter (the procedural sky's default)]")
    import numpy as np
    from validate_asobo_common import E195_SEARCH_PATHS, view
    cockpit = REAL_ASSETS.get("Cockpit")
    if cockpit and Path(cockpit).exists():
        result = interior("interior_e195_night", cockpit, list(view("cockpit_main_displays")["flags"]) + NIGHT_EMISSIVE,
                          E195_SCREENS, E195_SEARCH_PATHS)
        if result is None:
            check(False, "E195 cockpit night: no capture")
        else:
            rgb, luma, rects, outside, data = result
            _, meter = describe("E195 cockpit night (main displays)", data)
            check(data["night"]["exposure"]["effectiveMode"] == "auto" and meter.get("limitedBy") == "highlight",
                  "E195 cockpit night: the default exposure is Auto, limited by the highlight meter")
            check_screens("E195 cockpit night", luma, rects, 5)
            lit = float((luma[outside] >= PANEL_LIT_LUMA).mean())
            check(lit >= PANEL_LIT_FRACTION, f"E195 cockpit night: panel not black: {lit * 100:.2f}% of it (outside the "
                                             f"screens) above {PANEL_LIT_LUMA} (>= {PANEL_LIT_FRACTION * 100:.1f}%; "
                                             f"99th percentile {float(np.percentile(luma[outside], 99)):.3f})")
        result = interior("interior_e195_noon", cockpit, list(view("cockpit_windshield_inside")["flags"]) +
                          ["--time-preset", "noon", "--time-of-day", "auto"], E195_SCREENS, E195_SEARCH_PATHS)
        if result is None:
            check(False, "E195 cockpit noon: no capture")
        else:
            rgb, luma, rects, outside, data = result
            describe("E195 cockpit noon (windshield view)", data)
            check_screens("E195 cockpit noon", luma, rects, 2)
            clipped = float((rgb.max(axis=2)[outside] >= 250.0 / 255.0).mean())
            median = float(np.median(luma[outside]))
            check(clipped <= WINDOW_CLIP_FRACTION, f"E195 cockpit noon: exterior through the windows not blown "
                                                   f"({clipped * 100:.2f}% clipped <= {WINDOW_CLIP_FRACTION * 100:.0f}%)")
            check(median >= NOON_PANEL_MEDIAN, f"E195 cockpit noon: panel readable (median displayed luma {median:.3f} "
                                               f">= {NOON_PANEL_MEDIAN})")
    else:
        print("  SKIP E195 cockpit not available")
    if DA62_COCKPIT.exists():
        result = interior("interior_da62_night", DA62_COCKPIT, NIGHT_EMISSIVE + DA62_CAMERA, DA62_SCREENS)
        if result is None:
            check(False, "DA62 cockpit night: no capture")
        else:
            rgb, luma, rects, outside, data = result
            describe("DA62 cockpit night", data)
            check_screens("DA62 cockpit night", luma, rects, 2)
            # Stars through the windows: next to the not clipped 2.5-5 unit (32,000-64,000 cd/m^2) MSFS-scaled displays
            # they are far below black. Counted, and checked with the average meter (the A6.4 behaviour).
            print(f"  INFO DA62 cockpit night: {len(stars_in(OUT / 'interior_da62_night.png'))} stars visible with the "
                  f"highlight meter")
            png, data = render(DA62_COCKPIT, "interior_da62_night_average", SKY + NIGHT_EMISSIVE + DA62_CAMERA +
                               ["--exposure-metering", "average"], frame=24)
            count = len(stars_in(png)) if png else 0
            check(count >= 20 and data["night"]["exposure"]["metering"] == "average",
                  f"DA62 cockpit night, --exposure-metering average: {count} stars visible through the windows")
    else:
        print("  SKIP DA62 SDK cockpit not installed")

    print("[A6.4b small bright points do not change the Auto exposure]")
    model = e195_model()
    if model is not None:
        base = SKY + ["--ground", "plane"] + E195_CAMERA + ["--time-preset", "night"]
        _, on = render(model, "point_stars_on", base, frame=24)
        _, off = render(model, "point_stars_off", base + ["--stars", "off"], frame=24)
        if on and off:
            a, b = exposure_of(on)[0], exposure_of(off)[0]
            check(abs(a - b) <= POINT_LIGHT_EV, f"E195 exterior, star field on / off: Auto EV {a:.3f} / {b:.3f} "
                                                f"(|difference| {abs(a - b):.3f} <= {POINT_LIGHT_EV})")
        else:
            check(False, "star field on / off: no capture")
    # A single LED on a surface: a small emissive sphere (radius 0.04, 1 unit = 12,800 cd/m^2) in front of the Lambert
    # sphere at a full-moon night, lit / unlit (emissive 0: the same geometry, so the framing is the same).
    for lit in (True, False):
        doc = lambert()
        doc["materials"].append({"name": "LED", "emissiveFactor": [1.0, 0.25, 0.1] if lit else [0.0, 0.0, 0.0],
                                 "pbrMetallicRoughness": {"baseColorFactor": [0.02, 0.02, 0.02, 1], "metallicFactor": 0.0}})
        doc["meshes"].append({"name": "LED", "primitives": [dict(doc["meshes"][0]["primitives"][0], material=1)]})
        doc["nodes"].append({"name": "LED", "mesh": 1, "translation": [0.35, 0.35, 1.05], "scale": [0.04, 0.04, 0.04]})
        doc["scenes"][0]["nodes"].append(len(doc["nodes"]) - 1)
        write(f"led_{'on' if lit else 'off'}", doc)
    _, on = render(OUT / "led_on.gltf", "point_led_on", SKY + FULL_MOON + ["--camera-yaw", "0", "--camera-pitch", "0"], frame=24)
    _, off = render(OUT / "led_off.gltf", "point_led_off", SKY + FULL_MOON + ["--camera-yaw", "0", "--camera-pitch", "0"], frame=24)
    if on and off:
        a, b = exposure_of(on)[0], exposure_of(off)[0]
        check(abs(a - b) <= POINT_LIGHT_EV, f"single LED on a surface on / off: Auto EV {a:.3f} / {b:.3f} (|difference| "
                                            f"{abs(a - b):.3f} <= {POINT_LIGHT_EV}; limited by "
                                            f"{exposure_of(on)[1].get('limitedBy')})")
    else:
        check(False, "single LED on / off: no capture")
    # The moon disk in view, on / off, in a scene without surfaces (so its light changes nothing else).
    session = Session(empty_model(), SKY + FULL_MOON)
    try:
        moon = session.call("get_state")["night"]["moon"]
        session.look(moon["direction"], 60.0)
        values = []
        for setting in (True, False):
            session.env("moon", setting)
            session.capture(f"point_moon_{'on' if setting else 'off'}", frames=8)
            values.append(session.call("get_state")["night"]["exposure"]["viewports"][0]["exposureEv"])
        a, b = values
        check(abs(a - b) <= POINT_LIGHT_EV, f"moon in view on / off: Auto EV {a:.3f} / {b:.3f} (|difference| "
                                            f"{abs(a - b):.3f} <= {POINT_LIGHT_EV})")
        errors = session.close()
        check(errors == 0, f"moon session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"moon session failed: {error}")
        session.process.kill()

    print("[A6.4b Auto: command line = pipe after convergence]")
    flags = ["--time", "20:40", "--ground", "plane", "--ground-height", "-1", "--exposure-compensation", "0.5"]
    model = write("lambert", lambert())
    cli, _ = render(model, "auto_cli_reference", SKY + ["--sky-time-slicing", "off"] + flags, frame=16)
    session = Session(model, SKY + ["--sky-time-slicing", "off"])
    try:
        session.env("time", "20:40")
        session.call("set_viewport_setting", key="ground", value="plane")
        session.call("set_viewport_setting", key="ground_height_mode", value="custom")
        session.call("set_viewport_setting", key="ground_height", value=-1)
        session.env("exposure_compensation", 0.5)
        piped = session.capture("auto_pipe", frames=16)
        state = session.call("get_state")["night"]["exposure"]
        errors = session.close()
        check(state["effectiveMode"] == "auto" and same(cli, piped),
              "Auto (the sky's default): pipe settings = CLI flags (byte-identical capture)")
        check(errors == 0, f"Auto pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"Auto pipe session failed: {error}")
        session.process.kill()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", type=Path, default=None)
    parser.add_argument("--only", default="")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    baseline = args.baseline_exe
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None
    elif not baseline.is_absolute():
        baseline = REPO / baseline
    only = {s for s in args.only.split(",") if s}
    for name, function in (("baseline", lambda: check_baseline(baseline)), ("exposure", check_exposure), ("moon", check_moon),
                           ("moonlight", check_moonlight), ("stars", check_stars), ("emissive", check_emissive),
                           ("registry", check_registry), ("interiors", check_interiors)):
        if only and name not in only:
            continue
        session_check(name, function)
    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in {len(gl_logs)} captures" + (f" (errors in {bad})" if bad else ""))
    return finish()


if __name__ == "__main__":
    sys.exit(main())
