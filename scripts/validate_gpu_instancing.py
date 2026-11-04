# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""EXT_mesh_gpu_instancing: a node draws its mesh once per instance (node world matrix x the instance's T * R * S).

Fixtures are written at test time into build/gpu_instancing/fixtures (one .gltf per case, buffers as data URIs,
never committed). Each instanced file ("*_inst") has a plain twin ("*_plain") that writes the same scene without the
extension: the instanced node keeps its transform (and animation) but no mesh, and gets one child node per instance
whose TRS is the instance's value, dequantized in float32 exactly as the loader does (normalized integers divided by
127 / 32767 / 255 / 65535, other integers as they are), so the twin's JSON numbers are those float32 values. The
renderer composes an instance's world matrix with the expressions of a child node's (TransformHierarchyNode), so the
per-instance matrices are the bytes the twin's child nodes get, and the twin's opaque copies are merged by P8.3 into
the same instanced draw: the comparisons are byte for byte. BLEND instances are drawn back to front like the twin's
separate draws (Mesh::sortInstancesBackToFront), so these hold for instances that overlap on screen too.

Checks:
  * renders (isolated, 1280x720, AA none, frame 24 of a fixed 0.05 s clock) byte-identical to the plain twin:
    a 5 x 5 grid with float TRS (rotations about several axes, non-uniform scales); quantized accessors (ROTATION
    signed byte normalized, SCALE signed short normalized, TRANSLATION signed short with KHR_mesh_quantization);
    a parent transform with a node animation on the instanced node (mid-animation frame); transparent (BLEND)
    instances; overlapping BLEND instances at scrambled depths, seen from both sides and under the node animation;
    a morphed mesh; with the sun, shadows and the ground plane (shadow pass and cascade fit); SSAO
    (depth/normal prepass); Wireframe shading mode and the Wireframe Overlay; each instanced render differs from
    the same node drawn once (the comparisons are not trivially equal);
  * framing: the startup camera (scene extents) equals the twin's; frame_node on the instanced node gives the camera
    frame_node gives on the twin's parent node (all instances inside);
  * picking: a grid of window clicks (--ui-script) selects the instanced node exactly where the twin selects one of
    its instance nodes, and nothing where the twin selects nothing; pipe `pick` at each twin child's screen-rect
    centre returns the instanced node;
  * Inspector / Statistics data: the profile report's gpuInstancing summary (nodes, instances, instancedTriangles =
    mesh triangles x instances) and list_nodes' gpuInstances;
  * 10,000 instances (opaque and BLEND): the renderer draw calls equal those of 100 instances (draws do not scale with
    the instance count); the plain twins' draw calls and CPU frame times are printed for comparison (INFO);
  * fallbacks: without the extension in extensionsUsed, with GLIDER_TEST_DISABLE_GPU_INSTANCING=1, and for a skinned
    node (logged) the node is drawn once: byte-identical to the file without the extension;
  * validator: no error and no unsupported-extension notice for the instanced files;
  * with a baseline: the plain twins render byte-identical to it, and the baseline (no extension support) draws the
    instanced grid like the single node, not like its twin.

Run: python scripts/validate_gpu_instancing.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import json
import math
import os
import statistics
import struct
import sys
from pathlib import Path

import numpy as np

from glider_harness import (EXE, REPO, baseline_exe, capture, gl_errors_zero, read_json, run, same,
                            start_pipe_session)

OUT = REPO / "build" / "gpu_instancing"
FIXTURES = OUT / "fixtures"
FLAGS = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
SUN = ["--sun", "on", "--sun-intensity", "1.8", "--sun-dir", "60,40", "--ground", "plane"]
DISABLE = "GLIDER_TEST_DISABLE_GPU_INSTANCING"
EXT = "EXT_mesh_gpu_instancing"

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


# --------------------------------------------------------------------------------------------- fixtures

F32 = np.float32


def f32(value):
    """A Python float holding exactly the float32 nearest to value (written to JSON, it reads back as that float32)."""
    return float(F32(value))


