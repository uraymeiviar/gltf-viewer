# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.8 viewer ambient light (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.8; header/imgui/SceneEnvironment.h).

The ambient light is a viewer light (never read from or written into the glTF): a uniform radiance
L = colour x luminance / 12,800 cd/m^2 (environment units, both sources) from every direction, added to the IBL terms
of pbr.frag / ground.frag before the occlusion: diffuse c_diff x L, specular L x (F0 A + B) from the BRDF LUT.

Checks:
  * ambient off (the default), HDR and procedural sky: byte-identical to the baseline (HelloCube, two E195 ASOBO views),
    also with non-default ambient colour / intensity while it is off;
  * a white Lambert sphere (specularFactor 0) under a black HDR, lit only by the ambient: probe linear radiance =
    albedo x L within LAMBERT_TOLERANCE on the whole sphere, per channel (colour respected);
  * a smooth metallic sphere: radiance = L x (F0 A + B) within METAL_TOLERANCE; at roughness 0 the LUT integral is
    A = 1 - (1 - N.V)^5, B = (1 - N.V)^5 (Schlick), so the expected value is L x (F0 + (1 - F0)(1 - N.V)^5);
  * occlusion: a uniform 0.5 occlusion texture halves the ambient exactly as it halves the IBL diffuse of a constant
    HDR; with SSAO on, the per-pixel darkening of the ambient equals that of the IBL diffuse (two spheres with a gap);
  * emissive (black base colour) and the background pixels are byte-identical with the ambient on and off;
  * CLI = pipe (set_environment ambient / ambient_color / ambient_intensity): byte-identical captures;
  * the HDR and the procedural sky add the same radiance for the same cd/m^2 (sphere probes, ambient on - off);
  * registry keys listed, invalid values refused, get_state / --profile-report "ambient", zero GL errors.

Run: python scripts/validate_ambient.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import json
import math
import os
import struct
import sys
import time
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same, views, render_view  # noqa: E402
from validate_asobo_sss import write_hdr  # noqa: E402
from validate_sun import gltf, lambert, Camera  # noqa: E402

OUT = REPO / "build" / "ambient"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
VIEW = ["--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0", "--tone-mapping", "neutral",
        "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "4"]
UNIT = 12800.0                 # cd/m^2 per environment unit
INTENSITY = 6400.0             # cd/m^2 used by the radiance checks: L = 0.5 environment units (the maximum is 10,000)
SCALE = INTENSITY / UNIT
LAMBERT_TOLERANCE = 0.01       # ambient-only Lambert radiance against albedo x L (relative, per channel)
METAL_TOLERANCE = 0.02         # ambient-only smooth metal against L x (F0 A + B)
RATIO_TOLERANCE = 0.01         # occlusion ratio of the ambient against that of the IBL diffuse
SOURCE_TOLERANCE = 0.01        # HDR vs procedural sky ambient contribution
SILHOUETTE_NDOTV = 0.2

gl_logs = []


# ---------------------------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------------------------

def write(name, doc):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def png_uri(value):
    """A 4 x 4 grey PNG of 8-bit `value` as a data URI."""
    raw = b"".join(b"\x00" + bytes([value]) * 4 for _ in range(4))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 0, 0, 0, 0)) + \
        chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    return "data:image/png;base64," + base64.b64encode(data).decode("ascii")


def lambert_ao():
    doc = lambert()
    doc["materials"][0]["occlusionTexture"] = {"index": 0, "strength": 1.0}
    doc["textures"] = [{"source": 0, "sampler": 0}]
    doc["samplers"] = [{"magFilter": 9728, "minFilter": 9728}]
    # Occlusion is linear data: 128 / 255 = 0.50196.
    doc["images"] = [{"uri": png_uri(128)}]
    return doc


def metal():
    return gltf({"name": "Metal", "pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.6, 0.3, 1], "metallicFactor": 1.0,
                                                          "roughnessFactor": 0.0}})


