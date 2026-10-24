# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P4.2 selection-only update validation.

Selection-only frames no longer run updateGltf/updateScene/applyAnimationTime.
Each scenario runs the same UI script on the accepted Phase 3 binary (which
always resynchronized on selection) and on the candidate, then requires:
  * identical selection and camera state at every snapshot,
  * byte-identical full-window screenshots (viewport, OBB, gizmo, inspector),
  * on the candidate: selection-only steps add Selection revisions and skipped
    syncs but no renderer syncs; steps that also focus still sync.

Sections (run all, or one with --only):
  selfcheck  SceneRevisions self-check including the renderer sync mask.
  fixture    Real selection producers on the production fixture (None AA).
  taa        The fixture script with TAA (selection still drives the responsive blend).
  fox        Skinned/animated model: selection of mesh and joint nodes, then reload.
  multivp    Two viewports showing the same selection.
  camera     ViewCamera producers on Duck (glTF camera): attach/detach, ortho, FoV, focus.
  camera_taa The camera script with TAA.
  playback   Candidate only: selection while animation plays keeps per-tick syncs.
"""

import argparse
import hashlib
import json
import harness_process as subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
BASE_EXE = REPO / "build" / "p4.1" / "baseline" / "Glider.exe"  # accepted Phase 3 final binary
OUT = REPO / "build" / "p4.2"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
DUCK = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Duck/glTF-Binary/Duck.glb"
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
WINDOW = ["--isolated", "--window-size", "1280x800"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["selfcheck", "fixture", "taa", "fox", "multivp", "camera", "camera_taa", "playback"])
args = parser.parse_args()
failures = []
NO_SYNC_KINDS = {"Selection", "ViewCamera"}  # kinds excluded from the renderer sync (P4.2)


def section(name):
    return args.only in (None, name)


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run_script(exe, folder, name, script, extra):
    folder.mkdir(parents=True, exist_ok=True)
    scenario = folder / f"{name}.txt"
    report = folder / f"{name}.json"
    rel = folder.relative_to(REPO).as_posix()
    text = script.replace("@OUT@", rel)
    for line in text.splitlines():
        if line.startswith("shot "):
            (REPO / line.split(" ", 1)[1]).unlink(missing_ok=True)
    scenario.write_text(text, encoding="utf-8")
    report.unlink(missing_ok=True)
    result = subprocess.run([str(exe)] + extra + WINDOW + ["--ui-script", str(scenario), "--ui-report", str(report)],
                            cwd=REPO, capture_output=True, text=True, timeout=120)
    if result.returncode != 0 or not report.exists():
        check(False, f"{name} [{folder.name}]: exit {result.returncode} {result.stderr[-300:]}")
        return None
    data = json.loads(report.read_text(encoding="utf-8"))
    if data.get("failed", True):
        check(False, f"{name} [{folder.name}]: script errors {data.get('errors')}")
        return None
    shots = [line.split(" ", 1)[1] for line in text.splitlines() if line.startswith("shot ")]
    return {"snaps": {s["label"]: s for s in data["snaps"]}, "shots": {Path(p).name: REPO / p for p in shots}}


def compare(name, script, extra=None, steps=None):
    """Runs script on baseline and candidate; returns candidate snaps."""
    extra = extra or []
    base = run_script(BASE_EXE, OUT / "baseline", name, script, extra)
    cand = run_script(EXE, OUT / "candidate", name, script, extra)
    if base is None or cand is None:
        return None
    for label, snap in cand["snaps"].items():
        b = base["snaps"].get(label)
        check(b is not None and b["selection"] == snap["selection"], f"{name}/{label}: selection matches baseline ({snap['selection'].split('|')[1]})")
        check(b is not None and b["camera"] == snap["camera"], f"{name}/{label}: camera matches baseline")
    for shot, path in cand["shots"].items():
        bpath = base["shots"][shot]
        same = path.exists() and bpath.exists() and sha(path) == sha(bpath)
        check(same, f"{name}/{shot}: full-window screenshot byte-identical")
    snaps = {k: v["revisions"] for k, v in cand["snaps"].items()}
    for before, after, kinds, syncs in steps or []:
        a, b = snaps[before], snaps[after]
        changed = {k: b[k] - a[k] for k in a if k[0].isupper() and b[k] != a[k]}
        sync_delta = b["syncs"] - a["syncs"]
        skipped = b["skippedSyncs"] - a["skippedSyncs"]
        check(changed == kinds, f"{name} {before}->{after}: kinds {changed} == {kinds}")
        if kinds and set(kinds) <= NO_SYNC_KINDS:
            check(sync_delta == 0 and skipped >= 1, f"{name} {before}->{after}: no-sync kinds only, syncs +{sync_delta}, skipped +{skipped}")
        else:
            check(sync_delta == syncs, f"{name} {before}->{after}: syncs +{sync_delta} == {syncs}")
        check(b["pending"] == "", f"{name} {after}: nothing pending")
    return snaps


FIXTURE_SCRIPT = f"""wait 8
reload {FIXTURE}
wait 12
snap loaded
click 650 418
wait 8
snap vp_pick
shot @OUT@/fx_vp_pick.png
click 350 150
wait 8
snap vp_clear
shot @OUT@/fx_vp_clear.png
picknode 1
wait 8
snap harness_pick
click 20 536
wait 6
snap res_expanded
click 150 602
wait 8
snap res_node
shot @OUT@/fx_res_node.png
clickarrowp hier.node 0
wait 8
snap hier_node
clickp hier.mesh 0
wait 8
snap hier_mesh
shot @OUT@/fx_hier_mesh.png
click 650 418
wait 8
snap vp_pick2
click 438 60
wait 8
snap focus_key
shot @OUT@/fx_focus_key.png
click 1135 350
wait 8
snap inspect_material
shot @OUT@/fx_inspect_material.png
dclick 650 418
wait 8
snap vp_reclick
shot @OUT@/fx_vp_reclick.png
wait 30
snap idle
reload {FIXTURE}
wait 12
snap reloaded
shot @OUT@/fx_reloaded.png
wait 2
"""

FIXTURE_STEPS = [
    ("loaded", "vp_pick", {"Selection": 1}, 0),
    ("vp_pick", "vp_clear", {"Selection": 1}, 0),
    ("vp_clear", "harness_pick", {"Selection": 1}, 0),
    ("res_expanded", "res_node", {"Selection": 1}, 0),
    ("res_node", "hier_node", {"Selection": 1, "ViewCamera": 1}, 0),   # row click also focuses
    ("hier_node", "hier_mesh", {"Selection": 1}, 0),
    ("hier_mesh", "vp_pick2", {"Selection": 1}, 0),
    ("vp_pick2", "focus_key", {}, 0),                                    # F focuses without a mark (as before)
    ("focus_key", "inspect_material", {}, 0),                            # inspector link: unmarked selection path
    ("vp_reclick", "idle", {}, 0),
    ("idle", "reloaded", {"Topology": 1}, 0),   # 1 before P4.3 (load syncs itself)
]

if section("selfcheck"):
    print("[selfcheck]")
    out = OUT / "candidate" / "revision_selfcheck.txt"
    out.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(EXE), "--revision-selfcheck", str(out)], cwd=REPO, capture_output=True, text=True, timeout=60)
    text = out.read_text(encoding="utf-8") if out.exists() else ""
    check(result.returncode == 0 and "failures=0" in text and "PASS Selection does not require renderer sync" in text and "PASS ViewCamera does not require renderer sync" in text,
          "self-check: " + (text.strip().splitlines() or ["?"])[-1])

if not BASE_EXE.exists():
    check(False, f"baseline executable missing: {BASE_EXE} (comparisons NOT RUN)")
else:
    if section("fixture"):
        print("[fixture: real selection producers, AA none]")
        snaps = compare("fixture", FIXTURE_SCRIPT, ["--aa", "none"], FIXTURE_STEPS)
        if snaps:
            check(snaps["vp_pick"]["Selection"] - snaps["loaded"]["Selection"] == 1, "viewport click marks Selection")
            check(snaps["reloaded"]["skippedSyncs"] >= 6, f"selection-only frames skipped: {snaps['reloaded']['skippedSyncs']}")

    if section("taa"):
        print("[taa: same script with temporal AA]")
        compare("taa", FIXTURE_SCRIPT.replace("fx_", "taa_"), ["--aa", "taa"], FIXTURE_STEPS)

    if section("fox"):
        print("[fox: skinned mesh and joint selection]")
        compare("fox", f"""wait 8
