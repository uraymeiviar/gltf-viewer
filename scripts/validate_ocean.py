# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.8a: the FFT ocean ground (docs/ASOBO_EXTENSIONS_PHASE_6.md, A6.8).

A viewport's ground can be the animated ocean (--ground ocean, registry ground = ocean): a looping precomputed FFT
simulation (OceanSimulation) drawn as a camera-centred polar grid (ocean.vert / ocean.frag) at the A3.6 height.
Checks:
  * Grid (default) and Plane: byte-identical to the baseline binary (--baseline-exe) on HelloCube and the E195
    exterior reference view (the plane with the sun on);
  * loop seam: captures at ocean time t and t + T (T = the 30 s period) are byte-identical (t = 0 and t = 7.3);
  * deterministic captures: the same flags twice with the ocean clock running (--fixed-frame-time) are identical, and
    two different times differ (the surface moves);
  * CLI = pipe: --ocean-wind-speed, --ocean-wind-dir, --ocean-quality, --ocean-time and --ground ocean give the same
    capture as the registry keys set over the control pipe; out-of-range and unknown values are refused (CLI exit
    code and pipe error);
  * GL < 4.3: forced through the GLIDER_TEST_NO_COMPUTE=1 test hook (a 4.1 context cannot be forced here):
    --ground ocean logs one warning and renders exactly the grid;
  * depth: a red box straddling the sea level (calm sea): probes on the box above the waves show the box, probes on
    the box below the waves show the water (the ocean writes its depth); the pick returns the box in both cases and
    nothing on open water (the ocean is not pickable);
  * model interior: a camera inside a closed box (6 walls) whose middle is at the sea level sees exactly what it sees
    with the grid ground (no water inside), with the HDR (bounds rule) and with the procedural sky (A3.9 volume rule);
  * GPU cost: the Opaque pass with the ocean against the plane, per quality (median over 24 frames with the ocean
    animating), and the precompute time per quality (synchronous set) and the background recompute (slices against
    the 3 ms budget);
  * zero GL errors.

