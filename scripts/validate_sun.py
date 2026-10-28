# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.1: the sun (a directional light, scene-level; docs/ASOBO_EXTENSIONS_PHASE_3.md).

Conventions (header/imgui/SceneEnvironment.h): azimuth around +Y from +Z towards +X, elevation above the horizon
(glTF Y-up); intensity is the illuminance in the environment's radiance units x sr, so a white Lambert surface
facing the sun reflects colour x intensity / pi. While the sun is on, the HDR's own sun (brightest region,
IBLSampler.h EnvironmentSun) is removed from the IBL and the background; From HDR uses that region's direction,
colour and integrated illuminance (x HDR intensity).

Fixtures: a unit sphere at the origin seen from +Z (camera distance 4), under a black environment map (IBL
contributes nothing) unless stated, black clear colour, PBR Neutral tone mapping, exposure 0, no bloom/AA.
Checks:
  * sun off (the default) is byte-identical to the baseline binary (--baseline-exe): HelloCube, the punctual,
    SSS and anisotropy fixtures, the Lambert sphere, and two E195 reference views (exterior and cockpit, ASOBO);
  * Lambert sphere (white, roughness 1, KHR_materials_specular factor 0 = pure Lambert), sun at az 30 / el 20:
      - least-squares fit of the linear pixel values L = a . N over the lit pixels: direction within
        DIR_TOLERANCE_DEG of the set direction, |a| pi within NORM_TOLERANCE of the intensity;
      - the brightest plateau's centroid is within PEAK_TOLERANCE_PX of the projection of N = L;
      - terminator: no pixel with N.L < -TERMINATOR_BAND is lit, every pixel with N.L > TERMINATOR_BAND is;
      - moving the azimuth by 90 degrees moves the fitted direction by 90 (+-DIR_TOLERANCE_DEG);
      - over the control pipe, probe() linear values follow colour x intensity x N.L / pi (N = the probed world
        position) within PROBE_REL x expected + PROBE_ABS; set_environment sun_dir changes them like the CLI;
  * From HDR, synthetic HDR (sky 0.5, a 0.6-degree disk of 2e4 at a known direction): the detected direction is
    within 0.2 degrees, the detected illuminance within 2% of the disk's excess integral; --hdr-rotation turns it;
    the background with the sun on has no texel above the removal threshold (the sun disk is gone) while sun off
    shows it; the mean linear luminance of the IBL-lit Lambert sphere stays within ENERGY_TOLERANCE of sun off;
  * From HDR, the default HDR: the detected sun is identical across runs and render-quality presets, follows
    --hdr-rotation, and is reported (direction, illuminance, colour);
  * KHR_lights_punctual sphere: with the sun on, the probed linear value equals light-only + sun-only (additive);
  * SSS and anisotropy fixtures respond to the sun (the image changes, probe_region values finite);
  * mirror sphere: the sun's highlight covers more than one pixel and grows with the angular radius;
  * zero GL errors everywhere.

Run: python scripts/validate_sun.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import json
import math
import os
import struct
import base64
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same, views, render_view  # noqa: E402
from validate_asobo_sss import sphere as uv_sphere, document as sss_document, write_hdr  # noqa: E402
from validate_asobo_anisotropy import document as aniso_document  # noqa: E402

OUT = REPO / "build" / "sun"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
VIEW = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
        "--tone-mapping", "neutral", "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0",
        "--camera-distance", "4"]
INTENSITY = 1.8              # max radiance 1.8 / pi = 0.573, inside PBR Neutral's linear range (< 0.76)
DIR_TOLERANCE_DEG = 1.5      # fitted light direction (8-bit capture, tessellated sphere)
NORM_TOLERANCE = 0.05        # fitted |a| pi against the intensity (relative; 8-bit, grazing pixels at az 120)
PEAK_TOLERANCE_PX = 8        # brightest plateau centroid against the projected N = L point
TERMINATOR_BAND = 0.03       # N.L band around the terminator left out of the lit/unlit test (tessellation)
PROBE_REL, PROBE_ABS = 0.02, 2e-3
SILHOUETTE_NDOTV = 0.2       # sphere pixels with N.V below this are left out (mesh silhouette)
ENERGY_TOLERANCE = 0.05      # mean IBL-lit sphere luminance, sun From HDR against sun off (relative)
SYN_SKY, SYN_SUN, SYN_RADIUS = 0.5, (20000.0, 18000.0, 15000.0), 0.6
LUMA = (0.2126, 0.7152, 0.0722)

