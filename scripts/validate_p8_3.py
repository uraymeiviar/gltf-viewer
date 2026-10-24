# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P8.3 instancing validation.

CommandMapping::mergeInstances merges opaque-pass draws of the same geometry,
material, program and state into one glDraw*Instanced call. Each instance's
transforms are the mesh's own u_ModelMatrix and u_NormalMatrix bytes, read from a
per-instance vertex stream (primitive.vert, USE_INSTANCING). Skinned, morphed and
transmissive drawables, the transparent pass, picking copies and transmission
duplicates are never merged.

Sections:
  equivalence  Instancing-specific cases on three binaries: the reference
               (build/p8/Glider_p81c.exe, before P8.3), the candidate with
               --disable-instancing and the candidate. Screenshots and
               selections must be identical; the candidate must merge draws.
               The broad P8.1 suite (python scripts/validate_p8_1.py --ref-exe
               build/p8/Glider_p81c.exe --label p8.3) covers the rest.
  benchmark    Run that P8.1 suite's benchmark for timing.
"""

import argparse
import hashlib
import json
import harness_process as subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "p8" / "p8.3"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
ENGINE = "build/_deps/assimp-src/test/models/glTF2/2CylinderEngine-glTF-Binary/2CylinderEngine.glb"
MANY = "build/p8/fixtures/many_1600.gltf"
LEAF = "build/p8/fixtures/many_leaf.gltf"
ROOT = "build/p8/fixtures/many_root.gltf"
# The E195 cockpit with absolute texture paths (scripts/e195_assets.py), so every binary loads all its textures.
from e195_assets import COCKPIT, textured_copy
E195 = textured_copy(COCKPIT, REPO / "build" / "e195_assets") or COCKPIT
# --fixed-frame-time makes scripted runs deterministic, so they need not wait for vsync
# (a 400-click case: 108 s with vsync, 4 s without, identical results).
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05", "--vsync", "off"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--ref-exe", default=str(REPO / "build" / "p8" / "Glider_p81c.exe"))
parser.add_argument("--with-ab", action="store_true",
                    help="also compare with the candidate's own A/B switch (slower; the baseline comparison is the regression check)")
parser.add_argument("--case", action="append", help="run only cases whose name starts with this (repeatable)")
parser.add_argument("--extra", action="append", default=[], help="extra argument passed to every binary (repeatable)")
args = parser.parse_args()
REF = Path(args.ref_exe).resolve()
failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def viewport_sha(path):
    """Hash of a window screenshot's viewport area only. The default dock layout (Menus.cpp) gives the left
    panels 22% of the width and the Inspector 26% of the rest, so the viewports span about 22-79.7%; the panels
    are left out so that UI changes there do not fail render-equivalence checks."""
    from PIL import Image
    image = Image.open(path).convert("RGB")
    width, height = image.size
    # The menu bar (top) and the status bar (bottom, where hover tooltips also appear) are left out too.
    box = (int(width * 0.22) + 6, 30, int(width * (1 - 0.78 * 0.26)) - 6, height - 40)
    return hashlib.sha256(image.crop(box).tobytes()).hexdigest()


for path, size, anim in ((MANY, "40", "none"), (LEAF, "40", "leaf"), (ROOT, "40", "root")):
    (REPO / path).parent.mkdir(parents=True, exist_ok=True)
    if not (REPO / path).exists():  # shared with other harnesses; run_regression.py writes them up front
        subprocess.run([sys.executable, str(REPO / "scripts" / "generate_many_drawables_fixture.py"), str(REPO / path), size, anim],
                       check=True, capture_output=True)


def run(mode, name, model, extra, lines):
    exe = REF if mode == "ref" else EXE
    folder = OUT / mode
    folder.mkdir(parents=True, exist_ok=True)
    text = "\n".join(lines).replace("@OUT@", folder.relative_to(REPO).as_posix()) + "\n"
    shots = [l.split(" ", 1)[1] for l in text.splitlines() if l.startswith("shot ")]
    for s in shots:
        (REPO / s).unlink(missing_ok=True)
    scenario, report = folder / f"{name}.txt", folder / f"{name}.json"
    scenario.write_text(text, encoding="utf-8")
    report.unlink(missing_ok=True)
    flags = ["--disable-instancing"] if mode == "noinst" else []
    subprocess.run([str(exe), model] + COMMON + extra + args.extra + flags + ["--ui-script", str(scenario), "--ui-report", str(report)],
                   cwd=REPO, capture_output=True, timeout=600)
    if not report.exists():
        return None
    data = json.loads(report.read_text(encoding="utf-8"))
    if data.get("failed", True):
        return None
    return {"snaps": data["snaps"], "shots": {Path(s).name: REPO / s for s in shots}}


def case(name, model, extra, lines, expect_merge=True):
    if args.case and not any(name.startswith(c) for c in args.case):
        return None
    lines = lines + ["snap end"]  # the last snapshot carries the instancing counters
    modes = ("ref", "noinst", "cand") if args.with_ab else ("ref", "cand")
    results = {mode: run(mode, name, model, extra, lines) for mode in modes}
    if any(r is None for r in results.values()):
        check(False, f"{name}: script run failed ({[m for m, r in results.items() if r is None]})")
        return None
    ref, cand = results["ref"], results["cand"]
    others = [results[m] for m in modes if m != "cand"]
    key = lambda r: [(s["label"], s["selection"], s["camera"]) for s in r["snaps"]]
    check(all(key(o) == key(cand) for o in others), f"{name}: {len(cand['snaps'])} snapshots (selection, camera) identical")
    for shot, path in cand["shots"].items():
        h = viewport_sha(path)
        check(all(h == viewport_sha(o["shots"][shot]) for o in others),
              f"{name}/{shot}: viewports byte-identical to reference" + (" and to --disable-instancing" if args.with_ab else ""))
    rev = cand["snaps"][-1]["revisions"]
    if args.with_ab:
        off = results["noinst"]["snaps"][-1]["revisions"]
        check(off.get("instancedDraws", 0) == 0, f"{name}: --disable-instancing merges nothing")
    if name == "grid_hidden_node":
        check(rev.get("instancedMembers", 0) == 1199, f"{name}: the hidden node is not drawn ({rev.get('instancedMembers', 0)} of 1,200 opaque drawables merged)")
    print(f"    {name}: last frame {rev.get('instancedDraws', 0)} instanced draws for {rev.get('instancedMembers', 0)} drawables")
    if expect_merge:
        check(rev.get("instancedDraws", 0) > 0, f"{name}: candidate merges draws")
    return cand


print(f"[instancing cases: {REF.name}, candidate --disable-instancing, candidate]")
shots = lambda tag: [f"shot @OUT@/{tag}.png", "wait 2"]
case("grid", MANY, ["--aa", "none"], ["wait 10"] + shots("grid"))
case("grid_msaa_taa", MANY, ["--aa", "taa"], ["wait 80"] + shots("grid_taa"))
case("grid_close_culled", MANY, ["--aa", "msaa4", "--camera-distance", "6"], ["wait 10"] + shots("grid_close"))
case("grid_ortho", MANY, ["--aa", "none", "--camera-projection", "orthographic", "--camera-pitch", "60"], ["wait 10"] + shots("grid_ortho"))
case("grid_wireframe_mode", MANY, ["--aa", "none", "--render-mode", "wireframe"], ["wait 10"] + shots("grid_wire"))
case("grid_overlay_doublesided", MANY, ["--aa", "none"], ["wait 8", "vpsetting 1 wireframe on", "vpsetting 1 doublesided on", "wait 8"] + shots("grid_overlay"))
case("grid_hidden_node", MANY, ["--aa", "none", "--hide-node", "N_4_5"], ["wait 10"] + shots("grid_hidden"))
case("leaf_playback", LEAF, ["--aa", "none"], ["wait 8", "animplay on", "wait 25", "shot @OUT@/leaf_a.png", "wait 15",
                                              "shot @OUT@/leaf_b.png", "animplay off", "wait 4"] + shots("leaf_stop"))
case("root_playback", ROOT, ["--aa", "msaa4"], ["wait 8", "animplay on", "wait 25", "shot @OUT@/root_a.png", "wait 15",
                                               "shot @OUT@/root_b.png", "animplay off", "wait 4"] + shots("root_stop"))
clicks = [f"click {x} {y}\nwait 12\nsnap c{x}_{y}" for y in range(180, 700, 65) for x in range(330, 980, 65)]
case("grid_clicks", MANY, ["--aa", "none", "--camera-distance", "14"], ["wait 10"] + clicks + shots("grid_clicks"))
case("split_viewports", MANY, ["--aa", "none"], ["wait 8", "vpcreate", "wait 10", "vpsplit 2", "wait 10",
                                                  "vpsetting 2 doublesided on", "wait 8"] + shots("split"))
case("reload", MANY, ["--aa", "none"], ["wait 8", f"reload {ENGINE}", "wait 10", "shot @OUT@/reload_engine.png", f"reload {MANY}", "wait 10"] + shots("reload_back"))
case("engine", ENGINE, ["--aa", "msaa4"], ["wait 10"] + shots("engine"))
case("fixture", FIXTURE, ["--aa", "none", "--camera-yaw", "160", "--camera-pitch", "10"], ["wait 10"] + shots("fixture"), expect_merge=False)
if E195.exists():
    e195_clicks = [f"click {x} {y}\nwait 12\nsnap c{x}_{y}" for y in range(150, 720, 95) for x in range(330, 980, 95)]
    case("e195", str(E195), ["--aa", "msaa4"], ["wait 10"] + e195_clicks + shots("e195"))
else:
    print("  NOT RUN e195: asset not present")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
