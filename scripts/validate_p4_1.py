# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P4.1 revision-contract validation.

Sections (run all, or one with --only):
  selfcheck    SceneRevisions unit self-check, plus forced-failure exit code.
  transitions  Scripted UI transitions: each producer advances only its own
               kind, every mark causes exactly the conservative full sync, and
               idle frames neither mark nor sync.
  startup      Command-line producers (--hide-node, camera flags).
  compare      Accepted Phase 3 baseline vs candidate: byte-identical viewport
               captures and identical per-frame sync counters.
"""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "p4.1"
CAND = OUT / "candidate"
BASE = OUT / "baseline"
BASE_EXE = BASE / "Glider.exe"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
KINDS = ["Topology", "Visibility", "Transform", "Material", "Light", "SceneCamera", "Extension", "Metadata",
         "AnimationSet", "AnimationTime", "RenderSettings", "ViewportSettings", "ViewportLayout", "ViewCamera",
         "Selection"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["selfcheck", "transitions", "startup", "compare"])
args = parser.parse_args()
CAND.mkdir(parents=True, exist_ok=True)
failures = []
FIXTURE_SCRIPT = f"""wait 8
reload {FIXTURE}
wait 12
snap loaded
wait 30
snap idle
picknode 1
snap picked
wait 30
snap picked_idle
clickp hier.node 0
snap hier_node
click 264 171
snap hidden
click 264 171
snap shown
wait 30
snap shown_idle
reload {FIXTURE}
wait 12
snap reloaded
wait 2
"""


def section(name):
    return args.only in (None, name)


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(exe, argv, timeout=60):
    return subprocess.run([str(exe)] + argv, cwd=REPO, capture_output=True, text=True, timeout=timeout)


def run_script(name, script, extra=None):
    scenario = CAND / f"{name}.txt"
    report = CAND / f"{name}.json"
    scenario.write_text(script, encoding="utf-8")
    report.unlink(missing_ok=True)
    result = run(EXE, (extra or []) + ["--isolated", "--window-size", "1280x800",
                                       "--ui-script", str(scenario), "--ui-report", str(report)])
    if result.returncode != 0 or not report.exists():
        check(False, f"{name}: run exit={result.returncode} {result.stderr[-400:]}")
        return None
    data = json.loads(report.read_text(encoding="utf-8"))
    check(not data.get("failed", True), f"{name}: script completed {data.get('errors', '')}")
    return {s["label"]: s["revisions"] for s in data["snaps"]}


def delta(snaps, before, after):
    a, b = snaps[before], snaps[after]
    return {k: b[k] - a[k] for k in KINDS if b[k] != a[k]}, b["syncs"] - a["syncs"]


def expect(snaps, before, after, kinds, syncs):
    changed, sync_delta = delta(snaps, before, after)
    check(changed == kinds, f"{before}->{after}: kinds {changed} == {kinds}")
    if syncs == "any":
        check(sync_delta >= 1, f"{before}->{after}: syncs +{sync_delta} >= 1")
    else:
        check(sync_delta == syncs, f"{before}->{after}: syncs +{sync_delta} == {syncs}")
    check(snaps[after]["pending"] == "", f"{after}: nothing left pending")


if section("selfcheck"):
    print("[selfcheck]")
    out = CAND / "revision_selfcheck.txt"
    result = run(EXE, ["--revision-selfcheck", str(out)])
    text = out.read_text(encoding="utf-8") if out.exists() else ""
    check(result.returncode == 0 and "failures=0" in text, "self-check passes: " + (text.strip().splitlines() or ["?"])[-1])
    neg = CAND / "revision_selfcheck_forced.txt"
    result = run(EXE, ["--revision-selfcheck", str(neg), "--selfcheck-force-failure"])
    check(result.returncode == 1, f"forced failure exits 1 (got {result.returncode})")

if section("transitions"):
    print("[transitions: production fixture]")
    snaps = run_script("transitions_fixture", FIXTURE_SCRIPT)
    if snaps:
        check(snaps["loaded"]["Topology"] >= 1, "load marks Topology")
        expect(snaps, "loaded", "idle", {}, 0)
        expect(snaps, "idle", "picked", {"Selection": 1}, 0)  # 1 before P4.2 skipped selection-only syncs
        expect(snaps, "picked", "picked_idle", {}, 0)
        expect(snaps, "picked_idle", "hier_node", {"Selection": 1, "ViewCamera": 1}, 0)  # 1 before P4.2 (focus no longer syncs)
        # The eye button overlaps its tree row, so a click also selects and
        # focuses the node (pre-existing UI behavior, identical in the baseline).
        # Press and release land on different frames: one sync for each.
        for before, after in (("hier_node", "hidden"), ("hidden", "shown")):
            changed, sync_delta = delta(snaps, before, after)
            check(changed.get("Visibility") == 1, f"{before}->{after}: eye click advances Visibility once ({changed})")
            check(set(changed) <= {"Visibility", "Selection", "ViewCamera"}, f"{before}->{after}: only row-click side effects")
            check(1 <= sync_delta <= 2, f"{before}->{after}: syncs +{sync_delta} in [1, 2]")
            check(snaps[after]["pending"] == "", f"{after}: nothing left pending")
        expect(snaps, "shown", "shown_idle", {}, 0)
        expect(snaps, "shown_idle", "reloaded", {"Topology": 1}, 1)
        check(snaps["reloaded"]["lastConsumed"] in ("", "Topology"), "reload consumed only Topology")

    print("[transitions: animation playback]")
    snaps = run_script("transitions_fox", f"""wait 8