reload {FOX}
wait 12
snap loaded
picknode 1
wait 8
snap mesh_node
shot @OUT@/fox_mesh_node.png
picknode 4
wait 8
snap joint_node
shot @OUT@/fox_joint_node.png
click 438 60
wait 8
snap joint_focus
shot @OUT@/fox_joint_focus.png
reload {FOX}
wait 12
snap reloaded
shot @OUT@/fox_reloaded.png
wait 2
""", ["--aa", "none", "--anim-time", "0.75"], [
            ("loaded", "mesh_node", {"Selection": 1}, 0),
            ("mesh_node", "joint_node", {"Selection": 1}, 0),
            ("joint_node", "joint_focus", {}, 0),
            ("joint_focus", "reloaded", {"Topology": 1}, 0),
        ])

    if section("multivp"):
        print("[multivp: two viewports, one selection]")
        compare("multivp", f"""wait 8
reload {FIXTURE}
wait 12
vpcreate
wait 8
snap two
picknode 2
wait 8
snap picked
shot @OUT@/mv_picked.png
picknode 0
wait 8
snap repicked
shot @OUT@/mv_repicked.png
wait 2
""", ["--aa", "none"], [
            ("two", "picked", {"Selection": 1}, 0),
            ("picked", "repicked", {"Selection": 1}, 0),
        ])

CAMERA_SCRIPT = f"""wait 8
reload {DUCK}
wait 12
clickp hier.viewport 1
wait 8
click 1268 132
wait 3
click 1268 132
wait 3
click 1268 132
wait 3
click 1268 132
wait 8
snap camera_tab
shot @OUT@/cam_tab.png
click 1041 465
wait 8
snap ortho_on
shot @OUT@/cam_ortho_on.png
click 1041 465
wait 8
snap ortho_off
click 1060 497
wait 8
snap fov
shot @OUT@/cam_fov.png
click 815 60
wait 6
click 885 95
wait 10
snap attached
shot @OUT@/cam_attached.png
click 1106 176
wait 10
snap detached_inspector
shot @OUT@/cam_detached_inspector.png
click 815 60
wait 6
click 885 95
wait 10
snap reattached
click 795 60
wait 6
click 868 74
wait 10
snap detached_dropdown
shot @OUT@/cam_detached_dropdown.png
click 90 127
wait 10
snap hier_focus
shot @OUT@/cam_hier_focus.png
dclick 440 640
wait 10
snap vp_dclick
shot @OUT@/cam_vp_dclick.png
wait 30
snap idle
reload {DUCK}
wait 12
snap reloaded
shot @OUT@/cam_reloaded.png
wait 2
"""

CAMERA_STEPS = [
    ("camera_tab", "ortho_on", {"ViewCamera": 1}, 0),
    ("ortho_on", "ortho_off", {"ViewCamera": 1}, 0),
    ("fov", "attached", {"ViewCamera": 1}, 0),
    ("attached", "detached_inspector", {"ViewCamera": 1}, 0),
    ("detached_inspector", "reattached", {"ViewCamera": 1}, 0),
    ("reattached", "detached_dropdown", {"ViewCamera": 1}, 0),
    ("detached_dropdown", "hier_focus", {"Selection": 1, "ViewCamera": 1}, 0),
    ("idle", "reloaded", {"Topology": 1}, 0),   # 1 before P4.3 (load syncs itself)
]


def camera_checks(name, snaps):
    if not snaps:
        return
    fov_delta = snaps["fov"]["ViewCamera"] - snaps["ortho_off"]["ViewCamera"]
    check(fov_delta >= 1 and snaps["fov"]["syncs"] == snaps["ortho_off"]["syncs"],
          f"{name}: FoV slider marks ViewCamera ({fov_delta}) without syncing")
    dclick = {k: snaps["vp_dclick"][k] - snaps["hier_focus"][k] for k in NO_SYNC_KINDS}
    check(dclick["ViewCamera"] >= 1 and snaps["vp_dclick"]["syncs"] == snaps["hier_focus"]["syncs"],
          f"{name}: viewport double-click focus {dclick} without syncing")
    check(snaps["idle"]["syncs"] == snaps["camera_tab"]["syncs"], f"{name}: no syncs from camera tab to idle")


if BASE_EXE.exists():
    if section("camera"):
        print("[camera: ViewCamera producers on Duck, AA none]")
        camera_checks("camera", compare("camera", CAMERA_SCRIPT, ["--aa", "none"], CAMERA_STEPS))
    if section("camera_taa"):
        print("[camera_taa: same script with TAA]")
        camera_checks("camera_taa", compare("camera_taa", CAMERA_SCRIPT.replace("cam_", "camtaa_"), ["--aa", "taa"], CAMERA_STEPS))

if section("playback"):
    print("[playback: candidate only]")
    run = run_script(EXE, OUT / "candidate", "playback", f"""wait 8
reload {FOX}
wait 12
animplay on
wait 10
snap playing
picknode 4
wait 20
snap picked_playing
animplay off
wait 6
snap paused
picknode 1
wait 8
snap picked_paused
wait 2
""", ["--aa", "none"])
    if run:
        s = {k: v["revisions"] for k, v in run["snaps"].items()}
        ticks = s["picked_playing"]["AnimationTime"] - s["playing"]["AnimationTime"]
        syncs = s["picked_playing"]["syncs"] - s["playing"]["syncs"]
        applies = s["picked_playing"]["animationApplies"] - s["playing"]["animationApplies"]
        # Since P4.3 playback ticks re-apply the pose instead of rebuilding.
        check(ticks >= 20 and syncs == 0 and applies == ticks,
              f"playback applies the pose once per tick while selecting ({applies} applies, {syncs} rebuilds, {ticks} ticks)")
        check(s["picked_playing"]["Selection"] - s["playing"]["Selection"] == 1, "selection during playback recorded")
        check("node=b_Hip_01" in run["snaps"]["picked_playing"]["selection"], "joint selected during playback")
        check(s["picked_paused"]["syncs"] == s["paused"]["syncs"], "selection after pause does not sync")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
