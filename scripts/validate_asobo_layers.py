# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A7.1: ASOBO_material_clear_coat_v2, ASOBO_material_clear_coat, ASOBO_material_iridescent, ASOBO_material_pearlescent.

Rule: docs/ASOBO_EXTENSIONS_SEMANTICS.md section 20. Fixture: the anisotropy suite's UV sphere, lit by a directional
light from the camera. Each extension is compared with a material whose result is known, so these are exact checks:

  * clear coat v2: no texture and base normal off == KHR_materials_clearcoat (factor 1, same roughness); texture alpha is
    the coat roughness; texture RGB tints the base layer (black leaves only the coat); uniform base roughness == KHR
    coat with the COMP roughness over a base of clearcoatBaseRoughness; a normal texture at tiling 2 == the same pattern
    twice as dense at tiling 1; normal factor 0 == no normal texture; the base normal reaches the coat only with
    clearcoatBaseAffectCoat;
  * clear coat v1: dirtTexture == KHR with that image as clearcoatTexture and clearcoatRoughnessTexture; no texture = no coat;
  * iridescent: no texture, brightness 1 == KHR_materials_iridescence (IOR 1.3, same maximum thickness); texture R sets the
    thickness between min and max (G ignored); brightness 0 is the plain material, 2 is stronger; a windshield compiles
    and keeps working;
  * pearl: shift 0 makes the ramp 1, so the pearl colour is constant and a plain material of Asobo's formula's colour
    gives the same image; shift > 0 changes with the view angle; metallic 0 changes nothing; a decal ignores it and a
    Standard material with pearl still receives decals;
  * every switch off (--asobo-disable, --asobo off) == the file without the extension; --without-textures renders;
    zero GL errors; real files (DA62 fuselage, CJ4 holographic livery) load and react when present;
  * a file without these extensions is byte-identical to the baseline binary (--baseline-exe).

Run: python scripts/validate_asobo_layers.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import io
import json
import math
import sys
from pathlib import Path

from glider_harness import capture_run, gl_errors_zero
from validate_asobo_anisotropy import document, write_hdr
from validate_asobo_common import EXE, PLAIN_FIXTURE, REPO, check, finish, same

OUT = REPO / "build" / "asobo_layers"
COAT2 = "ASOBO_material_clear_coat_v2"
COAT1 = "ASOBO_material_clear_coat"
IRID = "ASOBO_material_iridescent"
PEARL = "ASOBO_material_pearlescent"
KHR_COAT = "KHR_materials_clearcoat"
KHR_IRID = "KHR_materials_iridescence"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
VIEW = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
        "--tone-mapping", "neutral", "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0",
        "--camera-distance", "4"]
TOLERANCE = 2  # 8-bit levels between two shader variants of the same value
DA62 = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
CJ4 = Path(r"E:\msfs-pkgs\Official\Steam\asobo-aircraft-cj4-livery-01\SimObjects\Airplanes\Asobo_CJ4_Livery_01\model.01\CJ4_LOD00.gltf")


def png(pixels, width=None, height=None):
    """pixels: one (r, g, b, a) or a list of rows of them."""
    from PIL import Image
    rows = pixels if isinstance(pixels, list) else [[pixels] * 4] * 4
    image = Image.new("RGBA", (len(rows[0]), len(rows)))
    image.putdata([p for row in rows for p in row])
    data = io.BytesIO()
    image.save(data, format="PNG")
    return "data:image/png;base64," + base64.b64encode(data.getvalue()).decode("ascii")


def checker(cells):
    """A normal map of alternating slopes, cells x cells texels."""
    a, b = (200, 128, 255, 255), (56, 128, 255, 255)
    return [[a if (x + y) % 2 == 0 else b for x in range(cells)] for y in range(cells)]


