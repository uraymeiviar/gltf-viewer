# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""KHR_materials_dispersion, KHR_node_selectability / KHR_node_hoverability and the 3D Tiles / Cesium extensions
(EXT_structural_metadata, EXT_mesh_features, EXT_instance_features, CESIUM_primitive_outline, CESIUM_RTC).

Fixtures are written at test time into build/trivial_extensions/fixtures (small .gltf files with data-URI buffers,
never committed); nothing needs the network. When the Khronos samples DispersionTest.glb / DragonDispersion.glb are in
build/khronos-samples they are rendered too (INFO / skip otherwise).

Dispersion ("disp_*"): a thick glass cube (transmission 1, volume thickness 1, IOR 1.5, roughness 0) turned 25 degrees
about Y in front of black and white unlit vertical bars. Twins: none (no extension), zero (dispersion 0), on
(dispersion 10), lo / hi (no dispersion, KHR_materials_ior = the red / blue channel IOR of "on":
ior -/+ (ior - 1) * 0.025 * dispersion), nofx (dispersion 10 without volume: no effect), anim (dispersion animated
0 -> 10 through KHR_animation_pointer, STEP at 1 s). Checks (byte for byte unless stated):
  * zero = none; --disable-extension KHR_materials_dispersion on "on" = none; nofx = its twin without the extension;
  * on differs from none, only inside the glass; its green channel equals none's green channel there (green keeps
    the IOR); the bar edges of the red and blue channels are shifted against green in opposite directions (colour
    fringes), the red shift in the direction lo shifts green against none, the blue shift in the direction hi does
    (red bends less, blue more), each within 2 px of that shift;
  * anim at 0.5 s = zero, at 1.5 s = on; pipe: set_material_param dispersion 10 on the zero file = the on file
    (pipe captures), get_material_param reads it back;
  * validator: no extension notice; the baseline renders on like none (it ignores the extension).

Selectability ("sel_*"): a back plate, a non-selectable Blocker in front of it, a non-selectable ParentNS whose child
ChildInherit (no extension) and ChildExplicit (selectable = true) inherit it, a non-selectable Lone with nothing behind,
and a small selectable Anchor that keeps the scene bounds of every twin equal. ParentNS also carries
KHR_node_hoverability hoverable = false, Blocker hoverable = true. Twins: noext (no extensions), removed (the
non-selectable nodes removed). Checks:
  * renders: sel = noext (selectability changes no pixel);
  * pipe pick at each non-selectable node's centre: Back (Lone: nothing) = the removed twin, noext: the node itself;
    the --disable-extension run picks the node like noext;
  * window clicks (--ui-script) over a grid: the selection equals the removed twin's at every click, and differs from
    noext's where noext hits a non-selectable node;
  * pipe select (the hierarchy) still selects Blocker and ChildInherit;
  * animated: Blocker's selectable animated 1 -> 0 (STEP at 1 s): pick at 0.5 s = Blocker, at 1.5 s = Back;
  * list_nodes: selectable / hoverable values; validator: no notice, *_INVALID_TYPE for a non-boolean value.
Hoverability: Glider has no viewport hover highlight; it is read and listed only (list_nodes, Inspector).

3D Tiles ("tiles_*"): one file using all five extensions (CESIUM_RTC center at ECEF-like 6.4e6 m, an
EXT_structural_metadata schema + property table, EXT_mesh_features attribute and implicit feature ID sets,
CESIUM_primitive_outline, EXT_instance_features on an EXT_mesh_gpu_instancing node); its plain twin is the same JSON
with every one of these extension objects and names removed (the rule for CESIUM_RTC: the scene is rendered relative
to the center, so it renders like the file without it). Checks: tiles = plain, required (all five in
extensionsRequired) = plain, each switched off = plain; validator: no extension notice; get_state.tiles3d lists the
center, schema, property table, feature ID counts, outline and instance features; switched off: empty.

Run: python scripts/validate_trivial_extensions.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import json
import math
import sys
import time

import numpy as np
from PIL import Image

from glider_harness import (EXE, REPO, baseline_exe, capture, capture_run, gl_errors_zero, read_json, run, same,
                            start_pipe_session)
from validate_gpu_instancing import Doc, cube_mesh

OUT = REPO / "build" / "trivial_extensions"
FIXTURES = OUT / "fixtures"
SAMPLES = REPO / "build" / "khronos-samples"
FLAGS = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
DISP = "KHR_materials_dispersion"
SEL = "KHR_node_selectability"
HOV = "KHR_node_hoverability"
TILES = ["EXT_structural_metadata", "EXT_mesh_features", "EXT_instance_features", "CESIUM_primitive_outline",
         "CESIUM_RTC"]
IOR = 1.5
DISPERSION = 10.0
HALF_SPREAD = (IOR - 1.0) * 0.025 * DISPERSION

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


def write(path, document):
    path.write_text(json.dumps(document), encoding="utf-8")
    return path


def finish(document, doc):
    document["buffers"], document["bufferViews"], document["accessors"] = doc.buffers(), doc.views, doc.accessors
    return document


def quat_y(degrees):
    half = math.radians(degrees) / 2.0
    return [0.0, math.sin(half), 0.0, math.cos(half)]


