# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors.
"""A2.2 decal pass and receiver application validation.

A synthetic, fully self-contained fixture: an opaque lit receiver quad with one (or two)
decal quads slightly above it. Per-property debug views compare against the same file with
the decal nodes removed (the decal material and buffer stay active), so the checks are
byte-exact where the design asks for it:

  factor 0     all six factors 0: every view is byte-identical to no decal.
  factor 1     each property separately: only its own debug view changes; the other five
               stay byte-identical to no decal.
  values       the changed view shows the decal value (base colour, metallic, roughness,
               shading normal, emissive) and occlusion is the receiver*decal product,
               including a receiver without an occlusion map.
  normal       the decal normal map is an offset added to the receiver normal: weighted by
               normalBlendFactor without the base-colour alpha (an alpha-0 decal still adds
               relief and changes nothing else), no offset without relief, and
               normalOverrideFactor choosing the base the map is applied around.
  normal tex   the Normal Texture view shows the decal normal in the receiver's tangent
               frame on a receiver without a normal map; outside it and at factor 0 it is
               byte-identical to no decal.
  order       two 0.5-weight decals, values 1 then 0: 0.25 in that order, 0.5 reversed.
  untouched    a MASK receiver and a BLEND glass material do not read the decal buffer.
  switch       the decal switch off renders the A1.4 forward fallback (decal visible),
               while the draw-order switch off leaves the offset at 0.
  depth        normal gaps of 1 mm stay visible and 2 mm stay hidden at 0/45/70 degrees
               in perspective and orthographic views; 3 mm tolerance restores 2 mm decals.
               A 2 cm occluder hides decals; the forward fallback keeps its existing bias.

Run: python scripts/validate_asobo_decal.py
"""

import base64
import hashlib
import json
import math
import re
import struct
import sys
import zlib
from pathlib import Path

import harness_process as subprocess
from validate_asobo_normal import _read_png

REPO = Path(__file__).resolve().parents[1]
EXE = subprocess.candidate_exe()
OUT = REPO / "build" / "a2_2"
COMMON = ["--isolated", "--window-size", "1280x800", "--aa", "none",
          "--msfs-emissive-scale", "1.0",
          "--fixed-frame-time", "0.05", "--vsync", "off",
          "--select-node", "Receiver", "--camera-target", "0,0,0",
          "--camera-projection", "ortho", "--camera-yaw", "0", "--camera-pitch", "0",
          "--camera-distance", "2"]
VIEWS = ("basecolor", "metallic", "roughness", "shadingnormal", "emissive", "occlusion")
PROPERTY_VIEW = {
    "baseColorBlendFactor": "basecolor",
    "metallicBlendFactor": "metallic",
    "roughnessBlendFactor": "roughness",
    "normalBlendFactor": "shadingnormal",
    "emissiveBlendFactor": "emissive",
    "occlusionBlendFactor": "occlusion",
}
ALL_FACTORS = tuple(PROPERTY_VIEW)
ZERO_FACTORS = {name: 0.0 for name in ALL_FACTORS}
DECAL_COLOR = (0.9, 0.1, 0.2, 1.0)
DECAL_METALLIC = 1.0
DECAL_ROUGHNESS = 0.1
DECAL_EMISSIVE = (0.8, 0.4, 0.1)
DECAL_NORMAL = (0.6, 0.0, 0.8)
# The decal's tangent-space normal map (one texel), encoded (n + 1) / 2 in 8 bits.
DECAL_TANGENT_NORMAL = (0.6, 0.0, 0.8)


def encode_normal(n):
    return tuple(round((c + 1.0) * 0.5 * 255.0) for c in n)

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest() if path and Path(path).is_file() else None


def same(a, b):
    return sha(a) is not None and sha(a) == sha(b)


def srgb(value):
    """The shader's linearTosRGB approximation (pow(v, 1/2.2)), as 8-bit."""
    return round(255.0 * (value ** (1.0 / 2.2)))


def srgb3(color):
    return tuple(srgb(max(0.0, min(1.0, channel))) for channel in color)


def close(actual, expected, tolerance=3):
    return all(abs(a - e) <= tolerance for a, e in zip(actual, expected))