gl_logs = []


# ---------------------------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------------------------

def gltf(material, light=None, extensions_used=()):
    blob = bytearray()
    views_, accessors = [], []

    def add(data, target, count, ctype, kind, minmax=None):
        while len(blob) % 4:
            blob.append(0)
        views_.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(data), "target": target})
        blob.extend(data)
        accessor = {"bufferView": len(views_) - 1, "componentType": ctype, "count": count, "type": kind}
        if minmax:
            accessor["min"], accessor["max"] = minmax
        accessors.append(accessor)
        return len(accessors) - 1

    attributes, indices = uv_sphere(add)
    nodes = [{"name": "Sphere", "mesh": 0}]
    doc = {"asset": {"version": "2.0", "generator": "A3.1 sun fixture"}, "scene": 0, "scenes": [{"nodes": [0]}],
           "nodes": nodes,
           "meshes": [{"name": "Sphere", "primitives": [{"attributes": attributes, "indices": indices, "material": 0}]}],
           "materials": [material], "accessors": accessors, "bufferViews": views_}
    used = set(extensions_used)
    if light is not None:
        used.add("KHR_lights_punctual")
        doc["extensions"] = {"KHR_lights_punctual": {"lights": [{"type": "directional", "intensity": 1.0, "color": [1, 1, 1]}]}}
        nodes.append({"name": "Light", **light, "extensions": {"KHR_lights_punctual": {"light": 0}}})
        doc["scenes"][0]["nodes"].append(1)
    doc["buffers"] = [{"byteLength": len(blob),
                       "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode("ascii")}]
    if used:
        doc["extensionsUsed"] = sorted(used)
    return doc


def lambert(light=None):
    # specularFactor 0: specularWeight 0, so the diffuse is exactly c_diff / pi and there is no specular lobe.
    material = {"name": "Lambert", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0.0,
                                                           "roughnessFactor": 1.0},
                "extensions": {"KHR_materials_specular": {"specularFactor": 0.0}}}
    return gltf(material, light, ["KHR_materials_specular"])


def mirror():
    return gltf({"name": "Mirror", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "metallicFactor": 1.0,
                                                           "roughnessFactor": 0.0}})


def write(name, doc):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def synthetic_hdr(name, azimuth, elevation, width=2048, height=1024):
    """Sky SYN_SKY everywhere plus a SYN_RADIUS-degree disk of SYN_SUN at (azimuth, elevation). Returns the path
    and the disk's excess illuminance (luminance of sum((L - sky) dw)) as Glider integrates it (row 0 = up)."""
    import numpy as np
    import cv2
    rows = (np.arange(height) + 0.5) / height * math.pi
    phi = ((np.arange(width) + 0.5) / width - 0.5) * 2 * math.pi
    x = np.outer(np.sin(rows), np.cos(phi))
    y = np.repeat(np.cos(rows)[:, None], width, 1)
    z = np.outer(np.sin(rows), np.sin(phi))
    d = sun_vector(azimuth, elevation)
    disk = np.degrees(np.arccos(np.clip(x * d[0] + y * d[1] + z * d[2], -1, 1))) < SYN_RADIUS
    image = np.full((height, width, 3), SYN_SKY, np.float32)
    image[disk] = SYN_SUN
    dw = (2 * math.pi / width) * (math.pi / height) * np.sin(rows)
    excess = ((image - SYN_SKY) * disk[:, :, None] * dw[:, None, None]).sum((0, 1))
    path = OUT / f"{name}.hdr"
    cv2.imwrite(str(path), image[:, :, ::-1])
    return path, float(np.dot(excess, LUMA))