def dequantize(values, component):
    """The loader's dequantization (gltf Node.cpp readFloats), in float32."""
    out = []
    for v in values:
        v = F32(v)
        if component == "byte_n":
            out.append(float(max(v / F32(127.0), F32(-1.0))))
        elif component == "short_n":
            out.append(float(max(v / F32(32767.0), F32(-1.0))))
        else:
            out.append(float(v))
    return out


def quat_axis_angle(axis, degrees):
    axis = np.array(axis, dtype=np.float64)
    axis /= np.linalg.norm(axis)
    half = math.radians(degrees) / 2.0
    s = math.sin(half)
    return [axis[0] * s, axis[1] * s, axis[2] * s, math.cos(half)]  # x, y, z, w


class Doc:
    """A minimal glTF writer: one data-URI buffer, bufferViews, accessors."""

    def __init__(self):
        self.data = bytearray()
        self.views, self.accessors = [], []

    def view(self, payload, target=None):
        while len(self.data) % 4:
            self.data.append(0)
        entry = {"buffer": 0, "byteOffset": len(self.data), "byteLength": len(payload)}
        if target is not None:
            entry["target"] = target
        self.data += payload
        self.views.append(entry)
        return len(self.views) - 1

    def accessor(self, payload, component_type, count, kind, normalized=False, minmax=None, target=None):
        entry = {"bufferView": self.view(payload, target), "componentType": component_type, "count": count, "type": kind}
        if normalized:
            entry["normalized"] = True
        if minmax is not None:
            entry["min"], entry["max"] = minmax
        self.accessors.append(entry)
        return len(self.accessors) - 1

    def buffers(self):
        uri = "data:application/octet-stream;base64," + base64.b64encode(bytes(self.data)).decode("ascii")
        return [{"byteLength": len(self.data), "uri": uri}]


def cube_mesh(doc, material, morph=False, skinned=False):
    """A unit cube (24 vertices with normals, 36 uint16 indices)."""
    faces = [((1, 0, 0), (0, 1, 0)), ((-1, 0, 0), (0, 1, 0)), ((0, 1, 0), (0, 0, 1)), ((0, -1, 0), (0, 0, 1)),
             ((0, 0, 1), (0, 1, 0)), ((0, 0, -1), (0, 1, 0))]
    positions, normals, indices = [], [], []
    for normal, up in faces:
        n, u = np.array(normal, float), np.array(up, float)
        r = np.cross(u, n)
        base = len(positions)
        for su, sr in ((-1, -1), (-1, 1), (1, 1), (1, -1)):
            positions.append(0.5 * n + 0.5 * su * u + 0.5 * sr * r)
            normals.append(n)
        quad = [base, base + 1, base + 2, base, base + 2, base + 3]
        # Counter-clockwise seen from outside: flip when the corner order winds the other way.
        a, b, c = (positions[i] for i in quad[:3])
        if np.dot(np.cross(b - a, c - a), n) < 0:
            quad = [quad[0], quad[2], quad[1], quad[3], quad[5], quad[4]]
        indices += quad
    pos = np.array(positions, dtype=F32)
    attributes = {
        "POSITION": doc.accessor(pos.tobytes(), 5126, 24, "VEC3", minmax=([-0.5] * 3, [0.5] * 3), target=34962),
        "NORMAL": doc.accessor(np.array(normals, dtype=F32).tobytes(), 5126, 24, "VEC3", target=34962),
    }
    primitive = {"attributes": attributes, "material": material,
                 "indices": doc.accessor(np.array(indices, dtype=np.uint16).tobytes(), 5123, 36, "SCALAR",
                                         target=34963)}
    mesh = {"primitives": [primitive]}
    if morph:
        delta = np.zeros((24, 3), dtype=F32)
        delta[pos[:, 1] > 0, 1] = 0.6  # top face up
        delta[pos[:, 0] > 0, 0] = 0.3
        lo, hi = delta.min(axis=0).tolist(), delta.max(axis=0).tolist()
        primitive["targets"] = [{"POSITION": doc.accessor(delta.tobytes(), 5126, 24, "VEC3", minmax=(lo, hi),
                                                          target=34962)}]
        mesh["weights"] = [0.6]
    if skinned:
        attributes["JOINTS_0"] = doc.accessor(np.zeros((24, 4), dtype=np.uint8).tobytes(), 5121, 24, "VEC4",
                                              target=34962)
        weights = np.zeros((24, 4), dtype=F32)
        weights[:, 0] = 1.0
        attributes["WEIGHTS_0"] = doc.accessor(weights.tobytes(), 5126, 24, "VEC4", target=34962)
    return mesh


