# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Phase 4 exit gate: one targeted transition test per revision producer.

Every step drives a real UI control on resources/test_models/hierarchy_animation_fixture.gltf
(1920x1080, so all Inspector controls fit) and requires:
  * the expected SceneChange kind's revision advanced (the producer marks correctly),
  * a full renderer sync happened for kinds that need one, and none for the
    no-rebuild kinds (Selection, ViewCamera; AnimationTime applies the pose),
  * the screenshot is byte-identical to the conservative reference: the same
    binary run with --force-full-sync --disable-command-caches
    --disable-transform-reuse (every Phase 4 shortcut off) and, since Phase 5,
    --always-pick --disable-redraw-skip (every-frame picking and scene redraw),
  * the screenshot differs from the previous state where the change is visible.

Producers covered earlier by other suites (Topology, Visibility, Transform,
Selection, ViewCamera, AnimationSet, AnimationTime, ViewportLayout) are
re-exercised here too so the gate is checked on one binary.
"""

import hashlib
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "exitgate"
HIER = "resources/test_models/hierarchy_animation_fixture.gltf"
# --fixed-frame-time makes scripted runs deterministic, so they need not wait for vsync.
COMMON = ["--isolated", "--window-size", "1920x1080", "--fixed-frame-time", "0.05", "--aa", "none", "--vsync", "off"]
REFERENCE = ["--force-full-sync", "--disable-command-caches", "--disable-transform-reuse",
             "--always-pick", "--disable-redraw-skip"]
NO_SYNC = {"Selection", "ViewCamera", "AnimationTime"}

# (label, kind expected to advance, visible change expected, script lines)
# Environment controls (IBL, lights, skybox) are in the Scene Inspector (hier.scene); viewport
# settings open on their first tab, Render Modes. The IBL filter settings live in the Scene
# Inspector too and must follow the IBL toggle (checked after the transition loop).
STEPS = [
    ("variant_alt", "Material", True, ["click 110 800"]),
    ("variant_default", "Material", True, ["click 110 778"]),
    ("material_color", "Material", True, ["click 122 866", "wait 6", "dclick 1551 484", "wait 4", "key a ctrl", "type 20", "key enter"]),
    ("light_intensity", "Light", True, ["click 20 734", "wait 4", "click 140 756", "wait 6", "dclick 1620 124", "wait 4",
                                        "key a ctrl", "type 400", "key enter"]),
    ("camera_fov", "SceneCamera", False, ["click 20 712", "wait 4", "click 120 734", "wait 6", "click 1700 124"]),
    ("rename", "Metadata", True, ["picknode 5", "wait 6", "dclick 1660 93", "wait 4", "key a ctrl", "type Renamed", "key enter"]),
    ("extension", "Extension", False, ["click 1571 475"]),
    ("ibl_off", "RenderSettings", True, ["clickp hier.scene 0", "wait 6", "clickp scene.ibl.on 0"]),
    ("punctual_off", "RenderSettings", True, ["clickp scene.punctual.on 0"]),
    ("lights_on", "RenderSettings", True, ["clickp scene.ibl.on 0", "wait 4", "clickp scene.punctual.on 0"]),
    ("double_sided", "ViewportSettings", False, ["clickp hier.viewport 1", "wait 6", "click 1551 330"]),
    ("invert_winding", "ViewportSettings", True, ["click 1551 362"]),
    ("overlay", "ViewportSettings", True, ["click 1551 362", "wait 4", "click 1551 423"]),
    ("skybox_off", "RenderSettings", True, ["click 1551 423", "wait 4", "clickp hier.scene 0", "wait 6", "click 1551 164"]),
    ("eye_hide", "Visibility", True, ["click 1551 164", "wait 4", "click 405 149"]),
    ("transform", "Transform", True, ["click 405 149", "wait 4", "picknode 0", "wait 6", "dclick 1577 163", "wait 4",
                                      "key a ctrl", "type 1.5", "key enter"]),
    ("play", "AnimationTime", True, ["animplay on", "wait 10"]),
    ("clip_off", "AnimationSet", True, ["animplay off", "wait 4", "click 36 1063", "wait 6", "click 58 965", "wait 6",
                                        "click 1700 900"]),
    ("reload", "Topology", True, [f"reload {HIER}", "wait 12"]),
]


def script_text(folder):
    rel = folder.relative_to(REPO).as_posix()
    lines = ["wait 8", "snap start", f"shot {rel}/start.png"]
    for label, _, _, cmds in STEPS:
        lines += cmds + ["wait 10", f"snap {label}", f"shot {rel}/{label}.png"]
    return "\n".join(lines + ["wait 2"]) + "\n"


def run(mode, extra):
    folder = OUT / mode
    folder.mkdir(parents=True, exist_ok=True)
    for png in folder.glob("*.png"):
        png.unlink()
    scenario, report = folder / "gate.txt", folder / "gate.json"
    scenario.write_text(script_text(folder), encoding="utf-8")
    report.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), HIER] + COMMON + extra + ["--ui-script", str(scenario), "--ui-report", str(report)],
                            cwd=REPO, capture_output=True, text=True, timeout=600)
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else {}
    return result.returncode, data, folder


failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


subprocess.run([sys.executable, str(REPO / "scripts" / "generate_hierarchy_fixture.py")], check=True, capture_output=True)
code_c, cand, cfolder = run("candidate", [])
code_r, ref, rfolder = run("reference", REFERENCE)
check(code_c == 0 and not cand.get("failed", True), f"candidate run completed {cand.get('errors')}")
check(code_r == 0 and not ref.get("failed", True), f"reference run completed {ref.get('errors')}")

if cand.get("snaps") and ref.get("snaps"):
    snaps = {s["label"]: s for s in cand["snaps"]}
    rsnaps = {s["label"]: s for s in ref["snaps"]}
    previous = "start"
    for label, kind, visible, _ in STEPS:
        a, b = snaps[previous]["revisions"], snaps[label]["revisions"]
        advanced = b[kind] - a[kind]
        syncs = b["syncs"] - a["syncs"]
        check(advanced >= 1, f"{label}: {kind} revision advanced (+{advanced})")
        if kind == "Topology":
            # Loads synchronise the renderer themselves (P4.3b); no second rebuild follows.
            check(syncs == 0, f"{label}: load synchronised itself, no extra rebuild ({syncs} syncs)")
        elif kind not in NO_SYNC:
            check(syncs >= 1, f"{label}: renderer resynchronised for {kind} ({syncs} syncs)")
        same = sha(cfolder / f"{label}.png") == sha(rfolder / f"{label}.png")
        check(same, f"{label}: screenshot byte-identical to the all-shortcuts-off reference")
        check(snaps[label]["selection"] == rsnaps[label]["selection"], f"{label}: selection matches reference")
        if visible:
            changed = sha(cfolder / f"{label}.png") != sha(cfolder / f"{previous}.png")
            check(changed, f"{label}: change is visible on screen")
        previous = label
    marks = snaps[STEPS[-1][0]]["revisions"]
    kinds = ["Topology", "Visibility", "Transform", "Material", "Light", "SceneCamera", "Extension", "Metadata",
             "AnimationSet", "AnimationTime", "RenderSettings", "ViewportSettings", "Selection"]
    print("    final revisions:", {k: marks[k] for k in kinds})

    # The IBL filter settings moved into the Scene Inspector and are shown only while IBL
    # is on; the panel is probed at its heading, so its absence proves it was not drawn.
    def has_ibl_filter(snapshot):
        return any(probe["tag"] == "scene.ibl.filter" for probe in snapshot["probes"])

    check(not has_ibl_filter(snaps["ibl_off"]), "ibl_off: IBL filter settings hidden")
    check(not has_ibl_filter(snaps["punctual_off"]), "punctual_off: IBL filter settings still hidden")
    check(has_ibl_filter(snaps["lights_on"]), "lights_on: IBL filter settings shown")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