reload {FOX}
wait 12
snap loaded
wait 20
snap idle
animplay on
wait 30
snap playing
animplay off
wait 6
snap paused
wait 30
snap paused_idle
wait 2
""")
    if snaps:
        expect(snaps, "loaded", "idle", {}, 0)
        changed, sync_delta = delta(snaps, "idle", "playing")
        ticks = changed.get("AnimationTime", 0)
        check(set(changed) == {"AnimationTime"} and ticks >= 30, f"playback marks only AnimationTime ({changed})")
        check(sync_delta == ticks, f"one sync per playback tick ({sync_delta} syncs, {ticks} ticks)")
        changed, _ = delta(snaps, "playing", "paused")
        check(set(changed) <= {"AnimationTime"}, f"pause marks nothing new ({changed})")
        expect(snaps, "paused", "paused_idle", {}, 0)

if section("startup"):
    print("[startup producers]")
    snaps = run_script("startup_flags", "wait 12\nsnap start\nwait 20\nsnap later\nwait 2\n",
                       [FIXTURE, "--hide-node", "TransmissiveQuadNode", "--camera-yaw", "30"])
    if snaps:
        check(snaps["start"]["Visibility"] == 1, f"--hide-node marks Visibility once ({snaps['start']['Visibility']})")
        check(snaps["start"]["ViewCamera"] >= 1, f"camera flags mark ViewCamera ({snaps['start']['ViewCamera']})")
        check(snaps["start"]["Topology"] >= 1, "argv model marks Topology")
        expect(snaps, "start", "later", {}, 0)

if section("compare"):
    print("[baseline vs candidate]")
    if not BASE_EXE.exists():
        check(False, f"baseline executable missing: {BASE_EXE} (NOT RUN)")
    else:
        cases = {
            "s0_cube": ["--aa", "none"],
            "s5_fixture_msaa4": [FIXTURE, "--aa", "msaa4"],
            "s5_fixture_selected": [FIXTURE, "--aa", "none", "--select-node", "BlendQuadNode"],
            "s4_fox_taa": [FOX, "--aa", "taa", "--anim-time", "0.75"],
        }
        for name, argv in cases.items():
            results = {}
            for label, exe in (("baseline", BASE_EXE), ("candidate", EXE)):
                folder = OUT / label
                folder.mkdir(parents=True, exist_ok=True)
                png = folder / f"{name}.png"
                prof = folder / f"{name}_profile.json"
                png.unlink(missing_ok=True)
                prof.unlink(missing_ok=True)
                result = run(exe, argv + ["--isolated", "--window-size", "1280x800", "--vsync", "off",
                                          "--capture-viewport", str(png), "--capture-frame", "90",
                                          "--profile-report", str(prof), "--exit-after-profile"])
                ok = result.returncode == 0 and png.exists() and prof.exists()
                check(ok, f"{name} {label} run (exit {result.returncode})")
                if ok:
                    frames = json.loads(prof.read_text(encoding="utf-8"))["frames"]
                    results[label] = (sha(png), [(f["counters"]["dirtySyncs"], f["counters"]["updateGltfCalls"],
                                                  f["counters"]["updateSceneCalls"]) for f in frames])
            if len(results) == 2:
                check(results["baseline"][0] == results["candidate"][0], f"{name}: viewport capture byte-identical")
                check(results["baseline"][1] == results["candidate"][1],
                      f"{name}: per-frame sync counters identical ({sum(c[0] for c in results['candidate'][1])} syncs)")
        # Interactive control: the same producer sequence on both binaries must
        # trigger the full sync on exactly the same frames.
        script = CAND / "compare_interactive.txt"
        script.write_text(FIXTURE_SCRIPT + "wait 300\n", encoding="utf-8")
        seqs = {}
        for label, exe in (("baseline", BASE_EXE), ("candidate", EXE)):
            prof = OUT / label / "interactive_profile.json"
            png = OUT / label / "interactive.png"
            report = OUT / label / "interactive_ui.json"
            for path in (prof, png, report):
                path.unlink(missing_ok=True)
            result = run(exe, ["--isolated", "--window-size", "1280x800", "--vsync", "off",
                               "--ui-script", str(script), "--ui-report", str(report),
                               "--capture-viewport", str(png), "--capture-frame", "330",
                               "--profile-report", str(prof)], timeout=120)
            ok = result.returncode == 0 and prof.exists() and png.exists()
            check(ok, f"interactive {label} run (exit {result.returncode})")
            if ok:
                frames = json.loads(prof.read_text(encoding="utf-8"))["frames"]
                no_sync_frames = [f["frame"] for f in frames if f["counters"].get("sceneChanges") in ("Selection", "ViewCamera", "Selection|ViewCamera", "ViewCamera|Selection")]
                seqs[label] = ([f["frame"] for f in frames if f["counters"]["dirtySyncs"]], sha(png), no_sync_frames)
        if len(seqs) == 2:
            # Since P4.2, frames whose only changes are Selection/ViewCamera skip the sync the
            # baseline performed; every other sync frame must be unchanged.
            skipped = seqs["candidate"][2]
            expected = [f for f in seqs["baseline"][0] if f not in skipped]
            check(expected == seqs["candidate"][0],
                  f"interactive: sync frames {seqs['candidate'][0]} == baseline minus no-sync frames {skipped}")
            check(seqs["baseline"][1] == seqs["candidate"][1], "interactive: final capture byte-identical")
        manifest = {"candidate_sha256": sha(EXE), "baseline_sha256": sha(BASE_EXE),
                    "baseline_note": "accepted Phase 3 final binary (3c86ff7) per PHASE_3 section 12.31"}
        (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