def emissive():
    material = {"name": "Emissive", "pbrMetallicRoughness": {"baseColorFactor": [0, 0, 0, 1], "metallicFactor": 0.0,
                                                            "roughnessFactor": 1.0},
                "emissiveFactor": [1.0, 0.5, 0.2], "extensions": {"KHR_materials_specular": {"specularFactor": 0.0}}}
    return gltf(material, None, ["KHR_materials_specular"])


def two_spheres():
    doc = lambert()
    doc["nodes"] = [{"name": "Left", "mesh": 0, "translation": [-1.02, 0, 0]},
                    {"name": "Right", "mesh": 0, "translation": [1.02, 0, 0]}]
    doc["scenes"] = [{"nodes": [0, 1]}]
    return doc


def black_env():
    return ["--env", str(write_hdr(OUT / "black.hdr", 64, 32, lambda x, y: (0.0, 0.0, 0.0)))]


def grey_env():
    return ["--env", str(write_hdr(OUT / "grey.hdr", 64, 32, lambda x, y: (0.25, 0.25, 0.25)))]


# ---------------------------------------------------------------------------------------------
# Glider runs
# ---------------------------------------------------------------------------------------------

def run(args, timeout=900):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(model, name, flags=(), exe=EXE, frame=24, view=True):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    args = [exe] + ([model] if model else []) + COMMON + (VIEW if view else []) + list(flags)
    result = run(args + ["--capture-viewport", png, "--capture-frame", str(frame), "--profile-report", report,
                         "--exit-after-profile"])
    if exe == EXE:
        gl_logs.append((name, result.stdout + result.stderr))
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    return (png if png.exists() else None), data


def gl_errors_zero(log):
    lines = [line for line in log.splitlines() if line.startswith("GL automation errors")]
    return lines == ["GL automation errors: 0"] and "GL error 0x" not in log


def load(png):
    import numpy as np
    from PIL import Image
    return np.asarray(Image.open(png).convert("RGB"), dtype=np.int16)


class Session:
    def __init__(self, model, flags=(), view=True):
        args = [str(EXE), str(model)] + COMMON + (VIEW if view else []) + list(flags) + ["--control-pipe"]
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
        path.unlink(missing_ok=True)
        self.call("capture", path=str(path), settle_frames=frames, overlays=False)
        return path

    def eye(self):
        return self.call("get_camera")["eye"]

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

    def kill(self):
        try:
            self.pipe.close()
        except Exception:  # noqa: BLE001
            pass
        try:
            self.process.kill()
        except Exception:  # noqa: BLE001
            pass


def grid(session, step=25, span=9):
    """Probe positions over the unit sphere at the origin: (x, y, normal, N.V, probe)."""
    state = session.call("get_state")
    width, height = state["viewports"][0]["width"], state["viewports"][0]["height"]
    eye = session.eye()
    out = []
    for gy in range(-span, span + 1):
        for gx in range(-span, span + 1):
            x, y = width // 2 + gx * step, height // 2 + gy * step
            result = session.call("probe", x=x, y=y)
            world = result.get("world")
            if not world:
                continue
            length = math.sqrt(sum(c * c for c in world))
            if abs(length - 1.0) > 0.02:
                continue
            n = [c / length for c in world]
            to_eye = [eye[i] - world[i] for i in range(3)]
            el = math.sqrt(sum(c * c for c in to_eye))
            ndotv = sum(n[i] * to_eye[i] for i in range(3)) / el
            if ndotv < SILHOUETTE_NDOTV:
                continue
            out.append((x, y, n, ndotv, result["hdr"][:3]))
    return out


