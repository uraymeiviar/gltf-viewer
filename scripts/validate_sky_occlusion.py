# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.9 sky occlusion inside the model (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.9; header/imgui/SkyOcclusion.h,
resources/shaders/sky_occlusion.glsl).

Checks:
  * HDR default (sky occlusion automatic = off with the HDR): byte-identical to the baseline (HelloCube, the E195
    cockpit_main_displays and exterior_fuselage_nose views, the DA62 SDK cockpit if installed, read only); the
    procedural sky with --sky-occlusion off renders HelloCube byte-identical to the baseline's sky; get_state reports
    the automatic default (off with the HDR, on with the sky);
  * synthetic box with one open side (2 x 2 x 2 m inside, opening on top), uniform HDR, --sky-occlusion on: the white
    Lambert floor's sky visibility at its centre (debug view and the shaded on / off radiance ratio, which are the same
    term) is the analytic form factor of the opening seen from the lookup point (BOX_TOLERANCE); the box's outer wall
    face (a point outside the box) reads 1;
  * an open ground plane and an isolated sphere (Lambert and a glossy dielectric): every probed region within
    OPEN_TOLERANCE of sky occlusion off; the debug view >= OPEN_DEBUG_MIN on every plane / sphere pixel;
  * environment occluders (ASOBO_material_environment_occluder lid on the same box): they block the sky (floor
    visibility < CLOSED_MAX) while they stay invisible in the image (sky occlusion off: the lidded box renders
    byte-identical to the open box); --sky-occlusion-occluders off gives the open box's value again;
  * light leaking through walls: a closed box of zero-thickness double-sided quads (occluder lid): floor visibility at
    least LEAK_CELLS cells from a wall <= LEAK_MAX; the largest value closer to a wall is reported (the measured leak);
  * the bake: every quality finishes (frames = directions / directions per frame, nothing left baking), the volume is
    reported in get_state / --profile-report; before it finishes the image is exactly the sky-occlusion-off image (no
    volume yet); while a newer bake (quality change) runs, the viewport keeps rendering the previous complete volume
    (byte-identical frame by frame) and swaps to the new one only when it is complete (= the CLI render at that
    quality); real assets (E195 cockpit, E195 exterior, DA62 cockpit) bake at every quality with no frame above
    FRAME_BUDGET_MS of CPU or GPU bake time (time-sliced); the per-frame cost of the lookup is reported;
  * registry keys and CLI flags: CLI = pipe (byte-identical), invalid values refused, zero GL errors.

