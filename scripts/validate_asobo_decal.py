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
  order        two 0.5-weight decals, values 1 then 0: 0.25 in that order, 0.5 reversed.
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
import struct
import sys
import zlib
from pathlib import Path

import harness_process as subprocess
from validate_asobo_normal import _read_png

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "a2_2"
COMMON = ["--isolated", "--window-size", "1280x800", "--aa", "none",
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
    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(
            ">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    raw = b"\x00" + bytes([value, value, value, 255])
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
                  receiver_occlusion=128, receiver_glass=False, decal_occlusion=128,
                  decals=None, normal_override=None, decal_z=0.01,
                  occluder_z=None, occluder_color=(0.05, 0.8, 0.1)):
    """Writes one fixture. The decal material always exists (so the buffer is active);
    decal_count controls how many decal nodes are in the scene. decals overrides the
    per-decal (color4, offset) list for the overlap case. decal_z places the decal relative
    to the receiver (negative is behind it); occluder_z adds a box whose front face is at
    that z, in front of the decal."""
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
    if receiver_glass:
        receiver["extensions"] = {"ASOBO_material_glass_v2": {}}
        document["extensionsUsed"].append("ASOBO_material_glass_v2")
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
        add_quad(decal_z, DECAL_NORMAL, 0.5, 1 + index, "Decal" + (str(index) if index else ""))
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
    ]
    document["textures"] = [{"source": 0}, {"source": 1}]
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

    # normalOverrideFactor 0 must use the receiver mesh normal (prepass G0) as the base.
    override_model = write_fixture("normal_override_0", decal_count=1,
                                   factors=property_factors("normalBlendFactor"), normal_override=0.0)
    sample = pixels(capture(override_model, "shadingnormal", "normal_override_0"))
    if sample:
        check(close(sample["center"], (128, 128, 255), tolerance=2),
              f"normalOverrideFactor 0: centre {sample['center']} is the receiver G0 normal")

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

    print("[depth: 1 mm accepted, 2 mm rejected at 0/45/70 degrees; 3 mm override and 2 cm occluder]")
    forward_only = ("--asobo-disable", "ASOBO_material_geometry_decal")
    behind = write_fixture("behind_1mm", decal_count=1,
                           factors=property_factors("baseColorBlendFactor"), decal_z=-0.001)
    decal_color = srgb3(DECAL_COLOR[:3])
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