# ---------------------------------------------------------------------------------------------
# Geometry
# ---------------------------------------------------------------------------------------------

def sun_vector(azimuth, elevation):
    az, el = math.radians(azimuth), math.radians(elevation)
    return (math.cos(el) * math.sin(az), math.sin(el), math.cos(el) * math.cos(az))


def angles(v):
    n = math.sqrt(sum(c * c for c in v))
    x, y, z = (c / n for c in v)
    return math.degrees(math.atan2(x, z)) % 360.0, math.degrees(math.asin(max(-1.0, min(1.0, y))))


def angle_between(a, b):
    na, nb = math.sqrt(sum(c * c for c in a)), math.sqrt(sum(c * c for c in b))
    return math.degrees(math.acos(max(-1.0, min(1.0, sum(p * q for p, q in zip(a, b)) / (na * nb)))))


def rotate_y_transposed(v, degrees):
    """transpose(glm::rotate(degrees, +Y)) * v: the world direction of cube-map direction v."""
    c, s = math.cos(math.radians(degrees)), math.sin(math.radians(degrees))
    # glm::rotate(a, Y) = [[c, 0, -s], [0, 1, 0], [s, 0, c]] (columns); its transpose applied to v:
    return (c * v[0] - s * v[2], v[1], s * v[0] + c * v[2])


class Camera:
    def __init__(self, report):
        cam = report["camera"]
        self.eye = cam["eye"]
        target, up = cam["target"], cam["up"]
        self.width, self.height = cam["viewport"]
        f = [t - e for t, e in zip(target, self.eye)]
        fl = math.sqrt(sum(c * c for c in f))
        self.f = [c / fl for c in f]
        r = [self.f[1] * up[2] - self.f[2] * up[1], self.f[2] * up[0] - self.f[0] * up[2], self.f[0] * up[1] - self.f[1] * up[0]]
        rl = math.sqrt(sum(c * c for c in r))
        self.r = [c / rl for c in r]
        self.u = [self.r[1] * self.f[2] - self.r[2] * self.f[1], self.r[2] * self.f[0] - self.r[0] * self.f[2],
                  self.r[0] * self.f[1] - self.r[1] * self.f[0]]
        self.t = math.tan(math.radians(cam["fovY"]) / 2.0)
        self.aspect = self.width / self.height

    def ray(self, px, py):
        nx = (2.0 * (px + 0.5) / self.width - 1.0) * self.t * self.aspect
        ny = (1.0 - 2.0 * (py + 0.5) / self.height) * self.t
        d = [self.f[i] + nx * self.r[i] + ny * self.u[i] for i in range(3)]
        n = math.sqrt(sum(c * c for c in d))
        return [c / n for c in d]

    def hit_unit_sphere(self, px, py):
        d, o = self.ray(px, py), self.eye
        b = sum(o[i] * d[i] for i in range(3))
        c = sum(v * v for v in o) - 1.0
        disc = b * b - c
        if disc < 0:
            return None
        t = -b - math.sqrt(disc)
        return [o[i] + t * d[i] for i in range(3)]

    def project(self, p):
        v = [p[i] - self.eye[i] for i in range(3)]
        z = sum(v[i] * self.f[i] for i in range(3))
        x = sum(v[i] * self.r[i] for i in range(3)) / (z * self.t * self.aspect)
        y = sum(v[i] * self.u[i] for i in range(3)) / (z * self.t)
        return ((x + 1.0) * 0.5 * self.width - 0.5, (1.0 - y) * 0.5 * self.height - 0.5)


# ---------------------------------------------------------------------------------------------
# Glider runs
# ---------------------------------------------------------------------------------------------