def fixture(name, extensions=None, pbr=None, normal=None, images=()):
    """The sphere with the given material extensions. images: data URIs, textures 0.. in order. normal: texture index."""
    doc = document(light=True)
    material = doc["materials"][0]
    material["pbrMetallicRoughness"].update(pbr or {})
    if extensions:
        material["extensions"] = extensions
    if normal is not None:
        material["normalTexture"] = {"index": normal}
    if images:
        doc["images"] = [{"uri": uri} for uri in images]
        doc["textures"] = [{"source": i, "sampler": 0} for i in range(len(images))]
        doc["samplers"] = [{"magFilter": 9728, "minFilter": 9728}]
    doc["extensionsUsed"] = sorted(set(doc.get("extensionsUsed", [])) | set(extensions or {}))
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def add_decal(doc, pearl=False):
    """A second, slightly larger copy of the sphere's front drawn as a red geometry decal over the receiver."""
    decal = {"name": "Decal", "alphaMode": "BLEND",
             "pbrMetallicRoughness": {"baseColorFactor": [1, 0, 0, 1], "metallicFactor": 1.0, "roughnessFactor": 0.4},
             "extensions": {"ASOBO_material_geometry_decal": {},
                            "ASOBO_material_draw_order": {"drawOrderOffset": 10}}}
    if pearl:
        decal["extensions"][PEARL] = {"pearlShift": 0.0, "pearlRange": 0.25, "pearlBrightness": 0.0}
    doc["materials"].append(decal)
    primitive = dict(doc["meshes"][0]["primitives"][0], material=len(doc["materials"]) - 1)
    doc["meshes"].append({"name": "Decal", "primitives": [primitive]})
    doc["nodes"].append({"name": "Decal", "mesh": len(doc["meshes"]) - 1, "scale": [1.002, 1.002, 1.002]})
    doc["scenes"][0]["nodes"].append(len(doc["nodes"]) - 1)
    doc["extensionsUsed"] = sorted(set(doc["extensionsUsed"]) | set(decal["extensions"]))


def render(model, name, flags=(), exe=EXE, frame=24):
    """Returns (png or None, gl errors zero)."""
    image, report = OUT / f"{name}.png", OUT / f"{name}.json"
    image.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    result = capture_run([exe, model, *COMMON, *VIEW, *flags, "--capture-viewport", image, "--capture-frame", str(frame),
                          "--profile-report", report, "--exit-after-profile"], session=False)
    log = result.stdout + result.stderr
    if not image.exists():
        print(log[-1500:])
    return (image if image.exists() else None), gl_errors_zero(log)


def level_diff(a, b):
    from PIL import Image, ImageChops
    if a is None or b is None:
        return 999
    ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    if ia.size != ib.size:
        return 999
    return max(hi for lo, hi in ImageChops.difference(ia, ib).getextrema())


def mean_rgb(path, box=None):
    from PIL import Image, ImageStat
    image = Image.open(path).convert("RGB")
    if box is not None:
        image = image.crop(box)
    return ImageStat.Stat(image).mean


def pearl_colour(rgb, shift_ramp, pearl_range, brightness):
    """Asobo's formula (MSFS2024Material_Standard.fx:94-105, utility.fx) for a constant ramp; rgb linear."""
    r, g, b = rgb
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    u = -0.09991 * r - 0.33609 * g + 0.436 * b
    v = 0.615 * r - 0.55861 * g - 0.05639 * b
    angle = -shift_ramp * pearl_range * 2.0 * math.pi
    c, s = math.cos(angle), math.sin(angle)
    u, v = (u * c + v * s) * math.pi, (-u * s + v * c) * math.pi
    y = min(max(y + min(max(shift_ramp, 0.0), 1.0) * brightness, 0.0), 1.0)
    out = (y + 1.28033 * v, y - 0.21482 * u - 0.38059 * v, y + 2.12798 * u)
    return [min(max(x, 0.0), 1.0) for x in out]


class Suite:
    def __init__(self):
        self.count = 0
        self.errors_clean = True

    def shot(self, model, flags=()):
        self.count += 1
        image, clean = render(model, f"r{self.count:03d}_{Path(model).stem}", flags)
        self.errors_clean = self.errors_clean and clean
        return image


