# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Animation sampling at and around keyframes against the glTF 2.0 rules.

glTF: the keyframe interval of time t starts at the last key with time <= t. STEP returns that key's output (so
exactly on key k it is key k, not k - 1), LINEAR and CUBICSPLINE return the key value exactly on a key, before
the first key the first value holds and at / after the last key the last. CUBICSPLINE tangents are scaled by the
keyframe interval (m = (t_k+1 - t_k) * tangent) and a morph-weights CUBICSPLINE output holds per key all N
in-tangents, then the N values, then the N out-tangents.

The fixture is written at test time into build/animation_keys/fixtures (data-URI buffers, never committed). One clip
"Keys" (keys at 0, 0.5, 1.0 s) animates:
  StepNode     translation STEP (0,0,0) (1,0,0) (2,0,0);
  LinearNode   translation LINEAR and rotation LINEAR (slerp; 0, 60, 150 degrees about Y);
  CubicNode    translation CUBICSPLINE with non-zero tangents;
  Blinker      KHR_node_visibility visible through KHR_animation_pointer, unsigned byte STEP 1, 0, 1;
  MorphStep    morph weights (2 targets) STEP;
  MorphCubic   morph weights (2 targets) CUBICSPLINE;
  LateNode     translation LINEAR with keys at 0.25 and 0.5 only (before its first key / after its last key).
Exactness: every key time (0, 0.25, 0.5, 1.0) and sample time (0.25, 0.75, 1.5, ...) is a binary fraction, exact in
float32 and in the pipe's JSON doubles; values come back as the stored float32 numbers. Key-time samples are
compared with ==, between-key LINEAR with the float32 formula (1e-6), CUBICSPLINE with the float64 spec formula
(1e-5; the check also asserts that the unscaled-tangent result would be off by more than 1e-2).

Checks:
  * pipe (set_animation_time / get_node_pose): every channel at t = 0, 0.25, 0.5, 0.75, 1.0 (the clip end), 1.5
    (past the end) and -0.5 (before the start) equals the spec value;
  * the timeline at the clip end: Jump-to-end time (= duration) holds the last keys; playback that lands exactly on
    the end does not wrap; playback past the end wraps with Loop on (fmod: 1.25 -> 0.25) and stops at the end
    (last keys) with Loop off;
  * renders (--anim-time, the timeline seek of the CLI): StepNode-only file at 0.5 and at 1.0 = static twins with the
    node at keys 1 and 2; Blinker at 0.5 = the hidden twin, at 1.0 = the visible twin;
  * with a baseline: the static twins render byte-identical to it.

Run: python scripts/validate_animation_keys.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import json
import math
import sys

import numpy as np

from glider_harness import EXE, REPO, baseline_exe, capture, gl_errors_zero, same, start_pipe_session
from validate_gpu_instancing import Doc, cube_mesh

OUT = REPO / "build" / "animation_keys"
FIXTURES = OUT / "fixtures"
FLAGS = ["--isolated", "--window-size", "960x540", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
VIS = "KHR_node_visibility"
PTR = "KHR_animation_pointer"
F32 = np.float32

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


def f32(values):
    return np.array(values, dtype=F32)


def quat_y(degrees):
    half = math.radians(degrees) / 2.0
    return [0.0, math.sin(half), 0.0, math.cos(half)]


MATERIALS = [{"name": "orange", "pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.45, 0.12, 1.0],
                                                          "metallicFactor": 0.1, "roughnessFactor": 0.45}},
             {"name": "green", "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.75, 0.3, 1.0],
                                                         "metallicFactor": 0.0, "roughnessFactor": 0.6}}]

TIMES = f32([0.0, 0.5, 1.0])
STEP_T = f32([[0, 0, 0], [1, 0, 0], [2, 0, 0]])
LIN_T = f32([[0, 1, 0], [1, 2, 0], [3, 1, 0]])
LIN_R = f32([quat_y(0), quat_y(60), quat_y(150)])
CUB_T = f32([[0, 0, 0], [1, 1, 0], [2, 0, 1]])
CUB_IN = f32([[0, 0, 0], [2, -1, 0.5], [1, 1, 1]])
CUB_OUT = f32([[3, 2, 0], [-1, 0.5, 2], [0, 0, 0]])
VIS_V = np.array([1, 0, 1], dtype=np.uint8)
MS_W = f32([[0.0, 0.0], [1.0, 0.0], [0.5, 1.0]])
MC_W = f32([[0.2, 0.4], [0.8, 0.1], [0.3, 0.9]])
MC_IN = f32([[0.0, 0.0], [1.5, -2.0], [0.5, 0.5]])
MC_OUT = f32([[2.0, 1.0], [-1.0, 3.0], [0.0, 0.0]])
LATE_TIMES = f32([0.25, 0.5])
LATE_T = f32([[0, 0, 5], [0, 0, 6]])


