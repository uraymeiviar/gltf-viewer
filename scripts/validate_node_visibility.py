# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""KHR_node_visibility: a node whose file value is visible = false is not rendered, and neither is its subtree.

Fixtures are written at test time into build/node_visibility/fixtures (small .gltf files with data-URI buffers,
never committed). The main scene ("hidden") holds: a visible parent with a hidden child (whose own child carries no
extension), a hidden parent with an explicitly visible child, a plain cube, a hidden transparent (BLEND) plate in
front of the visible parent, a hidden EXT_mesh_gpu_instancing node (three instances, below the model, so it would
also lower the ground's model base) and a hidden red point light (KHR_lights_punctual). Its twins:
  "removed"     the same JSON with every node the file hides (and their subtrees) removed;
  "allvisible"  every node carries the extension with visible = true;
  "noext"       the same scene without the extension at all (everything drawn);
  "undeclared"  the hidden scene without KHR_node_visibility in extensionsUsed (Glider reads only declared ones);
  "required"    the hidden scene with the extension also in extensionsRequired;
  "badtype"     visible = "no" on one node (validator).
The animated file ("anim_ptr") has a small "Blinker" cube whose visible is animated through KHR_animation_pointer
(unsigned byte 1, 0, 1, 1 at 0, 1, 2, 3 s, STEP; the captures sample between keys); its static twins carry visible = true / false and no animation. The
Blinker lies inside the bounds of the other nodes, so the startup framing does not depend on its value.

Checks (isolated, 1280x720, AA none, frame 24 of a fixed 0.05 s clock; byte for byte):
  * renders: hidden = removed (plain, sun + shadows + ground plane at the model base, SSAO, Wireframe shading,
    Wireframe Overlay), and each differs from noext; allvisible = noext; undeclared = noext;
    --disable-extension KHR_node_visibility = noext; required = removed; required + switched off is not loaded;
  * startup framing (scene extents) and frame_node without a node (frame all) equal the removed twin's;
    frame_node on a hidden node still frames it (camera differs from frame all);
  * picking: pipe pick at the screen centre of each hidden node returns what the removed twin returns there (what
    is behind, or nothing), in the same view; on noext the same pixel returns the hidden node or a node of its
    subtree (the check is not trivial);
  * draw calls of the profile report: hidden = removed < noext;
  * list_nodes: nodeVisibility false for the nodes the file hides, true for the explicit ones;
  * animation: anim_ptr at --anim-time 0.5 = the static visible twin, at 1.5 = the static hidden twin, with the
    switch off at 1.5 = the visible twin;
  * validator: no error and no extension notice for the hidden scene; KHR_NODE_VISIBILITY_INVALID_TYPE for badtype;
  * with a baseline: removed and noext render byte-identical to it; the baseline draws the hidden scene like noext.

Run: python scripts/validate_node_visibility.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import copy
import json
import sys

import numpy as np

from glider_harness import (EXE, REPO, baseline_exe, capture, capture_run, gl_errors_zero, read_json, run, same,
                            start_pipe_session)
from validate_gpu_instancing import Doc, cube_mesh

OUT = REPO / "build" / "node_visibility"
FIXTURES = OUT / "fixtures"
FLAGS = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
SUN = ["--sun", "on", "--sun-intensity", "1.8", "--sun-dir", "60,40", "--ground", "plane"]
EXT = "KHR_node_visibility"
OFF = ["--disable-extension", EXT]

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


# --------------------------------------------------------------------------------------------- fixtures

MATERIALS = [
    {"name": "opaque", "pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.45, 0.12, 1.0], "metallicFactor": 0.1,
                                                 "roughnessFactor": 0.45}},
    {"name": "blend", "alphaMode": "BLEND",
     "pbrMetallicRoughness": {"baseColorFactor": [0.15, 0.55, 0.95, 0.5], "metallicFactor": 0.0,
                              "roughnessFactor": 0.3}},
    {"name": "green", "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.75, 0.3, 1.0], "metallicFactor": 0.0,
                                                "roughnessFactor": 0.6}},
]

