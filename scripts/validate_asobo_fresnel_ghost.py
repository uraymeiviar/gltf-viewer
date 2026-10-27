# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A4.4: ASOBO_material_fresnel_fade and ASOBO_material_ghost_effect (view-angle opacity).

Formulas (docs/ASOBO_EXTENSIONS_SEMANTICS.md section 13; the SDK describes the parameters in words only,
so the combination is inferred), applied to BLEND materials: alpha = baseAlpha x factor, with
    fresnel fade  factor = clamp((N.V)^fresnelFactor - fresnelOpacityOffset, 0, 1)
    ghost effect  factor = clamp(bias + scale * (1 - N.V)^power, 0, 1)

Fixture: a unit UV sphere, KHR_materials_unlit white, BLEND, seen by the perspective camera at distance 3
on a black clear colour with bloom and auto exposure off. The pixel is then the neutral-tone-mapped sRGB
of the blended alpha, so the script inverts that mapping (relative to the opaque render's intensity) and
compares the recovered alpha with the formula at sampled pixels along the middle row: face-on, middle
and grazing. N.V comes from the pinhole geometry: the silhouette radius gives the focal length in pixels
(tan(asin(1/D)) = R / F), a pixel at offset x sees the sphere along the angle atan(x / F), and the ray's
closest approach h = D sin(angle) gives N.V = sqrt(1 - h^2). Checks:
  * the sample parameters (0.5 / 0.789; bias 0.2, power 0.5) and further settings match the formula;
  * edge cases: factor 0.001 and 100, offset +1 (invisible) and -1 (opaque), ghost scale 0 (constant
    bias, bias 1 = opaque), power 64, and the base-colour alpha multiplying the factor;
  * OPAQUE and MASK materials keep their alpha ignored/untouched;
  * --asobo-disable <extension> and --asobo off render byte-identical to the file without the extension;
  * files without the extensions are byte-identical to the baseline binary (--baseline-exe).

Run: python scripts/validate_asobo_fresnel_ghost.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import harness_process as subprocess
import json
import math
import struct
import sys
from pathlib import Path

from validate_asobo_common import EXE, PLAIN_FIXTURE, REPO, check, finish, render, same

OUT = REPO / "build" / "asobo_fresnel_ghost"
FRESNEL = "ASOBO_material_fresnel_fade"
GHOST = "ASOBO_material_ghost_effect"
VIEW = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off",
        "--exposure", "0", "--tone-mapping", "neutral", "--camera-target", "0,0,0",
        "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "3"]
DISTANCE = 3.0
RADII = (0.0, 0.5, 0.8, 0.92)  # sampled pixel offsets as fractions of the silhouette radius
ALPHA_TOLERANCE = 0.02


def sphere_document(extensions, alpha_mode="BLEND", base_alpha=1.0, rings=48, sectors=96):
    positions, indices = [], []
    for i in range(rings + 1):
        theta = math.pi * i / rings
        for j in range(sectors + 1):
            phi = 2.0 * math.pi * j / sectors
            positions.append((math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi)))
    for i in range(rings):
        for j in range(sectors):
            a, b = i * (sectors + 1) + j, (i + 1) * (sectors + 1) + j
            for tri in ((a, b, a + 1), (a + 1, b, b + 1)):
                p0, p1, p2 = (positions[k] for k in tri)
                e1 = [p1[n] - p0[n] for n in range(3)]
                e2 = [p2[n] - p0[n] for n in range(3)]
                cross = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
                outward = sum(cross[n] * (p0[n] + p1[n] + p2[n]) for n in range(3))
                if abs(outward) < 1e-12:
                    continue  # degenerate pole triangle
                indices.extend(tri if outward > 0 else (tri[0], tri[2], tri[1]))
    blob = b"".join(struct.pack("<3f", *p) for p in positions) * 2  # position = normal on a unit sphere
    position_bytes = len(positions) * 12
    blob += struct.pack(f"<{len(indices)}I", *indices)
    lows = [min(p[n] for p in positions) for n in range(3)]
    highs = [max(p[n] for p in positions) for n in range(3)]
    material = {"name": "Sphere", "alphaMode": alpha_mode,
                "pbrMetallicRoughness": {"baseColorFactor": [1.0, 1.0, 1.0, base_alpha], "metallicFactor": 0.0},
                "extensions": {"KHR_materials_unlit": {}, **extensions}}
    if alpha_mode == "MASK":
        material["alphaCutoff"] = 0.5
    return {
        "asset": {"version": "2.0"},
        "extensionsUsed": sorted({"KHR_materials_unlit", *extensions}),
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": "Sphere", "mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 0}]}],
        "materials": [material],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": lows, "max": highs},
            {"bufferView": 1, "componentType": 5126, "count": len(positions), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5125, "count": len(indices), "type": "SCALAR"}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": position_bytes, "target": 34962},
            {"buffer": 0, "byteOffset": position_bytes, "byteLength": position_bytes, "target": 34962},
            {"buffer": 0, "byteOffset": 2 * position_bytes, "byteLength": len(indices) * 4, "target": 34963}],
        "buffers": [{"byteLength": len(blob),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(blob).decode("ascii")}],
    }


