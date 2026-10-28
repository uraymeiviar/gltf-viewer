# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.2: cascaded shadow maps for the A3.1 sun (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.2).

Fixtures (scripts/shadow_fixtures.py): a 16 x 16 m Lambert receiver plane at y = 0 with occluders, under a black
environment map (IBL contributes nothing), black clear colour, PBR Neutral, exposure 0, no bloom, no AA unless
stated. A lit plane is colour x intensity x N.L / pi, a fully shadowed one 0.
Checks:
  * sun off (the default) is byte-identical to the baseline binary (--baseline-exe): HelloCube, the basic
    fixture and two E195 ASOBO reference views (cockpit and exterior); sun on with --shadows off is
    byte-identical to the baseline's A3.1 sun-on render (HelloCube, the basic fixture, the E195 exterior);
  * edge position: a 2 x 2 m occluder quad 1.5 m above the plane; for sun az 90 / el 45 and az 0 / el 30 the
    shadow edge (the 50% crossing of the linear luminance along a line across it) lies within EDGE_TOLERANCE_PX
    of the projection of the analytic edge (the occluder edge moved by height / tan(elevation) away from the sun);
  * probe over the control pipe: inside the shadow, on the penumbra and outside, the linear value equals the
    Shadow Visibility debug view's value x the unshadowed (--shadows off) value (sun term removed in the
    shadow: 0 under the black environment; unchanged outside), within PROBE_REL x expected + PROBE_ABS;
  * grazing sun (elevation 5 deg, a 1 m cube on the plane): no acne on the lit plane and on the lit cube face
    (coefficient of variation below ACNE_CV, mean within ACNE_MEAN of --shadows off) and no peter-panning: plane
    points beside the shadow-side contact line are shadowed from PETER_PAN_TOLERANCE_M on;
  * casters: noCastShadow (ASOBO_material_shadow_options), BLEND and ASOBO_material_invisible quads cast
    nothing (their shadow footprints match --shadows off); a MASK checker quad casts its cut-out pattern
    (>= MASK_MATCH of the 64 cell centres shadowed exactly where the texture is opaque); with
    --asobo-disable ASOBO_material_shadow_options the noCastShadow quad casts;
  * camera motion: moving the camera by 1 mm (sub-texel) keeps every cascade's texel grid (light-space origin
    a whole number of texels, identical radius) and changes the Shadow Visibility image by at most
    SHIMMER_LEVELS levels (no shimmer); TAA: frames 64 and 96 of a TAA render differ by at most TAA_LEVELS;
  * skinned caster (Fox.glb with a ground plane, two --anim-time poses): with an orthographic camera looking
    along the sun, no shadow is visible around the posed fox (the shadow hides exactly behind the drawn pose),
    while a sun 25 deg off the view direction shows a pose-dependent shadow;
  * debug views shadowcascades / shadowvisibility, the registry keys and --shadow-* flags, zero GL errors.

Run: python scripts/validate_shadows.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import json
import math
import os
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
import shadow_fixtures  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same, view, render_view  # noqa: E402
from validate_asobo_sss import write_hdr  # noqa: E402
from validate_sun import Camera, load, untonemap, LUMA, gl_errors_zero  # noqa: E402

OUT = REPO / "build" / "shadows"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
FOX = REPO / "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
HEIGHT = 1.5                 # occluder height of the basic fixture (m)
EDGE_TOLERANCE_PX = 2.0      # measured edge against the analytic edge (normal offset + filter, at ~40 px/m)
PROBE_REL, PROBE_ABS = 0.02, 2e-3
ACNE_CV = 0.01               # coefficient of variation of a lit flat region
ACNE_MEAN = 0.02             # its mean against the unshadowed render (relative)
PETER_PAN_TOLERANCE_M = 0.02  # plane points this far from the contact line must already be in shadow
MASK_MATCH = 0.9
SHIMMER_LEVELS = 8           # max 8-bit change of the visibility image under a 1 mm camera move
TAA_LEVELS = 8               # max 8-bit change of the shadow-edge region between two converged TAA frames
FOX_OUTLINE_PX = 2           # PCF footprint at the fox's silhouette (texel ~0.5 px here)
GRAZING_INTENSITY = 2.3      # the lit cube face (N.L = cos 5 deg) stays below PBR Neutral's 0.76 knee