def coat_checks(s):
    grey = {"baseColorFactor": [0.6, 0.6, 0.6, 1], "metallicFactor": 0.0, "roughnessFactor": 0.6}
    plain = s.shot(fixture("plain", pbr=grey))
    khr = s.shot(fixture("khr_coat", {KHR_COAT: {"clearcoatFactor": 1.0, "clearcoatRoughnessFactor": 0.1}}, pbr=grey))
    v2 = s.shot(fixture("v2", {COAT2: {"clearcoatRoughnessFactor": 0.1, "clearcoatBaseAffectCoat": False}}, pbr=grey))
    check(level_diff(v2, khr) <= TOLERANCE, f"v2 without a texture == KHR coat of the same roughness ({level_diff(v2, khr)})")
    check(level_diff(v2, plain) > 10, "v2 without a texture has a coat (the DA62 case; the schema would say none)")
    v2_default = s.shot(fixture("v2_affect", {COAT2: {"clearcoatRoughnessFactor": 0.1}}, pbr=grey))
    check(level_diff(v2_default, khr) <= TOLERANCE, "base normal on, no normal map anywhere: the same")

    half = {COAT2: {"clearcoatColorRoughnessTexture": {"index": 0}, "clearcoatBaseAffectCoat": False}}
    v2_alpha = s.shot(fixture("v2_alpha", half, pbr=grey, images=[png((255, 255, 255, 128))]))
    khr_half = s.shot(fixture("khr_half", {KHR_COAT: {"clearcoatFactor": 1.0, "clearcoatRoughnessFactor": 128 / 255}}, pbr=grey))
    check(level_diff(v2_alpha, khr_half) <= TOLERANCE, f"texture alpha is the coat roughness ({level_diff(v2_alpha, khr_half)})")
    factored = {COAT2: {"clearcoatColorRoughnessTexture": {"index": 0}, "clearcoatRoughnessFactor": 0.5, "clearcoatBaseAffectCoat": False}}
    v2_fac = s.shot(fixture("v2_fac", factored, pbr=grey, images=[png((255, 255, 255, 128))]))
    khr_fac = s.shot(fixture("khr_fac", {KHR_COAT: {"clearcoatFactor": 1.0, "clearcoatRoughnessFactor": 64 / 255}}, pbr=grey))
    check(level_diff(v2_fac, khr_fac) <= TOLERANCE, "roughness factor multiplies the texture")

    red = s.shot(fixture("v2_red", half, pbr=grey, images=[png((255, 0, 0, 128))]))
    red_mean = mean_rgb(red)
    check(red_mean[0] > 3 * max(red_mean[1], 0.5) or red_mean[0] > red_mean[1] + 5, f"red varnish tints the base layer {red_mean}")
    black = s.shot(fixture("v2_black", half, pbr=grey, images=[png((0, 0, 0, 128))]))
    no_base = s.shot(fixture("khr_black_base", {KHR_COAT: {"clearcoatFactor": 1.0, "clearcoatRoughnessFactor": 128 / 255}},
                             pbr={"baseColorFactor": [0, 0, 0, 1], "metallicFactor": 0.0, "roughnessFactor": 0.6}))
    # The dielectric specular of the base stays in the KHR twin, so black varnish must be darker than or equal to it.
    check(sum(mean_rgb(black)) <= sum(mean_rgb(no_base)) + 0.5, "black varnish absorbs the base layer, the coat stays")
    check(sum(mean_rgb(black)) > 0.05, "... and the coat itself is still visible")

    uniform = {COAT2: {"clearcoatInverseRoughness": True, "clearcoatBaseRoughness": 0.05, "clearcoatBaseAffectCoat": False,
                       "clearcoatColorRoughnessTexture": {"index": 0}}}
    pbr_u = dict(grey, roughnessFactor=0.7)
    v2_uni = s.shot(fixture("v2_uniform", uniform, pbr=pbr_u, images=[png((255, 0, 0, 10))]))
    khr_uni = s.shot(fixture("khr_uniform", {KHR_COAT: {"clearcoatFactor": 1.0, "clearcoatRoughnessFactor": 0.7}},
                             pbr=dict(grey, roughnessFactor=0.05)))
    check(level_diff(v2_uni, khr_uni) <= TOLERANCE,
          f"uniform base roughness: coat = COMP roughness, base = the value, colour texture ignored ({level_diff(v2_uni, khr_uni)})")

    tiled = {COAT2: {"clearcoatNormalTexture": {"index": 0}, "clearcoatNormalTiling": 2.0, "clearcoatBaseAffectCoat": False}}
    dense = {COAT2: {"clearcoatNormalTexture": {"index": 0}, "clearcoatBaseAffectCoat": False}}
    t2 = s.shot(fixture("v2_tiled", tiled, pbr=grey, images=[png(checker(8))]))
    t1 = s.shot(fixture("v2_dense", dense, pbr=grey, images=[png(checker(16))]))
    check(level_diff(t2, t1) <= TOLERANCE, f"normal tiling 2 == the pattern twice as dense ({level_diff(t2, t1)})")
    flat = s.shot(fixture("v2_flat", {COAT2: {"clearcoatBaseAffectCoat": False}}, pbr=grey))
    check(level_diff(t2, flat) > 10, "the coat normal map changes the highlight")
    zero = {COAT2: {"clearcoatNormalTexture": {"index": 0}, "clearcoatNormalFactor": 0.0, "clearcoatBaseAffectCoat": False}}
    z = s.shot(fixture("v2_zero", zero, pbr=grey, images=[png(checker(8))]))
    check(level_diff(z, flat) <= TOLERANCE, "normal factor 0 == no normal map")

    base_map = [png(checker(8))]
    on = s.shot(fixture("v2_base_on", {COAT2: {}}, pbr=grey, normal=0, images=base_map))
    off = s.shot(fixture("v2_base_off", {COAT2: {"clearcoatBaseAffectCoat": False}}, pbr=grey, normal=0, images=base_map))
    check(level_diff(on, off) > 5, "clearcoatBaseAffectCoat carries the base normal into the coat")

    # MSFS 2020 v1.
    image = [png((255, 128, 0, 255))]
    v1 = s.shot(fixture("v1", {COAT1: {"dirtTexture": {"index": 0}}}, pbr=grey, images=image))
    khr_v1 = s.shot(fixture("khr_v1", {KHR_COAT: {"clearcoatFactor": 1.0, "clearcoatRoughnessFactor": 1.0,
                                                  "clearcoatTexture": {"index": 0}, "clearcoatRoughnessTexture": {"index": 0}}},
                            pbr=grey, images=image))
    check(level_diff(v1, khr_v1) <= TOLERANCE, f"v1 dirtTexture R/G == KHR coat with that image ({level_diff(v1, khr_v1)})")
    v1_none = s.shot(fixture("v1_none", {COAT1: {}}, pbr=grey))
    check(level_diff(v1_none, plain) <= TOLERANCE, "v1 without a texture has no coat")

    # Switches: off == the file without the extension.
    for flags, label in ((["--asobo-disable", COAT2], "--asobo-disable"), (["--asobo", "off"], "--asobo off")):
        image = s.shot(fixture("v2_switch", {COAT2: {"clearcoatRoughnessFactor": 0.1, "clearcoatNormalTexture": {"index": 0}}},
                               pbr=grey, images=[png(checker(8))]), flags)
        check(same(image, plain), f"v2 {label} is byte-identical to no extension")
    image = s.shot(fixture("v1_switch", {COAT1: {"dirtTexture": {"index": 0}}}, pbr=grey, images=[png((255, 128, 0, 255))]),
                   ["--asobo-disable", COAT1])
    check(same(image, plain), "v1 --asobo-disable is byte-identical to no extension")
    image = s.shot(fixture("v2_notex", {COAT2: {"clearcoatRoughnessFactor": 0.1, "clearcoatColorRoughnessTexture": {"index": 0},
                                                "clearcoatBaseAffectCoat": False}},
                           pbr=grey, images=[png((255, 0, 0, 10))]), ["--without-textures"])
    khr_notex = s.shot(fixture("khr_notex", {KHR_COAT: {"clearcoatFactor": 1.0, "clearcoatRoughnessFactor": 0.1}}, pbr=grey),
                       ["--without-textures"])
    check(level_diff(image, khr_notex) <= TOLERANCE, "--without-textures: the coat stays, as if no texture were set")

    missing = fixture("v2_missing", {COAT2: {"clearcoatRoughnessFactor": 0.1, "clearcoatColorRoughnessTexture": {"index": 0},
                                             "clearcoatBaseAffectCoat": False}}, pbr=grey,
                      images=[png((1, 1, 1, 1))])
    doc = json.loads(missing.read_text(encoding="utf-8"))
    doc["images"][0]["uri"] = "missing_layer_texture.png"
    missing.write_text(json.dumps(doc), encoding="utf-8")
    image = s.shot(missing)
    check(level_diff(image, khr) <= TOLERANCE, "a failed image is not bound (no black varnish)")