def write_sphere(name, extensions=None, **kwargs):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(sphere_document(extensions or {}, **kwargs)), encoding="utf-8")
    return path


# ---- neutral tone mapping (resources/shaders/tonemapping.glsl) for grey input, and its inverse ----
def neutral_grey(x):
    offset = x - 6.25 * x * x if x < 0.08 else 0.04
    x -= offset
    start = 0.8 - 0.04
    if x < start:
        return x
    d = 1.0 - start
    new_peak = 1.0 - d * d / (x + d - start)
    return new_peak  # a grey colour keeps its peak; desaturation mixes toward the same grey


def srgb_to_linear(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def intensity_from_pixel(value):
    """Linear intensity before tone mapping of a grey sRGB byte (inverse of the neutral operator)."""
    target = srgb_to_linear(value)
    low, high = 0.0, 4.0
    for _ in range(50):
        mid = 0.5 * (low + high)
        if neutral_grey(mid) < target:
            low = mid
        else:
            high = mid
    return 0.5 * (low + high)


OPAQUE_INTENSITY = [1.0]  # intensity of the opaque white sphere, set from the plain capture


def silhouette(png):
    """Centre and radius in pixels of the lit disc in a capture."""
    from PIL import Image
    image = Image.open(png).convert("L")
    box = image.point(lambda v: 255 if v > 8 else 0).getbbox()
    if box is None:
        return None
    return 0.5 * (box[0] + box[2]), 0.5 * (box[1] + box[3]), 0.25 * (box[2] - box[0] + box[3] - box[1])


def sample_alphas(png, disc):
    from PIL import Image
    image = Image.open(png).convert("RGB")
    cx, cy, radius = disc
    result = []
    for fraction in RADII:
        x = int(round(cx + fraction * radius - 0.5))
        y = int(round(cy - 0.5))
        r, g, b = image.getpixel((x, y))
        result.append(intensity_from_pixel(g) / OPAQUE_INTENSITY[0])
    return result


def fresnel_alpha(n_dot_v, factor, offset, base=1.0):
    return base * min(1.0, max(0.0, n_dot_v ** factor - offset))


def ghost_alpha(n_dot_v, bias, scale, power, base=1.0):
    return base * min(1.0, max(0.0, bias + scale * (1.0 - n_dot_v) ** power))


def view_cosine(fraction):
    """N.V of the unit sphere at a pixel `fraction` of the silhouette radius from the centre (pinhole camera)."""
    focal_over_radius = 1.0 / math.tan(math.asin(1.0 / DISTANCE))  # F / R in pixels
    angle = math.atan(fraction / focal_over_radius)
    closest = DISTANCE * math.sin(angle)
    return math.sqrt(max(0.0, 1.0 - closest * closest))


def expected(model):
    return [model(view_cosine(fraction)) for fraction in RADII]


def compare(label, png, disc, wanted):
    got = sample_alphas(png, disc)
    worst = max(abs(a - b) for a, b in zip(got, wanted))
    text = " ".join(f"{g:.3f}/{w:.3f}" for g, w in zip(got, wanted))
    check(worst <= ALPHA_TOLERANCE, f"{label}: alpha at r={RADII} measured/formula {text} (max error {worst:.3f})")
    return got


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=None)
    args = parser.parse_args()
    baseline = Path(args.baseline_exe) if args.baseline_exe else None
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None

    plain_png = render(EXE, write_sphere("plain"), "plain", VIEW, folder=OUT)
    check(plain_png is not None, "plain sphere captured")
    disc = silhouette(plain_png) if plain_png else None
    check(disc is not None and disc[2] > 100, f"sphere silhouette found (centre/radius {disc})")
    if disc is None:
        return finish()
    from PIL import Image
    centre = Image.open(plain_png).convert("RGB").getpixel((int(disc[0]), int(disc[1])))[1]
    OPAQUE_INTENSITY[0] = intensity_from_pixel(centre)
    opaque_alpha = sample_alphas(plain_png, disc)
    check(min(opaque_alpha) > 0.99 and max(opaque_alpha) < 1.01,
          f"opaque reference is uniform ({[round(a, 3) for a in opaque_alpha]}, white intensity {OPAQUE_INTENSITY[0]:.3f})")

    print("[fresnel fade]")
    cases = {
        "sample_0_5_0_789": (0.5, 0.789),
        "factor_1_offset_0": (1.0, 0.0),
        "factor_2_offset_0_2": (2.0, 0.2),
        "factor_0_001": (0.001, 0.0),
        "factor_100": (100.0, 0.0),
        "offset_plus_1": (1.0, 1.0),
        "offset_minus_1": (1.0, -1.0),
    }
    pngs = {}
    for name, (factor, offset) in cases.items():
        ext = {FRESNEL: {"fresnelFactor": factor, "fresnelOpacityOffset": offset}}
        pngs[name] = render(EXE, write_sphere("fresnel_" + name, ext), "fresnel_" + name, VIEW, folder=OUT)
        if pngs[name] is not None:
            compare(f"fresnel {factor}/{offset}", pngs[name], disc, expected(lambda d: fresnel_alpha(d, factor, offset)))
    check(all(p is not None for p in pngs.values()), "every fresnel capture exists")
    # The PROPELERSIDE set must be visible face-on and gone at grazing incidence.
    sample = sample_alphas(pngs["sample_0_5_0_789"], disc)
    check(sample[0] > 0.15 and sample[-1] < 0.01, f"sample set: face-on {sample[0]:.3f} > 0.15, grazing {sample[-1]:.3f} < 0.01")
    check(sample_alphas(pngs["offset_plus_1"], disc)[0] < 0.01, "offset +1: the surface is fully transparent")
    check(same(pngs["offset_minus_1"], plain_png), "offset -1: alpha is 1 everywhere, identical to the plain opaque-alpha sphere")
    sphere_default_factor = write_sphere("fresnel_offset_only", {FRESNEL: {"fresnelOpacityOffset": 0.0}})
    default_factor = render(EXE, sphere_default_factor, "fresnel_offset_only", VIEW, folder=OUT)
    check(same(default_factor, pngs["factor_1_offset_0"]), "absent fresnelFactor behaves as the default 1.0")
    half = render(EXE, write_sphere("fresnel_base_alpha", {FRESNEL: {"fresnelFactor": 1.0, "fresnelOpacityOffset": 0.0}},
                                    base_alpha=0.5), "fresnel_base_alpha", VIEW, folder=OUT)
    if half is not None:
        compare("fresnel with base alpha 0.5", half, disc, expected(lambda d: fresnel_alpha(d, 1.0, 0.0, 0.5)))

    print("[ghost effect]")
    ghost_cases = {
        "sample_bias_0_2_power_0_5": {"bias": 0.2, "power": 0.5},
        "button_bias_1_scale_0": {"bias": 1.0, "scale": 0.0},
        "constant_bias_0_3": {"bias": 0.3, "scale": 0.0, "power": 4.0},
        "rim_1": {"bias": 0.0, "scale": 1.0, "power": 1.0},
        "rim_power_64": {"bias": 0.0, "scale": 1.0, "power": 64.0},
        "scale_0_5_power_2": {"bias": 0.1, "scale": 0.5, "power": 2.0},
    }
    ghost_pngs = {}
    for name, values in ghost_cases.items():
        bias, scale, power = values.get("bias", 0.0), values.get("scale", 1.0), values.get("power", 1.0)
        ghost_pngs[name] = render(EXE, write_sphere("ghost_" + name, {GHOST: values}), "ghost_" + name, VIEW, folder=OUT)
        if ghost_pngs[name] is not None:
            compare(f"ghost bias {bias} scale {scale} power {power}", ghost_pngs[name], disc,
                    expected(lambda d: ghost_alpha(d, bias, scale, power)))
    check(all(p is not None for p in ghost_pngs.values()), "every ghost capture exists")
    check(same(ghost_pngs["button_bias_1_scale_0"], plain_png), "bias 1 scale 0 (VR button): identical to the plain sphere")
    constant = sample_alphas(ghost_pngs["constant_bias_0_3"], disc)
    check(max(constant) - min(constant) < 0.01, f"scale 0: alpha is the constant bias ({[round(a, 3) for a in constant]})")
    rim = sample_alphas(ghost_pngs["sample_bias_0_2_power_0_5"], disc)
    check(rim[0] < rim[1] < rim[2] < rim[3], f"sample set: opacity rises toward grazing incidence ({[round(a, 3) for a in rim]})")

    print("[alpha modes]")
    fresnel_ext = {FRESNEL: {"fresnelFactor": 0.5, "fresnelOpacityOffset": 0.789}}
    ghost_ext = {GHOST: {"bias": 0.2, "power": 0.5}}
    for alpha_mode in ("OPAQUE", "MASK"):
        reference = render(EXE, write_sphere("plain_" + alpha_mode, alpha_mode=alpha_mode), "plain_" + alpha_mode, VIEW, folder=OUT)
        for label, ext in (("fresnel", fresnel_ext), ("ghost", ghost_ext)):
            image = render(EXE, write_sphere(f"{label}_{alpha_mode}", ext, alpha_mode=alpha_mode),
                           f"{label}_{alpha_mode}", VIEW, folder=OUT)
            check(same(image, reference), f"{label} on {alpha_mode}: alpha is ignored / untouched (identical to no extension)")

    print("[switches]")
    for label, ext, name, other in (("fresnel", fresnel_ext, FRESNEL, GHOST), ("ghost", ghost_ext, GHOST, FRESNEL)):
        model = write_sphere("switch_" + label, ext)
        applied = render(EXE, model, "switch_" + label, VIEW, folder=OUT)
        check(applied is not None and not same(applied, plain_png), f"{label}: the extension changes the render")
        disabled = render(EXE, model, f"switch_{label}_disabled", VIEW + ["--asobo-disable", name], folder=OUT)
        check(same(disabled, plain_png), f"--asobo-disable {name} equals no extension")
        other_disabled = render(EXE, model, f"switch_{label}_other_disabled", VIEW + ["--asobo-disable", other], folder=OUT)
        check(same(other_disabled, applied), f"--asobo-disable {other} leaves {name} applied")
        master = render(EXE, model, f"switch_{label}_master_off", VIEW + ["--asobo", "off"], folder=OUT)
        check(same(master, plain_png), f"--asobo off equals no extension ({label})")

    print("[GL errors]")
    for label, model in (("fresnel", OUT / "fresnel_sample_0_5_0_789.gltf"),
                         ("ghost", OUT / "ghost_sample_bias_0_2_power_0_5.gltf")):
        result = subprocess.run([str(EXE), str(model), "--isolated", "--window-size", "800x600", "--aa", "none",
                                 "--fixed-frame-time", "0.05", "--vsync", "off", "--profile-report",
                                 str(OUT / f"gl_{label}.json"), "--exit-after-profile"],
                                cwd=REPO, capture_output=True, text=True, timeout=300)
        errors = [line for line in result.stdout.splitlines() if line.startswith("GL automation errors")]
        check(errors == ["GL automation errors: 0"] and "GL error 0x" not in result.stderr, f"{label}: zero GL errors {errors}")

    print("[files without the extensions]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
    else:
        print(f"  INFO baseline {baseline.name}")
        for name, model, flags in (("plain_sphere", write_sphere("plain"), VIEW),
                                   ("hierarchy_fixture", PLAIN_FIXTURE, [])):
            now = render(EXE, model, "now_" + name, flags, folder=OUT)
            before = render(baseline, model, "base_" + name, flags, folder=OUT)
            check(same(now, before), f"{name}: byte-identical to the baseline {baseline.name}")
    return finish()


if __name__ == "__main__":
    sys.exit(main())