def write(path, document):
    path.write_text(json.dumps(document), encoding="utf-8")
    return path


def cubic_output(inn, val, out):
    """Per key: in-tangent(s), value(s), out-tangent(s) (glTF CUBICSPLINE layout)."""
    return np.stack([inn, val, out], axis=1).astype(F32)


def write_keys():
    doc = Doc()
    meshes = [cube_mesh(doc, 0), cube_mesh(doc, 1), cube_mesh(doc, 0, morph=True)]
    # Two morph targets for the weight channels.
    prim = meshes[2]["primitives"][0]
    second = np.zeros((24, 3), dtype=F32)
    second[:, 2] = 0.25
    prim["targets"].append({"POSITION": doc.accessor(second.tobytes(), 5126, 24, "VEC3",
                                                     minmax=([0.0, 0.0, 0.25], [0.0, 0.0, 0.25]), target=34962)})
    meshes[2]["weights"] = [0.0, 0.0]
    nodes = [{"name": "StepNode", "mesh": 0, "scale": [0.3, 0.3, 0.3]},
             {"name": "LinearNode", "mesh": 1, "translation": [0, 1, 0], "scale": [0.3, 0.3, 0.3]},
             {"name": "CubicNode", "mesh": 0, "scale": [0.3, 0.3, 0.3]},
             {"name": "Blinker", "mesh": 1, "translation": [0, -1, 0], "scale": [0.3, 0.3, 0.3],
              "extensions": {VIS: {"visible": True}}},
             {"name": "MorphStep", "mesh": 2, "translation": [-2, 0, 0], "scale": [0.3, 0.3, 0.3]},
             {"name": "MorphCubic", "mesh": 2, "translation": [-2, 1, 0], "scale": [0.3, 0.3, 0.3]},
             {"name": "LateNode", "mesh": 1, "translation": [0, 0, 5], "scale": [0.3, 0.3, 0.3]}]
    times = doc.accessor(TIMES.tobytes(), 5126, 3, "SCALAR", minmax=([0.0], [1.0]))
    late = doc.accessor(LATE_TIMES.tobytes(), 5126, 2, "SCALAR", minmax=([0.25], [0.5]))
    samplers, channels = [], []

    def add(node, path, input_accessor, payload, component, count, kind, interpolation, pointer=None):
        output = doc.accessor(payload.tobytes(), component, count, kind)
        samplers.append({"input": input_accessor, "output": output, "interpolation": interpolation})
        target = {"node": node, "path": path}
        if pointer is not None:
            target = {"path": "pointer", "extensions": {PTR: {"pointer": pointer}}}
        channels.append({"sampler": len(samplers) - 1, "target": target})

    add(0, "translation", times, STEP_T, 5126, 3, "VEC3", "STEP")
    add(1, "translation", times, LIN_T, 5126, 3, "VEC3", "LINEAR")
    add(1, "rotation", times, LIN_R, 5126, 3, "VEC4", "LINEAR")
    add(2, "translation", times, cubic_output(CUB_IN, CUB_T, CUB_OUT).reshape(-1, 3), 5126, 9, "VEC3", "CUBICSPLINE")
    add(3, None, times, VIS_V, 5121, 3, "SCALAR", "STEP", pointer=f"/nodes/3/extensions/{VIS}/visible")
    add(4, "weights", times, MS_W.reshape(-1), 5126, 6, "SCALAR", "STEP")
    add(5, "weights", times, cubic_output(MC_IN, MC_W, MC_OUT).reshape(-1), 5126, 18, "SCALAR", "CUBICSPLINE")
    add(6, "translation", late, LATE_T, 5126, 2, "VEC3", "LINEAR")
    document = {"asset": {"version": "2.0", "generator": "validate_animation_keys.py"}, "scene": 0,
                "extensionsUsed": [VIS, PTR], "materials": MATERIALS, "meshes": meshes, "nodes": nodes,
                "scenes": [{"nodes": list(range(len(nodes)))}],
                "animations": [{"name": "Keys", "samplers": samplers, "channels": channels}]}
    document["buffers"], document["bufferViews"], document["accessors"] = doc.buffers(), doc.views, doc.accessors
    return write(FIXTURES / "keys.gltf", document)