# --------------------------------------------------------------------------------------------- dispersion fixtures

def write_dispersion(name, mode):
    """mode: none, zero, on, lo, hi, nofx, nofx_plain, anim."""
    doc = Doc()
    glass = {"name": "glass", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0.0,
                                                        "roughnessFactor": 0.0},
             "extensions": {"KHR_materials_transmission": {"transmissionFactor": 1.0}}}
    used = ["KHR_materials_unlit", "KHR_materials_transmission"]
    if mode != "nofx" and mode != "nofx_plain":
        glass["extensions"]["KHR_materials_volume"] = {"thicknessFactor": 3.0}
        used.append("KHR_materials_volume")
    if mode in ("zero", "on", "nofx", "anim"):
        glass["extensions"][DISP] = {"dispersion": 0.0 if mode in ("zero", "anim") else DISPERSION}
        used.append(DISP)
    if mode in ("lo", "hi"):
        glass["extensions"]["KHR_materials_ior"] = {"ior": IOR - HALF_SPREAD if mode == "lo" else IOR + HALF_SPREAD}
        used.append("KHR_materials_ior")
    materials = [
        {"name": "white", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1]}, "extensions": {"KHR_materials_unlit": {}}},
        {"name": "black", "pbrMetallicRoughness": {"baseColorFactor": [0, 0, 0, 1]}, "extensions": {"KHR_materials_unlit": {}}},
        glass,
    ]
    meshes = [cube_mesh(doc, 0), cube_mesh(doc, 1), cube_mesh(doc, 2)]
    nodes = []
    for i in range(16):
        nodes.append({"name": f"Bar{i}", "mesh": i % 2, "translation": [-4.0 + 0.5 * i + 0.25, 0.0, -3.0],
                      "scale": [0.5, 6.0, 0.1]})
    nodes.append({"name": "Glass", "mesh": 2, "rotation": quat_y(25.0), "scale": [2.5, 2.5, 2.5]})
    document = {"asset": {"version": "2.0", "generator": "validate_trivial_extensions.py"}, "scene": 0,
                "scenes": [{"nodes": list(range(len(nodes)))}], "nodes": nodes, "materials": materials,
                "meshes": meshes, "extensionsUsed": used}
    if mode == "anim":
        used.append("KHR_animation_pointer")
        times = doc.accessor(np.array([0.0, 1.0, 2.0], dtype=np.float32).tobytes(), 5126, 3, "SCALAR",
                             minmax=([0.0], [2.0]))
        values = doc.accessor(np.array([0.0, DISPERSION, DISPERSION], dtype=np.float32).tobytes(), 5126, 3, "SCALAR")
        document["animations"] = [{"name": "Disperse", "samplers": [{"input": times, "output": values,
                                                                      "interpolation": "STEP"}],
                                   "channels": [{"sampler": 0, "target": {"path": "pointer", "extensions": {
                                       "KHR_animation_pointer": {"pointer": f"/materials/2/extensions/{DISP}/dispersion"}}}}]}]
    return write(FIXTURES / f"disp_{name}.gltf", finish(document, doc))


# --------------------------------------------------------------------------------------------- selectability fixtures

# (name, properties, selectable or None, hoverable or None, children)
SEL_SCENE = [
    ("Back", {"mesh": 0, "translation": [1.0, 0.0, -2.0], "scale": [6.0, 5.0, 0.3]}, None, None, []),
    ("Anchor", {"mesh": 1, "translation": [-3.9, 2.4, 1.0], "scale": [0.2, 0.2, 0.2]}, None, None, []),
    ("Blocker", {"mesh": 1, "translation": [-1.0, -1.0, 0.0]}, False, True, []),
    ("ParentNS", {"mesh": 1, "translation": [1.0, 1.2, 0.0], "scale": [0.8, 0.8, 0.8]}, False, False, [
        ("ChildInherit", {"mesh": 2, "translation": [-2.0, 0.0, 0.0]}, None, None, []),
        ("ChildExplicit", {"mesh": 2, "translation": [2.0, 0.0, 0.0]}, True, None, []),
    ]),
    ("Lone", {"mesh": 2, "translation": [-3.2, -1.6, 0.0], "scale": [0.6, 0.6, 0.6]}, False, None, []),
]
NON_SELECTABLE = ("Blocker", "ParentNS", "ChildInherit", "ChildExplicit", "Lone")
BEHIND = {"Blocker": "Back", "ParentNS": "Back", "ChildInherit": "Back", "ChildExplicit": "Back", "Lone": None}
SUBTREE = {"Blocker": ("Blocker",), "ParentNS": ("ParentNS", "ChildInherit", "ChildExplicit"),
           "ChildInherit": ("ChildInherit",), "ChildExplicit": ("ChildExplicit",), "Lone": ("Lone",)}


def pick_node(session, x, y):
    return (session.call("pick", x=x, y=y) or {}).get("node") or None


