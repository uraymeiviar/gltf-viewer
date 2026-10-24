# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P4.3 playback / initial-load decoupling validation.

Playback timing is wall-clock based, so the accepted baseline binary cannot be
compared mid-playback. Instead every scenario runs the same UI script twice on
the candidate with --fixed-frame-time (deterministic playback):
  * reference: --force-full-sync (P4.1 mapping: every pending change rebuilds),
  * candidate: default mapping.
Both must produce byte-identical full-window screenshots and identical
selection/camera/AnimationTime state. Counters must show the candidate applies
the pose without the full rebuild during playback, while clip-set changes and
reloads still rebuild.

P4.3b (initial load) is compared with the accepted Phase 3 binary, which
rebuilt again on the first frame after every load:
  load    startup loads: first presented frame and frame 30 byte-identical,
          candidate performs no extra rebuild (unless another kind is pending).
  reload  scripted reloads A -> B -> A and a reload while attached to a glTF
          camera: snapshots and screenshots identical, no rebuild after reload.

Sections: selfcheck, pointer, pointer_taa_mv, fox, morph, load, reload.
"""

import argparse
import hashlib
import json
import harness_process as subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "p4.3"
POINTER = "resources/test_models/pointer_animation_fixture.gltf"
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
DUCK = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Duck/glTF-Binary/Duck.glb"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
BASE_EXE = REPO / "build" / "p4.1" / "baseline" / "Glider.exe"  # accepted Phase 3 final binary
MORPH = "OpenGL-Renderer/Tools/tests/test_resources/AnimatedMorphCube.gltf"
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["selfcheck", "pointer", "pointer_taa_mv", "fox", "morph", "load", "reload"])
args = parser.parse_args()
failures = []


def section(name):
    return args.only in (None, name)


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run_script(mode, name, script, extra):
    folder = OUT / mode
    folder.mkdir(parents=True, exist_ok=True)
    rel = folder.relative_to(REPO).as_posix()
    text = script.replace("@OUT@", rel)
    shots = [line.split(" ", 1)[1] for line in text.splitlines() if line.startswith("shot ")]
    for shot in shots:
        (REPO / shot).unlink(missing_ok=True)
    scenario = folder / f"{name}.txt"
    report = folder / f"{name}.json"
    scenario.write_text(text, encoding="utf-8")
    report.unlink(missing_ok=True)
    argv = [str(EXE)] + extra + COMMON + (["--force-full-sync"] if mode == "reference" else []) \
        + ["--ui-script", str(scenario), "--ui-report", str(report)]
    result = subprocess.run(argv, cwd=REPO, capture_output=True, text=True, timeout=180)
    if result.returncode != 0 or not report.exists():
        check(False, f"{name} [{mode}]: exit {result.returncode} {result.stderr[-300:]}")
        return None
    data = json.loads(report.read_text(encoding="utf-8"))
    if data.get("failed", True):
        check(False, f"{name} [{mode}]: script errors {data.get('errors')}")
        return None
    return {"snaps": {s["label"]: s for s in data["snaps"]}, "shots": {Path(p).name: REPO / p for p in shots}}


def compare(name, script, extra, playing_windows, rebuild_steps):
    """playing_windows: (before, after) snapshot pairs spanning only playback.
    rebuild_steps: (before, after) pairs that must still run a full rebuild."""
    ref = run_script("reference", name, script, extra)
    cand = run_script("candidate", name, script, extra)
    if ref is None or cand is None:
        return None
    for label, snap in cand["snaps"].items():
        r = ref["snaps"][label]
        same = (r["selection"], r["camera"], r["revisions"]["AnimationTime"]) == \
               (snap["selection"], snap["camera"], snap["revisions"]["AnimationTime"])
        check(same, f"{name}/{label}: selection, camera and AnimationTime ticks match reference")
    for shot, path in cand["shots"].items():
        rpath = ref["shots"][shot]
        check(path.exists() and rpath.exists() and sha(path) == sha(rpath), f"{name}/{shot}: byte-identical to reference")
    c = {k: v["revisions"] for k, v in cand["snaps"].items()}
    r = {k: v["revisions"] for k, v in ref["snaps"].items()}
    for before, after in playing_windows:
        ticks = c[after]["AnimationTime"] - c[before]["AnimationTime"]
        cand_syncs = c[after]["syncs"] - c[before]["syncs"]
        applies = c[after]["animationApplies"] - c[before]["animationApplies"]
        ref_syncs = r[after]["syncs"] - r[before]["syncs"]
        check(ticks >= 3 and cand_syncs == 0 and applies == ticks,
              f"{name} {before}->{after}: {ticks} ticks, candidate 0 rebuilds ({cand_syncs}), {applies} pose applies")
        check(ref_syncs == ticks, f"{name} {before}->{after}: reference rebuilt every tick ({ref_syncs})")
    for before, after in rebuild_steps:
        check(c[after]["syncs"] - c[before]["syncs"] >= 1, f"{name} {before}->{after}: still rebuilds")
    return c


if section("selfcheck"):
    print("[selfcheck]")
    out = OUT / "candidate" / "revision_selfcheck.txt"
    out.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(EXE), "--revision-selfcheck", str(out)], cwd=REPO, capture_output=True, text=True, timeout=60)
    text = out.read_text(encoding="utf-8") if out.exists() else ""
    check(result.returncode == 0 and "failures=0" in text and "PASS AnimationTime does not require renderer sync" in text,
          "self-check: " + (text.strip().splitlines() or ["?"])[-1])

POINTER_SCRIPT = f"""wait 8
reload {POINTER}
wait 12
snap loaded
animplay on
wait 10
snap play_a
shot @OUT@/ptr_play_a.png
wait 7
snap play_b
shot @OUT@/ptr_play_b.png
wait 30
snap play_loop
shot @OUT@/ptr_play_loop.png
animplay off
wait 6
snap paused
shot @OUT@/ptr_paused.png
click 750 783
wait 8
snap seek
shot @OUT@/ptr_seek.png
click 155 783
wait 8
snap stopped
shot @OUT@/ptr_stopped.png
animplay on
wait 12
snap play_c
click 36 783
wait 6
click 58 685
wait 6
click 1150 400
wait 8
snap clip_off
shot @OUT@/ptr_clip_off.png
click 36 783
wait 6
click 58 685
wait 6
click 1150 400
wait 8
snap clip_on
shot @OUT@/ptr_clip_on.png
animplay off
wait 4
reload {POINTER}
wait 12
snap reloaded
shot @OUT@/ptr_reloaded.png
wait 2
"""

if section("pointer"):
    print("[pointer: KHR_animation_pointer material + node rotation, AA none]")
    c = compare("pointer", POINTER_SCRIPT, ["--aa", "none"],
                [("play_a", "play_b"), ("play_b", "play_loop")],
                [("play_c", "clip_off"), ("clip_off", "clip_on")])
    if c:
        check(c["seek"]["AnimationTime"] > c["paused"]["AnimationTime"] and c["seek"]["syncs"] == c["paused"]["syncs"],
              "status-bar seek marks AnimationTime without rebuilding")
        check(c["stopped"]["syncs"] == c["seek"]["syncs"], "stop does not rebuild")
        check(c["reloaded"]["syncs"] == c["clip_on"]["syncs"] and c["reloaded"]["Topology"] > c["clip_on"]["Topology"],
              "reload marks Topology without an extra rebuild")

if section("pointer_taa_mv"):
    print("[pointer_taa_mv: TAA with two viewports during playback]")
    script = POINTER_SCRIPT.replace("ptr_", "ptrmv_").replace("snap loaded\n", "vpcreate\nwait 8\nsnap loaded\n", 1)
    compare("pointer_taa_mv", script, ["--aa", "taa"], [("play_a", "play_b")], [("play_c", "clip_off")])

if section("fox"):
    print("[fox: skinned playback with loop wrap]")
    compare("fox", f"""wait 8