MATERIALS = [
    {"name": "opaque", "pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.45, 0.12, 1.0], "metallicFactor": 0.1,
                                                 "roughnessFactor": 0.45}},
    {"name": "blend", "alphaMode": "BLEND",
     "pbrMetallicRoughness": {"baseColorFactor": [0.15, 0.55, 0.95, 0.45], "metallicFactor": 0.0,
                              "roughnessFactor": 0.3}},
]


def grid_instances(nx, ny, spacing, rotate=True, depth=False):
    """(translations, rotations xyzw, scales) of an nx x ny grid in the XY plane, as float64 values."""
    t, r, s = [], [], []
    axes = [(0, 1, 0), (1, 0, 0), (0, 0, 1), (1, 1, 0), (0, 1, 1)]
    k = 0
    for j in range(ny):
        for i in range(nx):
            # depth: file order is unrelated to the distance from the camera on either side.
            z = ((k * 7) % (nx * ny)) * 0.35 if depth else 0.0
            t.append([(i - (nx - 1) / 2) * spacing, (j - (ny - 1) / 2) * spacing, z])
            r.append(quat_axis_angle(axes[k % len(axes)], (17 * k) % 80 + 5) if rotate else [0.0, 0.0, 0.0, 1.0])
            s.append([0.55 + 0.05 * (k % 5), 0.6 + 0.04 * (k % 7), 0.7] if rotate else [0.6, 0.6, 0.6])
            k += 1
    return t, r, s


