# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""The live draw-call count of the Statistics panel, read through get_state's drawCalls.

Generated fixtures (written at test time into build/draw_call_stat, never committed): 1 and 6 triangles, each with its
own mesh and material so the renderer's instancing cannot merge their draws (P8.3).
  * without profiling the count is above zero (it does not depend on --profile-report);
  * it holds while the view is idle (the last redraw's count, not the idle frame's zero);
  * 5 more nodes in view give exactly 5 more draws;
  * it equals the drawCalls of the profile report's last redraw for the same file;
  * dump_draw_calls (Statistics -> Copy Draw Calls) forces a redraw on an idle view and lists every draw: as many rows as
    the count it reports and get_state's count afterwards, each glTF node once with its own material and pbr.frag.

Run: python scripts/validate_draw_call_stat.py
"""

import base64
import json
import struct
import sys
import time

from glider_harness import EXE, REPO, capture, start_pipe_session

OUT = REPO / "build" / "draw_call_stat"
FLAGS = ["--isolated", "--window-size", "960x540", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def write_fixture(count):
    positions = struct.pack("<9f", -0.4, -0.4, 0, 0.4, -0.4, 0, 0, 0.4, 0)
    doc = {
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": len(positions),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(positions).decode()}],
        "bufferViews": [{"buffer": 0, "byteLength": len(positions)}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
                       "min": [-0.4, -0.4, 0], "max": [0.4, 0.4, 0]}],
        "materials": [{"name": f"Plain{i}", "doubleSided": True,
                       "pbrMetallicRoughness": {"baseColorFactor": [1, 1 - i * 0.1, 1, 1]}} for i in range(count)],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "material": i}]} for i in range(count)],
        "nodes": [{"name": f"Tri{i}", "mesh": i, "translation": [float(i), 0, 0]} for i in range(count)],
        "scenes": [{"nodes": list(range(count))}],
        "scene": 0,
    }
    path = OUT / f"tri_{count}.gltf"
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def settled_count(model):
    """The drawCalls of a settled view, and the same value read again after more idle frames."""
    process, pipe = start_pipe_session([EXE, model, *FLAGS, "--control-pipe"])
    try:
        previous = None
        for _ in range(40):
            time.sleep(0.25)
            state = pipe.call("get_state")
            if state.get("modelLoaded") and state.get("drawCalls") == previous and previous:
                break
            previous = state.get("drawCalls")
        time.sleep(1.0)
        return previous, pipe.call("get_state").get("drawCalls")
    finally:
        pipe.call("quit")
        process.wait(timeout=60)


def check_dump(model):
    process, pipe = start_pipe_session([EXE, model, *FLAGS, "--control-pipe"])
    try:
        previous = None
        for _ in range(40):
            time.sleep(0.25)
            state = pipe.call("get_state")
            if state.get("modelLoaded") and state.get("drawCalls") == previous and previous:
                break
            previous = state.get("drawCalls")
        time.sleep(1.0)
        reply = pipe.call("dump_draw_calls")
        check(reply.get("ready") is False, "the first dump_draw_calls starts a recording")
        for _ in range(40):
            time.sleep(0.25)
            reply = pipe.call("dump_draw_calls")
            if reply.get("ready"):
                break
        check(reply.get("ready") is True, "an idle view still finishes the dump (it forces a redraw)")
        text = reply.get("text", "")
        (OUT / "dump.txt").write_text(text, encoding="utf-8")
        columns = "#\tViewport\tStage\tPass\tShader\tMaterial\tItem\tElements\tInstances"
        rows = [line.split("\t") for line in text.split(columns, 1)[-1].splitlines() if line]
        total = reply.get("total")
        check(len(rows) == total and total > 0, f"one row per draw ({len(rows)} rows, total {total})")
        check(pipe.call("get_state").get("drawCalls") == total, "get_state reports the dumped redraw's count")
        check(text.startswith(f"Glider draw calls: {total}"), "the text starts with the count")
        for i in range(6):
            own = [row for row in rows if row[6] == f"Tri{i}"]
            check(len(own) == 1 and own[0][5] == f"Plain{i}" and "pbr.frag" in own[0][4] and own[0][3],
                  f"Tri{i} has one draw in a pass, with Plain{i} and pbr.frag")
        check(pipe.call("dump_draw_calls").get("ready") is False, "a dump is returned once; the next call records again")
    finally:
        pipe.call("quit")
        process.wait(timeout=60)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    one, six = write_fixture(1), write_fixture(6)
    count_one, held = settled_count(one)
    count_six, _ = settled_count(six)
    print(f"  INFO 1 node: {count_one} draws, 6 nodes: {count_six} draws")
    check(isinstance(count_one, int) and count_one > 0, "the count is above zero without profiling")
    check(held == count_one, f"the count holds while idle ({count_one} then {held})")
    check(count_six == (count_one or 0) + 5, "5 more nodes in view add exactly 5 draws")
    _, report, _ = capture(EXE, six, OUT / "tri_6.png", FLAGS, frame=24)
    redraws = [f["counters"]["drawCalls"] for f in (report or {}).get("frames", []) if f["counters"]["drawCalls"]]
    last = redraws[-1] if redraws else None
    check(last == count_six, f"the profile report's last redraw has the same count ({last})")
    check_dump(six)
    if failures:
        print(f"RESULT: FAIL ({len(failures)} check(s))")
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