# The main scene as a tree: (name, properties, visible or None (no extension), children). Mesh 0 opaque cube, mesh 1
# the BLEND cube, mesh 2 the green cube.
SCENE = [
    ("ParentVisible", {"mesh": 0}, True, [
        ("HiddenChild", {"mesh": 2, "translation": [0.0, 1.6, 0.0], "scale": [0.6, 0.6, 0.6]}, False, [
            ("GrandChild", {"mesh": 0, "translation": [0.0, 0.9, 0.0], "scale": [0.8, 0.8, 0.8]}, None, []),
        ]),
    ]),
    ("ParentHidden", {"mesh": 0, "translation": [-3.0, 0.0, 0.0]}, False, [
        ("ChildOfHidden", {"mesh": 2, "translation": [0.0, 0.8, 0.0], "scale": [0.6, 0.6, 0.6]}, True, []),
    ]),
    ("Plain", {"mesh": 2, "translation": [3.0, 0.0, 0.0]}, None, []),
    ("HiddenGlass", {"mesh": 1, "translation": [0.0, 0.0, 1.4], "scale": [1.6, 1.0, 0.2]}, False, []),
    ("HiddenInstances", {"mesh": 0, "translation": [0.0, -1.8, 0.0], "instanced": True}, False, []),
    ("HiddenLight", {"light": 0, "translation": [2.0, 1.0, 2.0]}, False, []),
]
HIDDEN_NODES = ("ParentHidden", "HiddenGlass", "HiddenInstances", "HiddenChild")
# The nodes of each hidden node's subtree (its screen rectangle covers them all).
SUBTREES = {"ParentHidden": ("ParentHidden", "ChildOfHidden"), "HiddenGlass": ("HiddenGlass",),
            "HiddenInstances": ("HiddenInstances",), "HiddenChild": ("HiddenChild", "GrandChild")}


def write(path, document):
    path.write_text(json.dumps(document), encoding="utf-8")
    return path


def base_document(doc, extra_used=()):
    meshes = [cube_mesh(doc, 0), cube_mesh(doc, 1), cube_mesh(doc, 2)]
    document = {"asset": {"version": "2.0", "generator": "validate_node_visibility.py"}, "scene": 0,
                "materials": MATERIALS, "meshes": meshes,
                "extensions": {"KHR_lights_punctual": {"lights": [
                    {"type": "point", "color": [1.0, 0.1, 0.05], "intensity": 60.0, "name": "Red"}]}}}
    document["extensionsUsed"] = ["KHR_lights_punctual", "EXT_mesh_gpu_instancing", *extra_used]
    return document


def write_scene(name, mode):
    """mode: hidden, removed, allvisible, noext, undeclared, required, badtype."""
    doc = Doc()
    document = base_document(doc, () if mode in ("noext", "undeclared", "removed") else (EXT,))
    translations = np.array([[-2.0, 0.0, 0.0], [0.0, 0.0, 0.0], [2.0, 0.0, 0.0]], dtype=np.float32)
    instance_accessor = doc.accessor(translations.tobytes(), 5126, 3, "VEC3")
    nodes = []

    def add(entry):
        node_name, props, visible, children = entry
        if mode == "removed" and visible is False:
            return None
        node = {"name": node_name}
        for key, value in props.items():
            if key == "light":
                node.setdefault("extensions", {})["KHR_lights_punctual"] = {"light": value}
            elif key == "instanced":
                node.setdefault("extensions", {})["EXT_mesh_gpu_instancing"] = {
                    "attributes": {"TRANSLATION": instance_accessor}}
            else:
                node[key] = value
        if mode in ("hidden", "undeclared", "required", "badtype") and visible is not None:
            node.setdefault("extensions", {})[EXT] = {"visible": visible}
            if mode == "badtype" and node_name == "ParentHidden":
                node["extensions"][EXT] = {"visible": "no"}
        elif mode == "allvisible":
            node.setdefault("extensions", {})[EXT] = {"visible": True}
        index = len(nodes)
        nodes.append(node)
        kids = [k for k in (add(child) for child in children) if k is not None]
        if kids:
            node["children"] = kids
        return index

    roots = [r for r in (add(entry) for entry in SCENE) if r is not None]
    document["scenes"] = [{"nodes": roots}]
    document["nodes"] = nodes
    if mode == "required":
        document["extensionsRequired"] = [EXT]
    document["buffers"], document["bufferViews"], document["accessors"] = doc.buffers(), doc.views, doc.accessors
    return write(FIXTURES / f"{name}.gltf", document)