def write_case(name, nx, ny, spacing, mode, material=0, quantized=False, parent=False, animated=False, morph=False,
               skinned=False, declare=True, rotate=True, depth=False):
    """One fixture. mode: inst (the extension), plain (one child node per instance), single (the node drawn once)."""
    doc = Doc()
    meshes = [cube_mesh(doc, material, morph=morph, skinned=skinned)]
    t, r, s = grid_instances(nx, ny, spacing, rotate, depth)
    count = len(t)
    used = []
    # The values the loader reads, as float32 (the plain twin writes exactly these).
    if quantized:
        t_q = [[int(round(v)) for v in row] for row in t]  # integer grid (spacing is an integer here)
        r_q = [[int(round(max(-1.0, min(1.0, v)) * 127)) for v in row] for row in r]
        s_q = [[int(round(v * 32767)) for v in row] for row in s]
        t_f = [dequantize(row, "short") for row in t_q]
        r_f = [dequantize(row, "byte_n") for row in r_q]
        s_f = [dequantize(row, "short_n") for row in s_q]
    else:
        t_f = [[f32(v) for v in row] for row in t]
        r_f = [[f32(v) for v in row] for row in r]
        s_f = [[f32(v) for v in row] for row in s]

    grid = {"name": "Grid"}
    nodes = [grid]
    if animated:
        grid["translation"] = [0.25, -0.5, 0.0]
    if mode == "inst":
        grid["mesh"] = 0
        if quantized:
            attributes = {
                "TRANSLATION": doc.accessor(np.array(t_q, dtype=np.int16).tobytes(), 5122, count, "VEC3"),
                "ROTATION": doc.accessor(np.array(r_q, dtype=np.int8).tobytes(), 5120, count, "VEC4", normalized=True),
                "SCALE": doc.accessor(np.array(s_q, dtype=np.int16).tobytes(), 5122, count, "VEC3", normalized=True),
            }
            used.append("KHR_mesh_quantization")
        else:
            attributes = {
                "TRANSLATION": doc.accessor(np.array(t_f, dtype=F32).tobytes(), 5126, count, "VEC3"),
                "ROTATION": doc.accessor(np.array(r_f, dtype=F32).tobytes(), 5126, count, "VEC4"),
                "SCALE": doc.accessor(np.array(s_f, dtype=F32).tobytes(), 5126, count, "VEC3"),
            }
        attributes["_ID"] = doc.accessor(np.arange(count, dtype=F32).tobytes(), 5126, count, "SCALAR")
        grid["extensions"] = {EXT: {"attributes": attributes}}
        if declare:
            used.append(EXT)
    elif mode == "plain":
        grid["children"] = []
        for k in range(count):
            grid["children"].append(len(nodes))
            nodes.append({"name": f"Cube_{k}", "mesh": 0, "translation": t_f[k], "rotation": r_f[k], "scale": s_f[k]})
    else:
        grid["mesh"] = 0
    roots = [0]
    if parent:
        for node in nodes:
            if "children" in node:
                node["children"] = [c + 1 for c in node["children"]]
        nodes.insert(0, {"name": "Parent", "translation": [0.5, 0.75, -1.0], "scale": [1.2, 1.1, 1.0],
                         "rotation": [f32(v) for v in quat_axis_angle((1, 0, 0), 20)], "children": [1]})
    grid_index = 1 if parent else 0
    document = {"asset": {"version": "2.0", "generator": "validate_gpu_instancing.py"}, "scene": 0,
                "materials": MATERIALS, "meshes": meshes}
    if skinned:
        joint = len(nodes)
        nodes.append({"name": "Joint"})
        roots.append(joint)
        nodes[grid_index]["skin"] = 0
        document["skins"] = [{"joints": [joint]}]
    if animated:
        times = np.array([0.0, 1.0, 2.0], dtype=F32)
        rotations = np.array([quat_axis_angle((0, 1, 0), a) for a in (0.0, 50.0, 100.0)], dtype=F32)
        moves = np.array([[0.25, -0.5, 0.0], [0.75, 0.0, 0.5], [0.25, 0.5, 0.0]], dtype=F32)
        inp = doc.accessor(times.tobytes(), 5126, 3, "SCALAR", minmax=([0.0], [2.0]))
        rot = doc.accessor(rotations.tobytes(), 5126, 3, "VEC4")
        mov = doc.accessor(moves.tobytes(), 5126, 3, "VEC3")
        document["animations"] = [{"name": "Move", "samplers": [{"input": inp, "output": rot},
                                                                {"input": inp, "output": mov}],
                                   "channels": [{"sampler": 0, "target": {"node": grid_index, "path": "rotation"}},
                                                {"sampler": 1, "target": {"node": grid_index, "path": "translation"}}]}]
    document["scenes"] = [{"nodes": roots}]
    document["nodes"] = nodes
    document["buffers"] = document_buffers = doc.buffers()
    document["bufferViews"] = doc.views
    document["accessors"] = doc.accessors
    if used:
        document["extensionsUsed"] = used
    path = FIXTURES / f"{name}_{mode}.gltf"
    path.write_text(json.dumps(document), encoding="utf-8")
    del document_buffers
    return path


CASES = {
    # name: (nx, ny, spacing, options)
    "grid": (5, 5, 2.5, {}),
    "quant": (5, 5, 3.0, {"quantized": True}),
    "anim": (4, 3, 2.5, {"parent": True, "animated": True}),
    "blend": (5, 4, 2.5, {"material": 1}),
    "overlap": (5, 4, 0.5, {"material": 1, "depth": True}),
    "overlapanim": (5, 4, 0.5, {"material": 1, "depth": True, "parent": True, "animated": True}),
    "morph": (3, 3, 3.0, {"morph": True}),
    "shadow": (4, 4, 2.0, {}),
}


