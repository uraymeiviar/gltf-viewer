# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Skinned mesh node transform: the app setting that decides whether a skinned mesh keeps its node's placement.

glTF ignores a skinned mesh node's own transform: the vertices go where jointWorld x IBM puts them. MSFS exports
(Blender + the Asobo exporter) write inverse bind matrices without the placed mesh node's transform, and Blender /
MSFS keep the mesh where its node places it. Glider detects that layout per skin (every joint gives one shared rest
jointWorld x IBM that is not the non-identity mesh node's world matrix) and the setting decides whether to correct it:
  spec  never (strict glTF);
  msfs  every detected skin;
  auto  (default) detected skins in files that declare an ASOBO_* extension.

The fixtures are written at test time into build/skin_node_transform (data-URI buffers, never committed):
  placed.gltf        a cube skinned to one joint at the origin (IBM identity), its mesh node at (3, 0, 0);
  placed_asobo.gltf  the same file declaring ASOBO_material_UV_options (an MSFS export as far as Auto can tell);
  twin_origin.gltf   an unskinned cube at the origin (where strict glTF draws the skinned one);
  twin_placed.gltf   an unskinned cube at (3, 0, 0) (where Blender / MSFS draw it).

Checks:
  * get_state.skinning per mode and file: detected 1, applied 0 / 1 as the mode says; the CLI flag and a pipe edit
    agree, and a pipe edit switches at once;
  * renders (fixed camera): each mode draws the skinned cube on the twin it should match, not on the other one;
  * persistence (GLIDER_DATA_DIR): defaults write no key; a pipe edit without set_persist writes nothing; with
    set_persist(true) it writes skinned_mesh_node_transform=msfs, a restart reads it, and Auto removes the key again.

Run: python scripts/validate_skin_node_transform.py
"""

import json
import shutil
import sys
import time

import numpy as np
from PIL import Image

from glider_harness import EXE, REPO, capture, start_pipe_session
from validate_gpu_instancing import Doc, cube_mesh

OUT = REPO / "build" / "skin_node_transform"
FIXTURES = OUT / "fixtures"
KEY = "skinned_mesh_node_transform"
# Fixed camera: the framing of the home view depends on where the geometry is, which is what this test moves.
VIEW = ["--isolated", "--window-size", "960x540", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none",
        "--camera-target", "1.5,0,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "9"]
F32 = np.float32

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


MATERIALS = [{"name": "orange", "pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.45, 0.12, 1.0],
                                                          "metallicFactor": 0.1, "roughnessFactor": 0.45}}]


def write(path, skinned, at, asobo=False):
    doc = Doc()
    mesh = cube_mesh(doc, 0, skinned=skinned)
    nodes = []
    gltf = {"asset": {"version": "2.0"}, "materials": MATERIALS, "meshes": [mesh]}
    if skinned:
        identity = np.eye(4, dtype=F32).flatten(order="F")
        ibm = doc.accessor(identity.tobytes(), 5126, 1, "MAT4")
        nodes.append({"name": "Bone"})
        nodes.append({"name": "Placed", "mesh": 0, "skin": 0, "translation": list(at)})
        gltf["skins"] = [{"joints": [0], "inverseBindMatrices": ibm}]
        roots = [0, 1]
    else:
        nodes.append({"name": "Twin", "mesh": 0, "translation": list(at)})
        roots = [0]
    gltf["nodes"] = nodes
    gltf["scenes"] = [{"nodes": roots}]
    gltf["scene"] = 0
    if asobo:
        gltf["extensionsUsed"] = ["ASOBO_material_UV_options"]
    gltf["accessors"], gltf["bufferViews"], gltf["buffers"] = doc.accessors, doc.views, doc.buffers()
    path.write_text(json.dumps(gltf), encoding="utf-8")
    return path


def fixtures():
    if FIXTURES.exists():
        shutil.rmtree(FIXTURES)
    FIXTURES.mkdir(parents=True)
    return {"placed": write(FIXTURES / "placed.gltf", True, (3, 0, 0)),
            "placed_asobo": write(FIXTURES / "placed_asobo.gltf", True, (3, 0, 0), asobo=True),
            "twin_origin": write(FIXTURES / "twin_origin.gltf", False, (0, 0, 0)),
            "twin_placed": write(FIXTURES / "twin_placed.gltf", False, (3, 0, 0))}


def skinning(pipe):
    # The mode is applied with the next pose update: give the frame loop a moment after a change.
    time.sleep(0.6)
    return pipe.call("get_state", timeout=120).get("skinning") or {}


def wait_loaded(pipe):
    for _ in range(240):
        if pipe.call("get_state", timeout=120).get("modelLoaded"):
            return True
        time.sleep(0.25)
    return False


def quit_session(process, pipe):
    try:
        pipe.call("quit")
    except Exception:
        pass
    pipe.close()
    try:
        process.wait(timeout=30)
    except Exception:
        process.kill()


def check_state(files):
    print("[get_state.skinning per mode and file; CLI flag and pipe edit]")
    expected = {"placed": {"auto": 0, "spec": 0, "msfs": 1}, "placed_asobo": {"auto": 1, "spec": 0, "msfs": 1}}
    for name, modes in expected.items():
        for mode, applied in modes.items():
            process, pipe = start_pipe_session([EXE, files[name], *VIEW, "--control-pipe", "--" + KEY.replace("_", "-"), mode])
            try:
                wait_loaded(pipe)
                state = skinning(pipe)
                check(state.get("meshNodeTransform") == mode and state.get("detected") == 1 and state.get("applied") == applied,
                      f"{name} --{KEY.replace('_', '-')} {mode}: detected 1, applied {applied} ({state})")
                if mode == "auto":
                    for live, live_applied in (("spec", 0), ("msfs", 1), ("auto", applied)):
                        pipe.call("set_viewport_setting", key=KEY, value=live)
                        state = skinning(pipe)
                        check(state.get("meshNodeTransform") == live and state.get("applied") == live_applied,
                              f"{name}: pipe edit {live} applies at once (applied {live_applied}; {state})")
                    reply = pipe.request("set_viewport_setting", {"key": KEY, "value": "strict"})
                    check(not reply.get("ok"), f"{name}: an unknown value is refused ({reply.get('error')})")
            finally:
                quit_session(process, pipe)


def load(png):
    return np.asarray(Image.open(png).convert("RGB"), dtype=np.float32)


def check_renders(files):
    print("[renders: the skinned cube lands on the twin its mode names]")
    shots = {}
    for label, model, mode in (("twin_origin", files["twin_origin"], "auto"), ("twin_placed", files["twin_placed"], "auto"),
                               ("placed_auto", files["placed"], "auto"), ("placed_spec", files["placed"], "spec"),
                               ("placed_msfs", files["placed"], "msfs"), ("asobo_auto", files["placed_asobo"], "auto"),
                               ("asobo_spec", files["placed_asobo"], "spec")):
        png, _, log = capture(EXE, model, OUT / f"{label}.png", flags=[*VIEW, "--" + KEY.replace("_", "-"), mode])
        if not check(png is not None, f"{label}: captured"):
            print(log[-2000:])
            continue
        shots[label] = load(png)
    if not {"twin_origin", "twin_placed"} <= shots.keys():
        return
    apart = float(np.abs(shots["twin_origin"] - shots["twin_placed"]).mean())
    check(apart > 0.3, f"the two twins differ clearly (mean |diff| {apart:.2f})")
    for label, twin in (("placed_auto", "twin_origin"), ("placed_spec", "twin_origin"), ("placed_msfs", "twin_placed"),
                        ("asobo_auto", "twin_placed"), ("asobo_spec", "twin_origin")):
        if label not in shots:
            continue
        other = "twin_placed" if twin == "twin_origin" else "twin_origin"
        near = float(np.abs(shots[label] - shots[twin]).mean())
        far = float(np.abs(shots[label] - shots[other]).mean())
        check(near < 0.25 * apart and far > 0.75 * apart,
              f"{label} draws the cube where {twin} has it (mean |diff| {near:.2f} to it, {far:.2f} to {other})")


def check_persistence(files):
    print("[persistence: settings.ini [GltfExtensions] skinned_mesh_node_transform]")
    data_dir = OUT / "data"
    if data_dir.exists():
        shutil.rmtree(data_dir)
    data_dir.mkdir(parents=True)
    ini = data_dir / "settings.ini"
    env = {**__import__("os").environ, "GLIDER_DATA_DIR": str(data_dir)}
    persistent = [a for a in VIEW if a != "--isolated"]

    def text():
        return ini.read_text(encoding="utf-8") if ini.exists() else ""

    process, pipe = start_pipe_session([EXE, files["placed"], *persistent, "--control-pipe"], env=env)
    try:
        wait_loaded(pipe)
        check(skinning(pipe).get("meshNodeTransform") == "auto", "default mode is auto")
        pipe.call("set_viewport_setting", key=KEY, value="spec")
        check(KEY not in text(), "a pipe edit without set_persist writes nothing")
        pipe.call("set_persist", enabled=True)
        pipe.call("set_viewport_setting", key=KEY, value="msfs")
        time.sleep(0.3)
        check(f"{KEY}=msfs" in text(), f"set_persist(true) writes {KEY}=msfs ({text().strip()!r})")
    finally:
        quit_session(process, pipe)
    check(f"{KEY}=msfs" in text(), "the key survives the shutdown save")

    process, pipe = start_pipe_session([EXE, files["placed"], *persistent, "--control-pipe"], env=env)
    try:
        wait_loaded(pipe)
        state = skinning(pipe)
        saved = pipe.call("get_state", timeout=120)["gltfExtensions"].get("savedSkinnedMeshNodeTransform")
        check(state.get("meshNodeTransform") == "msfs" and state.get("applied") == 1 and saved == "msfs",
              f"a restart reads msfs and applies it ({state}, saved {saved})")
        pipe.call("set_persist", enabled=True)
        pipe.call("set_viewport_setting", key=KEY, value="auto")
        time.sleep(0.3)
    finally:
        quit_session(process, pipe)
    check(KEY not in text(), f"auto (the default) removes the key ({text().strip()!r})")

    process, pipe = start_pipe_session([EXE, files["placed"], *VIEW, "--control-pipe", "--" + KEY.replace("_", "-"), "msfs"],
                                       env=env)
    try:
        wait_loaded(pipe)
    finally:
        quit_session(process, pipe)
    check(KEY not in text(), "the flag with --isolated writes nothing")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    files = fixtures()
    check_state(files)
    check_renders(files)
    check_persistence(files)
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