def write_render(name, mode):
    """mode: step (StepNode STEP translation; Blinker visible STEP), or a static twin: ("static", translation,
    visible). Two fixed anchor cubes set the scene bounds, so the startup framing does not depend on the animated
    nodes (a hidden node leaves the framing bounds)."""
    doc = Doc()
    meshes = [cube_mesh(doc, 0), cube_mesh(doc, 1)]
    animated = mode == "step"
    translation, visible = ([0.0, 0.0, 0.0], True) if animated else (mode[1], mode[2])
    nodes = [{"name": "AnchorLow", "mesh": 1, "translation": [-1.0, -1.5, 0.0]},
             {"name": "AnchorHigh", "mesh": 1, "translation": [3.0, 1.5, 0.0]},
             {"name": "StepNode", "mesh": 0, "translation": translation, "scale": [0.6, 0.6, 0.6]},
             {"name": "Blinker", "mesh": 1, "translation": [1.0, 1.2, 0.0], "scale": [0.5, 0.5, 0.5],
              "extensions": {VIS: {"visible": visible}}}]
    document = {"asset": {"version": "2.0", "generator": "validate_animation_keys.py"}, "scene": 0,
                "extensionsUsed": [VIS, PTR] if animated else [VIS], "materials": MATERIALS, "meshes": meshes,
                "nodes": nodes, "scenes": [{"nodes": [0, 1, 2, 3]}]}
    if animated:
        times = doc.accessor(TIMES.tobytes(), 5126, 3, "SCALAR", minmax=([0.0], [1.0]))
        out_t = doc.accessor(STEP_T.tobytes(), 5126, 3, "VEC3")
        out_v = doc.accessor(VIS_V.tobytes(), 5121, 3, "SCALAR")
        document["animations"] = [{"name": "Step", "samplers": [
            {"input": times, "output": out_t, "interpolation": "STEP"},
            {"input": times, "output": out_v, "interpolation": "STEP"}], "channels": [
            {"sampler": 0, "target": {"node": 2, "path": "translation"}},
            {"sampler": 1, "target": {"path": "pointer", "extensions": {PTR: {
                "pointer": f"/nodes/3/extensions/{VIS}/visible"}}}}]}]
    document["buffers"], document["bufferViews"], document["accessors"] = doc.buffers(), doc.views, doc.accessors
    return write(FIXTURES / f"{name}.gltf", document)


# --------------------------------------------------------------------------------------------- spec values

def interval(times, t):
    """(k, u) of the glTF keyframe interval containing t, or (index, None) when t is before / at / after a key."""
    if t <= times[0]:
        return 0, None
    if t >= times[-1]:
        return len(times) - 1, None
    k = int(np.searchsorted(times, F32(t), side="right")) - 1
    if F32(t) == times[k]:
        return k, None
    return k, (F32(t) - times[k]) / (times[k + 1] - times[k])


def step_value(times, values, t):
    k, _ = interval(times, t)
    return values[k]


def linear_value(times, values, t):
    k, u = interval(times, t)
    if u is None:
        return values[k], True
    u = F32(u)
    return (F32(1.0) - u) * values[k] + u * values[k + 1], False


def slerp_value(times, values, t):
    k, u = interval(times, t)
    if u is None:
        return values[k].astype(np.float64), True
    a, b = values[k].astype(np.float64), values[k + 1].astype(np.float64)
    d = float(np.dot(a, b))
    if d < 0:
        b, d = -b, -d
    theta = math.acos(min(1.0, d))
    s = math.sin(theta)
    return (math.sin((1 - u) * theta) / s) * a + (math.sin(u * theta) / s) * b, False


def cubic_value(times, inn, val, out, t, scale=True):
    k, u = interval(times, t)
    if u is None:
        return val[k].astype(np.float64), True
    u = float(u)
    dt = float(times[k + 1] - times[k]) if scale else 1.0
    p0, p1 = val[k].astype(np.float64), val[k + 1].astype(np.float64)
    m0, m1 = dt * out[k].astype(np.float64), dt * inn[k + 1].astype(np.float64)
    u2, u3 = u * u, u * u * u
    return (2 * u3 - 3 * u2 + 1) * p0 + (u3 - 2 * u2 + u) * m0 + (-2 * u3 + 3 * u2) * p1 + (u3 - u2) * m1, False