def write_anim(name, mode):
    """mode: ptr (visible animated 1, 0, 1, 1 at 0, 1, 2, 3 s), on / off (static, no animation)."""
    doc = Doc()
    document = base_document(doc, (EXT, "KHR_animation_pointer") if mode == "ptr" else (EXT,))
    document["extensionsUsed"] = [e for e in document["extensionsUsed"] if e != "EXT_mesh_gpu_instancing"]
    del document["extensions"]
    document["extensionsUsed"].remove("KHR_lights_punctual")
    nodes = [{"name": "ParentVisible", "mesh": 0},
             {"name": "Plain", "mesh": 2, "translation": [3.0, 0.0, 0.0]},
             {"name": "Blinker", "mesh": 2, "translation": [1.5, 0.0, 0.0], "scale": [0.5, 0.5, 0.5],
              "extensions": {EXT: {"visible": mode != "off"}}}]
    document["nodes"] = nodes
    document["scenes"] = [{"nodes": [0, 1, 2]}]
    if mode == "ptr":
        times = doc.accessor(np.array([0.0, 1.0, 2.0, 3.0], dtype=np.float32).tobytes(), 5126, 4, "SCALAR",
                             minmax=([0.0], [3.0]))
        values = doc.accessor(np.array([1, 0, 1, 1], dtype=np.uint8).tobytes(), 5121, 4, "SCALAR")
        document["animations"] = [{"name": "Blink", "samplers": [{"input": times, "output": values,
                                                                  "interpolation": "STEP"}],
                                   "channels": [{"sampler": 0, "target": {"path": "pointer", "extensions": {
                                       "KHR_animation_pointer": {"pointer": f"/nodes/2/extensions/{EXT}/visible"}}}}]}]
    document["buffers"], document["bufferViews"], document["accessors"] = doc.buffers(), doc.views, doc.accessors
    return write(FIXTURES / f"{name}.gltf", document)


def write_fixtures():
    FIXTURES.mkdir(parents=True, exist_ok=True)
    files = {mode: write_scene(mode, mode) for mode in
             ("hidden", "removed", "allvisible", "noext", "undeclared", "required", "badtype")}
    for mode in ("ptr", "on", "off"):
        files[f"anim_{mode}"] = write_anim(f"anim_{mode}", mode)
    return files


# --------------------------------------------------------------------------------------------- Glider runs

def render(exe, model, name, flags=(), report=True):
    """(png or None, profile report or None, log). report=False lets the candidate's capture run in a pipe session
    (glider_harness.capture_run; no profile report there)."""
    png = OUT / f"{name}.png"
    if report:
        return capture(exe, model, png, FLAGS + list(flags))
    png.unlink(missing_ok=True)
    result = capture_run([exe, model, *FLAGS, *flags, "--capture-viewport", png, "--capture-frame", "24",
                          "--profile-report", OUT / f"{name}.json", "--exit-after-profile"])
    return (png if png.exists() else None), None, result.stdout + result.stderr


def validate(model, name):
    report = OUT / f"{name}.validation.json"
    report.unlink(missing_ok=True)
    run([EXE, "--isolated", "--validate-file", model, "--validation-report", report])
    data = read_json(report)
    return None if data is None else data.get("issues", [])


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


def picked_name(reply):
    return (reply or {}).get("node") if reply else None


