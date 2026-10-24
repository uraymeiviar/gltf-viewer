# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P8.1 uniform submission validation.

P8.1a: draw commands resolve their uniform and sampler bindings when they are
built (and drop uniforms the program does not use) instead of looking every
name up in the program's string map on every draw.
P8.1b/c: per-frame camera and environment values move into uniform blocks.

Every substep must leave the image byte-identical, so the same suite runs for
each against the previous substep's binary (--ref-exe).

Sections:
  equivalence  Viewport captures and UI-script screenshots/selections, reference
               vs candidate: real models and fixtures in four AA modes,
               wireframe mode, skinned and animated playback, two viewports,
               clicks, transmission.
  benchmark    Paired runs, every frame redrawn: draw.submit, CPU frame and GPU
               frame time on the 1,600 and 10,000 grids, the E195 cockpit and
               2CylinderEngine.
"""

import argparse
import hashlib
import json
import statistics
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "p8" / "p8.1"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
HIER = "resources/test_models/hierarchy_animation_fixture.gltf"
POINTER = "resources/test_models/pointer_animation_fixture.gltf"
ALPHA = "OpenGL-Renderer/Tools/tools/operations/tests/resources/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf"
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
MORPH = "OpenGL-Renderer/Tools/tests/test_resources/AnimatedMorphCube.gltf"
ENGINE = "build/_deps/assimp-src/test/models/glTF2/2CylinderEngine-glTF-Binary/2CylinderEngine.glb"
TRUCK = "build/_deps/assimp-src/test/models/glTF/CesiumMilkTruck/CesiumMilkTruck.gltf"
MANY = "build/p8/fixtures/many_1600.gltf"
HUGE = "build/p8/fixtures/many_10000.gltf"
# The E195 cockpit with absolute texture paths (scripts/e195_assets.py), so every binary loads all its textures.
from e195_assets import COCKPIT, textured_copy
E195 = textured_copy(COCKPIT, REPO / "build" / "e195_assets") or COCKPIT
# --fixed-frame-time makes scripted runs deterministic, so they need not wait for vsync
# (a 400-click case: 108 s with vsync, 4 s without, identical results).
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05", "--vsync", "off"]
BEHIND = ["--camera-yaw", "160", "--camera-pitch", "10"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["equivalence", "benchmark"])
parser.add_argument("--ref-exe", default=str(REPO / "build" / "p8" / "Glider_p85.exe"), help="reference binary")
parser.add_argument("--label", default="p8.1", help="output subfolder name")
parser.add_argument("--pairs", type=int, default=5)
parser.add_argument("--bench", action="store_true", help="also run the benchmark section")
parser.add_argument("--extra", action="append", default=[], help="extra argument passed to both binaries (repeatable)")
args = parser.parse_args()
REF = Path(args.ref_exe).resolve()
OUT = OUT.parent / args.label
failures = []


def section(name):
    # Benchmarks back timing claims and are slow; regression runs skip them unless asked.
    if name == "benchmark":
        return args.only == "benchmark" or args.bench
    return args.only in (None, name)


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


for path, size in ((MANY, "40"), (HUGE, "100")):
    (REPO / path).parent.mkdir(parents=True, exist_ok=True)
    if not (REPO / path).exists():
        subprocess.run([sys.executable, str(REPO / "scripts" / "generate_many_drawables_fixture.py"), str(REPO / path), size],
                       check=True, capture_output=True)
for script in ("generate_hierarchy_fixture.py", "generate_pointer_animation_fixture.py"):
    subprocess.run([sys.executable, str(REPO / "scripts" / script)], capture_output=True)

MODELS = {"cube": None, "fixture": FIXTURE, "fixture_behind": FIXTURE, "alpha": ALPHA, "engine": ENGINE, "truck": TRUCK,
          "morph": MORPH, "pointer": POINTER, "many": MANY}
if E195.exists():
    MODELS["e195"] = str(E195)


def capture(exe, name, model, extra, folder):
    folder.mkdir(parents=True, exist_ok=True)
    png = folder / f"{name}.png"
    png.unlink(missing_ok=True)
    subprocess.run([str(exe)] + ([model] if model else []) + COMMON + extra + args.extra +
                   ["--capture-viewport", str(png), "--capture-frame", "24", "--profile-report", str(folder / "p.json"),
                    "--exit-after-profile"], cwd=REPO, capture_output=True, timeout=600)
    return png if png.exists() else None


def run_script(exe, name, model, extra, lines, folder):
    folder.mkdir(parents=True, exist_ok=True)
    text = "\n".join(lines).replace("@OUT@", folder.relative_to(REPO).as_posix()) + "\n"
    shots = [l.split(" ", 1)[1] for l in text.splitlines() if l.startswith("shot ")]
    for s in shots:
        (REPO / s).unlink(missing_ok=True)
    scenario, report = folder / f"{name}.txt", folder / f"{name}.json"
    scenario.write_text(text, encoding="utf-8")
    report.unlink(missing_ok=True)
    subprocess.run([str(exe)] + ([model] if model else []) + COMMON + extra + args.extra + ["--ui-script", str(scenario), "--ui-report", str(report)],
                   cwd=REPO, capture_output=True, timeout=600)
    if not report.exists():
        return None
    data = json.loads(report.read_text(encoding="utf-8"))
    if data.get("failed", True):
        return None
    return {"snaps": [(s["label"], s["selection"], s["camera"]) for s in data["snaps"]],
            "shots": {Path(s).name: REPO / s for s in shots}}


if section("equivalence"):
    print(f"[viewport captures, reference {REF.name} vs candidate]")
    for name, model in MODELS.items():
        for aa in ("none", "msaa4", "taa", "fxaa"):
            extra = ["--aa", aa] + (BEHIND if name == "fixture_behind" else [])
            ref = capture(REF, f"{name}_{aa}", model, extra, OUT / "ref")
            cand = capture(EXE, f"{name}_{aa}", model, extra, OUT / "cand")
            check(ref is not None and cand is not None and sha(ref) == sha(cand), f"{name} {aa}: viewport byte-identical")
    for name, model, extra in (("wire", FIXTURE, ["--render-mode", "wireframe"] + BEHIND),
                               ("ortho", ENGINE, ["--camera-projection", "orthographic"]),
                               ("doublesided", FIXTURE, ["--force-double-sided"] + BEHIND)):
        ref = capture(REF, name, model, ["--aa", "none"] + extra, OUT / "ref")
        cand = capture(EXE, name, model, ["--aa", "none"] + extra, OUT / "cand")
        check(ref is not None and cand is not None and sha(ref) == sha(cand), f"{name}: viewport byte-identical")

    print("[UI scripts: playback, two viewports, clicks]")
    clicks = [f"click {x} {y}\nwait 12\nsnap c{x}_{y}" for y in range(260, 620, 90) for x in range(360, 960, 120)]
    scripts = {
        "fox_play": (FOX, ["--aa", "none"], ["wait 8", "animplay on", "wait 40", "shot @OUT@/fox_play.png", "animplay off", "wait 2"]),
        "hier_play": (HIER, ["--aa", "msaa4"], ["wait 8", "animplay on", "wait 40", "shot @OUT@/hier_play.png", "animplay off", "wait 2"]),
        "pointer_play": (POINTER, ["--aa", "none"], ["wait 8", "animplay on", "wait 30", "shot @OUT@/pointer_play.png", "animplay off", "wait 2"]),
        "split": (FIXTURE, ["--aa", "none"] + BEHIND, ["wait 8", "vpcreate", "wait 10", "vpsplit 2", "wait 10",
                                                      "vpsetting 2 doublesided on", "wait 8", "shot @OUT@/split.png", "wait 2"]),
        "clicks": (FIXTURE, ["--aa", "none"] + BEHIND, ["wait 10"] + clicks + ["shot @OUT@/clicks.png", "wait 2"]),
    }
    for name, (model, extra, lines) in scripts.items():
        ref = run_script(REF, name, model, extra, lines, OUT / "ref")
        cand = run_script(EXE, name, model, extra, lines, OUT / "cand")
        if ref is None or cand is None:
            check(False, f"{name}: script run failed")
            continue
        check(ref["snaps"] == cand["snaps"], f"{name}: {len(cand['snaps'])} snapshots (selection, camera) identical")
        for shot, path in cand["shots"].items():
            check(viewport_sha(path) == viewport_sha(ref["shots"][shot]), f"{name}/{shot}: viewports byte-identical")


def bench(exe, model, label, index):
    out = OUT / "bench" / f"{Path(model).stem}_{label}_{index}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.unlink(missing_ok=True)
    subprocess.run([str(exe), model, "--isolated", "--window-size", "1280x800", "--aa", "none", "--vsync", "off",
                    "--disable-redraw-skip", "--profile", "--benchmark", "--benchmark-warmup", "120",
                    "--benchmark-frames", "300", "--benchmark-output", str(out), "--exit-after-benchmark"] + args.extra,
                   cwd=REPO, capture_output=True, timeout=900)
    data = json.loads(out.read_text(encoding="utf-8"))
    frames = data["measuredFrames"]
    ids = {f["frame"] for f in frames}
    gpu = {}
    for s in data["gpu"]["completed"]:
        if s["frame"] in ids and s.get("label") == "main":
            gpu[s["frame"]] = gpu.get(s["frame"], 0.0) + sum(p["ms"] for p in s["passes"] if p.get("available"))
    stage = lambda f, n: sum(s["ms"] for s in f["stages"] if s["name"] == n)
    return {"submit": statistics.median(stage(f, "draw.submit") for f in frames),
            "cmdgen": statistics.median(stage(f, "commands.generate") for f in frames),
            "cpu": statistics.median(f["cpuFrameMs"] for f in frames),
            "gpu": statistics.median(gpu.values()) if gpu else None,
            "glCalls": frames[-1]["counters"].get("glCalls")}


if section("benchmark"):
    models = [MANY, HUGE, ENGINE] + ([str(E195)] if E195.exists() else [])
    summary = {}
    for model in models:
        print(f"[{Path(model).stem}: {args.pairs} alternating pairs, 120 warmup + 300 frames, every frame redrawn]")
        ref, cand = [], []
        for i in range(args.pairs):
            order = [("ref", REF), ("cand", EXE)] if i % 2 == 0 else [("cand", EXE), ("ref", REF)]
            res = {label: bench(exe, model, label, i) for label, exe in order}
            ref.append(res["ref"])
            cand.append(res["cand"])
        print(f"    glCalls/frame: {ref[0]['glCalls']} -> {cand[0]['glCalls']}")
        for key in ("submit", "cmdgen", "cpu", "gpu"):
            r = [x[key] for x in ref]
            c = [x[key] for x in cand]
            if any(v is None for v in r + c):
                continue
            noise = (max(r) - min(r)) / statistics.median(r) if statistics.median(r) else 0.0
            deltas = [(a - b) / a if a else 0.0 for a, b in zip(r, c)]
            print(f"    {key}: {min(r):.3f}-{max(r):.3f} -> {min(c):.3f}-{max(c):.3f} ms, median paired "
                  f"{statistics.median(deltas):+.1%}, improved {sum(d > 0 for d in deltas)}/{len(deltas)}, noise {noise:.1%}")
            if key in ("cpu", "gpu"):
                check(statistics.median(deltas) > -max(0.05, noise), f"{Path(model).stem} {key}: no regression beyond 5% and noise")
        summary[model] = {"ref": ref, "cand": cand}
    (OUT / "bench" / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