gl_logs = []


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
    """Image position (px) and world point where the linear luminance along p0 -> p1 crosses the midpoint
    between its darkest and brightest samples."""
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

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

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
# Checks
# ---------------------------------------------------------------------------------------------

def check_baseline(baseline, black, basic):
    print("[byte-identical to the baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary")
        return
    view_flags = black + ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off",
                          "--exposure", "0", "--tone-mapping", "neutral", "--camera-target", "0,0,0", "--camera-yaw", "0",
                          "--camera-pitch", "60", "--camera-distance", "14"]
    sun = ["--sun", "on", "--sun-intensity", "1.8", "--sun-dir", "90,45"]
    for label, model, flags in (("hellocube", None, []), ("basic", basic, view_flags)):
        a, _ = render(model, f"base_{label}_off", flags, exe=baseline)
        b, _ = render(model, f"cur_{label}_off", flags)
        check(a is not None and same(a, b), f"{label}: sun off is byte-identical to {baseline.stem}")
        a, _ = render(model, f"base_{label}_sun", flags + sun, exe=baseline)
        b, _ = render(model, f"cur_{label}_sun_noshadow", flags + sun + ["--shadows", "off"])
        c, _ = render(model, f"cur_{label}_sun_shadow", flags + sun)
        check(a is not None and same(a, b), f"{label}: sun on, --shadows off is byte-identical to {baseline.stem}'s sun-on render")
        if label == "basic":
            check(c is not None and not same(b, c), "basic: sun on with shadows changes the image (the shadow is drawn)")
    for name in ("cockpit_main_displays", "exterior_fuselage_nose"):
        entry = view(name)
        a = render_view(baseline, entry, f"base_{name}", folder=OUT)
        b = render_view(EXE, entry, f"cur_{name}", folder=OUT)
        check(a is not None and same(a, b), f"{name}: sun off is byte-identical to {baseline.stem}")
    entry = view("exterior_fuselage_nose")
    a = render_view(baseline, entry, "base_exterior_sun", sun, folder=OUT)
    b = render_view(EXE, entry, "cur_exterior_sun_noshadow", sun + ["--shadows", "off"], folder=OUT)
    check(a is not None and same(a, b), f"exterior_fuselage_nose: sun on, --shadows off is byte-identical to {baseline.stem}")


def basic_view(black):
    return black + ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
                    "--tone-mapping", "neutral", "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "60",
                    "--camera-distance", "14"]


EDGE_CASES = (
    # (azimuth, elevation, line start, line end, expected edge): the edge of the occluder farthest from the sun,
    # moved by HEIGHT / tan(elevation) away from the sun, seen where the occluder itself does not hide the plane.
    (90.0, 45.0, (-4.0, 0.0, 0.5), (-1.2, 0.0, 0.5), (-1.0 - HEIGHT / math.tan(math.radians(45.0)), 0.0, 0.5)),
    (0.0, 30.0, (0.5, 0.0, -5.0), (0.5, 0.0, -2.3), (0.5, 0.0, -1.0 - HEIGHT / math.tan(math.radians(30.0)))),
)