def write_selectability(name, mode):
    """mode: sel, noext, removed, required, badtype, anim (Blocker selectable 1 -> 0 at 1 s)."""
    doc = Doc()
    materials = [{"name": "back", "pbrMetallicRoughness": {"baseColorFactor": [0.25, 0.6, 0.3, 1.0], "metallicFactor": 0.0}},
                 {"name": "front", "pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.45, 0.12, 1.0], "metallicFactor": 0.1}},
                 {"name": "child", "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.4, 0.9, 1.0], "metallicFactor": 0.0}}]
    with_ext = mode in ("sel", "required", "badtype", "anim")
    nodes = []

    def add(entry, parent_removed=False):
        node_name, props, selectable, hoverable, children = entry
        removed = parent_removed or (mode == "removed" and selectable is False)
        index = None
        if not removed:
            node = {"name": node_name, **props}
            if with_ext and selectable is not None:
                node.setdefault("extensions", {})[SEL] = {"selectable": selectable}
                if mode == "badtype" and node_name == "Blocker":
                    node["extensions"][SEL] = {"selectable": 0}
            if with_ext and hoverable is not None:
                node.setdefault("extensions", {})[HOV] = {"hoverable": hoverable}
            if mode == "anim" and node_name == "Blocker":
                node["extensions"][SEL] = {"selectable": True}
            index = len(nodes)
            nodes.append(node)
        kids = [k for k in (add(child, removed) for child in children) if k is not None]
        if kids and index is not None:
            nodes[index]["children"] = kids
        return index

    roots = [r for r in (add(entry) for entry in SEL_SCENE) if r is not None]
    document = {"asset": {"version": "2.0", "generator": "validate_trivial_extensions.py"}, "scene": 0,
                "scenes": [{"nodes": roots}], "nodes": nodes, "materials": materials,
                "meshes": [cube_mesh(doc, 0), cube_mesh(doc, 1), cube_mesh(doc, 2)]}
    if with_ext:
        document["extensionsUsed"] = [SEL, HOV]
    if mode == "required":
        document["extensionsRequired"] = [SEL]
    if mode == "anim":
        document["extensionsUsed"].append("KHR_animation_pointer")
        blocker = next(i for i, n in enumerate(nodes) if n["name"] == "Blocker")
        times = doc.accessor(np.array([0.0, 1.0, 2.0], dtype=np.float32).tobytes(), 5126, 3, "SCALAR",
                             minmax=([0.0], [2.0]))
        values = doc.accessor(np.array([1, 0, 0], dtype=np.uint8).tobytes(), 5121, 3, "SCALAR")
        document["animations"] = [{"name": "Lock", "samplers": [{"input": times, "output": values,
                                                                 "interpolation": "STEP"}],
                                   "channels": [{"sampler": 0, "target": {"path": "pointer", "extensions": {
                                       "KHR_animation_pointer": {"pointer": f"/nodes/{blocker}/extensions/{SEL}/selectable"}}}}]}]
    return write(FIXTURES / f"sel_{name}.gltf", finish(document, doc))


# --------------------------------------------------------------------------------------------- 3D Tiles fixtures

RTC_CENTER = [1215012.5, -4736313.25, 4081605.75]


def write_tiles(name, mode, keep=()):
    """mode: tiles (all five), plain (none), required (all five required), noattr (plain without _FEATURE_ID_0).
    keep: for "plain", the extensions kept (none by default)."""
    doc = Doc()
    use = set(TILES) if mode in ("tiles", "required") else set(keep)
    mesh = cube_mesh(doc, 0)
    primitive = mesh["primitives"][0]
    if mode != "noattr":
        feature = np.repeat(np.arange(6, dtype=np.float32), 4)  # one feature per cube face
        primitive["attributes"]["_FEATURE_ID_0"] = doc.accessor(feature.tobytes(), 5126, 24, "SCALAR", target=34962)
    ext = {}
    if "EXT_mesh_features" in use:
        ext["EXT_mesh_features"] = {"featureIds": [{"featureCount": 6, "attribute": 0, "propertyTable": 0,
                                                    "label": "faces"},
                                                   {"featureCount": 24, "nullFeatureId": 99}]}
    if "CESIUM_primitive_outline" in use:
        edges = np.array([0, 1, 1, 2, 2, 3, 3, 0], dtype=np.uint16)
        ext["CESIUM_primitive_outline"] = {"indices": doc.accessor(edges.tobytes(), 5123, 8, "SCALAR", target=34963)}
    if "EXT_structural_metadata" in use:
        ext["EXT_structural_metadata"] = {"propertyAttributes": [0]}
    if ext:
        primitive["extensions"] = ext
    heights = doc.view(np.array([3.0, 4.5, 6.0, 7.5, 9.0, 10.5], dtype=np.float32).tobytes())
    instances = doc.accessor(np.array([[-1.5, 0, 0], [0, 0, 0], [1.5, 0, 0]], dtype=np.float32).tobytes(), 5126, 3, "VEC3")
    nodes = [{"name": "Building", "mesh": 0},
             {"name": "Row", "mesh": 0, "translation": [0.0, -1.6, 0.0], "scale": [0.5, 0.5, 0.5],
              "extensions": {"EXT_mesh_gpu_instancing": {"attributes": {"TRANSLATION": instances}}}}]
    if "EXT_instance_features" in use:
        ids = doc.accessor(np.array([0, 1, 2], dtype=np.float32).tobytes(), 5126, 3, "SCALAR")
        nodes[1]["extensions"]["EXT_mesh_gpu_instancing"]["attributes"]["_FEATURE_ID_0"] = ids
        nodes[1]["extensions"]["EXT_instance_features"] = {"featureIds": [{"featureCount": 3, "attribute": 0}]}
    document = {"asset": {"version": "2.0", "generator": "validate_trivial_extensions.py"}, "scene": 0,
                "scenes": [{"nodes": [0, 1]}], "nodes": nodes,
                "materials": [{"name": "facade", "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.7, 0.55, 1.0],
                                                                           "metallicFactor": 0.0}}],
                "meshes": [mesh], "extensionsUsed": ["EXT_mesh_gpu_instancing"]}
    root = {}
    if "CESIUM_RTC" in use:
        root["CESIUM_RTC"] = {"center": RTC_CENTER}
    if "EXT_structural_metadata" in use:
        root["EXT_structural_metadata"] = {
            "schema": {"id": "city", "name": "City", "classes": {
                "building": {"properties": {"height": {"type": "SCALAR", "componentType": "FLOAT32"}}},
                "facade": {"properties": {"material": {"type": "ENUM", "enumType": "facadeType"}}}},
                "enums": {"facadeType": {"values": [{"name": "brick", "value": 0}, {"name": "glass", "value": 1}]}}},
            "propertyTables": [{"name": "buildings", "class": "building", "count": 6,
                                "properties": {"height": {"values": heights}}}],
            "propertyAttributes": [{"class": "facade", "properties": {"material": {"attribute": "_FEATURE_ID_0"}}}]}
    if root:
        document["extensions"] = root
    document["extensionsUsed"] += sorted(use)
    if mode == "required":
        document["extensionsRequired"] = sorted(use)
    return write(FIXTURES / f"tiles_{name}.gltf", finish(document, doc))


def write_fixtures():
    FIXTURES.mkdir(parents=True, exist_ok=True)
    files = {}
    for mode in ("none", "zero", "on", "lo", "hi", "nofx", "nofx_plain", "anim"):
        files[f"disp_{mode}"] = write_dispersion(mode, mode)
    for mode in ("sel", "noext", "removed", "required", "badtype", "anim"):
        files[f"sel_{mode}"] = write_selectability(mode, mode)
    for mode in ("tiles", "plain", "required", "noattr"):
        files[f"tiles_{mode}"] = write_tiles(mode, mode)
    return files


# --------------------------------------------------------------------------------------------- Glider runs

def render(exe, model, name, flags=(), report=True):
    png = OUT / f"{name}.png"
    if report:
        return capture(exe, model, png, FLAGS + list(flags))
    png.unlink(missing_ok=True)
    result = capture_run([exe, model, *FLAGS, *flags, "--capture-viewport", png, "--capture-frame", "24",
                          "--profile-report", OUT / f"{name}.json", "--exit-after-profile"])
    return (png if png.exists() else None), None, result.stdout + result.stderr


def validate(model, name, flags=()):
    report = OUT / f"{name}.validation.json"
    report.unlink(missing_ok=True)
    run([EXE, "--isolated", *flags, "--validate-file", model, "--validation-report", report])
    data = read_json(report)
    return None if data is None else data.get("issues", [])


def notices(issues):
    return [i.get("code") for i in issues or [] if str(i.get("code", "")).startswith("EXTENSION_")
            or i.get("severity") == "Error"]


class Session:
    def __init__(self, model, flags=()):
        self.process, self.pipe = start_pipe_session([EXE, model, *FLAGS, *flags, "--control-pipe"])
        # The model loads in the background: wait for it, then render a few frames, before any pick or probe.
        deadline = time.time() + 120
        while time.time() < deadline:
            opened = self.call("get_state")["gltfExtensions"]["openFile"]
            if opened.get("loaded") and not opened.get("loadPending"):
                break
            time.sleep(0.2)
        self.call("capture", path=str(OUT / "session_settle.png"))

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def close(self):
        errors = None
        try:
            errors = self.call("get_state")["glErrors"]
            self.call("quit")
        finally:
            self.pipe.close()
            try:
                self.process.wait(60)
            except Exception:
                self.process.kill()
        return errors


def load(png):
    return np.asarray(Image.open(png).convert("RGB")).astype(np.int32)


def row_shift(a, b, limit=24):
    """Integer shift s minimising mean |a[x] - b[x + s]| (b moved by s pixels matches a)."""
    best, best_s = None, 0
    n = len(a)
    for s in range(-limit, limit + 1):
        lo, hi = max(0, -s), min(n, n - s)
        if hi - lo < n // 2:
            continue
        cost = np.mean(np.abs(a[lo:hi] - b[lo + s:hi + s]))
        if best is None or cost < best:
            best, best_s = cost, s
    return best_s


def band_shift(img_a, ca, img_b, cb, rows, cols):
    shifts = [row_shift(img_a[y, cols, ca].astype(float), img_b[y, cols, cb].astype(float)) for y in rows]
    return float(np.median(shifts))


# --------------------------------------------------------------------------------------------- checks

def fringe_checks(files, diff, none, on, lo, hi):
    # The glass: where any of the IOR twins differs from none (the refracted bars move).
    glass = (np.abs(lo - none).max(axis=2) > 0) | (np.abs(hi - none).max(axis=2) > 0) | diff
    gy, gx = np.nonzero(glass)
    check(diff[~glass].sum() == 0, "dispersion changes pixels only inside the glass")
    cy = int(np.median(gy))
    # Green keeps the material IOR: in the linear HDR scene colour (before tone mapping, which mixes the channels)
    # the green of "on" equals the green of "none" everywhere in the glass, while red and blue change.
    probes = {}
    xs_probe = list(range(int(gx.min()) + 2, int(gx.max()) - 2, 3))
    for mode in ("on", "none"):
        session = Session(files[f"disp_{mode}"])
        try:
            probes[mode] = [session.call("probe", x=x, y=cy)["hdr"] for x in xs_probe]
            session.close()
        except Exception:
            session.process.kill()
            raise
    pairs = [(a, b) for a, b, x in zip(probes["on"], probes["none"], xs_probe) if glass[cy, x]]
    green_equal = all(abs(a[1] - b[1]) <= 1e-6 for a, b in pairs)
    rb_changed = sum(1 for a, b in pairs if abs(a[0] - b[0]) > 1e-3 or abs(a[2] - b[2]) > 1e-3)
    check(len(pairs) > 20 and green_equal and rb_changed > len(pairs) // 10,
          f"green keeps the material IOR: linear HDR green of dispersion 10 = without dispersion at {len(pairs)} "
          f"glass pixels of row {cy}; red / blue differ at {rb_changed}")
    x0, x1 = int(np.percentile(gx, 15)), int(np.percentile(gx, 55))
    rows = range(cy - 40, cy + 41, 4)
    cols = slice(x0, x1)
    s_r = band_shift(on, 1, on, 0, rows, cols)   # red moved against green
    s_b = band_shift(on, 1, on, 2, rows, cols)   # blue moved against green
    s_lo = band_shift(none, 1, lo, 1, rows, cols)  # green with the red IOR against green with the IOR
    s_hi = band_shift(none, 1, hi, 1, rows, cols)
    print(f"  INFO bar-edge shifts (px, columns {x0}..{x1}, rows {cy - 40}..{cy + 40}): red {s_r}, blue {s_b}, "
          f"ior lo {s_lo}, ior hi {s_hi}")
    check(s_r != 0 and s_b != 0 and np.sign(s_r) == -np.sign(s_b),
          f"colour fringes: red and blue bar edges shift against green in opposite directions ({s_r} / {s_b} px)")
    check(np.sign(s_r) == np.sign(s_lo) and abs(s_r - s_lo) <= 2 and np.sign(s_b) == np.sign(s_hi)
          and abs(s_b - s_hi) <= 2,
          f"direction and size follow halfSpread = (ior - 1) * 0.025 * dispersion: red like IOR {IOR - HALF_SPREAD} "
          f"({s_r} vs {s_lo} px), blue like IOR {IOR + HALF_SPREAD} ({s_b} vs {s_hi} px)")


def dispersion_checks(files, baseline):
    print("[KHR_materials_dispersion: renders]")
    img = {}
    for mode in ("none", "zero", "on", "lo", "hi", "nofx", "nofx_plain"):
        img[mode], _, log = render(EXE, files[f"disp_{mode}"], f"disp_{mode}", report=False)
        check(img[mode] is not None and gl_errors_zero(log), f"disp_{mode}: captured, zero GL errors")
    check(same(img["zero"], img["none"]), "dispersion 0: byte-identical to the material without the extension")
    off, _, log = render(EXE, files["disp_on"], "disp_on_switched_off", ["--disable-extension", DISP])
    check(same(off, img["none"]) and gl_errors_zero(log),
          "--disable-extension KHR_materials_dispersion: byte-identical to the material without the extension")
    check(same(img["nofx"], img["nofx_plain"]), "dispersion 10 without KHR_materials_volume: no effect (byte-identical)")
    if not all(img[m] is not None for m in ("none", "on", "lo", "hi")):
        check(False, "dispersion images missing")
        return
    none, on, lo, hi = (load(img[m]) for m in ("none", "on", "lo", "hi"))
    diff = np.abs(on - none).max(axis=2) > 0
    check(diff.any(), f"dispersion 10: differs from the material without the extension ({int(diff.sum())} pixels)")
    if diff.any():
        fringe_checks(files, diff, none, on, lo, hi)

    print("[KHR_materials_dispersion: animation pointer, pipe edit, validator]")
    a, _, log = render(EXE, files["disp_anim"], "disp_anim_0.5", ["--anim-time", "0.5"])
    check(same(a, img["zero"]) and gl_errors_zero(log), "anim at 0.5 s (dispersion 0): byte-identical to dispersion 0")
    b, _, log = render(EXE, files["disp_anim"], "disp_anim_1.5", ["--anim-time", "1.5"])
    check(same(b, img["on"]) and gl_errors_zero(log), "anim at 1.5 s (dispersion 10): byte-identical to dispersion 10")
    shots = {}
    for mode in ("on", "zero"):
        session = Session(files[f"disp_{mode}"])
        try:
            if mode == "zero":
                reply = session.call("set_material_param", material="glass", extension=DISP, param="dispersion",
                                     value=DISPERSION)
                check(reply.get("value") == DISPERSION or reply.get("values", {}).get("dispersion") == DISPERSION,
                      f"set_material_param {DISP}.dispersion = {DISPERSION}, read back ({reply})")
            shots[mode] = session.call("capture", path=str(OUT / f"disp_pipe_{mode}.png"))["path"]
            errors = session.close()
        except Exception:
            session.process.kill()
            raise
        check(errors == 0, f"disp_{mode} pipe session: zero GL errors ({errors})")
    check(same(shots.get("on"), shots.get("zero")),
          "dispersion edited 0 -> 10 at runtime renders byte-identical to the file with 10 (pipe captures)")
    issues = validate(files["disp_on"], "disp_on")
    check(issues is not None and not notices(issues), f"disp_on: no validation error, no extension notice ({notices(issues)})")
    if baseline is not None:
        base, _, _ = render(baseline, files["disp_on"], "base_disp_on")
        base_none, _, _ = render(baseline, files["disp_none"], "base_disp_none")
        check(same(base_none, img["none"]), f"disp_none: byte-identical to the baseline {baseline.name}")
        # A baseline from before e6fe72a has no dispersion (it renders dispersion 10 like none); once the baseline
        # disperses, dispersion 10 must stay byte-identical to it.
        if same(base, img["none"]):
            print(f"  INFO the baseline {baseline.name} has no dispersion (renders dispersion 10 like none)")
        else:
            check(same(base, img["on"]), f"disp_on: byte-identical to the baseline {baseline.name}")

    print("[Khronos samples]")
    for sample in ("DispersionTest", "DragonDispersion"):
        path = SAMPLES / f"{sample}.glb"
        if not path.exists():
            print(f"  INFO {path} not present (download it from KhronosGroup/glTF-Sample-Assets): skipped")
            continue
        image, _, log = render(EXE, path, f"sample_{sample}", report=False)
        off, _, _ = render(EXE, path, f"sample_{sample}_off", ["--disable-extension", DISP], report=False)
        check(image is not None and gl_errors_zero(log) and off is not None and not same(image, off),
              f"{sample}: captured, zero GL errors, differs with the extension switched off")


def selectability_checks(files, baseline):
    print("[KHR_node_selectability: renders]")
    img = {}
    for mode in ("sel", "noext", "removed"):
        img[mode], _, log = render(EXE, files[f"sel_{mode}"], f"sel_{mode}", report=False)
        check(img[mode] is not None and gl_errors_zero(log), f"sel_{mode}: captured, zero GL errors")
    check(same(img["sel"], img["noext"]), "non-selectable nodes render byte-identical to the file without the extension")
    check(not same(img["sel"], img["removed"]), "the removed twin differs (the non-selectable nodes are drawn)")
    image, _, _ = render(EXE, files["sel_required"], "sel_required", report=False)
    check(same(image, img["noext"]), "required: loads, renders like the file without the extension")

    print("[KHR_node_selectability: pipe pick, hierarchy select, list_nodes]")
    results = {}
    for mode, flags in (("sel", ()), ("removed", ()), ("noext", ()), ("off", ("--disable-extension", SEL))):
        model = files["sel_sel"] if mode == "off" else files[f"sel_{mode}"]
        session = Session(model, flags)
        try:
            data = {"camera": session.call("get_camera"), "picks": {}}
            if mode == "noext":
                centres = {}
                for name in NON_SELECTABLE:
                    rect = (session.call("node_screen_rect", node=name) or {}).get("screenRect")
                    if rect:
                        centres[name] = (int((rect[0] + rect[2]) / 2), int((rect[1] + rect[3]) / 2))
                results["centres"] = centres
            results[mode] = data
            if mode == "sel":
                data["nodes"] = session.call("list_nodes")["nodes"]
                session.call("select", node="Blocker")
                data["select_blocker"] = session.call("get_state")["selection"]
                session.call("select", node="ChildInherit")
                data["select_child"] = session.call("get_state")["selection"]
                data["state"] = session.call("get_state")
            errors = session.close()
        except Exception:
            session.process.kill()
            raise
        check(errors == 0, f"sel {mode} pipe session: zero GL errors ({errors})")
    # Picks in a second pass, now that the centres (from noext) are known; every twin frames the same bounds.
    check(len(results.get("centres", {})) == len(NON_SELECTABLE), f"screen centres ({results.get('centres')})")
    check(all(results[m]["camera"].get("eye") == results["noext"]["camera"].get("eye") for m in ("sel", "removed")),
          "every twin starts with the same camera (the Anchor keeps the bounds)")
    picks = {}
    for mode, flags in (("sel", ()), ("removed", ()), ("noext", ()), ("off", ("--disable-extension", SEL))):
        model = files["sel_sel"] if mode == "off" else files[f"sel_{mode}"]
        session = Session(model, flags)
        try:
            picks[mode] = {name: pick_node(session, x, y) for name, (x, y) in results["centres"].items()}
            session.close()
        except Exception:
            session.process.kill()
            raise
    for name in results["centres"]:
        a, b, c, d = picks["sel"][name], picks["removed"][name], picks["noext"][name], picks["off"][name]
        check(a == b == BEHIND[name] and c in SUBTREE[name] and d == c,
              f"pick at {name}: {a} (removed twin {b}, expected {BEHIND[name]}); noext {c}; switched off {d}")
    sel = results["sel"]
    check("node=Blocker" in sel.get("select_blocker", "") and "node=ChildInherit" in sel.get("select_child", ""),
          f"the hierarchy (pipe select) still selects non-selectable nodes ({sel.get('select_blocker')} / "
          f"{sel.get('select_child')})")
    by_name = {n["name"]: n for n in sel.get("nodes", [])}
    got = {n: (by_name.get(n, {}).get("selectable"), by_name.get(n, {}).get("hoverable"))
           for n in ("Blocker", "ParentNS", "ChildInherit", "ChildExplicit", "Lone", "Back")}
    expected = {"Blocker": (False, True), "ParentNS": (False, False), "ChildInherit": (None, None),
                "ChildExplicit": (True, None), "Lone": (False, None), "Back": (None, None)}
    check(got == expected, f"list_nodes selectable / hoverable (own values): {got}")

    print("[KHR_node_selectability: window clicks]")
    clicks = {mode: click_script(files[f"sel_{mode}"], f"sel_clicks_{mode}") for mode in ("sel", "removed", "noext")}
    if check(all(clicks[m] is not None for m in clicks) and len(clicks["sel"]) == len(clicks["removed"]),
             "click scripts ran"):
        through = sum(1 for a, c in zip(clicks["sel"], clicks["noext"]) if a != c)
        mismatch = [(i, a, b, c) for i, (a, b, c) in enumerate(zip(clicks["sel"], clicks["removed"], clicks["noext"]))
                    if a != b]
        if mismatch:
            print(f"  INFO clicks where sel != removed (index, sel, removed, noext): {mismatch[:12]}")
        hits = sum(1 for c in clicks["noext"] if c in NON_SELECTABLE)
        check(clicks["sel"] == clicks["removed"] and hits > 20 and through == hits,
              f"{len(clicks['sel'])} window clicks: the selection equals the removed twin's at every click; "
              f"{hits} clicks hit a non-selectable node in noext and pass through ({through} differ)")

    print("[KHR_node_selectability: animation pointer]")
    session = Session(files["sel_anim"])
    try:
        x, y = results["centres"]["Blocker"]
        session.call("set_animation_time", time=0.5)
        at05 = pick_node(session, x, y)
        session.call("set_animation_time", time=1.5)
        at15 = pick_node(session, x, y)
        session.call("set_animation_time", time=0.25)
        at025 = pick_node(session, x, y)
        nodes = {n["name"]: n for n in session.call("list_nodes")["nodes"]}
        errors = session.close()
    except Exception:
        session.process.kill()
        raise
    check(at05 == "Blocker" and at15 == "Back" and at025 == "Blocker" and errors == 0,
          f"animated selectable 1 -> 0 at 1 s: pick at 0.5 s {at05}, at 1.5 s {at15}, back at 0.25 s {at025}")
    check(nodes.get("Blocker", {}).get("selectable") is True, "list_nodes: authored selectable stays true")

    print("[KHR_node_selectability / hoverability: validator]")
    issues = validate(files["sel_sel"], "sel_sel")
    check(issues is not None and not notices(issues), f"sel: no validation error, no extension notice ({notices(issues)})")
    issues = validate(files["sel_badtype"], "sel_badtype")
    bad = [i for i in issues or [] if i.get("code") == "KHR_NODE_SELECTABILITY_INVALID_TYPE"]
    check(len(bad) == 1 and bad[0].get("pointer", "").endswith(f"/extensions/{SEL}/selectable"),
          f"badtype: KHR_NODE_SELECTABILITY_INVALID_TYPE ({[(i.get('code'), i.get('pointer')) for i in bad]})")
    if baseline is not None:
        session_exe = baseline
        before, _, _ = render(session_exe, files["sel_sel"], "base_sel")
        check(same(before, img["sel"]), f"sel: byte-identical to the baseline {baseline.name}")


def click_script(model, name):
    """Window clicks over the viewport; the selected node name per click (None: nothing selected)."""
    lines = ["wait 8"]
    points = [(x, y) for y in range(110, 640, 20) for x in range(300, 1000, 20)]
    for x, y in points:
        lines += [f"click {x} {y}", "wait 4", f"snap c{x}_{y}"]
    script = OUT / f"{name}.txt"
    report = OUT / f"{name}.ui.json"
    script.write_text("\n".join(lines) + "\n", encoding="utf-8")
    report.unlink(missing_ok=True)
    run([EXE, model, *FLAGS, "--ui-script", script, "--ui-report", report])
    data = read_json(report)
    if data is None or data.get("failed", True):
        return None
    selected = []
    for snap in data["snaps"]:
        node = snap["selection"].split("|node=", 1)[1].split("|", 1)[0] if "|node=" in snap["selection"] else ""
        selected.append(node or None)
    return selected


def tiles_checks(files, baseline):
    print("[3D Tiles / Cesium: renders]")
    plain, _, log = render(EXE, files["tiles_plain"], "tiles_plain", report=False)
    check(plain is not None and gl_errors_zero(log), "tiles_plain: captured, zero GL errors")
    for mode in ("tiles", "required"):
        image, _, log = render(EXE, files[f"tiles_{mode}"], f"tiles_{mode}", report=False)
        check(same(image, plain) and gl_errors_zero(log),
              f"tiles_{mode}: loads and renders byte-identical to the plain twin")
    noattr, _, _ = render(EXE, files["tiles_noattr"], "tiles_noattr", report=False)
    check(same(noattr, plain), "the _FEATURE_ID_0 vertex attribute changes no pixel (plain twin without it)")
    for name in TILES:
        image, _, log = render(EXE, files["tiles_tiles"], f"tiles_off_{name}", ["--disable-extension", name], report=False)
        check(same(image, plain), f"--disable-extension {name}: byte-identical to the plain twin")
    image, _, log = render(EXE, files["tiles_required"], "tiles_required_off", ["--disable-extension", "CESIUM_RTC"])
    check("requires CESIUM_RTC, which is switched off" in log and "Failed to load" in log,
          "required + switched off: the file is not loaded, the log names the reason")

    print("[3D Tiles / Cesium: validator, get_state]")
    for mode in ("tiles", "required"):
        issues = validate(files[f"tiles_{mode}"], f"tiles_{mode}")
        check(issues is not None and not notices(issues),
              f"tiles_{mode}: no validation error, no extension notice ({notices(issues)})")
    states = {}
    for mode, flags in (("tiles", ()), ("off", tuple(a for n in TILES for a in ("--disable-extension", n)))):
        session = Session(files["tiles_tiles"], flags)
        try:
            states[mode] = session.call("get_state").get("tiles3d") or {}
            errors = session.close()
        except Exception:
            session.process.kill()
            raise
        check(errors == 0, f"tiles {mode} pipe session: zero GL errors ({errors})")
    t = states.get("tiles") or {}
    if not check(bool(t), "get_state has tiles3d"):
        return
    check(t["rtcCenter"] == RTC_CENTER, f"get_state.tiles3d.rtcCenter = {t['rtcCenter']}")
    check(t["structuralMetadata"] and t["schemaId"] == "city" and sorted(t["classes"]) == ["building", "facade"]
          and t["enums"] == 1 and t["propertyAttributes"] == 1 and t["metadataPrimitives"] == 1
          and t["propertyTables"] == [{"name": "buildings", "class": "building", "count": 6, "properties": ["height"]}],
          f"EXT_structural_metadata: schema, classes, enum, property table, property attribute ({t})")
    check(t["meshFeaturePrimitives"] == 1 and t["featureIdSets"] == 2 and t["featureIdAttributes"] == 1
          and t["featureIdImplicit"] == 1 and t["featureIdTextures"] == 0,
          "EXT_mesh_features: 2 feature ID sets (attribute, implicit) on 1 primitive")
    check(t["instanceFeatureNodes"] == 1 and t["instanceFeatureIdSets"] == 1 and t["outlinePrimitives"] == 1,
          "EXT_instance_features on 1 node, CESIUM_primitive_outline on 1 primitive")
    o = states["off"]
    check(o["rtcCenter"] is None and not o["structuralMetadata"] and o["meshFeaturePrimitives"] == 0
          and o["instanceFeatureNodes"] == 0 and o["outlinePrimitives"] == 0,
          "every one switched off: get_state.tiles3d is empty")
    if baseline is not None:
        before, _, _ = render(baseline, files["tiles_plain"], "base_tiles_plain")
        check(same(before, plain), f"tiles_plain: byte-identical to the baseline {baseline.name}")
        issues_base = OUT / "base_tiles.validation.json"
        issues_base.unlink(missing_ok=True)
        run([baseline, "--isolated", "--validate-file", files["tiles_tiles"], "--validation-report", issues_base])
        base_codes = notices((read_json(issues_base) or {}).get("issues", []))
        print(f"  INFO the baseline {baseline.name} reports for tiles_tiles: {base_codes}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", help="reference binary (default: the newest build/baselines/*.exe)")
    parser.add_argument("--only", choices=("dispersion", "selectability", "tiles"), action="append")
    args = parser.parse_args()
    baseline = baseline_exe(args.baseline_exe)
    if baseline is not None and not baseline.exists():
        baseline = None
    OUT.mkdir(parents=True, exist_ok=True)
    print(f"[fixtures] candidate {EXE}" + (f", baseline {baseline.name}" if baseline else ", no baseline"))
    files = write_fixtures()
    print(f"  INFO {len(files)} generated fixtures (build/trivial_extensions/fixtures, not committed)")
    parts = args.only or ["dispersion", "selectability", "tiles"]
    if "dispersion" in parts:
        dispersion_checks(files, baseline)
    if "selectability" in parts:
        selectability_checks(files, baseline)
    if "tiles" in parts:
        tiles_checks(files, baseline)


if __name__ == "__main__":
    main()
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)
