# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.4: ASOBO_material_SSS (subsurface-scattering approximation).

Rule (docs/ASOBO_EXTENSIONS_SEMANTICS.md section 18; resources/shaders/asobo_sss.glsl). For OPAQUE / MASK
materials, with s = SSSColor.rgb (linear, 0..1, default white; alpha ignored; a 2-item list is invalid and
falls back to white with a warning) and t = 1 - opacityTexture.r (1 without the texture):
  * direct lights: the core clamp(N.L) becomes front + back with
      front = clamp((N.L + w) / (1 + w), 0, 1), w = 0.5 s;  back = 0.5 s t clamp(-N.L, 0, 1)
    (front + back <= 1, so no surface gets more than one facing the light);
  * IBL: + 0.5 s t c_diff irradiance(-N), before ambient occlusion.

Fixtures: a 2x2 quad facing the camera and a unit sphere, grey dielectric (base 0.8, roughness 1), black
environment unless stated, directional KHR_lights_punctual light in front, behind or to the side. Checks:
  * black SSSColor is byte-identical to the material without the extension (front, back, side light, sphere,
    default environment); 3-item RGB equals the 4-item RGBA list; a 2-item list renders as the default (white)
    and the validator reports it;
  * back-lit quad: the core material is black, SSS (white) transmits light: lit, finite, at most the front-lit
    core response, about half of it (kAsoboSssTranslucency); an orange SSSColor tints it (R > G > B);
  * opacityTexture: white (opaque) = no translucency (byte-identical to the core back-lit render), black = the
    same as no texture, grey 50% (linear) about half the transmitted light; a missing image is not read as opaque;
  * side-lit sphere: the wrap lights the core terminator column; nothing exceeds the core front-lit maximum;
  * front-lit quad: difference to the core material <= FRONT_TOLERANCE levels; front-lit sphere: mean difference
    small (rim wrap only);
  * IBL only (environment bright behind the quad): SSS front is brighter than the core front;
  * dithered SSS (ASOBO_material_alphamode_dither): every kept pixel equals the undithered SSS render, the others
    show the background;
  * decal receiver (zero-area decal activates the buffer): back-lit SSS unchanged, prepass depth == forward depth;
  * --asobo-disable ASOBO_material_SSS and --asobo off are byte-identical to the file without the extension; another
    extension's switch keeps SSS;
  * zero GL errors; files without the extension byte-identical to the baseline binary (--baseline-exe).

Run: python scripts/validate_asobo_sss.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import io
import json
import math
import struct
import sys
from pathlib import Path

import harness_process as subprocess
from glider_harness import gl_errors_zero
from validate_asobo_common import EXE, PLAIN_FIXTURE, REPO, check, finish, same
from validate_asobo_common import render as common_render

OUT = REPO / "build" / "asobo_sss"
SSS = "ASOBO_material_SSS"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off"]
VIEW = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
        "--tone-mapping", "neutral", "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0",
        "--camera-distance", "4"]
NO_AA = ["--aa", "none"]
FRONT_TOLERANCE = 2          # 8-bit levels, quad lit straight on
WRAP = 0.5                   # kAsoboSssWrap at SSSColor 1
HALF_TOLERANCE = 0.1         # relative tolerance of the "about half" ratios (linear light)
ORANGE = [1.0, 0.5239377, 0.09352517, 1.0]   # the MSFS windsock's SSSColor

# Directional lights shine along the node's -Z. Front: from the camera side; back: from behind the quad,
# towards the camera; side: from +X.
LIGHTS = {
    "front": {"translation": [0, 0, 5]},
    "back": {"translation": [0, 0, -5], "rotation": [0, 1, 0, 0]},
    "side": {"translation": [5, 0, 0], "rotation": [0, math.sin(math.pi / 4), 0, math.cos(math.pi / 4)]},
}


def png_rgb(rgb):
    from PIL import Image
    data = io.BytesIO()
    Image.new("RGB", (4, 4), rgb).save(data, format="PNG")
    return "data:image/png;base64," + base64.b64encode(data.getvalue()).decode("ascii")


def write_hdr(path, width, height, pixel):
    """Uncompressed Radiance RGBE; pixel(x, y) -> (r, g, b) linear."""
    def rgbe(r, g, b):
        v = max(r, g, b)
        if v < 1e-32:
            return bytes((0, 0, 0, 0))
        m, e = math.frexp(v)
        scale = m * 256.0 / v
        return bytes((int(r * scale), int(g * scale), int(b * scale), e + 128))
    header = f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode("ascii")
    body = bytearray()
    for y in range(height):
        for x in range(width):
            body += rgbe(*pixel(x, y))
    path.write_bytes(header + bytes(body))
    return path