def check_edges(black, basic):
    print("[shadow edge at the analytic position]")
    reports = {}
    for azimuth, elevation, p0, p1, expected in EDGE_CASES:
        flags = basic_view(black) + ["--sun", "on", "--sun-intensity", "1.8", "--sun-dir", f"{azimuth},{elevation}"]
        png, report = render(basic, f"edge_az{int(azimuth)}_el{int(elevation)}", flags)
        if png is None:
            check(False, f"edge az {azimuth} rendered")
            continue
        reports[(azimuth, elevation)] = (png, report, flags, None)
        cam = Camera(report)
        img = linear(png)
        point, lo, hi = edge_crossing(img, cam, p0, p1)
        lit = 1.8 * sun_vector(azimuth, elevation)[1] / math.pi
        check(lo < 0.01 and abs(hi - lit) <= 0.02 * lit + 2e-3,
              f"az {azimuth:.0f} el {elevation:.0f}: shadow {lo:.4f} (black), lit {hi:.4f} (expected {lit:.4f})")
        if point is None:
            check(False, f"az {azimuth:.0f} el {elevation:.0f}: an edge was found")
            continue
        ex, ey = cam.project(expected)
        mx, my = cam.project(point)
        distance = math.hypot(mx - ex, my - ey)
        world = math.dist(point, expected)
        texel = min(c["texelWorld"] for c in report["sun"]["shadows"]["viewports"][0]["cascades"])
        reports[(azimuth, elevation)] = (png, report, flags, point)
        check(distance <= EDGE_TOLERANCE_PX,
              f"az {azimuth:.0f} el {elevation:.0f}: edge at ({point[0]:.3f}, {point[2]:.3f}) m, {world * 100:.1f} cm / "
              f"{distance:.2f} px from the analytic ({expected[0]:.3f}, {expected[2]:.3f}) (<= {EDGE_TOLERANCE_PX} px; "
              f"finest texel {texel * 100:.2f} cm)")
    return reports


def check_probe(black, basic, reports):
    print("[control pipe: probe = visibility x unshadowed value]")
    key = (90.0, 45.0)
    if key not in reports:
        check(False, "edge render available for the probe check")
        return
    _, report, flags, edge = reports[key]
    cam = Camera(report)
    if edge is None:
        check(False, "measured edge available for the probe check")
        return
    points = {"inside": (-1.75, 0.0, 0.5), "penumbra": tuple(edge), "outside": (-3.5, 0.0, 0.5),
              "far side": (3.0, 0.0, 2.0)}
    pixels = {name: tuple(int(round(c)) for c in cam.project(p)) for name, p in points.items()}
    try:
        session = Session(basic, flags)
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        session.call("set_persist", enabled=True)  # shadows is a saved preference; --isolated never writes it
        shaded = {n: session.call("probe", x=x, y=y) for n, (x, y) in pixels.items()}
        session.call("set_viewport_setting", key="render_mode", value="shadowvisibility")
        visibility = {n: session.call("probe", x=x, y=y)["hdrLuminance"] for n, (x, y) in pixels.items()}
        session.call("set_viewport_setting", key="render_mode", value="shaded")
        session.call("set_viewport_setting", key="shadows", value=False)
        unshadowed = {n: session.call("probe", x=x, y=y)["hdrLuminance"] for n, (x, y) in pixels.items()}
        state = session.call("get_state")["sun"]["shadows"]
        check(not state["viewports"][0]["active"] and state["viewports"][0]["memoryBytes"] == 0,
              "shadows off over the pipe: no active shadow map, its memory released")
        session.call("set_viewport_setting", key="shadows", value=True)
        session.call("set_viewport_setting", key="shadow_filter", value="soft")
        soft = session.call("probe", x=pixels["inside"][0], y=pixels["inside"][1])["hdrLuminance"]
        check(soft < 1e-3, f"filter soft over the pipe: inside the shadow still {soft:.2e}")
        session.call("set_viewport_setting", key="shadow_filter", value="pcf")
        for name, result in shaded.items():
            world = result.get("world") or [0, 0, 0]
            off = math.dist(world, points[name])
            measured = result["hdrLuminance"]
            expected = visibility[name] * unshadowed[name]
            check(off < 0.05 and abs(measured - expected) <= PROBE_REL * expected + PROBE_ABS,
                  f"{name}: probe {measured:.4f} = visibility {visibility[name]:.3f} x unshadowed {unshadowed[name]:.4f} "
                  f"(world point {off * 100:.1f} cm from the target)")
        check(visibility["inside"] == 0.0 and shaded["inside"]["hdrLuminance"] < 1e-4,
              "inside: visibility 0, the sun term is removed (black environment: 0)")
        check(visibility["outside"] == 1.0 and abs(shaded["outside"]["hdrLuminance"] - unshadowed["outside"]) <= 1e-6,
              "outside: visibility 1, the value is the unshadowed one")
        check(0.02 < visibility["penumbra"] < 0.98, f"penumbra: partial visibility {visibility['penumbra']:.3f} (filtered edge)")
        listed = {s["key"] for s in session.call("list_settings", scope="renderer")["settings"]}
        wanted = {"shadows", "shadow_resolution", "shadow_cascades", "shadow_distance", "shadow_filter",
                  "shadow_split_lambda", "shadow_normal_bias", "shadow_slope_bias", "shadow_constant_bias"}
        check(wanted <= listed, f"list_settings lists the shadow settings ({sorted(wanted - listed) or 'all'})")
        errors = session.close()
        check(errors == 0, f"pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session failed: {error}")
        session.process.kill()