reload {FOX}
wait 12
snap loaded
animplay on
wait 10
snap play_a
shot @OUT@/fox_play_a.png
wait 25
snap play_b
shot @OUT@/fox_play_b.png
wait 45
snap play_wrap
shot @OUT@/fox_play_wrap.png
picknode 4
wait 8
snap picked
shot @OUT@/fox_picked.png
animplay off
wait 6
snap paused
shot @OUT@/fox_paused.png
wait 2
""", ["--aa", "none"], [("play_a", "play_b"), ("play_b", "play_wrap")], [])

if section("morph"):
    print("[morph: morph-target weight playback]")
    compare("morph", f"""wait 8
reload {MORPH}
wait 12
snap loaded
animplay on
wait 8
snap play_a
shot @OUT@/morph_play_a.png
wait 10
snap play_b
shot @OUT@/morph_play_b.png
wait 10
snap play_c
shot @OUT@/morph_play_c.png
animplay off
wait 4
snap paused
wait 2
""", ["--aa", "none"], [("play_a", "play_b"), ("play_b", "play_c")], [])

def run_startup(exe, folder, name, argv):
    folder.mkdir(parents=True, exist_ok=True)
    first, steady, prof = folder / f"{name}_first.png", folder / f"{name}_f30.png", folder / f"{name}_profile.json"
    for path in (first, steady, prof):
        path.unlink(missing_ok=True)
    result = subprocess.run([str(exe)] + argv + ["--isolated", "--window-size", "1280x800", "--vsync", "off",
                            "--capture-first-frame", str(first), "--capture-viewport", str(steady),
                            "--capture-frame", "30", "--profile-report", str(prof), "--exit-after-profile"],
                            cwd=REPO, capture_output=True, text=True, timeout=120)
    if result.returncode != 0 or not (first.exists() and steady.exists() and prof.exists()):
        check(False, f"load {name} [{folder.name}]: exit {result.returncode}")
        return None
    frames = json.loads(prof.read_text(encoding="utf-8"))["frames"]
    return sha(first), sha(steady), sum(f["counters"]["dirtySyncs"] for f in frames)


if section("load") and BASE_EXE.exists():
    print("[load: startup loads vs Phase 3 baseline]")
    cases = {
        "s0_cube": (["--aa", "none"], 0),
        "fixture_msaa4": ([FIXTURE, "--aa", "msaa4"], 0),
        "fox_taa_t075": ([FOX, "--aa", "taa", "--anim-time", "0.75"], 0),
        "pointer_t15": ([POINTER, "--aa", "none", "--anim-time", "1.5"], 0),
        "morph": ([MORPH, "--aa", "fxaa"], 0),
        "duck_camera_vp": ([DUCK, "--aa", "none", "--create-viewport-cam", "1"], 1),  # ViewportLayout still rebuilds
        "fixture_hidden": ([FIXTURE, "--aa", "none", "--hide-node", "BlendQuadNode"], 1),  # Visibility still rebuilds
    }
    for name, (argv, expected_syncs) in cases.items():
        base = run_startup(BASE_EXE, OUT / "baseline", name, argv)
        cand = run_startup(EXE, OUT / "candidate", name, argv)
        if base and cand:
            check(base[0] == cand[0], f"load {name}: first presented frame byte-identical")
            check(base[1] == cand[1], f"load {name}: frame 30 byte-identical")
            check(base[2] == 1 and cand[2] == expected_syncs,
                  f"load {name}: rebuilds after load baseline {base[2]} -> candidate {cand[2]} (expected {expected_syncs})")

if section("reload") and BASE_EXE.exists():
    print("[reload: A -> B -> A and reload while attached, vs Phase 3 baseline]")
    script = f"""wait 8