def write_fixtures():
    FIXTURES.mkdir(parents=True, exist_ok=True)
    files = {}
    for name, (nx, ny, spacing, options) in CASES.items():
        for mode in ("inst", "plain", "single"):
            files[(name, mode)] = write_case(name, nx, ny, spacing, mode, **options)
    files[("grid", "undeclared")] = write_case("grid_undeclared", 5, 5, 2.5, "inst", declare=False)
    files[("skinned", "inst")] = write_case("skinned", 3, 3, 2.5, "inst", skinned=True)
    files[("skinned", "single")] = write_case("skinned", 3, 3, 2.5, "single", skinned=True)
    for count, label in ((10, "100"), (100, "10k")):
        for material, kind in ((0, "opaque"), (1, "blend")):
            for mode in ("inst", "plain"):
                files[(f"{kind}_{label}", mode)] = write_case(f"{kind}_{label}", count, count, 1.5, mode,
                                                              material=material, rotate=False)
    return files


# --------------------------------------------------------------------------------------------- Glider runs

def render(exe, model, name, flags=(), env=None):
    """(png or None, profile report or None, log) of one isolated capture at frame 24."""
    png = OUT / f"{name}.png"
    if env is None:
        return capture(exe, model, png, FLAGS + list(flags))
    report = OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    result = run([exe, model, *FLAGS, *flags, "--capture-viewport", png, "--capture-frame", "24", "--profile-report",
                  report, "--exit-after-profile"], env=env)
    return (png if png.exists() else None), read_json(report), result.stdout + result.stderr


def validate(model, name):
    report = OUT / f"{name}.validation.json"
    report.unlink(missing_ok=True)
    run([EXE, "--isolated", "--validate-file", model, "--validation-report", report])
    data = read_json(report)
    return None if data is None else data.get("issues", [])


def click_script(model, name):
    """Window clicks over the viewport; returns the selected node name per click (None: nothing selected)."""
    lines = ["wait 8"]
    points = [(x, y) for y in range(120, 610, 26) for x in range(380, 910, 26)]
    for x, y in points:
        lines += [f"click {x} {y}", "wait 6", f"snap c{x}_{y}"]
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


class Session:
    def __init__(self, model):
        self.process, self.pipe = start_pipe_session([EXE, model, *FLAGS, "--control-pipe"])

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


def max_draws(report):
    frames = (report or {}).get("frames", [])
    return max((f["counters"].get("rendererDrawCalls", 0) for f in frames), default=0)


def frame_ms(report):
    """Median CPU frame time of the frames that rendered the scene."""
    frames = [f for f in (report or {}).get("frames", []) if f["counters"].get("rendererDrawCalls", 0) > 0
              and not f.get("firstFrame")]
    return statistics.median(f["cpuFrameMs"] for f in frames) if frames else None


def close(a, b, tolerance=1e-5):
    return a is not None and b is not None and len(a) == len(b) and all(
        abs(x - y) <= tolerance * max(1.0, abs(x), abs(y)) for x, y in zip(a, b))


