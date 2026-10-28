# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A7.1 (part): KHR_materials_anisotropy and ASOBO_material_anisotropic_v2.

Rule (docs/ASOBO_EXTENSIONS_SEMANTICS.md section 17): KHR_materials_anisotropy follows the Khronos
glTF-Sample-Viewer (anisotropic GGX D/V for punctual lights, bent-normal IBL). ASOBO_material_anisotropic_v2
maps onto the same lobe: direction = anisoDirectionRoughnessTexture.RG * 2 - 1 in tangent space (0.5/0.5 = the
tangent), roughness X = the core roughness (ORM G x factor) along it, roughness Y = texture B across it. The
rougher axis becomes the lobe's stretched axis, so the highlight stretches along the direction when X > Y.

Fixture: a UV sphere (explicit TANGENT along +longitude) of a grey metal, seen head-on, lit by
  * "punctual": a directional KHR_lights_punctual light from the camera under a black environment map, or
  * "ibl": no light, an environment that is black except for one bright spot behind the camera.
The highlight's principal axis comes from the second moments of the pixels brighter than half its peak
(angle 0 = screen horizontal = the projected tangent at the sphere centre, 90 = vertical). Checks:
  * KHR: anisotropyStrength 0 is byte-identical to the material without the extension (light, IBL spot,
    default environment); strength 0.8 stretches the highlight along the tangent (elongation >= MIN_ELONGATION,
    the isotropic one stays <= MAX_ISOTROPIC) under the light and under the IBL; anisotropyRotation pi/2 and a
    direction texture (0.5, 1.0) turn it to the bitangent; without TANGENT the generated tangents give the same
    axis, also under --without-textures (the direction then falls back to the tangent);
  * the Khronos samples AnisotropyBarnLamp/-DiscTest/-RotationTest/-StrengthTest (build/review/anisotropy/khr,
    skipped when absent) render with zero GL errors;
  * ASOBO v2: roughness X == Y with the neutral direction (0.5, 0.5) is byte-identical to the material without
    the extension (a zero strength takes the isotropic code); X > Y stretches along the tangent, X < Y across it;
    RG (0.5, 1.0) / (1.0, 0.5) turn it to the bitangent / tangent; generated tangents (no TANGENT, no normal map)
    work; --asobo-disable ASOBO_material_anisotropic_v2 and --asobo off are byte-identical to the file without
    the extension; --without-textures (no direction/roughness-Y texture) renders isotropic;
  * the anisotropic material as a decal receiver: still stretched, prepass depth == forward depth;
  * picking on the anisotropic sphere; zero GL errors in every render;
  * files without the extension (and the switched-off generated-tangent case) are byte-identical to the
    baseline binary (--baseline-exe).

Run: python scripts/validate_asobo_anisotropy.py [--baseline-exe build/baselines/<sha>.exe]
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
from validate_asobo_common import EXE, PLAIN_FIXTURE, REPO, check, finish, same
from validate_asobo_common import render as common_render

OUT = REPO / "build" / "asobo_anisotropy"
KHR_SAMPLES = REPO / "build" / "review" / "anisotropy" / "khr"
ASOBO = "ASOBO_material_anisotropic_v2"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off"]
VIEW = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
        "--tone-mapping", "neutral", "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0",
        "--camera-distance", "4"]
NO_AA = ["--aa", "none"]
MIN_ELONGATION = 1.6   # major / minor axis ratio of a stretched highlight
MAX_ISOTROPIC = 1.25   # an isotropic highlight's ratio stays below this
ANGLE_TOLERANCE = 20.0  # degrees between the measured principal axis and the expected one
ISO_TOLERANCE = 2       # 8-bit levels: X == Y ASOBO vs. isotropic, if not byte-identical


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


