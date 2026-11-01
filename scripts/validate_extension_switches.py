# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""glTF extension switches (File > Settings > glTF Extensions): every supported extension can be switched off, and
off means Glider behaves as a loader without it.

Fixtures are written at test time into build/extension_switches/fixtures (never committed): small .gltf files with
data-URI buffers (textures as separate PNG / DDS files), one per representative extension, each with a "plain twin"
that is the same JSON with the extension removed (its names left out of extensionsUsed / extensionsRequired and its
objects removed from every "extensions" object; for KHR_animation_pointer also its "pointer" channels). The meshopt
cases reuse build/meshopt/fixtures (scripts/validate_meshopt.py's generator, run when missing), the instancing case
validate_gpu_instancing.py's writer.

Checks:
  * per extension (KHR_materials_emissive_strength, KHR_texture_transform, KHR_lights_punctual,
    EXT_meshopt_compression used and required, EXT_mesh_gpu_instancing, MSFT_texture_dds (texture fallback),
    KHR_materials_variants, KHR_animation_pointer, ASOBO_material_invisible): with the switch off
    (--disable-extension / --asobo-disable) the render is byte-identical to the plain twin; with it on the render is
    byte-identical to the baseline exe (as today); on differs from off (except variants, whose default material is the
    same: there the variant count in get_state is checked);
  * required + off: the file is not loaded and the log names the reason ("requires X, which is switched off");
    KHR_mesh_quantization cannot be ignored: off refuses a file that declares it;
  * validator: a switched-off extension is reported like an unsupported one, with "switched off" in the message
    (info EXTENSION_USED_SWITCHED_OFF, warning EXTENSION_REQUIRED_SWITCHED_OFF);
  * runtime (pipe session): switching an extension the open file declares reloads the file and the capture equals
    the fresh open with that switch (off and back on); an extension the file does not declare reloads nothing; an
    ASOBO rendering switch applies without a reload; a refused file opens when its switch is turned back on;
  * persistence (GLIDER_DATA_DIR temp dirs): a pristine run creates no settings.ini; set_persist + one switch off
    writes exactly that name=0 under [GltfExtensions]; a restart keeps it off (and the file is read without it);
    switching it on removes the entry and the empty section; --isolated reads and writes nothing; a command-line
    --disable-extension is never saved;
  * get_state gltfExtensions lists the new vendor groups (KHR, EXT, MSFT, ux3d) keyed by extension name.

Run: python scripts/validate_extension_switches.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import copy
import json
import os
import shutil
import struct
import subprocess as std_subprocess
import sys
import tempfile
import time
import zlib
from pathlib import Path

import numpy as np

import harness_process as subprocess
from glider_harness import EXE, REPO, baseline_exe, capture, connect_pipe, gl_errors_zero, read_json, run, same

OUT = REPO / "build" / "extension_switches"
FIXTURES = OUT / "fixtures"
MESHOPT = REPO / "build" / "meshopt" / "fixtures"
MESHOPT_TOOL = REPO / "build" / "tools" / "Release" / "meshopt_fixture_gen.exe"
FLAGS = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]

# Extra capture flags per case: the pointer animation is shown at 0.8 s (the capture does not play animations).
CASE_FLAGS = {"pointer": ["--anim-time", "0.8"]}

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label, flush=True)
    if not condition:
        failures.append(label)
    return condition


# --------------------------------------------------------------------------------------------- fixtures

def strip_extension(doc, names):
    """The plain twin: the document as a loader without `names` reads it."""
    doc = copy.deepcopy(doc)
    names = set(names)
    for key in ("extensionsUsed", "extensionsRequired"):
        if key in doc:
            doc[key] = [n for n in doc[key] if n not in names]
            if not doc[key]:
                del doc[key]
    if "KHR_animation_pointer" in names:
        for animation in doc.get("animations", []):
            animation["channels"] = [c for c in animation.get("channels", [])
                                     if c.get("target", {}).get("path") != "pointer"]

    def walk(node):
        if isinstance(node, dict):
            ext = node.get("extensions")
            if isinstance(ext, dict):
                for name in names:
                    ext.pop(name, None)
                if not ext:
                    del node["extensions"]
            for key, value in node.items():
                if key != "extras":
                    walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(doc)
    return doc


