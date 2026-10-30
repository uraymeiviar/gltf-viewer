# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.6: the ground plane (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.6).

A viewport's ground is the editor grid (--ground grid, the default) or an infinite horizontal plane
(--ground plane) rendered into the linear HDR scene target by per-pixel ray intersection (ground.frag), lit by
the IBL and by the A3.1 sun with the A3.2 shadow lookup, fogged into the environment's horizon.

Fixtures (small glTF files written here): a 2 x 2 m double-sided occluder quad 1.5 m above y = 0 and a box
buried below y = 0 (occ_zero); the same occluder with a rotated box whose lowest world point is 0.2 m and a
buried ASOBO_material_invisible box (base); a 1 x 10 x 1 m pole (pole). Unless stated: black environment map (no
IBL), black clear colour, PBR Neutral, exposure 0, no bloom / auto exposure / AA, sun az 90 el 45 intensity 1.8,
plane albedo 0.5, roughness 1, no detail, no joints, no fog (--ground-haze 1e9).
Checks:
  * Grid (the default): byte-identical to the baseline binary (--baseline-exe) on HelloCube and two ASOBO E195
    reference views, and --ground grid equals the default; with the plane selected, the debug and wireframe
    modes (the plane is drawn in shaded views only) equal the Grid render;
  * shadow: the edge of the occluder's shadow on the plane (50% crossing of the linear luminance) lies within
    EDGE_TOLERANCE_PX of the analytic edge for the plane at Y = 0 and, in Model base mode, at the fixture's
    lowest point 0.2 m (get_state ground.modelBase / height, an invisible buried box is ignored, the rotated
    box's corners are exact);
  * a tall pole at a 20 degree sun: the shadow 12 m and 24 m from it lies on the plane (the cascades also cover
    the footprint of the model on the plane);
  * probe over the control pipe: the lit plane equals albedo x sun term, E N.L ((1 - F) / pi + F V D) with the
    pbr.frag GGX terms (roughness 1), within PROBE_REL x expected + PROBE_ABS; the shadow is 0; the occluder
    occludes the plane (its pixel is the quad, picked as Occluder) and the plane occludes the box below it (its
    pixel has the plane's value); a click on the plane picks nothing (background);
  * uniform environment L = 0.5 (no sun): the white-furnace plane (albedo 1) returns L within FURNACE_TOLERANCE,
    albedo 0.25 lies between albedo x L and albedo x L + (1 - albedo) x 0.12 L; IBL and sun add
    (IBL-lit + sun-lit within ADDITIVE_TOLERANCE of the two separate renders);
  * ground albedo: get_state ground.albedo follows the preset (concrete, grass, sand, grey, custom) and
    ground_color (the value the A6.2 sky reads); registry keys, --ground-* flags, invalid values rejected;
  * wheels: the E195 exterior without its stairs and barrier (ground support equipment that stands 1 m below the
    nose gear in the file), Model base mode, a low camera: the lowest nose-tyre pixel is within WHEEL_TOLERANCE_M
    of the plane;
  * grid on the ground: --ground-grid on draws the grid at the plane's height (a custom Y = 1 plane: the Z axis
    line is drawn at Y = 1) and off draws none;
  * stability (default procedural concrete with slab joints): TAA frames 64 and 96 differ by at most TAA_LEVELS
    on the ground and at the horizon; the far field (distance fade) has no high-frequency energy (std of the
    5 x 5 high-pass below FAR_HF_LEVELS) and a 0.03 degree camera turn changes it by at most FAR_SHIMMER_LEVELS
    (mean) with no AA;
  * zero GL errors.

Run: python scripts/validate_ground.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from glider_harness import start_pipe_session  # noqa: E402
import shadow_fixtures  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same, view, render_view  # noqa: E402
from validate_asobo_sss import write_hdr  # noqa: E402
from validate_sun import Camera, load, untonemap, LUMA, gl_errors_zero  # noqa: E402

OUT = REPO / "build" / "ground"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
HEIGHT = 1.5                 # occluder height above y = 0
BASE = 0.2                   # lowest point of the base fixture
EDGE_TOLERANCE_PX = 2.0
PROBE_REL, PROBE_ABS = 0.02, 2e-3
FURNACE_TOLERANCE = 0.03
ADDITIVE_TOLERANCE = 0.015
WHEEL_TOLERANCE_M = 0.03
TAA_LEVELS = 6
FAR_HF_LEVELS = 1.0
FAR_SHIMMER_LEVELS = 0.6
SUN_AZ, SUN_EL, SUN_I = 90.0, 45.0, 1.8
ALBEDO, ROUGHNESS = 0.5, 1.0

gl_logs = []


def run(args, timeout=900):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(model, name, flags=(), exe=EXE, frame=24, common=COMMON, folder=OUT):
    folder.mkdir(parents=True, exist_ok=True)
    png, report = folder / f"{name}.png", folder / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    args = [exe] + ([model] if model else []) + list(common) + list(flags)
    result = run(args + ["--capture-viewport", png, "--capture-frame", str(frame), "--profile-report", report,
                         "--exit-after-profile"])
    if Path(exe) == EXE:
        gl_logs.append((name, result.stdout + result.stderr))
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    return (png if png.exists() else None), data


def linear(png):
    import numpy as np
    return untonemap(load(png)) @ np.array(LUMA)


def sun_vector(azimuth, elevation):
    a, e = math.radians(azimuth), math.radians(elevation)
    return (math.cos(e) * math.sin(a), math.sin(e), math.cos(e) * math.cos(a))


def sample(img, cam, point):
    x, y = cam.project(point)
    xi, yi = int(round(x)), int(round(y))
    if 0 <= xi < cam.width and 0 <= yi < cam.height:
        return img[yi, xi]
    return None


def edge_crossing(img, cam, p0, p1, steps=2000):
    values = []
    for i in range(steps + 1):
        t = i / steps
        p = [p0[k] + (p1[k] - p0[k]) * t for k in range(3)]
        v = sample(img, cam, p)
        if v is not None:
            values.append((p, v))
    lo, hi = min(v for _, v in values), max(v for _, v in values)
    mid = 0.5 * (lo + hi)
    for (pa, va), (pb, vb) in zip(values, values[1:]):
        if (va - mid) * (vb - mid) < 0:
            f = (mid - va) / (vb - va)
            return [pa[k] + (pb[k] - pa[k]) * f for k in range(3)], lo, hi
    return None, lo, hi


class Session:
    def __init__(self, model, flags=()):
        args = [str(EXE)] + ([str(model)] if model else []) + COMMON + list(flags) + ["--control-pipe"]
        self.process, self.pipe = start_pipe_session(args)

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def settle(self, frames=6):
        """Renders a few frames so a setting change is on screen before probing."""
        self.call("capture", path=str(OUT / "_settle.png"), settle_frames=frames, overlays=False)

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

def occ_zero(path):
    b = shadow_fixtures.Builder("A3.6 ground fixture")
    white = b.material("White", double_sided=True)
    b.quad("Occluder", (0.0, HEIGHT, 0.0), (2.0, 2.0), white)
    b.box("Below", (3.5, -1.5, -0.5), (4.5, -0.5, 0.5), b.material("Below"))
    return b.write(path)


def base_fixture(path):
    b = shadow_fixtures.Builder("A3.6 ground base fixture")
    b.quad("Occluder", (0.0, HEIGHT, 0.0), (2.0, 2.0), b.material("White", double_sided=True))
    # Local box x in [-0.5, 0.5]; turned 90 degrees about Z it spans world y 0.7 + [-0.5, 0.5]: lowest point 0.2.
    index = b.box("Turned", (-0.5, -0.2, -0.1), (0.5, 0.2, 0.1), b.material("Turned"))
    b.nodes[index]["rotation"] = [0.0, 0.0, math.sqrt(0.5), math.sqrt(0.5)]
    b.nodes[index]["translation"] = [3.0, 0.7, -3.0]
    # Buried and invisible (MSFS collision geometry): never part of the model's base.
    b.box("Collision", (-6.0, -0.8, 5.0), (-5.0, -0.6, 6.0),
          b.material("Collision", extensions={"ASOBO_material_invisible": {"enabled": True}}))
    return b.write(path)


def pole(path):
    b = shadow_fixtures.Builder("A3.6 ground pole fixture")
    b.box("Pole", (-0.5, 0.0, -0.5), (0.5, 10.0, 0.5), b.material("Pole"))
    return b.write(path)


def view_flags(black, extra=()):
    return black + ["--clear-color", "0,0,0", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
                    "--tone-mapping", "neutral", "--hide-grid"] + list(extra)


def plane_flags(*extra, height="zero"):
    return ["--ground", "plane", "--ground-height", height, "--ground-color", f"{ALBEDO},{ALBEDO},{ALBEDO}",
            "--ground-roughness", str(ROUGHNESS), "--ground-detail", "0", "--ground-joints", "0",
            "--ground-haze", "1e9"] + list(extra)


def sun_flags(azimuth=SUN_AZ, elevation=SUN_EL, intensity=SUN_I):
    return ["--sun", "on", "--sun-intensity", str(intensity), "--sun-dir", f"{azimuth},{elevation}"]


TOP = ["--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "60", "--camera-distance", "14"]


# ---------------------------------------------------------------------------------------------
# Reference BRDF (pbr.frag / brdf.glsl terms for the ground's dielectric, F0 0.04)
# ---------------------------------------------------------------------------------------------

def sun_term(albedo, roughness, sun, view_dir, intensity, angular_radius_deg=0.27):
    """Linear radiance of a plane (normal +Y) lit by the sun only: E N.L ((1 - F) albedo / pi + F Vis D)."""
    n = (0.0, 1.0, 0.0)
    dot = lambda a, b: sum(p * q for p, q in zip(a, b))  # noqa: E731
    norm = lambda a: [c / math.sqrt(dot(a, a)) for c in a]  # noqa: E731
    h = norm([s + v for s, v in zip(sun, view_dir)])
    n_l, n_v, n_h, v_h = max(dot(n, sun), 0.0), max(dot(n, view_dir), 0.0), max(dot(n, h), 0.0), max(dot(view_dir, h), 0.0)
    fresnel = 0.04 + 0.96 * (1.0 - v_h) ** 5
    alpha = min(roughness * roughness + 0.5 * math.tan(math.radians(angular_radius_deg)), 1.0)
    a2 = alpha * alpha
    ggx_v = n_l * math.sqrt(n_v * n_v * (1.0 - a2) + a2)
    ggx_l = n_v * math.sqrt(n_l * n_l * (1.0 - a2) + a2)
    vis = 0.5 / (ggx_v + ggx_l) if ggx_v + ggx_l > 0 else 0.0
    f = n_h * n_h * (a2 - 1.0) + 1.0
    d = a2 / (math.pi * f * f)
    return intensity * n_l * ((1.0 - fresnel) * albedo / math.pi + fresnel * vis * d)


def view_vector(report, point):
    eye = report["camera"]["eye"]
    d = [e - p for e, p in zip(eye, point)]
    n = math.sqrt(sum(c * c for c in d))
    return [c / n for c in d]


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_baseline(baseline, black):
    print("[Grid is byte-identical to the baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary")
        return
    a, _ = render(None, "base_hellocube", exe=baseline)
    b, _ = render(None, "cur_hellocube")
    c, _ = render(None, "cur_hellocube_grid", ["--ground", "grid"])
    check(a is not None and same(a, b), f"HelloCube: the default ground is byte-identical to {baseline.stem}")
    check(b is not None and same(b, c), "HelloCube: --ground grid equals the default")
    for name in ("exterior_fuselage_nose", "cockpit_main_displays"):
        entry = view(name)
        a = render_view(baseline, entry, f"base_{name}", folder=OUT)
        b = render_view(EXE, entry, f"cur_{name}", folder=OUT)
        check(a is not None and same(a, b), f"{name}: byte-identical to {baseline.stem}")
    entry = view("exterior_fuselage_nose")
    a = render_view(baseline, entry, "base_exterior_sun", sun_flags(), folder=OUT)
    b = render_view(EXE, entry, "cur_exterior_sun_grid", sun_flags() + ["--ground", "grid"], folder=OUT)
    check(a is not None and same(a, b), f"exterior_fuselage_nose with the sun on (shadows): byte-identical to {baseline.stem}")
    # With the plane selected, debug views and wireframe keep the sky and the grid: they equal the Grid render.
    for label, flags in (("debug view base colour", ["--debug-view", "basecolor"]), ("wireframe", ["--render-mode", "wireframe"])):
        a, _ = render(None, f"mode_grid_{label[:5]}", ["--ground", "grid"] + flags)
        b, _ = render(None, f"mode_plane_{label[:5]}", ["--ground", "plane"] + flags)
        check(a is not None and same(a, b), f"{label}: --ground plane renders like Grid (the plane is drawn in shaded views only)")


def check_shadow_edges(black, zero, base):
    print("[shadow edge on the plane at the analytic position]")
    cases = (
        # (fixture, label, plane flags, plane Y, azimuth, elevation, line start/end along x or z at the plane)
        (zero, "Y = 0", plane_flags(), 0.0, 90.0, 45.0, ("x", -4.0, -1.2, 0.5)),
        (zero, "Y = 0", plane_flags(), 0.0, 0.0, 30.0, ("z", -5.0, -2.3, 0.5)),
        (base, "Model base", plane_flags(height="model-base"), BASE, 90.0, 45.0, ("x", -4.0, -1.2, 0.5)),
    )
    for model, label, pflags, plane_y, az, el, line in cases:
        flags = view_flags(black) + TOP + sun_flags(az, el) + pflags
        name = f"edge_{label.replace(' ', '_')}_az{int(az)}_el{int(el)}"
        png, report = render(model, name, flags)
        if png is None:
            check(False, f"{name} rendered")
            continue
        cam = Camera(report)
        img = linear(png)
        axis, a, b, fixed = line
        height = HEIGHT - plane_y
        if axis == "x":
            p0, p1 = (a, plane_y, fixed), (b, plane_y, fixed)
            expected = (-1.0 - height / math.tan(math.radians(el)), plane_y, fixed)
        else:
            p0, p1 = (fixed, plane_y, a), (fixed, plane_y, b)
            expected = (fixed, plane_y, -1.0 - height / math.tan(math.radians(el)))
        point, lo, hi = edge_crossing(img, cam, p0, p1)
        if point is None:
            check(False, f"{label} az {az:.0f} el {el:.0f}: an edge was found (shadow {lo:.4f}, lit {hi:.4f})")
            continue
        ex, ey = cam.project(expected)
        mx, my = cam.project(point)
        distance = math.hypot(mx - ex, my - ey)
        check(distance <= EDGE_TOLERANCE_PX and lo < 0.01,
              f"{label} az {az:.0f} el {el:.0f}: shadow edge {math.dist(point, expected) * 100:.1f} cm / {distance:.2f} px from "
              f"the analytic one (<= {EDGE_TOLERANCE_PX} px; plane Y {plane_y}; shadow {lo:.4f}, lit {hi:.4f})")


def check_tall_shadow(black):
    print("[shadow of a tall pole reaches far across the plane]")
    model = pole(OUT / "pole.gltf")
    # The camera stands on the sun side (+X) and 19 m to the side, 40 m from the target, looking along the shadow:
    # its depth range is the pole's alone (about 22 m) unless the cascades are also cut to the model's footprint
    # on the plane, which reaches 27 m from the pole.
    flags = view_flags(black) + ["--camera-target", "-12,0,0", "--camera-yaw", "300", "--camera-pitch", "20",
                                 "--camera-distance", "40"] + sun_flags(90.0, 20.0) + plane_flags()
    png, report = render(model, "pole", flags)
    if png is None:
        check(False, "pole rendered")
        return
    cam = Camera(report)
    img = linear(png)
    lit = sample(img, cam, (-12.0, 0.0, 5.0))
    for x in (-12.0, -24.0):
        # The line of sight to the probed points must not pass through the pole itself.
        eye = report["camera"]["eye"]
        blocked = any(abs(eye[0] + (x - eye[0]) * t) < 0.5 and abs(eye[2] + (0.0 - eye[2]) * t) < 0.5
                      and 0.0 < eye[1] * (1.0 - t) < 10.0 for t in (i / 400.0 for i in range(401)))
        check(not blocked, f"x = {x:.0f} m: the pole does not hide the probed point from the camera")
        inside = sample(img, cam, (x, 0.0, 0.0))
        outside = sample(img, cam, (x, 0.0, 4.0))
        check(inside is not None and outside is not None and inside < 0.01 and outside > 0.05,
              f"x = {x:.0f} m from the pole: shadowed {inside if inside is None else round(inside, 4)}, "
              f"lit beside it {outside if outside is None else round(outside, 4)}")
    check(lit is not None and lit > 0.05, "the plane is lit away from the shadow")


def check_probe_values(black, zero):
    print("[control pipe: plane value, occlusion, picking]")
    flags = view_flags(black) + TOP + sun_flags() + plane_flags()
    png, report = render(zero, "probe_ref", flags)
    if png is None:
        check(False, "probe reference rendered")
        return
    cam = Camera(report)
    sun = sun_vector(SUN_AZ, SUN_EL)
    points = {"lit": (-3.5, 0.0, 0.5), "lit far": (3.0, 0.0, 2.5), "inside shadow": (-1.75, 0.0, 0.5),
              "over the buried box": (4.0, 0.0, 0.0), "occluder": (0.5, HEIGHT, 0.5)}
    pixels = {name: tuple(int(round(c)) for c in cam.project(p)) for name, p in points.items()}
    try:
        session = Session(zero, flags)
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        session.settle()
        probed = {name: session.call("probe", x=x, y=y) for name, (x, y) in pixels.items()}
        for name in ("lit", "lit far"):
            p = points[name]
            expected = sun_term(ALBEDO, ROUGHNESS, sun, view_vector(report, p), SUN_I)
            measured = probed[name]["hdrLuminance"]
            check(abs(measured - expected) <= PROBE_REL * expected + PROBE_ABS,
                  f"{name}: probe {measured:.4f} = albedo x sun term {expected:.4f} (<= {PROBE_REL:.0%} + {PROBE_ABS})")
        inside = probed["inside shadow"]["hdrLuminance"]
        check(inside < 1e-3, f"inside the shadow: {inside:.2e} (black environment: 0)")
        buried = probed["over the buried box"]["hdrLuminance"]
        expected = sun_term(ALBEDO, ROUGHNESS, sun, view_vector(report, points["over the buried box"]), SUN_I)
        box_top = SUN_I * sun[1] / math.pi
        check(abs(buried - expected) <= PROBE_REL * expected + PROBE_ABS and abs(buried - box_top) > 0.05,
              f"the plane occludes the box buried below it: {buried:.4f} (plane {expected:.4f}, box top {box_top:.4f})")
        quad = probed["occluder"]
        pick = quad["pick"]
        check(isinstance(pick, dict) and pick.get("node", pick.get("name")) in ("Occluder",) or "Occluder" in json.dumps(pick),
              f"the occluder occludes the plane (pixel picks {pick if pick is None else 'Occluder'}, value {quad['hdrLuminance']:.3f})")
        check(abs(quad["hdrLuminance"] - SUN_I * sun[1] / math.pi) <= 0.02 * SUN_I * sun[1] / math.pi + PROBE_ABS,
              f"occluder pixel shows the quad (lit white Lambert {SUN_I * sun[1] / math.pi:.4f}), not the plane")
        for name in ("lit", "lit far", "inside shadow", "over the buried box"):
            pick = probed[name]["pick"]
            if name == "over the buried box":
                continue  # picking ignores the plane, so the buried box is still pickable through it
            check(pick is None, f"{name}: a click on the plane picks nothing (background): {pick}")
        state = session.call("get_state")["ground"]
        check(state["mode"] == "plane" and state["active"] and abs(state["height"]) < 1e-6,
              f"get_state ground: {state['mode']}, active, height {state['height']}")
        errors = session.close()
        check(errors == 0, f"pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session failed: {error}")
        session.process.kill()


def check_environment(black, zero, tmp):
    print("[IBL: uniform environment, white furnace, additivity]")
    level = 0.5
    uniform = ["--env", str(write_hdr(tmp / "uniform.hdr", 64, 32, lambda x, y: (level, level, level)))]
    base = ["--clear-color", "0,0,0", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
            "--tone-mapping", "neutral", "--hide-grid"] + TOP
    results = {}
    for label, env, sun, albedo in (("furnace", uniform, False, 1.0), ("quarter", uniform, False, 0.25),
                                    ("env+sun", uniform, True, 0.5), ("env", uniform, False, 0.5),
                                    ("sun", black, True, 0.5)):
        flags = env + base + (sun_flags() if sun else []) + plane_flags()
        flags += ["--ground-color", f"{albedo},{albedo},{albedo}"]
        point = (-3.5, 0.0, 0.5)
        png, report = render(zero, f"env_{label.replace('+', '_')}", flags)
        if png is None:
            check(False, f"{label} rendered")
            return
        cam = Camera(report)
        results[label] = sample(linear(png), cam, point)
    check(abs(results["furnace"] - level) <= FURNACE_TOLERANCE * level,
          f"white furnace: albedo 1 plane {results['furnace']:.4f} = environment {level} (<= {FURNACE_TOLERANCE:.0%})")
    lo, hi = 0.25 * level, 0.25 * level + 0.75 * 0.12 * level
    check(lo - 0.01 <= results["quarter"] <= hi + 0.01,
          f"albedo 0.25: {results['quarter']:.4f} within [{lo:.4f}, {hi:.4f}] (albedo x L + specular share)")
    total = results["env"] + results["sun"]
    check(abs(results["env+sun"] - total) <= ADDITIVE_TOLERANCE * total + 3e-3,
          f"IBL + sun add: {results['env+sun']:.4f} = {results['env']:.4f} (IBL) + {results['sun']:.4f} (sun)")


def check_state_and_settings(black, zero):
    print("[ground state, registry keys, flags]")
    try:
        session = Session(zero, view_flags(black) + TOP + ["--ground", "plane", "--ground-preset", "grass"])
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        state = session.call("get_state")["ground"]
        check(state["preset"] == "grass" and abs(state["albedo"][1] - 0.17) < 1e-6,
              f"--ground-preset grass: albedo {state['albedo']} (the average albedo A6.2 reads)")
        session.call("set_viewport_setting", key="ground_preset", value="sand")
        state = session.call("get_state")["ground"]
        check(state["preset"] == "sand" and abs(state["albedo"][0] - 0.52) < 1e-6, f"preset sand: {state['albedo']}")
        session.call("set_viewport_setting", key="ground_color", value=[0.3, 0.2, 0.1])
        state = session.call("get_state")["ground"]
        check(state["preset"] == "custom" and abs(state["albedo"][2] - 0.1) < 1e-6, f"ground_color marks the preset custom: {state['albedo']}")
        session.call("set_viewport_setting", key="ground_height_mode", value="custom")
        session.call("set_viewport_setting", key="ground_height", value=2.5)
        session.settle()
        state = session.call("get_state")["ground"]
        check(state["heightMode"] == "custom" and abs(state["height"] - 2.5) < 1e-6, f"custom height 2.5: {state['height']}")
        session.call("set_viewport_setting", key="ground_height_mode", value="model_base")
        session.settle()
        state = session.call("get_state")["ground"]
        check(abs(state["height"] + 1.5) < 1e-4 and abs(state["modelBase"] + 1.5) < 1e-4,
              f"model base of the fixture (its buried box): {state['modelBase']}")
        session.call("set_viewport_setting", key="ground", value="grid")
        session.settle()
        state = session.call("get_state")["ground"]
        check(state["mode"] == "grid" and not state["active"], "ground grid: the plane is inactive")
        listed = {s["key"] for s in session.call("list_settings", scope="viewport")["settings"]}
        wanted = {"ground", "ground_height_mode", "ground_height", "ground_preset", "ground_color", "ground_roughness",
                  "ground_detail", "ground_detail_scale", "ground_joint_spacing", "ground_haze", "ground_grid"}
        check(wanted <= listed, f"list_settings lists the ground settings ({sorted(wanted - listed) or 'all'})")
        errors = session.close()
        check(errors == 0, f"pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session failed: {error}")
        session.process.kill()
    for bad in (["--ground", "lake"], ["--ground-preset", "lava"], ["--ground-height", "sideways"], ["--ground-roughness", "2"]):
        result = run([EXE, zero] + COMMON + bad + ["--exit-after-dump"], timeout=120)
        check(result.returncode != 0, f"{' '.join(bad)} is rejected")


def without_ground_equipment(source, destination):
    """Copy of the E195 exterior without its stairs and barrier (ground support equipment standing 1 m below the
    nose gear in the file): the model's lowest point is then the nose tyre."""
    doc = json.loads(Path(source).read_text(encoding="utf-8"))
    drop = {i for i, node in enumerate(doc["nodes"]) if node.get("name") in ("Barier_ribbon", "FR_Stair", "RR_Stair")}
    for node in doc["nodes"]:
        if "children" in node:
            node["children"] = [c for c in node["children"] if c not in drop]
    for scene in doc["scenes"]:
        scene["nodes"] = [n for n in scene["nodes"] if n not in drop]
    Path(destination).write_text(json.dumps(doc), encoding="utf-8")
    return Path(destination)


def check_wheels(black):
    print("[wheels touch the plane (E195 nose gear, Model base)]")
    from validate_asobo_common import REAL_ASSETS
    exterior = REAL_ASSETS.get("Exterior")
    if not exterior or not Path(exterior).exists() or Path(exterior).suffix != ".gltf" or not Path(exterior).is_file():
        print("  SKIP E195 exterior not available")
        return
    model = without_ground_equipment(exterior, OUT / "e195_no_gse.gltf")
    flags = ["--ground", "plane", "--sun", "on", "--sun-dir", "60,35", "--camera-yaw", "-35", "--camera-pitch", "4",
             "--camera-distance", "5", "--camera-target", "0,-1.8,9.8", "--hide-grid"]
    png, report = render(model, "wheels", flags)
    if png is None:
        check(False, "wheel view rendered")
        return
    try:
        session = Session(model, flags)
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        session.settle()
        state = session.call("get_state")["ground"]
        base = state["height"]
        rect = session.call("node_screen_rect", node="LDG_GEAR_FWD_TYRE")
        x0, y0, x1, y1 = rect["screenRect"]
        height = session.call("get_state")["viewports"][0]["height"]
        lowest = None
        for column in range(int(x0 + 0.35 * (x1 - x0)), int(x0 + 0.66 * (x1 - x0)) + 1, 2):
            for row in range(int(min(y1 + 30, height - 1)), int(max(y0, 0)), -1):
                pick = session.call("probe", x=column, y=row)
                if pick["pick"] and "TYRE" in json.dumps(pick["pick"]) and pick.get("world") is not None:
                    if lowest is None or pick["world"][1] < lowest["world"][1]:
                        lowest = pick
                    break
        if lowest is None:
            check(False, "found a tyre pixel")
        else:
            gap = lowest["world"][1] - base
            check(-0.01 <= gap <= WHEEL_TOLERANCE_M,
                  f"nose tyre bottom {lowest['world'][1]:.4f} m vs plane {base:.4f} m (model base): gap {gap * 100:.1f} cm "
                  f"(<= {WHEEL_TOLERANCE_M * 100:.0f} cm)")
        errors = session.close()
        check(errors == 0, f"pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session failed: {error}")
        session.process.kill()


def check_grid_on_ground(black, zero):
    print("[grid on the ground]")
    common = view_flags(black)[:-1]  # keep the grid (no --hide-grid)
    common = common + ["--camera-target", "0,1,0", "--camera-yaw", "30", "--camera-pitch", "55",
                                    "--camera-distance", "10"]
    plane = ["--ground", "plane", "--ground-height", "1.0", "--ground-color", "0.5,0.5,0.5", "--ground-detail", "0",
             "--ground-joints", "0", "--ground-haze", "1e9"]
    off, report = render(zero, "gridon_off", common + plane + ["--ground-grid", "off"])
    on, _ = render(zero, "gridon_on", common + plane + ["--ground-grid", "on"])
    if off is None or on is None:
        check(False, "grid on ground renders")
        return
    import numpy as np
    a, b = load(off), load(on)
    cam = Camera(report)
    changed = (np.abs(a - b).max(axis=2) > 12)
    fraction = changed.mean()
    check(0.002 < fraction < 0.5, f"--ground-grid on changes {fraction:.1%} of the pixels (grid lines), off draws none")
    # The Z axis (x = 0) lies on the plane at Y = 1: its pixels change; the quad's own pixels are not what changed.
    hits = 0
    for z in (-3.0, -2.0, 2.0, 3.0, 4.0):
        x, y = cam.project((0.0, 1.0, z))
        xi, yi = int(round(x)), int(round(y))
        if 0 <= xi < cam.width and 0 <= yi < cam.height and changed[max(yi - 1, 0):yi + 2, max(xi - 1, 0):xi + 2].any():
            hits += 1
    check(hits >= 3, f"the Z axis line is drawn at the plane's height Y = 1 ({hits} of 5 sample points on it changed)")


def check_stability(black, zero):
    print("[stability: TAA frames, far-field distance fade]")
    import numpy as np
    tmp = OUT
    low = ["--camera-target", "0,0,0", "--camera-yaw", "20", "--camera-pitch", "5", "--camera-distance", "25", "--hide-grid"]
    plane = ["--ground", "plane", "--ground-height", "zero", "--sun", "on", "--sun-dir", "50,40"]
    a, report = render(zero, "taa_64", low + plane + ["--aa", "taa"], frame=64, common=[c for c in COMMON if c not in ("--aa", "none")])
    b, _ = render(zero, "taa_96", low + plane + ["--aa", "taa"], frame=96, common=[c for c in COMMON if c not in ("--aa", "none")])
    if a is None or b is None:
        check(False, "TAA frames rendered")
        return
    cam = Camera(report)
    ny = -cam.f[1] / cam.u[1]
    horizon = int(round((1.0 - ny / cam.t) / 2.0 * cam.height - 0.5))
    check(0 < horizon < cam.height - 100, f"horizon row {horizon} of {cam.height}")
    ia, ib = load(a), load(b)
    diff = np.abs(ia - ib).max(axis=2)
    # The side thirds of the image: the occluder fixture and its shadow stay in the middle.
    side = np.zeros(diff.shape[1], dtype=bool)
    side[:int(0.3 * diff.shape[1])] = True
    side[int(0.7 * diff.shape[1]):] = True
    ground = diff[horizon + 8:, side]
    band = diff[max(horizon - 6, 0):horizon + 8, side]
    check(ground.max() <= TAA_LEVELS, f"TAA frames 64 / 96: ground differs by at most {ground.max():.0f} levels (<= {TAA_LEVELS}), mean {ground.mean():.3f}")
    check(band.max() <= TAA_LEVELS, f"TAA frames 64 / 96: horizon band differs by at most {band.max():.0f} levels (<= {TAA_LEVELS})")
    # Far field without AA: rows from 6 px to 12% of the image below the horizon.
    c, _ = render(zero, "far_a", low + plane)
    e, _ = render(zero, "far_c", ["--camera-target", "0,0,0", "--camera-yaw", "20.03", "--camera-pitch", "5",
                                  "--camera-distance", "25", "--hide-grid"] + plane)
    if c is None or e is None:
        check(False, "far-field renders")
        return
    lum = lambda p: (load(p) @ np.array(LUMA))  # noqa: E731
    far = slice(horizon + 6, horizon + int(0.12 * cam.height))
    la, lc = lum(c)[:, side], lum(e)[:, side]
    from numpy.lib.stride_tricks import sliding_window_view
    window = sliding_window_view(la[far, :], (5, 5)).mean(axis=(2, 3))
    high_pass = la[far, :][2:-2, 2:-2] - window
    check(float(high_pass.std()) <= FAR_HF_LEVELS,
          f"far field: high-frequency std {float(high_pass.std()):.3f} levels (<= {FAR_HF_LEVELS}; details fade to the average)")
    shimmer = float(np.abs(la[far, :] - lc[far, :]).mean())
    check(shimmer <= FAR_SHIMMER_LEVELS, f"far field: a 0.03 degree turn changes it by {shimmer:.3f} levels on average (<= {FAR_SHIMMER_LEVELS})")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", default=None)
    parser.add_argument("--only", default=None, help="comma-separated: baseline,edges,tall,probe,env,state,wheels,grid,stability")
    args = parser.parse_args()
    only = set(args.only.split(",")) if args.only else None

    def wanted(name):
        return only is None or name in only
    baseline = Path(args.baseline_exe) if args.baseline_exe else None
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None
    OUT.mkdir(parents=True, exist_ok=True)
    black = ["--env", str(write_hdr(OUT / "black.hdr", 64, 32, lambda x, y: (0.0, 0.0, 0.0)))]
    zero = occ_zero(OUT / "occ_zero.gltf")
    base = base_fixture(OUT / "base.gltf")
    if wanted("baseline"):
        check_baseline(baseline, black)
    if wanted("edges"):
        check_shadow_edges(black, zero, base)
    if wanted("tall"):
        check_tall_shadow(black)
    if wanted("probe"):
        check_probe_values(black, zero)
    if wanted("env"):
        check_environment(black, zero, OUT)
    if wanted("state"):
        check_state_and_settings(black, zero)
    if wanted("wheels"):
        check_wheels(black)
    if wanted("grid"):
        check_grid_on_ground(black, zero)
    if wanted("stability"):
        check_stability(black, zero)
    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in {len(gl_logs)} renders {bad}")
    return finish()


if __name__ == "__main__":
    sys.exit(main())