def quad(add):
    pos = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)]
    nrm = [(0, 0, 1)] * 4
    uv = [(0, 1), (1, 1), (1, 0), (0, 0)]
    idx = [0, 1, 2, 0, 2, 3]
    p = add(b"".join(struct.pack("<3f", *v) for v in pos), 34962, 4, 5126, "VEC3", ([-1, -1, 0], [1, 1, 0]))
    n = add(b"".join(struct.pack("<3f", *v) for v in nrm), 34962, 4, 5126, "VEC3")
    t = add(b"".join(struct.pack("<2f", *v) for v in uv), 34962, 4, 5126, "VEC2")
    i = add(b"".join(struct.pack("<I", v) for v in idx), 34963, 6, 5125, "SCALAR")
    return {"POSITION": p, "NORMAL": n, "TEXCOORD_0": t}, i


def sphere(add, segments=96, rings=48):
    pos, uv, idx = [], [], []
    for r in range(rings + 1):
        theta = math.pi * r / rings
        for s in range(segments + 1):
            phi = 2.0 * math.pi * s / segments
            pos.append((math.sin(theta) * math.sin(phi), math.cos(theta), math.sin(theta) * math.cos(phi)))
            uv.append((s / segments, r / rings))
    for r in range(rings):
        for s in range(segments):
            a = r * (segments + 1) + s
            b = a + segments + 1
            idx += [a, b, a + 1, a + 1, b, b + 1]
    p = add(b"".join(struct.pack("<3f", *v) for v in pos), 34962, len(pos), 5126, "VEC3", ([-1, -1, -1], [1, 1, 1]))
    n = add(b"".join(struct.pack("<3f", *v) for v in pos), 34962, len(pos), 5126, "VEC3")
    t = add(b"".join(struct.pack("<2f", *v) for v in uv), 34962, len(uv), 5126, "VEC2")
    i = add(b"".join(struct.pack("<I", v) for v in idx), 34963, len(idx), 5125, "SCALAR")
    return {"POSITION": p, "NORMAL": n, "TEXCOORD_0": t}, i