def make_png(value):
    """A 1x1 RGBA8 PNG: value is a grey level or an (r, g, b) tuple."""
    rgb = tuple(value) if isinstance(value, (tuple, list)) else (value, value, value)

    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(
            ">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    raw = b"\x00" + bytes([*rgb, 255])
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def quad_geometry(z, normal, half):
    positions = ((-half, -half, z), (half, -half, z), (half, half, z), (-half, half, z))
    normals = (normal,) * 4
    uvs = ((0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0))
    indices = (0, 1, 2, 0, 2, 3)
    return positions, normals, uvs, indices


def box_geometry(x_half, y_half, z_min, z_max):
    """Six quads with outward normals. The occluder material is double-sided, so a winding
    mistake cannot hide a face; the front face (+Z, facing the camera) is the one under test."""
    x, y, z0, z1 = x_half, y_half, z_min, z_max
    faces = (
        ((0.0, 0.0, 1.0), ((-x, -y, z1), (x, -y, z1), (x, y, z1), (-x, y, z1))),
        ((0.0, 0.0, -1.0), ((x, -y, z0), (-x, -y, z0), (-x, y, z0), (x, y, z0))),
        ((1.0, 0.0, 0.0), ((x, -y, z0), (x, y, z0), (x, y, z1), (x, -y, z1))),
        ((-1.0, 0.0, 0.0), ((-x, -y, z0), (-x, y, z0), (-x, y, z1), (-x, -y, z1))),
        ((0.0, 1.0, 0.0), ((-x, y, z0), (x, y, z0), (x, y, z1), (-x, y, z1))),
        ((0.0, -1.0, 0.0), ((-x, -y, z0), (x, -y, z0), (x, -y, z1), (-x, -y, z1))),
    )
    positions, normals, uvs, indices = [], [], [], []
    for normal, corners in faces:
        base = len(positions)
        positions.extend(corners)
        normals.extend((normal,) * 4)
        uvs.extend(((0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0)))
        indices.extend((base, base + 1, base + 2, base, base + 2, base + 3))
    return tuple(positions), tuple(normals), tuple(uvs), tuple(indices)


def write_fixture(name, *, decal_count=0, factors=None, receiver_alpha="OPAQUE",
                  receiver_color=(0.2, 0.4, 0.6), receiver_metallic=0.25,
                  receiver_roughness=0.75, receiver_emissive=(0.1, 0.2, 0.3),
                  receiver_occlusion=128, receiver_glass=False, receiver_detail=False,
                  receiver_clear_coat=False, decal_occlusion=128,
                  decals=None, normal_override=None, decal_z=0.01,
                  decal_normal_map=DECAL_TANGENT_NORMAL, decal_mesh_normal=DECAL_NORMAL,
                  occluder_z=None, occluder_color=(0.05, 0.8, 0.1)):
    """Writes one fixture. The decal material always exists (so the buffer is active);
    decal_count controls how many decal nodes are in the scene. decals overrides the
    per-decal (color4, offset) list for the overlap case. decal_z places the decal relative
    to the receiver (negative is behind it); occluder_z adds a box whose front face is at
    that z, in front of the decal. decal_normal_map is the decal's tangent-space normal
    (None: no normal texture); decal_mesh_normal is the decal quad's vertex normal."""
    document = {
        "asset": {"version": "2.0", "generator": "A2.2 decal validation fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "Receiver", "mesh": 0}],
        "meshes": [],
        "materials": [],
        "accessors": [],
        "bufferViews": [],
        "extensionsUsed": ["ASOBO_material_geometry_decal", "ASOBO_material_draw_order"],
    }
    binary = bytearray()

    def add_geometry(positions, normals, uvs, indices, material, node_name):
        base = len(binary)
        position_bytes = b"".join(struct.pack("<3f", *value) for value in positions)
        normal_bytes = b"".join(struct.pack("<3f", *value) for value in normals)
        uv_bytes = b"".join(struct.pack("<2f", *value) for value in uvs)
        index_bytes = b"".join(struct.pack("<H", value) for value in indices)
        binary.extend(position_bytes + normal_bytes + uv_bytes + index_bytes)
        view = len(document["bufferViews"])
        document["bufferViews"].extend([
            {"buffer": 0, "byteOffset": base, "byteLength": len(position_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": base + len(position_bytes), "byteLength": len(normal_bytes),
             "target": 34962},
            {"buffer": 0, "byteOffset": base + len(position_bytes) + len(normal_bytes),
             "byteLength": len(uv_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": base + len(position_bytes) + len(normal_bytes) + len(uv_bytes),
             "byteLength": len(index_bytes), "target": 34963},
        ])
        minimum = [min(value[axis] for value in positions) for axis in range(3)]
        maximum = [max(value[axis] for value in positions) for axis in range(3)]
        accessor = len(document["accessors"])
        document["accessors"].extend([
            {"bufferView": view, "componentType": 5126, "count": len(positions), "type": "VEC3",
             "min": minimum, "max": maximum},
            {"bufferView": view + 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": view + 2, "componentType": 5126, "count": len(uvs), "type": "VEC2"},
            {"bufferView": view + 3, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        ])
        mesh = len(document["meshes"])
        document["meshes"].append({
            "name": node_name,
            "primitives": [{"attributes": {"POSITION": accessor, "NORMAL": accessor + 1,
                                           "TEXCOORD_0": accessor + 2},
                            "indices": accessor + 3, "material": material}],
        })
        document["nodes"].append({"name": node_name, "mesh": mesh})
        return mesh

    def add_quad(z, normal, half, material, node_name):
        return add_geometry(*quad_geometry(z, normal, half), material, node_name)

    receiver = {
        "name": "Receiver",
        "alphaMode": receiver_alpha,
        "pbrMetallicRoughness": {"baseColorFactor": [*receiver_color, 1.0],
                                 "metallicFactor": receiver_metallic,
                                 "roughnessFactor": receiver_roughness},
        "emissiveFactor": list(receiver_emissive),
    }
    if receiver_alpha == "MASK":
        receiver["alphaCutoff"] = 0.5
    if receiver_occlusion is not None:
        receiver["occlusionTexture"] = {"index": 0, "strength": 1.0}
    receiver_extensions = {}
    if receiver_glass:
        receiver_extensions["ASOBO_material_glass_v2"] = {}
    if receiver_clear_coat:
        receiver_extensions["ASOBO_material_clear_coat_v2"] = {}
    if receiver_detail:
        # The exporter writes the detail map for any material with a detail texture; it is an
        # option of the Standard type, not a type of its own (semantics section 14).
        receiver_extensions["ASOBO_material_detail_map"] = {"detailColorTexture": {"index": 0}}
    if receiver_extensions:
        receiver["extensions"] = receiver_extensions
        document["extensionsUsed"].extend(receiver_extensions)
    document["materials"].append(receiver)

    decal_specs = decals
    if decal_specs is None:
        decal_specs = [((*DECAL_COLOR[:3], 1.0), 10)] * max(1, decal_count)
    for index, (color, offset) in enumerate(decal_specs):
        decal_factors = dict(factors if factors is not None else {})
        if normal_override is not None:
            decal_factors["normalOverrideFactor"] = normal_override
        material = {
            "name": "Decal" + (str(index) if index else ""),
            "alphaMode": "BLEND",
            "pbrMetallicRoughness": {"baseColorFactor": list(color),
                                     "metallicFactor": DECAL_METALLIC,
                                     "roughnessFactor": DECAL_ROUGHNESS},
            "emissiveFactor": list(DECAL_EMISSIVE),
            "occlusionTexture": {"index": 1, "strength": 1.0},
            "extensions": {
                "ASOBO_material_geometry_decal": decal_factors,
                "ASOBO_material_draw_order": {"drawOrderOffset": offset},
            },
        }
        if decal_normal_map is not None:
            material["normalTexture"] = {"index": 2}
        document["materials"].append(material)

    if occluder_z is not None:
        document["materials"].append({
            "name": "Occluder",
            "alphaMode": "OPAQUE",
            "doubleSided": True,
            "pbrMetallicRoughness": {"baseColorFactor": [*occluder_color, 1.0],
                                     "metallicFactor": 0.0, "roughnessFactor": 1.0},
            "emissiveFactor": [0.0, 0.0, 0.0],
        })

    receiver_mesh = add_quad(0.0, (0.0, 0.0, 1.0), 1.0, 0, "Receiver")
    assert receiver_mesh == 0
    for index in range(decal_count):
        add_quad(decal_z, decal_mesh_normal, 0.5, 1 + index, "Decal" + (str(index) if index else ""))
        document["scenes"][0]["nodes"].append(len(document["nodes"]) - 1)
    if occluder_z is not None:
        occluder_material = len(document["materials"]) - 1
        positions, normals, uvs, indices = box_geometry(0.35, 0.35, occluder_z - 0.02, occluder_z)
        add_geometry(positions, normals, uvs, indices, occluder_material, "Occluder")
        document["scenes"][0]["nodes"].append(len(document["nodes"]) - 1)

    document["buffers"] = [{"byteLength": len(binary), "uri": "data:application/octet-stream;base64,"
                                                          + base64.b64encode(bytes(binary)).decode("ascii")}]
    document["images"] = [
        {"uri": "data:image/png;base64," + base64.b64encode(make_png(receiver_occlusion or 255)).decode("ascii")},
        {"uri": "data:image/png;base64," + base64.b64encode(make_png(decal_occlusion)).decode("ascii")},
        {"uri": "data:image/png;base64,"
                + base64.b64encode(make_png(encode_normal(decal_normal_map or (0.0, 0.0, 1.0)))).decode("ascii")},
    ]
    document["textures"] = [{"source": 0}, {"source": 1}, {"source": 2}]
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(document, indent=1), encoding="utf-8")
    return path


def capture(model, view, label, extra=()):
    png = OUT / f"{label}_{view}.png"
    png.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), str(model), *COMMON, *extra, "--debug-view", view,
                             "--capture-viewport", str(png), "--capture-frame", "24",
                             "--profile-report", str(OUT / f"{label}_{view}.json"),
                             "--exit-after-profile"], cwd=REPO, capture_output=True, text=True, timeout=300)
    if result.returncode != 0 or not png.is_file():
        check(False, f"{label}_{view}: capture failed ({result.returncode}) {result.stderr[-200:]}")
        return None
    log = result.stdout + result.stderr
    png.with_suffix(".log").write_text(log, encoding="utf-8")
    if "GL automation errors: 0" not in log or re.search(r"GL error (?:0x|during)|GL error \(0x", log):
        check(False, f"{label}_{view}: GL diagnostics failed; see capture log")
        return None
    return png


def pixels(path):
    image = _read_png(path) if path is not None else None
    if image is None:
        return None
    width, height, channels, data = image
    center = (width // 2, height // 2)
    outside = (width // 6, height // 2)
    result = {}
    for name, (x, y) in (("center", center), ("outside", outside)):
        offset = (y * width + x) * channels
        result[name] = tuple(data[offset:offset + 3])
    return result


def expected_shading_normal(mesh_normal, override, tangent_normal):
    """The receiver (mesh normal +z, no normal map) plus the decal's normal offset, as the
    shaders compute it: the decal map is applied around base = mix(+z, decal mesh normal,
    override) with the UV tangent +x, the offset is that minus base, the receiver adds it.
    Returns the 8-bit Shading Normal colour (n + 1) / 2."""
    def norm(v):
        length = math.sqrt(sum(c * c for c in v))
        return tuple(c / length for c in v)

    def dot(a, b):
        return sum(x * y for x, y in zip(a, b))

    def cross(a, b):
        return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])

    ntex = norm(tuple(c / 255.0 * 2.0 - 1.0 for c in encode_normal(tangent_normal)))
    base = norm(tuple((1.0 - override) * r + override * m for r, m in zip((0.0, 0.0, 1.0), norm(mesh_normal))))
    t = norm(tuple(x - b * dot(base, (1.0, 0.0, 0.0)) for x, b in zip((1.0, 0.0, 0.0), base)))
    bt = cross(base, t)
    decal = norm(tuple(ntex[0] * t[i] + ntex[1] * bt[i] + ntex[2] * base[i] for i in range(3)))
    n = norm(tuple(r + d - b for r, d, b in zip((0.0, 0.0, 1.0), decal, base)))
    return tuple(round((c + 1.0) * 0.5 * 255.0) for c in n)


def property_factors(property_name):
    factors = dict(ZERO_FACTORS)
    factors[property_name] = 1.0
    return factors


def main():
    OUT.mkdir(parents=True, exist_ok=True)

    print("[baseline: decal material present but no decal node]")
    baseline_model = write_fixture("no_decal", decal_count=0, factors=ZERO_FACTORS)
    baseline = {view: capture(baseline_model, view, "no_decal") for view in VIEWS}
    if not check(all(baseline.values()), "all six baseline views captured"):
        print("FAILED: baseline capture failed")
        sys.exit(1)

    print("[factor 0: every property stays byte-identical to no decal]")
    zero_model = write_fixture("zero", decal_count=1, factors=ZERO_FACTORS)
    for view in VIEWS:
        image = capture(zero_model, view, "zero")
        check(same(image, baseline[view]), f"factor 0 {view}: byte-identical to no decal")

    print("[factor 1: only the selected property changes]")
    for property_name, view in PROPERTY_VIEW.items():
        model = write_fixture("only_" + view, decal_count=1, factors=property_factors(property_name))
        own = capture(model, view, "only_" + view)
        check(own is not None and not same(own, baseline[view]), f"{property_name}: {view} view changes")
        for other in VIEWS:
            if other == view:
                continue
            image = capture(model, other, "only_" + view)
            check(same(image, baseline[other]), f"{property_name}: {other} stays byte-identical")

    print("[values: the decal value is what the debug view shows]")
    for property_name, expected in (
        ("baseColorBlendFactor", srgb3(DECAL_COLOR[:3])),
        ("metallicBlendFactor", srgb3((DECAL_METALLIC,) * 3)),
        ("roughnessBlendFactor", srgb3((DECAL_ROUGHNESS,) * 3)),
        ("normalBlendFactor", (204, 128, 230)),
        ("emissiveBlendFactor", srgb3(DECAL_EMISSIVE)),
    ):
        view = PROPERTY_VIEW[property_name]
        sample = pixels(capture(write_fixture("value_" + view, decal_count=1,
                                              factors=property_factors(property_name)), view, "value_" + view))
        if sample is None:
            check(False, f"{property_name}: value capture decodable")
            continue
        check(close(sample["center"], expected),
              f"{property_name}: centre {sample['center']} is the decal value {expected}")

    print("[normal: an offset added to the receiver normal, weighted without the alpha]")
    # normalOverrideFactor picks the base the decal map is applied around: the receiver mesh
    # normal (prepass G0) at 0, the tilted decal mesh normal at 1 (the default).
    tilt_back = (-0.6, 0.0, 0.8)
    for override in (0.0, 1.0):
        label = f"normal_override_{int(override)}"
        model = write_fixture(label, decal_count=1, factors=property_factors("normalBlendFactor"),
                              normal_override=override, decal_normal_map=tilt_back)
        expected = expected_shading_normal(DECAL_NORMAL, override, tilt_back)
        sample = pixels(capture(model, "shadingnormal", label))
        if sample:
            check(close(sample["center"], expected),
                  f"normalOverrideFactor {override:g}: centre {sample['center']} == {expected}")
    # A flat (absent) decal normal map is no offset: the receiver normal stays byte-identical,
    # even though the decal mesh normal is tilted.
    flat_model = write_fixture("normal_flat", decal_count=1, factors=property_factors("normalBlendFactor"),
                               decal_normal_map=None)
    check(same(capture(flat_model, "shadingnormal", "normal_flat"), baseline["shadingnormal"]),
          "a decal without normal relief leaves the shading normal byte-identical")
    # The base-colour alpha does not weight the normal: a fully transparent decal still adds its
    # relief (MSFS shows legend bevels beyond the legend alpha) and changes nothing else.
    transparent_model = write_fixture("normal_alpha_0", decal_count=1, factors={},
                                      decals=[((*DECAL_COLOR[:3], 0.0), 10)])
    sample = pixels(capture(transparent_model, "shadingnormal", "normal_alpha_0"))
    expected = expected_shading_normal(DECAL_NORMAL, 1.0, DECAL_TANGENT_NORMAL)
    if sample:
        check(close(sample["center"], expected),
              f"alpha 0 decal: centre {sample['center']} carries the decal relief {expected}")
    for view in ("basecolor", "metallic", "roughness", "emissive", "occlusion"):
        check(same(capture(transparent_model, view, "normal_alpha_0"), baseline[view]),
              f"alpha 0 decal: {view} stays byte-identical")

    # The Normal Texture view shows decal normals too: on a receiver without a normal map the
    # decal-offset normal in the receiver's tangent frame (here t = +x, b = +y, so the shading
    # normal (0.6, 0, 0.8) reads as itself), and the missing-data checkerboard outside the decal.
    print("[normal texture view: the decal normal on a receiver without a normal map]")
    texture_model = write_fixture("normal_texture", decal_count=1, factors=property_factors("normalBlendFactor"))
    texture_view = capture(texture_model, "normaltexture", "normal_texture")
    texture_baseline = capture(baseline_model, "normaltexture", "no_decal")
    sample = pixels(texture_view)
    base_sample = pixels(texture_baseline)
    if sample and base_sample:
        check(close(sample["center"], (204, 128, 230)),
              f"normal texture view: centre {sample['center']} is the decal normal (204, 128, 230)")
        check(sample["outside"] == base_sample["outside"],
              f"normal texture view: outside {sample['outside']} keeps the no-decal value {base_sample['outside']}")
        check(base_sample["center"] != sample["center"],
              f"normal texture view without the decal: centre {base_sample['center']} is not the decal normal")
    zero_texture = capture(zero_model, "normaltexture", "zero")
    check(same(zero_texture, texture_baseline), "normal texture view: factor 0 is byte-identical to no decal")

    print("[occlusion: receiver * decal, and without a receiver occlusion map]")
    occlusion_model = write_fixture("occlusion", decal_count=1, factors=property_factors("occlusionBlendFactor"))
    sample = pixels(capture(occlusion_model, "occlusion", "occlusion_value"))
    if sample:
        expected = srgb((128.0 / 255.0) * (128.0 / 255.0))
        outside = srgb(128.0 / 255.0)
        check(close(sample["center"], (expected,) * 3),
              f"occlusion multiplies: centre {sample['center']} == receiver*decal {expected}")
        check(close(sample["outside"], (outside,) * 3),
              f"occlusion outside the decal keeps the receiver value {outside}")
    no_map_model = write_fixture("occlusion_no_map", decal_count=1,
                                 factors=property_factors("occlusionBlendFactor"), receiver_occlusion=None)
    sample = pixels(capture(no_map_model, "occlusion", "occlusion_no_map"))
    if sample:
        decal_ao = srgb(128.0 / 255.0)
        check(close(sample["center"], (decal_ao,) * 3),
              f"occlusion without a receiver map: centre {sample['center']} == decal AO {decal_ao}")
        check(close(sample["outside"], (255, 255, 255)),
              f"occlusion without a receiver map: outside {sample['outside']} stays 1.0")

    print("[order: two 0.5-weight decals, 1 then 0 gives 0.25, reversed 0.5]")
    white = (1.0, 1.0, 1.0, 0.5)
    black = (0.0, 0.0, 0.0, 0.5)
    order = {
        "order_1_then_0": ([(white, 10), (black, 20)], srgb(0.25)),
        "order_0_then_1": ([(white, 20), (black, 10)], srgb(0.5)),
    }
    for name, (decals, expected) in order.items():
        model = write_fixture(name, decal_count=2, factors=property_factors("baseColorBlendFactor"),
                              receiver_color=(0.0, 0.0, 0.0), receiver_metallic=0.0,
                              receiver_roughness=1.0, receiver_emissive=(0.0, 0.0, 0.0),
                              receiver_occlusion=None, decals=decals)
        sample = pixels(capture(model, "basecolor", name))
        if sample:
            check(close(sample["center"], (expected,) * 3),
                  f"{name}: centre {sample['center']} == {expected} linear->sRGB")

    print("[non-receivers: MASK and glass are untouched]")
    for label, alpha, glass in (("mask", "MASK", False), ("glass", "BLEND", True)):
        with_decal = write_fixture(label + "_with_decal", decal_count=1,
                                   factors=property_factors("baseColorBlendFactor"),
                                   receiver_alpha=alpha, receiver_glass=glass)
        without = write_fixture(label + "_no_decal", decal_count=0, factors=ZERO_FACTORS,
                                receiver_alpha=alpha, receiver_glass=glass)
        a = capture(with_decal, "basecolor", label + "_with_decal")
        b = capture(without, "basecolor", label + "_no_decal")
        check(same(a, b), f"{label} receiver: decal leaves the base-colour view byte-identical")

    print("[receivers with a detail map: Standard and clear coat both take the decal]")
    decal_color = srgb3(DECAL_COLOR[:3])
    for label, clear_coat in (("detail", False), ("clearcoat_detail", True)):
        model = write_fixture(label + "_with_decal", decal_count=1,
                              factors=property_factors("baseColorBlendFactor"),
                              receiver_detail=True, receiver_clear_coat=clear_coat)
        sample = pixels(capture(model, "basecolor", label + "_with_decal"))
        check(sample is not None and close(sample["center"], decal_color),
              f"{label} receiver: centre {sample['center'] if sample else None} is the decal {decal_color}")

    print("[depth: 1 mm accepted, 2 mm rejected at 0/45/70 degrees; 3 mm override and 2 cm occluder]")
    forward_only = ("--asobo-disable", "ASOBO_material_geometry_decal")
    behind = write_fixture("behind_1mm", decal_count=1,
                           factors=property_factors("baseColorBlendFactor"), decal_z=-0.001)
    behind_2mm = write_fixture("behind_2mm", decal_count=1,
                               factors=property_factors("baseColorBlendFactor"), decal_z=-0.002)
    receiver_color = srgb3((0.2, 0.4, 0.6))
    for projection in ("ortho", "perspective"):
        for angle in (0, 45, 70):
            camera = ("--camera-projection", projection, "--camera-yaw", str(angle))
            for model, gap, expected, tolerance in (
                (behind, 1, decal_color, ()),
                (behind_2mm, 2, receiver_color, ()),
                (behind_2mm, 2, decal_color, ("--decal-tolerance", "3")),
            ):
                label = f"behind_{gap}mm_{projection}_{angle}" + ("_tol3" if tolerance else "")
                sample = pixels(capture(model, "basecolor", label, (*camera, *tolerance)))
                check(sample is not None and close(sample["center"], expected),
                      f"{label}: centre {sample['center'] if sample else None} == {expected}")
    sample = pixels(capture(behind, "basecolor", "behind_1mm_forward", forward_only))
    if sample:
        check(close(sample["center"], decal_color),
              f"1 mm behind (forward): centre {sample['center']} is the decal {decal_color}")

    occluder_color = (0.05, 0.8, 0.1)
    occluder = write_fixture("occluder_2cm", decal_count=1,
                             factors=property_factors("baseColorBlendFactor"), decal_z=0.001,
                             occluder_z=0.021, occluder_color=occluder_color)
    occluder_srgb = srgb3(occluder_color)
    sample = pixels(capture(occluder, "basecolor", "occluder_2cm"))
    if sample:
        check(close(sample["center"], occluder_srgb),
              f"occluder 2 cm in front (buffer): centre {sample['center']} is the occluder "
              f"{occluder_srgb}, not the decal")
    sample = pixels(capture(occluder, "basecolor", "occluder_2cm_forward", forward_only))
    if sample:
        check(close(sample["center"], occluder_srgb),
              f"occluder 2 cm in front (forward): centre {sample['center']} is the occluder "
              f"{occluder_srgb}, not the decal")

    print("[switch off: the decal stays in the A1.4 forward list]")
    forward = write_fixture("switch_off", decal_count=1, factors=property_factors("baseColorBlendFactor"))
    image = OUT / "switch_off_basecolor.png"
    image.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), str(forward), *COMMON, "--asobo-disable",
                             "ASOBO_material_geometry_decal", "--debug-view", "basecolor",
                             "--capture-viewport", str(image), "--capture-frame", "24",
                             "--profile-report", str(OUT / "switch_off.json"),
                             "--exit-after-profile"], cwd=REPO, capture_output=True, text=True, timeout=300)
    sample = pixels(image) if result.returncode == 0 and image.is_file() else None
    if sample is None:
        check(False, "switch off: capture failed")
    else:
        # The forward fallback draws the red decal over the receiver; it is still visible.
        check(not close(sample["center"], srgb3((0.2, 0.4, 0.6))),
              f"switch off: the forward decal is visible at the centre {sample['center']}")

    print()
    if failures:
        print(f"FAILED ({len(failures)}):")
        for failure in failures:
            print("  - " + failure)
        sys.exit(1)
    print("A2.2 decal checks passed")


if __name__ == "__main__":
    main()