def sphere(add, segments=96, rings=48, tangents=True):
    pos, nrm, uv, tan, idx = [], [], [], [], []
    for r in range(rings + 1):
        theta = math.pi * r / rings          # 0 at +Y
        for s in range(segments + 1):
            phi = 2.0 * math.pi * s / segments
            n = (math.sin(theta) * math.sin(phi), math.cos(theta), math.sin(theta) * math.cos(phi))
            pos.append(n)
            nrm.append(n)
            uv.append((s / segments, r / rings))
            tan.append((math.cos(phi), 0.0, -math.sin(phi), 1.0))   # d/dphi: +U
    for r in range(rings):
        for s in range(segments):
            a = r * (segments + 1) + s
            b = a + segments + 1
            idx += [a, b, a + 1, a + 1, b, b + 1]
    p = add(b"".join(struct.pack("<3f", *v) for v in pos), 34962, len(pos), 5126, "VEC3", ([-1, -1, -1], [1, 1, 1]))
    n = add(b"".join(struct.pack("<3f", *v) for v in nrm), 34962, len(nrm), 5126, "VEC3")
    t = add(b"".join(struct.pack("<2f", *v) for v in uv), 34962, len(uv), 5126, "VEC2")
    attributes = {"POSITION": p, "NORMAL": n, "TEXCOORD_0": t}
    if tangents:
        attributes["TANGENT"] = add(b"".join(struct.pack("<4f", *v) for v in tan), 34962, len(tan), 5126, "VEC4")
    i = add(b"".join(struct.pack("<I", v) for v in idx), 34963, len(idx), 5125, "SCALAR")
    return attributes, i


def document(khr=None, asobo=None, roughness=0.35, light=True, tangents=True, comp_g=None, asobo_missing=False):
    """khr: dict for KHR_materials_anisotropy (may hold "texture": (r, g, b)); asobo: (r, g, b) of the
    direction/roughness-Y texture; comp_g: roughness X through a metallicRoughnessTexture G (factor 1)."""
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

    attributes, indices = sphere(add, tangents=tangents)
    images, textures = [], []

    def texture(rgb):
        # None: an image file that does not exist (the loader substitutes its placeholder).
        images.append({"uri": "missing_anisotropy_texture.png" if rgb is None else png_rgb(rgb)})
        textures.append({"source": len(images) - 1, "sampler": 0})
        return {"index": len(textures) - 1}

    pbr = {"baseColorFactor": [0.9, 0.9, 0.9, 1.0], "metallicFactor": 1.0, "roughnessFactor": roughness}
    if comp_g is not None:
        pbr["roughnessFactor"] = 1.0
        pbr["metallicRoughnessTexture"] = texture((255, comp_g, 255))
    material = {"name": "Metal", "pbrMetallicRoughness": pbr}
    used = []
    extensions = {}
    if khr is not None:
        value = {k: v for k, v in khr.items() if k != "texture"}
        if "texture" in khr:
            value["anisotropyTexture"] = texture(khr["texture"])
        extensions["KHR_materials_anisotropy"] = value
        used.append("KHR_materials_anisotropy")
    if asobo is not None or asobo_missing:
        extensions[ASOBO] = {"anisoDirectionRoughnessTexture": texture(None if asobo_missing else asobo)}
        used.append(ASOBO)
    if extensions:
        material["extensions"] = extensions
    nodes = [{"name": "Sphere", "mesh": 0}]
    doc = {
        "asset": {"version": "2.0", "generator": "A7.1 anisotropy fixture"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": nodes,
        "meshes": [{"name": "Sphere", "primitives": [{"attributes": attributes, "indices": indices, "material": 0}]}],
        "materials": [material],
        "accessors": accessors, "bufferViews": views,
        "buffers": [{"byteLength": len(blob),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode("ascii")}],
    }
    if images:
        doc["images"], doc["textures"] = images, textures
        doc["samplers"] = [{"magFilter": 9728, "minFilter": 9728}]
    if light:
        used.append("KHR_lights_punctual")
        doc["extensions"] = {"KHR_lights_punctual": {"lights": [{"type": "directional", "intensity": 3.0,
                                                                  "color": [1, 1, 1]}]}}
        # Directional lights shine along the node's -Z; the node sits on the camera side, unrotated.
        nodes.append({"name": "Light", "translation": [0, 0, 5],
                      "extensions": {"KHR_lights_punctual": {"light": 0}}})
        doc["scenes"][0]["nodes"].append(1)
    if used:
        doc["extensionsUsed"] = sorted(set(used))
    return doc


def write(name, **kwargs):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(document(**kwargs)), encoding="utf-8")
    return path