reload {FIXTURE}
wait 12
snap a1
shot @OUT@/rl_a1.png
reload {FOX}
wait 12
snap b
shot @OUT@/rl_b.png
picknode 4
wait 8
snap b_picked
reload {FIXTURE}
wait 12
snap a2
shot @OUT@/rl_a2.png
reload {DUCK}
wait 12
click 815 60
wait 6
click 885 95
wait 10
snap duck_attached
shot @OUT@/rl_duck_attached.png
reload {DUCK}
wait 12
snap duck_reloaded_attached
shot @OUT@/rl_duck_reloaded_attached.png
click 795 60
wait 6
click 868 74
wait 10
snap duck_detached
shot @OUT@/rl_duck_detached.png
wait 2
"""
    results = {}
    for mode, exe in (("baseline", BASE_EXE), ("candidate", EXE)):
        folder = OUT / mode
        folder.mkdir(parents=True, exist_ok=True)
        rel = folder.relative_to(REPO).as_posix()
        text = script.replace("@OUT@", rel)
        shots = [line.split(" ", 1)[1] for line in text.splitlines() if line.startswith("shot ")]
        for shot in shots:
            (REPO / shot).unlink(missing_ok=True)
        (folder / "reload.txt").write_text(text, encoding="utf-8")
        report = folder / "reload.json"
        report.unlink(missing_ok=True)
        result = subprocess.run([str(exe), "--isolated", "--window-size", "1280x800", "--aa", "none",
                                 "--ui-script", str(folder / "reload.txt"), "--ui-report", str(report)],
                                cwd=REPO, capture_output=True, text=True, timeout=180)
        ok = result.returncode == 0 and report.exists() and not json.loads(report.read_text(encoding="utf-8")).get("failed", True)
        check(ok, f"reload [{mode}] script completed")
        if ok:
            results[mode] = ({x["label"]: x for x in json.loads(report.read_text(encoding="utf-8"))["snaps"]},
                             {Path(p).name: REPO / p for p in shots})
    if len(results) == 2:
        bsnaps, bshots = results["baseline"]
        csnaps, cshots = results["candidate"]
        for label, snap in csnaps.items():
            check((bsnaps[label]["selection"], bsnaps[label]["camera"]) == (snap["selection"], snap["camera"]),
                  f"reload/{label}: selection and camera match baseline")
        for shot, path in cshots.items():
            check(sha(path) == sha(bshots[shot]), f"reload/{shot}: byte-identical to baseline")
        rev = {k: v["revisions"] for k, v in csnaps.items()}
        for before, after in (("a1", "b"), ("b_picked", "a2"), ("duck_attached", "duck_reloaded_attached")):
            check(rev[after]["Topology"] - rev[before]["Topology"] == 1 and rev[after]["syncs"] == rev[before]["syncs"],
                  f"reload {before}->{after}: Topology +1 with no extra rebuild")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