Run: python scripts/validate_sky_occlusion.py [--baseline-exe build/baselines/<sha>.exe] [--only a,b]
"""

import argparse
import atexit
import json
import os
import statistics
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
import sky_occlusion_fixtures as fixtures  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO, check, finish, same, view, render_view  # noqa: E402
from validate_asobo_sss import write_hdr  # noqa: E402
from validate_sun import Camera, gltf, lambert, gl_errors_zero, load  # noqa: E402

OUT = REPO / "build" / "sky_occlusion"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
LOOK = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
        "--tone-mapping", "neutral"]
TOP_VIEW = ["--camera-target", "0,0,0", "--camera-yaw", "20", "--camera-pitch", "80", "--camera-distance", "6"]
SIDE_VIEW = ["--camera-target", "0,1,0", "--camera-yaw", "60", "--camera-pitch", "10", "--camera-distance", "6"]
PLANE_VIEW = ["--camera-target", "0,0,0", "--camera-yaw", "30", "--camera-pitch", "35", "--camera-distance", "9"]
SPHERE_VIEW = ["--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "4"]
ON = ["--sky-occlusion", "on"]
SKY = ["--environment", "sky", "--time-of-day-mode", "on", "--hide-grid", "--time-preset", "noon"]
DA62 = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
DA62_COCKPIT = DA62 / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo/Part_Interior_Cockpit/model/Cockpit_LOD00.gltf"
DA62_CAMERA = ["--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05", "--camera-pitch", "-20", "--camera-fov", "55",
               "--camera-yaw", "220", "--search-path", str(DA62 / "Sources/Art/Max/textures")]

BOX_TOLERANCE = 0.05      # floor-centre visibility against the analytic opening form factor (absolute)
RATIO_TOLERANCE = 0.01    # shaded on / off radiance ratio against the debug view's visibility (absolute)
OPEN_TOLERANCE = 0.01     # open plane / isolated sphere: relative radiance change per probed region
OPEN_DEBUG_MIN = 0.99     # debug view on the open plane / sphere
CLOSED_MAX = 0.05         # floor-centre visibility with the occluder lid
LEAK_CELLS = 2.0          # thin box: floor points at least this many cells from a wall ...
LEAK_MAX = 0.02           # ... see no sky
FRAME_BUDGET_MS = 200.0   # largest bake time in one frame (CPU submission, GPU timestamps)

gl_logs = []
_processes = []


def _kill_all():
    for process in _processes:
        if process.poll() is None:
            process.kill()


atexit.register(_kill_all)


def run(args, timeout=900):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(model, name, flags=(), exe=EXE, frame=24, common=COMMON):
    OUT.mkdir(parents=True, exist_ok=True)
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    args = [exe] + ([model] if model else []) + list(common) + list(flags)
    result = run(args + ["--capture-viewport", png, "--capture-frame", str(frame), "--profile-report", report,
                         "--exit-after-profile"])
    if Path(exe) == EXE:
        gl_logs.append((name, result.stdout + result.stderr))
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    return (png if png.exists() else None), data


def debug_value(png, cam, point):
    """The Sky Visibility debug view's value (red channel / 255) at a world point."""
    x, y = cam.project(point)
    return load(png)[int(round(y)), int(round(x)), 0] / 255.0


class Session:
    """A --control-pipe Glider; killed whenever the session fails or the script exits."""

    def __init__(self, model, flags=()):
        args = [str(EXE)] + ([str(model)] if model else []) + COMMON + list(flags) + ["--control-pipe"]
        self.process = subprocess.Popen(args, cwd=REPO, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                        stderr=subprocess.DEVNULL)
        _processes.append(self.process)
        self.pipe = None
        try:
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
                        raise
                    time.sleep(0.2)
        except BaseException:
            self.kill()
            raise

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def capture(self, name, frames=8):
        path = OUT / f"{name}.png"
        self.call("capture", path=str(path), settle_frames=frames, overlays=False)
        return path

    def kill(self):
        if self.pipe is not None:
            try:
                self.pipe.close()
            except Exception:  # noqa: BLE001
                pass
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(30)

    def close(self):
        errors = None
        try:
            errors = self.call("get_state")["glErrors"]
            self.call("quit")
            self.pipe.close()
            self.pipe = None
            self.process.wait(60)
        finally:
            self.kill()
        return errors