Run: python scripts/validate_ocean.py [--baseline-exe build/baselines/<sha>.exe] [--only a,b]
"""

import argparse
import json
import math
import os
import statistics
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
import shadow_fixtures  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same, view, render_view  # noqa: E402
from validate_sun import Camera, gl_errors_zero  # noqa: E402

OUT = REPO / "build" / "ocean"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
OCEAN = ["--ground", "ocean", "--hide-grid"]
PERIOD = 30.0
SLICE_BUDGET_MS = 3.0

gl_logs = []


def run(args, timeout=900, env=None):
    kwargs = {"cwd": REPO, "capture_output": True, "text": True, "timeout": timeout}
    if env is not None:
        kwargs["env"] = env
    return subprocess.run([str(a) for a in args], **kwargs)


def render(model, name, flags=(), exe=EXE, frame=24, common=COMMON, env=None):
    OUT.mkdir(parents=True, exist_ok=True)
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    args = [exe] + ([model] if model else []) + list(common) + list(flags)
    result = run(args + ["--capture-viewport", png, "--capture-frame", str(frame), "--profile-report", report,
                         "--exit-after-profile"], env=env)
    log = result.stdout + result.stderr
    (OUT / f"{name}.log").write_text(log, encoding="utf-8")
    if Path(exe) == EXE:
        gl_logs.append((name, log))
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    return (png if png.exists() else None), data, log


class Session:
    def __init__(self, model, flags=(), common=COMMON):
        args = [str(EXE)] + ([str(model)] if model else []) + list(common) + list(flags) + ["--control-pipe"]
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

    def set(self, key, value):
        return self.call("set_viewport_setting", key=key, value=value)

    def settle(self, frames=6, name="_settle"):
        path = OUT / f"{name}.png"
        self.call("capture", path=str(path), settle_frames=frames, overlays=False)
        return path

    def camera(self):
        cam = self.call("get_camera")
        vp = self.call("get_state")["viewports"][0]
        return Camera({"camera": {"eye": cam["eye"], "target": cam["target"], "up": cam["up"], "fovY": cam["fovY"],
                                  "viewport": [vp["width"], vp["height"]]}})

    def ocean(self):
        return self.call("get_state")["ground"]["ocean"]

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


# ---------------------------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------------------------

def hull_box(path):
    """A 2 x 2 x 2 m red box centred on y = 0 (the sea level of the depth check)."""
    b = shadow_fixtures.Builder("A6.8 ocean hull fixture")
    b.box("Hull", (-1.0, -1.0, -1.0), (1.0, 1.0, 1.0), b.material("Red", color=(0.8, 0.05, 0.03, 1.0)))
    return b.write(path)


def closed_box(path):
    """A closed 4 x 2 x 4 m room of 10 cm walls from y = -1 to y = 1 (floor, four walls, roof)."""
    b = shadow_fixtures.Builder("A6.8 ocean closed box")
    m = b.material("Wall", color=(0.7, 0.7, 0.7, 1.0))
    w, h, t = 2.0, 1.0, 0.1
    b.box("Floor", (-w - t, -h - t, -w - t), (w + t, -h, w + t), m)
    b.box("Roof", (-w - t, h, -w - t), (w + t, h + t, w + t), m)
    b.box("Back", (-w - t, -h, -w - t), (w + t, h, -w), m)
    b.box("Front", (-w - t, -h, w), (w + t, h, w + t), m)
    b.box("Left", (-w - t, -h, -w), (-w, h, w), m)
    b.box("Right", (w, -h, -w), (w + t, h, w), m)
    return b.write(path)


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_baseline(baseline):
    print("[Grid and Plane against the baseline]")
    if baseline is None or not baseline.exists():
        check(False, f"baseline binary present ({baseline})")
        return
    for name, flags in (("hellocube_grid", []), ("hellocube_plane", ["--ground", "plane"]),
                        ("hellocube_plane_sun", ["--ground", "plane", "--sun", "on", "--sun-dir", "40,35"])):
        a, _, _ = render(None, f"base_{name}", flags, exe=baseline)
        b, _, _ = render(None, f"cur_{name}", flags)
        check(same(a, b), f"HelloCube {' '.join(flags) or '(default grid)'}: byte-identical to {baseline.stem}")
    entry = view("exterior_fuselage_nose")
    for name, flags in (("grid", []), ("plane_sun", ["--ground", "plane", "--sun", "on", "--sun-dir", "40,35"])):
        a = render_view(baseline, entry, f"base_e195_{name}", flags, folder=OUT)
        b = render_view(EXE, entry, f"cur_e195_{name}", flags, folder=OUT)
        check(a is not None and same(a, b), f"E195 exterior_fuselage_nose {' '.join(flags) or '(default)'}: "
                                            f"byte-identical to {baseline.stem}")


def check_loop_and_determinism():
    print("[Loop seam and deterministic captures]")
    a, _, _ = render(None, "seam_0", OCEAN + ["--ocean-time", "0"])
    b, _, _ = render(None, "seam_0_plus_period", OCEAN + ["--ocean-time", f"{PERIOD:g}"])
    check(a is not None and same(a, b), f"ocean time 0 s and {PERIOD:g} s (one period later): byte-identical")
    # 7.3 and 37.3 are not exactly 30 s apart as floats (4e-6 s, so rounding noise of a level or two at wave edges: with
    # A6.8a one level in 1366 pixels, with A6.8b two levels in one pixel of 450,000): the images are compared at 7.25 s
    # and 37.25 s (exact in binary floating point, so the times are exactly one period apart; same threshold, one level)
    # and the frame positions at 7.3 s and 37.3 s agree within 1e-4 frames (below).
    a, _, _ = render(None, "seam_7.25", OCEAN + ["--ocean-time", "7.25"])
    b, _, _ = render(None, "seam_37.25", OCEAN + ["--ocean-time", "37.25"])
    if a is not None and b is not None:
        import numpy as np
        from validate_sun import load
        diff = np.abs(load(a) - load(b)).max()
        check(diff <= 1.0, f"ocean time 7.25 s and 37.25 s: images differ by at most {diff:.0f} level (<= 1)")
    else:
        check(False, "seam captures at 7.25 s and 37.25 s")
    try:
        session = Session(None, OCEAN + ["--ocean-time", "7.3"])
        session.settle(4, "seam_settle")
        first = [c["framePosition"] for c in session.ocean()["cascadeInfo"]]
        session.set("ocean_time", 37.3)
        session.settle(4, "seam_settle")
        second = [c["framePosition"] for c in session.ocean()["cascadeInfo"]]
        session.close()
        worst = max(abs(x - y) for x, y in zip(first, second))
        check(worst <= 1e-4, f"frame positions at 7.3 s and 37.3 s agree within {worst:.2e} frames (<= 1e-4)")
    except Exception as error:  # noqa: BLE001
        check(False, f"seam session failed: {error}")
    c, _, _ = render(None, "seam_15", OCEAN + ["--ocean-time", "15"])
    check(c is not None and not same(c, OUT / "seam_0.png"), "ocean time 15 s differs from 0 s (the surface moves)")
    # The clock runs with --fixed-frame-time: two runs with the same flags end in the same image.
    a, _, _ = render(None, "clock_run1", OCEAN, frame=32)
    b, _, _ = render(None, "clock_run2", OCEAN, frame=32)
    check(a is not None and same(a, b), "the same flags twice (ocean clock running, 32 frames): identical captures")
    d, _, _ = render(None, "clock_run_frame40", OCEAN, frame=40)
    check(d is not None and not same(a, d), "frame 40 differs from frame 32 (the clock moves the ocean)")


def check_registry():
    print("[Registry, CLI and pipe]")
    flags = ["--ocean-wind-speed", "14", "--ocean-wind-dir", "120", "--ocean-quality", "low", "--ocean-time", "3.5"]
    cli, _, _ = render(None, "cli_ocean", OCEAN + flags)
    try:
        session = Session(None, ["--hide-grid"])
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        keys = {s["key"] for s in session.call("list_settings", scope="viewport")["settings"]}
        check({"ocean_wind_speed", "ocean_wind_dir", "ocean_quality", "ocean_time"} <= keys,
              "registry keys ocean_wind_speed, ocean_wind_dir, ocean_quality, ocean_time")
        refused = []
        for key, value in (("ground", "lake"), ("ocean_wind_speed", 100), ("ocean_wind_speed", 0.1),
                           ("ocean_wind_dir", -5), ("ocean_quality", "ultra"), ("ocean_time", -3)):
            try:
                session.set(key, value)
                refused.append(False)
            except glider_ctl.GliderError:
                refused.append(True)
        check(all(refused), f"pipe refuses ground lake, wind speed 100 / 0.1, wind dir -5, quality ultra, time -3 {refused}")
        session.set("ground", "ocean")
        session.set("ocean_wind_speed", 14)
        session.set("ocean_wind_dir", 120)
        session.set("ocean_quality", "low")
        session.set("ocean_time", 3.5)
        session.call("capture", path=str(OUT / "pipe_ocean.png"), settle_frames=24, overlays=False)
        state = session.ocean()
        check(state["active"] and state["ready"] and state["quality"] == "low" and state["cascades"] == 2,
              f"get_state ground.ocean: active, ready, low quality, 2 cascades ({state['active']}, {state['ready']}, "
              f"{state['quality']}, {state['cascades']})")
        errors = session.close()
        check(errors == 0, f"registry session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"registry session failed: {error}")
        session.process.kill()
        return
    check(cli is not None and same(cli, OUT / "pipe_ocean.png"), "pipe settings = CLI flags (byte-identical capture)")
    for bad in (["--ground", "lake"], ["--ocean-wind-speed", "100"], ["--ocean-wind-dir", "400"],
                ["--ocean-quality", "ultra"], ["--ocean-time", "-2"]):
        result = run([EXE, "--isolated"] + bad, timeout=120)
        check(result.returncode != 0, f"{' '.join(bad)} is refused (exit {result.returncode})")


def check_no_compute():
    print("[GL < 4.3 (test hook GLIDER_TEST_NO_COMPUTE=1)]")
    env = dict(os.environ)
    env["GLIDER_TEST_NO_COMPUTE"] = "1"
    grid, _, _ = render(None, "nocompute_grid", ["--ground", "grid"], env=env)
    ocean, _, log = render(None, "nocompute_ocean", ["--ground", "ocean"], env=env)
    warnings = [line for line in log.splitlines() if "Ocean ground needs an OpenGL 4.3" in line]
    check(len(warnings) == 1, f"one warning logged ({len(warnings)})")
    check(grid is not None and same(grid, ocean), "--ground ocean renders exactly the grid")


def check_depth_and_pick():
    print("[Depth against a hull, not pickable]")
    model = hull_box(OUT / "hull_box.gltf")
    flags = OCEAN + ["--ground-height", "0", "--ocean-wind-speed", "3", "--ocean-time", "2", "--sun", "on",
                     "--sun-dir", "200,40", "--camera-target", "0,0,0", "--camera-yaw", "20", "--camera-pitch", "4",
                     "--camera-distance", "7"]
    try:
        session = Session(model, flags)
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        session.settle(24, "depth_settle")
        cam = session.camera()
        hs = session.ocean().get("significantWaveHeight", 0.0)
        offset = min(max(0.45, 2.0 * hs), 0.8)
        # Points on the box face towards the camera (the face normal closest to the view direction).
        face = max(((1, 0, 0), (-1, 0, 0), (0, 0, 1), (0, 0, -1)),
                   key=lambda n: -sum(n[i] * cam.f[i] for i in range(3)))
        def on_face(y):
            u = (face[2], 0.0, -face[0])
            return [face[0] * 1.0 + u[0] * 0.2, y, face[2] * 1.0 + u[2] * 0.2]
        above, below = on_face(offset), on_face(-offset)
        results = {}
        for label, point in (("above", above), ("below", below)):
            x, y = cam.project(point)
            results[label] = session.call("probe", x=int(round(x)), y=int(round(y)))
        r_above, r_below = results["above"]["hdr"], results["below"]["hdr"]
        pick_above = json.dumps(results["above"]["pick"])
        pick_below = json.dumps(results["below"]["pick"])
        check(r_above[0] > 2.0 * r_above[2], f"box {offset:.2f} m above the sea level shows the box (red {r_above[:3]})")
        check(r_below[2] > r_below[0], f"box {offset:.2f} m below the sea level shows the water (blue {r_below[:3]}; "
                                       f"Hs {hs:.3f} m)")
        check("Hull" in pick_above and "Hull" in pick_below,
              "the pick returns the box above and below the water (the ocean is not pickable)")
        # Open water beside the box.
        x, y = cam.project([face[0] * 1.0 + (face[2] if face[2] else 0.0) * 4.0, 0.0,
                            face[2] * 1.0 + (-face[0] if face[0] else 0.0) * 4.0])
        water = session.call("probe", x=int(round(x)), y=int(round(y)))
        check(water["pick"] is None, f"a click on open water picks nothing ({water['pick']})")
        errors = session.close()
        check(errors == 0, f"depth session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"depth session failed: {error}")
        session.process.kill()


def check_interior():
    print("[Model interior: camera inside a closed box]")
    model = closed_box(OUT / "closed_box.gltf")
    view_flags = ["--ground-height", "0", "--camera-target", "0,-0.2,0", "--camera-yaw", "30", "--camera-pitch", "35",
                  "--camera-distance", "1.0"]
    for label, extra in (("hdr", []), ("sky", ["--environment", "sky", "--sky-ground-albedo", "0.3"])):
        grid, _, _ = render(model, f"interior_{label}_grid", ["--ground", "grid", "--hide-grid"] + view_flags + extra)
        ocean, report, _ = render(model, f"interior_{label}_ocean", OCEAN + view_flags + extra)
        if label == "sky":
            active = bool(report and report.get("skyOcclusion", {}).get("active"))
            check(active, "the procedural sky's sky-visibility volume is active (the volume rule applies)")
        rule = "bounds rule" if label == "hdr" else "sky-visibility rule"
        check(grid is not None and same(grid, ocean), f"{label}: inside the box with the ocean = with the grid ({rule}; no water)")
    # The same box from outside: the water does meet the walls at the sea level.
    outside, _, _ = render(model, "interior_outside", OCEAN + ["--ground-height", "0", "--camera-target", "0,0,0",
                                                               "--camera-yaw", "30", "--camera-pitch", "15",
                                                               "--camera-distance", "12"])
    grid_outside, _, _ = render(model, "interior_outside_grid", ["--ground", "grid", "--hide-grid", "--ground-height", "0",
                                                                 "--camera-target", "0,0,0", "--camera-yaw", "30",
                                                                 "--camera-pitch", "15", "--camera-distance", "12"])
    check(outside is not None and not same(outside, grid_outside), "outside the box the ocean is drawn (sanity)")


def gpu_opaque_ms(report, first=24):
    values = []
    for frame in (report or {}).get("gpu", {}).get("completed", []):
        if frame.get("frame", 0) < first:
            continue
        for p in frame.get("passes", []):
            if p.get("name") == "Opaque" and p.get("available"):
                values.append(p["ms"])
    return statistics.median(values) if values else None


def check_cost():
    print("[GPU cost and precompute]")
    # HelloCube seen from above: the ocean fills the frame (1280 x 720 window). The ocean animates every frame.
    timing = ["--sun", "on", "--sun-dir", "40,35", "--disable-redraw-skip", "--camera-pitch", "35"]
    _, report, _ = render(None, "cost_plane", ["--ground", "plane", "--hide-grid"] + timing, frame=48)
    plane = gpu_opaque_ms(report)
    numbers = {}
    for quality in ("low", "medium", "high"):
        _, report, _ = render(None, f"cost_ocean_{quality}", OCEAN + ["--ocean-quality", quality] + timing, frame=48)
        numbers[quality] = gpu_opaque_ms(report)
    if plane is None:
        check(False, "plane timing available")
        return
    for quality in ("low", "medium", "high"):
        ms = numbers[quality]
        if ms is None:
            check(False, f"{quality}: ocean timing available")
            continue
        print(f"  INFO {quality}: Opaque pass {ms:.3f} ms with the ocean, {plane:.3f} ms with the plane "
              f"(ocean draw ~{ms - plane:.3f} ms, median of frames 24-47)")
        check(ms - plane < 4.0, f"{quality}: the ocean adds {ms - plane:.3f} ms to the Opaque pass (< 4 ms)")
    # Precompute: synchronous sets (capture runs) per quality, and a background recompute in an interactive session.
    try:
        session = Session(None, OCEAN)
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        for quality in ("low", "medium", "high"):
            session.set("ocean_quality", quality)
            session.settle(2, "cost_settle")
            state = session.ocean()
            pre = state.get("precompute", {})
            print(f"  INFO {quality}: precompute {pre.get('wallMs', 0):.1f} ms wall, {pre.get('gpuMs', 0):.1f} ms GPU "
                  f"(synchronous), memory {state.get('memoryBytes', 0) / 1048576:.1f} MiB")
            check(state["ready"] and pre.get("appFrames") == 1, f"{quality}: synchronous set ready in one frame")
        errors = session.close()
        check(errors == 0, f"precompute session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"precompute session failed: {error}")
        session.process.kill()
        return
    # Background: no capture-run flags, so a change is spread over frames.
    interactive = ["--isolated", "--window-size", "1280x720", "--vsync", "off", "--aa", "none"]
    try:
        session = Session(None, OCEAN, common=interactive)
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        session.settle(4, "bg_settle")
        before = session.ocean()["version"]
        session.set("ocean_wind_speed", 15)
        deadline = time.time() + 60
        while time.time() < deadline:
            session.settle(1, "bg_step")
            state = session.ocean()
            if state["ready"] and state["version"] != before:
                break
        session.settle(4, "bg_step")  # the last slices' timestamps
        state = session.ocean()
        pre = state["precompute"]
        measured = pre.get("slicesGpuMs", [])
        median = statistics.median(measured) if measured else 0.0
        print(f"  INFO background recompute (medium): {pre['appFrames']} frames, {pre['gpuMs']:.1f} ms GPU in total, "
              f"{len(measured)} measured slices, median {median:.2f} ms, largest {pre['maxSliceGpuMs']:.2f} ms: "
              f"{[round(v, 2) for v in measured]}")
        check(state["ready"] and state["version"] != before and pre["appFrames"] > 1,
              f"a wind change recomputes in the background over {pre['appFrames']} frames and swaps")
        check(measured and median <= SLICE_BUDGET_MS and pre["maxSliceGpuMs"] <= 1.5 * SLICE_BUDGET_MS,
              f"background slices: median {median:.2f} ms <= {SLICE_BUDGET_MS} ms budget, largest "
              f"{pre['maxSliceGpuMs']:.2f} ms <= 1.5 x budget")
        errors = session.close()
        check(errors == 0, f"background session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"background session failed: {error}")
        session.process.kill()


# ---------------------------------------------------------------------------------------------
# A6.8b checks
# ---------------------------------------------------------------------------------------------

UNIFORM_LEVEL = 0.5
ENERGY_TOLERANCE = 0.25      # relative, per channel (the split-sum LUT and the wave slopes move F by a few percent)
SHADOW_RATIO_RANGE = (1.4, 3.5)
FOAM_NEAR = ["--camera-pitch", "60", "--camera-distance", "30"]
FOAM_WIDE = ["--camera-pitch", "60", "--camera-distance", "120"]
STATE_KEYS = ("available", "active", "mode", "quality", "windSpeed", "windDirection", "fetch", "choppiness", "swell", "foam",
              "color", "play", "time", "timePinned", "period", "cascades", "resolution", "frames", "ready", "busy",
              "progress", "memoryBytes", "version", "significantWaveHeight", "cascadeInfo", "precompute", "live",
              "foamParameters", "reflections")


def uniform_environment():
    from validate_asobo_sss import write_hdr
    return write_hdr(OUT / "uniform_env.hdr", 64, 32, lambda x, y: (UNIFORM_LEVEL,) * 3)


NEUTRAL = ["--clear-color", "0,0,0", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0", "--tone-mapping", "neutral"]


def check_registry_b():
    print("[A6.8b registry: all keys and flags, CLI = pipe, refused values, state and profile report]")
    flags = ["--ocean-wind-speed", "14", "--ocean-wind-dir", "120", "--ocean-quality", "low", "--ocean-time", "3.5",
             "--ocean-choppiness", "1.3", "--ocean-swell", "0.6", "--ocean-foam", "0.8", "--ocean-color", "0.01,0.03,0.04",
             "--ocean-period", "20", "--ocean-fetch", "50000"]
    cli, report, _ = render(None, "cli_ocean_b", OCEAN + flags)
    ocean = (report or {}).get("ground", {}).get("ocean", {})
    missing = [key for key in STATE_KEYS if key not in ocean]
    check(not missing, f"--profile-report ground.ocean carries every field (missing: {missing})")
    check(len(ocean.get("cascadeInfo", [])) == 2 and ocean.get("period") == 20 and ocean.get("mode") == "loop"
          and abs(ocean.get("choppiness", 0) - 1.3) < 1e-6,
          f"profile report ground.ocean reflects the flags (mode {ocean.get('mode')}, period {ocean.get('period')}, "
          f"choppiness {ocean.get('choppiness')}, {len(ocean.get('cascadeInfo', []))} cascades)")
    try:
        session = Session(None, ["--hide-grid"])
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        keys = {s["key"] for s in session.call("list_settings", scope="viewport")["settings"]}
        wanted = {"ocean_wind_speed", "ocean_wind_dir", "ocean_quality", "ocean_time", "ocean_choppiness", "ocean_swell",
                  "ocean_foam", "ocean_color", "ocean_period", "ocean_mode", "ocean_play", "ocean_fetch"}
        check(wanted <= keys, f"registry keys {sorted(wanted - keys) or 'all present'}")
        refused = []
        for key, value in (("ocean_choppiness", 3), ("ocean_choppiness", -0.1), ("ocean_swell", 2), ("ocean_foam", -0.1),
                           ("ocean_foam", 1.5), ("ocean_period", 2), ("ocean_period", 500), ("ocean_mode", "fast"),
                           ("ocean_play", "maybe"), ("ocean_color", [2, 0, 0]), ("ocean_color", [0, 0]), ("ocean_fetch", 10)):
            try:
                session.set(key, value)
                refused.append(False)
            except glider_ctl.GliderError:
                refused.append(True)
        check(all(refused), f"pipe refuses choppiness 3 / -0.1, swell 2, foam -0.1 / 1.5, period 2 / 500, mode fast, play maybe, "
                            f"colour [2,0,0] / [0,0], fetch 10 {refused}")
        session.set("ground", "ocean")
        for key, value in (("ocean_wind_speed", 14), ("ocean_wind_dir", 120), ("ocean_quality", "low"), ("ocean_time", 3.5),
                           ("ocean_choppiness", 1.3), ("ocean_swell", 0.6), ("ocean_foam", 0.8),
                           ("ocean_color", [0.01, 0.03, 0.04]), ("ocean_period", 20), ("ocean_fetch", 50000)):
            session.set(key, value)
        session.call("capture", path=str(OUT / "pipe_ocean_b.png"), settle_frames=24, overlays=False)
        state = session.ocean()
        check(all(key in state for key in STATE_KEYS), "get_state ground.ocean carries every field")
        check(state["mode"] == "loop" and state["play"] is True and state["period"] == 20 and abs(state["foam"] - 0.8) < 1e-6,
              f"get_state reports mode {state['mode']}, play {state['play']}, period {state['period']}, foam {state['foam']}")
        errors = session.close()
        check(errors == 0, f"registry session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"registry session failed: {error}")
        session.process.kill()
        return
    check(cli is not None and same(cli, OUT / "pipe_ocean_b.png"),
          "pipe settings = CLI flags for choppiness, swell, foam, colour, period, fetch (byte-identical capture)")
    for bad in (["--ocean-choppiness", "5"], ["--ocean-swell", "-1"], ["--ocean-foam", "3"], ["--ocean-color", "2,0,0"],
                ["--ocean-period", "1"], ["--ocean-mode", "fast"], ["--ocean-play", "maybe"], ["--ocean-fetch", "5"]):
        result = run([EXE, "--isolated"] + bad, timeout=120)
        check(result.returncode != 0, f"{' '.join(bad)} is refused (exit {result.returncode})")
    # Play off holds the clock: the same image as the pinned time 0.
    held, _, _ = render(None, "play_off", OCEAN + ["--ocean-play", "off"], frame=32)
    zero, _, _ = render(None, "play_zero", OCEAN + ["--ocean-time", "0"], frame=32)
    check(held is not None and same(held, zero), "--ocean-play off: 32 frames later the surface is still at time 0")
    running, _, _ = render(None, "play_on", OCEAN, frame=32)
    check(running is not None and not same(running, zero), "--ocean-play on (default): the surface has moved")


def check_live():
    print("[Live mode: determinism, CLI = pipe, cost]")
    live = OCEAN + ["--ocean-mode", "live"]
    a, _, _ = render(None, "live_run1", live, frame=32)
    b, _, _ = render(None, "live_run2", live, frame=32)
    check(a is not None and same(a, b), "live: the same flags twice (clock running, 32 frames): identical captures")
    c, _, _ = render(None, "live_run40", live, frame=40)
    check(c is not None and not same(a, c), "live: frame 40 differs from frame 32 (the per-frame FFT moves the surface)")
    pinned = live + ["--ocean-time", "6", "--ocean-wind-speed", "12", "--ocean-quality", "low", "--ocean-foam", "0.7"]
    cli, _, _ = render(None, "live_cli", pinned)
    try:
        session = Session(None, ["--hide-grid"])
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        session.set("ground", "ocean")
        session.set("ocean_mode", "live")
        for key, value in (("ocean_time", 6), ("ocean_wind_speed", 12), ("ocean_quality", "low"), ("ocean_foam", 0.7)):
            session.set(key, value)
        session.call("capture", path=str(OUT / "live_pipe.png"), settle_frames=24, overlays=False)
        state = session.ocean()
        check(state["mode"] == "live" and state["liveSet"] and state["frames"] == 1,
              f"live state: mode {state['mode']}, live set {state['liveSet']}, {state['cascades']} x {state['resolution']}^2")
        errors = session.close()
        check(errors == 0, f"live registry session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"live registry session failed: {error}")
        session.process.kill()
        return
    check(cli is not None and same(cli, OUT / "live_pipe.png"), "live: pipe settings = CLI flags (pinned time, byte-identical)")
    # Cost: the running clock steps the FFT every frame.
    try:
        session = Session(None, OCEAN + ["--ocean-mode", "live", "--disable-redraw-skip"])
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        for quality in ("low", "medium", "high"):
            session.set("ocean_quality", quality)
            session.settle(4, "live_cost")
            before = session.ocean()["live"]["measured"]
            session.settle(60, "live_cost")
            session.settle(4, "live_cost")
            state = session.ocean()
            live_state = state["live"]
            # The average covers the steps measured since this set (the first step of a set builds its pipelines).
            print(f"  INFO live {quality}: {state['cascades']} x {state['resolution']}^2, {live_state['gpuAverageMs']:.3f} ms GPU per frame "
                  f"(average of {live_state['measured']} steps, last {live_state['gpuMs']:.3f}, max {live_state['gpuMaxMs']:.3f}), "
                  f"memory {state['memoryBytes'] / 1048576:.1f} MiB, set build {state['precompute']['wallMs']:.1f} ms wall")
            check(live_state["measured"] - before >= 30, f"live {quality}: {live_state['measured'] - before} steps measured in 64 frames")
            limit = {"low": 2.0, "medium": 3.0, "high": 6.0}[quality]
            check(live_state["gpuAverageMs"] < limit, f"live {quality}: {live_state['gpuAverageMs']:.3f} ms GPU per frame < {limit} ms")
        errors = session.close()
        check(errors == 0, f"live cost session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"live cost session failed: {error}")
        session.process.kill()


def check_energy():
    print("[Energy: a calm sea under a uniform sky against the Fresnel expectation]")
    env = ["--env", str(uniform_environment())]
    base = OCEAN + env + NEUTRAL + ["--ocean-wind-speed", "3", "--ocean-foam", "0", "--ground-haze", "1e9", "--ocean-time", "5"]
    color = (0.0035, 0.022, 0.035)  # the default water colour
    f0 = 0.02
    results = {}
    for label, pitch in (("nadir", 88), ("oblique", 20)):
        try:
            session = Session(None, base + ["--camera-pitch", str(pitch), "--camera-distance", "30"])
        except Exception as error:  # noqa: BLE001
            check(False, f"pipe session started: {error}")
            return
        try:
            session.settle(24, "energy_settle")
            vp = session.call("get_state")["viewports"][0]
            w, h = vp["width"], vp["height"]
            # A region of open water away from the cube at the centre (lower left of the image).
            x0, x1 = int(w * 0.08), int(w * 0.28)
            y0, y1 = (int(h * 0.62), int(h * 0.92)) if pitch > 45 else (int(h * 0.72), int(h * 0.88))
            region = session.call("probe_region", x0=x0, x1=x1, y0=y0, y1=y1)
            results[label] = region["rgb"]["mean"]
            session.close()
        except Exception as error:  # noqa: BLE001
            check(False, f"energy session failed: {error}")
            session.process.kill()
            return
    nadir = results["nadir"]
    expected = [UNIFORM_LEVEL * (f0 + (1.0 - f0) * c) for c in color]
    print(f"  INFO nadir: measured {[round(v, 4) for v in nadir]}, expected L (F0 + (1 - F0) colour) = {[round(v, 4) for v in expected]}")
    for channel, name in enumerate("RGB"):
        error = abs(nadir[channel] - expected[channel]) / expected[channel]
        check(error <= ENERGY_TOLERANCE, f"nadir {name}: {nadir[channel]:.4f} vs {expected[channel]:.4f} ({error:.1%} <= {ENERGY_TOLERANCE:.0%})")
    # Fresnel rise: the red channel (hardly any body light) is the sky reflection alone; at 70 degrees off the normal it
    # is several times the nadir value (Schlick F(70 deg) = 0.143 against 0.02; the waves and the roughness lower it).
    ratio = results["oblique"][0] / max(nadir[0], 1e-9)
    check(2.5 <= ratio <= 10.0, f"Fresnel: red at 70 degrees incidence / at nadir = {ratio:.2f} (2.5 .. 10; Schlick {0.143 / 0.0234:.1f})")


def check_foam():
    print("[Foam: whitecap coverage against the wind speed (uniform sky, foam = lit white against dark water)]")
    import numpy as np
    from PIL import Image
    env = ["--env", str(uniform_environment())]
    base = OCEAN + env + NEUTRAL + ["--ground-haze", "1e9", "--ocean-time", "5"]

    def coverage(name, wind, foam, view):
        png, _, _ = render(None, name, base + ["--ocean-wind-speed", str(wind), "--ocean-foam", str(foam)] + view)
        if png is None:
            return None
        image = np.asarray(Image.open(png).convert("RGB")).astype(float) / 255.0
        luma = image.mean(axis=2)
        near = luma[int(luma.shape[0] * 0.45):, :int(luma.shape[1] * 0.4)]
        return float((near > 0.3).mean()) * 100.0

    table = {}
    for wind in (3, 10, 20):
        table[wind] = (coverage(f"foam_wide_{wind}", wind, 0.5, FOAM_WIDE), coverage(f"foam_near_{wind}", wind, 0.5, FOAM_NEAR))
        target = 3.84e-6 * wind ** 3.41 * 100.0
        print(f"  INFO {wind} m/s: whitecaps {table[wind][0]:.2f}% over 120 m of sea, {table[wind][1]:.2f}% over 30 m "
              f"(Monahan and O'Muircheartaigh {target:.2f}%)")
    off = coverage("foam_off_20", 20, 0, FOAM_WIDE)
    # Live mode with a pinned time: the foam history is pre-rolled, so the coverage matches the looping set.
    live = {}
    for wind in (10, 20):
        png, _, _ = render(None, f"foam_live_{wind}", base + ["--ocean-wind-speed", str(wind), "--ocean-foam", "0.5",
                                                                "--ocean-mode", "live"] + FOAM_WIDE)
        image = np.asarray(Image.open(png).convert("RGB")).astype(float) / 255.0
        luma = image.mean(axis=2)
        near = luma[int(luma.shape[0] * 0.45):, :int(luma.shape[1] * 0.4)]
        live[wind] = float((near > 0.3).mean()) * 100.0
        print(f"  INFO live {wind} m/s: whitecaps {live[wind]:.2f}% (pinned time, history pre-rolled)")
    check(None not in (table[3][0], table[10][0], table[20][0]) and table[3][0] < table[10][0] < table[20][0],
          f"foam increases with the wind: {table[3][0]:.2f}% < {table[10][0]:.2f}% < {table[20][0]:.2f}% (3, 10, 20 m/s)")
    check(table[3][0] < 0.5, f"calm (3 m/s): little foam ({table[3][0]:.2f}% < 0.5%)")
    check(0.3 <= table[10][0] <= 3.5, f"10 m/s: whitecaps {table[10][0]:.2f}% within 0.3 .. 3.5% (Monahan 0.99%)")
    check(4.0 <= table[20][0] <= 26.0, f"20 m/s: whitecaps {table[20][0]:.2f}% within 4 .. 26% (Monahan 10.5%)")
    check(off is not None and off < 0.05, f"foam amount 0 at 20 m/s: {off:.3f}% < 0.05%")
    check(0.3 <= live[10] <= 3.5 and 4.0 <= live[20] <= 26.0,
          f"live mode (pinned time): {live[10]:.2f}% at 10 m/s (0.3 .. 3.5%), {live[20]:.2f}% at 20 m/s (4 .. 26%)")


def check_shadow_water():
    print("[Shadow on the water: only the sun's direct terms are removed]")
    import shadow_fixtures
    b = shadow_fixtures.Builder("A6.8 ocean shadow slab")
    b.box("Slab", (-5.0, 7.9, -5.0), (5.0, 8.0, 5.0), b.material("Grey", color=(0.5, 0.5, 0.5, 1.0)))
    model = b.write(OUT / "shadow_slab.gltf")
    az, el = 40.0, 55.0
    flags = OCEAN + ["--ground-height", "0", "--ocean-wind-speed", "6", "--ocean-time", "3", "--environment", "sky",
                     "--sun-source", "manual", "--sun-dir", f"{az},{el}", "--camera-target", "0,0,0", "--camera-yaw", "200",
                     "--camera-pitch", "35", "--camera-distance", "40", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0"]
    try:
        session = Session(model, flags)
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        session.settle(24, "shadow_settle")
        cam = session.camera()
        d = (math.sin(math.radians(az)), math.cos(math.radians(az)))
        t = 8.0 / math.tan(math.radians(el))
        shadow = (-d[0] * t, 0.0, -d[1] * t)
        perp = (d[1], -d[0])

        def mean_luminance(point):
            x, y = cam.project(point)
            region = session.call("probe_region", x0=int(x) - 8, x1=int(x) + 8, y0=int(y) - 8, y1=int(y) + 8)
            return region["luminance"]["mean"]

        dark = mean_luminance(shadow)
        lit = [mean_luminance((shadow[0] + s * perp[0] * 14, 0.0, shadow[2] + s * perp[1] * 14)) for s in (1, -1)]
        # The sun's glitter can fall into one of the lit samples: the dimmer one has none.
        ratio = min(lit) / dark
        print(f"  INFO shadow {dark:.4f}, lit {[round(v, 4) for v in lit]} (sun {el:.0f} deg elevation, sky, 6 m/s): ratio {ratio:.2f}")
        check(SHADOW_RATIO_RANGE[0] <= ratio <= SHADOW_RATIO_RANGE[1],
              f"lit / shadowed water radiance {ratio:.2f} within {SHADOW_RATIO_RANGE[0]} .. {SHADOW_RATIO_RANGE[1]} "
              f"(the sky reflection and sky-lit body light stay; a full removal of the sun-driven body light would give about 2.5 to 4)")
        errors = session.close()
        check(errors == 0, f"shadow session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"shadow session failed: {error}")
        session.process.kill()


OCEAN_UI_TAGS = ["vp.ground.type", "vp.ground.height", "vp.ocean.mode", "vp.ocean.quality", "vp.ocean.wind_speed",
                 "vp.ocean.wind_dir", "vp.ocean.choppiness", "vp.ocean.swell", "vp.ocean.foam", "vp.ocean.color",
                 "vp.ocean.period", "vp.ocean.fetch", "vp.ocean.play", "vp.ocean.reset"]
PLANE_UI_TAGS = ["vp.ground.type", "vp.ground.height", "vp.ground.material", "vp.ground.albedo", "vp.ground.roughness",
                 "vp.ground.detail", "vp.ground.detail_size", "vp.ground.joints", "vp.ground.haze", "vp.ground.show_grid"]


def check_ui():
    print("[Settings UI: the Ground section in Ocean and Plane mode, ImGui ID conflicts]")
    commands = ["wait 24", "clickp hier.viewport 1", "wait 8", "clickp vp.tab.camera 0", "wait 8",
                "set_viewport_setting ground ocean", "wait 12"]
    for tag in OCEAN_UI_TAGS:
        commands += [f"hoverp {tag} 0", "wait 3", f"snap {tag}"]
    commands += [f"shot {(OUT / 'ui_ocean.png').as_posix()}", "set_viewport_setting ground plane", "wait 12"]
    for tag in PLANE_UI_TAGS:
        commands += [f"hoverp {tag} 0", "wait 3", f"snap plane_{tag}"]
    commands += [f"shot {(OUT / 'ui_plane.png').as_posix()}", "set_viewport_setting ground ocean", "wait 6",
                 "set_viewport_setting ocean_mode live", "wait 12", f"shot {(OUT / 'ui_ocean_live.png').as_posix()}",
                 "hoverp vp.ocean.mode 0", "wait 3", "snap live_mode", "snap final", ""]
    script, report = OUT / "ui_ground.txt", OUT / "ui_ground.json"
    script.write_text("\n".join(commands), encoding="utf-8")
    report.unlink(missing_ok=True)
    result = run([EXE, "--isolated", "--window-size", "1920x1080", "--fixed-frame-time", "0.05", "--aa", "none",
                  "--vsync", "off", "--ui-script", script, "--ui-report", report], timeout=300)
    (OUT / "ui_ground.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else {}
    snaps = data.get("snaps", [])
    conflicts = [s["label"] for s in snaps if s.get("idConflict", -1) != 0 or s.get("hoveredIdCount", 0) > 1]
    check(result.returncode == 0 and not data.get("failed", True) and not data.get("errors"),
          f"UI script ran (exit {result.returncode}, failed {data.get('failed')}, errors {data.get('errors')})")
    check(len(snaps) >= len(OCEAN_UI_TAGS) + len(PLANE_UI_TAGS), f"{len(snaps)} hover snapshots (every Ocean and Plane control)")
    check(not conflicts, f"no ImGui ID conflict on any Ground control in Ocean and Plane mode {conflicts}")
    check(max((s["glErrors"] for s in snaps), default=-1) == 0, "UI script: 0 GL errors")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=str(REPO / "build" / "baselines" / "7d5a091.exe"))
    parser.add_argument("--only", default=None, help="comma-separated: baseline,loop,registry,registry_b,live,energy,foam,shadow,ui,nocompute,depth,"
                                                  "interior,cost")
    args = parser.parse_args()
    only = set(args.only.split(",")) if args.only else None

    def wanted(name):
        return only is None or name in only

    OUT.mkdir(parents=True, exist_ok=True)
    if wanted("baseline"):
        check_baseline(Path(args.baseline_exe))
    if wanted("loop"):
        check_loop_and_determinism()
    if wanted("registry"):
        check_registry()
    if wanted("registry_b"):
        check_registry_b()
    if wanted("live"):
        check_live()
    if wanted("energy"):
        check_energy()
    if wanted("foam"):
        check_foam()
    if wanted("shadow"):
        check_shadow_water()
    if wanted("ui"):
        check_ui()
    if wanted("nocompute"):
        check_no_compute()
    if wanted("depth"):
        check_depth_and_pick()
    if wanted("interior"):
        check_interior()
    if wanted("cost"):
        check_cost()
    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in {len(gl_logs)} renders {bad if bad else ''}")
    finish()


if __name__ == "__main__":
    main()
