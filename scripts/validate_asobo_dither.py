# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A1.7: ASOBO_material_alphamode_dither (screen-door transparency).

Rule (docs/ASOBO_EXTENSIONS_SEMANTICS.md section 6; the SDK only says "blended with the background using a
dithering pattern", so the pattern is Glider's choice): an OPAQUE material (or the exporter's literal core
"DITHER") with the extension discards a pixel where alpha <= threshold, alpha being the standard alpha
(base colour factor x texture x detail x vertex), threshold = (bayer4x4(x, y) + 0.5) / 16 without TAA; under
TAA every accumulation frame adds frame * 0.618 (mod 1). Opaque pass, depth writes, no blending.

Fixture: a green unlit backdrop behind seven red unlit quads (alpha 0, 0.25, 0.5, 0.75, 1, vertex alpha 0.5,
texture alpha 128/255), on a blue clear colour, no MSAA/post AA. Checks:
  * coverage: every 4x4 window inside a quad keeps exactly round(16 alpha) red pixels (no TAA);
  * alpha 1 is byte-identical to the OPAQUE quads without the extension; alpha 0 draws nothing;
  * core "DITHER" without the extension renders like the extension; {"enabled": false},
    --asobo-disable and --asobo off render byte-identical to the file without the extension;
  * TAA: colours are 0.5 (red quads, green backdrop) so the neutral tone mapper is the identity and each
    pixel's TAA coverage e can be read back exactly by undoing the TAA's Karis weighting
    (e = (C_r / (1 + luma C)) / (R / (1 + luma R))). After settling, the mean e of each quad is within
    TAA_MEAN_TOLERANCE of alpha, every pixel within TAA_PIXEL_TOLERANCE (the stationary EMA weight 0.1
    bounds a binary sequence's residual; the golden-ratio offset sequence gives at most 0.097 in simulation),
    and two later frames are identical (stable). The Karis weighting itself makes the resolved colour differ
    from a linear alpha blend; that difference is printed, not checked;
  * picking: a dithered-away pixel picks the backdrop, a kept pixel picks the quad;
  * the decal prepass discards the same pixels (prepass depth == forward depth, --decal-prepass-check);
  * the validator reports core "DITHER" as an info, not an error; zero GL errors;
  * files without the extension are byte-identical to the baseline binary (--baseline-exe).

Run: python scripts/validate_asobo_dither.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import io
import json
import struct
import sys
from pathlib import Path

import harness_process as subprocess
from validate_asobo_common import EXE, PLAIN_FIXTURE, REPO, check, finish, same
from validate_asobo_common import render as common_render

OUT = REPO / "build" / "asobo_dither"
EXT = "ASOBO_material_alphamode_dither"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off"]
VIEW = ["--clear-color", "0,0,1", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
        "--tone-mapping", "neutral", "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0",
        "--camera-distance", "6"]
NO_AA = ["--aa", "none"]
TAA = ["--msaa", "0", "--post-aa", "taa"]
QUADS = [("A000", 0.0, "factor"), ("A025", 0.25, "factor"), ("A050", 0.5, "factor"), ("A075", 0.75, "factor"),
         ("A100", 1.0, "factor"), ("V050", 0.5, "vertex"), ("T050", 128.0 / 255.0, "texture")]
TAA_MEAN_TOLERANCE = 0.02  # coverage, mean over the quad interior
TAA_PIXEL_TOLERANCE = 0.12  # coverage, any pixel of the quad interior
LEVEL = 0.5  # linear colour of the quads (red) and backdrop (green)
INSET = 6  # pixels trimmed from each quad's screen rectangle


def png_rgba(rgba):
    from PIL import Image
    data = io.BytesIO()
    Image.new("RGBA", (1, 1), rgba).save(data, format="PNG")
    return base64.b64encode(data.getvalue()).decode("ascii")


def document(mode="ext", ext_value=None, alpha_mode="OPAQUE", decal=False):
    """mode: 'ext' (extension on every quad), 'none' (no extension), 'core' (alphaMode DITHER, no extension)."""
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

    def quad(half_w, half_h, z):
        pos = [(-half_w, -half_h, z), (half_w, -half_h, z), (half_w, half_h, z), (-half_w, half_h, z)]
        p = add(b"".join(struct.pack("<3f", *v) for v in pos), 34962, 4, 5126, "VEC3",
                ([-half_w, -half_h, z], [half_w, half_h, z]))
        n = add(struct.pack("<3f", 0, 0, 1) * 4, 34962, 4, 5126, "VEC3")
        uv = add(b"".join(struct.pack("<2f", *t) for t in [(0, 1), (1, 1), (1, 0), (0, 0)]), 34962, 4, 5126, "VEC2")
        return p, n, uv

    indices = add(struct.pack("<6H", 0, 1, 2, 0, 2, 3), 34963, 6, 5123, "SCALAR")
    backdrop = quad(4.5, 1.5, -0.5)
    front = quad(0.4, 0.4, 0.0)
    colour = add(struct.pack("<4f", 1, 1, 1, 0.5) * 4, 34962, 4, 5126, "VEC4")

    materials = [{"name": "Backdrop", "pbrMetallicRoughness": {"baseColorFactor": [0, LEVEL, 0, 1]},
                  "extensions": {"KHR_materials_unlit": {}}}]
    meshes = [{"name": "Backdrop", "primitives": [{"attributes": {"POSITION": backdrop[0], "NORMAL": backdrop[1]},
                                                   "indices": indices, "material": 0}]}]
    nodes = [{"name": "Backdrop", "mesh": 0}]
    for i, (name, alpha, source) in enumerate(QUADS):
        pbr = {"baseColorFactor": [LEVEL, 0, 0, alpha if source == "factor" else 1.0]}
        if source == "texture":
            pbr["baseColorTexture"] = {"index": 0}
        material = {"name": name, "pbrMetallicRoughness": pbr, "extensions": {"KHR_materials_unlit": {}}}
        if mode == "ext":
            material["extensions"][EXT] = dict(ext_value or {})
        if mode == "core":
            material["alphaMode"] = "DITHER"
        elif alpha_mode != "OPAQUE":
            material["alphaMode"] = alpha_mode
        materials.append(material)
        attributes = {"POSITION": front[0], "NORMAL": front[1], "TEXCOORD_0": front[2]}
        if source == "vertex":
            attributes["COLOR_0"] = colour
        meshes.append({"name": name, "primitives": [{"attributes": attributes, "indices": indices,
                                                     "material": len(materials) - 1}]})
        nodes.append({"name": name, "mesh": len(meshes) - 1, "translation": [-3.0 + i, 0.0, 0.0]})
    used = {"KHR_materials_unlit"}
    if mode == "ext":
        used.add(EXT)
    if decal:
        # A zero-area decal triangle: it activates the decal buffer and prepass without drawing.
        tri = add(struct.pack("<3f", 0, 0, -5) * 3, 34962, 3, 5126, "VEC3", ([0, 0, -5], [0, 0, -5]))
        normal = add(struct.pack("<3f", 0, 0, 1) * 3, 34962, 3, 5126, "VEC3")
        materials.append({"name": "Decal", "alphaMode": "BLEND", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 0]},
                          "extensions": {"ASOBO_material_geometry_decal": {}, "ASOBO_material_draw_order": {"drawOrderOffset": 10}}})
        meshes.append({"name": "Decal", "primitives": [{"attributes": {"POSITION": tri, "NORMAL": normal},
                                                        "material": len(materials) - 1}]})
        nodes.append({"name": "Decal", "mesh": len(meshes) - 1})
        used |= {"ASOBO_material_geometry_decal", "ASOBO_material_draw_order"}
    return {
        "asset": {"version": "2.0", "generator": "A1.7 dither fixture"},
        "extensionsUsed": sorted(used),
        "scene": 0, "scenes": [{"nodes": list(range(len(nodes)))}],
        "nodes": nodes, "meshes": meshes, "materials": materials,
        "textures": [{"source": 0, "sampler": 0}],
        "samplers": [{"magFilter": 9728, "minFilter": 9728}],
        "images": [{"uri": "data:image/png;base64," + png_rgba((255, 0, 0, 128))}],
        "accessors": accessors, "bufferViews": views,
        "buffers": [{"byteLength": len(blob),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode("ascii")}],
    }