# --------------------------------------------------------------------------------------------- checks

def compare(label, got, expected, exact, tolerance):
    got = np.array(got, dtype=np.float64)
    expected = np.array(expected, dtype=np.float64)
    if got.shape != expected.shape:
        return check(False, f"{label}: {got.tolist()} (expected {expected.tolist()})")
    if exact:
        ok = bool(np.all(got == expected))
    else:
        ok = bool(np.all(np.abs(got - expected) <= tolerance))
    return check(ok, f"{label}: {np.round(got, 6).tolist()} (spec {np.round(expected, 6).tolist()}"
                     f"{', exact' if exact else f', tol {tolerance}'})")


def quat_compare(label, got, expected, exact):
    got = np.array(got, dtype=np.float64)
    if exact:
        return compare(label, got, expected, True, 0)
    if np.dot(got, expected) < 0:
        got = -got
    return compare(label, got, expected, False, 1e-6)


def sample_all(session, t):
    session.call("set_animation_time", time=t)
    return {name: session.call("get_node_pose", node=name) for name in
            ("StepNode", "LinearNode", "CubicNode", "Blinker", "MorphStep", "MorphCubic", "LateNode")}


def check_samples(poses, t, tag=""):
    at = f"{tag}t = {t}"
    # Clamped like the clip (Animation::updateChannels clamps to [0, duration]).
    tc = min(max(t, 0.0), 1.0)
    compare(f"{at}: StepNode translation (STEP)", poses["StepNode"]["translation"], step_value(TIMES, STEP_T, tc),
            True, 0)
    value, exact = linear_value(TIMES, LIN_T, tc)
    compare(f"{at}: LinearNode translation (LINEAR)", poses["LinearNode"]["translation"], value, exact, 1e-6)
    value, exact = slerp_value(TIMES, LIN_R, tc)
    quat_compare(f"{at}: LinearNode rotation (LINEAR slerp)", poses["LinearNode"]["rotation"], value, exact)
    value, exact = cubic_value(TIMES, CUB_IN, CUB_T, CUB_OUT, tc)
    compare(f"{at}: CubicNode translation (CUBICSPLINE)", poses["CubicNode"]["translation"], value, exact, 1e-5)
    if not exact:
        unscaled, _ = cubic_value(TIMES, CUB_IN, CUB_T, CUB_OUT, tc, scale=False)
        check(float(np.max(np.abs(unscaled - value))) > 1e-2,
              f"{at}: CUBICSPLINE tangent scaling is observable here (unscaled differs by "
              f"{float(np.max(np.abs(unscaled - value))):.4f})")
    expected_visible = bool(step_value(TIMES, VIS_V, tc))
    check(poses["Blinker"].get("visibleNow") is expected_visible,
          f"{at}: Blinker visible (KHR_animation_pointer STEP) = {poses['Blinker'].get('visibleNow')} "
          f"(spec {expected_visible})")
    compare(f"{at}: MorphStep weights (STEP)", poses["MorphStep"]["weights"], step_value(TIMES, MS_W, tc), True, 0)
    value, exact = cubic_value(TIMES, MC_IN, MC_W, MC_OUT, tc)
    compare(f"{at}: MorphCubic weights (CUBICSPLINE)", poses["MorphCubic"]["weights"], value, exact, 1e-5)
    value, exact = linear_value(LATE_TIMES, LATE_T, tc)
    compare(f"{at}: LateNode translation (keys 0.25 / 0.5)", poses["LateNode"]["translation"], value, exact, 1e-6)