def run(args, timeout=600):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(model, name, flags=(), exe=EXE, frame=24, view=True):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    args = [exe] + ([model] if model else []) + COMMON + (VIEW if view else []) + list(flags)
    result = run(args + ["--capture-viewport", png, "--capture-frame", str(frame), "--profile-report", report,
                         "--exit-after-profile"])
    log = result.stdout + result.stderr
    if exe == EXE:
        gl_logs.append((name, log))
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    return (png if png.exists() else None), data


def gl_errors_zero(log):
    lines = [line for line in log.splitlines() if line.startswith("GL automation errors")]
    return lines == ["GL automation errors: 0"] and "GL error 0x" not in log


def load(png):
    import numpy as np
    from PIL import Image
    return np.asarray(Image.open(png).convert("RGB"), dtype=np.float64)


def untonemap(levels):
    """Scene-linear value of 8-bit channels under exposure 0 and Khronos PBR Neutral (below its 0.76 start)."""
    import numpy as np
    c = levels / 255.0
    lin = np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
    return np.where(lin >= 0.04, lin + 0.04, np.sqrt(np.maximum(lin, 0.0) / 6.25))


def sphere_samples(png, report):
    """Per sphere pixel: (x, y, normal, linear luminance)."""
    import numpy as np
    cam = Camera(report)
    linear = untonemap(load(png)) @ np.array(LUMA)
    samples = []
    for y in range(cam.height):
        for x in range(cam.width):
            p = cam.hit_unit_sphere(x, y)
            if p is None:
                continue
            # Leave out the silhouette band, where the tessellated mesh lies inside the analytic sphere.
            to_eye = [cam.eye[i] - p[i] for i in range(3)]
            if sum(p[i] * to_eye[i] for i in range(3)) / math.sqrt(sum(c * c for c in to_eye)) < SILHOUETTE_NDOTV:
                continue
            samples.append((x, y, p, linear[y, x]))
    return cam, samples


def fit_light(samples, floor=0.08):
    """Least squares L = a . N over pixels brighter than floor (avoids the tone curve's toe and the terminator)."""
    import numpy as np
    rows = [(n, v) for _, _, n, v in samples if v > floor]
    a = np.linalg.lstsq(np.array([n for n, _ in rows]), np.array([v for _, v in rows]), rcond=None)[0]
    return a, len(rows)