def check_grazing(black):
    print("[grazing sun, elevation 5 deg: acne and peter-panning]")
    model = shadow_fixtures.contact(OUT / "contact.gltf")
    import numpy as np
    common = black + ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
                      "--tone-mapping", "neutral", "--camera-target", "0,0,0", "--camera-pitch", "30",
                      "--camera-distance", "4", "--sun", "on", "--sun-intensity", str(GRAZING_INTENSITY), "--sun-dir", "90,5"]
    lit_plane = GRAZING_INTENSITY * math.sin(math.radians(5.0)) / math.pi
    lit_face = GRAZING_INTENSITY * math.cos(math.radians(5.0)) / math.pi

    # Acne: seen from the sunlit side (+X), the plane and the cube's sunlit face.
    flags = common + ["--camera-yaw", "-50"]
    png, report = render(model, "grazing_lit", flags)
    ref, _ = render(model, "grazing_lit_noshadow", flags + ["--shadows", "off"])
    if png is None or ref is None:
        check(False, "grazing renders")
        return
    cam = Camera(report)
    img, unshadowed = linear(png), linear(ref)
    grid = [(x, 0.0, z) for x in np.linspace(0.7, 4.0, 34) for z in np.linspace(-1.5, 3.0, 46)]
    face = [(0.5, y, z) for y in np.linspace(0.05, 0.95, 19) for z in np.linspace(-0.45, 0.45, 19)]
    for label, points, level in (("lit plane", grid, lit_plane), ("lit cube face", face, lit_face)):
        pairs = [(sample(img, cam, p), sample(unshadowed, cam, p)) for p in points]
        # Only points where the unshadowed render shows this surface (not hidden, not the background).
        pairs = [(v, u) for v, u in pairs if v is not None and u is not None and abs(u - level) <= max(0.03 * level, 0.005)]
        if len(pairs) < 50:
            check(False, f"{label}: visible samples ({len(pairs)})")
            continue
        values = np.array([v for v, _ in pairs])
        reference = np.array([u for _, u in pairs])
        cv = float(values.std() / max(values.mean(), 1e-9))
        mean_error = abs(values.mean() / reference.mean() - 1.0)
        check(cv <= ACNE_CV and mean_error <= ACNE_MEAN,
              f"{label}: no acne ({len(values)} samples, coefficient of variation {cv:.4f} <= {ACNE_CV}, min "
              f"{values.min():.4f}, mean {values.mean():.4f} vs unshadowed {reference.mean():.4f})")

    # Peter-panning: seen from the shadow side (-X), plane points beside the contact line.
    flags = common + ["--camera-yaw", "50"]
    png, report = render(model, "grazing_contact", flags)
    if png is None:
        check(False, "grazing contact render")
        return
    cam = Camera(report)
    img = linear(png)
    gap, seen = 0.0, 0
    for d in np.linspace(0.002, 0.5, 250):
        for z in (-0.3, 0.0, 0.3):
            v = sample(img, cam, (-0.5 - d, 0.0, z))
            if v is None:
                continue
            seen += 1
            if v > 0.5 * lit_plane:
                gap = max(gap, float(d))
    check(seen > 500 and gap <= PETER_PAN_TOLERANCE_M,
          f"contact line: plane lit at most {gap * 100:.1f} cm from the cube's shadow-side face "
          f"(<= {PETER_PAN_TOLERANCE_M * 100:.0f} cm; {seen} samples; lit level {lit_plane:.3f})")