def png_bytes(width, height, pixels):
    """RGBA8 PNG (pixels: rows of (r, g, b, a))."""
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in pixels)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def dds_bytes(width, height, pixels):
    """Uncompressed DDS, 32-bit RGB with R in the low byte (DDPF_RGB | DDPF_ALPHAPIXELS), one mip level."""
    header = struct.pack("<4sIIIIIII44x", b"DDS ", 124, 0x1 | 0x2 | 0x4 | 0x1000 | 0x8, height, width, width * 4, 0, 1)
    pixel_format = struct.pack("<IIIIIIII", 32, 0x40 | 0x1, 0, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000)
    caps = struct.pack("<IIIII", 0x1000, 0, 0, 0, 0)
    data = bytes(c for row in pixels for px in row for c in px)
    return header + pixel_format + caps + data


def checker(colors, n=8):
    return [[colors[(x + y) % 2] for x in range(n)] for y in range(n)]


class Doc:
    """A minimal glTF writer: one data-URI buffer, bufferViews, accessors."""

    def __init__(self):
        self.data = bytearray()
        self.views, self.accessors = [], []

    def accessor(self, array, component_type, kind, minmax=False, target=None):
        payload = array.tobytes()
        while len(self.data) % 4:
            self.data.append(0)
        view = {"buffer": 0, "byteOffset": len(self.data), "byteLength": len(payload)}
        if target is not None:
            view["target"] = target
        self.data += payload
        self.views.append(view)
        entry = {"bufferView": len(self.views) - 1, "componentType": component_type, "count": int(array.shape[0]),
                 "type": kind}
        if minmax:
            entry["min"] = array.min(axis=0).tolist()
            entry["max"] = array.max(axis=0).tolist()
        self.accessors.append(entry)
        return len(self.accessors) - 1

    def finish(self, doc):
        doc["asset"] = {"version": "2.0", "generator": "validate_extension_switches.py"}
        doc["buffers"] = [{"byteLength": len(self.data),
                           "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(self.data)).decode()}]
        doc["bufferViews"] = self.views
        doc["accessors"] = self.accessors
        return doc


def cube(doc, material, extras_primitive=None):
    """A unit cube with normals and per-face UVs; returns a mesh."""
    faces = [((1, 0, 0), (0, 1, 0)), ((-1, 0, 0), (0, 1, 0)), ((0, 1, 0), (0, 0, 1)), ((0, -1, 0), (0, 0, 1)),
             ((0, 0, 1), (0, 1, 0)), ((0, 0, -1), (0, 1, 0))]
    positions, normals, uvs, indices = [], [], [], []
    for normal, up in faces:
        n, u = np.array(normal, float), np.array(up, float)
        r = np.cross(u, n)
        base = len(positions)
        for su, sr in ((-1, -1), (-1, 1), (1, 1), (1, -1)):
            positions.append(0.5 * n + 0.5 * su * u + 0.5 * sr * r)
            normals.append(n)
            uvs.append(((sr + 1) / 2, (1 - su) / 2))
        quad = [base, base + 1, base + 2, base, base + 2, base + 3]
        a, b, c = (positions[i] for i in quad[:3])
        if np.dot(np.cross(b - a, c - a), n) < 0:
            quad = [quad[0], quad[2], quad[1], quad[3], quad[5], quad[4]]
        indices += quad
    primitive = {
        "attributes": {
            "POSITION": doc.accessor(np.array(positions, np.float32), 5126, "VEC3", minmax=True, target=34962),
            "NORMAL": doc.accessor(np.array(normals, np.float32), 5126, "VEC3", target=34962),
            "TEXCOORD_0": doc.accessor(np.array(uvs, np.float32), 5126, "VEC2", target=34962),
        },
        "indices": doc.accessor(np.array(indices, np.uint16), 5123, "SCALAR", target=34963),
        "material": material,
    }
    if extras_primitive:
        primitive.update(extras_primitive)
    return {"primitives": [primitive]}


def scene_doc(materials, nodes_extra=None, used=(), required=(), root_extensions=None, images=None, textures=None,
              mesh_extensions=None, animations=None, second_material=None):
    """Two cubes side by side (material 0 on the left one, material `second_material` or 0 on the right one)."""
    doc = Doc()
    meshes = [cube(doc, 0, mesh_extensions)]
    nodes = [{"name": "Left", "mesh": 0, "translation": [-0.8, 0, 0], "rotation": [0.2, 0.3, 0.0, 0.93]}]
    if second_material is not None:
        meshes.append(cube(doc, second_material))
        nodes.append({"name": "Right", "mesh": 1, "translation": [0.8, 0, 0], "rotation": [0.2, -0.3, 0.0, 0.93]})
    nodes += nodes_extra or []
    out = {"scene": 0, "scenes": [{"nodes": list(range(len(nodes)))}], "nodes": nodes, "meshes": meshes,
           "materials": materials}
    if used:
        out["extensionsUsed"] = list(used)
    if required:
        out["extensionsRequired"] = list(required)
    if root_extensions:
        out["extensions"] = root_extensions
    if images:
        out["images"] = images
    if textures:
        out["textures"] = textures
        out["samplers"] = [{"magFilter": 9728, "minFilter": 9728}]
    if animations:
        out["animations"] = animations(doc)
    return doc.finish(out)


def pbr(color, metallic=0.1, roughness=0.5, **extra):
    material = {"pbrMetallicRoughness": {"baseColorFactor": color, "metallicFactor": metallic,
                                         "roughnessFactor": roughness}}
    material.update(extra)
    return material


def write(name, doc):
    path = FIXTURES / f"{name}.gltf"
    path.write_text(json.dumps(doc, indent=1), encoding="utf-8")
    return path


def write_case(name, doc, extension):
    return write(name, doc), write(name + "_plain", strip_extension(doc, [extension]))


def write_fixtures():
    FIXTURES.mkdir(parents=True, exist_ok=True)
    cases = {}

    # KHR_materials_emissive_strength: an emissive cube 6x brighter with the extension.
    emissive = pbr([0.2, 0.2, 0.2, 1.0], emissiveFactor=[1.0, 0.4, 0.1],
                   extensions={"KHR_materials_emissive_strength": {"emissiveStrength": 6.0}})
    doc = scene_doc([emissive, pbr([0.3, 0.6, 0.9, 1.0])], used=["KHR_materials_emissive_strength"], second_material=1)
    cases["emissive"] = ("KHR_materials_emissive_strength",) + write_case("emissive", doc, "KHR_materials_emissive_strength")
    doc_required = copy.deepcopy(doc)
    doc_required["extensionsRequired"] = ["KHR_materials_emissive_strength"]
    cases["emissive_required"] = ("KHR_materials_emissive_strength", write("emissive_required", doc_required), None)

    # Textures: a PNG checker (core source) and a differently coloured DDS checker (MSFT_texture_dds).
    (FIXTURES / "checker.png").write_bytes(png_bytes(8, 8, checker([(240, 240, 240, 255), (30, 90, 200, 255)])))
    (FIXTURES / "checker.dds").write_bytes(dds_bytes(8, 8, checker([(250, 220, 40, 255), (200, 30, 30, 255)])))

    # KHR_texture_transform on the base colour texture.
    textured = pbr([1, 1, 1, 1], metallic=0.0, roughness=0.6)
    textured["pbrMetallicRoughness"]["baseColorTexture"] = {
        "index": 0, "extensions": {"KHR_texture_transform": {"offset": [0.25, 0.1], "rotation": 0.6, "scale": [3, 2]}}}
    doc = scene_doc([textured], used=["KHR_texture_transform"], images=[{"uri": "checker.png"}],
                    textures=[{"source": 0, "sampler": 0}], second_material=0)
    cases["transform"] = ("KHR_texture_transform",) + write_case("transform", doc, "KHR_texture_transform")

    # MSFT_texture_dds: the DDS image preferred, the PNG as the core source (fallback).
    plain_tex = pbr([1, 1, 1, 1], metallic=0.0, roughness=0.6)
    plain_tex["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
    doc = scene_doc([plain_tex], used=["MSFT_texture_dds"], images=[{"uri": "checker.png"}, {"uri": "checker.dds"}],
                    textures=[{"source": 0, "sampler": 0, "extensions": {"MSFT_texture_dds": {"source": 1}}}],
                    second_material=0)
    cases["dds"] = ("MSFT_texture_dds",) + write_case("dds", doc, "MSFT_texture_dds")

    # KHR_lights_punctual: a strong red spot light and a green point light next to the cubes.
    lights = {"KHR_lights_punctual": {"lights": [
        {"type": "spot", "color": [1.0, 0.1, 0.1], "intensity": 60.0, "spot": {"innerConeAngle": 0.3, "outerConeAngle": 0.7}},
        {"type": "point", "color": [0.1, 1.0, 0.2], "intensity": 25.0}]}}
    light_nodes = [{"name": "Spot", "translation": [0, 0, 2.5], "extensions": {"KHR_lights_punctual": {"light": 0}}},
                   {"name": "Point", "translation": [1.6, 1.2, 0.8], "extensions": {"KHR_lights_punctual": {"light": 1}}}]
    doc = scene_doc([pbr([0.8, 0.8, 0.8, 1.0])], nodes_extra=light_nodes, used=["KHR_lights_punctual"],
                    root_extensions=lights, second_material=0)
    cases["lights"] = ("KHR_lights_punctual",) + write_case("lights", doc, "KHR_lights_punctual")

    # KHR_materials_variants: the default material orange, the variant "Blue" maps the primitive to blue.
    variants = {"KHR_materials_variants": {"variants": [{"name": "Blue"}]}}
    mapping = {"extensions": {"KHR_materials_variants": {"mappings": [{"material": 1, "variants": [0]}]}}}
    doc = scene_doc([pbr([0.95, 0.5, 0.1, 1.0]), pbr([0.1, 0.3, 0.95, 1.0])], used=["KHR_materials_variants"],
                    root_extensions=variants, mesh_extensions=mapping)
    cases["variants"] = ("KHR_materials_variants",) + write_case("variants", doc, "KHR_materials_variants")

    # KHR_animation_pointer: the base colour factor animated from red to green (shown at --anim-time 0.8).
    def pointer_animation(doc):
        times = doc.accessor(np.array([0.0, 1.0], np.float32), 5126, "SCALAR", minmax=True)
        colors = doc.accessor(np.array([[0.9, 0.2, 0.2, 1.0], [0.1, 0.9, 0.2, 1.0]], np.float32), 5126, "VEC4")
        return [{"name": "Colour", "samplers": [{"input": times, "output": colors, "interpolation": "LINEAR"}],
                 "channels": [{"sampler": 0, "target": {"path": "pointer", "extensions": {"KHR_animation_pointer": {
                     "pointer": "/materials/0/pbrMetallicRoughness/baseColorFactor"}}}}]}]

    doc = scene_doc([pbr([0.9, 0.2, 0.2, 1.0])], used=["KHR_animation_pointer"], animations=pointer_animation,
                    second_material=0)
    cases["pointer"] = ("KHR_animation_pointer",) + write_case("pointer", doc, "KHR_animation_pointer")

    # ASOBO_material_invisible (a rendering switch): the right cube is hidden with the extension.
    invisible = pbr([0.2, 0.8, 0.3, 1.0], extensions={"ASOBO_material_invisible": {}})
    doc = scene_doc([pbr([0.8, 0.8, 0.8, 1.0]), invisible], used=["ASOBO_material_invisible"], second_material=1)
    cases["asobo"] = ("ASOBO_material_invisible",) + write_case("asobo", doc, "ASOBO_material_invisible")

    # EXT_mesh_gpu_instancing: validate_gpu_instancing.py's 5 x 5 grid; the plain twin is the stripped file.
    import validate_gpu_instancing as gi
    gi.FIXTURES = FIXTURES
    inst = gi.write_case("grid", 5, 5, 2.5, "inst")
    single = gi.write_case("grid", 5, 5, 2.5, "single")
    stripped = strip_extension(json.loads(Path(inst).read_text(encoding="utf-8")), ["EXT_mesh_gpu_instancing"])
    cases["instancing"] = ("EXT_mesh_gpu_instancing", Path(inst), write("grid_inst_plain", stripped))
    cases["instancing_single"] = ("EXT_mesh_gpu_instancing", Path(single), None)

    # EXT_meshopt_compression: build/meshopt/fixtures (ext_optional.gltf only uses it, ext.glb requires it).
    if not (MESHOPT / "ext_optional.gltf").exists() and MESHOPT_TOOL.exists():
        std_subprocess.run([str(MESHOPT_TOOL), str(MESHOPT)], capture_output=True, text=True)
    if (MESHOPT / "ext_optional.gltf").exists():
        for name in ("ext_optional.bin", "ext_optional_fallback.bin"):
            shutil.copyfile(MESHOPT / name, FIXTURES / name)
        doc = json.loads((MESHOPT / "ext_optional.gltf").read_text(encoding="utf-8"))
        cases["meshopt"] = ("EXT_meshopt_compression", write("meshopt_optional", doc),
                            write("meshopt_optional_plain", strip_extension(doc, ["EXT_meshopt_compression"])))
        cases["meshopt_required"] = ("EXT_meshopt_compression", MESHOPT / "ext.glb", None)
        cases["meshopt_plain_glb"] = ("EXT_meshopt_compression", MESHOPT / "ext_plain.glb", None)
    return cases


# --------------------------------------------------------------------------------------------- Glider runs

def render(exe, model, name, flags=()):
    """(png or None, log) of one isolated capture at frame 24."""
    png, _report, log = capture(exe, model, OUT / f"{name}.png", FLAGS + list(flags))
    return png, log


def validate(model, name, flags=()):
    report = OUT / f"{name}.validation.json"
    report.unlink(missing_ok=True)
    run([EXE, "--isolated", *flags, "--validate-file", model, "--validation-report", report])
    data = read_json(report)
    return None if data is None else data.get("issues", [])


class Session:
    """A Glider with --control-pipe and a connected client (no os.path.exists poll on the pipe)."""

    def __init__(self, model, flags=(), env_extra=None, isolated=True):
        common = FLAGS if isolated else [a for a in FLAGS if a != "--isolated"]
        args = [str(EXE)] + ([str(model)] if model else []) + common + list(flags) + ["--control-pipe"]
        env = os.environ.copy()
        env["GLIDER_NO_FOCUS"] = "1"
        env.update(env_extra or {})
        self.process = subprocess.Popen(args, cwd=REPO, env=env, stdin=subprocess.DEVNULL,
                                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            self.pipe = connect_pipe(self.process)
        except BaseException:
            self.process.kill()
            raise

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def request(self, cmd, **args):
        return self.pipe.request(cmd, args)

    def vset(self, key, value):
        return self.call("set_viewport_setting", key=key, value=value)

    def open_file(self):
        return self.call("get_state")["gltfExtensions"]["openFile"]

    def wait_reloads(self, count, timeout=120):
        """Waits until the switch reloads reach `count` and no load is pending; returns the openFile state."""
        deadline = time.time() + timeout
        while True:
            state = self.open_file()
            if state["reloads"] >= count and not state["loadPending"]:
                return state
            if time.time() > deadline:
                return state
            time.sleep(0.2)

    def capture(self, name, settle=20):
        path = OUT / f"{name}.png"
        path.unlink(missing_ok=True)
        self.call("capture", path=str(path), settle_frames=settle)
        return path if path.exists() else None

    def close(self):
        errors = None
        try:
            errors = self.call("get_state")["glErrors"]
            self.call("quit")
        finally:
            self.pipe.close()
            try:
                self.process.wait(60)
            except Exception:  # noqa: BLE001
                self.process.kill()
        return errors


def read_ini(path):
    return path.read_text(encoding="utf-8") if path.exists() else ""


def extension_lines(text):
    lines, inside = [], False
    for line in text.splitlines():
        if line.startswith("["):
            inside = line.strip() == "[GltfExtensions]"
            continue
        if inside and line.strip() and not line.startswith(";"):
            lines.append(line.strip())
    return lines


def off_flags(extension):
    return ["--asobo-disable", extension] if extension.startswith("ASOBO_") else ["--disable-extension", extension]


# --------------------------------------------------------------------------------------------- checks

def check_renders(cases, baseline):
    print("[renders: off = plain twin, on = the baseline]")
    images = {}
    for case in ("emissive", "transform", "dds", "lights", "variants", "pointer", "asobo", "instancing", "meshopt"):
        if case not in cases:
            check(False, f"{case}: fixture written")
            continue
        extension, model, plain = cases[case]
        extra = CASE_FLAGS.get(case, [])
        on, log_on = render(EXE, model, f"{case}_on", extra)
        off, log_off = render(EXE, model, f"{case}_off", extra + off_flags(extension))
        twin, log_twin = render(EXE, plain, f"{case}_plain", extra)
        images[case] = (on, off, twin)
        check(on is not None and off is not None and twin is not None and "Loaded successfully" in log_off
              and gl_errors_zero(log_on) and gl_errors_zero(log_off) and gl_errors_zero(log_twin),
              f"{case}: on, off and plain twin loaded and captured, zero GL errors")
        check(same(off, twin), f"{case}: {extension} off renders byte-identical to the file without it")
        if not extension.startswith("ASOBO_"):
            check(f"read as absent: {extension}" in log_off,
                  f"{case}: the log says {extension} was read as absent")
        if case == "meshopt":
            # The fallback buffer holds the decoded bytes (validate_meshopt.py), so both read the same geometry.
            check(same(on, off), f"{case}: on (decoded) and off (fallback buffer read) render the same geometry")
        elif case != "variants":
            # Variants: the default material is the same either way; the variant list (get_state, Viewport Settings
            # panel) is what changes, checked in the runtime part.
            check(on is not None and not same(on, off), f"{case}: on differs from off (the comparison is not trivial)")
        if baseline is not None and baseline.exists():
            reference, _ = render(baseline, model, f"{case}_baseline", extra)
            check(same(on, reference), f"{case}: {extension} on renders byte-identical to {baseline.stem}")
    if "instancing_single" in cases:
        single, _ = render(EXE, cases["instancing_single"][1], "instancing_single")
        check(same(images.get("instancing", (None, None))[1], single),
              "instancing: off draws the node once (= the same node without instances)")
    if "meshopt_plain_glb" in cases and "meshopt" in images:
        glb, _ = render(EXE, cases["meshopt_plain_glb"][1], "meshopt_plain_glb")
        check(same(images["meshopt"][1], glb), "meshopt: off (fallback buffer read) = ext_plain.glb")


def check_required(cases):
    print("[required + off: not loaded, the log names the reason]")
    for case, extension in (("emissive_required", "KHR_materials_emissive_strength"),
                            ("meshopt_required", "EXT_meshopt_compression")):
        if case not in cases:
            check(False, f"{case}: fixture written")
            continue
        model = cases[case][1]
        png, log = render(EXE, model, f"{case}_off", off_flags(extension))
        check(f"requires {extension}, which is switched off" in log and "Failed to load" in log,
              f"{case}: {extension} off: not loaded, the log names the reason")
        check(gl_errors_zero(log), f"{case}: zero GL errors after the refused load")
        on, log_on = render(EXE, model, f"{case}_on")
        check(on is not None and "Loaded successfully" in log_on, f"{case}: loads with the switch on")
    if "meshopt_plain_glb" in cases:
        _, log = render(EXE, cases["meshopt_plain_glb"][1], "quantization_off", ["--disable-extension",
                                                                                   "KHR_mesh_quantization"])
        check("KHR_mesh_quantization, which is switched off" in log and "Failed to load" in log,
              "KHR_mesh_quantization off: a file that uses it is not loaded (cannot be ignored)")
    # The meshopt test hook keeps working as an alias of both meshopt switches.
    if "meshopt_required" in cases:
        env = dict(os.environ, GLIDER_TEST_DISABLE_MESHOPT="1")
        result = run([EXE, cases["meshopt_required"][1], *FLAGS, "--capture-viewport", OUT / "alias.png",
                      "--capture-frame", "8", "--exit-after-profile", "--profile-report", OUT / "alias.json"], env=env)
        log = result.stdout + result.stderr
        check("requires EXT_meshopt_compression, which is switched off" in log and "Failed to load" in log,
              "GLIDER_TEST_DISABLE_MESHOPT=1 is an alias of the meshopt switches")


def check_validator(cases):
    print("[validator: switched off is reported like unsupported]")
    issues = validate(cases["emissive"][1], "emissive_off", ["--disable-extension", "KHR_materials_emissive_strength"])
    codes = [(i.get("code"), i.get("severity"), i.get("message", "")) for i in issues or []]
    check(any(c == "EXTENSION_USED_SWITCHED_OFF" and "switched off" in m and s.lower().startswith("info")
              for c, s, m in codes), f"used + off: info EXTENSION_USED_SWITCHED_OFF ({codes})")
    issues = validate(cases["emissive_required"][1], "emissive_required_off",
                      ["--disable-extension", "KHR_materials_emissive_strength"])
    codes = [(i.get("code"), i.get("severity"), i.get("message", "")) for i in issues or []]
    check(any(c == "EXTENSION_REQUIRED_SWITCHED_OFF" and "switched off" in m and s.lower().startswith("warn")
              for c, s, m in codes), f"required + off: warning EXTENSION_REQUIRED_SWITCHED_OFF ({codes})")
    issues = validate(cases["emissive"][1], "emissive_on")
    check(issues is not None and not any("SWITCHED_OFF" in (i.get("code") or "") for i in issues),
          "default switches: no switched-off notice")


def check_runtime(cases):
    print("[runtime: a switch change reloads the open file; the view equals a fresh open]")
    lights = cases["lights"][1]
    session = Session(lights)
    try:
        state = session.open_file()
        check(state["lights"] == 2 and "KHR_lights_punctual" in state["declared"] and state["reloads"] == 0,
              f"open file: 2 lights, KHR_lights_punctual declared ({state})")
        session.vset("KHR_texture_basisu", False)  # the file does not declare it: nothing to reload
        time.sleep(1.0)
        state = session.open_file()
        check(state["reloads"] == 0, f"a switch the file does not declare reloads nothing ({state['reloads']})")
        session.vset("KHR_texture_basisu", True)
        session.vset("KHR_lights_punctual", False)
        state = session.wait_reloads(1)
        check(state["reloads"] == 1 and state["lights"] == 0 and state["switchedOff"] == ["KHR_lights_punctual"],
              f"KHR_lights_punctual off: the file is reopened without lights ({state})")
        off = session.capture("runtime_lights_off")
        session.vset("KHR_lights_punctual", True)
        state = session.wait_reloads(2)
        check(state["reloads"] == 2 and state["lights"] == 2 and state["switchedOff"] == [],
              f"back on: reopened with its lights ({state})")
        on = session.capture("runtime_lights_on")
        ext = session.call("get_state")["gltfExtensions"]
        check(ext["vendors"].get("KHR", {}).get("total", 0) >= 20 and "EXT" in ext["vendors"] and "MSFT" in ext["vendors"]
              and "ux3d" in ext["vendors"] and ext["vendors"]["KHR"]["key"] is None
              and ext["extensions"].get("KHR_lights_punctual") is True and "KHR_lights_punctual" in ext["appliedAtLoad"],
              "get_state gltfExtensions: KHR / EXT / MSFT / ux3d groups keyed by extension name")
        errors = session.close()
        check(errors == 0, f"runtime lights session: zero GL errors ({errors})")
    except Exception as error:  # noqa: BLE001
        check(False, f"runtime lights session failed: {error}")
        session.process.kill()
        return
    check(same(off, OUT / "lights_off.png"), "runtime off == a fresh open with --disable-extension KHR_lights_punctual")
    check(same(on, OUT / "lights_on.png"), "runtime back on == a fresh open")

    # A refused file opens when its switch is turned back on.
    session = Session(cases["emissive_required"][1], ["--disable-extension", "KHR_materials_emissive_strength"])
    try:
        state = session.open_file()
        check(state["loaded"] is False and state["switchedOff"] == ["KHR_materials_emissive_strength"],
              f"required + off at startup: not loaded ({state})")
        session.vset("KHR_materials_emissive_strength", True)
        state = session.wait_reloads(1)
        check(state["loaded"] is True and state["reloads"] == 1, f"switched on: the refused file is opened ({state})")
        opened = session.capture("runtime_required_on")
        errors = session.close()
        check(errors == 0, f"refused-file session: zero GL errors ({errors})")
    except Exception as error:  # noqa: BLE001
        check(False, f"refused-file session failed: {error}")
        session.process.kill()
        return
    check(same(opened, OUT / "emissive_required_on.png"), "the reopened file renders like a fresh open")

    # An ASOBO rendering switch applies at once, without a reload.
    session = Session(cases["asobo"][1])
    try:
        session.vset("ASOBO_material_invisible", False)
        time.sleep(1.0)
        state = session.open_file()
        check(state["reloads"] == 0, f"ASOBO rendering switch: no reload ({state['reloads']})")
        off = session.capture("runtime_asobo_off")
        errors = session.close()
        check(errors == 0, f"ASOBO session: zero GL errors ({errors})")
    except Exception as error:  # noqa: BLE001
        check(False, f"ASOBO session failed: {error}")
        session.process.kill()
        return
    check(same(off, OUT / "asobo_off.png"), "ASOBO rendering switch at runtime == --asobo-disable at startup")

    # Variants: the variant list follows the switch.
    session = Session(cases["variants"][1])
    try:
        check(session.open_file()["materialVariants"] == 1, "variants on: 1 material variant")
        session.vset("KHR_materials_variants", False)
        state = session.wait_reloads(1)
        check(state["materialVariants"] == 0, f"variants off: no material variants ({state['materialVariants']})")
        session.close()
    except Exception as error:  # noqa: BLE001
        check(False, f"variants session failed: {error}")
        session.process.kill()


def check_persistence(cases):
    print("[persistence: settings.ini [GltfExtensions] through GltfExtensionSettings]")
    lights = cases["lights"][1]
    data_dir = Path(tempfile.mkdtemp(prefix="glider_extsw_"))
    settings = data_dir / "settings.ini"
    env = {"GLIDER_DATA_DIR": str(data_dir)}
    key = "KHR_lights_punctual"
    try:
        # The default sample (no model: opening a file would add it to [RecentFiles]).
        session = Session(None, env_extra=env, isolated=False)
        session.close()
        check(not settings.exists(), f"a pristine run creates no settings.ini ({sorted(p.name for p in data_dir.iterdir())})")

        session = Session(lights, env_extra=env, isolated=False)
        try:
            session.vset(key, False)
            check(extension_lines(read_ini(settings)) == [], "a pipe edit without set_persist saves nothing")
            session.wait_reloads(1)
            session.vset(key, True)
            session.wait_reloads(2)
            session.call("set_persist", enabled=True)
            session.vset(key, False)
            check(extension_lines(read_ini(settings)) == [f"{key}=0"],
                  f"set_persist + {key} off writes exactly {key}=0 ({extension_lines(read_ini(settings))})")
            session.wait_reloads(3)
            session.close()
        except Exception as error:  # noqa: BLE001
            check(False, f"persistence session 1 failed: {error}")
            session.process.kill()
            return
        before = read_ini(settings)
        mtime = settings.stat().st_mtime_ns

        session = Session(lights, env_extra=env, isolated=False)
        try:
            ext = session.call("get_state")["gltfExtensions"]
            state = ext["openFile"]
            check(ext["extensions"][key] is False and ext["saved"] == [key] and state["lights"] == 0
                  and state["switchedOff"] == [key],
                  f"restart: still off, the file is read without lights ({ext['saved']}, {state})")
            session.close()
        except Exception as error:  # noqa: BLE001
            check(False, f"persistence session 2 failed: {error}")
            session.process.kill()
            return
        check(read_ini(settings) == before and settings.stat().st_mtime_ns == mtime,
              "a restart does not rewrite unchanged settings.ini")

        # --isolated reads nothing and writes nothing, even under set_persist.
        session = Session(lights, env_extra=env, isolated=True)
        try:
            ext = session.call("get_state")["gltfExtensions"]
            check(ext["extensions"][key] is True and ext["openFile"]["lights"] == 2,
                  "--isolated does not read the saved switch")
            session.call("set_persist", enabled=True)
            session.vset("KHR_materials_variants", False)
            session.close()
        except Exception as error:  # noqa: BLE001
            check(False, f"isolated session failed: {error}")
            session.process.kill()
            return
        check(read_ini(settings) == before and settings.stat().st_mtime_ns == mtime, "--isolated writes nothing")

        # A command-line flag is never saved, not even by a later persisted edit of another key.
        session = Session(lights, ["--disable-extension", "KHR_texture_transform"], env_extra=env, isolated=False)
        try:
            ext = session.call("get_state")["gltfExtensions"]
            check("KHR_texture_transform" in ext["disabled"] and "KHR_texture_transform" not in ext["saved"],
                  "--disable-extension: off for this run, not saved")
            session.call("set_persist", enabled=True)
            session.vset(key, True)
            text = read_ini(settings)
            check(extension_lines(text) == [] and "[GltfExtensions]" not in text,
                  f"switched back on: the entry and the empty section are removed ({text!r})")
            session.close()
        except Exception as error:  # noqa: BLE001
            check(False, f"persistence session 3 failed: {error}")
            session.process.kill()
    finally:
        shutil.rmtree(data_dir, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", help="reference binary (default: the newest build/baselines/*.exe)")
    args = parser.parse_args()
    baseline = baseline_exe(args.baseline_exe)
    if not EXE.exists():
        sys.exit(f"no build at {EXE}")
    OUT.mkdir(parents=True, exist_ok=True)
    print(f"[fixtures] candidate {EXE}, baseline {baseline}")
    cases = write_fixtures()
    total = sum(p.stat().st_size for p in FIXTURES.iterdir() if p.is_file())
    print(f"  INFO {len(cases)} cases, {total} bytes in build/extension_switches/fixtures (not committed)")
    check_renders(cases, baseline)
    check_required(cases)
    check_validator(cases)
    check_runtime(cases)
    check_persistence(cases)


if __name__ == "__main__":
    main()
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)
