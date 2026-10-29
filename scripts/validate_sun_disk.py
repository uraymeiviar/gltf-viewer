# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.7 sun disk and glare (docs/ASOBO_EXTENSIONS_PHASE_6.md; header/imgui/SunDisk.h).

The disk is drawn in the visible background only (cubemap.frag SUN_DISK) while the sun is on: mean radiance
E / (pi sin^2 R) (E the A3.1 sun illuminance the renderer uses, R the A3.1 angular radius x sun_disk_scale), Hestroffer &
Magnan limb darkening mu^alpha normalised to that mean, an antialiased edge. Its glare is the existing bloom; an optional
per-viewport lens effect (ghosts / halo / anamorphic streak) is built from the bloom mips.

Checks:
  * sun off (the default) with the HDR source, with every disk / lens setting changed: byte-identical to the baseline
    (HelloCube, two E195 ASOBO views); the sun on with --sun-disk off: byte-identical to the baseline's sun-on render;
  * HDR source (black panorama, Manual sun, R = 1 deg, limb darkening off): the disk's centre (first moments of the
    probed linear radiance) lies within CENTRE_TOLERANCE_PX of the sun direction's projection, its diameter
    (sqrt of the integrated radiance / interior radiance) within DIAMETER_TOLERANCE_PX of 2 R projected through the
    camera, the interior radiance within RADIANCE_TOLERANCE of E / (pi sin^2 R); the same with sun_disk_scale 2;
  * limb darkening: the centre radiance = mean x (alpha + 2) / 2 x mu^alpha (RADIANCE_TOLERANCE), the profile at
    rho 0.5 and 0.8 follows mu^alpha per channel within LIMB_TOLERANCE, the centre is brighter and bluer than the edge;
  * procedural sky with the A6.1 clock: the disk is centred on the clock's sun direction (CENTRE_TOLERANCE_PX), has the
    A3.1 size (DIAMETER_TOLERANCE_PX), the transmittance colour of the sun illuminance and the reported radiance;
  * not in the IBL input: a white Lambert sphere with the disk in view renders identically with the disk on and off
    (HDR and procedural sky; the sky's IBL is neither re-filtered nor swapped), and the only pixels that change are
    within the disk;
  * mirror sphere: the A3.1 highlight's energy-equivalent diameter (integrated / peak radiance) is the size of the
    disk's ray-traced reflection within MIRROR_SIZE_TOLERANCE and its peak radiance E / (pi tan^2 R) within
    MIRROR_PEAK_TOLERANCE (GGX widened by tan(R) / 2 integrates to E and peaks at that value);
  * glare: bloom off leaves no light outside the disk edge (the capture is black beyond 2 px); bloom on spreads it;
    the lens effect off, at intensity 0, or with bloom off is byte-identical to the effect-free render, on adds light
    only with the disk in view (below its threshold the image is byte-identical);
  * registry keys, invalid values refused, pipe = CLI (byte-identical), zero GL errors.

Run: python scripts/validate_sun_disk.py [--baseline-exe build/baselines/<sha>.exe] [--only a,b]
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
from validate_asobo_common import EXE, REPO, check, finish, same, views, render_view  # noqa: E402
from validate_sun import lambert, mirror, Camera  # noqa: E402
from validate_asobo_sss import write_hdr  # noqa: E402

OUT = REPO / "build" / "sun_disk"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
LUMA = (0.2126, 0.7152, 0.0722)
LIMB_ALPHA = (0.397, 0.503, 0.652)

CENTRE_TOLERANCE_PX = 1.0     # disk centre against the projected sun direction
DIAMETER_TOLERANCE_PX = 1.0   # disk diameter against 2 R projected
RADIANCE_TOLERANCE = 0.01     # interior / centre radiance against the formula (half-float storage: 0.1%)
LIMB_TOLERANCE = 0.03         # limb profile against mu^alpha (per channel)
MIRROR_SIZE_TOLERANCE = 0.15  # highlight energy-equivalent diameter against the ray-traced disk reflection
MIRROR_PEAK_TOLERANCE = 0.10  # highlight peak against E / (pi tan^2 R)

gl_logs = []


# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------

def direction(azimuth, elevation):
    az, el = math.radians(azimuth), math.radians(elevation)
    return [math.cos(el) * math.sin(az), math.sin(el), math.cos(el) * math.cos(az)]


def luminance(rgb):
    return sum(c * w for c, w in zip(rgb, LUMA))


def rel(a, b):
    return abs(a - b) / max(abs(b), 1e-9)


def gl_errors_zero(log):
    lines = [line for line in log.splitlines() if line.startswith("GL automation errors")]
    return lines == ["GL automation errors: 0"] and "GL error 0x" not in log


def write(name, doc):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def empty_model():
    return write("empty", {"asset": {"version": "2.0", "generator": "A6.7 sun disk fixture"}, "scene": 0,
                           "scenes": [{"nodes": [0]}], "nodes": [{"name": "Empty"}]})


def black_env():
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "black.hdr"
    if not path.exists():
        write_hdr(path, 64, 32, lambda x, y: (0.0, 0.0, 0.0))
    return ["--env", str(path)]


def run(args, timeout=900):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(model, name, flags=(), exe=EXE, frame=24):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    result = run([exe] + ([model] if model else []) + COMMON + list(flags) +
                 ["--capture-viewport", png, "--capture-frame", str(frame), "--profile-report", report, "--exit-after-profile"])
    if exe == EXE:
        gl_logs.append((name, result.stdout + result.stderr))
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    return (png if png.exists() else None), data


def load(png):
    import numpy as np
    from PIL import Image
    return np.asarray(Image.open(png).convert("RGB"), dtype=np.float64)


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

    def settle(self, frames=4, name="_settle"):
        path = OUT / f"{name}.png"
        self.call("capture", path=str(path), settle_frames=frames, overlays=False)
        return path

    def converge(self, limit_frames=6000, step=16):
        frames = 0
        while frames <= limit_frames:
            if self.call("get_state")["sky"]["refine"]["state"] == "converged":
                return frames
            self.settle(step)
            frames += step
        return None

    def place(self, eye, target, fov_y):
        """set_camera with the eye at `eye` looking at `target`, checked against get_camera; returns a Camera."""
        self.call("set_camera", eye=list(eye), target=list(target), fov_y=fov_y)
        return self.camera(want=[target[i] - eye[i] for i in range(3)])

    def look(self, view, fov_y):
        return self.place([0.0, 0.0, 0.0], list(view), fov_y)

    def camera(self, want=None):
        cam = self.call("get_camera")
        vp = self.call("get_state")["viewports"][0]
        got = [cam["target"][i] - cam["eye"][i] for i in range(3)]
        if want is not None:
            cosine = sum(a * b for a, b in zip(got, want)) / max(math.sqrt(sum(a * a for a in got) * sum(b * b for b in want)), 1e-12)
            if cosine < math.cos(math.radians(0.02)):
                raise RuntimeError(f"set_camera looks along {got}, wanted {want}")
        return Camera({"camera": {"eye": cam["eye"], "target": cam["target"], "up": cam["up"], "fovY": cam["fovY"],
                                  "viewport": [vp["width"], vp["height"]]}})

    def probe(self, x, y):
        return self.call("probe", x=int(x), y=int(y))["hdr"][:3]

    def region(self, x0, y0, x1, y1):
        r = self.call("probe_region", x0=int(x0), y0=int(y0), x1=int(x1), y1=int(y1))
        n = (int(x1) - int(x0)) * (int(y1) - int(y0))
        return {"sum": [c * n for c in r["rgb"]["mean"]], "lsum": r["luminance"]["mean"] * n, "max": r["rgb"]["max"],
                "lmax": r["luminance"]["max"], "n": n}

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


def offset_direction(base, d_azimuth, d_elevation):
    az = math.degrees(math.atan2(base[0], base[2]))
    el = math.degrees(math.asin(max(-1.0, min(1.0, base[1]))))
    return direction(az + d_azimuth, el + d_elevation)


def disk_axes(camera, sun, radius):
    """Projected centre and the half-axes in pixels of a disk of angular radius `radius` (radians) around `sun`."""
    cx, cy = camera.project([camera.eye[i] + sun[i] for i in range(3)])
    halves = []
    for axis in (camera.r, camera.u):
        # A direction at the angle `radius` from the sun, rotated about the axis perpendicular to `axis` and `sun`.
        side = [axis[i] - sum(axis[j] * sun[j] for j in range(3)) * sun[i] for i in range(3)]
        n = math.sqrt(sum(c * c for c in side))
        side = [c / n for c in side]
        points = []
        for sign in (1.0, -1.0):
            d = [math.cos(radius) * sun[i] + sign * math.sin(radius) * side[i] for i in range(3)]
            points.append(camera.project([camera.eye[i] + d[i] for i in range(3)]))
        halves.append(0.5 * math.hypot(points[0][0] - points[1][0], points[0][1] - points[1][1]))
    return (cx, cy), halves


def measure_disk(session, centre, half_axes, background=(0.0, 0.0, 0.0), margin=6):
    """Centre and diameter of the disk from probe_region sums (radiance x coverage) around `centre`. Needs a uniform
    disk (limb darkening off): interior radiance from the centre probe; the background radiance is subtracted."""
    cx, cy = centre
    radius = max(half_axes)
    half = int(math.ceil(radius + margin))
    x0, x1 = int(math.floor(cx)) - half, int(math.floor(cx)) + half + 1
    y0, y1 = int(math.floor(cy)) - half, int(math.floor(cy)) + half + 1
    bg = luminance(background)
    inner = luminance(session.probe(round(cx), round(cy))) - bg
    total = session.region(x0, y0, x1, y1)["lsum"] - bg * (x1 - x0) * (y1 - y0)
    bx, by = int(math.floor(cx)), int(math.floor(cy))
    left = session.region(x0, y0, bx + 1, y1)["lsum"] - bg * (bx + 1 - x0) * (y1 - y0)
    right = total - left
    top = session.region(x0, y0, x1, by + 1)["lsum"] - bg * (x1 - x0) * (by + 1 - y0)
    bottom = total - top
    area = total / max(inner, 1e-12)
    measured_radius = math.sqrt(max(area, 0.0) / math.pi)
    # A uniform disk of radius r centred delta to the right of a vertical line through its middle has
    # (right - left) = 4 r delta x inner radiance; area = pi r^2.
    mx = bx + 0.5 + (right - left) / max(4.0 * measured_radius * inner, 1e-12)
    my = by + 0.5 + (bottom - top) / max(4.0 * measured_radius * inner, 1e-12)
    return {"centre": (mx, my), "diameter": 2.0 * measured_radius, "inner": inner}


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_baseline(baseline):
    print("[Sun off, HDR source: byte-identical to the baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    print(f"  INFO baseline {baseline.name}")
    disk_flags = ["--sun-disk", "on", "--sun-disk-scale", "3", "--sun-disk-limb-darkening", "off",
                  "--lens-flare", "off", "--lens-flare-intensity", "4"]
    for name, flags in (("default", []), ("disk_settings", disk_flags)):
        now, _ = render(None, f"base_now_hellocube_{name}", flags)
        before, _ = render(None, f"base_ref_hellocube_{name}", [], exe=baseline)
        check(same(now, before), f"HelloCube ({name}): byte-identical to {baseline.stem}")
    for name in ("exterior_fuselage_nose", "cockpit_main_displays"):
        try:
            entry = next(v for v in views() if v["name"] == name)
        except StopIteration:
            print(f"  SKIP reference view {name} not found")
            continue
        now = render_view(EXE, entry, f"base_now_{name}", disk_flags, folder=OUT)
        before = render_view(baseline, entry, f"base_ref_{name}", folder=OUT)
        if now is None and before is None:
            print(f"  SKIP {name}: E195 asset not available")
            continue
        check(same(now, before), f"E195 {name} (ASOBO), disk/lens settings with the sun off: byte-identical to {baseline.stem}")
    # Sun on (the baseline has the A3.1 sun), the disk switched off: the background is the baseline's.
    model = write("lambert", lambert())
    sun = ["--sun", "on", "--sun-dir", "196,6", "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0",
           "--camera-distance", "4", "--auto-exposure", "off"]
    now, _ = render(model, "base_now_sun_disk_off", sun + ["--sun-disk", "off"])
    # Baselines from 0ddd430 on draw the disk too: pass the same switch to both binaries.
    before, _ = render(model, "base_ref_sun_disk_off", sun + ["--sun-disk", "off"], exe=baseline)
    check(same(now, before), f"sun on, --sun-disk off: byte-identical to {baseline.stem}")
    on, report = render(model, "sun_disk_on_default", sun)
    visible = report is not None and report["sun"]["disk"]["visible"]
    check(visible and not same(on, before), "sun on, disk on by default: the background shows the disk (the image changes)")


def check_hdr_disk():
    print("[HDR source: disk position, size and radiance]")
    model = empty_model()
    flags = black_env() + ["--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0", "--sun", "on",
                           "--sun-intensity", "8", "--sun-angular-radius", "1", "--sun-disk-limb-darkening", "off"]
    session = Session(model, flags)
    try:
        for label, sun_angles, scale, view_offset in (("az 30 el 20", (30.0, 20.0), 1.0, (-3.0, -1.5)),
                                                       ("az 200 el 55", (200.0, 55.0), 1.0, (2.0, 2.5)),
                                                       ("az 30 el 20, scale 2", (30.0, 20.0), 2.0, (-3.0, 1.0))):
            session.env("sun_dir", list(sun_angles))
            session.env("sun_disk_scale", scale)
            sun = direction(*sun_angles)
            camera = session.look(offset_direction(sun, *view_offset), 20.0)
            session.settle(4)
            state = session.call("get_state")["sun"]
            disk = state["disk"]
            radius = math.radians(1.0 * scale)
            centre, halves = disk_axes(camera, sun, radius)
            m = measure_disk(session, centre, halves)
            error = math.hypot(m["centre"][0] - centre[0], m["centre"][1] - centre[1])
            check(error <= CENTRE_TOLERANCE_PX,
                  f"{label}: disk centre ({m['centre'][0]:.2f}, {m['centre'][1]:.2f}) px vs projected sun "
                  f"({centre[0]:.2f}, {centre[1]:.2f}): {error:.2f} px")
            expected_d = 2.0 * math.sqrt(halves[0] * halves[1])
            check(abs(m["diameter"] - expected_d) <= DIAMETER_TOLERANCE_PX,
                  f"{label}: diameter {m['diameter']:.2f} px vs 2R projected {expected_d:.2f} px (R {math.degrees(radius):g} deg)")
            expected = [e / (math.pi * math.sin(radius) ** 2) for e in state["illuminance"]]
            got = session.probe(round(centre[0]), round(centre[1]))
            worst = max(rel(g, e) for g, e in zip(got, expected))
            check(worst <= RADIANCE_TOLERANCE and disk["rangeScale"] == 1.0,
                  f"{label}: interior radiance {[round(g, 1) for g in got]} vs E / (pi sin^2 R) "
                  f"{[round(e, 1) for e in expected]} ({worst * 100:.2f}%)")
        errors = session.close()
        check(errors == 0, f"HDR disk session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"HDR disk session failed: {error}")
        session.process.kill()


def check_limb():
    print("[Limb darkening]")
    model = empty_model()
    flags = black_env() + ["--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0", "--sun", "on",
                           "--sun-intensity", "8", "--sun-angular-radius", "1", "--sun-dir", "60,30"]
    session = Session(model, flags)
    try:
        sun = direction(60.0, 30.0)
        camera = session.look(offset_direction(sun, 1.0, 0.5), 20.0)
        session.settle(4)
        state = session.call("get_state")["sun"]
        disk = state["disk"]
        radius = math.radians(1.0)
        centre, halves = disk_axes(camera, sun, radius)
        r_px = halves[0]
        mean = [e / (math.pi * math.sin(radius) ** 2) for e in state["illuminance"]]

        def law(px, py):
            rho = min(math.hypot(px - centre[0], py - centre[1]) / r_px, 1.0)
            mu = math.sqrt(max(1.0 - rho * rho, 0.0))
            return rho, [mu ** a for a in LIMB_ALPHA]

        cx, cy = round(centre[0]), round(centre[1])
        c = session.probe(cx, cy)
        _, f0 = law(cx, cy)
        expected = [m * (a + 2.0) / 2.0 * f for m, a, f in zip(mean, LIMB_ALPHA, f0)]
        worst = max(rel(g, e) for g, e in zip(c, expected))
        check(worst <= RADIANCE_TOLERANCE,
              f"centre radiance {[round(v, 1) for v in c]} vs mean x (alpha + 2) / 2 x mu^alpha "
              f"{[round(v, 1) for v in expected]} ({worst * 100:.2f}%); reported centre {[round(v, 1) for v in disk['centreRadiance']]}")
        profile_ok = True
        for target_rho in (0.5, 0.8):
            px = round(centre[0] + target_rho * r_px)
            rho, f = law(px, cy)
            got = session.probe(px, cy)
            ratios = [g / max(cc, 1e-9) / (ff / max(f00, 1e-9)) for g, cc, ff, f00 in zip(got, c, f, f0)]
            ok = all(abs(r - 1.0) <= LIMB_TOLERANCE for r in ratios)
            profile_ok = profile_ok and ok
            check(ok, f"rho {rho:.3f}: radiance / centre {[round(g / cc, 3) for g, cc in zip(got, c)]} vs mu^alpha "
                      f"{[round(ff / f00, 3) for ff, f00 in zip(f, f0)]}")
        edge = session.probe(round(centre[0] + 0.85 * r_px), cy)
        check(all(e < cc for e, cc in zip(edge, c)) and edge[2] / edge[0] < c[2] / c[0],
              f"the centre is brighter than the edge in every channel and the edge redder "
              f"(blue/red {c[2] / c[0]:.3f} centre, {edge[2] / edge[0]:.3f} edge)")
        errors = session.close()
        check(errors == 0, f"limb session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"limb session failed: {error}")
        session.process.kill()


def check_sky_disk():
    print("[Procedural sky: disk at the A6.1 sun direction]")
    model = empty_model()
    flags = ["--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0", "--environment", "sky",
             "--time-of-day-mode", "on", "--time-preset", "afternoon", "--sun-disk-limb-darkening", "off"]
    session = Session(model, flags)
    try:
        state = session.call("get_state")
        sun = state["sun"]["direction"]
        camera = session.look(offset_direction(sun, 1.2, -0.6), 8.0)
        session.settle(6)
        state = session.call("get_state")["sun"]
        disk = state["disk"]
        radius = math.radians(disk["angularRadius"])
        centre, halves = disk_axes(camera, sun, radius)
        # Sky radiance next to the disk (four probes 1.6 R away), subtracted from the sums.
        r_px = max(halves)
        ring = [session.probe(round(centre[0] + dx * 1.6 * r_px), round(centre[1] + dy * 1.6 * r_px))
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))]
        background = [sum(p[i] for p in ring) / 4.0 for i in range(3)]
        m = measure_disk(session, centre, halves, background)
        error = math.hypot(m["centre"][0] - centre[0], m["centre"][1] - centre[1])
        el = math.degrees(math.asin(sun[1]))
        check(error <= CENTRE_TOLERANCE_PX,
              f"A6.1 afternoon sun (elevation {el:.2f} deg, azimuth {math.degrees(math.atan2(sun[0], sun[2])):.2f} deg): "
              f"disk centre {error:.2f} px from the projected clock sun direction")
        expected_d = 2.0 * math.sqrt(halves[0] * halves[1])
        check(abs(m["diameter"] - expected_d) <= DIAMETER_TOLERANCE_PX,
              f"diameter {m['diameter']:.2f} px vs 2R projected {expected_d:.2f} px (R {disk['angularRadius']:.3f} deg)")
        got = session.probe(round(centre[0]), round(centre[1]))
        drawn = [m_ * disk["rangeScale"] for m_ in disk["meanRadiance"]]
        formula = [e / (math.pi * math.sin(radius) ** 2) for e in state["illuminance"]]
        worst_formula = max(rel(a, b) for a, b in zip(disk["meanRadiance"], formula))
        worst = max(rel(g - b, d) for g, b, d in zip(got, background, drawn))
        check(worst <= RADIANCE_TOLERANCE and worst_formula <= 1e-4,
              f"interior radiance {[round(g, 1) for g in got]} = reported mean x range scale {disk['rangeScale']:.4f} "
              f"({worst * 100:.2f}%); mean = E / (pi sin^2 R) with E = the sun illuminance {[round(e, 3) for e in state['illuminance']]}")
        sun_colour = [e / max(state["illuminance"]) for e in state["illuminance"]]
        disk_colour = [(g - b) / max(gg - bb for gg, bb in zip(got, background)) for g, b in zip(got, background)]
        check(max(abs(a - b) for a, b in zip(sun_colour, disk_colour)) <= 0.01 and sun_colour[2] < sun_colour[0],
              f"disk colour {[round(c, 3) for c in disk_colour]} = the transmittance colour of the sun "
              f"{[round(c, 3) for c in sun_colour]}")
        errors = session.close()
        check(errors == 0, f"sky disk session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"sky disk session failed: {error}")
        session.process.kill()


def check_ibl():
    print("[Not in the IBL input: Lambert sphere with the disk on and off]")
    import numpy as np
    model = write("lambert", lambert())
    for source, flags in (("HDR", ["--sun", "on", "--sun-dir", "30,20"]),
                          ("sky", ["--environment", "sky", "--sun-dir", "30,20"])):
        session = Session(model, ["--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0"] + flags)
        try:
            sun = direction(30.0, 20.0)
            view = offset_direction(sun, 24.0, -4.0)
            camera = session.place([-4.0 * c for c in view], [0.0, 0.0, 0.0], 60.0)
            if source == "sky":
                session.converge()
            session.settle(4)
            centre, halves = disk_axes(camera, sun, math.radians(0.27))
            before = session.call("get_state")
            png_on = session.settle(2, name=f"ibl_{source}_on")
            w, h = camera.width, camera.height
            sphere_on = session.region(w // 2 - 40, h // 2 - 40, w // 2 + 40, h // 2 + 40)
            session.env("sun_disk", False)
            session.settle(4)
            png_off = session.settle(2, name=f"ibl_{source}_off")
            sphere_off = session.region(w // 2 - 40, h // 2 - 40, w // 2 + 40, h // 2 + 40)
            after = session.call("get_state")
            check(sphere_on["sum"] == sphere_off["sum"] and sphere_on["max"] == sphere_off["max"] and sphere_on["lsum"] > 0,
                  f"{source}: Lambert sphere radiance (80 x 80 px at its centre) identical with the disk on and off "
                  f"(mean luminance {sphere_on['lsum'] / sphere_on['n']:.5f})")
            a, b = load(png_on), load(png_off)
            ys, xs = np.nonzero(np.any(a != b, axis=2))
            inside = bool(len(xs)) and float(np.max(np.hypot(xs - centre[0], ys - centre[1]))) <= max(halves) + 2.0
            check(0 <= centre[0] < w and 0 <= centre[1] < h and inside,
                  f"{source}: {len(xs)} pixels change, all within {max(halves) + 2.0:.1f} px of the projected sun "
                  f"({centre[0]:.1f}, {centre[1]:.1f})")
            if source == "sky":
                sb, sa = before["sky"], after["sky"]
                keys = ("fastJobs", "refineJobs", "synchronousBuilds", "swaps")
                check(all(sb["stats"][k] == sa["stats"][k] for k in keys) and
                      sb["refine"]["frontVersion"] == sa["refine"]["frontVersion"] and sa["refine"]["state"] == "converged",
                      f"sky: switching the disk neither re-filters nor swaps the IBL "
                      f"({', '.join(f'{k} {sa['stats'][k]}' for k in keys)}, front version {sa['refine']['frontVersion']})")
            errors = session.close()
            check(errors == 0, f"{source} IBL session: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"{source} IBL session failed: {error}")
            session.process.kill()


def ray_traced_reflection_area(camera, sun, radius, cx, cy, half):
    """Pixels whose ray hits the unit sphere and reflects to within `radius` of `sun` (4 x 4 supersampled)."""
    count = 0.0
    cos_r = math.cos(radius)
    for py in range(int(cy) - half, int(cy) + half + 1):
        for px in range(int(cx) - half, int(cx) + half + 1):
            for sy in range(4):
                for sx in range(4):
                    fx, fy = px - 0.5 + (sx + 0.5) / 4.0, py - 0.5 + (sy + 0.5) / 4.0
                    p = camera.hit_unit_sphere(fx, fy)
                    if p is None:
                        continue
                    d = camera.ray(fx, fy)
                    dn = sum(d[i] * p[i] for i in range(3))
                    r = [d[i] - 2.0 * dn * p[i] for i in range(3)]
                    if sum(r[i] * sun[i] for i in range(3)) >= cos_r:
                        count += 1.0 / 16.0
    return count


def check_mirror():
    print("[Mirror sphere: the A3.1 highlight shows the disk]")
    model = write("mirror", mirror())
    for radius_deg in (2.0, 4.0):
        session = Session(model, black_env() + ["--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
                                                "--sun", "on", "--sun-dir", "0,0", "--sun-intensity", "2",
                                                "--sun-angular-radius", str(radius_deg)])
        try:
            camera = session.place([0.0, 0.0, 4.0], [0.0, 0.0, 0.0], 20.0)
            session.settle(4)
            state = session.call("get_state")["sun"]
            sun = state["direction"]
            radius = math.radians(radius_deg)
            cx, cy = camera.project([0.0, 0.0, 1.0])
            # Ray-traced reflection of the disk on the sphere: its pixel area.
            disk_area = ray_traced_reflection_area(camera, sun, radius, cx, cy, int(4 * radius_deg + 8))
            disk_d = 2.0 * math.sqrt(disk_area / math.pi)
            half = int(math.ceil(2.5 * disk_d)) + 2
            r = session.region(int(cx) - half, int(cy) - half, int(cx) + half + 1, int(cy) + half + 1)
            peak = r["lmax"]
            equivalent_d = 2.0 * math.sqrt(r["lsum"] / max(peak, 1e-9) / math.pi)
            expected_peak = luminance(state["illuminance"]) / (math.pi * math.tan(radius) ** 2)
            check(abs(equivalent_d / disk_d - 1.0) <= MIRROR_SIZE_TOLERANCE,
                  f"R {radius_deg:g} deg: highlight energy-equivalent diameter {equivalent_d:.2f} px vs the disk's "
                  f"ray-traced reflection {disk_d:.2f} px ({(equivalent_d / disk_d - 1.0) * 100:+.1f}%)")
            check(abs(peak / expected_peak - 1.0) <= MIRROR_PEAK_TOLERANCE,
                  f"R {radius_deg:g} deg: highlight peak {peak:.2f} vs E / (pi tan^2 R) {expected_peak:.2f} "
                  f"({(peak / expected_peak - 1.0) * 100:+.1f}%; the disk's mean radiance is E / (pi sin^2 R) "
                  f"{luminance(state['illuminance']) / (math.pi * math.sin(radius) ** 2):.2f})")
            session.settle(2, name=f"mirror_r{radius_deg:g}")
            errors = session.close()
            check(errors == 0, f"mirror session: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"mirror session failed: {error}")
            session.process.kill()


def check_glare():
    print("[Glare: bloom and the lens effect]")
    import numpy as np
    model = empty_model()
    # Camera at +Z looking at -Z (the sun at az 180 is ahead), black panorama.
    view = black_env() + ["--hide-grid", "--auto-exposure", "off", "--exposure", "0", "--sun", "on", "--sun-intensity", "8",
                          "--sun-dir", "196,6", "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0",
                          "--camera-distance", "4"]
    shots = {}
    for name, flags in (("bloom_off", ["--bloom", "off"]), ("bloom_on", []),
                        ("lens_flag_off", ["--lens-flare", "off"]), ("lens_zero", ["--lens-flare", "on", "--lens-flare-intensity", "0"]),
                        ("lens_on", ["--lens-flare", "on"]), ("lens_streak", ["--lens-flare", "streak"]),
                        ("lens_on_bloom_off", ["--lens-flare", "on", "--bloom", "off"])):
        shots[name] = render(model, f"glare_{name}", view + flags)
    if not all(p for p, _ in shots.values()):
        check(False, "glare renders")
        return
    report = shots["bloom_off"][1]
    cam = report["camera"]
    camera = Camera({"camera": {"eye": cam["eye"], "target": cam["target"], "up": cam["up"], "fovY": cam["fovY"],
                                "viewport": cam["viewport"]}})
    sun = report["sun"]["direction"]
    centre, halves = disk_axes(camera, sun, math.radians(0.27))
    off = load(shots["bloom_off"][0])
    on = load(shots["bloom_on"][0])
    h, w = off.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    dist = np.hypot(xx - centre[0], yy - centre[1])
    outside = dist > max(halves) + 2.0
    check(0 <= centre[0] < w and 0 <= centre[1] < h and off[dist <= max(halves) - 1.0].min() >= 250.0,
          f"the disk is in view at ({centre[0]:.1f}, {centre[1]:.1f}), radius {max(halves):.2f} px, saturated")
    check(off[outside].max() == 0.0, f"bloom off: no light outside the disk edge (max level beyond 2 px: {off[outside].max():.0f})")
    ring = (dist > max(halves) + 4.0) & (dist < max(halves) + 40.0)
    check(on[ring].mean() > 8.0, f"bloom on: the disk glares (mean level {on[ring].mean():.1f} in the ring 4..40 px "
                                 f"beyond the edge, {off[ring].mean():.1f} without bloom)")
    check(same(shots["lens_flag_off"][0], shots["bloom_on"][0]), "--lens-flare off: byte-identical to the effect-free render")
    check(same(shots["lens_zero"][0], shots["bloom_on"][0]), "lens effect at intensity 0: byte-identical to the effect-free render")
    check(same(shots["lens_on_bloom_off"][0], shots["bloom_off"][0]), "bloom off removes the lens effect (byte-identical)")
    lens = load(shots["lens_on"][0])
    streak = load(shots["lens_streak"][0])
    mirrored = (np.hypot(xx - (w - 1 - centre[0]), yy - (h - 1 - centre[1])) < 0.35 * h)
    added = (lens - on)
    check(added.min() >= 0.0 and added[mirrored & outside].sum() > 0.0,
          f"lens on: adds light only (min change {added.min():.0f}), ghosts on the far side of the centre "
          f"(sum {added[mirrored & outside].sum():.0f})")
    row = (np.abs(yy - centre[1]) <= 2) & (np.abs(xx - centre[0]) > 60)
    column = (np.abs(xx - centre[0]) <= 2) & (np.abs(yy - centre[1]) > 60)
    s_added = streak - on
    check(s_added[row].mean() > 2.0 * max(s_added[column].mean(), 0.05),
          f"streak: horizontal (mean change {s_added[row].mean():.2f} along the sun's row, {s_added[column].mean():.2f} along its column)")
    # Below the threshold nothing changes: a Lambert sphere lit by the sun, the disk out of view.
    sphere = write("lambert", lambert())
    calm = black_env() + ["--hide-grid", "--auto-exposure", "off", "--sun", "on", "--sun-dir", "0,30", "--sun-intensity", "2",
                          "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "4"]
    a, _ = render(sphere, "glare_calm_off", calm)
    b, _ = render(sphere, "glare_calm_lens", calm + ["--lens-flare", "on", "--lens-flare-intensity", "10"])
    check(same(a, b), "lens effect on, nothing above its threshold (Lambert sphere, sun out of view): byte-identical")


def check_registry():
    print("[Registry, CLI and pipe]")
    model = empty_model()
    base = black_env() + ["--hide-grid", "--auto-exposure", "off", "--sun", "on", "--sun-dir", "196,6",
                          "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "4"]
    session = Session(model, base)
    try:
        environment = {s["key"] for s in session.call("list_settings", scope="environment")["settings"]}
        viewport = {s["key"] for s in session.call("list_settings", scope="viewport")["settings"]}
        check({"sun_disk", "sun_disk_scale", "sun_disk_limb_darkening"} <= environment and
              {"lens_flare", "lens_flare_intensity"} <= viewport,
              "sun_disk, sun_disk_scale, sun_disk_limb_darkening (environment), lens_flare, lens_flare_intensity (viewport)")
        refused = []
        for scope, key, value in (("set_environment", "sun_disk_scale", 50), ("set_environment", "sun_disk", "maybe"),
                                  ("set_viewport_setting", "lens_flare", "foo"), ("set_viewport_setting", "lens_flare_intensity", -1)):
            try:
                session.call(scope, key=key, value=value)
                refused.append(False)
            except glider_ctl.GliderError:
                refused.append(True)
        check(all(refused), f"invalid sun_disk_scale / sun_disk / lens_flare / lens_flare_intensity values are refused {refused}")
        session.env("sun_disk_scale", 3.0)
        session.env("sun_disk_limb_darkening", False)
        session.call("set_viewport_setting", key="lens_flare", value="streak")
        session.call("set_viewport_setting", key="lens_flare_intensity", value=2.0)
        session.call("capture", path=str(OUT / "pipe_disk.png"), settle_frames=24, overlays=False)
        errors = session.close()
        check(errors == 0, f"registry session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"registry session failed: {error}")
        session.process.kill()
        return
    cli, _ = render(model, "cli_disk", base + ["--sun-disk-scale", "3", "--sun-disk-limb-darkening", "off",
                                               "--lens-flare", "streak", "--lens-flare-intensity", "2"])
    check(same(cli, OUT / "pipe_disk.png"), "pipe settings = CLI flags (byte-identical capture)")
    for flags in (["--sun-disk-scale", "50"], ["--lens-flare", "foo"], ["--sun-disk", "maybe"]):
        bad = run([EXE, model, "--isolated"] + flags, timeout=120)
        check(bad.returncode != 0, f"{' '.join(flags)} is refused (exit {bad.returncode})")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=None)
    parser.add_argument("--only", default=None, help="comma-separated: baseline,hdr,limb,sky,ibl,mirror,glare,registry")
    args = parser.parse_args()
    baseline = Path(args.baseline_exe) if args.baseline_exe else None
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None
    OUT.mkdir(parents=True, exist_ok=True)
    only = set(args.only.split(",")) if args.only else None
    steps = [("baseline", lambda: check_baseline(baseline)), ("hdr", check_hdr_disk), ("limb", check_limb),
             ("sky", check_sky_disk), ("ibl", check_ibl), ("mirror", check_mirror), ("glare", check_glare),
             ("registry", check_registry)]
    for name, step in steps:
        if only is None or name in only:
            step()
    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in {len(gl_logs)} renders {bad}")
    return finish()


if __name__ == "__main__":
    sys.exit(main())