# --------------------------------------------------------------------------------------------- checks

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", help="reference binary (default: the newest build/baselines/*.exe)")
    args = parser.parse_args()
    baseline = baseline_exe(args.baseline_exe)
    OUT.mkdir(parents=True, exist_ok=True)
    print(f"[fixtures] candidate {EXE}")
    files = write_fixtures()
    print(f"  INFO {len(files)} generated fixtures (build/node_visibility/fixtures, not committed)")

    print("[renders: hidden nodes = the nodes removed from the JSON]")
    images, reports = {}, {}
    variants = [("plain", []), ("sun", SUN), ("ssao", ["--ssao", "on", "--ground", "plane"]),
                ("wire", ["--render-mode", "wireframe"]), ("overlay", ["--wireframe-overlay"])]
    for variant, flags in variants:
        for mode in ("hidden", "removed", "noext"):
            image, report, log = render(EXE, files[mode], f"{variant}_{mode}", flags)
            images[(variant, mode)], reports[(variant, mode)] = image, report
            check(image is not None and "Loaded successfully" in log and gl_errors_zero(log),
                  f"{variant}_{mode}: loaded, captured, zero GL errors")
        check(same(images[(variant, "hidden")], images[(variant, "removed")]),
              f"{variant}: hidden scene byte-identical to the removed-node twin")
        check(images[(variant, "hidden")] is not None and not same(images[(variant, "hidden")], images[(variant, "noext")]),
              f"{variant}: differs from the file without the extension (the hidden nodes would be drawn)")

    print("[renders: twins without hidden nodes]")
    image, _, log = render(EXE, files["allvisible"], "plain_allvisible", report=False)
    check(same(image, images[("plain", "noext")]), "every node visible = true: byte-identical to the file without it")
    image, _, log = render(EXE, files["undeclared"], "plain_undeclared", report=False)
    check(same(image, images[("plain", "noext")]),
          "not declared in extensionsUsed: ignored, byte-identical to the file without it")
    image, _, log = render(EXE, files["hidden"], "plain_switched_off", OFF)
    check(same(image, images[("plain", "noext")]) and gl_errors_zero(log),
          "--disable-extension KHR_node_visibility: byte-identical to the file without it")
    image, _, log = render(EXE, files["required"], "plain_required", report=False)
    check(same(image, images[("plain", "removed")]), "required: byte-identical to the removed-node twin")
    image, _, log = render(EXE, files["required"], "plain_required_off", OFF)
    check(image is not None and f"requires {EXT}, which is switched off" in log and "Failed to load" in log,
          "required + switched off: the file is not loaded, the log names the reason")

    print("[framing and draw calls]")
    for variant in ("plain", "sun"):
        a = (reports[(variant, "hidden")] or {}).get("camera", {})
        b = (reports[(variant, "removed")] or {}).get("camera", {})
        c = (reports[(variant, "noext")] or {}).get("camera", {})
        check(a and a.get("eye") == b.get("eye") and a.get("target") == b.get("target") and a.get("far") == b.get("far")
              and a.get("near") == b.get("near") and a.get("eye") != c.get("eye"),
              f"{variant}: startup framing (scene extents, clipping planes) equals the removed twin's, not noext's "
              f"(eye {a.get('eye')} / {b.get('eye')} / {c.get('eye')})")
    d_hidden, d_removed, d_noext = (max_draws(reports[("plain", m)]) for m in ("hidden", "removed", "noext"))
    check(d_hidden == d_removed and 0 < d_hidden < d_noext,
          f"renderer draw calls: hidden {d_hidden} = removed {d_removed} < noext {d_noext}")

    print("[pipe: frame all, frame a hidden node, pick, list_nodes]")
    results, centres, view = {}, {}, None
    for mode in ("noext", "hidden", "removed"):
        session = Session(files[mode])
        try:
            data = {"all": session.call("frame_node", margin=1.0)["camera"], "picks": {}}
            if mode == "noext":
                # The view of every pick: the file without the extension framed all (every node on screen), and the
                # centre of each (would-be hidden) node's screen rectangle there.
                view = data["all"]
                for name in HIDDEN_NODES:
                    rect = (session.call("node_screen_rect", node=name) or {}).get("screenRect")
                    if rect:
                        centres[name] = (int((rect[0] + rect[2]) / 2), int((rect[1] + rect[3]) / 2))
            else:
                session.call("set_camera", eye=view["eye"], target=view["target"], fov_y=view["fovY"],
                             near=view["near"], far=view["far"])
            for name, (x, y) in centres.items():
                data["picks"][name] = picked_name(session.call("pick", x=x, y=y))
            if mode != "removed":
                data["nodes"] = session.call("list_nodes")["nodes"]
            if mode == "hidden":
                data["frame_hidden"] = session.call("frame_node", node="ParentHidden", margin=1.0)["camera"]
            results[mode] = data
            errors = session.close()
        except Exception:
            session.process.kill()
            raise
        check(errors == 0, f"{mode} pipe session: zero GL errors ({errors})")
    results["centres"] = centres
    hidden, removed, noext = results["hidden"], results["removed"], results["noext"]
    check(hidden["all"].get("eye") == removed["all"].get("eye") and hidden["all"].get("target") == removed["all"].get("target"),
          f"frame all: the camera equals the removed twin's (eye {hidden['all'].get('eye')} / {removed['all'].get('eye')})")
    check(hidden["all"].get("eye") != noext["all"].get("eye"), "frame all: differs from noext (hidden bounds excluded)")
    check(hidden.get("frame_hidden") and hidden["frame_hidden"].get("target") != hidden["all"].get("target"),
          f"frame_node on the hidden ParentHidden still frames it (target {hidden.get('frame_hidden', {}).get('target')})")
    check(len(results.get("centres", {})) == len(HIDDEN_NODES), f"screen rectangles of the hidden nodes ({results.get('centres')})")
    for name in results.get("centres", {}):
        a, b, c = hidden["picks"].get(name), removed["picks"].get(name), noext["picks"].get(name)
        check(a == b and c in SUBTREES[name] and a not in SUBTREES[name],
              f"pick at {name}'s centre {results['centres'][name]}: {a} (removed twin {b}; noext {c})")
    by_name = {n["name"]: n for n in hidden.get("nodes", [])}
    expected = {"ParentVisible": True, "HiddenChild": False, "ParentHidden": False, "ChildOfHidden": True,
                "HiddenGlass": False, "HiddenInstances": False, "HiddenLight": False}
    got = {name: by_name.get(name, {}).get("nodeVisibility") for name in expected}
    check(got == expected and "nodeVisibility" not in by_name.get("Plain", {}) and
          "nodeVisibility" not in by_name.get("GrandChild", {}),
          f"list_nodes nodeVisibility: {got}")
    check(all("nodeVisibility" not in n for n in noext.get("nodes", [])), "noext: list_nodes has no nodeVisibility")

    print("[KHR_animation_pointer: animated visible]")
    on, _, log_on = render(EXE, files["anim_on"], "anim_on", report=False)
    off, _, log_off = render(EXE, files["anim_off"], "anim_off", report=False)
    check(on is not None and off is not None and not same(on, off), "static twins: visible and hidden Blinker differ")
    a, _, log = render(EXE, files["anim_ptr"], "anim_ptr_0.5", ["--anim-time", "0.5"])
    check(same(a, on) and gl_errors_zero(log), "anim_ptr at 0.5 s (visible = 1): byte-identical to the visible twin")
    b, _, log = render(EXE, files["anim_ptr"], "anim_ptr_1.5", ["--anim-time", "1.5"])
    check(same(b, off) and gl_errors_zero(log), "anim_ptr at 1.5 s (visible = 0): byte-identical to the hidden twin")
    c, _, log = render(EXE, files["anim_ptr"], "anim_ptr_2.5", ["--anim-time", "2.5"])
    check(same(c, on), "anim_ptr at 2.5 s (visible = 1 again): byte-identical to the visible twin")
    d, _, log = render(EXE, files["anim_ptr"], "anim_ptr_1.5_off", ["--anim-time", "1.5", *OFF])
    check(same(d, on), "anim_ptr at 1.5 s with the switch off: the channel is skipped, the Blinker is visible")

    print("[validator]")
    issues = validate(files["hidden"], "hidden")
    errors = [i.get("code") for i in issues or [] if i.get("severity") == "Error"]
    notices = [i.get("code") for i in issues or [] if str(i.get("code", "")).startswith("EXTENSION_")]
    check(issues is not None and not errors and not notices,
          f"hidden: no validation error, no extension notice ({errors + notices})")
    issues = validate(files["badtype"], "badtype")
    bad = [i for i in issues or [] if i.get("code") == "KHR_NODE_VISIBILITY_INVALID_TYPE"]
    check(len(bad) == 1 and bad[0].get("severity") == "Error" and "/nodes/" in bad[0].get("pointer", "")
          and bad[0].get("pointer", "").endswith("/extensions/KHR_node_visibility/visible"),
          f"badtype: KHR_NODE_VISIBILITY_INVALID_TYPE error at the node ({[(i.get('code'), i.get('pointer')) for i in bad]})")

    print("[baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    print(f"  INFO baseline {baseline.name}")
    for mode in ("removed", "noext"):
        before, _, _ = render(baseline, files[mode], f"base_plain_{mode}")
        check(same(before, images[("plain", mode)]), f"plain_{mode}: byte-identical to the baseline {baseline.name}")
    before, _, _ = render(baseline, files["hidden"], "base_plain_hidden")
    if same(before, images[("plain", "removed")]):
        check(same(before, images[("plain", "hidden")]), f"plain_hidden: byte-identical to the baseline {baseline.name}")
    else:
        check(same(before, images[("plain", "noext")]),
              f"the baseline {baseline.name} draws the hidden scene like the file without the extension")


if __name__ == "__main__":
    main()
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)