def run(args, timeout=600):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(exe, model, name, flags=(), frame=24, picks=()):
    """Viewport capture; returns (png or None, profile report or None, stdout)."""
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    extra = [a for p in picks for a in ("--report-pick", f"{p[0]},{p[1]}")]
    result = run([exe, model, *COMMON, *VIEW, *flags, *extra, "--capture-viewport", png, "--capture-frame", str(frame),
                  "--profile-report", report, "--exit-after-profile"])
    data = None
    try:
        data = json.loads(report.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        pass
    return (png if png.exists() else None), data, result.stdout + result.stderr


def gl_errors_zero(log):
    lines = [line for line in log.splitlines() if line.startswith("GL automation errors")]
    return lines == ["GL automation errors: 0"] and "GL error 0x" not in log


def highlight_axis(png):
    """(angle in degrees in [0, 180), elongation major/minor, peak, pixel count) of the bright highlight."""
    from PIL import Image
    image = Image.open(png).convert("RGB")
    w, h = image.size
    data = image.load()
    lum = [[0.0] * w for _ in range(h)]
    peak = 0.0
    for y in range(h):
        for x in range(w):
            r, g, b = data[x, y]
            v = 0.2126 * r + 0.7152 * g + 0.0722 * b
            lum[y][x] = v
            peak = max(peak, v)
    if peak <= 0.0:
        return None
    threshold = 0.5 * peak
    sw = sx = sy = sxx = syy = sxy = 0.0
    for y in range(h):
        for x in range(w):
            v = lum[y][x]
            if v >= threshold:
                wgt = v - threshold
                sw += wgt
                sx += wgt * x
                sy += wgt * y
                sxx += wgt * x * x
                syy += wgt * y * y
                sxy += wgt * x * y
    if sw <= 0.0:
        return None
    mx, my = sx / sw, sy / sw
    cxx, cyy, cxy = sxx / sw - mx * mx, syy / sw - my * my, sxy / sw - mx * my
    tr, det = cxx + cyy, cxx * cyy - cxy * cxy
    disc = math.sqrt(max(tr * tr / 4.0 - det, 0.0))
    l1, l2 = tr / 2.0 + disc, max(tr / 2.0 - disc, 1e-9)
    angle = math.degrees(0.5 * math.atan2(2.0 * cxy, cxx - cyy)) % 180.0
    # Image y grows downwards: report the angle counter-clockwise on screen.
    angle = (180.0 - angle) % 180.0
    count = sum(1 for row in lum for v in row if v >= threshold)
    return angle, math.sqrt(l1 / l2), peak, count, (mx, my)


def angle_close(a, b, tolerance=ANGLE_TOLERANCE):
    d = abs(a - b) % 180.0
    return min(d, 180.0 - d) <= tolerance


def max_level_diff(a, b):
    from PIL import Image, ImageChops
    ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    if ia.size != ib.size:
        return 999
    return max(hi for lo, hi in ImageChops.difference(ia, ib).getextrema())


def spot_environment():
    """Black equirectangular map with one bright spot behind the camera, so its reflection sits at the sphere
    centre (u = 0.75 in Glider's mapping, found with a mirror sphere; u = 0.25 lies behind the sphere)."""
    def pixel(x, y):
        u, v = (x + 0.5) / 256.0, (y + 0.5) / 128.0
        du = min(abs(u - 0.75), 1.0 - abs(u - 0.75)) * 2.0 * math.pi
        dv = (v - 0.5) * math.pi
        s = 40.0 * math.exp(-(du * du + dv * dv) / (2.0 * 0.05 ** 2))
        return (s, s, s)
    return write_hdr(OUT / "spot.hdr", 256, 128, pixel)


def decal_document(**kwargs):
    """The fixture plus a zero-area geometry decal, which activates the decal buffer and the prepass."""
    doc = document(**kwargs)
    blob = bytearray(base64.b64decode(doc["buffers"][0]["uri"].split(",", 1)[1]))
    while len(blob) % 4:
        blob.append(0)
    offset = len(blob)
    blob += struct.pack("<3f", 0, 0, -5) * 3 + struct.pack("<3f", 0, 0, 1) * 3
    doc["bufferViews"] += [{"buffer": 0, "byteOffset": offset, "byteLength": 36, "target": 34962},
                           {"buffer": 0, "byteOffset": offset + 36, "byteLength": 36, "target": 34962}]
    n = len(doc["accessors"])
    doc["accessors"] += [{"bufferView": len(doc["bufferViews"]) - 2, "componentType": 5126, "count": 3, "type": "VEC3",
                          "min": [0, 0, -5], "max": [0, 0, -5]},
                         {"bufferView": len(doc["bufferViews"]) - 1, "componentType": 5126, "count": 3, "type": "VEC3"}]
    doc["buffers"][0] = {"byteLength": len(blob),
                         "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode("ascii")}
    doc["materials"].append({"name": "Decal", "alphaMode": "BLEND", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 0]},
                             "extensions": {"ASOBO_material_geometry_decal": {},
                                            "ASOBO_material_draw_order": {"drawOrderOffset": 10}}})
    doc["meshes"].append({"name": "Decal", "primitives": [{"attributes": {"POSITION": n, "NORMAL": n + 1},
                                                           "material": len(doc["materials"]) - 1}]})
    doc["nodes"].append({"name": "Decal", "mesh": len(doc["meshes"]) - 1})
    doc["scenes"][0]["nodes"].append(len(doc["nodes"]) - 1)
    doc["extensionsUsed"] = sorted(set(doc.get("extensionsUsed", [])) | {"ASOBO_material_geometry_decal",
                                                                        "ASOBO_material_draw_order"})
    return doc


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
    spot = ["--env", str(spot_environment())]
    gl_logs = []

    def shot(model, name, flags=(), exe=EXE, picks=()):
        png, report, log = render(exe, model, name, list(flags), picks=picks)
        if exe == EXE:
            gl_logs.append((name, log))
        return png, report

    def axis(png):
        result = highlight_axis(png) if png is not None else None
        return result if result is not None else (float("nan"), 0.0, 0.0, 0, (0, 0))

    def stretched(png, expected, label):
        angle, elongation, _, count, _ = axis(png)
        check(elongation >= MIN_ELONGATION and angle_close(angle, expected),
              f"{label}: highlight axis {angle:.1f} deg (expected {expected:.0f}), elongation {elongation:.2f} "
              f"(>= {MIN_ELONGATION}), {count} px")

    def round_highlight(png, label):
        _, elongation, _, count, _ = axis(png)
        check(elongation <= MAX_ISOTROPIC, f"{label}: round highlight, elongation {elongation:.2f} (<= {MAX_ISOTROPIC})")

    # Lit fixtures have a directional light from the camera (black environment); *_ibl ones have no
    # light and use the spot environment.
    plain = write("plain")
    plain_ibl = write("plain_ibl", light=False)
    khr0 = write("khr_strength0", khr={"anisotropyStrength": 0.0, "anisotropyRotation": 0.7})
    khr0_ibl = write("khr_strength0_ibl", light=False, khr={"anisotropyStrength": 0.0, "anisotropyRotation": 0.7})
    khr = write("khr", khr={"anisotropyStrength": 0.8})
    khr_ibl = write("khr_ibl", light=False, khr={"anisotropyStrength": 0.8})
    khr_rot = write("khr_rot90", khr={"anisotropyStrength": 0.8, "anisotropyRotation": math.pi / 2})
    khr_rot_ibl = write("khr_rot90_ibl", light=False, khr={"anisotropyStrength": 0.8, "anisotropyRotation": math.pi / 2})
    khr_tex = write("khr_tex_v", khr={"anisotropyStrength": 0.8, "texture": (128, 255, 255)})
    khr_notan = write("khr_no_tangent", tangents=False, khr={"anisotropyStrength": 0.8})

    print("[KHR_materials_anisotropy]")
    for label, a, b, flags in (("punctual", khr0, plain, black), ("ibl", khr0_ibl, plain_ibl, spot),
                               ("default_env", khr0, plain, [])):
        pa, _ = shot(a, f"khr0_{label}", NO_AA + flags)
        pb, _ = shot(b, f"plain_{label}", NO_AA + flags)
        check(pa is not None and same(pa, pb), f"strength 0 ({label}): byte-identical to the material without the extension")
    iso_png, _ = shot(plain, "plain_punctual_ref", NO_AA + black)
    round_highlight(iso_png, "isotropic reference (punctual)")
    iso_ibl, _ = shot(plain_ibl, "plain_ibl_ref", NO_AA + spot)
    round_highlight(iso_ibl, "isotropic reference (IBL spot)")
    stretched(shot(khr, "khr_punctual", NO_AA + black)[0], 0.0, "strength 0.8, punctual: stretched along the tangent")
    stretched(shot(khr_ibl, "khr_ibl", NO_AA + spot)[0], 0.0, "strength 0.8, IBL: stretched along the tangent")
    stretched(shot(khr_rot, "khr_rot90_punctual", NO_AA + black)[0], 90.0, "rotation pi/2, punctual: turned to the bitangent")
    stretched(shot(khr_rot_ibl, "khr_rot90_ibl", NO_AA + spot)[0], 90.0, "rotation pi/2, IBL: turned to the bitangent")
    stretched(shot(khr_tex, "khr_tex_v", NO_AA + black)[0], 90.0, "texture direction (0.5, 1.0): along the bitangent")
    stretched(shot(khr_notan, "khr_no_tangent", NO_AA + black)[0], 0.0, "no TANGENT attribute (generated tangents)")
    stretched(shot(khr_notan, "khr_no_tangent_without_textures", NO_AA + black + ["--without-textures"])[0], 0.0,
              "--without-textures, no TANGENT: direction falls back to the generated tangent")

    print("[KHR display switch]")
    ui_flags = ["--isolated", "--window-size", "1920x1080", "--fixed-frame-time", "0.05", "--vsync", "off",
                *VIEW, *NO_AA, *black, "--select-material", "0"]
    script, ui_report, ui_png = (OUT / f"ui_switch{suffix}" for suffix in (".txt", ".json", ".png"))
    script.write_text("\n".join(["wait 8", "clickp khr.display.KHR_materials_anisotropy 0", "wait 10", "snap final",
                                 "move 900 20", "wait 220"]), encoding="utf-8")
    ui_report.unlink(missing_ok=True)
    ui_png.unlink(missing_ok=True)
    result = run([EXE, khr, *ui_flags, "--ui-script", script, "--ui-report", ui_report, "--capture-viewport", ui_png,
                  "--capture-frame", "180"])
    try:
        ui_data = json.loads(ui_report.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        ui_data = {}
    snaps = {snap["label"]: snap for snap in ui_data.get("snaps", [])}
    check(result.returncode == 0 and not ui_data.get("failed", True)
          and "GL error 0x" not in result.stdout + result.stderr, f"UI run completed {ui_data.get('errors')}")
    probe = next((p for p in snaps.get("final", {}).get("probes", [])
                  if p.get("tag") == "khr.display.KHR_materials_anisotropy"), None)
    check(probe is not None and probe.get("selected") is False,
          "Inspector: KHR_materials_anisotropy has a per-material display switch, now off")
    reference = OUT / "ui_reference.png"
    reference.unlink(missing_ok=True)
    run([EXE, plain, *ui_flags, "--capture-viewport", reference, "--capture-frame", "180", "--profile-report",
         OUT / "ui_reference.json", "--exit-after-profile"])
    check(same(ui_png, reference), "display switch off: byte-identical to the material without the extension")

    print("[KHR samples]")
    samples = ["AnisotropyBarnLamp", "AnisotropyDiscTest", "AnisotropyRotationTest", "AnisotropyStrengthTest"]
    if not all((KHR_SAMPLES / f"{s}.glb").exists() for s in samples):
        print(f"  SKIP Khronos samples not found under {KHR_SAMPLES}")
    else:
        for sample in samples:
            png = OUT / f"sample_{sample}.png"
            png.unlink(missing_ok=True)
            result = run([EXE, KHR_SAMPLES / f"{sample}.glb", *COMMON, "--hide-grid", "--aa", "none",
                          "--capture-viewport", png, "--capture-frame", "24",
                          "--profile-report", OUT / f"sample_{sample}.json", "--exit-after-profile"])
            check(png.exists() and gl_errors_zero(result.stdout + result.stderr), f"{sample}: renders, zero GL errors")

    print(f"[{ASOBO}]")
    comp = 150  # roughness X = 150/255 through the ORM G channel (factor 1)
    asobo_plain = write("asobo_plain", comp_g=comp)
    asobo_plain_ibl = write("asobo_plain_ibl", light=False, comp_g=comp)
    equal = write("asobo_x_eq_y", comp_g=comp, asobo=(128, 128, comp))
    equal_ibl = write("asobo_x_eq_y_ibl", light=False, comp_g=comp, asobo=(128, 128, comp))
    x_gt_y = write("asobo_x_gt_y", comp_g=comp, asobo=(128, 128, 30))
    x_gt_y_ibl = write("asobo_x_gt_y_ibl", light=False, comp_g=comp, asobo=(128, 128, 30))
    x_lt_y = write("asobo_x_lt_y", comp_g=40, asobo=(128, 128, 200))
    dir_v = write("asobo_dir_v", comp_g=comp, asobo=(128, 255, 30))
    dir_h = write("asobo_dir_h", comp_g=comp, asobo=(255, 128, 30))
    no_tangent = write("asobo_no_tangent", tangents=False, comp_g=comp, asobo=(128, 128, 30))
    no_tangent_plain = write("asobo_no_tangent_plain", tangents=False, comp_g=comp)

    for label, a, b, flags in (("punctual", equal, asobo_plain, black), ("ibl", equal_ibl, asobo_plain_ibl, spot)):
        pa, _ = shot(a, f"asobo_x_eq_y_{label}", NO_AA + flags)
        pb, _ = shot(b, f"asobo_plain_{label}", NO_AA + flags)
        diff = max_level_diff(pa, pb) if pa and pb else 999
        check(diff == 0, f"roughness X == Y, neutral direction ({label}): byte-identical to the isotropic material "
                         f"(max diff {diff} levels)")
    stretched(shot(x_gt_y, "asobo_x_gt_y", NO_AA + black)[0], 0.0, "X > Y, neutral direction, punctual: along the tangent")
    stretched(shot(x_gt_y_ibl, "asobo_x_gt_y_ibl", NO_AA + spot)[0], 0.0, "X > Y, neutral direction, IBL: along the tangent")
    stretched(shot(x_lt_y, "asobo_x_lt_y", NO_AA + black)[0], 90.0, "X < Y, neutral direction: across (the bitangent)")
    stretched(shot(dir_v, "asobo_dir_v", NO_AA + black)[0], 90.0, "direction RG (0.5, 1.0), X > Y: along the bitangent")
    stretched(shot(dir_h, "asobo_dir_h", NO_AA + black)[0], 0.0, "direction RG (1.0, 0.5), X > Y: along the tangent")
    stretched(shot(no_tangent, "asobo_no_tangent", NO_AA + black)[0], 0.0,
              "no TANGENT, no normal map (generated tangents): along the tangent")

    print("[ASOBO switches]")
    for label, flags in ((f"--asobo-disable {ASOBO}", ["--asobo-disable", ASOBO]), ("--asobo off", ["--asobo", "off"])):
        png, _ = shot(x_gt_y, "asobo_switch_" + flags[-1], NO_AA + black + flags)
        reference, _ = shot(asobo_plain, "asobo_plain_switch_" + flags[-1], NO_AA + black + flags)
        check(png is not None and same(png, reference), f"{label}: byte-identical to the file without the extension")
    other, _ = shot(x_gt_y, "asobo_other_switch", NO_AA + black + ["--asobo-disable", "ASOBO_material_detail_map"])
    check(same(other, OUT / "asobo_x_gt_y.png"), "another extension's switch leaves the anisotropy applied")
    missing, _ = shot(write("asobo_missing_texture", comp_g=comp, asobo_missing=True), "asobo_missing_texture",
                      NO_AA + black)
    check(missing is not None and same(missing, OUT / "asobo_plain_punctual.png"),
          "image of the direction/roughness-Y texture missing: isotropic (the placeholder is not read as a direction)")
    khr_missing, _ = shot(write("khr_missing_texture", khr={"anisotropyStrength": 0.8, "texture": None}),
                          "khr_missing_texture", NO_AA + black)
    check(khr_missing is not None and same(khr_missing, OUT / "khr_punctual.png"),
          "KHR texture image missing: renders like strength 0.8 without a texture")
    without, _ = shot(x_gt_y, "asobo_without_textures", NO_AA + black + ["--without-textures"])
    without_plain, _ = shot(asobo_plain, "asobo_plain_without_textures", NO_AA + black + ["--without-textures"])
    check(without is not None and same(without, without_plain),
          "--without-textures: no direction/roughness-Y texture, so isotropic (byte-identical to the file without it)")

    print("[decal receiver]")
    decal_model = OUT / "asobo_decal.gltf"
    decal_model.write_text(json.dumps(decal_document(comp_g=comp, asobo=(128, 128, 30))), encoding="utf-8")
    stretched(shot(decal_model, "asobo_decal", NO_AA + black)[0], 0.0, "with the decal buffer active (receiver): still stretched")
    report = OUT / "prepass.json"
    report.unlink(missing_ok=True)
    run([EXE, decal_model, *COMMON, *VIEW, *NO_AA, *black, "--debug-view", "geometrynormal", "--decal-prepass-check", report])
    try:
        data = json.loads(report.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        data = None
    check(data is not None and data.get("decalBuffer", {}).get("active") is True, "decal buffer active for the prepass check")
    if data is not None:
        depth = data.get("depth", {})
        check(depth.get("compared") is True and depth.get("overOneUlp", 1) == 0,
              f"anisotropic receiver: prepass depth equals forward depth ({depth.get('overOneUlp')} beyond one ulp)")
        check(data.get("glErrors", 1) == 0, "no GL errors in the prepass check")

    print("[picking]")
    if iso_png is not None:
        from PIL import Image
        w, h = Image.open(iso_png).size
        png, report = shot(x_gt_y, "asobo_pick", NO_AA + black, picks=[(w // 2, h // 2), (2, 2)])
        picks = (report or {}).get("picks", [])
        got = [p.get("nodeName") if isinstance(p, dict) else None for p in picks]
        check(len(got) == 2 and got[0] == "Sphere" and got[1] in (None, ""),
              f"picking on the anisotropic sphere: the centre picks the sphere, a corner nothing ({got})")
        check(same(png, OUT / "asobo_x_gt_y.png"), "the picking run draws the same image")

    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in {len(gl_logs)} renders {bad}")

    print("[files without the extension]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
    else:
        print(f"  INFO baseline {baseline.name}")
        for name, model, flags in (("plain", plain, black), ("plain_default_env", plain, []),
                                   ("plain_ibl", plain_ibl, spot), ("asobo_plain", asobo_plain, black),
                                   ("no_tangent_plain", no_tangent_plain, black)):
            now, _ = shot(model, "now_" + name, NO_AA + flags)
            before, _ = shot(model, "base_" + name, NO_AA + flags, exe=baseline)
            check(now is not None and same(now, before), f"{name}: byte-identical to the baseline {baseline.name}")
        now, _ = shot(no_tangent, "now_no_tangent_off", NO_AA + black + ["--asobo-disable", ASOBO])
        before, _ = shot(no_tangent, "base_no_tangent", NO_AA + black, exe=baseline)
        check(now is not None and same(now, before),
              "generated tangents for an ASOBO material without a normal map: switch-off equals the baseline")
        now = common_render(EXE, PLAIN_FIXTURE, "now_hierarchy", [], folder=OUT)
        before = common_render(baseline, PLAIN_FIXTURE, "base_hierarchy", [], folder=OUT)
        check(same(now, before), f"hierarchy fixture: byte-identical to the baseline {baseline.name}")
    return finish()


if __name__ == "__main__":
    sys.exit(main())