def check_casters(black):
    print("[casters: noCastShadow, BLEND, invisible, MASK]")
    model = shadow_fixtures.casters(OUT / "casters.gltf", HEIGHT)
    flags = basic_view(black) + ["--sun", "on", "--sun-intensity", "1.8", "--sun-dir", "0,45"]
    flags[flags.index("--camera-pitch") + 1] = "85"
    png, report = render(model, "casters", flags)
    ref, _ = render(model, "casters_noshadow", flags + ["--shadows", "off"])
    honoured, _ = render(model, "casters_shadow_options_off", flags + ["--asobo-disable", "ASOBO_material_shadow_options"])
    if png is None or ref is None or honoured is None:
        check(False, "caster renders")
        return
    import numpy as np
    cam = Camera(report)
    img, unshadowed, ignored = linear(png), linear(ref), linear(honoured)
    shift = HEIGHT / math.tan(math.radians(45.0))  # towards -Z
    lit = 1.8 * math.sin(math.radians(45.0)) / math.pi

    def footprint(x0, image):
        values = []
        for u in np.linspace(0.1, 0.9, 9):
            # v <= 0.75: the far end of the footprint lies under the occluder quad seen from the camera.
            for v in np.linspace(0.1, 0.75, 8):
                value = sample(image, cam, (x0 - 0.75 + 1.5 * u, 0.0, -0.75 + 1.5 * v - shift))
                if value is not None:
                    values.append(value)
        return np.array(values)

    for label, x0 in (("noCastShadow", -3.0), ("BLEND", -1.0), ("invisible", 1.0)):
        shadowed, reference = footprint(x0, img), footprint(x0, unshadowed)
        check(len(shadowed) > 40 and np.abs(shadowed - reference).max() <= 2e-3 and reference.min() > 0.9 * lit,
              f"{label}: casts nothing (footprint max difference {np.abs(shadowed - reference).max():.2e} to --shadows off)")
    cast = footprint(-3.0, ignored)
    check(len(cast) > 40 and cast.max() < 0.05 * lit,
          f"--asobo-disable ASOBO_material_shadow_options: the noCastShadow quad casts (footprint max {cast.max():.4f})")
    matches, total = 0, 0
    for i in range(8):
        for j in range(8):
            u, v = (i + 0.5) / 8, (j + 0.5) / 8
            value = sample(img, cam, (3.0 - 0.75 + 1.5 * u, 0.0, -0.75 + 1.5 * v - shift))
            if value is None:
                continue
            opaque = (i + j) % 2 == 0
            total += 1
            matches += (value < 0.2 * lit) if opaque else (value > 0.8 * lit)
    check(total >= 60 and matches >= MASK_MATCH * total,
          f"MASK checker: {matches}/{total} cell centres shadowed exactly where the texture is opaque (>= {MASK_MATCH:.0%})")


def edge_region(cam):
    """Boolean pixel mask of the plane area around the shadow of the basic fixture (sun az 90 / el 45), away from
    every geometry silhouette: x in [-3.4, -1.2], z in [-1.6, 1.6] on the plane."""
    import numpy as np
    from matplotlib.path import Path as Polygon
    corners = [cam.project(p) for p in ((-3.4, 0.0, -1.6), (-1.2, 0.0, -1.6), (-1.2, 0.0, 1.6), (-3.4, 0.0, 1.6))]
    yy, xx = np.mgrid[0:cam.height, 0:cam.width]
    inside = Polygon(corners).contains_points(np.stack([xx.ravel(), yy.ravel()], axis=1))
    return inside.reshape(cam.height, cam.width)


