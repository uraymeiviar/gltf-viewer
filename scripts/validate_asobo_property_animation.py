# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""ASOBO_property_animation playback (A7.3, docs/ASOBO_EXTENSIONS_PHASE_7.md).

Fixture: resources/test_models/asobo_property_animation/MaterialAnimationTest_LOD0.gltf, a real export of the
msfs-blender-5-addons exporter (30 frames at 30 fps, Bezier easing baked per frame): a STEP node scale channel, base
colour (VEC4, sampler 1) and emissive (VEC3, sampler 2, emissive scale 2.5) in the extension. Twins are written at test
time into build/asobo_property_animation/fixtures (never committed).

Checks, all byte-identical renders at --anim-time on a key (the times are the stored float32 numbers):
  * the clip really changes the picture (first key against last key);
  * equals the same clip written as KHR_animation_pointer channels;
  * equals static twins whose material holds that key's sampled values;
  * the emissive channel is visible at --msfs-emissive-scale 0.25 (the default 0.001 almost hides these small values);
  * --disable-extension and a file that does not declare the extension leave the file's own material;
  * malformed channels (bad sampler, non-numeric or out-of-range material, non-material target, wrong types) are skipped
    with a warning and the valid channel of the same clip still plays;
  * a synthetic textured quad, with no animation.channels and no ASOBO_material_UV_options on its material, plays UV
    offset, tiling and rotation like static twins carrying those values in the extension;
  * the validator accepts the file with no untargeted-channel warning.