def write(name, doc):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def glossy():
    return gltf({"name": "Glossy", "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.8, 0.8, 1], "metallicFactor": 0.0,
                                                           "roughnessFactor": 0.3}})


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_baseline(baseline):
    print("[HDR default (off): byte-identical to the baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    print(f"  INFO baseline {baseline.name}")
    now, data = render(None, "base_now_hellocube", [])
    before, _ = render(None, "base_ref_hellocube", [], exe=baseline)
    check(same(now, before), f"HelloCube (HDR): byte-identical to {baseline.stem}")
    so = (data or {}).get("skyOcclusion") or {}
    check(so.get("setting") == "auto" and so.get("enabled") is False and so.get("active") is False and so.get("volume") is None,
          "HDR default: sky occlusion automatic and off, nothing baked (get_state / --profile-report skyOcclusion)")
    sky = ["--environment", "sky", "--sky-time-slicing", "off"]
    parity = sky + ["--sky-occlusion", "off", "--ambient", "off"]
    now, data = render(None, "base_now_hellocube_sky_off", parity)
    before, _ = render(None, "base_ref_hellocube_sky", parity, exe=baseline)
    check(same(now, before), f"HelloCube, procedural sky, --sky-occlusion off: byte-identical to {baseline.stem}'s sky")
    _, data = render(None, "now_hellocube_sky_default", sky)
    so = (data or {}).get("skyOcclusion") or {}
    check(so.get("setting") == "auto" and so.get("enabled") is True and so.get("active") is True and so.get("volume"),
          "procedural sky default: sky occlusion automatic and on, the volume baked")
    for name in ("cockpit_main_displays", "exterior_fuselage_nose"):
        entry = view(name)
        a = render_view(EXE, entry, f"base_now_{name}", folder=OUT)
        b = render_view(baseline, entry, f"base_ref_{name}", folder=OUT)
        if a is None and b is None:
            print(f"  SKIP {name}: E195 asset not available")
            continue
        check(same(a, b), f"E195 {name} (HDR): byte-identical to {baseline.stem}")
    if DA62_COCKPIT.exists():
        a, _ = render(DA62_COCKPIT, "base_now_da62", DA62_CAMERA)
        b, _ = render(DA62_COCKPIT, "base_ref_da62", DA62_CAMERA, exe=baseline)
        check(same(a, b), f"DA62 SDK cockpit (HDR): byte-identical to {baseline.stem}")
    else:
        print("  SKIP DA62 SDK cockpit not installed")


def box_floor(uniform, box, name, flags=()):
    """Debug-view visibility at the floor centre, the analytic value at the lookup point, and the report."""
    png, data = render(box, name, uniform + LOOK + TOP_VIEW + ON + ["--debug-view", "skyvisibility"] + list(flags))
    if png is None or data is None:
        return None, None, data
    so = data["skyOcclusion"]
    height = so["volume"]["cell"] * so["normalOffsetCells"] if so.get("volume") else 0.0
    analytic = fixtures.opening_form_factor(fixtures.HALF, fixtures.HEIGHT - height)
    return debug_value(png, Camera(data), (0.0, 0.0, 0.0)), analytic, data


def check_box(uniform, box, occluder):
    print("[box with one open side: floor visibility against the analytic form factor]")
    for quality in ("low", "medium", "high"):
        measured, analytic, data = box_floor(uniform, box, f"box_dbg_{quality}", ["--sky-occlusion-quality", quality])
        if measured is None:
            check(False, f"{quality}: box render")
            continue
        so = data["skyOcclusion"]
        raw = measured * so["openFraction"]
        check(abs(measured - analytic) <= BOX_TOLERANCE,
              f"{quality}: floor centre {measured:.3f} (raw {raw:.3f} before the open-fraction scale) vs analytic "
              f"{analytic:.3f} (|difference| {abs(measured - analytic):.3f} <= {BOX_TOLERANCE}; volume "
              f"{'x'.join(map(str, so['volume']['dims']))}, cell {so['volume']['cell']:.4f} m, {so['directions']} directions)")
    # A point outside the box: the outer wall face that looks towards the camera.
    png, data = render(box, "box_side_dbg", uniform + LOOK + SIDE_VIEW + ON + ["--debug-view", "skyvisibility"])
    if png is not None:
        cam = Camera(data)
        eye = cam.eye
        face = (fixtures.HALF + fixtures.WALL, 1.0, 0.0) if eye[0] > 0 else (-fixtures.HALF - fixtures.WALL, 1.0, 0.0)
        side = (0.0, 1.0, fixtures.HALF + fixtures.WALL) if eye[2] > 0 else (0.0, 1.0, -fixtures.HALF - fixtures.WALL)
        values = [debug_value(png, cam, p) for p in (face, side)]
        check(min(values) >= OPEN_DEBUG_MIN, f"outer wall faces (points outside the box) read {values[0]:.3f} / {values[1]:.3f} "
                                             f">= {OPEN_DEBUG_MIN}")
    else:
        check(False, "box side render")

    print("[shaded: the on / off radiance ratio is the debug view's visibility (CLI = pipe)]")
    cli, _ = render(box, "box_cli_on", uniform + LOOK + TOP_VIEW + ON)
    try:
        session = Session(box, uniform + LOOK + TOP_VIEW)
        session.call("set_environment", key="sky_occlusion", value="off")
        session.capture("box_pipe_off", 24)
        x, y = Camera(json.loads((OUT / "box_dbg_medium.json").read_text(encoding="utf-8"))).project((0.0, 0.0, 0.0))
        off = session.call("probe_region", x0=int(x) - 2, y0=int(y) - 2, x1=int(x) + 3, y1=int(y) + 3)
        session.call("set_environment", key="sky_occlusion", value="on")
        piped = session.capture("box_pipe_on", 24)
        on = session.call("probe_region", x0=int(x) - 2, y0=int(y) - 2, x1=int(x) + 3, y1=int(y) + 3)
        state = session.call("get_state")["skyOcclusion"]
        errors = session.close()
        check(same(cli, piped), "set_environment sky_occlusion on = --sky-occlusion on (byte-identical capture)")
        ratio = on["luminance"]["mean"] / max(off["luminance"]["mean"], 1e-9)
        measured, _, _ = box_floor(uniform, box, "box_dbg_medium_again")
        check(abs(ratio - measured) <= RATIO_TOLERANCE,
              f"floor centre radiance on / off {ratio:.4f} = debug visibility {measured:.4f} (|difference| <= {RATIO_TOLERANCE})")
        check(state["active"] and state["bake"]["completed"] >= 1 and not state["baking"],
              f"pipe: the volume is baked and active ({state['bake']['completed']} bake(s))")
        check(errors == 0, f"box pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"box pipe session failed: {error}")

    print("[environment occluders block the sky, invisible in the image]")
    closed, _, data = box_floor(uniform, occluder, "occluder_dbg")
    opened, analytic, _ = box_floor(uniform, occluder, "occluder_dbg_off", ["--sky-occlusion-occluders", "off"])
    if closed is not None and opened is not None:
        casters = data["skyOcclusion"]["occluderCasters"]
        check(closed < CLOSED_MAX and casters == 1,
              f"occluder lid: floor centre {closed:.3f} < {CLOSED_MAX} ({casters} occluder caster)")
        check(abs(opened - analytic) <= BOX_TOLERANCE,
              f"--sky-occlusion-occluders off: floor centre {opened:.3f} = the open box (analytic {analytic:.3f})")
    else:
        check(False, "occluder renders")
    a, _ = render(occluder, "occluder_color_off", uniform + LOOK + TOP_VIEW + ["--sky-occlusion", "off"])
    b, _ = render(box, "box_color_off", uniform + LOOK + TOP_VIEW + ["--sky-occlusion", "off"])
    check(same(a, b), "the occluder lid is invisible in colour (lidded box = open box, byte-identical)")


def check_leak(uniform, thin):
    import numpy as np
    print("[light leaking through zero-thickness walls]")
    png, data = render(thin, "thin_dbg", uniform + LOOK + TOP_VIEW + ON + ["--debug-view", "skyvisibility"])
    if png is None:
        check(False, "thin box render")
        return
    cam, image = Camera(data), load(png)[..., 0] / 255.0
    cell = data["skyOcclusion"]["volume"]["cell"]
    inner, near = [], []
    for x in np.linspace(-0.98, 0.98, 99):
        for z in np.linspace(-0.98, 0.98, 99):
            px, py = cam.project((x, 0.0, z))
            if not (0 <= px < image.shape[1] and 0 <= py < image.shape[0]):
                continue
            value = image[int(round(py)), int(round(px))]
            distance = (fixtures.HALF - max(abs(x), abs(z))) / cell
            (inner if distance >= LEAK_CELLS else near).append((value, distance))
    inner_max = max(v for v, _ in inner)
    near_max = max(near, key=lambda item: item[0]) if near else (0.0, 0.0)
    check(inner_max <= LEAK_MAX, f"closed thin box: floor >= {LEAK_CELLS:.0f} cells from a wall sees {inner_max:.3f} "
                                 f"<= {LEAK_MAX} ({len(inner)} points)")
    print(f"  INFO measured leak within {LEAK_CELLS:.0f} cells of a zero-thickness wall: up to {near_max[0]:.3f} "
          f"(at {near_max[1]:.2f} cells; cell {cell * 100:.2f} cm)")


def region_means(session, cam, points, radius=3):
    out = []
    for point in points:
        x, y = cam.project(point)
        r = session.call("probe_region", x0=int(x) - radius, y0=int(y) - radius, x1=int(x) + radius + 1, y1=int(y) + radius + 1)
        out.append(r["luminance"]["mean"])
    return out


def check_open(uniform):
    import math
    print("[open ground plane and isolated spheres: unchanged within 1%]")
    plane = fixtures.open_plane(OUT / "open_plane.gltf")
    cases = [("plane", plane, PLANE_VIEW, [(x, 0.0, z) for x in (-3.0, 0.0, 3.0) for z in (-3.0, 0.0, 3.0)]),
             ("lambert sphere", write("sphere_lambert", lambert()), SPHERE_VIEW, None),
             ("glossy sphere", write("sphere_glossy", glossy()), SPHERE_VIEW, None)]
    for label, model, camera, points in cases:
        png, data = render(model, f"open_dbg_{label.replace(' ', '_')}", uniform + LOOK + camera + ON + ["--debug-view", "skyvisibility"])
        if png is None:
            check(False, f"{label}: debug render")
            continue
        cam = Camera(data)
        if points is None:
            points = []
            for theta in range(-60, 61, 30):
                for phi in range(-60, 61, 30):
                    t, p = math.radians(theta), math.radians(phi)
                    points.append((math.sin(p) * math.cos(t), math.sin(t), math.cos(p) * math.cos(t)))
            # Only points facing the camera.
            points = [q for q in points if sum(q[i] * (cam.eye[i] - q[i]) for i in range(3)) > 0.3]
        image = load(png)[..., 0] / 255.0
        mask = load(png).max(axis=2) > 0
        values = image[mask]
        check(values.size > 1000 and values.min() >= OPEN_DEBUG_MIN,
              f"{label}: debug view min {values.min():.3f} >= {OPEN_DEBUG_MIN} over {values.size} px")
        try:
            session = Session(model, uniform + LOOK + camera + ["--sky-occlusion", "off"])
            session.capture(f"open_off_{label.replace(' ', '_')}", 24)
            off = region_means(session, cam, points)
            session.call("set_environment", key="sky_occlusion", value="on")
            session.capture(f"open_on_{label.replace(' ', '_')}", 24)
            on = region_means(session, cam, points)
            errors = session.close()
            worst = max(abs(a / max(b, 1e-9) - 1.0) for a, b in zip(on, off))
            check(worst <= OPEN_TOLERANCE, f"{label}: {len(points)} regions, largest relative change {worst * 100:.3f}% "
                                           f"<= {OPEN_TOLERANCE * 100:.0f}%")
            check(errors == 0, f"{label} session: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"{label} session failed: {error}")


def check_bake(uniform, box):
    print("[bake: completes, never swapped in half-baked]")
    for quality, directions, per_frame in (("low", 64, 16), ("medium", 128, 16), ("high", 256, 32)):
        _, data = render(box, f"bake_{quality}", uniform + LOOK + TOP_VIEW + ON + ["--sky-occlusion-quality", quality])
        so = (data or {}).get("skyOcclusion") or {}
        bake = so.get("bake") or {}
        check(so.get("active") and not so.get("baking") and bake.get("completed") == 1 and so.get("directions") == directions
              and bake.get("frames") == directions // per_frame and (so.get("volume") or {}).get("quality") == quality,
              f"{quality}: one bake of {directions} directions in {bake.get('frames')} frames, volume "
              f"{'x'.join(map(str, (so.get('volume') or {}).get('dims', [])))}, {bake.get('cpuMs', 0):.1f} ms CPU, "
              f"{bake.get('gpuMs') or 0:.1f} ms GPU")
    # Before the first bake completes there is no volume: the frame equals the off frame.
    early_on, _ = render(box, "early_on", uniform + LOOK + TOP_VIEW + ON, frame=2)
    early_off, _ = render(box, "early_off", uniform + LOOK + TOP_VIEW + ["--sky-occlusion", "off"], frame=2)
    check(same(early_on, early_off), "frame 2 (bake running): the image is the sky-occlusion-off image (no volume yet)")
    # A quality change: the previous complete volume stays in use until the new one is complete.
    reference, _ = render(box, "swap_reference_high", uniform + LOOK + TOP_VIEW + ON + ["--sky-occlusion-quality", "high"])
    try:
        session = Session(box, uniform + LOOK + TOP_VIEW + ON + ["--disable-redraw-skip"])
        before = session.capture("swap_before", 24)
        session.call("set_environment", key="sky_occlusion_quality", value="high")
        frames, held, swapped, intermediate = 0, True, None, 0
        while frames < 40:
            image = session.capture(f"swap_{frames:02d}", 1)
            state = session.call("get_state")["skyOcclusion"]
            frames += 1
            if state["baking"]:
                intermediate += 1
                held = held and same(image, before) and state["volume"]["quality"] == "medium"
            else:
                swapped = image
                break
        errors = session.close()
        check(intermediate > 0 and held, f"quality change: {intermediate} frames rendered while baking, all with the previous "
                                         f"complete volume (byte-identical, volume quality medium)")
        check(swapped is not None and same(swapped, reference),
              f"after {frames} frames the new volume is in: byte-identical to the CLI render at high quality")
        check(errors == 0, f"swap session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"swap session failed: {error}")


def check_registry(uniform, box):
    print("[registry keys, CLI flags]")
    try:
        session = Session(box, uniform + LOOK + TOP_VIEW)
        keys = {s["key"] for s in session.call("list_settings")["settings"]}
        check({"sky_occlusion", "sky_occlusion_quality", "sky_occlusion_occluders"} <= keys,
              "registry keys sky_occlusion, sky_occlusion_quality, sky_occlusion_occluders")
        values = []
        for key, value in (("sky_occlusion", "on"), ("sky_occlusion", "auto"), ("sky_occlusion_quality", "low"),
                           ("sky_occlusion_occluders", False)):
            session.call("set_environment", key=key, value=value)
            values.append(session.call("get_environment", key=key)["value"] == value)
        check(all(values), "set_environment / get_environment round-trip every value")
        refused = 0
        for key, value in (("sky_occlusion", "maybe"), ("sky_occlusion_quality", "ultra")):
            try:
                session.call("set_environment", key=key, value=value)
            except glider_ctl.GliderError:
                refused += 1
        check(refused == 2, f"invalid values refused ({refused} / 2)")
        errors = session.close()
        check(errors == 0, f"registry session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"registry session failed: {error}")
    result = run([EXE, "--isolated", "--sky-occlusion", "sometimes", "--exit-after-dump", "--dump-frame", OUT / "bad.png"], timeout=120)
    check(result.returncode != 0, f"--sky-occlusion sometimes is refused (exit {result.returncode})")


def gpu_main_ms(report, label="main"):
    totals = {}
    for frame in report.get("gpu", {}).get("completed", []):
        if frame["label"] != label:
            continue
        totals[frame["frame"]] = totals.get(frame["frame"], 0.0) + sum(p["ms"] for p in frame["passes"] if p.get("available"))
    return totals


def check_costs():
    print("[real assets: bake time, memory, per-frame cost]")
    scenes = []
    cockpit, exterior = REAL_ASSETS.get("Cockpit"), REAL_ASSETS.get("Exterior")
    if cockpit and Path(cockpit).exists():
        scenes.append(("E195 cockpit", cockpit, list(view("cockpit_windshield_inside")["flags"])))
    if exterior and Path(exterior).exists():
        scenes.append(("E195 exterior", exterior, list(view("exterior_fuselage_nose")["flags"])))
    if DA62_COCKPIT.exists():
        scenes.append(("DA62 cockpit", DA62_COCKPIT, DA62_CAMERA))
    if not scenes:
        print("  SKIP no real assets")
        return
    results = {}
    for label, model, flags in scenes:
        for quality in ("low", "medium", "high"):
            tag = f"cost_{label.split()[0]}_{label.split()[1]}_{quality}".lower()
            _, data = render(model, tag, SKY + flags + ["--sky-occlusion-quality", quality], frame=40)
            so = (data or {}).get("skyOcclusion") or {}
            bake, volume = so.get("bake") or {}, so.get("volume") or {}
            results[f"{label} {quality}"] = {"volume": volume, "bake": bake, "casters": so.get("casters"),
                                             "occluderCasters": so.get("occluderCasters")}
            gpu = bake.get("maxFrameGpuMs")
            check(bake.get("completed") == 1 and bake.get("maxFrameCpuMs", 1e9) <= FRAME_BUDGET_MS and
                  (gpu is None or gpu <= FRAME_BUDGET_MS),
                  f"{label} {quality}: volume {'x'.join(map(str, volume.get('dims', [])))} (cell {volume.get('cell', 0) * 100:.1f} cm, "
                  f"{volume.get('memoryBytes', 0) / 2**20:.1f} MiB), {so.get('casters')} casters, bake {bake.get('frames')} frames, "
                  f"{bake.get('cpuMs', 0):.0f} ms CPU / {bake.get('gpuMs') or 0:.0f} ms GPU in total, largest frame "
                  f"{bake.get('maxFrameCpuMs', 0):.0f} ms CPU / {gpu or 0:.0f} ms GPU (<= {FRAME_BUDGET_MS:.0f})")
    # Per-frame cost of the lookup: the main frame's GPU pass times (median over frames 40-120), every frame redrawn.
    if cockpit and Path(cockpit).exists():
        flags = SKY + list(view("cockpit_windshield_inside")["flags"]) + ["--disable-redraw-skip"]
        times = {}
        for setting in ("off", "on"):
            _, data = render(cockpit, f"cost_frame_{setting}", flags + ["--sky-occlusion", setting], frame=120)
            per_frame = gpu_main_ms(data or {})
            samples = [ms for frame, ms in per_frame.items() if frame >= 40]
            times[setting] = statistics.median(samples) if samples else None
        results["frame"] = times
        if times["on"] is not None and times["off"] is not None:
            print(f"  INFO E195 cockpit, 1280 x 720: main frame GPU {times['off']:.3f} ms off, {times['on']:.3f} ms on "
                  f"({times['on'] - times['off']:+.3f} ms)")
    (OUT / "costs.json").write_text(json.dumps(results, indent=1), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", type=Path, default=None)
    parser.add_argument("--only", default="", help="comma-separated: baseline,box,leak,open,bake,registry,costs")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    baseline = args.baseline_exe
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None
    elif not baseline.is_absolute():
        baseline = REPO / baseline
    only = {s for s in args.only.split(",") if s}

    def wanted(name):
        return not only or name in only
    uniform = ["--env", str(write_hdr(OUT / "uniform.hdr", 64, 32, lambda x, y: (0.5, 0.5, 0.5)))]
    box = fixtures.open_box(OUT / "open_box.gltf")
    occluder = fixtures.occluder_box(OUT / "occluder_box.gltf")
    thin = fixtures.thin_box(OUT / "thin_box.gltf")
    if wanted("baseline"):
        check_baseline(baseline)
    if wanted("box"):
        check_box(uniform, box, occluder)
    if wanted("leak"):
        check_leak(uniform, thin)
    if wanted("open"):
        check_open(uniform)
    if wanted("bake"):
        check_bake(uniform, box)
    if wanted("registry"):
        check_registry(uniform, box)
    if wanted("costs"):
        check_costs()
    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in {len(gl_logs)} renders" + (f" (errors in {bad})" if bad else ""))
    return finish()


if __name__ == "__main__":
    sys.exit(main())