def write(name, **kwargs):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(document(**kwargs)), encoding="utf-8")
    return path


def run(args, timeout=600):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(exe, model, name, flags=(), frame=24, picks=(), nodes=()):
    """Viewport capture at `frame`; returns (png or None, profile report dict or None)."""
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    extra = [a for p in picks for a in ("--report-pick", f"{p[0]},{p[1]}")]
    extra += [a for n in nodes for a in ("--report-node", n)]
    run([exe, model, *COMMON, *VIEW, *flags, *extra, "--capture-viewport", png, "--capture-frame", str(frame),
         "--profile-report", report, "--exit-after-profile"])
    data = None
    try:
        data = json.loads(report.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        pass
    return (png if png.exists() else None), data


def is_red(p):
    return p[0] > 100 and p[1] < 60


def is_backdrop(p):
    return p[1] > 100 and p[0] < 60


def rects_from(report):
    rects = {}
    for node in (report or {}).get("nodes", []):
        x0, y0, x1, y1 = node["screenRect"]
        rects[node["name"]] = (int(x0) + INSET, int(y0) + INSET, int(x1) - INSET, int(y1) - INSET)
    return rects


def window_counts(image, rect):
    """Set of red-pixel counts over every 4x4 window inside rect."""
    x0, y0, x1, y1 = rect
    red = [[1 if is_red(image.getpixel((x, y))) else 0 for x in range(x0, x1)] for y in range(y0, y1)]
    counts = set()
    for y in range(len(red) - 3):
        for x in range(len(red[0]) - 3):
            counts.add(sum(red[y + j][x + i] for j in range(4) for i in range(4)))
    return counts, sum(map(sum, red)) / max(1, len(red) * len(red[0]))


def mean_rgb(image, rect):
    x0, y0, x1, y1 = rect
    total, count = [0.0, 0.0, 0.0], 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            p = image.getpixel((x, y))
            for c in range(3):
                total[c] += p[c]
            count += 1
    return [t / max(1, count) for t in total]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=None)
    args = parser.parse_args()
    baseline = Path(args.baseline_exe) if args.baseline_exe else None
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None
    from PIL import Image

    names = [q[0] for q in QUADS]
    ext_model = write("dither_ext")
    plain_model = write("dither_none", mode="none")

    print("[coverage without TAA]")
    plain_png, plain_report = render(EXE, plain_model, "none", NO_AA, nodes=names + ["Backdrop"])
    ext_png, ext_report = render(EXE, ext_model, "ext", NO_AA, nodes=names)
    rects = rects_from(plain_report)
    check(plain_png is not None and ext_png is not None and len(rects) == len(QUADS) + 1,
          f"captures and quad rectangles ({len(rects)} nodes)")
    if plain_png is None or ext_png is None or len(rects) != len(QUADS) + 1:
        return finish()
    plain_image = Image.open(plain_png).convert("RGB")
    image = Image.open(ext_png).convert("RGB")
    for name, alpha, source in QUADS:
        rect = rects[name]
        check(rect[2] - rect[0] > 40 and rect[3] - rect[1] > 40, f"{name}: quad interior {rect} is large enough")
        plain_counts, plain_fraction = window_counts(plain_image, rect)
        check(plain_counts == {16}, f"{name}: without the extension the quad is fully opaque ({plain_fraction:.3f})")
        counts, fraction = window_counts(image, rect)
        wanted = sum(1 for k in range(16) if alpha > (k + 0.5) / 16.0)
        check(counts == {wanted}, f"{name} ({source} alpha {alpha:.3f}): every 4x4 window keeps {wanted}/16 pixels "
                                  f"(found {sorted(counts)}, covered fraction {fraction:.4f})")
    x0, y0, x1, y1 = rects["A000"]
    zero_backdrop = all(is_backdrop(image.getpixel((x, y))) for y in range(y0, y1) for x in range(x0, x1))
    check(zero_backdrop, "alpha 0: nothing drawn, the backdrop shows everywhere")
    x0, y0, x1, y1 = rects["A100"]
    region = (x0 - INSET - 4, y0 - INSET - 4, x1 + INSET + 4, y1 + INSET + 4)
    check(plain_image.crop(region).tobytes() == image.crop(region).tobytes(),
          "alpha 1: the quad region is byte-identical to the OPAQUE quad without the extension")
    only_opaque = write("dither_alpha1_ext", mode="ext")
    doc = json.loads(only_opaque.read_text(encoding="utf-8"))
    for material in doc["materials"][1:]:
        material["pbrMetallicRoughness"]["baseColorFactor"][3] = 1.0
    doc["meshes"][6]["primitives"][0]["attributes"].pop("COLOR_0")
    doc["materials"][7]["pbrMetallicRoughness"].pop("baseColorTexture")
    only_opaque.write_text(json.dumps(doc), encoding="utf-8")
    for material in doc["materials"][1:]:
        material["extensions"].pop(EXT)
    doc["extensionsUsed"].remove(EXT)
    only_opaque_plain = OUT / "dither_alpha1_none.gltf"
    only_opaque_plain.write_text(json.dumps(doc), encoding="utf-8")
    a1, _ = render(EXE, only_opaque, "alpha1_ext", NO_AA)
    a1_plain, _ = render(EXE, only_opaque_plain, "alpha1_none", NO_AA)
    check(same(a1, a1_plain), "alpha 1 everywhere: whole image byte-identical to OPAQUE without the extension")

    print("[core DITHER and switches]")
    core_png, _ = render(EXE, write("dither_core", mode="core"), "core", NO_AA)
    check(same(core_png, ext_png), "core alphaMode \"DITHER\" without the extension renders like the extension")
    core_off, _ = render(EXE, write("dither_core_disabled", mode="core"), "core_switch_off",
                         NO_AA + ["--asobo-disable", EXT])
    check(same(core_off, plain_png), "core \"DITHER\" with the switch off renders the OPAQUE result")
    disabled_png, _ = render(EXE, write("dither_enabled_false", ext_value={"enabled": False}), "enabled_false", NO_AA)
    check(same(disabled_png, plain_png), "{\"enabled\": false}: byte-identical to the file without the extension")
    explicit_png, _ = render(EXE, write("dither_enabled_true", ext_value={"enabled": True}), "enabled_true", NO_AA)
    check(same(explicit_png, ext_png), "{\"enabled\": true} renders like {}")
    switch_png, _ = render(EXE, ext_model, "switch_off", NO_AA + ["--asobo-disable", EXT])
    check(same(switch_png, plain_png), f"--asobo-disable {EXT}: byte-identical to the file without the extension")
    master_png, _ = render(EXE, ext_model, "master_off", NO_AA + ["--asobo", "off"])
    check(same(master_png, plain_png), "--asobo off: byte-identical to the file without the extension")
    other_png, _ = render(EXE, ext_model, "other_off", NO_AA + ["--asobo-disable", "ASOBO_material_detail_map"])
    check(same(other_png, ext_png), "another extension's switch leaves the dither applied")

    print("[picking]")
    x0, y0, x1, y1 = rects["A050"]
    cy = (y0 + y1) // 2
    row = [(x, cy) for x in range(x0 + 8, x0 + 16)]
    away = next((p for p in row if is_backdrop(image.getpixel(p))), None)
    kept = next((p for p in row if is_red(image.getpixel(p))), None)
    zero = ((rects["A000"][0] + rects["A000"][2]) // 2, (rects["A000"][1] + rects["A000"][3]) // 2)
    check(away is not None and kept is not None, f"alpha 0.5 row has a dithered-away {away} and a kept pixel {kept}")
    if away is not None and kept is not None:
        pick_png, pick_report = render(EXE, ext_model, "pick", NO_AA, picks=[away, kept, zero])
        picks = (pick_report or {}).get("picks", [])
        got = [p.get("nodeName") if isinstance(p, dict) else None for p in picks]
        check(same(pick_png, ext_png), "the picking run draws the same image")
        check(len(got) == 3 and got[0] == "Backdrop", f"dithered-away pixel {away} picks the backdrop ({got[:1]})")
        check(len(got) == 3 and got[1] == "A050", f"kept pixel {kept} picks the quad ({got[1:2]})")
        check(len(got) == 3 and got[2] == "Backdrop", f"alpha 0 quad centre {zero} picks the backdrop ({got[2:]})")

    print("[decal prepass]")
    report = OUT / "prepass.json"
    report.unlink(missing_ok=True)
    run([EXE, write("dither_decal", decal=True), *COMMON, *VIEW, *NO_AA, "--debug-view", "geometrynormal",
         "--decal-prepass-check", report])
    try:
        data = json.loads(report.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        data = None
    check(data is not None and data.get("decalBuffer", {}).get("active") is True, "decal buffer active for the check")
    if data is not None:
        depth = data.get("depth", {})
        check(depth.get("compared") is True and depth.get("overOneUlp", 1) == 0,
              f"prepass depth equals forward depth on {depth.get('pixels')} pixels "
              f"(max |diff| {depth.get('maxAbsDiff')}, {depth.get('overOneUlp')} beyond one ulp)")
        check(data.get("glErrors", 1) == 0, "no GL errors in the prepass check")
    decal_png, _ = render(EXE, write("dither_decal", decal=True), "decal_fixture", NO_AA)
    if decal_png is not None:
        counts, fraction = window_counts(Image.open(decal_png).convert("RGB"), rects["A050"])
        check(counts == {8}, f"with the decal buffer active the alpha 0.5 quad still keeps 8/16 ({sorted(counts)})")

    print("[TAA]")
    taa_a, _ = render(EXE, ext_model, "taa_ext_f160", TAA, frame=160)
    taa_b, _ = render(EXE, ext_model, "taa_ext_f200", TAA, frame=200)
    check(taa_a is not None and taa_b is not None, "TAA captures exist")
    if taa_a is not None and taa_b is not None:
        ia = Image.open(taa_a).convert("RGB")
        check(same(taa_a, taa_b), "stable once settled: frames 160 and 200 are byte-identical")

        def lin(v):
            v /= 255.0
            return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4

        def luma(r, g):
            return 0.2126 * r + 0.7152 * g

        def weighted_red(x, y):
            r, g, _ = (lin(v) for v in ia.getpixel((x, y)))
            return r / (1.0 + luma(r, g))

        def mean_weighted_red(name):
            x0, y0, x1, y1 = rects[name]
            values = [weighted_red(x, y) for y in range(y0, y1) for x in range(x0, x1)]
            return sum(values) / len(values)

        # The two end points as captured (8-bit sRGB): the alpha 0 and alpha 1 quads.
        empty, full = mean_weighted_red("A000"), mean_weighted_red("A100")
        red_weighted = LEVEL / (1.0 + luma(LEVEL, 0.0))
        green_weighted = LEVEL / (1.0 + luma(0.0, LEVEL))
        print(f"  INFO weighted red end points {empty:.4f} / {full:.4f} (exact 0 / {red_weighted:.4f})")
        for name, alpha, source in QUADS:
            x0, y0, x1, y1 = rects[name]
            coverage = [(weighted_red(x, y) - empty) / (full - empty) for y in range(y0, y1) for x in range(x0, x1)]
            mean = sum(coverage) / len(coverage)
            worst = max(abs(c - alpha) for c in coverage)
            # The colour a linear blend would give, against the Karis-resolved mean colour.
            cr, cg = mean * red_weighted, (1.0 - mean) * green_weighted
            unweight = 1.0 - luma(cr, cg)
            check(abs(mean - alpha) <= TAA_MEAN_TOLERANCE and worst <= TAA_PIXEL_TOLERANCE,
                  f"{name}: TAA coverage mean {mean:.4f} vs alpha {alpha:.4f} (<= {TAA_MEAN_TOLERANCE}), "
                  f"worst pixel {worst:.3f} (<= {TAA_PIXEL_TOLERANCE}); resolved linear colour "
                  f"({cr / unweight:.3f}, {cg / unweight:.3f}) vs linear blend ({alpha * LEVEL:.3f}, {(1 - alpha) * LEVEL:.3f})")

    print("[validation]")
    report = OUT / "core_validation.json"
    report.unlink(missing_ok=True)
    run([EXE, "--isolated", "--validate-file", OUT / "dither_core.gltf", "--validation-report", report])
    try:
        issues = json.loads(report.read_text(encoding="utf-8")).get("issues", [])
    except (OSError, ValueError):
        issues = None
    infos = [i for i in issues or [] if i.get("code") == "MATERIAL_ALPHAMODE_DITHER"]
    errors = [i for i in issues or [] if i.get("severity") == "Error"]
    check(issues is not None and len(infos) == len(QUADS) and all(i.get("severity") == "Info" for i in infos),
          f"core \"DITHER\": one info per material ({len(infos)}), pointer {infos[0].get('pointer') if infos else None}")
    check(issues is not None and not errors, f"core \"DITHER\": no validation errors ({[e.get('code') for e in errors]})")

    print("[GL errors]")
    for label, flags in (("no_aa", NO_AA), ("taa", TAA)):
        result = run([EXE, ext_model, *COMMON, *VIEW, *flags, "--profile-report", OUT / f"gl_{label}.json",
                      "--exit-after-profile"])
        errors = [line for line in result.stdout.splitlines() if line.startswith("GL automation errors")]
        check(errors == ["GL automation errors: 0"] and "GL error 0x" not in result.stderr, f"{label}: zero GL errors {errors}")

    print("[files without the extension]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
    else:
        print(f"  INFO baseline {baseline.name}")
        for name, model, flags in (("dither_none", plain_model, NO_AA), ("dither_none_taa", plain_model, TAA)):
            now, _ = render(EXE, model, "now_" + name, flags)
            before, _ = render(baseline, model, "base_" + name, flags)
            check(same(now, before), f"{name}: byte-identical to the baseline {baseline.name}")
        now = common_render(EXE, PLAIN_FIXTURE, "now_hierarchy", [], folder=OUT)
        before = common_render(baseline, PLAIN_FIXTURE, "base_hierarchy", [], folder=OUT)
        check(same(now, before), f"hierarchy fixture: byte-identical to the baseline {baseline.name}")
    return finish()


if __name__ == "__main__":
    sys.exit(main())
