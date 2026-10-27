# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A1.4: ASOBO_material_draw_order.

Overlapping, head-on unlit BLEND quads. Decals sort by offset ascending and then back
to front; glass adds the offset to the distance key (key = distance + offset, larger key
drawn first); every other material keeps the old order. The quads use opaque alpha so
the colour in the overlap names the draw that ended up on top; their node pivots are
deliberately offset from the geometry bounds in the equality case, which tells the
bounding-box-centre sort apart from the old pivot sort.

A2 decals accumulate material properties on opaque receivers, so decal cases add
a black opaque unlit receiver within the 1.2 mm tolerance. Free-floating quads no
longer show decal order. A4 glass has its own pass: the disabled-order reference
keeps the glass extension and removes only draw_order (same binary). Ordinary
quads have pivots at their geometry centres so distance order remains observable
when A2/A4 disable depth writes; the equality case retains misleading pivots and
compares disabled order against decals without draw_order, preserving A2.
"""

import base64
import json
import struct
from pathlib import Path

from validate_asobo_common import EXE, REPO, check, finish, fixture, render, same
from validate_asobo_normal import _read_png

OUT = REPO / "build" / "asobo_draworder"
DRAW_ORDER = "ASOBO_material_draw_order"
DECAL = "ASOBO_material_geometry_decal"
GLASS = "ASOBO_material_glass_v2"
WINDSHIELD = "ASOBO_material_windshield_v3"
RED = (1.0, 0.0, 0.0)
BLUE = (0.0, 0.0, 1.0)


def _quad_binary(quad):
    z = quad["z"]
    positions = ((-1.0, -1.0, z), (1.0, -1.0, z), (1.0, 1.0, z), (-1.0, 1.0, z))
    normals = ((0.0, 0.0, 1.0),) * 4
    indices = (0, 1, 2, 0, 2, 3)
    data = b"".join(struct.pack("<3f", *value) for value in positions)
    data += b"".join(struct.pack("<3f", *value) for value in normals)
    data += struct.pack("<6H", *indices)
    return data


def make_base(name, quads, receiver=False):
    """Writes the shared geometry with KHR_materials_unlit and no ASOBO extensions."""
    OUT.mkdir(parents=True, exist_ok=True)
    if receiver:
        quads = quads + [{"name": "receiver", "color": (0.0, 0.0, 0.0), "z": -0.0006, "opaque": True}]
    binary = b""
    buffer_views = []
    accessors = []
    meshes = []
    nodes = []
    materials = []
    for index, quad in enumerate(quads):
        # Give ordinary cases meaningful distance keys even with depth writes off.
        # The equality case explicitly supplies misleading pivots to test bounds.
        if "pivot" not in quad:
            quad = dict(quad, pivot=quad["z"], z=0.0)
        base = len(binary)
        binary += _quad_binary(quad)
        buffer_views.extend([
            {"buffer": 0, "byteOffset": base, "byteLength": 48, "target": 34962},
            {"buffer": 0, "byteOffset": base + 48, "byteLength": 48, "target": 34962},
            {"buffer": 0, "byteOffset": base + 96, "byteLength": 12, "target": 34963},
        ])
        accessors.extend([
            {"bufferView": 3 * index, "componentType": 5126, "count": 4, "type": "VEC3",
             "min": [-1.0, -1.0, quad["z"]], "max": [1.0, 1.0, quad["z"]]},
            {"bufferView": 3 * index + 1, "componentType": 5126, "count": 4, "type": "VEC3"},
            {"bufferView": 3 * index + 2, "componentType": 5123, "count": 6, "type": "SCALAR"},
        ])
        meshes.append({"primitives": [{"attributes": {"POSITION": 3 * index, "NORMAL": 3 * index + 1},
                                      "indices": 3 * index + 2, "material": index}]})
        nodes.append({"name": quad["name"], "mesh": index, "translation": [0.0, 0.0, quad.get("pivot", 0.0)]})
        materials.append({
            "name": quad["name"],
            "pbrMetallicRoughness": {"baseColorFactor": [*quad["color"], 1.0],
                                     "metallicFactor": 0.0, "roughnessFactor": 1.0},
            "alphaMode": "OPAQUE" if quad.get("opaque") else "BLEND",
            "doubleSided": True,
            "extensions": {"KHR_materials_unlit": {}},
        })

    document = {
        "asset": {"version": "2.0", "generator": "Glider A1.4 draw-order fixture"},
        "scene": 0,
        "scenes": [{"nodes": list(range(len(nodes)))}],
        "nodes": nodes,
        "meshes": meshes,
        "materials": materials,
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": len(binary),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(binary).decode("ascii")}],
        "extensionsUsed": ["KHR_materials_unlit"],
    }
    path = OUT / name
    path.write_text(json.dumps(document, indent=1), encoding="utf-8")
    return path


def declared(name, base, material_extensions):
    """Adds and declares ASOBO extensions with the common fixture helper."""
    return fixture(name, material_extensions, base=base, folder=OUT)


def capture(model, name, flags=()):
    return render(EXE, model, name, flags, OUT)


def undeclared(name, declared_path):
    """Keeps the material extensions but removes every ASOBO name from extensionsUsed."""
    document = json.loads(Path(declared_path).read_text(encoding="utf-8"))
    document["extensionsUsed"] = [name for name in document["extensionsUsed"] if not name.startswith("ASOBO_")]
    path = OUT / name
    path.write_text(json.dumps(document, indent=1), encoding="utf-8")
    return path


def overlap_color(path):
    """R or B when the viewport centre is clearly red or blue, else None."""
    image = _read_png(path) if path is not None else None
    if image is None:
        return None
    width, height, channels, pixels = image
    offset = ((height // 2) * width + width // 2) * channels
    red, green, blue = pixels[offset], pixels[offset + 1], pixels[offset + 2]
    if red > 100 and blue < 80 and green < 80:
        return "R"
    if blue > 100 and red < 80 and green < 80:
        return "B"
    return None


def expect(path, channel, label):
    actual = overlap_color(path)
    return check(actual == channel, f"{label}: {channel} on top (centre channel {actual})")


def main():
    OUT.mkdir(parents=True, exist_ok=True)

    # 1. Decals: the higher offset wins even when that decal is the farther one.
    base = make_base("draworder_decals_base.gltf", [
        {"name": "decal_near", "color": RED, "z": 0.0003},
        {"name": "decal_far", "color": BLUE, "z": -0.0003},
    ], receiver=True)
    decals = declared("draworder_decals", base, {
        0: {DECAL: {}, DRAW_ORDER: {"drawOrderOffset": 3}},
        1: {DECAL: {}, DRAW_ORDER: {"drawOrderOffset": 9}},
    })
    reference = capture(base, "draworder_decals_reference")
    on = capture(decals, "draworder_decals")
    expect(on, "B", "higher decal offset on the farther quad")
    expect(reference, "R", "reference without the extension keeps back-to-front order")

    # 2. Equal decal offsets keep the back-to-front order, by bounding-box centre: the
    #    red pivot is behind the blue one, the red geometry is in front.
    base = make_base("draworder_decals_equal_base.gltf", [
        {"name": "decal_near", "color": RED, "z": 0.5003, "pivot": -0.5},
        {"name": "decal_far", "color": BLUE, "z": -0.5003, "pivot": 0.5},
    ], receiver=True)
    equal = declared("draworder_decals_equal", base, {
        0: {DECAL: {}, DRAW_ORDER: {"drawOrderOffset": 7}},
        1: {DECAL: {}, DRAW_ORDER: {"drawOrderOffset": 7}},
    })
    reference = capture(base, "draworder_decals_equal_reference")
    on = capture(equal, "draworder_decals_equal")
    expect(on, "R", "equal offsets sort by the bounding-box centre (near quad last)")
    expect(reference, "R", "reference without the extension keeps its previous order")
    disabled = capture(equal, "draworder_decals_equal_disabled", ["--asobo-disable", DRAW_ORDER])
    no_order = declared("draworder_decals_equal_no_order", base, {0: {DECAL: {}}, 1: {DECAL: {}}})
    no_order_capture = capture(no_order, "draworder_decals_equal_no_order")
    check(same(disabled, no_order_capture), "--asobo-disable keeps the old decal order byte-identically")

    # 3. Decals draw before glass, even when glass would be drawn first by distance.
    base = make_base("draworder_decal_glass_base.gltf", [
        {"name": "decal_near", "color": RED, "z": 0.0003},
        {"name": "glass_far", "color": BLUE, "z": -0.0003},
    ], receiver=True)
    mixed = declared("draworder_decal_glass", base, {
        0: {DECAL: {}, DRAW_ORDER: {"drawOrderOffset": 5}},
        1: {GLASS: {}},
    })
    reference = capture(base, "draworder_decal_glass_reference")
    on = capture(mixed, "draworder_decal_glass")
    expect(on, "B", "glass is drawn after the decal")
    expect(reference, "R", "reference draws the far glass first, then the near decal")

    # 4. Glass: key = distance + offset, larger key drawn first. A positive offset on the
    #    near quad makes it sort behind the far one; a small one does not.
    base = make_base("draworder_glass_base.gltf", [
        {"name": "glass_near", "color": RED, "z": 0.1},
        {"name": "glass_far", "color": BLUE, "z": -0.1},
    ])
    large = declared("draworder_glass_large", base, {
        0: {GLASS: {}, DRAW_ORDER: {"drawOrderOffset": 2}},
        1: {GLASS: {}},
    })
    small = declared("draworder_glass_small", base, {
        0: {GLASS: {}, DRAW_ORDER: {"drawOrderOffset": 0.02}},
        1: {GLASS: {}},
    })
    glass_reference = declared("draworder_glass_no_order", base, {0: {GLASS: {}}, 1: {GLASS: {}}})
    reference = capture(glass_reference, "draworder_glass_reference")
    expect(capture(large, "draworder_glass_large"), "B", "large positive glass offset swaps the order")
    expect(capture(small, "draworder_glass_small"), "R", "small glass offset keeps the distance order")
    expect(reference, "R", "reference glass order is back to front")
    disabled = capture(large, "draworder_glass_large_disabled", ["--asobo-disable", DRAW_ORDER])
    check(same(disabled, reference), "--asobo-disable keeps the glass order byte-identically")

    # 5. Windshield is in the same distance-offset class as glass.
    base = make_base("draworder_windshield_base.gltf", [
        {"name": "windshield_near", "color": RED, "z": 0.1},
        {"name": "windshield_far", "color": BLUE, "z": -0.1},
    ])
    windshield = declared("draworder_windshield", base, {
        0: {WINDSHIELD: {}, DRAW_ORDER: {"drawOrderOffset": 2}},
        1: {WINDSHIELD: {}},
    })
    expect(capture(windshield, "draworder_windshield"), "B",
           "windshield uses the same distance-offset key as glass")

    # 6. A non-decal, non-glass blended material ignores the offset: its order is exactly
    #    the file's order without the extension.
    base = make_base("draworder_ordinary_base.gltf", [
        {"name": "ordinary_near", "color": RED, "z": 0.1},
        {"name": "ordinary_far", "color": BLUE, "z": -0.1},
    ])
    ordinary = declared("draworder_ordinary", base, {
        0: {DRAW_ORDER: {"drawOrderOffset": 100}},
    })
    reference = capture(base, "draworder_ordinary_reference")
    on = capture(ordinary, "draworder_ordinary")
    expect(on, "R", "an offset on a plain blended material is ignored")
    check(same(on, reference), "plain blended material with an offset is byte-identical to no extension")

    # 7. Switch off, and the same material extension left undeclared, are the old order.
    base = make_base("draworder_switch_base.gltf", [
        {"name": "decal_near", "color": RED, "z": 0.0003},
        {"name": "decal_far", "color": BLUE, "z": -0.0003},
    ], receiver=True)
    switched = declared("draworder_switch", base, {
        0: {DECAL: {}, DRAW_ORDER: {"drawOrderOffset": 3}},
        1: {DECAL: {}, DRAW_ORDER: {"drawOrderOffset": 9}},
    })
    silent = undeclared("draworder_undeclared.gltf", switched)
    reference = capture(base, "draworder_switch_reference")
    disabled = capture(switched, "draworder_switch_disabled", ["--asobo-disable", DRAW_ORDER])
    all_off = capture(switched, "draworder_switch_all_off", ["--asobo", "off"])
    undeclared_capture = capture(silent, "draworder_undeclared")
    expect(disabled, "R", "--asobo-disable restores the old order")
    check(same(disabled, reference), "--asobo-disable is byte-identical to the file without the extension")
    check(same(all_off, reference), "--asobo off is byte-identical to the file without the extension")
    check(same(undeclared_capture, reference),
          "an extension not declared in extensionsUsed is byte-identical to the file without it")

    finish()


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        check(False, str(error))
        finish()