def light_origin_texels(report):
    result = []
    for cascade in report["sun"]["shadows"]["viewports"][0]["cascades"]:
        x, y = cascade["lightOrigin"]
        result.append((x / cascade["texelWorld"], y / cascade["texelWorld"], cascade["radius"]))
    return result


def check_stability(black, basic):
    print("[camera motion and TAA stability]")
    flags = basic_view(black) + ["--sun", "on", "--sun-intensity", "1.8", "--sun-dir", "90,45", "--debug-view", "shadowvisibility"]
    a, ra = render(basic, "stable_a", flags)
    moved = list(flags)
    moved[moved.index("--camera-target") + 1] = "0.001,0,0.0007"
    b, rb = render(basic, "stable_b", moved)
    if a is None or b is None:
        check(False, "stability renders")
        return
    import numpy as np
    ta, tb = light_origin_texels(ra), light_origin_texels(rb)
    snapped = all(abs(x - round(x)) < 1e-2 and abs(y - round(y)) < 1e-2 for x, y, _ in ta + tb)
    check(snapped and [r for _, _, r in ta] == [r for _, _, r in tb],
          f"1 mm camera move: every cascade origin is a whole number of texels and the radii are unchanged ({len(ta)} cascades)")
    region = edge_region(Camera(ra))
    da, db = load(a)[region], load(b)[region]
    diff = np.abs(da - db).max()
    changed = int((np.abs(da - db).max(axis=1) > 0).sum())
    check(diff <= SHIMMER_LEVELS, f"1 mm camera move: shadow visibility changes by at most {diff:.0f} levels "
                                  f"(<= {SHIMMER_LEVELS}; {changed} px differ)")
    taa = basic_view(black) + ["--sun", "on", "--sun-intensity", "1.8", "--sun-dir", "90,45"]
    taa_common = [c for c in COMMON]
    taa_common[taa_common.index("--aa") + 1] = "taa"
    f1, r1 = render(basic, "taa_64", taa, frame=64, common=taa_common)
    f2, _ = render(basic, "taa_96", taa, frame=96, common=taa_common)
    if f1 is None or f2 is None:
        check(False, "TAA renders")
        return
    full = np.abs(load(f1) - load(f2)).max(axis=2)
    region = edge_region(Camera(r1))
    d = full[region]
    geometry = full[~region].max()
    check(d.max() <= TAA_LEVELS and d.max() <= geometry,
          f"TAA: the shadow edges change by at most {d.max():.0f} levels between frames 64 and 96 (<= {TAA_LEVELS}; mean "
          f"{d.mean():.3f}), no more than the geometry silhouettes ({geometry:.0f} levels)")