def pipe_checks(model):
    print("[pipe: set_animation_time / get_node_pose]")
    process, pipe = start_pipe_session([EXE, model, *FLAGS, "--control-pipe"])
    try:
        try:
            state = pipe.call("set_animation_time", time=0.0)
        except Exception as error:  # an exe without the command
            check(False, f"set_animation_time is available ({error})")
            return
        check(state.get("duration") == 1.0 and state.get("playing") is False,
              f"timeline: duration 1.0, paused ({state})")
        for t in (0.0, 0.25, 0.5, 0.75, 1.0, 1.5, -0.5):
            check_samples(sample_all(pipe, t), t)

        print("[timeline at the clip end]")
        state = pipe.call("set_animation_time", time=state.get("duration", 1.0))
        check(state.get("time") == 1.0, f"Jump to end: time = duration ({state.get('time')})")
        check_samples({n: pipe.call("get_node_pose", node=n) for n in
                       ("StepNode", "LinearNode", "CubicNode", "Blinker", "MorphStep", "MorphCubic", "LateNode")},
                      1.0, "end: ")
        state = pipe.call("set_animation_time", time=0.5, advance=0.5, loop=True)
        check(state.get("time") == 1.0, f"Loop on, playback landing exactly on the end: no wrap (time {state.get('time')})")
        pose = pipe.call("get_node_pose", node="StepNode")
        compare("Loop on, at the end: StepNode = last key", pose["translation"], STEP_T[2], True, 0)
        state = pipe.call("set_animation_time", advance=0.25)
        check(state.get("time") == 0.25, f"Loop on, playback past the end wraps: 1.25 -> {state.get('time')}")
        check_samples({n: pipe.call("get_node_pose", node=n) for n in
                       ("StepNode", "LinearNode", "CubicNode", "Blinker", "MorphStep", "MorphCubic", "LateNode")},
                      0.25, "wrapped: ")
        state = pipe.call("set_animation_time", time=0.75, advance=0.5, loop=False)
        check(state.get("time") == 1.0 and state.get("playing") is False,
              f"Loop off, playback past the end: stops at the end ({state})")
        pose = pipe.call("get_node_pose", node="StepNode")
        compare("Loop off, stopped at the end: StepNode = last key", pose["translation"], STEP_T[2], True, 0)
        check(pipe.call("get_state").get("glErrors") == 0, "no GL errors in the pipe session")
        pipe.call("quit")
    finally:
        pipe.close()
        try:
            process.wait(60)
        except Exception:
            process.kill()


def render(exe, model, name, flags=()):
    png, _, log = capture(exe, model, OUT / f"{name}.png", FLAGS + list(flags))
    return png, log


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", help="reference binary (default: the newest build/baselines/*.exe)")
    args = parser.parse_args()
    baseline = baseline_exe(args.baseline_exe)
    FIXTURES.mkdir(parents=True, exist_ok=True)
    print(f"[fixtures] candidate {EXE}")
    keys = write_keys()
    files = {"step": write_render("render_step", "step"),
             "key1_hidden": write_render("render_key1", ("static", [1.0, 0.0, 0.0], False)),
             "key2_visible": write_render("render_key2", ("static", [2.0, 0.0, 0.0], True))}
    print("  INFO generated fixtures in build/animation_keys/fixtures (not committed)")

    pipe_checks(keys)

    print("[renders: --anim-time on a key = the static twin of that key]")
    twin1, log1 = render(EXE, files["key1_hidden"], "twin_key1")
    twin2, log2 = render(EXE, files["key2_visible"], "twin_key2")
    check(twin1 is not None and twin2 is not None and not same(twin1, twin2) and gl_errors_zero(log1)
          and gl_errors_zero(log2),
          "static twins render and differ")
    a, log = render(EXE, files["step"], "step_at_0.5", ["--anim-time", "0.5"])
    check(same(a, twin1) and gl_errors_zero(log),
          "t = 0.5 (key 1): StepNode at (1,0,0) and Blinker hidden, byte-identical to the key-1 twin")
    b, log = render(EXE, files["step"], "step_at_1.0", ["--anim-time", "1.0"])
    check(same(b, twin2) and gl_errors_zero(log),
          "t = 1.0 (key 2, the clip end): StepNode at (2,0,0) and Blinker visible, byte-identical to the key-2 twin")
    c, log = render(EXE, files["step"], "step_at_0.75", ["--anim-time", "0.75"])
    check(same(c, twin1), "t = 0.75 (between keys 1 and 2): byte-identical to the key-1 twin")

    print("[baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    print(f"  INFO baseline {baseline.name}")
    for key, png in (("key1_hidden", twin1), ("key2_visible", twin2)):
        before, _ = render(baseline, files[key], f"base_{key}")
        check(same(before, png), f"static twin {key}: byte-identical to the baseline {baseline.name}")


if __name__ == "__main__":
    main()
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)
