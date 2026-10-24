# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P5.3 cached scene redraw validation.

A viewport whose render key (layer, scene, every revision counter, camera
matrices, size/samples/AA, the whole settings struct, camera attachment, AA
target generation) is unchanged shows its last image instead of redrawing.
Clicks still redraw (picking). TAA keeps accumulating for 64 unchanged renders,
then holds its image.

Sections:
  equivalence  Each UI script runs three times: the pre-P5.3 binary
               (build/p5.3/pre/Glider.exe, i.e. P5.2), the candidate with
               --disable-redraw-skip, and the candidate. Screenshots after
               every step must be byte-identical; the candidate must redraw
               after every change (changes are made through UI controls and
               through direct assignments that mark no revision) and must not
               redraw on idle frames.
  taa          The held TAA image must be one the every-frame reference
               displayed; redraw resumes after a change and a history reset.
  benchmark    Idle CPU/GPU frame on the 1,600-drawable fixture; playback of
               the leaf-animated grid (redraws every frame) as the control.
"""

import argparse
import hashlib
import json
import statistics
import harness_process as subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
PRE_EXE = REPO / "build" / "p5.3" / "pre" / "Glider.exe"
OUT = REPO / "build" / "p5.3"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
HIER = "resources/test_models/hierarchy_animation_fixture.gltf"
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
MANY = "build/p5.3/fixtures/many_drawables.gltf"
LEAF = "build/p5.3/fixtures/many_leaf.gltf"
BEHIND = ["--camera-yaw", "160", "--camera-pitch", "10"]
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["equivalence", "taa", "benchmark"])
parser.add_argument("--pairs", type=int, default=5)
parser.add_argument("--exe", help="candidate binary (default: build/bin/Release/Glider.exe)")
parser.add_argument("--scenario", help="run only the equivalence scenario with this name")
args = parser.parse_args()
if args.exe:
    EXE = Path(args.exe).resolve()
failures = []


def section(name):
    return args.only in (None, name)


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


for out, variant in ((MANY, "none"), (LEAF, "leaf")):
    subprocess.run([sys.executable, str(REPO / "scripts" / "generate_many_drawables_fixture.py"), str(REPO / out), "40", variant],
                   check=True, capture_output=True)


def run(mode, exe, name, text, extra):
    folder = OUT / mode
    folder.mkdir(parents=True, exist_ok=True)
    text = text.replace("@OUT@", folder.relative_to(REPO).as_posix())
    shots = [line.split(" ", 1)[1] for line in text.splitlines() if line.startswith("shot ")]
    for shot in shots:
        (REPO / shot).unlink(missing_ok=True)
    scenario, report = folder / f"{name}.txt", folder / f"{name}.json"
    scenario.write_text(text, encoding="utf-8")
    report.unlink(missing_ok=True)
    flags = ["--disable-redraw-skip"] if mode == "noskip" else []
    result = subprocess.run([str(exe)] + extra + COMMON + flags + ["--ui-script", str(scenario), "--ui-report", str(report)],
                            cwd=REPO, capture_output=True, text=True, timeout=300)
    if result.returncode != 0 or not report.exists():
        check(False, f"{name} [{mode}]: exit {result.returncode} {result.stderr[-300:]}")
        return None
    data = json.loads(report.read_text(encoding="utf-8"))
    if data.get("failed", True):
        check(False, f"{name} [{mode}]: script errors {data.get('errors')}")
        return None
    return {"snaps": {s["label"]: s for s in data["snaps"]}, "order": [s["label"] for s in data["snaps"]],
            "shots": {Path(p).name: REPO / p for p in shots}}


def renders(snap, vp=1):
    return next(v["renderCount"] for v in snap["viewports"] if v["id"] == vp)


def steps_script(steps, settle=12):
    """steps: (label, commands, visible). Each step settles, then snaps and shoots."""
    lines = []
    for label, cmds, _ in steps:
        lines += cmds + [f"wait {settle}", f"snap {label}", f"shot @OUT@/{label}.png"]
    return lines


def equivalence(name, lines, extra, steps=(), idle=(), vp_checks=(), first="start"):
    if args.scenario and args.scenario != name:
        return None
    text = "\n".join(lines + ["wait 2"]) + "\n"
    pre = run("pre", PRE_EXE, name, text, extra)
    nos = run("noskip", EXE, name, text, extra)
    cand = run("candidate", EXE, name, text, extra)
    if not (pre and nos and cand):
        return None
    for label, snap in cand["snaps"].items():
        same = all((o["snaps"][label]["selection"], o["snaps"][label]["camera"]) == (snap["selection"], snap["camera"])
                   for o in (pre, nos))
        check(same, f"{name}/{label}: selection and camera match pre and --disable-redraw-skip")
    for shot, path in cand["shots"].items():
        h = sha(path)
        check(h == sha(pre["shots"][shot]) and h == sha(nos["shots"][shot]),
              f"{name}/{shot}: byte-identical to pre-P5.3 and to --disable-redraw-skip")
    snaps = cand["snaps"]
    previous = first
    for label, _, visible in steps:
        drawn = renders(snaps[label]) - renders(snaps[previous])
        check(drawn >= 1, f"{name}/{label}: the change was redrawn ({drawn} renders)")
        if visible:
            check(sha(cand["shots"][f"{label}.png"]) != sha(cand["shots"][f"{previous}.png"]),
                  f"{name}/{label}: change is visible")
        previous = label
    for before, after in idle:
        for vp in (v["id"] for v in snaps[after]["viewports"] if v["open"]):
            drawn = renders(snaps[after], vp) - renders(snaps[before], vp)
            ref = renders(nos["snaps"][after], vp) - renders(nos["snaps"][before], vp)
            check(drawn == 0 and ref > 0, f"{name} {before}->{after}: viewport {vp} idle, 0 redraws ({drawn}; every-frame reference {ref})")
    for before, after, vp, expect in vp_checks:
        drawn = renders(snaps[after], vp) - renders(snaps[before], vp)
        check((drawn > 0) == expect, f"{name} {before}->{after}: viewport {vp} {'redrawn' if expect else 'not redrawn'} ({drawn})")
    nlast = nos["snaps"][nos["order"][-1]]["revisions"]
    check(nlast["sceneSkips"] == 0, f"{name}: --disable-redraw-skip never skips ({nlast['sceneSkips']})")
    return cand


if section("equivalence"):
    print("[production fixture: changes through toolbar, camera and direct assignments that mark no revision]")
    steps = [
        ("grid_btn", ["click 323 60"], True),          # toolbar grid toggle: no revision
        ("grid_btn_on", ["click 323 60"], True),
        ("bbox_btn", ["click 351 60"], False),          # bbox toggle: no revision (no selection, scene box shows)
        ("ortho", ["click 296 60"], True),              # orthographic toggle
        ("persp", ["click 296 60"], True),
        ("wheel", ["move 650 400", "wait 2", "wheel 2"], True),
        ("compass_iso", ["click 968 120"], True),       # compass hub: isometric
        ("pick", ["click 650 418"], True),              # selection: OBB drawn into the scene image
        ("deselect", ["click 400 700"], True),
        ("set_grid", ["vpsetting 1 grid off"], True),   # direct assignments, no revision marked
        ("set_bbox", ["vpsetting 1 bbox on"], True),
        ("set_overlay", ["vpsetting 1 wireframe on"], True),
        ("set_overlay_off", ["vpsetting 1 wireframe off"], True),
        ("set_invert", ["vpsetting 1 invertwinding on"], False),
        ("aa_fxaa", ["aamode fxaa"], True),
        ("aa_msaa4", ["aamode msaa4"], True),
        ("aa_none", ["aamode none"], True),
        ("resize", ["resize 1100 700"], True),
        ("resize_back", ["resize 1280 800"], True),
    ]
    lines = ["wait 10", "snap idle0", "wait 30", "snap start", "shot @OUT@/start.png"] + steps_script(steps) + \
            ["wait 30", "snap idle2"]
    equivalence("controls", lines, [FIXTURE, "--aa", "none"] + BEHIND, steps=steps, first="start",
                idle=[("idle0", "start"), (steps[-1][0], "idle2")])

    print("[hierarchy fixture: playback, seek, attached glTF camera]")
    steps = [
        ("play", ["animplay on", "wait 10"], True),
        ("paused", ["animplay off"], False),
        ("seek", ["click 750 783"], True),
        # Status-bar Stop / End / Rewind apply a pose without marking a revision (P4.4 audit).
        ("stop_btn", ["click 155 783"], True),
        ("end_btn", ["click 187 783"], True),
        ("rewind_btn", ["click 87 783"], True),
        ("attach", ["click 815 60", "wait 6", "click 885 95"], True),
        ("attached_play", ["animplay on", "wait 10"], True),
        ("attached_paused", ["animplay off"], False),
        ("reload", [f"reload {HIER}"], True),
    ]
    lines = ["wait 8", "snap start", "shot @OUT@/start.png"] + steps_script(steps) + ["wait 30", "snap idle"]
    equivalence("hierarchy", lines, [HIER, "--aa", "none", "--camera-distance", "10", "--camera-yaw", "20",
                                     "--camera-pitch", "15"], steps=steps, idle=[("reload", "idle")])

    print("[skinned Fox: playback redraws every tick; pause holds]")
    lines = ["wait 8", "animplay on", "wait 12", "snap play", "shot @OUT@/fox_play.png", "animplay off", "wait 6",
             "snap paused", "wait 30", "snap paused_idle", "shot @OUT@/fox_paused.png"]
    cand = equivalence("fox", lines, [FOX, "--aa", "none"], idle=[("paused", "paused_idle")])

    print("[two viewports: hidden tab changed while hidden; side by side, change in one]")
    lines = ["wait 8", "vpcreate", "wait 10",                        # viewport 2 tabbed over viewport 1
             "vpsetting 1 grid off", "vpsetting 1 wireframe on",     # viewport 1 changed while hidden
             "wait 10", "snap hidden_changed", "vpfocus 1", "wait 10", "snap vp1_shown", "shot @OUT@/vp1_shown.png",
             "vpsplit 2", "wait 10", "snap split", "wait 20", "snap split_idle", "shot @OUT@/split.png",
             "vpsetting 2 wireframe on", "wait 10", "snap vp2_changed", "shot @OUT@/vp2_changed.png",
             "wait 20", "snap split_idle2"]
    equivalence("multi", lines, [FIXTURE, "--aa", "none"] + BEHIND,
                idle=[("split", "split_idle"), ("vp2_changed", "split_idle2")],
                vp_checks=[("split_idle", "vp2_changed", 2, True), ("split_idle", "vp2_changed", 1, False)])


if section("taa"):
    print("[TAA: accumulation, hold, change and reset]")
    # The reference redraws every frame: shoot frames 40..120 after load, one per frame.
    ref_lines = ["wait 40"]
    for i in range(80):
        ref_lines += [f"shot @OUT@/ref_{i:03d}.png", "wait 1"]
    ref = run("noskip", EXE, "taa_ref", "\n".join(ref_lines) + "\n", [FIXTURE, "--aa", "taa"] + BEHIND)
    cand_lines = ["wait 40", "snap early", "shot @OUT@/early.png", "wait 160", "snap held", "shot @OUT@/held.png",
                  "wait 40", "snap held2", "shot @OUT@/held2.png",
                  "vpsetting 1 grid off", "wait 10", "snap changed", "shot @OUT@/changed.png",
                  "wait 120", "snap changed_held",
                  "click 968 120", "wait 10", "snap reset", "wait 120", "snap reset_held", "wait 2"]
    cand = run("candidate", EXE, "taa", "\n".join(cand_lines) + "\n", [FIXTURE, "--aa", "taa"] + BEHIND)
    nos = run("noskip", EXE, "taa_noskip", "\n".join(cand_lines) + "\n", [FIXTURE, "--aa", "taa"] + BEHIND)
    if ref and cand and nos:
        refs = {sha(p): name for name, p in ref["shots"].items()}
        check(sha(cand["shots"]["early.png"]) == sha(nos["shots"]["early.png"]),
              "TAA before the hold: frame 40 byte-identical to the same script with --disable-redraw-skip")
        held = sha(cand["shots"]["held.png"])
        check(held in refs, f"TAA held image is a frame the every-frame reference displayed ({refs.get(held)})")
        check(held == sha(cand["shots"]["held2.png"]), "TAA image stays held")
        s = cand["snaps"]
        check(renders(s["held2"]) == renders(s["held"]), f"TAA held: 0 redraws over 40 frames")
        check(renders(s["held"]) - renders(s["early"]) < 160, "TAA stopped redrawing during the hold")
        drawn = renders(s["changed_held"]) - renders(s["held2"])
        check(64 <= drawn <= 80, f"TAA after a change: accumulates again, then holds ({drawn} renders)")
        drawn = renders(s["reset_held"]) - renders(s["changed_held"])
        check(64 <= drawn <= 80, f"TAA after a history reset (compass): accumulates again, then holds ({drawn} renders)")
        import struct, zlib  # noqa: E401 - small PNG reader for a difference report

        def pixels(path):
            data = Path(path).read_bytes()
            pos, chunks = 8, []
            width = height = 0
            while pos < len(data):
                length = struct.unpack(">I", data[pos:pos + 4])[0]
                kind = data[pos + 4:pos + 8]
                body = data[pos + 8:pos + 8 + length]
                if kind == b"IHDR":
                    width, height = struct.unpack(">II", body[:8])
                    depth, ctype = body[8], body[9]
                elif kind == b"IDAT":
                    chunks.append(body)
                pos += 12 + length
            raw = zlib.decompress(b"".join(chunks))
            bpp = {2: 3, 6: 4}[ctype]
            stride = width * bpp
            out, prev = bytearray(), bytearray(stride)
            for y in range(height):
                f = raw[y * (stride + 1)]
                line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
                for x in range(stride):
                    a = line[x - bpp] if x >= bpp else 0
                    b = prev[x]
                    c = prev[x - bpp] if x >= bpp else 0
                    if f == 1: line[x] = (line[x] + a) & 255
                    elif f == 2: line[x] = (line[x] + b) & 255
                    elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
                    elif f == 4:
                        pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                        line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
                out += line
                prev = line
            return out

        a, b = pixels(cand["shots"]["held.png"]), pixels(nos["shots"]["held.png"])
        diff = [abs(x - y) for x, y in zip(a, b)]
        changed = sum(1 for d in diff if d) / len(diff)
        print(f"    held vs every-frame reference at the same frame: max channel diff {max(diff)}, "
              f"mean {sum(diff) / len(diff):.4f}, changed channels {changed:.4%}")
        r0, r1 = pixels(ref["shots"]["ref_078.png"]), pixels(ref["shots"]["ref_079.png"])
        rdiff = [abs(x - y) for x, y in zip(r0, r1)]
        print(f"    every-frame reference, consecutive frames: max channel diff {max(rdiff)}, mean {sum(rdiff) / len(rdiff):.4f}")


def bench(exe, label, index, model, script):
    folder = OUT / "bench"
    folder.mkdir(parents=True, exist_ok=True)
    tag = Path(model).stem
    out = folder / f"{tag}_{label}_{index}.json"
    out.unlink(missing_ok=True)
    extra = []
    if script:
        s = folder / f"{tag}.txt"
        s.write_text(script, encoding="utf-8")
        extra = ["--ui-script", str(s), "--ui-report", str(folder / f"{tag}_{label}_{index}_ui.json")]
    subprocess.run([str(exe), model, "--isolated", "--window-size", "1280x800", "--aa", "none", "--vsync", "off",
                    "--profile", "--benchmark", "--benchmark-warmup", "120", "--benchmark-frames", "300",
                    "--benchmark-output", str(out), "--exit-after-benchmark"] + extra,
                   cwd=REPO, capture_output=True, text=True, timeout=600)
    data = json.loads(out.read_text(encoding="utf-8"))
    frames = data["measuredFrames"]
    measured = {f["frame"] for f in frames}
    gpu = {}
    for sample in data["gpu"]["completed"]:
        if sample["frame"] in measured:
            gpu[sample["frame"]] = gpu.get(sample["frame"], 0.0) + sum(p["ms"] for p in sample["passes"] if p.get("available"))
    # Frames without a GPU sample drew no scene pass: 0 ms.
    gpu_frames = [gpu.get(f, 0.0) for f in measured]
    return {"cpu": statistics.median(f["cpuFrameMs"] for f in frames),
            "gpu": statistics.mean(gpu_frames)}


if section("benchmark"):
    for model, script, gate in ((MANY, None, "win"), (LEAF, "wait 4\nanimplay on\nwait 2000\n", "control")):
        print(f"[benchmark {Path(model).stem}{' (playing)' if script else ' (idle)'}: {args.pairs} alternating pairs]")
        pre, cand = [], []
        for i in range(args.pairs):
            order = [("pre", PRE_EXE), ("candidate", EXE)] if i % 2 == 0 else [("candidate", EXE), ("pre", PRE_EXE)]
            res = {label: bench(exe, label, i, model, script) for label, exe in order}
            pre.append(res["pre"]); cand.append(res["candidate"])
            print(f"    pair {i}: cpu {res['pre']['cpu']:.3f} -> {res['candidate']['cpu']:.3f} ms, "
                  f"gpu mean {res['pre']['gpu']:.3f} -> {res['candidate']['gpu']:.3f} ms")
        for key in ("cpu", "gpu"):
            p, c = [r[key] for r in pre], [r[key] for r in cand]
            noise = (max(p) - min(p)) / statistics.median(p)
            deltas = [(a - b) / a for a, b in zip(p, c)]
            med = statistics.median(deltas)
            print(f"    {key}: pre {min(p):.3f}-{max(p):.3f} -> cand {min(c):.3f}-{max(c):.3f} ms, "
                  f"median paired {med:+.1%} improvement, {sum(d > 0 for d in deltas)}/{len(deltas)}, noise {noise:.1%}")
            if gate == "win":
                check(med > max(0.03, noise) and sum(d > 0 for d in deltas) >= len(deltas) - 1, f"idle {key}: improvement exceeds 3% and noise")
            else:
                check(med > -max(0.05, noise), f"playing {key}: no regression beyond 5% and noise")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