def fox_with_ground(path):
    """Fox.glb as a .gltf (embedded buffer) plus a 400 x 400 ground quad (Lambert) under its feet."""
    data = FOX.read_bytes()
    length = struct.unpack("<I", data[12:16])[0]
    doc = json.loads(data[20:20 + length])
    offset = 20 + length
    bin_length = struct.unpack("<I", data[offset:offset + 4])[0]
    blob = data[offset + 8:offset + 8 + bin_length]
    doc["buffers"] = [{"byteLength": len(blob), "uri": "data:application/octet-stream;base64," + base64.b64encode(blob).decode("ascii")}]
    ground = shadow_fixtures.Builder()
    ground.quad("Ground", (0.0, 0.0, 0.0), (400.0, 400.0), ground.material("Ground"))
    buffer_index = len(doc["buffers"])
    view_base, accessor_base = len(doc.get("bufferViews", [])), len(doc.get("accessors", []))
    for v in ground.views:
        doc["bufferViews"].append(dict(v, buffer=buffer_index))
    for a in ground.accessors:
        doc["accessors"].append(dict(a, bufferView=a["bufferView"] + view_base))
    doc["buffers"].append({"byteLength": len(ground.blob),
                           "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(ground.blob)).decode("ascii")})
    material_index = len(doc.setdefault("materials", []))
    doc["materials"].append(ground.materials[0])
    primitive = ground.meshes[0]["primitives"][0]
    primitive = {"attributes": {k: v + accessor_base for k, v in primitive["attributes"].items()},
                 "indices": primitive["indices"] + accessor_base, "material": material_index}
    doc.setdefault("meshes", []).append({"name": "Ground", "primitives": [primitive]})
    doc["nodes"].append({"name": "Ground", "mesh": len(doc["meshes"]) - 1})
    doc["scenes"][doc.get("scene", 0)]["nodes"].append(len(doc["nodes"]) - 1)
    doc["extensionsUsed"] = sorted(set(doc.get("extensionsUsed", [])) | {"KHR_materials_specular"})
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def check_skinned(black):
    print("[skinned caster: Fox.glb, two poses]")
    if not FOX.exists():
        print("  SKIP Fox.glb not found")
        return
    model = fox_with_ground(OUT / "fox_ground.gltf")
    import numpy as np
    base = black + ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
                    "--tone-mapping", "neutral", "--camera-target", "0,30,0", "--camera-distance", "260",
                    "--camera-yaw", "35", "--camera-pitch", "40", "--camera-projection", "orthographic",
                    "--sun", "on", "--sun-intensity", "2", "--shadow-distance", "2000"]  # the fox is in centimetres
    shading = base
    base = base + ["--debug-view", "shadowvisibility"]
    # The camera's direction gives the sun direction that hides every shadow behind the drawn fox.
    _, probe = render(model, "fox_camera", base + ["--anim-time", "0.5"])
    if probe is None:
        check(False, "fox render")
        return
    eye, target = probe["camera"]["eye"], probe["camera"]["target"]
    d = [e - t for e, t in zip(eye, target)]
    n = math.sqrt(sum(c * c for c in d))
    d = [c / n for c in d]
    azimuth = math.degrees(math.atan2(d[0], d[2]))
    elevation = math.degrees(math.asin(d[1]))
    areas = {}
    for t in (0.5, 1.3):
        png, report = render(model, f"fox_hidden_t{t}", base + ["--anim-time", str(t), "--sun-dir", f"{azimuth},{elevation}"])
        side, _ = render(model, f"fox_side_t{t}", base + ["--anim-time", str(t), "--sun-dir", f"{azimuth + 25.0},{elevation}"])
        if png is None or side is None:
            check(False, f"fox renders at t {t}")
            continue
        geometry, _ = render(model, f"fox_noshadow_t{t}", base + ["--anim-time", str(t), "--shadows", "off"])
        normals, _ = render(model, f"fox_normals_t{t}", shading + ["--anim-time", str(t), "--debug-view", "geometrynormal"])
        if geometry is None or normals is None:
            check(False, f"fox reference renders at t {t}")
            continue
        mask = load(geometry)[..., 0] > 250
        # The fox's silhouette: every pixel whose geometric normal is not the ground's, grown by FOX_OUTLINE_PX.
        n = load(normals)
        ground = np.median(n[mask].reshape(-1, 3), axis=0)
        fox = (np.abs(n - ground).max(axis=2) > 6) & mask
        grown = fox.copy()
        for dy in range(-FOX_OUTLINE_PX, FOX_OUTLINE_PX + 1):
            for dx in range(-FOX_OUTLINE_PX, FOX_OUTLINE_PX + 1):
                grown |= np.roll(np.roll(fox, dy, axis=0), dx, axis=1)
        visible = load(png)[..., 0]
        shadowed = int(((visible < 128) & mask & ~grown).sum())
        outline = int(((visible < 128) & mask & grown).sum())
        check(int(fox.sum()) > 2000 and shadowed <= 5,
              f"t {t}: sun along the view: {shadowed} shadowed pixels beyond {FOX_OUTLINE_PX} px of the posed fox's "
              f"silhouette ({int(fox.sum())} px; {outline} filtered outline px within it)")
        areas[t] = (load(side)[..., 0] < 128) & mask
    if len(areas) == 2:
        a, b = areas[0.5], areas[1.3]
        changed = int((a != b).sum())
        check(a.sum() > 500 and b.sum() > 500 and changed > 200,
              f"sun 25 deg off the view: shadows of {int(a.sum())} / {int(b.sum())} px, {changed} px differ between the poses")


def check_debug_views(black, basic):
    print("[debug views and settings]")
    flags = basic_view(black) + ["--sun", "on", "--sun-intensity", "1.8", "--sun-dir", "90,45"]
    cascades, report = render(basic, "debug_cascades", flags + ["--debug-view", "shadowcascades"])
    visibility, _ = render(basic, "debug_visibility", flags + ["--debug-view", "shadowvisibility"])
    off, _ = render(basic, "debug_visibility_sunoff", basic_view(black) + ["--debug-view", "shadowvisibility"])
    if cascades is None or visibility is None or off is None:
        check(False, "debug view renders")
        return
    import numpy as np
    img = load(cascades)
    colours = {"red": (img[..., 0] > 200) & (img[..., 1] < 100), "green": (img[..., 1] > 200) & (img[..., 0] < 100),
               "blue": (img[..., 2] > 200) & (img[..., 0] < 120)}
    present = [name for name, mask in colours.items() if mask.sum() > 200]
    check(len(present) >= 2, f"shadowcascades: cascade colours {present} on the plane")
    vis = load(visibility)
    check((vis[..., 0] < 5).sum() > 500 and (vis[..., 0] > 250).sum() > 5000, "shadowvisibility: black shadow, white lit plane")
    check(load(off)[..., 0].min() >= 0 and (load(off)[..., 0] < 250).sum() == (load(off)[..., 0] < 1).sum(),
          "shadowvisibility with the sun off: 1 everywhere on geometry (no shadow)")
    state = report["sun"]["shadows"]
    check(state["resolution"] == 2048 and state["cascadeCount"] == 3 and state["filter"] == "pcf",
          f"defaults: {state['resolution']} x {state['cascadeCount']} cascades, {state['filter']}")
    check(state["viewports"][0]["memoryBytes"] == 2048 * 2048 * 3 * 4, "memory: 2048 x 2048 x 3 x 4 bytes (48 MiB)")
    _, custom = render(basic, "debug_flags", flags + ["--shadow-resolution", "1024", "--shadow-cascades", "4",
                                                      "--shadow-filter", "soft", "--shadow-distance", "50"])
    s = custom["sun"]["shadows"] if custom else {}
    check(custom is not None and s["resolution"] == 1024 and s["cascadeCount"] == 4 and s["filter"] == "soft"
          and abs(s["maxDistance"] - 50.0) < 1e-6 and len(s["viewports"][0]["cascades"]) == 4
          and s["viewports"][0]["memoryBytes"] == 1024 * 1024 * 4 * 4,
          "--shadow-resolution/-cascades/-filter/-distance apply (1024 x 4, soft, 50 m: 16 MiB)")
    bad = run([EXE, basic] + COMMON + ["--shadow-resolution", "3000", "--exit-after-dump"], timeout=120)
    check(bad.returncode != 0, "--shadow-resolution 3000 is rejected")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", default=None)
    parser.add_argument("--only", default=None,
                        help="comma-separated subset: edges,probe,grazing,casters,stability,skinned,debug,baseline")
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
    basic = shadow_fixtures.basic(OUT / "basic.gltf", HEIGHT)
    if wanted("edges") or wanted("probe"):
        reports = check_edges(black, basic)
        if wanted("probe"):
            check_probe(black, basic, reports)
    if wanted("grazing"):
        check_grazing(black)
    if wanted("casters"):
        check_casters(black)
    if wanted("stability"):
        check_stability(black, basic)
    if wanted("skinned"):
        check_skinned(black)
    if wanted("debug"):
        check_debug_views(black, basic)
    if wanted("baseline"):
        check_baseline(baseline, black, basic)
    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in {len(gl_logs)} renders {bad}")
    return finish()


if __name__ == "__main__":
    sys.exit(main())