def document(sss=None, shape="quad", light="back", opacity=None, dither=False, decal=False):
    """sss: the ASOBO_material_SSS value (None: no extension); opacity: (r, g, b) of opacityTexture, or
    "missing" for an image file that does not exist; light: a LIGHTS key or None."""
    blob = bytearray()
    views, accessors = [], []

    def add(data, target, count, ctype, kind, minmax=None):
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(data), "target": target})
        blob.extend(data)
        accessor = {"bufferView": len(views) - 1, "componentType": ctype, "count": count, "type": kind}
        if minmax:
            accessor["min"], accessor["max"] = minmax
        accessors.append(accessor)
        return len(accessors) - 1

    attributes, indices = (quad if shape == "quad" else sphere)(add)
    material = {"name": "Skin", "doubleSided": shape == "quad", "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.8, 0.8, 0.5],
                                                         "metallicFactor": 0.0, "roughnessFactor": 1.0}}
    used = set()
    extensions = {}
    images, textures = [], []
    if sss is not None:
        value = dict(sss)
        if opacity is not None:
            images.append({"uri": "missing_opacity_texture.png" if opacity == "missing" else png_rgb(opacity)})
            textures.append({"source": 0, "sampler": 0})
            value["opacityTexture"] = {"index": 0}
        extensions[SSS] = value
        used.add(SSS)
    if dither:
        extensions["ASOBO_material_alphamode_dither"] = {}
        used.add("ASOBO_material_alphamode_dither")
    if extensions:
        material["extensions"] = extensions
    nodes = [{"name": "Shape", "mesh": 0}]
    doc = {
        "asset": {"version": "2.0", "generator": "A3.4 SSS fixture"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": nodes,
        "meshes": [{"name": "Shape", "primitives": [{"attributes": attributes, "indices": indices, "material": 0}]}],
        "materials": [material],
        "accessors": accessors, "bufferViews": views,
    }
    if images:
        doc["images"], doc["textures"] = images, textures
        doc["samplers"] = [{"magFilter": 9728, "minFilter": 9728}]
    if light is not None:
        used.add("KHR_lights_punctual")
        doc["extensions"] = {"KHR_lights_punctual": {"lights": [{"type": "directional", "intensity": 2.0,
                                                                  "color": [1, 1, 1]}]}}
        nodes.append({"name": "Light", **LIGHTS[light], "extensions": {"KHR_lights_punctual": {"light": 0}}})
        doc["scenes"][0]["nodes"].append(len(nodes) - 1)
    if decal:
        # A zero-area geometry decal far behind the shape activates the decal buffer and the prepass.
        while len(blob) % 4:
            blob.append(0)
        offset = len(blob)
        blob += struct.pack("<3f", 0, 0, -5) * 3 + struct.pack("<3f", 0, 0, 1) * 3
        views += [{"buffer": 0, "byteOffset": offset, "byteLength": 36, "target": 34962},
                  {"buffer": 0, "byteOffset": offset + 36, "byteLength": 36, "target": 34962}]
        n = len(accessors)
        accessors += [{"bufferView": len(views) - 2, "componentType": 5126, "count": 3, "type": "VEC3",
                       "min": [0, 0, -5], "max": [0, 0, -5]},
                      {"bufferView": len(views) - 1, "componentType": 5126, "count": 3, "type": "VEC3"}]
        doc["materials"].append({"name": "Decal", "alphaMode": "BLEND", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 0]},
                                 "extensions": {"ASOBO_material_geometry_decal": {},
                                                "ASOBO_material_draw_order": {"drawOrderOffset": 10}}})
        doc["meshes"].append({"name": "Decal", "primitives": [{"attributes": {"POSITION": n, "NORMAL": n + 1},
                                                               "material": len(doc["materials"]) - 1}]})
        nodes.append({"name": "Decal", "mesh": len(doc["meshes"]) - 1})
        doc["scenes"][0]["nodes"].append(len(nodes) - 1)
        used |= {"ASOBO_material_geometry_decal", "ASOBO_material_draw_order"}
    doc["buffers"] = [{"byteLength": len(blob),
                       "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode("ascii")}]
    if used:
        doc["extensionsUsed"] = sorted(used)
    return doc


def write(name, **kwargs):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(document(**kwargs)), encoding="utf-8")
    return path


def run(args, timeout=600):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(exe, model, name, flags=(), frame=24):
    """Viewport capture; returns (png or None, stdout + stderr)."""
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    result = run([exe, model, *COMMON, *VIEW, *flags, "--capture-viewport", png, "--capture-frame", str(frame),
                  "--profile-report", report, "--exit-after-profile"])
    return (png if png.exists() else None), result.stdout + result.stderr


def pixels(png):
    from PIL import Image
    image = Image.open(png).convert("RGB")
    return image.size, image.load()


def srgb_to_linear(level):
    c = level / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def centre_mean(png, half=40):
    """Mean (r, g, b) 8-bit levels of the central (2 half)^2 block."""
    (w, h), data = pixels(png)
    sums, count = [0.0, 0.0, 0.0], 0
    for y in range(h // 2 - half, h // 2 + half):
        for x in range(w // 2 - half, w // 2 + half):
            for c in range(3):
                sums[c] += data[x, y][c]
            count += 1
    return tuple(v / count for v in sums)


def luminance(rgb):
    return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]


def max_level(png):
    from PIL import Image
    return max(hi for lo, hi in Image.open(png).convert("RGB").getextrema())


def max_level_diff(a, b):
    from PIL import Image, ImageChops
    ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    if ia.size != ib.size:
        return 999
    return max(hi for lo, hi in ImageChops.difference(ia, ib).getextrema())


def mean_diff_over(a, b, mask_png):
    """Mean absolute 8-bit difference over the pixels that are non-black in mask_png."""
    (w, h), da = pixels(a)
    _, db = pixels(b)
    _, dm = pixels(mask_png)
    total, count = 0.0, 0
    for y in range(h):
        for x in range(w):
            if max(dm[x, y]) > 0:
                total += sum(abs(da[x, y][c] - db[x, y][c]) for c in range(3)) / 3.0
                count += 1
    return total / max(1, count), count


def column_mean(png, x_offset=0, rows=60):
    """Mean luminance of a short column at the image centre (shifted by x_offset pixels)."""
    (w, h), data = pixels(png)
    x = w // 2 + x_offset
    values = [luminance(data[x, y]) for y in range(h // 2 - rows // 2, h // 2 + rows // 2)]
    return sum(values) / len(values)


def untonemap(level):
    """Scene-linear value of one 8-bit channel under exposure 0 and the Khronos PBR Neutral tone map (below its
    0.76 compression start): out = x - 0.04 for x >= 0.08, out = 6.25 x^2 below."""
    out = srgb_to_linear(level)
    return out + 0.04 if out >= 0.04 else math.sqrt(out / 6.25)


def disc_linear_gain(a, b):
    """sum(linear a) / sum(linear b) - 1 over the pixels that are lit in b (green channel)."""
    (w, h), da = pixels(a)
    _, db = pixels(b)
    sa = sb = 0.0
    for y in range(h):
        for x in range(w):
            if db[x, y][1] > 0:
                sa += untonemap(da[x, y][1])
                sb += untonemap(db[x, y][1])
    return sa / sb - 1.0 if sb > 0 else float("nan")


def linear_ratio(a, b):
    """Ratio of the linear-light centre luminance of a to b (the neutral tone map is linear in this range,
    after its 0.04 toe offset)."""
    la = luminance([srgb_to_linear(v) for v in centre_mean(a)]) + 0.04
    lb = luminance([srgb_to_linear(v) for v in centre_mean(b)]) + 0.04
    return la / lb if lb > 0 else float("nan")


def validation_issues(model):
    report = OUT / f"{model.stem}_validation.json"
    report.unlink(missing_ok=True)
    run([EXE, model, "--isolated", "--validate-file", model, "--validation-report", report])
    try:
        return json.loads(report.read_text(encoding="utf-8")).get("issues", [])
    except (OSError, ValueError):
        return None


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

    def behind(x, y):
        # Bright only in the hemisphere behind the quad (u = 0.25 lies behind the shapes in Glider's mapping).
        u = (x + 0.5) / 128.0
        du = min(abs(u - 0.25), 1.0 - abs(u - 0.25))
        return (0.6, 0.6, 0.6) if du < 0.25 else (0.0, 0.0, 0.0)
    back_env = ["--env", str(write_hdr(OUT / "behind.hdr", 128, 64, behind))]
    gl_logs = []

    def shot(model, name, flags=(), exe=EXE):
        png, log = render(exe, model, name, list(NO_AA) + list(flags))
        if exe == EXE:
            gl_logs.append((name, log))
        return png

    print("[neutral SSSColor]")
    zero = {"SSSColor": [0.0, 0.0, 0.0, 1.0]}
    for shape, light, env in (("quad", "back", black), ("quad", "front", black), ("sphere", "side", black),
                              ("sphere", "front", black), ("sphere", "side", []), ("quad", "back", back_env)):
        label = f"{shape}_{light}{'_default_env' if not env else ('_behind_env' if env is back_env else '')}"
        a = shot(write(f"black_{label}", sss=zero, shape=shape, light=light), f"black_{label}", env)
        b = shot(write(f"core_{label}", shape=shape, light=light), f"core_{label}", env)
        check(a is not None and same(a, b), f"black SSSColor, {label}: byte-identical to the material without the extension")

    core_back = OUT / "core_quad_back.png"
    core_back_model = OUT / "core_quad_back.gltf"
    core_front = OUT / "core_quad_front.png"
    core_side = OUT / "core_sphere_side.png"
    core_sphere_front = OUT / "core_sphere_front.png"

    print("[back-lit quad]")
    white_back = shot(write("white_back", sss={}, light="back"), "white_back", black)
    core_level = max_level(core_back)
    check(core_level <= 1, f"core material back-lit: black (max level {core_level})")
    lit = centre_mean(white_back)
    check(luminance(lit) > 40, f"SSS (default white) back-lit: transmits light (centre {lit[0]:.1f} {lit[1]:.1f} {lit[2]:.1f})")
    check(max_level(white_back) <= max_level(core_front) + 1,
          f"bounded: back-lit max {max_level(white_back)} <= front-lit core max {max_level(core_front)}")
    ratio = linear_ratio(white_back, core_front)
    check(abs(ratio - 0.5) <= 0.5 * HALF_TOLERANCE,
          f"back-lit / front-lit core (linear) = {ratio:.3f} (translucency 0.5, +-{HALF_TOLERANCE * 100:.0f}%)")
    lit_side = shot(write("white_back", sss={}, light="back"), "white_back_lit_side", black + ["--camera-yaw", "180"])
    core_lit_side = shot(core_back_model, "core_back_lit_side", black + ["--camera-yaw", "180"])
    diff = max_level_diff(lit_side, core_lit_side)
    check(luminance(centre_mean(core_lit_side)) > 40 and diff <= FRONT_TOLERANCE,
          f"back-lit quad seen from the light's side: {diff} levels from the core material (<= {FRONT_TOLERANCE})")
    orange = centre_mean(shot(write("orange_back", sss={"SSSColor": ORANGE}, light="back"), "orange_back", black))
    check(orange[0] > orange[1] > orange[2], f"orange SSSColor tints the transmitted light (R {orange[0]:.1f} > "
                                             f"G {orange[1]:.1f} > B {orange[2]:.1f})")
    rgb3 = shot(write("orange3_back", sss={"SSSColor": ORANGE[:3]}, light="back"), "orange3_back", black)
    check(same(rgb3, OUT / "orange_back.png"), "3-item RGB SSSColor equals the 4-item RGBA list (alpha ignored)")
    two = write("two_items_back", sss={"SSSColor": [0.0, 0.0]}, light="back")
    check(same(shot(two, "two_items_back", black), white_back), "2-item SSSColor: invalid, renders as the default white")
    issues = validation_issues(two)
    flagged = [i for i in (issues or []) if "SSSColor" in i.get("pointer", "") and i.get("severity") == "Warning"]
    check(len(flagged) == 1, f"validator warns about the 2-item SSSColor ({[i.get('message') for i in flagged]})")

    print("[opacityTexture]")
    opaque = shot(write("opacity_white", sss={}, light="back", opacity=(255, 255, 255)), "opacity_white", black)
    check(opaque is not None and same(opaque, core_back), "opacity 1 (white): no translucency, byte-identical to the core material")
    clear = shot(write("opacity_black", sss={}, light="back", opacity=(0, 0, 0)), "opacity_black", black)
    check(clear is not None and same(clear, white_back), "opacity 0 (black): the same as no texture")
    grey = shot(write("opacity_grey", sss={}, light="back", opacity=(128, 128, 128)), "opacity_grey", black)
    half = (luminance([srgb_to_linear(v) for v in centre_mean(grey)]) + 0.04) / \
           (luminance([srgb_to_linear(v) for v in centre_mean(white_back)]) + 0.04)
    check(abs(half - 0.5) <= 0.5 * HALF_TOLERANCE, f"opacity 0.5 (linear texture): {half:.3f} of the transmitted light")
    missing = shot(write("opacity_missing", sss={}, light="back", opacity="missing"), "opacity_missing", black)
    check(missing is not None and same(missing, white_back), "opacity image missing: not read as opaque (same as no texture)")

    print("[wrap: side-lit sphere]")
    side = shot(write("white_side", sss={}, shape="sphere", light="side"), "white_side", black)
    orange_side = shot(write("orange_side", sss={"SSSColor": ORANGE}, shape="sphere", light="side"), "orange_side", black)
    core_term, sss_term = column_mean(core_side, -20), column_mean(side, -20)
    check(core_term < 3 and sss_term > core_term + 10,
          f"wrap lights the region beyond the core terminator (core {core_term:.1f}, SSS {sss_term:.1f})")
    (w, h), od = pixels(orange_side)
    r, g, b = od[w // 2 - 20, h // 2]
    check(r > g > b, f"orange wrap tints the terminator ({r} {g} {b})")
    check(max_level(side) <= max_level(core_sphere_front) + 1 and max_level(orange_side) <= max_level(core_sphere_front) + 1,
          f"bounded: side-lit SSS max {max_level(side)} <= front-lit core max {max_level(core_sphere_front)}")

    print("[front-lit]")
    front = shot(write("white_front", sss={}, light="front"), "white_front", black)
    diff = max_level_diff(front, core_front)
    check(diff <= FRONT_TOLERANCE, f"quad lit straight on: {diff} levels from the core material (<= {FRONT_TOLERANCE})")
    sphere_front = shot(write("white_sphere_front", sss={}, shape="sphere", light="front"), "white_sphere_front", black)
    centre = max(abs(a - b) for a, b in zip(centre_mean(sphere_front, 20), centre_mean(core_sphere_front, 20)))
    check(centre <= FRONT_TOLERANCE, f"sphere lit from the camera, centre (N.L ~ 1): {centre:.2f} levels from the core")
    # The wrap adds w (1 - N.L) / (1 + w) of the light; over a disc lit from the viewer (mean N.L = 2/3) that is
    # w / (2 (1 + w)) = 1/6 of the core diffuse at w = 0.5.
    gain, bound = disc_linear_gain(sphere_front, core_sphere_front), WRAP / (2.0 * (1.0 + WRAP))
    mean, count = mean_diff_over(sphere_front, core_sphere_front, core_sphere_front)
    check(gain <= bound * (1.0 + HALF_TOLERANCE),
          f"sphere lit from the camera: linear gain {gain:.3f} <= wrap bound {bound:.3f} (+{HALF_TOLERANCE * 100:.0f}%); "
          f"mean difference {mean:.2f} levels over {count} px, rim only")

    print("[IBL only]")
    ibl = shot(write("white_ibl", sss={}, light=None), "white_ibl", back_env)
    core_ibl = shot(write("core_ibl", light=None), "core_ibl", back_env)
    a, b = luminance(centre_mean(ibl)), luminance(centre_mean(core_ibl))
    check(a > b + 10, f"environment behind the quad: SSS front {a:.1f} > core front {b:.1f}")
    check(max_level(ibl) < 255, f"finite and not clipped under the IBL ({max_level(ibl)})")

    print("[dither]")
    dithered = shot(write("dither_back", sss={}, light="back", dither=True), "dither_back", black)
    if dithered is not None:
        (w, h), dd = pixels(dithered)
        _, ds = pixels(white_back)
        kept = gone = bad = 0
        for y in range(h // 2 - 60, h // 2 + 60):
            for x in range(w // 2 - 60, w // 2 + 60):
                if dd[x, y] == ds[x, y]:
                    kept += 1
                elif dd[x, y] == (0, 0, 0):
                    gone += 1
                else:
                    bad += 1
        check(bad == 0 and kept > 0 and gone > 0,
              f"dithered SSS: kept pixels equal the undithered SSS ({kept}), others background ({gone}), other {bad}")
    else:
        check(False, "dithered SSS rendered")

    print("[decal receiver]")
    decal = write("decal_back", sss={}, light="back", decal=True)
    decal_png = shot(decal, "decal_back", black)
    diff = max_level_diff(decal_png, white_back) if decal_png else 999
    check(diff <= 1, f"decal buffer active (SSS is a receiver): back-lit SSS unchanged ({diff} levels)")
    report = OUT / "prepass.json"
    report.unlink(missing_ok=True)
    run([EXE, decal, *COMMON, *VIEW, *NO_AA, *black, "--debug-view", "geometrynormal", "--decal-prepass-check", report])
    try:
        data = json.loads(report.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        data = None
    check(data is not None and data.get("decalBuffer", {}).get("active") is True, "decal buffer active for the prepass check")
    if data is not None:
        depth = data.get("depth", {})
        check(depth.get("compared") is True and depth.get("overOneUlp", 1) == 0,
              f"SSS receiver: prepass depth equals forward depth ({depth.get('overOneUlp')} beyond one ulp)")
        check(data.get("glErrors", 1) == 0, "no GL errors in the prepass check")

    print("[switches]")
    sss_back = OUT / "white_back.gltf"
    for label, flags in ((f"--asobo-disable {SSS}", ["--asobo-disable", SSS]), ("--asobo off", ["--asobo", "off"])):
        png = shot(sss_back, "switch_" + flags[-1], black + flags)
        reference = shot(core_back_model, "core_switch_" + flags[-1], black + flags)
        check(png is not None and same(png, reference), f"{label}: byte-identical to the file without the extension")
    other = shot(sss_back, "other_switch", black + ["--asobo-disable", "ASOBO_material_detail_map"])
    check(same(other, white_back), "another extension's switch leaves SSS applied")

    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in {len(gl_logs)} renders {bad}")

    print("[files without the extension]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
    else:
        print(f"  INFO baseline {baseline.name}")
        for name, flags in (("core_quad_back", black), ("core_quad_front", black), ("core_sphere_side", black),
                            ("core_sphere_side_default_env", []), ("core_quad_back_behind_env", back_env)):
            model = OUT / f"{name}.gltf"
            now = shot(model, "now_" + name, flags)
            before = shot(model, "base_" + name, flags, exe=baseline)
            check(now is not None and same(now, before), f"{name}: byte-identical to the baseline {baseline.name}")
        now = common_render(EXE, PLAIN_FIXTURE, "now_hierarchy", [], folder=OUT)
        before = common_render(baseline, PLAIN_FIXTURE, "base_hierarchy", [], folder=OUT)
        check(same(now, before), f"hierarchy fixture: byte-identical to the baseline {baseline.name}")
    return finish()


if __name__ == "__main__":
    sys.exit(main())
