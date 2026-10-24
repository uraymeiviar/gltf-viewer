# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Phase 8 follow-up validation (multi-viewport work after 23f61c6).

  ASOBO_material_invisible  Materials with the extension ("enabled" true, the
               schema default) are not rendered unless the viewport option
               "Show Invisible Materials" (--show-invisible-materials,
               vpsetting <id> invisible on) is set.
  Shared traversal  Viewports drawn in one frame reuse one scene traversal;
               --disable-shared-traversal traverses per viewport.
  Redundant uploads  Uniform values a program already holds and texture units
               that already hold the right texture and sampler are not set
               again.

Sections:
  equivalence  Invisible-material rules on the E195 cockpit (when present) and
               a generated fixture; three-viewport scenes byte-identical with
               and without --disable-shared-traversal, including viewports with
               different invisible settings.
  benchmark    Three TAA viewports of the E195 cockpit at 1920x1080, every frame
               redrawn: the 23f61c6 binary vs the candidate (default and with
               --show-invisible-materials).
"""

import argparse
import hashlib
import json
import os
import statistics
import harness_process as subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "p8" / "followup"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
MANY = "build/p8/fixtures/many_1600.gltf"
E195 = Path(r"F:/new-fss-aircraft-ejet/msfs24-projects/fss-aircraft-e195/SimObjects/Airplanes/E195/model/E195-Cockpit-LOD00.gltf")
# --fixed-frame-time makes scripted runs deterministic, so they need not wait for vsync
# (a 400-click case: 108 s with vsync, 4 s without, identical results).
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05", "--vsync", "off"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["equivalence", "invisible", "traversal", "benchmark"],
                    help="equivalence runs both the invisible and the traversal sections")
parser.add_argument("--ref-exe", default=str(REPO / "build" / "p8" / "Glider_23f61c6.exe"))
parser.add_argument("--pairs", type=int, default=3)
parser.add_argument("--bench", action="store_true", help="also run the benchmark section")
args = parser.parse_args()
REF = Path(args.ref_exe).resolve()
OUT.mkdir(parents=True, exist_ok=True)
failures = []


def section(name):
    # Benchmarks back timing claims and are slow; regression runs skip them unless asked.
    if name == "benchmark":
        return args.only == "benchmark" or args.bench
    if name in ("invisible", "traversal"):
        return args.only in (None, "equivalence", name)
    return args.only in (None, name)


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_fixtures():
    """A many-drawables grid whose Blend material is ASOBO-invisible, a copy with
    "enabled": false, and (when the E195 asset exists) an E195 copy with absolute
    buffer and image paths so it renders with textures under --isolated."""
    if not (REPO / MANY).exists():  # shared with other harnesses; run_regression.py writes them up front
        (REPO / MANY).parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([sys.executable, str(REPO / "scripts" / "generate_many_drawables_fixture.py"), str(REPO / MANY), "40"],
                       check=True, capture_output=True)
    grid = json.loads((REPO / MANY).read_text(encoding="utf-8"))
    for material in grid["materials"]:
        if material["name"] == "Blend":
            material.setdefault("extensions", {})["ASOBO_material_invisible"] = {"enabled": True}
    grid["extensionsUsed"] = sorted(set(grid.get("extensionsUsed", [])) | {"ASOBO_material_invisible"})
    (OUT / "grid_invisible.gltf").write_text(json.dumps(grid), encoding="utf-8")
    for material in grid["materials"]:
        if "ASOBO_material_invisible" in material.get("extensions", {}):
            material["extensions"]["ASOBO_material_invisible"]["enabled"] = False
    (OUT / "grid_invisible_disabled.gltf").write_text(json.dumps(grid), encoding="utf-8")
    # The E195 cockpit with absolute texture paths (scripts/e195_assets.py).
    from e195_assets import textured_copy
    copy = textured_copy(E195, OUT)
    if copy is None:
        return False
    copy.replace(OUT / "e195_abs.gltf")
    return True


HAS_E195 = write_fixtures()
GRID_INV = (OUT / "grid_invisible.gltf").as_posix()
GRID_INV_OFF = (OUT / "grid_invisible_disabled.gltf").as_posix()
E195_ABS = (OUT / "e195_abs.gltf").as_posix()


def shot(exe, name, model, extra, lines, size=None):
    folder = OUT / "shots"
    folder.mkdir(parents=True, exist_ok=True)
    png = folder / f"{name}.png"
    png.unlink(missing_ok=True)
    script = folder / f"{name}.txt"
    script.write_text("\n".join(lines + [f"shot {png.relative_to(REPO).as_posix()}", "wait 2", "snap end"]) + "\n", encoding="utf-8")
    report = folder / f"{name}.json"
    report.unlink(missing_ok=True)
    common = list(COMMON)
    if size:
        common[common.index("--window-size") + 1] = size
    subprocess.run([str(exe), model] + common + extra + ["--ui-script", str(script), "--ui-report", str(report)],
                   cwd=REPO, capture_output=True, timeout=600)
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    return (sha(png) if png.exists() else None), (data["snaps"][-1]["revisions"] if data and data.get("snaps") else {})


THREE = ["wait 8", "vpcreate", "vpsplit 2", "vpcreate", "vpsplit 3", "aamode taa 1", "aamode taa 2", "aamode taa 3", "wait 70"]

# The regression runs only the traversal section: validate_asobo_common.py and validate_asobo_model.py cover
# ASOBO_material_invisible since A0.4.
if section("invisible"):
    print("[ASOBO_material_invisible on a grid whose Blend quads carry the extension]")
    base, _ = shot(REF, "grid_ref", MANY, ["--aa", "msaa4"], ["wait 10"])
    hidden, _ = shot(EXE, "grid_hidden", GRID_INV, ["--aa", "msaa4"], ["wait 10"])
    shown, _ = shot(EXE, "grid_shown", GRID_INV, ["--aa", "msaa4", "--show-invisible-materials"], ["wait 10"])
    toggled, _ = shot(EXE, "grid_toggled", GRID_INV, ["--aa", "msaa4"], ["wait 6", "vpsetting 1 invisible on", "wait 10"])
    disabled, _ = shot(EXE, "grid_disabled", GRID_INV_OFF, ["--aa", "msaa4"], ["wait 10"])
    check(hidden is not None and hidden != base, "invisible material hidden by default")
    check(shown == base, "--show-invisible-materials renders the asset exactly as before")
    check(toggled == base, "the viewport option, switched on at runtime, renders the asset exactly as before")
    check(disabled == base, '"enabled": false renders the asset exactly as before')
    plain_ref, _ = shot(REF, "plain_ref", FIXTURE, ["--aa", "none"], ["wait 10"])
    plain, _ = shot(EXE, "plain", FIXTURE, ["--aa", "none"], ["wait 10"])
    check(plain == plain_ref, "assets without the extension are unchanged")
    if HAS_E195:
        # The reference draws everything: builds before the extension ignore the flag, later ones honor it.
        e_ref, _ = shot(REF, "e195_ref", E195_ABS, ["--aa", "msaa4", "--show-invisible-materials"], ["wait 10"])
        e_def, _ = shot(EXE, "e195_default", E195_ABS, ["--aa", "msaa4"], ["wait 10"])
        e_show, _ = shot(EXE, "e195_show", E195_ABS, ["--aa", "msaa4", "--show-invisible-materials"], ["wait 10"])
        check(e_def != e_show, "E195: collider materials hidden by default")
        check(e_show == e_ref, "E195: with --show-invisible-materials, identical to the reference drawing everything")
    else:
        print("  NOT RUN E195: asset not present")

if section("traversal"):
    print("[three TAA viewports: shared traversal vs --disable-shared-traversal]")
    cases = [("fixture3", FIXTURE, THREE),
             ("grid3_mixed", GRID_INV, THREE[:-1] + ["vpsetting 2 invisible on", "wait 70"])]
    if HAS_E195:
        cases.append(("e195_3", E195_ABS, THREE))
        cases.append(("e195_3_mixed", E195_ABS, THREE[:-1] + ["vpsetting 3 invisible on", "wait 70"]))
    for name, model, lines in cases:
        shared, rev = shot(EXE, f"{name}_shared", model, ["--aa", "none"], lines, size="1920x1080")
        separate, rev_off = shot(EXE, f"{name}_separate", model, ["--aa", "none", "--disable-shared-traversal"], lines, size="1920x1080")
        check(shared is not None and shared == separate, f"{name}: byte-identical with and without the shared traversal")
        check(rev.get("traversalReuses", 0) > 0 and rev_off.get("traversalReuses", 0) == 0,
              f"{name}: traversal reused {rev.get('traversalReuses', 0)} times (separate: {rev_off.get('traversalReuses', 0)})")


def bench(exe, extra, label, index):
    out = OUT / "bench" / f"{label}_{index}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.unlink(missing_ok=True)
    script = OUT / "bench" / "three.txt"
    script.write_text("\n".join(THREE[:-1] + ["wait 3000"]) + "\n", encoding="utf-8")
    subprocess.run([str(exe), E195_ABS, "--isolated", "--window-size", "1920x1080", "--vsync", "off", "--disable-redraw-skip",
                    "--profile", "--benchmark", "--benchmark-warmup", "250", "--benchmark-frames", "300",
                    "--benchmark-output", str(out), "--exit-after-benchmark", "--ui-script", str(script),
                    "--ui-report", str(OUT / "bench" / "ui.json")] + extra, cwd=REPO, capture_output=True, timeout=900)
    data = json.loads(out.read_text(encoding="utf-8"))
    frames = data["measuredFrames"]
    stage = lambda f, n: sum(s["ms"] for s in f["stages"] if s["name"] == n)
    return {"interval": statistics.median(f["frameIntervalMs"] for f in frames),
            "submit": statistics.median(stage(f, "draw.submit") for f in frames),
            "scene": statistics.median(stage(f, "scene.create") for f in frames),
            "glCalls": frames[-1]["counters"]["glCalls"], "draws": frames[-1]["counters"]["rendererDrawCalls"]}


if section("benchmark"):
    if not HAS_E195:
        print("  NOT RUN benchmark: E195 asset not present")
    else:
        print(f"[three TAA viewports of E195, 1920x1080, every frame redrawn, {args.pairs} rounds]")
        variants = [("23f61c6", REF, []), ("candidate", EXE, []), ("candidate_show", EXE, ["--show-invisible-materials"])]
        results = {name: [] for name, _, _ in variants}
        for i in range(args.pairs):
            for name, exe, extra in (variants if i % 2 == 0 else list(reversed(variants))):
                results[name].append(bench(exe, extra, name, i))
        for name, rows in results.items():
            print(f"    {name:15s} interval {min(r['interval'] for r in rows):.3f}-{max(r['interval'] for r in rows):.3f} ms "
                  f"(median {statistics.median(r['interval'] for r in rows):.3f}), draw.submit {statistics.median(r['submit'] for r in rows):.3f} ms, "
                  f"scene.create {statistics.median(r['scene'] for r in rows):.3f} ms, draws {rows[0]['draws']}, glCalls {rows[0]['glCalls']}")
        ref = statistics.median(r["interval"] for r in results["23f61c6"])
        show = statistics.median(r["interval"] for r in results["candidate_show"])
        check(show <= ref * 1.05, f"same content (--show-invisible-materials) is not slower than 23f61c6 ({ref:.3f} -> {show:.3f} ms)")
        (OUT / "bench" / "summary.json").write_text(json.dumps(results, indent=2), encoding="utf-8")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