def rel(a, b):
    return abs(a - b) / max(abs(b), 1e-9)


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_baseline(baseline):
    print("[ambient off: byte-identical to the baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    print(f"  INFO baseline {baseline.name}")
    off_settings = ["--ambient", "off", "--ambient-color", "1,0.2,0.1", "--ambient-intensity", "500"]
    # A1.6b changes the sky's default ASOBO emissive units. Pin the old scale on BOTH binaries to isolate ambient
    # parity; validate_emissive_units covers the corrected sky default independently. A6.4c moved the highlight
    # meter's target, so the average meter is pinned on both as well (validate_night covers the meter).
    sources = (("hdr", []), ("sky", ["--environment", "sky", "--sky-time-slicing", "off",
                                    "--msfs-emissive-scale", "0.001", "--exposure-metering", "average",
                                    "--ambient", "off", "--sky-occlusion", "off"]))
    # A3.10: the pinned baseline supports A3.9; disable sky occlusion and ambient on BOTH sides to isolate off parity.
    current_only = {"hdr": [], "sky": []} # Both binaries support A3.9; pass the identical flags above.
    for source, source_flags in sources:
        for name, flags in (("default", []), ("off_settings", off_settings)):
            now, _ = render(None, f"base_now_hellocube_{source}_{name}", source_flags + flags + current_only[source], view=False)
            before, _ = render(None, f"base_ref_hellocube_{source}_{name}", source_flags + flags, exe=baseline, view=False)
            check(same(now, before), f"HelloCube {source} ({name}): byte-identical to {baseline.stem}")
        for name in ("exterior_fuselage_nose", "cockpit_main_displays"):
            try:
                entry = next(v for v in views() if v["name"] == name)
            except StopIteration:
                print(f"  SKIP reference view {name} not found")
                continue
            now = render_view(EXE, entry, f"base_now_{source}_{name}", source_flags + off_settings + current_only[source],
                              folder=OUT)
            before = render_view(baseline, entry, f"base_ref_{source}_{name}", source_flags + off_settings, folder=OUT)
            if now is None and before is None:
                print(f"  SKIP {name}: E195 asset not available")
                continue
            check(same(now, before), f"E195 {name} {source} (ASOBO), ambient off: byte-identical to {baseline.stem}")


def check_lambert():
    print("[white Lambert sphere lit only by the ambient (black HDR)]")
    model = write("lambert", lambert())
    color = (1.0, 0.5, 0.25)
    session = Session(model, black_env() + ["--ambient", "on", "--ambient-color", ",".join(map(str, color)),
                                            "--ambient-intensity", str(INTENSITY)])
    try:
        session.capture("lambert_ambient")
        samples = grid(session)
        worst = max((rel(p[c], color[c] * SCALE) for _, _, _, _, p in samples for c in range(3)), default=1.0)
        check(len(samples) >= 60, f"{len(samples)} probes on the sphere (N.V >= {SILHOUETTE_NDOTV})")
        check(worst <= LAMBERT_TOLERANCE, f"radiance = albedo x L = {SCALE} x ({color[0]}, {color[1]}, {color[2]}) on the whole "
                                          f"sphere: worst {worst * 100:.3f}% (<= {LAMBERT_TOLERANCE * 100:.0f}%)")
        state = session.call("get_state")["ambient"]
        check(state["enabled"] and abs(state["intensity"] - INTENSITY) < 1e-3 and abs(state["intensityEnvironmentUnits"] - SCALE) < 1e-6
              and all(abs(state["radiance"][c] - color[c] * SCALE) < 1e-6 for c in range(3)),
              f"get_state ambient: {state}")
        # Intensity 3 cd/m^2 (the default when switched on), white: L = 3 / 12,800.
        session.env("ambient_color", [1.0, 1.0, 1.0])
        session.env("ambient_intensity", 3.0)
        session.capture("lambert_ambient_3cd")
        samples = grid(session, step=80, span=3)
        expected = 3.0 / UNIT
        worst = max((rel(p[c], expected) for _, _, _, _, p in samples for c in range(3)), default=1.0)
        check(worst <= LAMBERT_TOLERANCE, f"3 cd/m^2: radiance {expected:.4e} env units, worst {worst * 100:.3f}%")
        listed = {s["key"] for s in session.call("list_settings", scope="environment")["settings"]}
        wanted = {"ambient", "ambient_color", "ambient_intensity"}
        check(wanted <= listed, f"list_settings lists {sorted(wanted)} ({sorted(wanted - listed) or 'all'})")
        refused = 0
        for key, value in (("ambient_intensity", -1.0), ("ambient_intensity", 20000.0), ("ambient", "maybe"),
                           ("ambient_color", [1.0, 2.0])):
            try:
                session.env(key, value)
            except glider_ctl.GliderError:
                refused += 1
        check(refused == 4, f"invalid values refused: {refused} / 4")
        errors = session.close()
        check(errors == 0, f"Lambert session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"Lambert session failed: {error}")
        session.kill()
    dump = OUT / "refused.png"
    dump.unlink(missing_ok=True)
    bad = run([EXE, "--isolated", "--ambient-intensity", "-5", "--dump-frame", dump, "--exit-after-dump"], timeout=300)
    check(bad.returncode != 0 and not dump.exists(),
          f"--ambient-intensity -5 refused at the command line (exit {bad.returncode})")


def check_metal():
    print("[smooth metallic sphere lit only by the ambient]")
    model = write("metal", metal())
    f0 = (0.9, 0.6, 0.3)
    session = Session(model, black_env() + ["--ambient", "on", "--ambient-intensity", str(INTENSITY)])
    try:
        session.capture("metal_ambient")
        samples = grid(session)
        worst, worst_at = 0.0, None
        for x, y, _, ndotv, p in samples:
            fc = (1.0 - ndotv) ** 5
            for c in range(3):
                expected = SCALE * (f0[c] + (1.0 - f0[c]) * fc)  # L x (F0 A + B) at roughness 0
                e = rel(p[c], expected)
                if e > worst:
                    worst, worst_at = e, (x, y, round(ndotv, 3), c, p[c], expected)
        check(len(samples) >= 60, f"{len(samples)} probes on the sphere")
        check(worst <= METAL_TOLERANCE, f"radiance = L x (F0 A + B): worst {worst * 100:.3f}% "
                                        f"(<= {METAL_TOLERANCE * 100:.0f}%; at {worst_at})")
        errors = session.close()
        check(errors == 0, f"metal session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"metal session failed: {error}")
        session.kill()


def probe_set(session, offsets):
    """Probes at pixel offsets from the viewport centre."""
    vp = session.call("get_state")["viewports"][0]
    cx, cy = vp["width"] // 2, vp["height"] // 2
    out = {}
    for k, (dx, dy) in offsets.items():
        result = session.call("probe", x=cx + dx, y=cy + dy)
        if result.get("world"):  # background probes (no geometry) are left out
            out[k] = result["hdr"][:3]
    return out


def check_occlusion():
    print("[occlusion: authored AO and SSAO darken the ambient like the IBL diffuse]")
    plain, occluded = write("lambert", lambert()), write("lambert_ao", lambert_ao())
    ambient = ["--ambient", "on", "--ambient-intensity", str(INTENSITY)]
    pixels = {(gx, gy): (gx * 60, gy * 60) for gx in range(-3, 4) for gy in range(-3, 4)
              if gx * gx + gy * gy <= 9}
    values = {}
    for label, model in (("plain", plain), ("ao", occluded)):
        for source, flags in (("ambient", black_env() + ambient), ("ibl", grey_env())):
            session = Session(model, flags)
            try:
                session.capture(f"occlusion_{label}_{source}")
                values[(label, source)] = probe_set(session, pixels)
                errors = session.close()
                check(errors == 0, f"{label} / {source} session: {errors} GL errors")
            except Exception as error:  # noqa: BLE001
                check(False, f"{label} / {source} session failed: {error}")
                session.kill()
                return
    worst, ratios = 0.0, []
    keys = set.intersection(*(set(v) for v in values.values()))
    check(len(keys) >= 20, f"{len(keys)} sphere probes")
    for k in keys:
        r_amb = values[("ao", "ambient")][k][0] / max(values[("plain", "ambient")][k][0], 1e-9)
        r_ibl = values[("ao", "ibl")][k][0] / max(values[("plain", "ibl")][k][0], 1e-9)
        ratios.append(r_amb)
        worst = max(worst, abs(r_amb - r_ibl))
    check(all(abs(r - 128.0 / 255.0) < 0.005 for r in ratios),
          f"occlusion texture 0.502: ambient ratio {min(ratios):.4f}..{max(ratios):.4f}")
    check(worst <= RATIO_TOLERANCE, f"ambient AO ratio = IBL diffuse AO ratio within {RATIO_TOLERANCE} (worst {worst:.5f})")

    print("[SSAO on: the ambient's darkening equals the IBL diffuse's]")
    model = write("two_spheres", two_spheres())
    view = ["--camera-distance", "6"]
    # Probes along the gap between the spheres (x = +-1.02, radius 1), where SSAO sees the neighbour.
    pixels = {(gx, gy): (gx * 6, gy * 40) for gx in range(-8, 9) for gy in range(-4, 5)}
    values = {}
    for source, flags in (("ambient", black_env() + ambient), ("ibl", grey_env())):
        session = Session(model, flags + view)
        try:
            for ssao in (False, True):
                session.call("set_viewport_setting", key="ssao", value=ssao)
                session.call("set_viewport_setting", key="ssao_radius", value=1.0)
                session.call("set_viewport_setting", key="ssao_intensity", value=3.0)
                session.capture(f"ssao_{source}_{'on' if ssao else 'off'}", frames=6)
                values[(source, ssao)] = probe_set(session, pixels)
            errors = session.close()
            check(errors == 0, f"SSAO {source} session: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"SSAO {source} session failed: {error}")
            session.kill()
            return
    worst, darkest, counted = 0.0, 1.0, 0
    for k in set.intersection(*(set(v) for v in values.values())):
        off_a, off_i = values[("ambient", False)][k][0], values[("ibl", False)][k][0]
        if off_a <= 1e-6 or off_i <= 1e-6:
            continue  # background between the spheres
        r_amb = values[("ambient", True)][k][0] / off_a
        r_ibl = values[("ibl", True)][k][0] / off_i
        darkest = min(darkest, r_amb)
        worst = max(worst, abs(r_amb - r_ibl))
        counted += 1
    check(counted >= 40 and darkest < 0.9, f"{counted} sphere probes near the gap, darkest SSAO ratio {darkest:.3f} (< 0.9)")
    check(worst <= RATIO_TOLERANCE, f"SSAO ratio of the ambient = of the IBL diffuse within {RATIO_TOLERANCE} (worst {worst:.5f})")


def check_emissive_background():
    print("[emissive and background unchanged by the ambient]")
    model = write("emissive", emissive())
    off, _ = render(model, "emissive_off", black_env())
    on, _ = render(model, "emissive_on", black_env() + ["--ambient", "on", "--ambient-intensity", "5000"])
    check(same(on, off), "emissive sphere (black base colour, no specular): ambient on = off, byte-identical")
    model = write("lambert", lambert())
    off, report = render(model, "background_off", [])
    on, _ = render(model, "background_on", ["--ambient", "on", "--ambient-intensity", "2000"])
    if not (off and on and report):
        check(False, "background renders")
        return
    import numpy as np
    a, b = load(off), load(on)
    cam = Camera(report)
    changed = np.any(a != b, axis=2)
    outside = np.ones(changed.shape, bool)
    for y in range(cam.height):
        for x in range(cam.width):
            # A 3-pixel margin around the analytic silhouette (the tessellated mesh lies inside it).
            if any(cam.hit_unit_sphere(x + dx, y + dy) is not None for dx, dy in ((0, 0), (-3, 0), (3, 0), (0, -3), (0, 3))):
                outside[y, x] = False
    check(int(changed[outside].sum()) == 0 and int(outside.sum()) > 100000,
          f"default HDR background: {int(changed[outside].sum())} of {int(outside.sum())} background pixels changed")
    check(int(changed[~outside].sum()) > 1000, f"the sphere is brighter with the ambient ({int(changed[~outside].sum())} px changed)")
    # A3.6 ground plane: the same ambient term (ground.frag).
    ground = black_env() + ["--ground", "plane", "--ground-preset", "grey", "--camera-pitch", "-20"]
    off, _ = render(model, "ground_off", ground)
    on, _ = render(model, "ground_on", ground + ["--ambient", "on", "--ambient-intensity", str(INTENSITY)])
    if off and on:
        a, b = load(off), load(on)
        lower = (b[a.shape[0] * 3 // 4:] - a[a.shape[0] * 3 // 4:]).mean()
        check(lower > 5.0, f"ground plane lit by the ambient (lower quarter +{lower:.1f} levels)")
    else:
        check(False, "ground plane renders")


def check_cli_pipe():
    print("[CLI = pipe]")
    model = write("metal", metal())
    flags = ["--ambient", "on", "--ambient-color", "0.8,0.9,1", "--ambient-intensity", "4000"]
    images = {}
    for label, start, pipe in (("cli", flags, []), ("pipe", [], [("ambient", True), ("ambient_color", [0.8, 0.9, 1.0]),
                                                                  ("ambient_intensity", 4000.0)])):
        session = Session(model, start)
        try:
            for key, value in pipe:
                session.env(key, value)
            images[label] = session.capture(f"cli_pipe_{label}", frames=6)
            errors = session.close()
            check(errors == 0, f"{label} session: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"{label} session failed: {error}")
            session.kill()
            return
    check(same(images["cli"], images["pipe"]), "CLI flags and set_environment give byte-identical captures")
    png, report = render(model, "report_ambient", flags)
    amb = (report or {}).get("ambient", {})
    check(amb.get("enabled") is True and abs(amb.get("intensity", 0) - 4000.0) < 1e-3 and
          abs(amb.get("intensityEnvironmentUnits", 0) - 4000.0 / UNIT) < 1e-6, f"--profile-report ambient: {amb}")


def check_sources():
    print("[HDR and procedural sky: same ambient contribution for the same cd/m^2]")
    model = write("lambert", lambert())
    ambient = [("ambient", True), ("ambient_color", [1.0, 0.7, 0.4]), ("ambient_intensity", INTENSITY)]
    sky = ["--environment", "sky", "--sky-time-slicing", "off", "--time-of-day-mode", "on", "--time-preset", "night",
           "--exposure-mode", "manual"]
    pixels = {(gx, gy): (gx * 50, gy * 50) for gx in range(-4, 5) for gy in range(-4, 5) if gx * gx + gy * gy <= 16}
    diffs = {}
    for source, flags in (("hdr", []), ("sky", sky)):
        session = Session(model, flags)
        try:
            session.capture(f"source_{source}_off", frames=6)
            off = probe_set(session, pixels)
            for key, value in ambient:
                session.env(key, value)
            session.capture(f"source_{source}_on", frames=6)
            on = probe_set(session, pixels)
            diffs[source] = {k: [on[k][c] - off[k][c] for c in range(3)] for k in set(on) & set(off)}
            errors = session.close()
            check(errors == 0, f"{source} session: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"{source} session failed: {error}")
            session.kill()
            return
    worst, worst_expected = 0.0, 0.0
    expected = (1.0, 0.7, 0.4)
    keys = set(diffs["hdr"]) & set(diffs["sky"])
    check(len(keys) >= 20, f"{len(keys)} sphere probes")
    for k in keys:
        for c in range(3):
            worst = max(worst, rel(diffs["sky"][k][c], diffs["hdr"][k][c]))
            worst_expected = max(worst_expected, rel(diffs["hdr"][k][c], expected[c] * SCALE))
    check(worst <= SOURCE_TOLERANCE, f"sky - HDR ambient contribution: worst {worst * 100:.3f}% (<= {SOURCE_TOLERANCE * 100:.0f}%)")
    check(worst_expected <= SOURCE_TOLERANCE, f"HDR contribution = albedo x L: worst {worst_expected * 100:.3f}%")


def check_auto_fill():
    """A3.10: physical fill measured on the closed-box floor and an open floor, without changing legacy checks."""
    import sky_occlusion_fixtures as fixtures
    import validate_night as night
    import validate_sky_occlusion as so
    night.OUT = OUT
    for label, fixture in (("closed", fixtures.occluder_box), ("open", fixtures.open_plane)):
        model = fixture(OUT / f"auto_{label}.gltf")
        flags = ["--environment", "sky", "--time-of-day-mode", "on", "--time-preset", "noon",
                 "--sky-time-slicing", "off", "--ambient", "off"] + so.LOOK + so.TOP_VIEW
        session = night.Session(model, flags)
        try:
            session.capture(f"auto_{label}_off", frames=24)
            cam = session.camera()
            x, y = [int(round(c)) for c in cam.project((0.0, 0.0, 0.0))]
            def probes():
                p = session.call("probe", x=x, y=y)
                region = session.call("probe_region", x0=x-2, y0=y-2, x1=x+2, y1=y+2)
                return p["hdr"][:3] + region["rgb"]["mean"][:3]
            off = probes()
            session.env("ambient_mode", "auto")
            session.env("ambient_fill", 0.05)
            session.env("ambient", "auto")
            session.capture(f"auto_{label}_on", frames=12)
            state = session.call("get_state")
            on = probes()
            ambient = state["ambient"]
            expected = 0.8 * 0.05 * ambient["outdoorIlluminance"] / math.pi
            delta = [b-a for a, b in zip(off, on)]
            worst = max(abs(d - (expected if label == "closed" else 0.0)) / max(expected, 1e-9) for d in delta)
            check(worst <= LAMBERT_TOLERANCE,
                  f"Auto {label} floor, probe + region: delta {delta}, closed albedo*k*E/pi {expected:.8g}, "
                  f"worst {worst*100:.4f}% <= 1%")
            check(ambient["mode"] == "auto" and ambient["enabled"] and ambient["interiorWeighted"] and
                  rel(ambient["intensityEnvironmentUnits"], 0.05 * ambient["outdoorIlluminance"] / math.pi) < 0.0001,
                  f"Auto {label}: source default, units/formula and active visibility weighting")
            errors = session.close()
            check(errors == 0, f"Auto {label}: {errors} GL errors")
        except BaseException:
            session.process.kill()
            session.process.wait(30)
            raise


def check_auto_hdr():
    print("[Auto HDR: Lambertian +Y irradiance and uniform fallback without a volume]")
    model = write("auto_hdr_lambert", lambert())
    session = Session(model, grey_env() + ["--ambient-mode", "auto", "--ambient", "on", "--ambient-fill", "0.1"])
    try:
        session.capture("auto_hdr_uniform", frames=12)
        state = session.call("get_state")["ambient"]
        check(rel(state["outdoorIlluminance"], math.pi * 0.25) <= LAMBERT_TOLERANCE,
              f"HDR E_out {state['outdoorIlluminance']:.8g} = pi * 0.25 within 1%")
        check(state["mode"] == "auto" and not state["interiorWeighted"] and
              rel(state["intensityEnvironmentUnits"], 0.025) <= LAMBERT_TOLERANCE,
              f"HDR Auto without volume is uniform L=0.025: {state}")
        session.env("sun", True)
        session.env("sun_dir", [0, 90])
        session.env("sun_intensity", 2)
        session.env("hdr_intensity", 1.5)
        session.capture("auto_hdr_with_sun", frames=12)
        state = session.call("get_state")["ambient"]
        expected = math.pi * 0.25 * 1.5 + 2
        check(rel(state["outdoorIlluminance"], expected) <= LAMBERT_TOLERANCE,
              f"HDR E_out {state['outdoorIlluminance']:.8g} = irradiance * 1.5 + sun horizontal {expected:.8g}")
        check(session.close() == 0, "HDR Auto: zero GL errors")
    except BaseException:
        session.kill()
        raise


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=None)
    parser.add_argument("--only", default="", help="comma-separated subset: baseline,lambert,metal,occlusion,emissive,cli,sources")
    args = parser.parse_args()
    baseline = Path(args.baseline_exe) if args.baseline_exe else None
    if baseline is not None and not baseline.is_absolute():
        baseline = REPO / baseline
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None
    OUT.mkdir(parents=True, exist_ok=True)
    only = {s for s in args.only.split(",") if s}
    steps = (("lambert", check_lambert), ("metal", check_metal), ("occlusion", check_occlusion),
             ("emissive", check_emissive_background), ("cli", check_cli_pipe), ("sources", check_sources),
             ("baseline", lambda: check_baseline(baseline)), ("auto", check_auto_fill), ("auto_hdr", check_auto_hdr))
    for name, step in steps:
        if not only or name in only:
            step()
    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in {len(gl_logs)} renders {bad}")
    return finish()


if __name__ == "__main__":
    sys.exit(main())