def iridescence_checks(s):
    metal = {"baseColorFactor": [0.8, 0.8, 0.8, 1], "metallicFactor": 1.0, "roughnessFactor": 0.3}
    plain = s.shot(fixture("irid_plain", pbr=metal))
    khr = s.shot(fixture("khr_irid", {KHR_IRID: {"iridescenceFactor": 1.0, "iridescenceIor": 1.3, "iridescenceThicknessMaximum": 300.0}}, pbr=metal))
    asobo = s.shot(fixture("irid", {IRID: {"iridescentMaxThickness": 300.0}}, pbr=metal))
    check(level_diff(asobo, khr) <= TOLERANCE, f"no texture, brightness 1 == KHR film at the maximum thickness ({level_diff(asobo, khr)})")
    check(level_diff(asobo, plain) > 5, "the film changes the colour")

    tex = {IRID: {"iridescentMinThickness": 100.0, "iridescentMaxThickness": 500.0, "iridescentThicknessTexture": {"index": 0}}}
    thick = s.shot(fixture("irid_tex", tex, pbr=metal, images=[png((128, 0, 0, 255))]))
    khr_mid = s.shot(fixture("khr_irid_mid", {KHR_IRID: {"iridescenceFactor": 1.0, "iridescenceIor": 1.3,
                                                         "iridescenceThicknessMaximum": 100.0 + 400.0 * 128 / 255}}, pbr=metal))
    check(level_diff(thick, khr_mid) <= TOLERANCE + 1, f"texture R sets the thickness between min and max, G ignored ({level_diff(thick, khr_mid)})")

    zero = s.shot(fixture("irid_zero", {IRID: {"iridescentMaxThickness": 300.0, "iridescentBrightness": 0.0}}, pbr=metal))
    check(level_diff(zero, plain) <= TOLERANCE + 2, f"brightness 0 is the plain material ({level_diff(zero, plain)})")
    strong = s.shot(fixture("irid_two", {IRID: {"iridescentMaxThickness": 300.0, "iridescentBrightness": 2.0}}, pbr=metal))
    check(level_diff(strong, plain) > level_diff(asobo, plain), "brightness 2 departs further than 1")

    image = s.shot(fixture("irid_off", {IRID: {"iridescentMaxThickness": 300.0}}, pbr=metal), ["--asobo-disable", IRID])
    check(same(image, plain), "--asobo-disable is byte-identical to no extension")

    glass = {"baseColorFactor": [0.8, 0.9, 1.0, 0.3], "metallicFactor": 0.0, "roughnessFactor": 0.05}
    def windshield(name, extra):
        path = fixture(name, {"ASOBO_material_windshield_v3": {}, **extra}, pbr=glass)
        doc = json.loads(path.read_text(encoding="utf-8"))
        doc["materials"][0]["alphaMode"] = "BLEND"
        path.write_text(json.dumps(doc), encoding="utf-8")
        return path
    plain_shield = s.shot(windshield("shield", {}))
    film_shield = s.shot(windshield("shield_film", {IRID: {"iridescentMaxThickness": 300.0}}))
    check(plain_shield is not None and level_diff(film_shield, plain_shield) > 2, "an iridescent windshield renders with the film")
    off_shield = s.shot(windshield("shield_off", {IRID: {"iridescentMaxThickness": 300.0}}), ["--asobo-disable", IRID])
    check(same(off_shield, plain_shield), "windshield with the switch off == the windshield without it")


