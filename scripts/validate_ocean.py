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
    # 7.3 and 37.3 are not exactly 30 s apart as floats (1e-6 s): the frame positions agree within 1e-4 frames and the
    # images within one level.
    a, _, _ = render(None, "seam_7.3", OCEAN + ["--ocean-time", "7.3"])
    b, _, _ = render(None, "seam_37.3", OCEAN + ["--ocean-time", "37.3"])
    if a is not None and b is not None:
        import numpy as np
        from validate_sun import load
        diff = np.abs(load(a) - load(b)).max()
        check(diff <= 1.0, f"ocean time 7.3 s and 37.3 s: images differ by at most {diff:.0f} level (<= 1)")
    else:
        check(False, "seam captures at 7.3 s and 37.3 s")
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=str(REPO / "build" / "baselines" / "361d538.exe"))
    parser.add_argument("--only", default=None, help="comma-separated: baseline,loop,registry,nocompute,depth,interior,cost")
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