class Session:
    def __init__(self, model, flags=()):
        args = [str(EXE), str(model)] + COMMON + VIEW + list(flags) + ["--control-pipe"]
        self.process = subprocess.Popen(args, cwd=REPO, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                        stderr=subprocess.DEVNULL)
        deadline = time.time() + 120
        while not os.path.exists(glider_ctl.pipe_name(self.process.pid)):
            if self.process.poll() is not None or time.time() > deadline:
                raise RuntimeError(f"Glider did not open its pipe (exit {self.process.poll()})")
            time.sleep(0.1)
        # The pipe can exist a moment before it accepts a client.
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
            except Exception:
                self.process.kill()
        return errors


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_lambert(black):
    print("[Lambert sphere, manual sun, IBL black]")
    model = write("lambert", lambert())
    sun = ["--sun", "on", "--sun-intensity", str(INTENSITY)]
    fits = {}
    for azimuth in (30.0, 120.0):
        png, report = render(model, f"lambert_az{int(azimuth)}", black + sun + ["--sun-dir", f"{azimuth},20"])
        if png is None or report is None:
            check(False, f"Lambert sphere az {azimuth} rendered")
            return
        cam, samples = sphere_samples(png, report)
        a, count = fit_light(samples)
        expected = sun_vector(azimuth, 20.0)
        error = angle_between(a, expected)
        magnitude = math.sqrt(sum(c * c for c in a)) * math.pi
        fits[azimuth] = a
        check(error <= DIR_TOLERANCE_DEG, f"az {azimuth:.0f} el 20: fitted direction {angles(a)[0]:.2f}/{angles(a)[1]:.2f} deg, "
                                          f"{error:.2f} deg from the set direction (<= {DIR_TOLERANCE_DEG}; {count} px)")
        check(abs(magnitude / INTENSITY - 1.0) <= NORM_TOLERANCE,
              f"az {azimuth:.0f}: fitted |a| pi = {magnitude:.4f} against intensity {INTENSITY} (colour x intensity x N.L / pi, "
              f"+-{NORM_TOLERANCE * 100:.0f}%)")
        if azimuth == 30.0:
            peak = max(v for _, _, _, v in samples)
            plateau = [(x, y) for x, y, _, v in samples if v >= peak - 0.004]
            cx = sum(x for x, _ in plateau) / len(plateau)
            cy = sum(y for _, y in plateau) / len(plateau)
            ex, ey = cam.project(expected)
            distance = math.hypot(cx - ex, cy - ey)
            check(distance <= PEAK_TOLERANCE_PX, f"brightest plateau ({len(plateau)} px) centred at ({cx:.1f}, {cy:.1f}), "
                                                 f"{distance:.1f} px from N = L at ({ex:.1f}, {ey:.1f}) (<= {PEAK_TOLERANCE_PX})")
            dark_lit = sum(1 for _, _, n, v in samples
                           if sum(n[i] * expected[i] for i in range(3)) < -TERMINATOR_BAND and v > 0.0)
            lit_dark = sum(1 for _, _, n, v in samples
                           if sum(n[i] * expected[i] for i in range(3)) > TERMINATOR_BAND and v <= 0.0)
            behind = sum(1 for _, _, n, _ in samples if sum(n[i] * expected[i] for i in range(3)) < -TERMINATOR_BAND)
            check(behind > 300 and dark_lit == 0 and lit_dark == 0,
                  f"terminator at N.L = 0: {dark_lit} lit pixels beyond it ({behind} px with N.L < -{TERMINATOR_BAND}), "
                  f"{lit_dark} dark pixels before it")
    if len(fits) == 2:
        turned = angles(fits[120.0])[0] - angles(fits[30.0])[0]
        check(abs(turned - 90.0) <= DIR_TOLERANCE_DEG, f"azimuth +90: the fitted light turns by {turned:.2f} deg")

    print("[Lambert sphere over the control pipe: probe]")
    session = Session(model, black)
    try:
        session.call("set_environment", key="sun", value=True)
        session.call("set_environment", key="sun_intensity", value=INTENSITY)
        worst = 0.0
        for azimuth, elevation in ((30.0, 20.0), (-60.0, 45.0)):
            session.call("set_environment", key="sun_dir", value=[azimuth, elevation])
            session.call("capture", path=str(OUT / f"pipe_lambert_{int(azimuth)}.png"))
            state = session.call("get_state")
            width, height = state["viewports"][0]["width"], state["viewports"][0]["height"]
            light = sun_vector(azimuth, elevation)
            probes = 0
            for gy in range(-3, 4):
                for gx in range(-3, 4):
                    result = session.call("probe", x=width // 2 + gx * 30, y=height // 2 + gy * 30)
                    world = result.get("world")
                    if not world:
                        continue
                    n = [c / math.sqrt(sum(w * w for w in world)) for c in world]
                    expected = INTENSITY * max(0.0, sum(n[i] * light[i] for i in range(3))) / math.pi
                    measured = result["hdrLuminance"]
                    worst = max(worst, abs(measured - expected) - PROBE_REL * expected)
                    probes += 1
            check(probes >= 40, f"sun_dir {azimuth:.0f},{elevation:.0f}: {probes} probes on the sphere")
        check(worst <= PROBE_ABS, f"probe linear luminance = intensity x N.L / pi within {PROBE_REL * 100:.0f}% + {PROBE_ABS} "
                                  f"(worst excess {worst:.2e})")
        reported = session.call("get_state")["sun"]
        check(reported["active"] and abs(reported["azimuth"] - 300.0) < 1e-3 and abs(reported["elevation"] - 45.0) < 1e-3,
              f"get_state sun: active, az {reported['azimuth']}, el {reported['elevation']}")
        listed = {s["key"] for s in session.call("list_settings", scope="environment")["settings"]}
        wanted = {"sun", "sun_mode", "sun_dir", "sun_azimuth", "sun_elevation", "sun_color", "sun_intensity", "sun_angular_radius"}
        check(wanted <= listed, f"list_settings lists the sun settings ({sorted(wanted - listed) or 'all'})")
        errors = session.close()
        check(errors == 0, f"pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session failed: {error}")
        session.process.kill()


def check_from_hdr_synthetic():
    print("[From HDR, synthetic HDR]")
    front_hdr, front_e = synthetic_hdr("synthetic_front", 20.0, 35.0)
    model = write("lambert", lambert())
    off_png, off = render(model, "syn_front_off", ["--env", front_hdr])
    on_png, on = render(model, "syn_front_hdr", ["--env", front_hdr, "--sun", "on", "--sun-mode", "hdr"])
    if not (off and on):
        check(False, "synthetic From HDR renders")
        return
    detected = on["sun"]["hdr"]
    error = angle_between(detected["worldDirection"], sun_vector(20.0, 35.0))
    check(detected["found"] and error <= 0.2, f"detected sun {detected['azimuth']:.3f}/{detected['elevation']:.3f} deg, "
                                             f"{error:.3f} deg from the disk (az 20, el 35)")
    check(abs(detected["illuminance"] / front_e - 1.0) <= 0.02,
          f"detected illuminance {detected['illuminance']:.4f} against the disk's excess {front_e:.4f} (+-2%)")
    check(detected["removed"] and on["sun"]["active"], "the HDR sun is removed while the sun is on")
    check(not off["sun"]["hdr"]["removed"], "sun off: the HDR is untouched (not removed)")
    _, rotated = render(model, "syn_front_rot90", ["--env", front_hdr, "--hdr-rotation", "90", "--sun", "on",
                                                    "--sun-mode", "hdr"])
    if rotated:
        expected = rotate_y_transposed(sun_vector(20.0, 35.0), 90.0)
        err = angle_between(rotated["sun"]["direction"], expected)
        check(err <= 0.2, f"--hdr-rotation 90 turns the From HDR sun to {rotated['sun']['azimuth']:.2f} deg "
                          f"(expected {angles(expected)[0]:.2f}; {err:.3f} deg)")
    # Energy: the mean linear luminance of the sphere (IBL + analytic sun against IBL with the sun in it).
    import numpy as np
    _, samples_off = sphere_samples(off_png, off)
    _, samples_on = sphere_samples(on_png, on)
    mean_off = np.mean([v for *_, v in samples_off])
    mean_on = np.mean([v for *_, v in samples_on])
    check(abs(mean_on / mean_off - 1.0) <= ENERGY_TOLERANCE,
          f"IBL-lit Lambert sphere: mean linear luminance {mean_on:.4f} (From HDR) against {mean_off:.4f} (sun off), "
          f"{(mean_on / mean_off - 1.0) * 100:+.1f}% (<= {ENERGY_TOLERANCE * 100:.0f}%)")
    a_off, _ = fit_light(samples_off, 0.0)
    a_on, _ = fit_light(samples_on, 0.0)
    check(angle_between(a_on, a_off) <= 3.0, f"the lit side matches: dominant light direction off/on "
                                             f"{angle_between(a_on, a_off):.2f} deg apart")

    print("[From HDR, synthetic HDR: background]")
    back_hdr, _ = synthetic_hdr("synthetic_back", 180.0, 25.0)
    shots = {}
    # The background stays on (no --clear-color); the camera looks at -Z, the sun is 25 degrees up behind the sphere.
    background_view = [a for a in VIEW if a not in ("--clear-color", "0,0,0")]
    for label, flags in (("off", []), ("hdr", ["--sun", "on", "--sun-mode", "hdr"]), ("manual", ["--sun", "on"])):
        png, report = render(model, f"syn_back_{label}", background_view + ["--env", back_hdr] + flags, view=False)
        shots[label] = (png, report)
    if all(p for p, _ in shots.values()):
        white = {k: int((load(p).min(axis=2) >= 254).sum()) for k, (p, _) in shots.items()}
        check(white["off"] > 0, f"sun off: the background shows the HDR's sun disk ({white['off']} white pixels)")
        check(white["hdr"] == 0 and white["manual"] == 0,
              f"sun on: no clipped sun left in the background (From HDR {white['hdr']}, Manual {white['manual']} px)")


def check_default_hdr():
    print("[From HDR, the default HDR]")
    model = write("lambert", lambert())
    results = []
    for label, flags in (("a", []), ("b", []), ("perf", ["--quality", "performance"]), ("rot", ["--hdr-rotation", "90"])):
        _, report = render(model, f"default_hdr_{label}", ["--sun", "on", "--sun-mode", "hdr"] + flags, view=False)
        results.append(report["sun"] if report else None)
    if not all(results):
        check(False, "default HDR reports")
        return
    a, b, perf, rot = results
    hdr = a["hdr"]
    check(hdr["found"], f"default HDR sun: az {hdr['azimuth']:.3f}, el {hdr['elevation']:.3f} deg, illuminance "
                        f"{hdr['illuminance']:.5g}, colour {[round(c, 3) for c in hdr['color']]}, {hdr['texels']} texels, "
                        f"sky {hdr['skyLevel']:.4g}, peak {hdr['peakLuminance']:.4g}")
    check(a == b and perf["hdr"]["cubeDirection"] == hdr["cubeDirection"] and perf["hdr"]["illuminance"] == hdr["illuminance"],
          "the detected sun is identical across runs and quality presets")
    expected = rotate_y_transposed(hdr["cubeDirection"], 90.0)
    check(angle_between(rot["direction"], expected) <= 1e-3, f"--hdr-rotation 90: From HDR azimuth {rot['azimuth']:.3f}")
    (OUT / "default_hdr_sun.json").write_text(json.dumps(a, indent=1), encoding="utf-8")


def check_punctual(black):
    print("[KHR_lights_punctual: additive with the sun]")
    # The directional light points along the node's -Z: from +X (rotation +90 deg about Y) lights the +X side.
    side = {"translation": [5, 0, 0], "rotation": [0, math.sin(math.pi / 4), 0, math.cos(math.pi / 4)]}
    with_light = write("lambert_light", lambert(side))
    without = write("lambert", lambert())
    flags = black + ["--sun-intensity", str(INTENSITY), "--sun-dir", "0,30"]
    values = {}
    for label, model, sun in (("light", with_light, "off"), ("sun", without, "on"), ("both", with_light, "on")):
        session = Session(model, flags + ["--sun", sun])
        try:
            session.call("capture", path=str(OUT / f"punctual_{label}.png"))
            state = session.call("get_state")
            width, height = state["viewports"][0]["width"], state["viewports"][0]["height"]
            values[label] = [session.call("probe", x=width // 2 + dx, y=height // 2 + dy)["hdr"][:3]
                             for dx, dy in ((40, 0), (80, -40), (40, -60), (100, 20))]
            errors = session.close()
            check(errors == 0, f"{label}: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"{label} session failed: {error}")
            session.process.kill()
            return
    worst = max(abs(both[c] - (light[c] + sun[c])) - 2e-3 * both[c]
                for both, light, sun in zip(values["both"], values["light"], values["sun"]) for c in range(3))
    check(worst <= 1e-3 and all(min(v) > 0.01 for v in values["light"]) and all(min(v) > 0.01 for v in values["sun"]),
          f"light + sun = both at 4 probes (worst excess {worst:.2e})")


def check_responds():
    print("[SSS, anisotropy and mirror respond to the sun]")
    black = ["--env", str(write_hdr(OUT / "black.hdr", 64, 32, lambda x, y: (0.0, 0.0, 0.0)))]
    fixtures = {
        "sss": write("sss_sphere", sss_document(sss={}, shape="sphere", light="side")),
        "anisotropy": write("aniso_sphere", aniso_document(khr={"anisotropyStrength": 0.8}, light=False)),
    }
    for label, model in fixtures.items():
        off, _ = render(model, f"{label}_sun_off", black)
        on, _ = render(model, f"{label}_sun_on", black + ["--sun", "on", "--sun-dir", "30,30", "--sun-intensity", "2"])
        changed = off is not None and on is not None and not same(off, on)
        check(changed, f"{label}: the sun changes the image")
        if changed:
            session = Session(model, black + ["--sun", "on", "--sun-dir", "30,30", "--sun-intensity", "2"])
            try:
                session.call("capture", path=str(OUT / f"{label}_pipe.png"))
                region = session.call("probe_region", x0=0, y0=0, x1=4000, y1=4000)
                values = region["rgb"]["max"] + region["rgb"]["min"] + [region["luminance"]["mean"]]
                check(all(math.isfinite(v) for v in values) and region["luminance"]["max"] > 0.0,
                      f"{label}: linear values finite (max luminance {region['luminance']['max']:.4f})")
                errors = session.close()
                check(errors == 0, f"{label}: {errors} GL errors in the pipe session")
            except Exception as error:  # noqa: BLE001
                check(False, f"{label} session failed: {error}")
                session.process.kill()
    model = write("mirror", mirror())
    areas = {}
    for radius in ("0.27", "2"):
        png, _ = render(model, f"mirror_r{radius}", black + ["--sun", "on", "--sun-dir", "0,0", "--sun-intensity", "2",
                                                               "--sun-angular-radius", radius])
        areas[radius] = int((load(png).max(axis=2) >= 128).sum()) if png else 0
    check(areas["0.27"] > 1 and areas["2"] > areas["0.27"],
          f"mirror sphere: the sun highlight covers {areas['0.27']} px at 0.27 deg and {areas['2']} px at 2 deg")


def check_baseline(baseline, black):
    print("[sun off: byte-identical to the baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    print(f"  INFO baseline {baseline.name}")
    cases = [("hellocube", None, [], False), ("lambert", OUT / "lambert.gltf", black, True),
             ("lambert_light", OUT / "lambert_light.gltf", black, True), ("sss_sphere", OUT / "sss_sphere.gltf", black, True),
             ("aniso_sphere", OUT / "aniso_sphere.gltf", [], True)]
    for name, model, flags, view in cases:
        now, _ = render(model, f"base_now_{name}", flags, view=view)
        before, _ = render(model, f"base_ref_{name}", flags, exe=baseline, view=view)
        check(same(now, before), f"{name}: sun off is byte-identical to {baseline.stem}")
    for name in ("exterior_fuselage_nose", "cockpit_main_displays"):
        try:
            entry = next(v for v in views() if v["name"] == name)
        except StopIteration:
            print(f"  SKIP reference view {name} not found")
            continue
        now = render_view(EXE, entry, f"base_now_{name}", folder=OUT)
        before = render_view(baseline, entry, f"base_ref_{name}", folder=OUT)
        if now is None and before is None:
            print(f"  SKIP {name}: E195 asset not available")
            continue
        check(same(now, before), f"E195 {name} (ASOBO): sun off is byte-identical to {baseline.stem}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=None)
    args = parser.parse_args()
    baseline = Path(args.baseline_exe) if args.baseline_exe else None
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None
    OUT.mkdir(parents=True, exist_ok=True)
    black = ["--env", str(write_hdr(OUT / "black.hdr", 64, 32, lambda x, y: (0.0, 0.0, 0.0)))]
    check_lambert(black)
    check_from_hdr_synthetic()
    check_default_hdr()
    check_punctual(black)
    check_responds()
    check_baseline(baseline, black)
    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in {len(gl_logs)} renders {bad}")
    return finish()


if __name__ == "__main__":
    sys.exit(main())