# --------------------------------------------------------------------------------------------- checks

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", help="reference binary (default: the newest build/baselines/*.exe)")
    args = parser.parse_args()
    baseline = baseline_exe(args.baseline_exe)
    OUT.mkdir(parents=True, exist_ok=True)
    print(f"[fixtures] candidate {EXE}")
    files = write_fixtures()
    total = sum(p.stat().st_size for p in FIXTURES.iterdir() if p.is_file())
    print(f"  INFO {len(files)} generated fixtures, {total} bytes (build/gpu_instancing/fixtures, not committed)")

    print("[renders: instanced = plain twin, byte for byte]")
    images, reports = {}, {}
    variants = [("grid", []), ("quant", []), ("anim", []), ("blend", []), ("overlap", []),
                ("overlap_back", ["--camera-yaw", "180"]), ("overlapanim", []), ("morph", []), ("shadow", SUN),
                ("grid_ssao", ["--ssao", "on", "--ground", "plane"]), ("grid_wire", ["--render-mode", "wireframe"]),
                ("grid_overlay", ["--wireframe-overlay"])]
    for variant, flags in variants:
        case = variant.split("_", 1)[0] if variant.startswith(("grid_", "overlap_")) else variant
        for mode in ("inst", "plain", "single"):
            image, report, log = render(EXE, files[(case, mode)], f"{variant}_{mode}", flags)
            images[(variant, mode)], reports[(variant, mode)] = image, report
            check(image is not None and "Loaded successfully" in log and gl_errors_zero(log),
                  f"{variant}_{mode}: loaded, captured, zero GL errors")
        check(same(images[(variant, "inst")], images[(variant, "plain")]),
              f"{variant}: instanced render byte-identical to the plain twin")
        check(images[(variant, "inst")] is not None and not same(images[(variant, "inst")], images[(variant, "single")]),
              f"{variant}: differs from the node drawn once")

    print("[framing]")
    for case in ("grid", "anim", "quant"):
        a, b = (reports[(case, "inst")] or {}).get("camera", {}), (reports[(case, "plain")] or {}).get("camera", {})
        check(a and a.get("eye") == b.get("eye") and a.get("target") == b.get("target") and a.get("far") == b.get("far"),
              f"{case}: startup framing (scene extents) equals the twin's (eye {a.get('eye')}, target {a.get('target')})")
    framed = {}
    for mode in ("inst", "plain"):
        session = Session(files[("anim", mode)])
        try:
            framed[mode] = session.call("frame_node", node="Grid", margin=1.0)["camera"]
            nodes = session.call("list_nodes", filter="Grid")["nodes"]
            if mode == "inst":
                grid = next((n for n in nodes if n["name"] == "Grid"), {})
                check(grid.get("gpuInstances") == 12 and grid.get("gpuInstancing") == "applied",
                      f"list_nodes: Grid draws 12 instances ({grid.get('gpuInstances')}, {grid.get('gpuInstancing')})")
            errors = session.close()
        except Exception:
            session.process.kill()
            raise
        check(errors == 0, f"anim_{mode} pipe session: zero GL errors ({errors})")
    check(close(framed["inst"].get("eye"), framed["plain"].get("eye"))
          and close(framed["inst"].get("target"), framed["plain"].get("target")),
          f"frame_node Grid: the camera equals the twin's (eye {framed['inst'].get('eye')} / {framed['plain'].get('eye')})")

    print("[picking]")
    inst_clicks = click_script(files[("grid", "inst")], "clicks_inst")
    plain_clicks = click_script(files[("grid", "plain")], "clicks_plain")
    if check(inst_clicks is not None and plain_clicks is not None and len(inst_clicks) == len(plain_clicks),
             "click scripts ran"):
        expected = ["Grid" if (n or "").startswith("Cube_") else n for n in plain_clicks]
        hits = sum(1 for n in plain_clicks if (n or "").startswith("Cube_"))
        instances = len({n for n in plain_clicks if (n or "").startswith("Cube_")})
        check(inst_clicks == expected and hits > 30 and instances >= 20,
              f"{len(inst_clicks)} window clicks: the instanced node is selected wherever the twin selects an instance "
              f"node ({hits} clicks on {instances} different instances), nothing elsewhere")
    plain = Session(files[("grid", "plain")])
    try:
        centres = {}
        for k in (0, 6, 12, 18, 24):
            rect = plain.call("node_screen_rect", node=f"Cube_{k}")["screenRect"]
            centres[k] = (int((rect[0] + rect[2]) / 2), int((rect[1] + rect[3]) / 2))
            picked = plain.call("pick", x=centres[k][0], y=centres[k][1])
            check(picked and picked.get("node") == f"Cube_{k}", f"twin: pick at Cube_{k}'s centre {centres[k]} returns it")
        plain.close()
    except Exception:
        plain.process.kill()
        raise
    inst = Session(files[("grid", "inst")])
    try:
        for k, (x, y) in centres.items():
            picked = inst.call("pick", x=x, y=y)
            check(picked and picked.get("node") == "Grid", f"pick at instance {k}'s centre ({x}, {y}) returns Grid ({picked and picked.get('node')})")
        inst.close()
    except Exception:
        inst.process.kill()
        raise

    print("[Statistics summary]")
    summary = (reports[("grid", "inst")] or {}).get("gpuInstancing")
    check(summary == {"nodes": 1, "instances": 25, "instancedTriangles": 300, "notApplied": 0},
          f"grid: profile gpuInstancing {summary} (1 node, 25 instances, 25 x 12 triangles)")
    check("gpuInstancing" not in (reports[("grid", "plain")] or {}), "plain twin: no gpuInstancing summary")

    print("[10,000 instances: draw calls do not scale]")
    big = {}
    for kind in ("opaque", "blend"):
        for label in ("100", "10k"):
            for mode in ("inst", "plain"):
                image, report, log = render(EXE, files[(f"{kind}_{label}", mode)], f"{kind}_{label}_{mode}")
                big[(kind, label, mode)] = report
                check(image is not None and gl_errors_zero(log), f"{kind}_{label}_{mode}: captured, zero GL errors")
        d100, d10k = max_draws(big[(kind, "100", "inst")]), max_draws(big[(kind, "10k", "inst")])
        summary = (big[(kind, "10k", "inst")] or {}).get("gpuInstancing", {})
        check(summary.get("instances") == 10000, f"{kind}_10k: 10,000 instances ({summary})")
        check(d100 > 0 and d10k == d100, f"{kind}: renderer draw calls 100 instances {d100}, 10,000 instances {d10k}")
        print(f"  INFO {kind}: plain twins {max_draws(big[(kind, '100', 'plain')])} / "
              f"{max_draws(big[(kind, '10k', 'plain')])} draw calls; CPU frame ms (median of rendered frames) "
              f"instanced 100 {frame_ms(big[(kind, '100', 'inst')])}, 10k {frame_ms(big[(kind, '10k', 'inst')])}; "
              f"plain 100 {frame_ms(big[(kind, '100', 'plain')])}, 10k {frame_ms(big[(kind, '10k', 'plain')])}")

    print("[fallbacks: the node is drawn once]")
    image, _, log = render(EXE, files[("grid", "undeclared")], "grid_undeclared")
    check(same(image, images[("grid", "single")]), "extension not in extensionsUsed: byte-identical to the single node")
    image, report, log = render(EXE, files[("grid", "inst")], "grid_disabled", env=dict(os.environ, **{DISABLE: "1"}))
    check(same(image, images[("grid", "single")]) and (report or {}).get("gpuInstancing", {}).get("notApplied") == 1,
          f"{DISABLE}=1: byte-identical to the single node, reported not applied")
    a, _, log = render(EXE, files[("skinned", "inst")], "skinned_inst")
    b, _, _ = render(EXE, files[("skinned", "single")], "skinned_single")
    check(same(a, b) and "is skinned; instancing is not applied" in log,
          "skinned node: logged, byte-identical to the node drawn once")

    print("[validator]")
    for case in ("grid", "quant", "anim", "blend", "morph"):
        issues = validate(files[(case, "inst")], f"{case}_inst")
        errors = [i.get("code") for i in issues or [] if i.get("severity") == "Error"]
        notices = [i.get("code") for i in issues or [] if str(i.get("code", "")).startswith("EXTENSION_")]
        warnings = [i.get("code") for i in issues or [] if i.get("severity") == "Warning"]
        check(issues is not None and not errors and not notices,
              f"{case}_inst: no validation error, no unsupported-extension notice ({errors + notices}; warnings {warnings})")

    print("[baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    print(f"  INFO baseline {baseline.name}")
    for case in ("grid", "anim", "blend"):
        before, _, _ = render(baseline, files[(case, "plain")], f"base_{case}_plain")
        check(same(before, images[(case, "plain")]), f"{case}_plain: byte-identical to the baseline {baseline.name}")
    # A baseline from before 9551e7c has no instancing (it draws the grid once, like the single node); once the
    # baseline instances, the instanced grid must stay byte-identical to it.
    before, _, _ = render(baseline, files[("grid", "inst")], "base_grid_inst")
    if same(before, images[("grid", "plain")]):
        check(same(before, images[("grid", "inst")]), f"grid_inst: byte-identical to the baseline {baseline.name}")
    else:
        check(same(before, images[("grid", "single")]),
              f"the baseline {baseline.name} draws the instanced grid once (like the single node, not like the twin)")


if __name__ == "__main__":
    main()
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)
