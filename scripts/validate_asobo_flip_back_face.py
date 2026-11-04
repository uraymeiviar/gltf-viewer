# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""ASOBO_material_flip_back_face: the back of an MSFS double-sided material keeps the front normal by default.

The SDK (models-and-textures/textures/materials/flightsim-materials) says the back-side normal is not flipped by
default and the option flips it; the exporter writes {"enabled": true} only when double-sided and flip are both set.
The core glTF rule always flips, so a file that declares no ASOBO_ extension keeps it.

Fixture: one double-sided quad in the XY plane with normal +Z, drawn in Geometry Normal debug view. From +Z the camera
sees the front face, from -Z the back face. Checks, from behind:
  * the plain file shows the flipped normal (-Z, darker blue) and the MSFS file the front normal (+Z, full blue);
  * enabled true and an absent "enabled" render like the plain file; enabled false renders like the MSFS default;
  * --asobo-disable ASOBO_material_flip_back_face and --asobo off render like the plain file;
  * lit, the flip changes the picture, and NaN never reaches the frame (no magenta/black quad);
  * from the front all variants are byte-identical; a single-sided MSFS quad is culled from behind in every variant;
  * a file without ASOBO_ extensions is byte-identical to the baseline binary (--baseline-exe).

Run: python scripts/validate_asobo_flip_back_face.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import json
import struct
import sys
from pathlib import Path

from PIL import Image

from validate_asobo_common import EXE, REPO, check, finish, render, same

OUT = REPO / "build" / "asobo_flip_back_face"
FLIP = "ASOBO_material_flip_back_face"
OTHER = "ASOBO_material_draw_order"  # an unrelated extension: it is what makes the file an MSFS file here
FRAMING = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
           "--camera-target", "0,0,0", "--camera-pitch", "0", "--camera-distance", "3"]
FRONT = ["--camera-yaw", "0"]
BACK = ["--camera-yaw", "180"]
NORMALS = ["--debug-view", "Geometry Normal"]
LIT = ["--sun", "on", "--sun-intensity", "3", "--sun-dir", "0,45"]


def write_quad(name, extensions=None, declared=(), double_sided=True):
    positions = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)]
    blob = b"".join(struct.pack("<3f", *p) for p in positions)
    blob += b"".join(struct.pack("<3f", 0, 0, 1) for _ in positions)
    blob += struct.pack("<6H", 0, 1, 2, 0, 2, 3)
    material = {"name": "Quad", "doubleSided": double_sided,
                "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.8, 0.8, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.6}}
    if extensions:
        material["extensions"] = extensions
    used = sorted(set(declared) | set(extensions or {}))
    doc = {
        "asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": "Quad", "mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 0}]}],
        "materials": [material],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3", "min": [-1, -1, 0], "max": [1, 1, 0]},
            {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "count": 6, "type": "SCALAR"}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 48, "target": 34962},
            {"buffer": 0, "byteOffset": 48, "byteLength": 48, "target": 34962},
            {"buffer": 0, "byteOffset": 96, "byteLength": 12, "target": 34963}],
        "buffers": [{"byteLength": len(blob),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(blob).decode("ascii")}],
    }
    if used:
        doc["extensionsUsed"] = used
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def centre(image):
    with Image.open(image) as picture:
        rgb = picture.convert("RGB")
        return rgb.getpixel((rgb.width // 2, rgb.height // 2))


def shot(model, name, flags):
    return render(EXE, model, name, FRAMING + flags, folder=OUT, session=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", default=None)
    args = parser.parse_args()

    plain = write_quad("plain")
    msfs = write_quad("msfs", {OTHER: {"drawOrderOffset": 0}})
    flip = write_quad("flip", {FLIP: {"enabled": True}})
    flip_bare = write_quad("flip_bare", {FLIP: {}})
    flip_false = write_quad("flip_false", {FLIP: {"enabled": False}})
    single = write_quad("single", {OTHER: {"drawOrderOffset": 0}}, double_sided=False)

    print("[back face, Geometry Normal]")
    back = {name: shot(model, f"{name}_back", BACK + NORMALS) for name, model in
            (("plain", plain), ("msfs", msfs), ("flip", flip), ("flip_bare", flip_bare), ("flip_false", flip_false))}
    plain_pixel, msfs_pixel = centre(back["plain"]), centre(back["msfs"])
    print(f"  INFO centre pixels: plain {plain_pixel}, MSFS {msfs_pixel}")
    check(msfs_pixel[2] > plain_pixel[2] + 40, "the MSFS file keeps the front normal (+Z), the plain file flips it (-Z)")
    check(same(back["flip"], back["plain"]), "enabled true flips like the core rule")
    check(same(back["flip_bare"], back["plain"]), "an absent enabled flips too")
    check(same(back["flip_false"], back["msfs"]), "enabled false stays unflipped")
    check(same(shot(msfs, "msfs_back_disabled", BACK + NORMALS + ["--asobo-disable", FLIP]), back["plain"]),
          "--asobo-disable restores the core flip")
    check(same(shot(msfs, "msfs_back_off", BACK + NORMALS + ["--asobo", "off"]), back["plain"]),
          "--asobo off restores the core flip")

    print("[back face, lit]")
    lit_plain = shot(plain, "plain_lit", BACK + LIT)
    lit_msfs = shot(msfs, "msfs_lit", BACK + LIT)
    check(not same(lit_plain, lit_msfs), "lit, the unflipped back face looks different from the flipped one")
    pixel = centre(lit_msfs)
    print(f"  INFO lit centre pixels: plain {centre(lit_plain)}, MSFS {pixel}")
    check(pixel != (255, 0, 255) and sum(pixel) > 0, "the unflipped back face stays a finite, drawn colour")

    print("[front face and culling]")
    front = shot(plain, "plain_front", FRONT + NORMALS)
    check(same(shot(msfs, "msfs_front", FRONT + NORMALS), front) and same(shot(flip, "flip_front", FRONT + NORMALS), front),
          "the front face is byte-identical in every variant")
    culled = shot(single, "single_back", BACK + NORMALS)
    check(centre(culled) == (0, 0, 0), "a single-sided MSFS quad stays culled from behind")

    if args.baseline_exe:
        print("[baseline]")
        before = render(Path(args.baseline_exe).resolve(), plain, "plain_back_baseline", FRAMING + BACK + NORMALS, folder=OUT)
        check(same(before, back["plain"]), "a file without ASOBO_ extensions is byte-identical to the baseline")
    finish()


if __name__ == "__main__":
    sys.exit(main())