Run: python scripts/validate_asobo_property_animation.py
"""

import base64
import copy
import json
import shutil
import struct
import sys
import zlib

import numpy as np

import harness_process as subprocess
from glider_harness import EXE, REPO, capture, gl_errors_zero, read_json, same

SOURCE = REPO / "resources" / "test_models" / "asobo_property_animation"
NAME = "MaterialAnimationTest_LOD0"
OUT = REPO / "build" / "asobo_property_animation"
FIXTURES = OUT / "fixtures"
FLAGS = ["--isolated", "--window-size", "960x540", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
EXT = "ASOBO_property_animation"
PTR = "KHR_animation_pointer"
UV = "ASOBO_material_UV_options"
BASE = "materials/0/pbrMetallicRoughness/baseColorFactor"
EMISSIVE = "materials/0/emissiveFactor"

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


def render(model, name, time=None, flags=()):
    # From above: the exported plate lies flat in the XZ plane. The UV quad faces this camera's side too (+Z tilted).
    view = ["--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "55", "--camera-distance", "5"]
    extra = view + list(flags) + (["--anim-time", repr(float(time))] if time is not None else [])
    png, _, log = capture(EXE, model, OUT / f"{name}.png", FLAGS + extra)
    return png, log


def write_twin(doc, name):
    """Writes a variant of the real file beside its .bin."""
    path = FIXTURES / f"{name}.gltf"
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def read_accessor(doc, index, components):
    accessor = doc["accessors"][index]
    view = doc["bufferViews"][accessor["bufferView"]]
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    data = (SOURCE / doc["buffers"][view["buffer"]]["uri"]).read_bytes()
    return np.frombuffer(data, np.float32, accessor["count"] * components, start).reshape(-1, components)


def khr_twin(doc):
    twin = copy.deepcopy(doc)
    animation = twin["animations"][0]
    for channel in animation["extensions"].pop(EXT)["channels"]:
        animation["channels"].append({"sampler": channel["sampler"], "target": {
            "path": "pointer", "extensions": {PTR: {"pointer": "/" + channel["target"]}}}})
    del animation["extensions"]
    twin["extensionsUsed"] = [PTR if name == EXT else name for name in twin["extensionsUsed"]]
    return twin


def static_twin(doc, base=None, emissive=None):
    """No material animation: the node clip stays, the material holds the given values (or the file's own)."""
    twin = copy.deepcopy(doc)
    del twin["animations"][0]["extensions"]
    twin["extensionsUsed"].remove(EXT)
    if base is not None:
        twin["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"] = [float(v) for v in base]
    if emissive is not None:
        twin["materials"][0]["emissiveFactor"] = [float(v) for v in emissive]
    return twin


def real_file_checks():
    doc = json.loads((SOURCE / f"{NAME}.gltf").read_text(encoding="utf-8"))
    for bin_file in SOURCE.glob("*.bin"):
        shutil.copy2(bin_file, FIXTURES / bin_file.name)
    model = FIXTURES / f"{NAME}.gltf"
    model.write_text(json.dumps(doc), encoding="utf-8")

    channels = {c["target"]: c["sampler"] for c in doc["animations"][0]["extensions"][EXT]["channels"]}
    samplers = doc["animations"][0]["samplers"]
    times = read_accessor(doc, samplers[channels[BASE]]["input"], 1)[:, 0]
    bases = read_accessor(doc, samplers[channels[BASE]]["output"], 4)
    emissives = read_accessor(doc, samplers[channels[EMISSIVE]]["output"], 3)
    check(len(times) == 30 and float(emissives.max()) > 1.0,
          f"the fixture is the 30-key export with emissive above 1 (max {float(emissives.max()):.3f})")

    print("[playback]")
    khr = write_twin(khr_twin(doc), "khr_twin")
    rendered = {}
    for key in (0, 14, 29):
        png, log = render(model, f"asobo_key{key}", times[key])
        rendered[key] = png
        check(png is not None and gl_errors_zero(log), f"key {key}: renders with no GL errors")
        twin_png, _ = render(khr, f"khr_key{key}", times[key])
        check(same(png, twin_png), f"key {key} (t = {times[key]:.4f}): equals the KHR_animation_pointer twin")
        static = write_twin(static_twin(doc, bases[key], emissives[key]), f"static_key{key}")
        static_png, _ = render(static, f"static_key{key}", times[key])
        check(same(png, static_png), f"key {key}: equals the static twin holding base {np.round(bases[key], 3).tolist()} "
                                     f"and emissive {np.round(emissives[key], 3).tolist()}")
    check(not same(rendered[0], rendered[29]), "the first and last keys render differently")

    print("[emissive, visible only above the default MSFS emissive scale of 0.001]")
    scale = ["--msfs-emissive-scale", "0.25"]
    played, _ = render(model, "emissive_played", times[29], scale)
    lit = write_twin(static_twin(doc, bases[29], emissives[29]), "emissive_lit")
    lit_png, _ = render(lit, "emissive_lit", times[29], scale)
    dark = write_twin(static_twin(doc, bases[29], [0.0, 0.0, 0.0]), "emissive_dark")
    dark_png, _ = render(dark, "emissive_dark", times[29], scale)
    check(same(played, lit_png), "the played emissive equals the static twin at a visible scale")
    check(not same(played, dark_png), "the emissive channel changes the picture")

    print("[switch off and undeclared]")
    own = write_twin(static_twin(doc), "own_material")
    own_png, _ = render(own, "own_material", times[29])
    check(not same(own_png, rendered[29]), "the file's own material differs from the last key")
    off, _ = render(model, "switched_off", times[29], ["--disable-extension", EXT])
    check(same(off, own_png), "--disable-extension leaves the file's own material")
    undeclared_doc = copy.deepcopy(doc)
    undeclared_doc["extensionsUsed"].remove(EXT)
    undeclared, _ = render(write_twin(undeclared_doc, "undeclared"), "undeclared", times[29])
    check(same(undeclared, own_png), "a file that does not declare the extension: channels ignored")

    print("[malformed channels]")
    bad_doc = copy.deepcopy(doc)
    bad_doc["animations"][0]["extensions"][EXT]["channels"] = [
        {"sampler": 99, "target": EMISSIVE},
        {"sampler": -1, "target": EMISSIVE},
        {"sampler": 2, "target": "materials/x/emissiveFactor"},
        {"sampler": 2, "target": "materials/99/emissiveFactor"},
        {"sampler": 2, "target": "materials/0"},
        {"sampler": 2, "target": "nodes/0/scale"},
        {"sampler": 2, "target": 5},
        "not an object",
        {"sampler": channels[BASE], "target": BASE},
    ]
    bad_png, bad_log = render(write_twin(bad_doc, "malformed"), "malformed", times[29])
    expected = write_twin(static_twin(doc, bases[29]), "malformed_expected")
    expected_png, _ = render(expected, "malformed_expected", times[29])
    check(same(bad_png, expected_png), "the valid channel still plays; every malformed one is skipped")
    check(bad_log.count("ASOBO_property_animation: channel") == 8, "each malformed channel logged a warning")

    print("[validator]")
    report = OUT / "validation.json"
    report.unlink(missing_ok=True)
    subprocess.run([str(EXE), "--isolated", "--validate-file", str(model), "--validation-report", str(report)],
                   cwd=REPO, capture_output=True, text=True, timeout=600)
    data = read_json(report)
    codes = [issue.get("code") for issue in (data or {}).get("issues", [])]
    check(data is not None and "ANIMATION_CHANNEL_UNTARGETED" not in codes, "no untargeted-channel warning")


def checker_png():
    size = 8
    rows = b""
    for y in range(size):
        rows += b"\x00" + bytes(c for x in range(size) for c in
                                ((230, 60, 40) if (x // 2 + y // 2) % 2 else (40, 90, 220)))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


UV_TIMES = [0.0, 0.5, 1.0]
UV_KEYS = {"UVOffsetU": [0.0, 0.25, 0.5], "UVOffsetV": [0.0, 0.125, 0.375], "UVTilingU": [1.0, 2.0, 3.0],
           "UVTilingV": [1.0, 1.5, 0.5], "UVRotation": [0.0, 30.0, 90.0]}


def uv_doc(animated, values=None):
    """A textured quad. animated: every UV component on its own channel, no animation.channels, no UV extension."""
    data = bytearray()
    views, accessors = [], []

    def add(payload, component, count, kind, minmax=None, target=None):
        while len(data) % 4:
            data.append(0)
        view = {"buffer": 0, "byteOffset": len(data), "byteLength": len(payload)}
        if target:
            view["target"] = target
        data.extend(payload)
        views.append(view)
        accessor = {"bufferView": len(views) - 1, "componentType": component, "count": count, "type": kind}
        if minmax:
            accessor["min"], accessor["max"] = minmax
        accessors.append(accessor)
        return len(accessors) - 1

    def floats(values):
        return np.array(values, dtype=np.float32).tobytes()

    position = add(floats([[-1, -1, 0], [1, -1, 0], [1, 1, 0], [-1, 1, 0]]), 5126, 4, "VEC3",
                   ([-1, -1, 0], [1, 1, 0]), 34962)
    normal = add(floats([[0, 0, 1]] * 4), 5126, 4, "VEC3", None, 34962)
    uv = add(floats([[0, 1], [1, 1], [1, 0], [0, 0]]), 5126, 4, "VEC2", None, 34962)
    index = add(np.array([0, 1, 2, 0, 2, 3], dtype=np.uint16).tobytes(), 5123, 6, "SCALAR", None, 34963)

    material = {"name": "uv", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0.0,
                                                        "roughnessFactor": 0.8}}
    used = []
    doc = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
           "nodes": [{"mesh": 0, "name": "quad"}],
           "meshes": [{"primitives": [{"attributes": {"POSITION": position, "NORMAL": normal, "TEXCOORD_0": uv},
                                       "indices": index, "material": 0}]}],
           "materials": [material], "textures": [{"source": 0}],
           "images": [{"uri": "data:image/png;base64," + base64.b64encode(checker_png()).decode()}]}
    if animated:
        times = add(floats(UV_TIMES), 5126, 3, "SCALAR", ([UV_TIMES[0]], [UV_TIMES[-1]]))
        samplers, channels = [], []
        for key, keys in UV_KEYS.items():
            samplers.append({"input": times, "output": add(floats(keys), 5126, 3, "SCALAR"), "interpolation": "LINEAR"})
            channels.append({"sampler": len(samplers) - 1, "target": f"materials/0/extensions/{UV}/{key}"})
        doc["animations"] = [{"name": "uv", "samplers": samplers, "extensions": {EXT: {"channels": channels}}}]
        used.append(EXT)
    else:
        material["extensions"] = {UV: values}
        used.append(UV)
    doc["extensionsUsed"] = used
    doc["bufferViews"] = views
    doc["accessors"] = accessors
    doc["buffers"] = [{"byteLength": len(data),
                       "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(data)).decode()}]
    return doc


def uv_checks():
    print("[UV options, material without the extension, clip without animation.channels]")
    animated = write_twin(uv_doc(True), "uv_animated")
    pictures = []
    for k, time in enumerate(UV_TIMES):
        values = {key: keys[k] for key, keys in UV_KEYS.items()}
        png, log = render(animated, f"uv_key{k}", time)
        check(png is not None and gl_errors_zero(log), f"UV key {k}: renders with no GL errors")
        static, _ = render(write_twin(uv_doc(False, values), f"uv_static{k}"), f"uv_static{k}")
        check(same(png, static), f"UV key {k}: equals the static {UV} twin {values}")
        pictures.append(png)
    check(not same(pictures[0], pictures[2]), "the UV clip changes the picture")


def main():
    FIXTURES.mkdir(parents=True, exist_ok=True)
    print(f"[fixtures] candidate {EXE}")
    real_file_checks()
    uv_checks()


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        check(False, repr(error))
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)