def pearl_checks(s):
    base = (0.7, 0.25, 0.1)
    pbr = {"baseColorFactor": [*base, 1], "metallicFactor": 1.0, "roughnessFactor": 0.35}
    plain = s.shot(fixture("pearl_plain", pbr=pbr))
    for shift_range, brightness in ((0.25, 0.0), (0.5, 0.2), (0.1, -0.1)):
        tag = f"{shift_range}_{brightness}".replace(".", "p").replace("-", "m")
        pearl = s.shot(fixture(f"pearl_{tag}", {PEARL: {"pearlShift": 0.0, "pearlRange": shift_range, "pearlBrightness": brightness}}, pbr=pbr))
        twin = pearl_colour(base, 1.0, shift_range, brightness)
        expected = s.shot(fixture(f"pearl_twin_{tag}", pbr=dict(pbr, baseColorFactor=[*twin, 1])))
        check(level_diff(pearl, expected) <= TOLERANCE,
              f"shift 0, range {shift_range}, brightness {brightness}: Asobo's formula gives {[round(c, 3) for c in twin]} ({level_diff(pearl, expected)})")
    view = s.shot(fixture("pearl_view", {PEARL: {"pearlShift": 4.0, "pearlRange": 0.4, "pearlBrightness": 0.0}}, pbr=pbr))
    # The centre of the sphere has a ramp near 0, its rim a ramp near 1. The box avoids the saturated highlight.
    from PIL import Image
    width, height = Image.open(view).size
    near_centre = (width // 2 + 30, height // 2 - 10, width // 2 + 50, height // 2 + 10)
    face_on = pearl_colour(base, 0.0, 0.4, 0.0)
    head_on = s.shot(fixture("pearl_view_twin", pbr=dict(pbr, baseColorFactor=[*face_on, 1])))
    centre, twin_centre = mean_rgb(view, near_centre), mean_rgb(head_on, near_centre)
    check(sum(twin_centre) > 10 and max(abs(a - b) for a, b in zip(centre, twin_centre)) <= 6,
          f"shift 4: face-on keeps the no-rotation colour {centre} vs {twin_centre}")
    check(level_diff(view, head_on) > 10, "shift 4: the colour changes towards the rim")

    dielectric = dict(pbr, metallicFactor=0.0)
    d_plain = s.shot(fixture("pearl_d_plain", pbr=dielectric))
    d_pearl = s.shot(fixture("pearl_d", {PEARL: {"pearlShift": 0.0, "pearlRange": 0.4, "pearlBrightness": 0.5}}, pbr=dielectric))
    check(level_diff(d_pearl, d_plain) <= TOLERANCE, "metallic 0: no change (the formula lerps by metalness)")
    grey = dict(pbr, baseColorFactor=[0.5, 0.5, 0.5, 1])
    g_plain = s.shot(fixture("pearl_g_plain", pbr=grey))
    g_pearl = s.shot(fixture("pearl_g", {PEARL: {"pearlShift": 0.0, "pearlRange": 0.4, "pearlBrightness": 0.0}}, pbr=grey))
    check(level_diff(g_pearl, g_plain) <= TOLERANCE, "a neutral colour has no hue to turn")
    edge = s.shot(fixture("pearl_edge", {PEARL: {"pearlShift": -999.0, "pearlRange": 999.0, "pearlBrightness": 1.0}}, pbr=pbr))
    edge_mean = mean_rgb(edge)
    check(all(math.isfinite(c) for c in edge_mean), "the exporter's extreme values stay finite")

    image = s.shot(fixture("pearl_off", {PEARL: {"pearlShift": 0.0, "pearlRange": 0.25, "pearlBrightness": 0.1}}, pbr=pbr),
                   ["--asobo-disable", PEARL])
    check(same(image, plain), "--asobo-disable is byte-identical to no extension")

    # Decals: Max writes pearl for Standard only; MSFS showed no difference on the add-on's decal livery.
    def decal_file(name, decal_pearl, receiver_ext=None, receiver_pbr=None):
        path = fixture(name, receiver_ext, pbr=receiver_pbr or dict(pbr, metallicFactor=0.0, baseColorFactor=[0.2, 0.2, 0.2, 1]))
        doc = json.loads(path.read_text(encoding="utf-8"))
        add_decal(doc, pearl=decal_pearl)
        path.write_text(json.dumps(doc), encoding="utf-8")
        return path
    decal_plain = s.shot(decal_file("decal_plain", False))
    decal_pearl = s.shot(decal_file("decal_pearl", True))
    check(same(decal_pearl, decal_plain), "a decal's pearl is not drawn")
    received = s.shot(decal_file("decal_receiver", False, {PEARL: {"pearlShift": 0.0, "pearlRange": 0.25, "pearlBrightness": 0.0}}))
    check(level_diff(received, decal_plain) <= TOLERANCE, "a Standard material with pearl still receives decals")
    no_decal = s.shot(fixture("no_decal", pbr=dict(pbr, metallicFactor=0.0, baseColorFactor=[0.2, 0.2, 0.2, 1])))
    check(level_diff(decal_plain, no_decal) > 10, "(the decal is visible in this fixture)")


def real_file_checks(s):
    candidates = sorted(DA62.rglob("Airframe_LOD00.gltf"))
    if candidates:
        airframe = candidates[0]
        flags = ["--camera-yaw", "35", "--camera-pitch", "10", "--camera-distance", "14", "--without-textures"]
        coat = s.shot(airframe, flags)
        off = s.shot(airframe, [*flags, "--asobo-disable", COAT2])
        check(coat is not None and level_diff(coat, off) > 3, "DA62 airframe: the clear coat changes its fuselage")
    else:
        print("  SKIP DA62 airframe not found")
    if CJ4.exists():
        flags = ["--camera-yaw", "35", "--camera-pitch", "10", "--camera-distance", "16", "--without-textures"]
        pearl = s.shot(CJ4, flags)
        off = s.shot(CJ4, [*flags, "--asobo-disable", PEARL])
        check(pearl is not None and off is not None, "CJ4 holographic livery loads with and without pearl")
    else:
        print("  SKIP CJ4 livery not found")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe")
    parser.add_argument("--only", choices=("coat", "iridescence", "pearl", "real"))
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    s = Suite()
    sections = {"coat": coat_checks, "iridescence": iridescence_checks, "pearl": pearl_checks, "real": real_file_checks}
    for name, section in sections.items():
        if args.only in (None, name):
            print(f"--- {name}")
            section(s)
    check(s.errors_clean, f"zero GL errors in all {s.count} renders")
    if args.baseline_exe and Path(args.baseline_exe).exists():
        current, _ = render(PLAIN_FIXTURE, "plain_current")
        old, _ = render(PLAIN_FIXTURE, "plain_baseline", exe=Path(args.baseline_exe))
        check(same(current, old), "a file without these extensions is byte-identical to the baseline")
    finish()


if __name__ == "__main__":
    main()
